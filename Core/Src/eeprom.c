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

	if (((uint32_t) memptr & 0x8100000) == 0x8000000)	// the lower 512K
			{
		EraseInitStruct.Sector = FLASH_SECTOR_0;
		EraseInitStruct.NbSectors = 5;
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

	if (dirty) {
		printf("Erasing Flash for %d sector(s) from %d\n", EraseInitStruct.NbSectors, EraseInitStruct.Sector);

		EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
		EraseInitStruct.Banks = FLASH_BANK_1;
		EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

		res = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
		if (res != HAL_OK) {
			printf("EraseFlash: failed\n");
			printflasherr();
		} else {
			printf("Flash successfully erased\n");
		}
	} else {
		printf("Flash erase unnecessary\n");
	}
}

// write 32 bits
int WriteFlashWord(uint32_t address, uint32_t data) {
	HAL_StatusTypeDef res;
	int trys;

	if (((int) address < FLASH_START_ADDRESS) || ((int) address > (FLASH_END_ADDRESS))) {
		printf("WriteFlash: failed address check\n");
		return -1;
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
				return (-1);
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
	}
	return (0);
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

/// fix up the boot vectors in the option flash
void fixboot() {
	HAL_StatusTypeDef res;
	FLASH_OBProgramInitTypeDef OBInitStruct;

	HAL_FLASHEx_OBGetConfig(&OBInitStruct);

	HAL_FLASH_OB_Unlock();

	OBInitStruct.BootAddr0 = 0x2000;		// corresponds to 0x8000000  (flash)
	OBInitStruct.BootAddr1 = 0x2040;		// corresponds to 0x8100000  (flash)

	res = HAL_FLASHEx_OBProgram(&OBInitStruct);
	if (res != HAL_OK) {
		printf("fixboot: failed to OBProgram %d\n", res);
	}

	res = HAL_FLASH_OB_Launch();
	if (res != HAL_OK) {
		printf("fixboot: failed to OBLaunch %d\n", res);
	}
	printf("fixing boot....\n");
	HAL_FLASH_OB_Lock();

	printf("fixboot ran\n");
}

