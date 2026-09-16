/*
 * www.h
 *
 *  Created on: 13Jun.,2018
 *      Author: bob
 */

#ifndef WWW_H_
#define WWW_H_

void httpclient(char*);
void returnpage(volatile char *content, volatile u16_t charcount, int errorm);

extern osSemaphoreId ssicontentHandle;

extern char stmuid[96];
extern ip_addr_t remoteip;
extern uint32_t polltime;

void requestapisn();
void init_httpd_ssi();
void initialapisn();

#endif /* WWW_H_ */
