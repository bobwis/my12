/*
 * eeprom.c
 *
 *  Created on: 19Jun.,2018
 *      Author: bob
 */

#include <stdio.h>

// emulate eeprom using STM32 flash memory
// based on http://www.sonic2kworld.com/blog/using-stm32f-flash-as-eeprom-the-easy-way (also derived from elsewhere)
// Included headers
//-----------------------------------
#include "eeprom.h"
#include "stm32f7xx_hal.h"

uint32_t flash_load_address = LOADER_BASE_MEM1;
void *flash_memptr = (void*) 0;
int flash_filelength = 0;
int flash_abort = 0;		// 1 == abort
uint32_t dl_filecrc = 0;
int notflashed = 1;		// 1 == not flashed,  0 = flashed
uint32_t q_bytes[4];	// memwrite buffer between calls
int q_index = 0;	// number of bytes queued for next memwrite
uint32_t *p, patt = 0x80010000;	// used for teting only zzz
// Functions
//-----------------------------------

//----------------------------------------------------------------------------------
// Name: ReadEE
// Function: Read data from the EEPROM
// Parameters: address, pointer to byte location where read data is to be stored
// Returns: 0
//----------------------------------------------------------------------------------
int ReadEE(uint32_t address, uint32_t *Data) {
	uint32_t Address = EEPROM_START_ADDRESS;								// Base address in FLASH
	uint32_t dataword;														// Temp storage for read from FLASH

	if (address > PAGE_SIZE) {
		return -1;													// Address is greater than EEPROM size, return -1
	}

	// Compute the read address into the FLASH memory
	// Passed parameter does not need to know about alignment and such
	Address = Address + (address & 0x3FFFC);									// Compute address from supplied address
	dataword = (*(__IO uint32_t*) (Address));// Retrieve dword from flash memory (Warning- it can only be dword boundaries)
	*Data = *(((uint32_t*) &dataword) + (address & 0x03));
	return 0;
}

//-----------------------------------------------------------------------------------
// Name: WriteEE
// Function: Write data to the EEPROM
// Parameter: relative EEPROM address, data
// Returns: Result of operation
//-----------------------------------------------------------------------------------
int WriteEE(uint32_t address, uint32_t Data) {
	uint32_t Address = EEPROM_START_ADDRESS;
	uint32_t dataword;

	if (address > PAGE_SIZE) {
		return -1;
	}

	// Compute destination write address in the FLASH
	Address = Address + (address & 0x3FFFC);

	/**
	 * @brief  Program byte, halfword, word or double word at a specified address
	 * @param  TypeProgram  Indicate the way to program at a specified address.
	 *                           This parameter can be a value of @ref FLASH_Type_Program
	 * @param  Address  specifies the address to be programmed.
	 * @param  Data specifies the data to be programmed
	 *
	 * @retval HAL_Status
	 *
	 * *TypeDef HAL Status
	 *TypeDef  HAL_OK       = 0x00U,
	 HAL_ERROR    = 0x01U,
	 HAL_BUSY     = 0x02U,
	 HAL_TIMEOUT  = 0x03U
	 } HAL_StatusTypeDef;*

	 #define FLASH_TYPEPROGRAM_BYTE        ((uint32_t)0x00U)  //!< Program byte (8-bit) at a specified address
	 #define FLASH_TYPEPROGRAM_HALFWORD    ((uint32_t)0x01U)  //!< Program a half-word (16-bit) at a specified address
	 #define FLASH_TYPEPROGRAM_WORD        ((uint32_t)0x02U)  //!< Program a word (32-bit) at a specified address
	 #define FLASH_TYPEPROGRAM_DOUBLEWORD  ((uint32_t)0x03U)  //!< Program a double word (64-bit) at a specified address
	 */
	if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, Data) != HAL_OK) {
		return -1;
	}
	return 0;
}

