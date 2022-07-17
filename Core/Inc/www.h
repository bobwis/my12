/*
 * www.h
 *
 *  Created on: 13Jun.,2018
 *      Author: bob
 */

#ifndef WWW_H_
#define WWW_H_

void httpclient(char*);

extern osSemaphoreId ssicontentHandle;

extern char stmuid[96];
extern ip_addr_t remoteip;

#endif /* WWW_H_ */
