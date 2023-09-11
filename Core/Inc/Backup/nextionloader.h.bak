/*
 * nextionloader.h
 *
 *  Created on: 17 Jun. 2022
 *      Author: bob
 *      adapted from the below C++
 */

#ifndef INC_NEXTIONLOADER_H_
#define INC_NEXTIONLOADER_H_

/**
 * @file NexUpload.h
 *
 * The definition of class NexUpload.
 *
 * @author Chen Zengpeng (email:<zengpeng.chen@itead.cc>), Bogdan Symchych (email:<bogdan.symchych@gmail.com>)
 * @date 2019/10/1
 *
 * @copyright
 * Copyright (C) 2014-2015 ITEAD Intelligent Systems Co., Ltd. \n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */
#include "stm32f7xx_hal.h"
#include "Nextion.h"
#include "httpclient.h"

/**
 * @addtogroup CoreAPI
 * @{
 */


/*
 * Send command to Nextion.
 *
 * @param cmd - the string of command.
 *
 * @return none.
 */
void sendCommand(const char *cmd);

/*
 * Receive string data.
 *
 * @param buffer - save string data.
 * @param timeout - set timeout time.
 * @param recv_flag - if recv_flag is true,will braak when receive 0x05.
 *
 * @return the length of string buffer.
 *
 */

extern int lcdupneeded();		// check if LCD version etc is up to date

extern IWDG_HandleTypeDef hiwdg;

extern uint32_t _baudrate; /*nextion serail baudrate*/
extern uint32_t _undownloadByte; /*undownload byte of tft file*/
extern uint32_t _download_baudrate; /*download baudrate*/
extern uint32_t _uploaded_bytes; /*counter of uploaded bytes*/

extern int nxt_blocksacked;	// number of acks recieved by the LCD (every 4k bytes)
extern int nxt_abort;			// 1 == abort

#endif /* INC_NEXTIONLOADER_H_ */
