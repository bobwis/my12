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

#if LWIP_UDP

/* Define this to a file to get via tftp client */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_FILENAME
#define LWIP_TFTP_EXAMPLE_CLIENT_FILENAME "test.bin"
#endif

/* Define this to a server IP string */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP
#define LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP "10.10.201.124"
#endif

#define TFTP_BASE_MEM	0x8100000			// 2nd 1M Flash

extern void EraseFlash(void* memptr);

static void *memptr = (void*) 0;
static int filelength = 0;
static uint32_t load_address = 0;


// close 'handle'
static void* memclose() {

	printf("tftp memclose: filelength=%d, memptr=0x%0x\n", filelength, (unsigned int) memptr);
	osDelay(1000);
	if (LockFlash() != HAL_OK) {
		printf("tftp: flash2 failed\n");
		return ((void*) 0);
	}
}

// not implemented
static void* memread() {

}

// memwrite - this writes an unspecified block size to Flash (with verification)
// assume mem is pointing at byte array
static int memwrite(const uint8_t buf[], size_t size, size_t len, volatile void *mem) {
	int i, j, res;
	volatile uint32_t data;


	filelength += (int) len;

//	for (i = 0; i < len; i++) {
//		printf(" %02x", buf[i]);
//	}
//	printf("\n");

	for (i = 0; i < len;) {		// avoid read buffer overflow
		data = 0;
		for (j = 0; j < 4; j++) {
			data >>= 8;
			data |= (i < len) ? (buf[i++] << 24) : 0;
		}

//		printf("memptr=%08x, data[%d]=%08x\n", (uint32_t) memptr, i, data);
		if ((res = WriteFlashWord(memptr, data)) != 0) {
			printf("memwrite: WriteFlash error\n");
			return(-1);
		}
		if (*(uint32_t*) memptr != data) {
			printf("memwrite: Readback error at %08x\n", memptr);
			return(-1);
		}
		memptr += 4;
	}
///	memptr += len;
//	printf("memwrite: buf=0x%0x, size=%d, size_=%d, memptr=0x%x\n",(uint32_t)buf,size,len,(uint32_t)mem);
	return ((int) len);
}

static void* tftp_open_mem(const unsigned int memaddress, u8_t is_write) {
	void *basememptr;
	uint32_t  myaddr;

	if (is_write) {
		myaddr = (uint32_t)tftp_open_mem & 0x8100000;				// find which 1M segment we are now running in
		if ((memaddress & 0x8100000) != myaddr) {	// dont allow write to this segment!
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

	printf("TFTP error: %d (%s)", err, message);
}

static const struct tftp_context tftp = { tftp_open, tftp_close, tftp_read, tftp_write, tftp_error };


// unused
void tftp_example_init_server(void) {
	tftp_init_server(&tftp);
}

void tftp_client(void) {
	void *mptr;
	err_t err;
	ip_addr_t srv;

	printf("+++++++++++++ tftp_init_client: start\n");

	int ret = ipaddr_aton(LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP, &srv);
	LWIP_ASSERT("ipaddr_aton failed", ret == 1);

	err = tftp_init_client(&tftp);
	if ((err != ERR_OK) && (err != ERR_USE))		// ERR_USE might be subsequent call
		LWIP_ASSERT("tftp_init_client failed", err == ERR_OK);

	mptr = tftp_open_mem(load_address, 1);
	LWIP_ASSERT("failed to create memory", mptr != NULL);
	if (mptr == NULL)
		return;

	memptr = mptr;
	filelength = 0;

	EraseFlash(memptr);
	err = tftp_get(mptr, &srv, TFTP_PORT, LWIP_TFTP_EXAMPLE_CLIENT_FILENAME, TFTP_MODE_OCTET);
	LWIP_ASSERT("tftp_get failed", err == ERR_OK);

	printf("+++++++++++++ tftp_init_client: end\n");
}

// attempt to load new firmware
void tftloader(char filename[], uint32_t crc)
{
static char newfilename[24];
int i;

sprintf(newfilename,"%s-%02u-%04u.bin",filename,circuitboardpcb,newbuild);
printf("**************** Attempting to download new firmware - do not switch off *************\n");
printf("Trying to TFTPload %s\n",newfilename);
load_address = TFTP_BASE_MEM;
tftp_client();
}


#endif /* LWIP_UDP */