// test the eeprom emulation
void testeeprom(void) {
	int i;
	uint32_t data[64];

	osDelay(6000);
	printf("Reading EEPROM 1\n");
	for (i = 0; i < 64; i++) {
		ReadEE(i * 4, &(data[i]));
	}
	myhexDump("", data, 256);

	printf("Writing EEPROM\n");
	HAL_FLASH_Unlock();
	for (i = 0; i < 64; i++) {
		if (WriteEE(i * 4, (i << 24) | (i << 16) | (i << 8) | i)) {
			printf("WriteEE Failed at %d\n", i);
		}
	}
	HAL_FLASH_Lock();

	printf("Reading EEPROM 2\n");
	for (i = 0; i < 64; i++) {
		ReadEE(i * 4, &(data[i]));
	}
	myhexDump("", data, 256);

	while (1)
		;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///   GENERAL FLASH MEMORY WRITE ROUTINES (used by TFTP)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static FLASH_EraseInitTypeDef EraseInitStruct;
#include "eeprom.h"

// flash unlock
HAL_StatusTypeDef UnlockFlash() {
	HAL_StatusTypeDef res;

//	printf("UnlockFlash:\n");
	res = HAL_FLASH_Unlock();
	if (res != HAL_OK) {
		printf("UnlockFlash: failed to unlock 0x%x\n", res);
		printflasherr();
		return (res);
	}
	return (res);
}

HAL_StatusTypeDef LockFlash() {
	HAL_StatusTypeDef res;

//	printf("lockFlash:\n");
	res = HAL_FLASH_Lock();
	if (res != HAL_OK) {
		printf("LockFlash: failed to lock\n");
		printflasherr();
		return (res);
	}
	return (res);
}

// display the error
void printflasherr() {
	char *msg;
	uint32_t err;

	err = HAL_FLASH_GetError();

	switch (err) {
	case FLASH_ERROR_ERS:
		msg = "Erasing Sequence";
		break;
	case FLASH_ERROR_PGP:
		msg = "Programming Parallelism";
		break;
	case FLASH_ERROR_PGA:
		msg = "Programming alignment";
		break;
	case FLASH_ERROR_WRP:
		msg = "Write Protected";
		break;
	case FLASH_ERROR_OPERATION:
		msg = "Operation";
		break;
	default:
		msg = NULL;
		sprintf(msg, "Unknown err 0x%0x", err);
		break;
	}
	if (msg == NULL) {
		printf("Flash failed Unknown err 0x%0x\n", err);
	} else {
		printf("Flash operation failed: %s error\n", msg);
	}
	LockFlash();		// for safety
}

// erase flash sector(s)
// (start at sector corresponding to memptr, fixed erase of 512K)
HAL_StatusTypeDef EraseFlash(void *memptr) {
	HAL_StatusTypeDef res;
	uint32_t SectorError, *ptr;
	int dirty;

	if ((res = UnlockFlash()) != HAL_OK) {
		printf("EraseFlash: unlock failed\n");
		printflasherr();
	}

	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;		// should this be 2???

	if (((uint32_t) memptr & 0x8100000) == 0x8000000)	// the lower 512K
			{
		EraseInitStruct.Sector = FLASH_SECTOR_0;
		EraseInitStruct.NbSectors = 6;
	} else	// the upper 512M starting at 1M
	{
		EraseInitStruct.Sector = FLASH_SECTOR_8;
		EraseInitStruct.NbSectors = 2;
	}

	dirty = 0;
	for (ptr = memptr; ptr < (uint32_t) (memptr + 0x80000); ptr++) {		// 512K
		if (*ptr != 0xffffffff) {
			dirty = 1;
			break;
		}
	}

	if ((dirty) && (notflashed)) {
		osDelay(1000);
		printf("Erasing Flash for %d sector(s) from %d\n", EraseInitStruct.NbSectors, EraseInitStruct.Sector);

		EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
		EraseInitStruct.Banks = FLASH_BANK_1;
		EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

		res = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
		if (SectorError != 0xffffffff) {
			printf("Flash Erase failed sectorerror 0x%08x\n", SectorError);
		}
		if (res != HAL_OK) {
			printf("EraseFlash: failed\n");
			printflasherr();
			dirty = 1;
		} else {
			printf("Flash successfully erased\n");
			notflashed = 0;

			// check the erasure
			dirty = 0;
			for (ptr = memptr; ptr < (uint32_t) (memptr + 0x80000); ptr++) {		// 512K
				if (*ptr != 0xffffffff) {
					dirty = 1;
					break;
				}
			}
			if (dirty) {
				notflashed = 1;
				printf("*** ERROR: Flash was erased but bits still dirty at 0x%08x\n",ptr);
			}
		}

	} else {
		printf("Flash erase unnecessary\n");
	}
}

// write 32 bits
HAL_StatusTypeDef WriteFlashWord(uint32_t address, uint32_t data) {
	HAL_StatusTypeDef res;
	int trys;

	if (((int) address < FLASH_START_ADDRESS) || ((int) address > (FLASH_END_ADDRESS))) {
		printf("WriteFlash: failed address check\n");
		return (HAL_ERROR);
	}

	trys = 0;
	__HAL_FLASH_ART_DISABLE();
	while ((res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data) != HAL_OK)) {
		printflasherr();		// deleteme
		if (res == HAL_BUSY) {
			if (trys > 3) {
				__HAL_FLASH_ART_RESET();
				__HAL_FLASH_ART_ENABLE();
				printf("WriteFlashWord: failed write trys\n");
				return (HAL_ERROR);
			}
			trys++;
			osDelay(0);
			continue;
		} // if busy

		if (res != HAL_OK) {
			printflasherr();
			printf("WriteFlashWord: failed write at 0x%0x err=0x%x\n", address, res);
			__HAL_FLASH_ART_RESET();
			__HAL_FLASH_ART_ENABLE();
			return (res);
		}
	}
	__HAL_FLASH_ART_RESET();
	__HAL_FLASH_ART_ENABLE();

	if (*(uint32_t*) address != data) {
		printf("WriteFlashWord: Failed at 0x%08x with data=%08x, read=0x%08x\n", address, data, *(uint32_t*) address);
		return (HAL_ERROR);
	}
	return (HAL_OK);
}

