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
//#include "httpd.h"

// Support functions

/*--------------------------------------------------*/
// httpd server support
/*--------------------------------------------------*/

extern I2C_HandleTypeDef hi2c1;
char udp_target[64];	// dns or ip address of udp target

// The cgi handler is called when the user changes something on the webpage
void httpd_cgi_handler(struct fs_file *file, const char *uri, int count, char **http_cgi_params, char **http_cgi_param_vals) {
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
			osDelay(1000);
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

	HAL_GPIO_TogglePin(GPIOD, LED_D3_Pin);
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
init_httpd_ssi() {

	http_set_ssi_handler(tag_callback, tagname, 21);	// was 32
}

/* ---------------------------------------------- */
// http client
/* ---------------------------------------------- */

// callback with the page
void returnpage(volatile u8_t Num, volatile hc_errormsg errorm, volatile char *content, volatile u16_t charcount) {
	char *errormsg[] = { "OK", "OUT_MEM", "TIMEOUT", "NOT_FOUND", "GEN_ERROR" };
	volatile uint32_t sn;
	int nconv;
	int p1, p2;

	if (errorm == 0) {
//		printf("returnpage: Num=%d, errorm=%d, charcount=%d, content=%s\n", Num, errorm, charcount,	content);
		nconv = sscanf(content, "%5u%48s%u%u", &sn, udp_target, &p1, &p2);
		if (nconv != EOF) {
			switch (nconv) {

			case 4: 							// converted  4 fields
			case 3: 							// converted  3 fields
			case 2: 							// converted  2 fields
#ifdef TESTING
				strcpy(udp_target, SERVER_DESTINATION);
#endif
				if (strlen(udp_target) < 7) {					// bad url or ip address
					strcpy(udp_target, SERVER_DESTINATION);		// default it
				}
				printf("A response from Server -> Target UDP host changed to %s\n", udp_target);

			case 1: 							// converted the first field which is the serial number
				statuspkt.uid = sn;
				printf("A response from Server -> Serial Number Changed to %lu\n", statuspkt.uid);
				break;

			default:
				printf("Wrong number of params from Server -> %d\n", nconv);
				break;
			}
		} else {
			printf("returnpage: (error returned) Num=%d, errno=%d, error=%s\n", Num, errorm, errormsg[errorm]);
		}
	}
}

void httpclient(char Page[64]) {
	volatile int result;
	uint32_t ip;
	int err;
	static ip_addr_t remoteip = { 0 };
	static char *Postvars = NULL;

	if (remoteip.addr == 0) {
		err = dnslookup(SERVER_DESTINATION, &remoteip);		// find serial number server IP address

		if (err != ERR_OK)
			rebootme();
		ip = remoteip.addr;
		printf("\n%s Control Server IP: %lu.%lu.%lu.%lu\n", SERVER_DESTINATION, ip & 0xff, (ip & 0xff00) >> 8, (ip & 0xff0000) >> 16,
				(ip & 0xff000000) >> 24);
	}

	result = hc_open(remoteip, Page, Postvars, returnpage);
//	printf("result=%d\n", result);

}
