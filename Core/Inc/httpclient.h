/*
 * httpclient.h
 *
 *  Created on: 6Jun.,2018
 *      Author: bob
 */
/*
 HTTP CLIENT FOR RAW LWIP
 (c) 2008-2009 Noyens Kenneth
 PUBLIC VERSION V0.2 16/05/2009

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE Version 2.1 as published by
 the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#ifndef __HTTPCLIENT_H
#define __HTTPCLIENT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "main.h"
#include "stm32f7xx_hal.h"
#include "lwip.h"
#include "httpd_structs.h"
#include "www.h"
#include "neo7m.h"
#include "udpstream.h"
#include "splat1.h"
#include "adcstream.h"

#include "eeprom.h"
#include "tftp/tftp_loader.h"

#include "ethernet.h"

//#include "inc/hw_memmap.h"
//#include "inc/hw_types.h"

// http rx callback mode
#define NOT_LOADING 0
#define FLASH_LOADING 1
#define NXT_LOADING 2

// You can replace this enum for saving MEMORY (replace with define's)
typedef enum {
	OK, OUT_MEM, TIMEOUT, NOT_FOUND, GEN_ERROR
} hc_errormsg;

struct hc_state {
	u8_t Num;
	char *Page;
	char *PostVars;
	char *RecvData;
	u16_t Len;
	u8_t ConnectionTimeout;
	void (*ReturnPage)(u8_t num, hc_errormsg, char *data, u16_t len);
};

// Nextion http download buffer size
#define NXDL_BUFF_SIZE 600
#define DOWNLOAD_PORT 8083


// Public function
int hc_open(char *servername, char *page, char Postvars, void *returpage);
void http_dlclient(char *filename, char *host, void *flash_memptr);

#endif //  __HTTPCLIENT_H
