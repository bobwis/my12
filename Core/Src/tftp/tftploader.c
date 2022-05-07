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

#if LWIP_UDP


/* Define this to a file to get via tftp client */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_FILENAME
#define LWIP_TFTP_EXAMPLE_CLIENT_FILENAME "test.bin"
#endif

/* Define this to a server IP string */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP
#define LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP "192.168.0.1"
#endif

#define TFTP_BASE_MEM	0x280000

static char filename[32];

static void* memclose() {

}


static void* memread() {

}

static void *
tftp_open_mem(const unsigned int memaddress, u8_t is_write)
{
	void* memptr;

  if (is_write) {
	  if (memaddress == TFTP_BASE_MEM) {			// only allow this memory address
		  memptr = TFTP_BASE_MEM;
		  return (memptr);		// write
	  }
	  else
		  return(0);
  } else {
	  return(0);						// not implemented
 //   return (void*)memopen(memptr, 0);		// read
  }
}

static void*
tftp_open(const char* fname, const char* mode, u8_t is_write)
{
  LWIP_UNUSED_ARG(mode);
  return tftp_open_mem(fname, is_write);
}

static void
tftp_close(void* memptr)
{
  memclose(memptr);
}

static int
tftp_read(void* memptr, void* buf, int bytes)
{
  int ret = memread(buf, 1, bytes, memptr);
  if (ret <= 0) {
    return -1;
  }
  return ret;
}

static int
tftp_write(void* memptr, struct pbuf* p)
{
  while (p != NULL) {
    if (fwrite(p->payload, 1, p->len, memptr) != (size_t)p->len) {
      return -1;
    }
    p = p->next;
  }

  return 0;
}

/* For TFTP client only */
static void
tftp_error(void* memptr, int err, const char* msg, int size)
{
  char message[100];

  LWIP_UNUSED_ARG(memptr);

  memset(message, 0, sizeof(message));
  MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

  printf("TFTP error: %d (%s)", err, message);
}

static const struct tftp_context tftp = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write,
  tftp_error
};

void
tftp_example_init_server(void)
{
  tftp_init_server(&tftp);
}

void
tftp_example_init_client(void)
{
  void *f;
  err_t err;
  ip_addr_t srv;
  int ret = ipaddr_aton(LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP, &srv);
  LWIP_ASSERT("ipaddr_aton failed", ret == 1);

  err = tftp_init_client(&tftp);
  LWIP_ASSERT("tftp_init_client failed", err == ERR_OK);

  f = tftp_open_mem(TFTP_BASE_MEM, 1);
  LWIP_ASSERT("failed to create file", f != NULL);

  err = tftp_get(f, &srv, TFTP_PORT, LWIP_TFTP_EXAMPLE_CLIENT_FILENAME, TFTP_MODE_OCTET);
  LWIP_ASSERT("tftp_get failed", err == ERR_OK);
}

#endif /* LWIP_UDP */
