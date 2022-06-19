/*
 * nextionloader.c
 *
 *  Created on: 17 Jun. 2022
 *      Author: bob
 *
 *      Adapted from C++, author below
 */

/**
 * @file NexUpload.cpp
 *
 * The implementation of download tft file for nextion.
 *
 * @author  Chen Zengpeng (email:<zengpeng.chen@itead.cc>)
 * @date    2016/3/29
 * @copyright
 * Copyright (C) 2014-2015 ITEAD Intelligent Systems Co., Ltd. \n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "Nextionloader.h"

#if 0

#define dbSerialPrintln printf
#define bool int
#define false 0

char string[64];

void sendCommand(const char *cmd) {

	while (nexSerial.available()) {
		nexSerial.read();
	}

	nexSerial.print(cmd);
	nexSerial.write(0xFF);
	nexSerial.write(0xFF);
	nexSerial.write(0xFF);
}

uint16_t recvRetString(char *str, uint32_t timeout,bool recv_flag)
{
	uint16_t ret = 0;
	uint8_t c = 0;
	long start;
	bool exit_flag = false;

	start = onesectimer;
	while (millis() - start <= timeout)
	{
		while (nexSerial.available())
		{
			c = nexSerial.read();
			if(c == 0)
			{
				continue;
			}
			str[ret++] += (char)c;
			if(recv_flag)
			{
				if(string[5] != -1)
				{
					exit_flag = true;
				}
			}
		}
		if(exit_flag)
		{
			break;
		}
	}
	return ret;
}

bool _downloadTftFile(void) {
	uint8_t c;
	uint16_t send_timer = 0;
	uint16_t last_send_num = 0;
	String string = String("");
	send_timer = _undownloadByte / 4096 + 1;
	last_send_num = _undownloadByte % 4096;

	while (send_timer) {

		if (send_timer == 1) {
			for (uint16_t j = 1; j <= 4096; j++) {
				if (j <= last_send_num) {
					c = _myFile.read();
					nexSerial.write(c);
				} else {
					break;
				}
			}
		}

		else {
			for (uint16_t i = 1; i <= 4096; i++) {
				c = _myFile.read();
				nexSerial.write(c);
			}
		}
		this->recvRetString(string, 500, true);
		if (string.indexOf(0x05) != -1) {
			string = "";
		} else {
			return 0;
		}
		--send_timer;
	}
}
#endif

