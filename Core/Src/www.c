/*
 * www.c
 *
 *  Created on: 6Jun.,2018
 *      Author: bob
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "main.h"
#include "stm32f7xx_hal.h"
#include "lwip.h"
#include "httpd_structs.h"
#include "httpclient.h"
#include "www.h"
#include "neo7m.h"
#include "udpstream.h"
#include "splat1.h"
#include "adcstream.h"
#include "lcd.h"
#include "nextionloader.h"
#include "tftp/tftp_loader.h"
#include "lwip/dns.h"

//#include "httpd.h"

// Support functions

/*--------------------------------------------------*/
// httpd server support
/*--------------------------------------------------*/

extern I2C_HandleTypeDef hi2c1;
char udp_target[64];	// dns or ip address of udp target
char stmuid[96] = { 0 };	// STM UUID
ip_addr_t remoteip = { 0 };
int expectedapage = 0;
uint32_t polltime = 900;	// polling server timer in seconds
volatile uint32_t trigcomp = 0;		// trigger threshold compensation provided by the server

// The cgi handler is called when the user changes something on the webpage
void httpd_cgi_handler(struct fs_file *file, const char *uri, int count, char **http_cgi_params,
		char **http_cgi_param_vals) {

	int i, j, val;
	char *ptr;

	j = strtol(*http_cgi_params, &ptr, 10);		// allow two chars len for the number

	printf("httpd_cgi_handler: uri=%s, count=%d j=%d\n", uri, count, j);

	for (i = 0; i < count; i++) {			/// number of things sent from the form
//		printf("params=%d, id=%c, val=%c, j=%d\n", i, **http_cgi_params, (*http_cgi_param_vals)[i],j);

		switch (j) {

		case 10:			// reboot button
			printf("Reboot command from wwww\n");
			osDelay(500);
			__NVIC_SystemReset();   // reboot
			break;
		case 11:				// LED1
#ifdef TESTING
				stats_display(); // this needs stats in LwIP enabling to do anything
#endif
			if (((*http_cgi_param_vals)[i]) == '0')
				HAL_GPIO_WritePin(GPIOD, LED_D4_Pin, GPIO_PIN_RESET);
			else
				HAL_GPIO_WritePin(GPIOD, LED_D4_Pin, GPIO_PIN_SET);
			break;

		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:		// output switch array
		case 19:
			j -= 11;	// now offset 0
			if (((*http_cgi_param_vals)[i]) == '0') {
				muxdat[0] = muxdat[0] & ~(1 << (j - 1));
			} else {
				muxdat[0] = muxdat[0] | (1 << (j - 1));
			}
			logampmode = muxdat[0] & 2;		// lin/logamp output mux
			printf("setting outmux to 0x%02x\n", muxdat[0]);
			if (HAL_I2C_Master_Transmit(&hi2c1, 0x44 << 1, &muxdat[0], 1, 1000) != HAL_OK) {		// RF dual MUX
				printf("I2C HAL returned error 1\n\r");
			}
			break;
		case 20:		// PGA G2
			val = (((*http_cgi_param_vals)[i]) == '0' ? pgagain & ~4 : pgagain | 4);
			setpgagain(val);
			break;
		case 21:		// PGA G1
			val = (((*http_cgi_param_vals)[i]) == '0' ? pgagain & ~2 : pgagain | 2);
			setpgagain(val);
			break;
		case 22:		// PGA G0
			val = (((*http_cgi_param_vals)[i]) == '0' ? pgagain & ~1 : pgagain | 1);
			setpgagain(val);
			break;

		case 23:		// RF Switch
			if (((*http_cgi_param_vals)[i]) == '1')
				HAL_GPIO_WritePin(GPIOE, LP_FILT_Pin, GPIO_PIN_RESET);// select RF Switches to LP filter (normal route)
			else
				HAL_GPIO_WritePin(GPIOE, LP_FILT_Pin, GPIO_PIN_SET);		// select RF Switches to bypass LP filter
			break;

		case 24:		// PGA
			agc = (((*http_cgi_param_vals)[i]) == '0' ? 0 : 1);
			break;

		default:
			printf("Unknown id in cgi handler %s\n", *http_cgi_params);
			break;
		} // end switch
	} // end for
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
	printf("httpd_post_receive_data: \n");
	return (0);
}

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request, u16_t http_request_len,
		int content_len, char *response_uri, u16_t response_uri_len, u8_t *post_auto_wnd) {
	printf("httpd_post_begin: \n");
	return (0);
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
	printf("httpd_post_finished: \n");
}