// write sector of 32k bytes
// datablock must be 32k bytes even if valid data is not filling all
int WriteFlash32k(void *startadd, uint32_t *datablock) {
	HAL_StatusTypeDef res;
	int i;

	printf("progflash32k: \n");
	if (UnlockFlash() != HAL_OK) {
		return (-1);
	}
	if (((unsigned long) startadd & 0x7FFF) > 0) {
		printf("progflash32k: failed 32k boundary\n");
		return (-1);
	}
	for (i = 0; i < 0x2000; i++) {		// 0x2000 words is 0x8000 bytes
		if (res = WriteFlashWord(startadd + i, datablock[i])) {
			printf("WriteEE Failed at %d\n", startadd + i);
			return (-1);
		}
	}
	if (LockFlash() != HAL_OK) {
		return (-1);
		return (0);
	}
}

// make sure the boot vector points to this running program
void stampboot() {
	HAL_StatusTypeDef res;
	FLASH_OBProgramInitTypeDef OBInitStruct;
	uint32_t *newadd, options, addr;

	HAL_FLASHEx_OBGetConfig(&OBInitStruct);

	addr = (uint32_t) stampboot & LOADER_BASE_MEM2; 	// where are we running this code?
	newadd = (addr == LOADER_BASE_MEM1) ? 0x2000 : 0x2040;

	if (OBInitStruct.BootAddr0 != newadd) {
		HAL_FLASH_OB_Unlock();

		OBInitStruct.BootAddr0 = newadd;	// stamp the running boot address
		OBInitStruct.BootAddr1 = (OBInitStruct.BootAddr0 == 0x2000) ? 0x2040 : 0x2000;// flip alternate (this is only used if boot pin inverted)

		OBInitStruct.USERConfig |= FLASH_OPTCR_nDBOOT;		// disable mirrored flash dual boot
		OBInitStruct.USERConfig |= FLASH_OPTCR_nDBANK;

		res = HAL_FLASHEx_OBProgram(&OBInitStruct);
		if (res != HAL_OK) {
			printf("swapboot: failed to OBProgram %d\n", res);
		}

		res = HAL_FLASH_OB_Launch();
		if (res != HAL_OK) {
			printf("swapboot: failed to OBLaunch %d\n", res);
		}
		printf(".......re-stamped boot vector.......\n");
	}
}

