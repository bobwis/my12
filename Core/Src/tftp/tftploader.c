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


extern void EraseFlash(void *memptr);
extern xcrc32(const unsigned char *buf, int len, unsigned int init);

static void *memptr = (void*) 0;
static int filelength = 0;
static int tftabort = 0;
static uint32_t load_address = TFTP_BASE_MEM1;

int notflashed = 1;		// 1 == not flashed,  0 = flashed

// calculate the crc over a range of memory
uint32_t findcrc(void *base, int length) {
	uint32_t crc, xinit = 0xffffffff;

	crc = xcrc32(base, length, xinit);
//	printf("findcrc: crc=0x%08x, base=0x%08x, len=%d\n", crc, base, length);
	return (crc);
}

// close 'handle'
static void* memclose() {
	uint32_t xcrc;
	static FLASH_OBProgramInitTypeDef OBInitStruct;
	HAL_StatusTypeDef res;

	if (tftabort) {
		tftabort = 0;
		return;
	}

	printf("tftp memclose: filelength=%d, memptr=0x%0x\n", filelength, (unsigned int) memptr);
	osDelay(1000);
	if (LockFlash() != HAL_OK) {
		printf("tftp: flash2 failed\n");
		return ((void*) 0);
	}

	xcrc = findcrc(memptr - filelength, filelength);
	if ((filecrc != xcrc) && (filecrc != 0xffffffff)) {
		printf("****************** Downloaded file/ROM CRC check failed crc=0x%x **********\n", xcrc);
	} else {
		HAL_FLASHEx_OBGetConfig(&OBInitStruct);

		HAL_FLASH_OB_Unlock();
		OBInitStruct.BootAddr0 = ((memptr - filelength) == TFTP_BASE_MEM1) ? 0x2000 : 0x2040;
		OBInitStruct.BootAddr1 = ((memptr - filelength) == TFTP_BASE_MEM1) ? 0x2040 : 0x2000;

		res = HAL_FLASHEx_OBProgram(&OBInitStruct);
		if (res != HAL_OK) {
			printf("memclose: failed to OBProgram %d\n", res);
		}

		res = HAL_FLASH_OB_Launch();
		if (res != HAL_OK) {
			printf("memclose: failed to OBLaunch %d\n", res);
		}

//		*(uint32_t *)(0x1FFF0010) = ((memptr - filelength) == TFTP_BASE_MEM1) ? 0x0080 : 0x00c0;
		HAL_FLASH_OB_Lock();
		printf("New FLASH image loaded; rebooting please wait...\n");
		osDelay(50);
		rebootme(0);
	}
#if 0
	{
		uint32_t *mem1, *mem2, d1, d2, i;

		xcrc = findcrc(TFTP_BASE_MEM1, filelength);		// debug
		printf("TFTP_BASE_MEM1 CRC=0x%x\n", xcrc);

		xcrc = findcrc(TFTP_BASE_MEM2, filelength);		// debug
		printf("TFTP_BASE_MEM2 CRC=0x%x\n", xcrc);

		mem1 = TFTP_BASE_MEM1;
		mem2 = TFTP_BASE_MEM2;
		for (i = 0; i < filelength; i += 4) {
			d1 = *mem1++;
			d2 = *mem2++;
			if (d1 != d2) {
				mem1--;
				mem2--;
				printf("memclose1: 0x%08x[0x%08x], 0x%08x[0x%08x], d1=0x%08x, d2=0x%08x\n", mem1, *mem1, mem2, *mem2,
						d1, d2);
				mem1++;
				mem2++;
			}
		}
	}
#endif
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

#if 0
////////////////////////////////////////////////
	static int totlen = 0, count = 0;

totlen += len;
count++;
printf("memwrite: count=%d, memptr=0x%x, totlen=%d, len=%d\n",count, memptr, totlen, len);

//	for (i = 0; i < len; i++) {
//		printf(" %02x", buf[i]);
//	}
//	printf("\n");
//////////////////////////////////////////////////////
#endif

	if ((!(tftabort)) && (notflashed)) {
		EraseFlash(memptr);
		notflashed = 0;
	}

	for (i = 0; i < len;) {		// avoid read buffer overflow
		data = 0;
		for (j = 0; j < 4; j++) {
			data >>= 8;
			data |= (i < len) ? (buf[i++] << 24) : 0;
		}

//		printf("memptr=%08x, data[%d]=%08x\n", (uint32_t) memptr, i, data);
		if ((res = WriteFlashWord(memptr, data)) != 0) {
			printf("memwrite: WriteFlash error\n");
			return (-1);
		}
		if (*(uint32_t*) memptr != data) {
			printf("memwrite: Readback error at %08x\n", memptr);
			return (-1);
		}
		memptr += 4;
	}
///	memptr += len;
//	printf("memwrite: buf=0x%0x, size=%d, size_=%d, memptr=0x%x\n",(uint32_t)buf,size,len,(uint32_t)mem);
	return ((int) len);
}

static void* tftp_open_mem(const unsigned int memaddress, u8_t is_write) {
	void *basememptr;
	uint32_t myaddr;

	if (is_write) {
		myaddr = (uint32_t) tftp_open_mem & TFTP_BASE_MEM2;				// find which 1M segment we are now running in
		if ((memaddress & TFTP_BASE_MEM2) != myaddr) {	// dont allow write to this segment!
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
	tftabort = 1;
}

static const struct tftp_context tftp = { tftp_open, tftp_close, tftp_read, tftp_write, tftp_error };

// unused
void tftp_example_init_server(void) {
	tftp_init_server(&tftp);
}

void tftp_client(char *filename, char *hostip) {
	void *mptr;
	err_t err;
	ip_addr_t srv;

	printf("+++++++++++++ tftp_init_client: start, host=%s\n",hostip);

	tftabort = 0;
	int ret = ipaddr_aton(hostip, &srv);
	LWIP_ASSERT("ipaddr_aton failed", ret == 1);

	err = tftp_init_client(&tftp);
	if ((err != ERR_OK) && (err != ERR_USE))		// ERR_USE might be subsequent call
		LWIP_ASSERT("tftp_init_client failed", err == ERR_OK);

	mptr = tftp_open_mem(load_address, 1);
	LWIP_ASSERT("failed to create memory", mptr != NULL);

	memptr = mptr;
	filelength = 0;

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

	filecrc = 0;

	addr = (uint32_t)tftloader & TFTP_BASE_MEM2; 	// where are we running this code?
	load_address = (addr == TFTP_BASE_MEM1) ? TFTP_BASE_MEM2 : TFTP_BASE_MEM1; // find the other segment

	switch (load_address) {		// assign a code letter for the load address filename
	case TFTP_BASE_MEM1:
		segment = 'A';
		filecrc = crc1;
		break;
	case TFTP_BASE_MEM2:
		segment = 'I';
		filecrc = crc2;
		break;
	default:
		printf("tftloader: bad load address\n");
		return;
	}

	sprintf(newfilename, "%s-%c%02u-%04u.bin", filename, segment, circuitboardpcb, newbuild);
	printf("*** Attempting to download new firmware %s : do not switch off ***\n", newfilename);

	tftp_client(newfilename, hostip);
}

#endif /* LWIP_UDP */