// this is called to process tags when constructing the webpage being sent to the user
void http_set_ssi_handler(tSSIHandler ssi_handler, const char **tags, int num_tags);  // prototype
// embedded ssi handler
const char *tagname[] = { "temp", "pressure", "time", "led1", "sw1A", "sw1B", "sw1C", "sw1D", "sw2A", "sw2B", "sw2C",
		"sw2D", "butt1", "PG0", "PG1", "PG2", "RF1", "devid", "detinfo", "GPS", "AGC", (void*) NULL };
int i, j;

// the tag callback handler
u16_t tag_callback(int index, char *newstring, int maxlen) {
//  LOCK_TCPIP_CORE();
	if (ledsenabled) {
		HAL_GPIO_TogglePin(GPIOD, LED_D3_Pin);
	} else {
		HAL_GPIO_WritePin(GPIOD, LED_D3_Pin, GPIO_PIN_RESET);
	}
	/*
	 newstring[0] = '5';
	 newstring[1] = '\0';
	 return (1);
	 */
//		printf("tSSIHandler: index=0x%x, newstring=%s, maxlen=%d\n",index,newstring,maxlen);
#if 0
	if (xSemaphoreTake(ssicontentHandle,( TickType_t ) 100 ) == pdTRUE) {	// get the ssi generation semaphore (portMAX_DELAY == infinite)
		/*printf("We have the semaphore\n")*/;
	} else {
		printf("semaphore take2 failed\n");
	}
#endif
	while (!(xSemaphoreTake(ssicontentHandle,( TickType_t ) 1 ) == pdTRUE)) {// get the ssi generation semaphore (portMAX_DELAY == infinite)
		printf("sem wait 2\n");
	}
	{
//		printf("sem2 wait done\n");
	}

	if ((index > 3) && (index < 12)) {		// omux array
		i = index - 4;		// 0 to 7
		i = (muxdat[0] & (1 << i));
		if (i == 0)		// around the houses
			strcpy(newstring, "0");
		else
			strcpy(newstring, "1");
//			sprintf(newstring,"<%d>",index);
	} else
		switch (index) {
		case 0:
			strcpy(newstring, tempstr);		// temperature
			break;
		case 1:
			strcpy(newstring, pressstr);		// pressure
			break;
		case 2:
			strcpy(newstring, nowtimestr);
			break;
		case 3:			// Led1
			if (HAL_GPIO_ReadPin(GPIOD, LED_D4_Pin) == GPIO_PIN_SET)
				strcpy(newstring, "1");
			else
				strcpy(newstring, "0");
			break;
		case 12:		// butt1
			strcpy(newstring, "5");
			break;
		case 13:	// PG0
			strcpy(newstring, (pgagain & 1) ? "1" : "0");
			break;
		case 14:	// PG1
			strcpy(newstring, (pgagain & 2) ? "1" : "0");
			break;
		case 15:	// PG2
			strcpy(newstring, (pgagain & 4) ? "1" : "0");
			break;
		case 16:	// RF1
			strcpy(newstring, (HAL_GPIO_ReadPin(GPIOE, LP_FILT_Pin) ? "0" : "1"));
			break;
		case 17:	// Device IDs
			strcpy(newstring, snstr);			// Detector ID
			break;
		case 18:	// Detector Info
			strcpy(newstring, statstr);		// Detector Status
			break;
		case 19:	// GPS
			strcpy(newstring, gpsstr);		// GPS Status
			break;
		case 20:	// AGC
			strcpy(newstring, (agc) ? "1" : "0");		// AGC Status
			break;
		default:
			sprintf(newstring, "\"ssi_handler: bad tag index %d\"", index);
			break;
		}
//		sprintf(newstring,"index=%d",index);
//  UNLOCK_TCPIP_CORE();

	if (xSemaphoreGive(ssicontentHandle) != pdTRUE) {		// give the ssi generation semaphore
		printf("semaphore give2 failed\n");		// expect this to fail as part of the normal setup
	}
	return (strlen(newstring));
}

// embedded ssi tag handler setup
void init_httpd_ssi() {

	http_set_ssi_handler(tag_callback, tagname, 21);	// was 32
}