/// fix up the boot vectors in the option flash
void swapboot() {
	HAL_StatusTypeDef res;
	FLASH_OBProgramInitTypeDef OBInitStruct;
	uint32_t *newadd, options;

	HAL_FLASHEx_OBGetConfig(&OBInitStruct);
	HAL_FLASH_OB_Unlock();

	// swap boot address (maybe)

	newadd = (OBInitStruct.BootAddr0 == 0x2000) ? 0x2040 : 0x2000;	// toggle boot segment start add
	if (*newadd != 0xffffffff) {	// if new area is not an empty region
		OBInitStruct.BootAddr0 = newadd;	// change boot address
	}
	OBInitStruct.BootAddr1 = (OBInitStruct.BootAddr0 == 0x2000) ? 0x2040 : 0x2000;// flip alternate (this is only used if boot pin inverted)

	OBInitStruct.USERConfig |= FLASH_OPTCR_nDBOOT;		// disable mirrored flash dual boot
	OBInitStruct.USERConfig |= FLASH_OPTCR_nDBANK;

	res = HAL_FLASHEx_OBProgram(&OBInitStruct);
	if (res != HAL_OK) {
		printf("swapboot: failed to OBProgram %d\n", res);
	}

	res = HAL_FLASH_OB_Launch();
	if (res != HAL_OK) {
		printf("swapboot: failed to OBLaunch %d\n", res);
	}
	printf("fixing boot....\n");
	HAL_FLASH_OB_Lock();

	printf("swapboot ran\n");
}

// not implemented
static void* memread() {

}

// write tp flash with data at memptr
int flash_writeword(uint32_t worddata) {
	HAL_StatusTypeDef res;

	if ((res = WriteFlashWord(flash_memptr, worddata)) != 0) {
		printf("memwrite: WriteFlash error\n");
		return (-1);
	}
	if (*(uint32_t*) flash_memptr != worddata) {
		printf("memwrite: Readback error at %08x\n", flash_memptr);
		return (-1);
	}
	return (0);
}

// flash_memwrite - this writes an unspecified block size to Flash (with verification)
// assume mem is pointing at byte array
int flash_memwrite(const uint8_t buf[], size_t size, size_t len, volatile void *mem) {
	volatile int i, j, k;
	volatile uint32_t data;
	HAL_StatusTypeDef res;
	static int lastbyte = 0;

	flash_filelength += (int) len;

#if 0
////////////////////////////////////////////////
	static int totlen = 0, count = 0;

totlen += len;
count++;
printf("memwrite: count=%d, memptr=0x%x, totlen=%d, len=%d\n",count, flash_memptr, totlen, len);

//	for (i = 0; i < len; i++) {
//		printf(" %02x", buf[i]);
//	}
//	printf("\n");
//////////////////////////////////////////////////////
#endif

	if ((!(flash_abort)) && (notflashed)) {
		res = EraseFlash(flash_memptr);
		notflashed = 0;
	}

#if 0
	if (len % 4 != 0) {
		printf("memwrite: len %d chunk not multiple of 4 at %u\n", len, flash_filelength);
	}
	if (len % 2 != 0) {
		printf("memwrite: len %d chunk not multiple of 2 at %u\n", len, flash_filelength);
	}
#endif
	if (len == 0) {
		printf("memwrite: len %d at %u\n", len, flash_filelength);
	}


	data = 0xffffffff;		// the 32 bit word we will write

	lastbyte = 0;
	if (q_index > 0) {		// some residual data from last time through here
		for (i = 0; i < q_index;) {
			data >>= 8;
			data |= (q_bytes[i++] << 24);
			lastbyte++;
		}
	}

	k = len % 4;		// see if buf fits full into 32 bit words

	for (i = 0; (i + q_index) < (len - k);) {		// take full words, avoid read buffer overflow
		for (j = lastbyte; j < 4; j++) {
			data >>= 8;
			data |= buf[i++] << 24;
		}
#if 0
		if (patt != data) {				///  zzz debug
			printf("memwrite: expected 0x%08x, found 0x%08x\n", flash_memptr, data);
		}
		patt += 4;
#endif
		lastbyte = 0;	// no more residual

		//		printf("memptr=%08x, data[%d]=%08x\n", (uint32_t) memptr, i, data);
		flash_writeword(data);

		flash_memptr += 4;
	}

	for (q_index = 0; i < len;) {
		q_bytes[q_index++] = buf[i++];		// put extra odd bytes in queue
	}

///	memptr += len;
//	printf("memwrite: buf=0x%0x, size=%d, size_=%d, memptr=0x%x\n",(uint32_t)buf,size,len,(uint32_t)mem);
	return ((int) len);
}

