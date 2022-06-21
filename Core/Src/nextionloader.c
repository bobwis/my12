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

#include "Nextionloader.h"

int nxt_abort = 0;			// 1 == abort
int nxt_blocksacked = 0;	// number of acks recieved by the LCD (every 4k bytes)
char nxtbuffer[NXDL_BUFF_SIZE];


// attempt to load new LCD user firmware
void nextionloader(char filename[], char host[], uint32_t crc) {
	static char newfilename[24];
	volatile uint32_t count;

	dl_filecrc = 0;
	count = 0;

	printf("nextionloader: fliename=%s, host=%s, crc=%u\n",filename,host,crc);

	sprintf(newfilename, "/firmware/%s-%04u.tft", filename, newbuild);
	printf("Attempting to download new Nextion firmware %s from %s, ******* DO NOT SWITCH OFF ******\n", newfilename, host);

	http_downloading = NXT_LOADING;		// mode == nextion download

#define TFTSIZE 1971204

	lcd_startdl(TFTSIZE);		// put LCD into upload firmware mode	ZZZ filesize
	http_dlclient(newfilename, host, (void *)0);

	// wait for transfer to complete
	// unblock http client
	osDelay(5);
}


// http callback for Nextion firmware download
// this gets called for each downloaded chunk received
//
int nxt_rx_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
	char *buf;
	struct pbuf *q;
	int i, res, count = 0, tlen = 0, len = 0;
	static int residual, blockssent = 0;
	static int bytesinblocksent = 0;

	printf("nxt_rx_callback:\n");

	LWIP_ASSERT("p != NULL", p != NULL);
	if (err != ERR_OK) {
		putchar('#');
		printlwiperr(err);
		return;
	}

	if (nxt_abort)
		return(-1);

	i = 0;
	while (blockssent != nxt_blocksacked) {
		osDelay(1);
		i++;
		if (i > 1000) {
			printf("nxt_rx_callback: LCD upload timed out; aborting\n");
			nxt_abort = 1;
			return(-1);
		}
	}

	for (q = p; q != NULL; q = q->next) {
		count += q->len;
		tlen = q->tot_len;
		len = q->len;
#if 0
		putchar('.');
#endif
		if (nxt_abort == 0)  { // we need to upload this data to the NXT
			if (q->len > 4096) {
				printf("nxt_rx_callback: tried to write > 4096\n");
				nxt_abort = 1;
				return(-1);
			}

			if (residual) {				// residual data from last call to send first
				if ((res=lcd_writeblock(nxtbuffer,residual)) == -1) {
					printf("NXT Write2 failed from http client\n");
					nxt_abort = 1;
					return(-1);
				}
				bytesinblocksent += residual;
			}

			residual = 0;
			if ((q->len + bytesinblocksent) > 4096) {	// will we will overflow the 4096 boundary?
				len = 4096 - bytesinblocksent;		// we only have to send 0.. len this time

				buf = q->payload;
				for (i = len; i < q->len; i++) {		// copy the extra bytes we cant send into a buffer
					nxtbuffer[residual++] = buf[i];		// keep the rest back until next time
				}
				len = q->len - residual;		// work out how many we can send now

			} else {
				len = q->len;		// just send what we have got
			}


			if ((res=lcd_writeblock(q->payload,len) == -1)) {
				printf("NXT Write1 failed from http client\n");
				nxt_abort = 1;
				return(-1);
			}
			bytesinblocksent += len;
			if (bytesinblocksent == 4096) {
				printf("nxt_rx_callback: block sent\n");
				bytesinblocksent = 0;		// start new block
				blockssent++;
			}
		}

		down_total += q->len;			// downloaded but not necessarily all sent to lcd
		altcp_recved(pcb, p->tot_len);
		pbuf_free(p);

//		p = p->next;
		printf("nxt_rx_callback: chunk=%d, tlen=%d, len=%d, total=%d\n", count, tlen, len, tlen);
	}
	return (0);
}