/* ---------------------------------------------------------------------------------------------------------------------------- */
// Remote settings from the control server
/* ---------------------------------------------------------------------------------------------------------------------------- */
// The server's reply is "<sn> <udp_target> <p1> {key:value,key:value,...}" (no spaces inside the braces).
// Every key below is matched exactly; unknown keys are ignored and absent keys leave the variable unchanged, so new
// firmware works with an older server and vice versa. To add a setting, add a row: key, type, variable, range.
// Out-of-range values are rejected (and logged); rows with log=1 are printed only when the value changes.

enum { RS_U8 = 1, RS_U16, RS_U32, RS_HEX32, RS_STR };

typedef struct {
	const char *key;		// key in the server's {key:value,...} list
	uint8_t type;			// RS_U8/U16/U32 decimal, RS_HEX32 hex, RS_STR [A-Za-z0-9._]
	void *var;				// destination variable
	uint32_t min, max;		// numeric: accepted range; RS_STR: max = buffer size
	uint8_t log;			// print when the value changes
	const char *name;		// console label
} remote_setting_t;

static uint32_t rs_crc1, rs_crc2, rs_n2;	// firmware image checksums etc. from the last reply
static char rs_s1[16];

static const remote_setting_t remote_settings[] = {
	// firmware and LCD updates (the server advertises a build)
	{ "fw",   RS_STR,   fwfilename,          0, sizeof(fwfilename), 0, "Firmware file" },
	{ "bld",  RS_U32,   &newbuild,           0, 0xffff,     0, "Firmware build" },
	{ "crc1", RS_HEX32, &rs_crc1,            0, 0xffffffff, 0, "crc1" },
	{ "crc2", RS_HEX32, &rs_crc2,            0, 0xffffffff, 0, "crc2" },
	{ "srv",  RS_STR,   loaderhost,          0, sizeof(loaderhost), 1, "Loader host" },
	{ "n2",   RS_HEX32, &rs_n2,              0, 0xffffffff, 0, "n2" },
	{ "s1",   RS_STR,   rs_s1,               0, sizeof(rs_s1), 0, "s1" },
	{ "lcd",  RS_STR,   lcdfile,             0, sizeof(lcdfile), 0, "LCD file" },
	{ "lbl",  RS_U32,   &srvlcdbld,          0, 0xffff,     0, "LCD build" },
	{ "siz",  RS_U32,   &lcdlen,             0, 0x1000000,  0, "LCD file size" },
	// detection and display
	{ "tt",   RS_U32,   (void*) &trigcomp,   0, 4049,       1, "Trigger level modifier" },
	{ "pt",   RS_U32,   &polltime,           1, 900,        1, "Poll interval modifier" },
	{ "al",   RS_U32,   (void*) &alert_mv,   1, 10000,      1, "Alert level mV" },
	{ "dsp",  RS_U8,    (void*) &despike,    0, 3,          1, "Impulse filter mode" },
	{ "dsk",  RS_U16,   (void*) &despike_k,  1, 4095,       1, "Impulse filter threshold" },
	{ "dsn",  RS_U8,    (void*) &despike_n,  0, 20,         1, "Impulse filter hold" },
};
#define RS_COUNT (sizeof(remote_settings) / sizeof(remote_settings[0]))

// Value of "key:" in the list as a whole key (not part of a longer key), or NULL.
static const char* rs_find(const char *list, const char *key) {
	const size_t n = strlen(key);
	const char *p;

	for (p = list; *p && (*p != '}'); p++) {
		if (((p == list) || !(isalnum((unsigned char) p[-1]) || (p[-1] == '_'))) && (strncmp(p, key, n) == 0) && (p[n] == ':'))
			return (p + n + 1);
	}
	return (NULL);
}

static uint32_t rs_read(const remote_setting_t *s) {
	switch (s->type) {
	case RS_U8:
		return (*(volatile uint8_t*) s->var);
	case RS_U16:
		return (*(volatile uint16_t*) s->var);
	default:
		return (*(volatile uint32_t*) s->var);
	}
}

static void rs_write(const remote_setting_t *s, uint32_t v) {
	switch (s->type) {
	case RS_U8:
		*(volatile uint8_t*) s->var = (uint8_t) v;
		break;
	case RS_U16:
		*(volatile uint16_t*) s->var = (uint16_t) v;
		break;
	default:
		*(volatile uint32_t*) s->var = v;
		break;
	}
}

