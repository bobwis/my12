/*
 * udpstream.h
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */

#ifndef UDPSTREAM_H_
#define UDPSTREAM_H_

#include "version.h"

void startudp();



#define UDP_PORT_NO 5000
extern TaskHandle_t xTaskToNotify;

// status packet types
#define TIMED 2
#define ENDSEQ 1

extern char udp_target[64];	// dns or ip address of udp target
extern char ips[16]; // string version of IP address
extern struct ip4_addr udpdestip;		// udp dst ipv4 address
extern struct ip4_addr locateip(char *targetname);
extern struct ip4_addr udpdestip;		// udp dst ipv4 address

extern osSemaphoreId dnssemHandle;

#endif /* UDPSTREAM_H_ */