// close memory 'handle'
void* memclose() {
	uint32_t xcrc, residual;
	static FLASH_OBProgramInitTypeDef OBInitStruct;
	HAL_StatusTypeDef res;
	int i;

	notflashed = 1;		// now assumed dirty
	if (flash_abort) {
		flash_abort = 0;
		http_downloading = 0;
		return;
	}

	if (q_index > 0) {			// unfinished residual write still needed
		residual = 0;
		for (i = 0; i < 4; i++) {
			residual >>= 8;
			residual |= (q_bytes[i] << 24);
		}
		flash_writeword(residual);
	}

	printf("eeprom memclose: flash_load_addr=0x%08x, filelength=%d, flash_memptr=0x%0x total=%d\n", flash_load_address,
			flash_filelength, (unsigned int) flash_memptr, down_total);
	osDelay(1000);
	if (LockFlash() != HAL_OK) {
		printf("eeprom: flash2 failed\n");
		return ((void*) 0);
	}

	xcrc = flash_findcrc(flash_load_address, flash_filelength);
	if ((dl_filecrc != xcrc) && (dl_filecrc != 0xffffffff)) {
		printf(
				"\n****************** Downloaded file/ROM CRC check failed ourcrc=0x%08x, filecrc=0x%08x Total=%d **********\n",
				xcrc, dl_filecrc, down_total);
#if 0
////////////////////////////		// test debug zzz
		{
			int count = 0;

			patt = 0x80010000;
			for (i = 0; i < flash_filelength; i += 4) {
				p = (uint32_t*) (flash_load_address + i);
				if (*p != patt) {
					if (count < 8) {
						printf("patt failed at 0x%08x, read 0x%08x, should be 0x%0x8\n", (uint32_t) p, *p, patt);
					}
					count++;
				}
				patt += 4;
			}
		}
////////////////////////////////////////////
#endif
	} else {

		//if !(check firmwarsatckpointer)  check integrity
		osDelay(5);

		HAL_FLASHEx_OBGetConfig(&OBInitStruct);

		HAL_FLASH_OB_Unlock();
		OBInitStruct.BootAddr0 = (flash_load_address == LOADER_BASE_MEM1) ? 0x2000 : 0x2040;
		OBInitStruct.BootAddr1 = (flash_load_address == LOADER_BASE_MEM1) ? 0x2040 : 0x2000;

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
		printf("New FLASH image loaded; rebooting please wait 45 secs...\n");
		osDelay(50);
		rebootme(0);
	}
#if 0
	{
		uint32_t *mem1, *mem2, d1, d2, i;

		xcrc = flash_findcrc(LOADER_BASE_MEM1, flash_filelength);		// debug
		printf("LOADER_BASE_MEM1 CRC=0x%x\n", xcrc);

		xcrc = flash_findcrc(TFTP_BASE_MEM2, flash_filelength);		// debug
		printf("LOADER_BASE_MEM2 CRC=0x%x\n", xcrc);

		mem1 = LOADER_BASE_MEM1;
		mem2 = TFTP_BASE_MEM2;
		for (i = 0; i < flash_filelength; i += 4) {
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
	http_downloading = 0;
}

// calculate the crc over a range of memory
uint32_t flash_findcrc(void *base, int length) {
	uint32_t crc, xinit = 0xffffffff;

	crc = xcrc32(base, length, xinit);
	printf("findcrc: crc=0x%08x, base=0x%08x, len=%d\n", crc, base, length);
	return (crc);
}