// Apply every key present in list ("key:value,..." up to '}'). Returns a bitmask of the rows found and valid.
static uint32_t remote_settings_apply(const char *list) {
	uint32_t found = 0;
	unsigned int i;

	for (i = 0; i < RS_COUNT; i++) {
		const remote_setting_t *s = &remote_settings[i];
		const char *v = rs_find(list, s->key);
		if (v == NULL)
			continue;
		if (s->type == RS_STR) {
			char tmp[64];
			size_t j = 0;
			while ((j + 1 < s->max) && (j + 1 < sizeof(tmp)) && (isalnum((unsigned char) v[j]) || (v[j] == '.') || (v[j] == '_'))) {
				tmp[j] = v[j];
				j++;
			}
			tmp[j] = '\0';
			if (j == 0)
				continue;
			if (s->log && strcmp(tmp, (char*) s->var))
				printf("Server -> %s: %s\n", s->name, tmp);
			strcpy((char*) s->var, tmp);
		} else {
			char *end;
			const uint32_t val = strtoul(v, &end, (s->type == RS_HEX32) ? 16 : 10);
			if (end == v)
				continue;
			if ((val < s->min) || (val > s->max)) {
				printf("Server -> %s %lu out of range %lu..%lu, ignored\n", s->name, (unsigned long) val, (unsigned long) s->min,
						(unsigned long) s->max);
				continue;
			}
			if (s->log && (rs_read(s) != val))
				printf("Server -> %s %lu\n", s->name, (unsigned long) val);
			rs_write(s, val);
		}
		found |= (1UL << i);
	}
	return (found);
}

// 1 if every key in keys[] (NULL terminated) was found by the last remote_settings_apply()
static int rs_all_found(uint32_t found, const char *const keys[]) {
	unsigned int i;
	int k;

	for (k = 0; keys[k]; k++) {
		for (i = 0; i < RS_COUNT; i++)
			if (strcmp(remote_settings[i].key, keys[k]) == 0)
				break;
		if ((i == RS_COUNT) || !(found & (1UL << i)))
			return (0);
	}
	return (1);
}

/* ---------------------------------------------------------------------------------------------------------------------------- */
// http client
/* ---------------------------------------------------------------------------------------------------------------------------- */

/*
 p1 commands:-
 1 == reboot
 2 == freeze UDP streaming
 3 == download new firmware if needed
 4 ==
 5 == null (do nothing)

 p2 operands (strings):-
 */

// callback with the page
void returnpage(char *content, u16_t charcount, int errorm) {
	uint32_t sn;
	int nconv, res;
	int p1;
	char p2[256];
	uint32_t crc1 = rs_crc1, crc2 = rs_crc2;	// last values the server sent
	struct ip4_addr newip;
	err_t err;

//	printf("returnpage:\n");
	if (errorm == 0) {
		if (expectedapage) {
			expectedapage = 0;
			res = 0;

//			printf("returnpage: =%d, charcount=%d, content=%.*s\n", errorm, charcount, charcount, content);
//			printf("Server replied: \"%.*s\"\n", charcount, content);
			nconv = sscanf(content, "%5lu%48s%u%255s", &sn, udp_target, &p1, p2);

			switch (nconv) {

			case EOF:
				printf("returnpage: nconv == EOF errno=%d\n", errorm);
				break;

			case 4: 							// converted  4 fields
				// this param is for a variable number of string tokens
				if (p2[0] == '{') {		// its the start of enclosed params
					static const char *const fwkeys[] = { "fw", "bld", "crc1", "crc2", NULL };
					const uint32_t found = remote_settings_apply(&p2[1]);
					res = rs_all_found(found, fwkeys) ? 0 : -1;
					crc1 = rs_crc1;
					crc2 = rs_crc2;
				} // else ignore it
				  // fall through

			case 3:				  // converted  3 fields
				if (p1 == 1) {		// reboot
					printf("Server -> commands a reboot...\n");
					osDelay(500);
					rebootme(6);
				}

				if (p1 == 2) {		// freeze the UDP streaming
					globalfreeze |= 1;
					printf("Server -> commands a streaming freeze\n");
				} else
					globalfreeze &= ~1;
				// falls through

			case 2:		// converted  2 fields
#ifdef TESTING
				strcpy(udp_target, HTTP_CONTROL_SERVER);
#endif
				if (strlen(udp_target) < 7) {					// bad url or ip address
					strcpy(udp_target, HTTP_CONTROL_SERVER);					// default it
				}
//					newip = locateip(udp_target);
// 			try altrnate method below. The above fails and times out (occasionally even triggering watchdog....)
				newip.addr = 0;
				err = dns_gethostbyname(udp_target, &newip, NULL, 0);
				if (err == ERR_OK) {
					if ((newip.addr > 0) && (newip.addr != udpdestip.addr)) {
						printf("******* Target UDP host just changed ********\n ");
						udpdestip = newip;
					}
				}
				printf("Server -> Target UDP host: %s %d:%d:%d:%d\n", udp_target, (int) (udpdestip.addr & 0xFF),
						(int) ((udpdestip.addr & 0xFF00) >> 8), (int) ((udpdestip.addr & 0xFF0000) >> 16),
						(int) ((udpdestip.addr & 0xFF000000) >> 24));

				// falls through

			case 1:					// converted the first field which is the serial number
				if (statuspkt.uid != sn) {
					statuspkt.uid = sn;
					printf("Server -> Serial Number: %u\n", statuspkt.uid);
				}
				break;

			default:
				printf("Wrong number of params from Server -> %d\n", nconv);
				down_total = 0;
				nxt_abort = 1;
				flash_abort = 1;
				http_downloading = NOT_LOADING;
				break;
			} // end case

			// this has to happen last
			if (res) {		// build changed?
				printf("Firmware: this build is %d, the server build is %d\n", BUILDNO, newbuild);
			}
			if ((statuspkt.uid != 0xfeed) && (newbuild != BUILDNO) && (http_downloading == NOT_LOADING)) {// the stm firmware version advertised is different to this one running now
				if (lptask_init_done == 0) {		// if running, reboot before trying to load
//			tftloader(filename, host, crc1, crc2);
					osDelay(1000);
					httploader(fwfilename, loaderhost, crc1, crc2);
				} else {
					printf("Rebooting before loading new firmware, wait...\n");
					rebootme(0);
				}
			}
		} // end if expectedpag
	} else {
		printf("returnpage: (error returned) errno=%d\n", errorm); // end if errorrm
	}
}

