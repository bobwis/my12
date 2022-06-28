/*
 * httploader.c
 *
 *  Created on: 28 May 2022
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
#include "altcp.h"

#include "eeprom.h"
#include "tftp/tftp_loader.h"

int http_downloading = NOT_LOADING;

// attempt to load new firmware
void httploader(char filename[], char host[], uint32_t crc1, uint32_t crc2) {
	static char newfilename[48];
	volatile uint32_t addr;
	char segment;

	dl_filecrc = 0;

	addr = (uint32_t)httploader & LOADER_BASE_MEM2; 	// where are we running this code?
	flash_load_address = (addr == LOADER_BASE_MEM1) ? LOADER_BASE_MEM2 : LOADER_BASE_MEM1; // find the other segment

	switch (flash_load_address) {		// assign a code letter for the load address filename
	case LOADER_BASE_MEM1:
		segment = 'A';
		dl_filecrc = crc1;
		break;
	case LOADER_BASE_MEM2:
		segment = 'I';
		dl_filecrc = crc2;
		break;
	default:
		printf("httploader: bad load address\n");
		return;
	}

	printf("httploader: fliename=%s, host=%s, crc1=%u, crc2=%u\n",filename,host,crc1,crc2);

	flash_memptr = flash_load_address;
	flash_filelength = 0;

	sprintf(newfilename, "/firmware/%s-%c%02u-%04u.bin", filename, segment, circuitboardpcb, newbuild);
	printf("Attempting to download new firmware %s to 0x%08x from %s, ******* DO NOT SWITCH OFF ******\n", newfilename, flash_memptr, host);

	http_downloading = FLASH_LOADING;
	http_dlclient(newfilename, host, flash_memptr);
	osDelay(5);
}


// http callback for stm firmware download
// this gets called for each downloaded chunk received
//
int stm_rx_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
	char *buf;
	struct pbuf *q;
	int count = 0, tlen = 0, len = 0;

//	printf("stm_rx_callback:\n");

	LWIP_ASSERT("p != NULL", p != NULL);
	if (err != ERR_OK) {
		putchar('#');
		printlwiperr(err);
		return;
	}

	for (q = p; q != NULL; q = q->next) {
		count += q->len;
		tlen = q->tot_len;
		len = q->len;
#if 0
		putchar('.');
#endif
		if ((flash_abort == 0) && (flash_memptr != 0)) { // we need to write this data to flash
			if (flash_memwrite(q->payload, 1, q->len, flash_memptr) != (size_t) len) {
				flash_abort = 1;
				flash_memptr = 0;
				printf("Flash Write failed from http client\n");
				return (-1);
			}
		}
		down_total += q->len;

		altcp_recved(pcb, p->tot_len);
		pbuf_free(p);

//		p = p->next;
//		printf("stm_rx_callback: chunk=%d, tlen=%d, len=%d, total=%d\n", count, tlen, len, tlen);
	}
	return (0);
}

