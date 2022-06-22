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
	static char newfilename[48];

	dl_filecrc = 0;

//	printf("nextionloader: fliename=%s, host=%s, crc=%u\n",filename,host,crc);

	sprintf(newfilename, "/firmware/%s-%04u.tft", filename, newbuild);
	printf("Attempting to download new Nextion firmware %s from %s, ******* DO NOT SWITCH OFF ******\n", newfilename,
			host);
	osDelay(600);
	http_downloading = NXT_LOADING;		// mode == nextion download
	nxt_abort = 0;

#define TFTSIZE 1971204

	nxt_blocksacked = 0;
	lcd_startdl(TFTSIZE);		// put LCD into upload firmware mode	ZZZ filesize
	osDelay(600);				// wait half a second for LCD to Ack
	if (nxt_blocksacked) {		// LCD acks the start, its now in DL mode
		nxt_blocksacked = 0;		// reset counter
		http_dlclient(newfilename, host, (void*) 0);
	} else {
		http_downloading = NOT_LOADING;
		printf("nextionloader: Nextion download not acked start\n");
	}

	// wait for transfer to complete
	// unblock http client
	osDelay(5);
	printf("nextionloader: exit\n");
}

//#define lcd_writeblock(nxtbuffer, residual) printf("%d ",residual)

// http callback for Nextion firmware download
// this gets called for each downloaded chunk received
//
int nxt_rx_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
	char *buf;
	struct pbuf *q;
	volatile int i, pktlen, res, tlen = 0, len = 0, ch;
	static int residual, blockssent = 0;
	static int bytesinblocksent = 0, qlentot = 0, tot_sent = 0;

//	printf("nxt_rx_callback:\n");

	LWIP_ASSERT("p != NULL", p != NULL);
	if (err != ERR_OK) {
		putchar('@');
		printlwiperr(err);
		return;
	}

//	printf("nxt_rx_callback1: nxt_abort=%d, blockssent=%d, nxt_blocksacked=%d, q->len=%d\n", nxt_abort, blockssent,	nxt_blocksacked, p->len);

	if (nxt_abort)
		return (-1);

	i = 0;
#if 0
	while (blockssent != nxt_blocksacked) {
		osDelay(10);
		i++;
		if (i > 200) {		// 2 seconds
			printf("nxt_rx_1: LCD upload timed out; aborting\n");
			nxt_abort = 1;
			return (-1);
		}
	}
#endif
	for (q = p; q != NULL; q = q->next) {
		qlentot += q->len;
		tlen = q->tot_len;
		len = q->len;

		if (nxt_abort == 0) { // we need to upload this data to the NXT

			if (residual) {				// residual data from last call to send first
				tot_sent += residual;
				if ((res = lcd_writeblock(nxtbuffer, residual)) == -1) {
					printf("NXT Write2 failed from http client\n");
					nxt_abort = 1;
					return (-1);
				}
				bytesinblocksent += residual;
			}

			pktlen = q->len;
			residual = 0;

			if ((pktlen + bytesinblocksent) > 4096) {	// will we will overflow the 4096 boundary?
				len = 4096 - bytesinblocksent;		// we only have to send 0.. len this time

				buf = q->payload;
				for (i = len; i < pktlen; i++) {		// copy the extra bytes we cant send into a buffer
					nxtbuffer[residual++] = buf[i];		// keep the rest back until next time
				}
				len = pktlen - residual;		// work out how many we can send now

			} else {
				len = pktlen;		// just send what we have got
			}

			tot_sent += len;
			if ((res = lcd_writeblock(q->payload, len) == -1)) {
				printf("NXT Write1 failed from http client\n");
				nxt_abort = 1;
				return (-1);
			}

			bytesinblocksent += len;

			if (bytesinblocksent == 4096) {
//				printf("nxt_rx_3: blk=%d, down_total=%d, tot_sent=%d\n",blockssent,down_total,tot_sent);

				lcd_rxdma();		// get any new characters received
				for (i = 0; i < 2000; i++) {
					ch = lcd_getc();
					if (ch >= 0) {
						if (ch == 0x05) {
							printf("ACK\n");
							break;
						} else {
							printf("Not Ack, was %d\n", ch);		// ignore it otherwise
						}
					}
					osDelay(1);
					lcd_rxdma();		// get any new characters received
					if (i == 1999) {
						printf("AMISSED ACK\n");
						ch = -1;
					}

					bytesinblocksent = 0;		// start new block
					blockssent++;
				}
				if (ch < 0) {		// error
					nxt_abort = 1;
					printf("ABORT ERR ON ACK\n");
					return (-1);
				} else {
					nxt_blocksacked++;
				}
			}
		}
		printf("nxt_rx_5: blk=%d, down_total=%d, tot_sent=%d, qlentot=%d\n", blockssent, down_total, tot_sent, qlentot);
		down_total += q->len;			// downloaded but not necessarily all sent to lcd
		altcp_recved(pcb, p->tot_len);
		pbuf_free(p);

//		p = p->next;
//		printf("nxt_rx_4: len=%d, tot=%d qlentot=%d\n",  len, down_total, qlentot);
	}
	return (0);
}

