/*
 * tftploader.c
 *
 *  Created on: 4 May 2022
 *      Author: bob
 */

/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Dirk Ziegelmeier <dziegel@gmx.de>
 *
 */

#include <stdio.h>

#include "tftp/tftp_client.h"
#include "tftp/tftp_server.h"
#include "tftp/tftp_loader.h"

#include <string.h>
#include "neo7m.h"
#include "eeprom.h"

#if LWIP_UDP


static void* tftp_open_mem(const unsigned int memaddress, u8_t is_write) {
	void *basememptr;
	uint32_t myaddr;

	if (is_write) {
		myaddr = (uint32_t) tftp_open_mem & LOADER_BASE_MEM2;				// find which 1M segment we are now running in
		if ((memaddress & LOADER_BASE_MEM2) != myaddr) {	// dont allow write to this segment!
			basememptr = (void*) memaddress;
			return (basememptr);		// write
		} else
			return (0);
	} else {
		return (0);						// not implemented
		//   return (void*)memopen(memptr, 0);		// read
	}
}

static void* tftp_open(const char *fname, const char *mode, u8_t is_write) {
	LWIP_UNUSED_ARG(mode);
	return tftp_open_mem(fname, is_write);
}

static void tftp_close(void *memptr) {
	memclose(memptr);
}

static int tftp_read(void *memptr, void *buf, int bytes) {
	int ret;

	ret = memread(buf, 1, bytes, (void*) memptr);
	if (ret <= 0) {
		return -1;
	}
	return ret;
}

static int tftp_write(void *memptr, struct pbuf *p) {
	putchar('.');
	while (p != NULL) {
		if (memwrite(p->payload, 1, p->len, memptr) != (size_t) p->len) {
			return -1;
		}
		p = p->next;
	}
	return 0;
}

/* For TFTP client only */
static void tftp_error(void *memptr, int err, const char *msg, int size) {
	char message[100];

	LWIP_UNUSED_ARG(memptr);

	memset(message, 0, sizeof(message));
	MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

	printf("TFTP host error: %d (%s)\n", err, message);
	flash_abort = 1;
}

static const struct tftp_context tftp = { tftp_open, tftp_close, tftp_read, tftp_write, tftp_error };



void tftp_client(char *filename, char *hostip) {
	void *mptr;
	err_t err;
	ip_addr_t srv;

	printf("+++++++++++++ tftp_init_client: start, host=%s\n",hostip);

	flash_abort = 0;
	int ret = ipaddr_aton(hostip, &srv);
	LWIP_ASSERT("ipaddr_aton failed", ret == 1);

	err = tftp_init_client(&tftp);
	if ((err != ERR_OK) && (err != ERR_USE))		// ERR_USE might be subsequent call
		LWIP_ASSERT("tftp_init_client failed", err == ERR_OK);

	mptr = tftp_open_mem(flash_load_address, 1);
	LWIP_ASSERT("failed to create memory", mptr != NULL);

	flash_memptr = mptr;
	flash_filelength = 0;

	err = tftp_get(mptr, &srv, TFTP_PORT, filename, TFTP_MODE_OCTET);
	LWIP_ASSERT("tftp_get failed", err == ERR_OK);

//	printf("+++++++++++++ tftp_init_client: end\n");
}

// attempt to load new firmware
void tftloader(char filename[], char hostip[], uint32_t crc1, uint32_t crc2) {
	static char newfilename[24];
	int i;
	volatile uint32_t addr;
	char segment;

	dl_filecrc = 0;

	addr = (uint32_t)tftloader & LOADER_BASE_MEM2; 	// where are we running this code?
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
		printf("tftloader: bad load address\n");
		return;
	}

	sprintf(newfilename, "%s-%c%02u-%04u.bin", filename, segment, circuitboardpcb, newbuild);
	printf("Attempting to download new firmware %s : *******  do not switch off *******\n", newfilename);

	tftp_client(newfilename, hostip);
}

#endif /* LWIP_UDP */