// sends a URL request to a http server
void getpage(char page[64]) {
	volatile int result;

//	printf("getpage: %s\n", page);

//    err = dnslookup(HTTP_CONTROL_SERVER, &(remoteip.addr));		// find serial number and udp target IP address
//	if (err != ERR_OK)
//		rebootme(7);
//	ip.addr = remoteip.addr;
//	printf("\n%s Control Server IP: %lu.%lu.%lu.%lu\n", HTTP_CONTROL_SERVER, (ip.addr) & 0xff, ((ip.addr) & 0xff00) >> 8,
//			((ip.addr) & 0xff0000) >> 16, ((ip.addr) & 0xff000000) >> 24);
	printf("Polling the control server: %s\n", HTTP_CONTROL_SERVER);
	result = hc_open(HTTP_CONTROL_SERVER, page, 0, NULL);
	if (result != 0)
		printf("Result from getpage was %d\n", result);
//	printf("httpclient: result=%d\n", result);

}

// get the serial number and udp target for this device
// reboot if fails
void initialapisn() {
	int i, j;
	char localip[32];
	char params[78];

	j = 1;
	sprintf(localip, "%u:%u:%u:%u", (uint) (myip & 0xFF), (uint) ((myip & 0xFF00) >> 8),
			(uint) ((myip & 0xFF0000) >> 16), (uint) (myip & 0xFF000000) >> 24);
	sprintf(params, "?bld=%d&ip=%s&nx=%s", BUILDNO, localip, nex_model);
	sprintf(stmuid, "/api/Device/%lx%lx%lx", STM32_UUID[0], STM32_UUID[1], STM32_UUID[2]);

	strcat(stmuid, params);

	while (statuspkt.uid == 0xfeed)		// not yet found new S/N from server
	{
		printf("initialapisn: getting params from server on port %d Try=%d\n", DOWNLOAD_PORT, j);
		getpage(stmuid);
		printf("initialapisn: waiting...\n");
		osDelay(500); 	// get sn and targ
		for (i = 0; i < 5000; i++) {
			if (statuspkt.uid != 0xfeed)
				break;
			osDelay(2);
		}
		j++;
		if (j > 3) {
			writelcdcmd("xstr 5,88,470,48,2,BLACK,RED,0,1,1,\"NET FAIL -2\"");
			osDelay(5000);
			printf("initialapisn: ************* ABORTED **************\n");
			rebootme(8);
		}
	}
//	printf("initialapisn: got page okay\n");
}

void requestapisn() {
//	printf("requestapisn: updating params from server on port %d\n", DOWNLOAD_PORT);
	getpage(stmuid);		// get sn and targ
}

