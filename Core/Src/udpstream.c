/*
 printf("******* ps->ref = %d *******\n", ps->ref); * udpstream.c
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */
#include "lwip.h"
#include "udpstream.h"
#include "adcstream.h"
#include "mydebug.h"
#include "FreeRTOS.h"
#include "neo7m.h"
#include "splat1.h"
#include "ip_addr.h"
#include "lwip/dns.h"
#include "lwip/prot/dns.h"

#define pbuf_free pbuf_free_callback

extern uint32_t t1sec;
uint8_t gpslocked = 0;
uint8_t epochvalid = 0;
unsigned int globalfreeze;		// freeze udp streaming

struct ip4_addr udpdestip;		// udp dst ipv4 address
char udp_ips[16]; // string version of IP address
static uint32_t ip_ready = 0;

// reboot
void myreboot(char *msg) {
	printf("%s, ... rebooting\n", msg);
	osDelay(2000);
	__NVIC_SystemReset();   // reboot

}

//
// send a udp packet and try to recover if an error detected from LwIP return
//
/*inline*/err_t sendudp(struct udp_pcb *pcb, struct pbuf *ps, const ip_addr_t *dst_ip, u16_t dst_port) {
	volatile err_t err;
	static int busycount = 0;

	err = udp_sendto(pcb, ps, &udpdestip, UDP_PORT_NO);
	if (err != ERR_OK) {
#ifdef TESTING
		stats_display(); // this needs stats in LwIP enabling to do anything
#endif
		printf("sendudp: err %i\n", err);
		vTaskDelay(100); //some delay!
		if (err == ERR_MEM) {
			myreboot("sendudp: out of mem");
		}
		if (err == ERR_USE) {
			if (busycount++ > 10)
				myreboot("sendudp: udp always busy");
		}
	} else
		busycount = 0;
	return (err);
}

void myudp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port) {
	volatile err_t err;
	if (p != NULL) {
		/* send received packet back to sender */
		err = sendudp(pcb, p, addr, port);
		/* free the pbuf */
		pbuf_free(p);
		if (err != ERR_OK) {
			printf("myudp_recv: err %i\n", err);
		}
		//		pbuf_free_callback(p);
	}
}

//
// send a status packet
//
/*inline*/void sendstatus(int stype, struct pbuf *ps, struct udp_pcb *pcb, uint8_t batchid) {

	volatile err_t err;

	statuspkt.adcnoise = abs(meanwindiff) & 0xfff;	// agc
//	statuspkt.adcbase = (globaladcavg & 0xfff) | (((pgagain & 0x38) >> 3) << 13); 	// agc + boost gain
	statuspkt.adcbase = (globaladcavg & 0xfff) | (((pgagain > 7) ? (1 << 12) : 0)); 	// agc + boost gain
	statuspkt.auxstatus1 = (statuspkt.auxstatus1 & 0xffff0000) | (((jabbertimeout & 0xff) << 8) | adcbatchid);

//	statuspkt.adctrigoff = ((TRIG_THRES + (abs(globaladcnoise - statuspkt.adcbase))) & 0xFFF); //  | ((pgagain & 7) << 12);
	statuspkt.adctrigoff = abs(meanwindiff - lastmeanwindiff) + trigthresh | ((pgagain & 7) << 12);

#if 0
	while (ps->ref > 0) { // old packet not finished with yet
		printf("******* timed status1: ps->ref = %d *******\n", ps->ref);
#ifdef TESTING
		osDelay(100);
#endif
		vTaskDelay(0); // but we need wait to update the data packet next, so wait
	}
#endif

	((uint8_t*) (ps->payload))[3] = stype; // timed status pkt type

	err = sendudp(pcb, ps, &udpdestip, UDP_PORT_NO);

#if 0
	while (ps->ref > 0) { // old packet not finished with yet
		printf("******* timed status2: ps->ref = %d *******\n", ps->ref);

#ifdef TESTING
		osDelay(100);
#endif
		vTaskDelay(0); // but we need wait to update the data packet next, so wait
	}
#endif
	statuspkt.udppknum++;
}

