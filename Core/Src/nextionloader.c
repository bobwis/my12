/*
 * nextionloader.c
 *
 *  Created on: 17 Jun. 2022
 *      Author: bob
 *
 *      Adapted from C++, author below
 */

/**
 * @file NexUpload.cpp
 *
 * The implementation of download tft file for nextion.
 *
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
#include "altcp.h"

#include "eeprom.h"
#include "tftp/tftp_loader.h"
#include "lcd.h"
#include "Nextionloader.h"


int nxt_blocksacked = 0;	// number of acks recieved by the LCD (every 4k bytes)
static int residual = 0;	// left over unsent to LCD bytes when block size overflowed
static int bytesinblocksent = 0; 		// byte count into current block
static char nxtbuffer[NXDL_BUFF_SIZE];
int nxt_abort = 0;			// 1 == abort

int decnxtmodel(char *nex_model) {
	char lcdmod;

	lcdmod = 'Z';

	if (!(strncmp(nex_model, "MX4832T035", 10))) {
		lcdmod = 'A';
	} else if (!(strncmp(nex_model, "MX4832F035", 10))) {
		lcdmod = 'B';
	} else if (!(strncmp(nex_model, "MX4832K035", 10))) {
		lcdmod = 'C';
	} else if (!(strncmp(nex_model, "MX4024K032", 10))) {
		lcdmod = 'D';
	} else if (!(strncmp(nex_model, "MX4024T032", 10))) {
		lcdmod = 'E';
	} else if (!(strncmp(nex_model, "MX3224F028", 10))) {
		lcdmod = 'F';
	}
	return (lcdmod);
}

// attempt to load new LCD user firmware
int nxt_loader(char filename[], char host[], uint32_t nxtfilesize) {
	static char newfilename[48];
	int i;
	char lcdmod;

//	printf("nextionloader:  fliename=%s, host=%s, len=%u\n", filename, host, nxtfilesize);

	if ((nxtfilesize == 0) || (nxtfilesize == -1)) {

		printf("nxt_loader: nxt file length was bad\n");
		return (-1);
	}

	if (filename[0] == 0) {

		printf("nxt_loader: nxt file name was bad\n");
		return (-1);
	}

	if (host[0] == 0) {

		printf("nxt_loader: nxt host name was bad\n");
		return (-1);
	}

	lcdmod = decnxtmodel(nex_model);

	http_downloading = NXT_PRELOADING;		// mode == getting ready for nextion download
	sprintf(newfilename, "/firmware/%s-%04u-%c%u.tft", lcdfile, newbuild, lcdmod, lcdbuildno);
	printf("Attempting to download Nextion firmware %s from %s, ******* DO NOT SWITCH OFF ******\n", newfilename, host);
	osDelay(100);

	nxt_abort = 0;
	nxt_blocksacked = 0;
	http_dlclient(newfilename, host, (void*) 0);		// start the download

	for (i = 0; i < 3000; i++) {
		osDelay(1);
		if ((http_downloading != NXT_PRELOADING) || (nxt_abort)) {
			break;
		}		// see if file downloader returned an error before starting LCD upload
	}
	if ((nxt_abort) || (http_downloading == NOT_LOADING)) {
		printf("nxt_loader: Server aborted before sending NXT file\n");
		http_downloading = NOT_LOADING;
		return (-1);
	}
	http_downloading = NXT_LOADING;
	lcd_startdl(nxtfilesize);	// put LCD into its download new user firmware mode
	osDelay(600);				// wait > half a second for LCD to Ack
	if (nxt_blocksacked) {		// LCD acks the start, its now in DL mode
		nxt_blocksacked = 0;		// reset counter
		http_dlclient(newfilename, host, (void*) 0);
	} else {
		http_downloading = NOT_LOADING;
		printf("nextionloader: Nextion download not acked start\n");
	}

	// wait for transfer to complete
	// unblock http client

	return (0);
}

// send residual buffer to the LCD
// gets called from rx_callback and from rx_complete
int nxt_sendres() {
	int res = 0;

	if ((residual) && (nxt_abort == 0)) {				// residual data from last call to send first
		if ((res = lcd_writeblock(nxtbuffer, residual)) == -1) {
			printf("nxt_sendres: failed\n");
			nxt_abort = 1;
		} else {
			while (txdmadone == 0)		// tx in progress
				osDelay(1);
		}
	}
	residual = 0;
	return (res);
}

//#define lcd_writeblock(nxtbuffer, residual) printf("%d ",residual)

// http callback for Nextion firmware download
// this gets called for each downloaded chunk received
//
int nxt_rx_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
	char *buf;
	struct pbuf *q;
	volatile int i, pktlen, res, tlen = 0, len = 0, ch;
	static int blockssent = 0;
	static int qlentot = 0, tot_sent = 0;

//	printf("nxt_rx_callback:\n");

	LWIP_ASSERT("p != NULL", p != NULL);
	if (err != ERR_OK) {
		putchar('@');
		printlwiperr(err);
		return;
	}

//	printf("nxt_rx_callback1: nxt_abort=%d, blockssent=%d, nxt_blocksacked=%d, q->len=%d\n", nxt_abort, blockssent,	nxt_blocksacked, p->len);

	if (nxt_abort) {
		http_downloading = NOT_LOADING;
	}

	if (http_downloading == NXT_PRELOADING) {
		http_downloading = NXT_LOADING;
	}

	i = 0;

	for (q = p; q != NULL; q = q->next) {
		qlentot += q->len;
		tlen = q->tot_len;
		len = q->len;

		if (residual > 0) {
			tot_sent += residual;
			bytesinblocksent += residual;

			if (nxt_sendres() == -1) {	// send residual (if any)
				return (-1);		// abort will now be set
			}
		}

		pktlen = q->len;

		if ((pktlen + bytesinblocksent) > 4096) {	// will we will overflow the 4096 boundary?
			len = 4096 - bytesinblocksent;		// we only have to send len this time

			buf = q->payload;
			for (i = len; i < pktlen; i++) {		// copy the extra bytes we cant send into a buffer
				nxtbuffer[residual++] = buf[i];		// keep the rest back until next time
			}

		} else {
			len = pktlen;		// just try to send what we have got
		}

		tot_sent += len;
		if (nxt_abort == 0) {
			if ((res = lcd_writeblock(q->payload, len) == -1)) {
				printf("NXT Write1 failed from http client\n");
				nxt_abort = 1;
				return (-1);
			}
			while (txdmadone == 0)		// tx in progress
				osDelay(1);

			bytesinblocksent += len;

			if (bytesinblocksent > 4096) {
				printf("BLOCK OVERRUN\n");
			}

			if (bytesinblocksent == 4096) {
//				printf("nxt_rx_3: blk=%d, down_total=%d, tot_sent=%d\n", blockssent, down_total, tot_sent);

				lcd_rxdma();		// get any new characters received
				for (i = 0; i < 2000; i++) {
					ch = lcd_getc();
					if (ch >= 0) {
						if (ch == 0x05) {
//							printf("ACK\n");
							break;
						} else {
							printf("Not Ack, was %d\n", ch);		// ignore it otherwise
						}
					}
					osDelay(1);
					lcd_rxdma();		// get any new characters received
					if (i == 1999) {
						printf("MISSED ACK\n");
						ch = -1;
					}
				}

				if (ch < 0) {		// error
					nxt_abort = 1;
					printf("ABORT ERR ON ACK\n");
					return (-1);
				} else {
					nxt_blocksacked++;
				}
				bytesinblocksent = 0;		// start new block
				blockssent++;
			}
		}

//		printf("nxt_rx_5: blk=%d, down_total=%d, tot_sent=%d, qlentot=%d\n", blockssent, down_total, tot_sent, qlentot);
		down_total += q->len;		// downloaded but not necessarily all sent to lcd
		altcp_recved(pcb, p->tot_len);
		pbuf_free(p);
	}
//		p = p->next;
//		printf("nxt_rx_4: len=%d, tot=%d qlentot=%d\n",  len, down_total, qlentot);
	return (0);
}

// Get Nextion version and see if we are current
int nxt_check() {
	int res;

	if (nex_model[0] == '\0') {
//		printf("LCD Model number invalid\n)");
		lcd_init(9600);  // reset LCD to 9600 from current (unknown) speed
		lcd_getid();	// try again to read its model number etc
		printf("nxt_check: Trying to reset the LCD\n");
		osDelay(1500);
		lcd_init(230400);  // reset LCD to normal speed
		osDelay(100);
		if (nex_model[0] == '\0')
			return (-1);
	}

// find LCD sys0 value
	if (lcd_sys0 == -1) {
//		printf("LCD Buildno was invalid\n");
		return (-2);
	}

	return (lcd_sys0);
}

///  Check if LCD needs updating and update it if so
nxt_update() {
	if (nxt_check() == -1) {		// we could not identify LCD
		printf("nxt_update: LCD not identified\n");
	} else {
		if (lcdbuildno == -2) {		// LCD user firmware might be corrupted
			printf("LCD firmware corrupted?\n");
		}
#if 1
		if (((lcd_sys0 >> 8) != BUILDNO) ||		// this LCD matches the wrong STM build number
				(((lcd_sys0 & 0xff) != lcdbuildno)		// OR lcdbuildno != latest lcdbuildno  AND
				&& ((lcd_sys0 >> 8) == BUILDNO)))			// its the same buildno as the STM
#else
		if (1)
#endif
		{

			if ((lcd_sys0 >> 8) != BUILDNO) {
				printf("nxt_update: Our STM build is %d, LCD is for STM build %d \n",  BUILDNO, (lcd_sys0 >> 8));
			}
			if ((lcd_sys0 & 0xff) != lcdbuildno) {
				printf("nxt_update: LCD's build is %d, server's LCD build is %d\n", lcd_sys0 & 0xff, lcdbuildno);
			}

			// do the load

			if (nxt_loader(fwfilename, loaderhost, lcdlen) == 0) {		// valid source file
				while ((http_downloading) && (nxt_abort == 0)) {
					HAL_IWDG_Refresh(&hiwdg);
					osDelay(5);
				}
				osDelay(2000);
				printf("Attempting LCD re-sync\n");
				lcd_init(230400);	// resync hardware
				osDelay(200);
				lcd_putsys0((BUILDNO << 8) | (lcdbuildno & 0xff));//  write back this new lcd build ver (NON VOLATILE IN LCD)
			}
			lcd_txblocked = 0;		// unblock LCD sending blocked
		} else {
			printf("LCD firmware matched stm firmware\n");
			http_downloading = NOT_LOADING;
		}
	}
}

