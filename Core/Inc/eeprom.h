/*
 * eeprom.h
 *
 *  Created on: 19Jun.,2018
 *      Author: bob
 */

#ifndef EEPROM_H_
#define EEPROM_H_

/*
 RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 512K
 FLASH (rx)     : ORIGIN = 0x8010000, LENGTH = 1984 // 2048K
 EEPROM (rx)    : ORIGIN = 0x8008000, LENGTH = 256K  // simulated eeprom
 */
#define EEPROM_START_ADDRESS 0x870000
#define PAGE_SIZE 32768		// obsolete donot use with start address not in first 4 sectors

#define FLASH_START_ADDRESS 0x8000000
#define FLASH_END_ADDRESS   0x81FFFFF

#define TFTP_BASE_MEM1	0x8000000			// 1st 1M Flash
#define TFTP_BASE_MEM2	0x8100000			// 2nd 1M Flash

extern int notflashed;		// 1 == not flashed,  0 = flashed

#endif /* EEPROM_H_ */
