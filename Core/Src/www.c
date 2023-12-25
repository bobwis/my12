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

// The cgi handler is called when the user changes something on the webpage
void httpd_cgi_handler(struct fs_file *file, const char *uri, int count, char **http_cgi_params,
		char **http_cgi_param_vals) {
	const char id[15][6] = { "led1", "sw1A", "sw1B", "sw1C", "sw1D", "sw2A", "sw2B", "sw2C", "sw2D", "btn", "PG2",
			"PG1", "PG0", "RF1", "AGC" };

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
tSSIHandler tag_callback(int index, char *newstring, int maxlen) {
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

///////////////////////////////////////////////////////
/// parse p2 params
// return 0 for success
//////////////////////////////////////////////////////
int parsep2(char *buf, char *match, int type, void *value) {
	int i, j;
	char *pch;
	uint32_t *val;

	i = 0;
	j = 0;
	val = value;
	while ((buf[i]) && (buf[i] != '}')) {
		if (buf[i++] == match[j]) {
			j++;
		} else {
			j = 0;
		}
		if (j > 0) {		// started matching something
			if (buf[i] == ':') {		// end of match
				i++;
				if (type == 1) {		// looking for a string
					j = 0;
					pch = value;
					while ((buf[i]) && ((isalnum(buf[i])) || (buf[i] == '.') || (buf[i] == '_'))) {
						pch[j++] = buf[i++];
					}
					pch[j] = 0;
					return ((j > 0) ? 0 : -1);
				} else if (type == 2) { // uint32_t base 10 string
					return ((sscanf(&buf[i], "%u", val) == 1) ? 0 : -1);
				} else if (type == 3) { // uint32_t hex string
					return ((sscanf(&buf[i], "%lx", val) == 1) ? 0 : -1);
				}
			}
		}
	}
	return (-1);
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
void returnpage(volatile char *content, volatile u16_t charcount, int errorm) {
	char *errormsg[] = { "OK", "OUT_MEM", "TIMEOUT", "NOT_FOUND", "GEN_ERROR" };
	volatile uint32_t sn;
	volatile int nconv, res, res2, res3;
	volatile int p1;
	volatile char p2[256];
	volatile char s1[16];
	volatile uint32_t crc1, crc2, n1 = 0, n2 = 0;
	struct ip4_addr newip;
	err_t err;
	;

//	printf("returnpage:\n");
	if (expectedapage) {
		if (errorm == 0) {
//			printf("returnpage: errorm=%d, charcount=%d, content=%.*s\n", errorm, charcount, charcount, content);
//			printf("Server replied: \"%.*s\"\n", charcount, content);
			s1[0] = '\0';
			nconv = sscanf(content, "%5u%48s%u%255s", &sn, udp_target, &p1, &p2);
			if (nconv != EOF) {
				switch (nconv) {

				case 4: 							// converted  4 fields
					// this param is for a variable number of string tokens
					if (p2[0] == '{') {		// its the start of enclosed params
						res = 0;
						res2 = 0;
						res3 = 0;
						res |= parsep2(&p2[1], "fw", 1, fwfilename);
						res |= parsep2(&p2[1], "bld", 2, &newbuild);
						res |= parsep2(&p2[1], "crc1", 3, &crc1);  // low addr
						res |= parsep2(&p2[1], "crc2", 3, &crc2);

						res2 |= parsep2(&p2[1], "srv", 1, &loaderhost);
						res2 |= parsep2(&p2[1], "n2", 3, &n2);
						res2 |= parsep2(&p2[1], "s1", 1, s1);

						res3 |= parsep2(&p2[1], "lcd", 1, lcdfile);
						res3 |= parsep2(&p2[1], "lbl", 2, &srvlcdbld);
						res3 |= parsep2(&p2[1], "siz", 2, &lcdlen);

//						printf("returnpage: filename=%s, srv=%s, build=%d, crc1=0x%08x, crc2=0x%08x, n1=0x%x, n2=0x%x, s1='%s', res=%d\n",	filename, host, newbuild, crc1, crc2, n1, n2, s1, res);

					} // else ignore it
					  // fall through

				case 3: 							// converted  3 fields
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

				case 2: 							// converted  2 fields
#ifdef TESTING
				strcpy(udp_target, HTTP_CONTROL_SERVER);
#endif
					if (strlen(udp_target) < 7) {					// bad url or ip address
						strcpy(udp_target, HTTP_CONTROL_SERVER);				// default it
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
					printf("Server -> Target UDP host: %s %d:%d:%d:%d\n", udp_target, udpdestip.addr & 0xFF,
							(udpdestip.addr & 0xFF00) >> 8, (udpdestip.addr & 0xFF0000) >> 16,
							(udpdestip.addr & 0xFF000000) >> 24);

					// falls through

				case 1: 							// converted the first field which is the serial number
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
				}
			} else {
				printf("returnpage: (error returned) errno=%d\n", errorm);
			}
			// this has to happen last
			if (!res) {		// build changed?
				printf("Firmware: this build is %d, the server build is %d\n", BUILDNO, newbuild);
			}
#if 1
			if ((statuspkt.uid != 0xfeed) && (newbuild != BUILDNO) && (http_downloading == NOT_LOADING)) {// the stm firmware version advertised is different to this one running now
#else
				if (1) {
#endif
				if (lptask_init_done == 0) {		// if running, reboot before trying to load
//			tftloader(filename, host, crc1, crc2);
					osDelay(1000);
					httploader(fwfilename, loaderhost, crc1, crc2);
				} else {
					printf("Rebooting before loading new firmware, wait...\n");
					rebootme(0);
				}
			}
		}
	}
	expectedapage = 0;
}

// sends a URL request to a http server
void getpage(char page[64]) {
	volatile int result;
	ip_addr_t ip;
	int err = 0;

	static char *postvars = NULL;

//	printf("getpage: %s\n", page);

//    err = dnslookup(HTTP_CONTROL_SERVER, &(remoteip.addr));		// find serial number and udp target IP address
//	if (err != ERR_OK)
//		rebootme(7);
//	ip.addr = remoteip.addr;
//	printf("\n%s Control Server IP: %lu.%lu.%lu.%lu\n", HTTP_CONTROL_SERVER, (ip.addr) & 0xff, ((ip.addr) & 0xff00) >> 8,
//			((ip.addr) & 0xff0000) >> 16, ((ip.addr) & 0xff000000) >> 24);
	printf("Polling the control server: %s\n", HTTP_CONTROL_SERVER);
	result = hc_open(HTTP_CONTROL_SERVER, page, postvars, NULL);
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
	sprintf(params, "?bld=%d\&ip=%s\&nx=%s", BUILDNO, localip, nex_model);
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
		if (j > 5) {
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