//
// send timed status packet if is time
//
void sendtimedstatus(struct pbuf *ps, struct udp_pcb *pcb, uint8_t batchid) {
	static uint32_t talive = 0;

	if ((t1sec != talive) && (t1sec % STAT_TIME == 0)) { // this is a temporary mech to send timed status pkts...
		talive = t1sec;
		sendstatus(TIMED, ps, pcb, batchid);
	}
}

// Delayed DNS lookup result callback

void dnsfound(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
	if (ipaddr->addr == NULL) {
		ip_ready = -1;
	} else
		ip_ready = ipaddr->addr;
}

// set destination server IP using DNS lookup
int dnslookup(char *name, struct ip4_addr *ip) {
	int i, err = 0;

	printf("DNS Resolving %s ", name);
//	osDelay(500);
	ip_ready = 0;
	err = dns_gethostbyname(name, ip, dnsfound, 0);

	switch (err) {
	case ERR_OK:		// a cached result already in *ip.addr
		break;
	case ERR_INPROGRESS:	// a callback result to dnsfound if it finds it
		printf("gethostbyname INPROGRESS");
		for (i = 0; i < 20; i++) {
			osDelay(1000);		// give it 20 seconds
			printf(".");
			if (ip_ready) {
				if (ip_ready == -1) {
					ip->addr = "127.0.0.1";	// safe ?
					return (ERR_TIMEOUT);	// not always timeout, but some error
				}
				ip->addr = ip_ready;
				return (ERR_OK);
			}
			if (err == ERR_OK)
				break;
		} // falls through on timeout
	default:
		printf("****** gethostbyname failed *****\n ");
		break;
	}
	return (err);
}

uint32_t locateudp()		// called from LPtask every n seconds
{
	volatile err_t err;
	uint32_t ip = 0;

	printf("Finding %s for UDP streaming\n", udp_target);
	err = dnslookup(udp_target, &udpdestip);
	if (err)
		rebootme(3);

	ip = udpdestip.addr;
	sprintf(udp_ips, "%lu.%lu.%lu.%lu", ip & 0xff, (ip & 0xff00) >> 8, (ip & 0xff0000) >> 16, (ip & 0xff000000) >> 24);
	printf("\nUDP Target IP: %s\n", udp_ips);
	return (ip);
}

