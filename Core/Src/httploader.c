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

#include "eeprom.h"
#include "tftp/tftp_loader.h"


// attempt to load new firmware
void httploader(char filename[], char host[], uint32_t crc1, uint32_t crc2) {
	static char newfilename[24];
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
	printf("*** Attempting to download new firmware %s to 0x%08x from %s, do not switch off ***\n", newfilename, flash_memptr, host);

	http_dlclient(newfilename, host, flash_memptr);
	osDelay(1);
}

