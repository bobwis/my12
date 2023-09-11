/*
 * eeprom.h
 *
 *  Created on: 19Jun.,2018
 *      Author: bob
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include "stm32f7xx_hal.h"

/*
 RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 512K
 FLASH (rx)     : ORIGIN = 0x8010000, LENGTH = 1984 // 2048K
 EEPROM (rx)    : ORIGIN = 0x8008000, LENGTH = 256K  // simulated eeprom
 */
#define EEPROM_START_ADDRESS 0x870000
#define PAGE_SIZE 32768		// obsolete donot use with start address not in first 4 sectors

#define FLASH_START_ADDRESS 0x8000000
#define FLASH_END_ADDRESS   0x81FFFFF

#define LOADER_BASE_MEM1	0x8000000			// 1st 1M Flash
#define LOADER_BASE_MEM2	0x8100000			// 2nd 1M Flash

extern int noterased;		// 1 == not flashed,  0 = flashed

extern void *flash_memptr;
extern int flash_filelength;
extern int flash_abort;
extern uint32_t dl_filecrc;
extern uint32_t flash_load_address;
extern uint32_t http_content_len;
extern int flash_abort;
extern int expectedapage;
extern int http_downloading;


extern  HAL_StatusTypeDef EraseFlash(void *memptr);
extern unsigned int xcrc32(const unsigned char *buf, int len, unsigned int init);
extern uint32_t flash_findcrc(void *base, int length);

extern int down_total;

#endif /* EEPROM_H_ */