void startudp(uint32_t ip) {
	struct udp_pcb *pcb;
	struct pbuf *pd, *p1, *p2, *ps;
	uint32_t ulNotificationValue = 0;
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(1000);
	volatile err_t err;
	int i;

//printf("Startudp:\n");
	/* Store the handle of the calling task. */
	xTaskToNotify = xTaskGetCurrentTaskHandle();
	osDelay(1000);

	/* get new pcbs */
	pcb = udp_new();
	if (pcb == NULL) {
		printf("startudp: udp_new failed!\n");
		for (;;)
			;
		return;
	}

	/* bind to any IP address on port UDP_PORT_NO */
	if (udp_bind(pcb, IP_ADDR_ANY, UDP_PORT_NO) != ERR_OK) {
		printf("startudp: udp_bind failed!\n");
		for (;;)
			;
	}

//	udp_recv(pcb, myudp_recv, NULL);

	p1 = pbuf_alloc(PBUF_TRANSPORT, UDPBUFSIZE, PBUF_REF /* PBUF_ROM */); // pk1 pbuf

	if (p1 == NULL) {
		printf("startudp: p1 buf_alloc failed!\n");
		return;
	}
	p1->payload = &(*pktbuf)[0];
//	p1->len = ADCBUFSIZE;

	p2 = pbuf_alloc(PBUF_TRANSPORT, UDPBUFSIZE, PBUF_REF /* PBUF_ROM */); // pk1 pbuf
	if (p2 == NULL) {
		printf("startudp: p2 buf_alloc failed!\n");
		return;
	}
	p2->payload = &(*pktbuf)[(UDPBUFSIZE / 4)];	// half way along physical buffer

//	p2->len = ADCBUFSIZE;

// trailing packet status packet
	ps = pbuf_alloc(PBUF_TRANSPORT, sizeof(statuspkt), PBUF_ROM);	// pks pbuf
	if (ps == NULL) {
		printf("startudp: ps buf_alloc failed!\n");
		return;
	}
	ps->payload = &statuspkt;	// point at status / GPS data

	osDelay(5000);

	statuspkt.auxstatus1 = 0;
	statuspkt.adcudpover = 0;		// debug use count overruns
	statuspkt.trigcount = 0;		// debug use adc trigger count
	statuspkt.udpsent = 0;	// debug use adc udp sample packet sent count
	statuspkt.telltale1 = 0xDEC0EDFE; //  0xFEEDC0DE marker at the end of each status packet

	netup = 1; // this is incomplete - it should be set by the phys layer also
	printf("Arming UDP Railgun\nSystem ready and operating....\n");

	while (1) {
//			for(;;) osDelay(1000);
//		p1 = pbuf_alloc(PBUF_TRANSPORT, sizeof(mypbuf), PBUF_ROM);		// header pbuf
//		p1->tot_len = sizeof(mypbuf);
//		vTaskDelay(1); //some delay!

		//    memcpy (p1->payload, (lastbuf == 0) ? testbuf : testbuf, ADCBUFLEN);

		/* Wait to be notified */
#ifdef TESTING
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET /*PB11*/);	// debug pin
#endif

		ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime);
#ifdef TESTING
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET /*PB11*/);	// debug pin
#endif

		if (ulNotificationValue > 0) {		// we were notified
			sigsend = 0;
			/* if we have a trigger, send a sample packet */
			if ((gpslocked) && (jabbertimeout == 0) && (!(globalfreeze))) { // only send if adc threshold was exceeded and GPS is locked

				//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET /*PB11*/);	// debug pin
				pd = (dmabufno) ? p2 : p1; // which dma buffer to send, dmabuf is last filled buffer, 0 or 1

				((uint8_t*) (pd->payload))[3] = 0;	// pkt type
				((uint8_t*) (pd->payload))[0] = statuspkt.udppknum & 0xff;
				((uint8_t*) (pd->payload))[1] = (statuspkt.udppknum & 0xff00) >> 8;
				((uint8_t*) (pd->payload))[2] = (statuspkt.udppknum & 0xff0000) >> 16;

				while (pd->ref != 1) {	// old packet not finished with yet
					printf("*******send sample failed p->ref = %d *******\n", pd->ref);
				}

				err = sendudp(pcb, pd, &udpdestip, UDP_PORT_NO);		// send the sample packet

				statuspkt.udpsent++;	// debug no of sample packets set
				statuspkt.adcpktssent++;	// UDP sample packet counter
				statuspkt.udppknum++;		// UDP packet number
#if 0
				while (ps->ref != 1) { // old status packet not finished with yet
					printf("******* end sample status: ps->ref = %d *******\n", ps->ref);
					vTaskDelay(0); // but we need wait to update the data packet next, so wait
				}
#endif
				/* send end of sequence status packet if end of batch sequence */
				if (sendendstatus > 0) {
//					if (jabbertimeout == 0)	// terminate curtailed sequence???
						sendstatus(ENDSEQ, ps, pcb, adcbatchid); // send end of seq status
					sendendstatus = 0;	// cancel the flag
					statuspkt.adcpktssent = 0;	// end of sequence so start again at 0
				}
			} // if sigsend via wakeup
		}
//			printf("ulNotificationValue = %d\n",ulNotificationValue );
		/* The transmission ended as expected. */
		else {
			/* The call to ulTaskNotifyTake() timed out. */
			sendtimedstatus(ps, pcb, adcbatchid);
//			printf("ulNotificationValue = %d\n",ulNotificationValue );
		}

	} // forever while
}
