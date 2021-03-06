/*
 * mydebug.h
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */

#ifndef MYDEBUG_H_
#define MYDEBUG_H_

#include "stm32f7xx_hal.h"
#include <stdio.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;	// Console
extern UART_HandleTypeDef huart5;	// LCD

void myhexDump(char *desc, void *addr, int len);

// GPIO E Set and Clr output bits

void gpioeset(uint32_t setbits);
void gpioeclr(uint32_t clrbits);

#endif /* MYDEBUG_H_ */
