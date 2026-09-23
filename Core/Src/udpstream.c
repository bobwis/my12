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
#include "cmsis_os.h"
#include "queue.h"
#include "semphr.h"
#include "neo7m.h"
#include "splat1.h"
#include "ip_addr.h"
#include "lwip/dns.h"
#include "lwip/prot/dns.h"
#include <string.h>

#define pbuf_free pbuf_free_callback

extern uint32_t t1sec;
uint8_t gpslocked = 0;
uint8_t epochvalid = 0;
unsigned int globalfreeze;		// freeze udp streaming
extern volatile uint32_t trigcomp;

struct ip4_addr udpdestip;		// udp dst ipv4 address
char ips[16]; // string version of IP address
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

/* Software send queue for triggered ADC sample packets AND status packets
 * (end-of-sequence / timed) - all outbound UDP traffic funnels through this
 * one ring, in production order, so wire ordering is automatic.
 *
 * Two-task split (branch: feature/decoupled-udp-send-queue):
 *   - The ADC-facing producer (startudp()'s notified/timeout branches) only
 *     ever does a fixed-size memcpy into the next free slot, pushes a small
 *     descriptor, and returns - no lwIP/network call, no dependency on send
 *     timing at all. This runs at the producer's existing priority.
 *   - A separate, lower-priority task (netsendtask(), osPriorityBelowNormal)
 *     owns every sendudp() call. It blocks on the descriptor queue (zero CPU
 *     while idle) and drains as fast as the network stack/hardware allow -
 *     not gated to one packet per producer-loop-timeout like before.
 *   Because the producer is always higher priority, it can never be blocked
 *   by anything the sender is doing: a new ADC notification preempts the
 *   sender immediately, mid-send-prep if need be.
 *
 * A previous version of this file opportunistically called the drain step
 * inline from the producer's notified branch; that reintroduced exactly the
 * variable-latency-in-the-hot-path problem this queue exists to avoid (the
 * sendudp() call chain - now including software UDP checksum generation and
 * IP fragmentation - is not constant-time) and caused ADC overruns under a
 * fast trigger burst. Moving all sending onto its own lower-priority task
 * fixes that at the scheduler level instead of by hand-tuning call sites.
 *
 * Slot free/busy accounting uses a counting semaphore (freeslots), not the
 * queue's own occupancy: a descriptor is popped off sendqueueq the moment
 * netsendtask() wakes, but the physical slot isn't actually safe to reuse
 * until sendudp() has been called for it - freeslots is given back right
 * after that, the same point in the pipeline the old single-task drain used
 * to advance sq_tail/sq_count. Popping the descriptor earlier than that
 * would let the producer overwrite a slot lwIP/hardware might still be
 * reading out of.
 *
 * Sized for a comfortable multiple of the ~50-packet trigger bursts this
 * system produces. At 1472 bytes/slot, 96 slots costs ~141KB; last checked,
 * that leaves ~57KB RAM headroom below the 512KB ceiling for future
 * development. Deliberately a plain static array (not FreeRTOS-heap-backed)
 * for the same RAM-fragmentation reasons as before - re-check the linker map
 * after changing this number.
 */
#define SEND_QUEUE_DEPTH 96
static uint8_t sendqueuebuf[SEND_QUEUE_DEPTH][UDPBUFSIZE];
static struct pbuf *sendqueuepbuf[SEND_QUEUE_DEPTH];
static uint8_t sq_head = 0;	// next slot the producer will fill; producer-owned only, never touched by netsendtask()

typedef struct {
	uint8_t slot;	// which sendqueuebuf/sendqueuepbuf slot this item lives in
	uint16_t len;	// actual payload length for this item (sample: UDPBUFSIZE, status: sizeof(statuspkt))
} senditem_t;

static QueueHandle_t sendqueueq;	// producer -> sender transport; also the sender's blocking wake signal
static SemaphoreHandle_t freeslots;	// counts physical slots currently safe for the producer to write into

// Push an item (already copied into sendqueuebuf[sq_head] by the caller) onto
// the queue for netsendtask() to send, and advance sq_head. Never blocks: the
// caller must already hold a free slot (see enqueue_sample()/enqueue_status()).
static void submit_queued_item(uint16_t len) {
	senditem_t item = { .slot = sq_head, .len = len };
	xQueueSend(sendqueueq, &item, 0);
	sq_head = (sq_head + 1) % SEND_QUEUE_DEPTH;
}

// Copy a completed trigger sample into the send queue. Fast and bounded -
// never waits on anything, never touches lwIP. If the queue is already full
// (network can't keep up even with SEND_QUEUE_DEPTH slots of buffering),
// drops the sample and counts it, matching the existing sigsend/adcudpover
// overrun idiom in adcstream.c for the same "producer outpaced consumer"
// situation.
static void enqueue_sample(void *payload) {
	if (xSemaphoreTake(freeslots, 0) != pdTRUE) {
		statuspkt.adcudpover++;	// queue full, drop this sample
		return;
	}
	memcpy(sendqueuebuf[sq_head], payload, UDPBUFSIZE);
	((uint8_t*) sendqueuebuf[sq_head])[3] = 4;	// pkt type (was 0, changed to 4 29-oct-22)
	((uint8_t*) sendqueuebuf[sq_head])[0] = statuspkt.udppknum & 0xff;
	((uint8_t*) sendqueuebuf[sq_head])[1] = (statuspkt.udppknum & 0xff00) >> 8;
	((uint8_t*) sendqueuebuf[sq_head])[2] = (statuspkt.udppknum & 0xff0000) >> 16;
	statuspkt.udppknum++;		// UDP packet number - assigned in production order
	submit_queued_item(UDPBUFSIZE);
}

// Snapshot the current status packet fields into the send queue as an
// end-of-sequence or timed status packet (stype: ENDSEQ or TIMED). Same
// cost/blocking profile as enqueue_sample() - a fixed ~156-byte memcpy, no
// lwIP call. Taking a snapshot here (rather than the old zero-copy pointer
// straight at the live statuspkt struct) also closes a latent race: the live
// struct can keep being mutated by other tasks (AGC etc.) for as long as it
// takes netsendtask() to actually get around to sending it.
static void enqueue_status(int stype) {
	if (xSemaphoreTake(freeslots, 0) != pdTRUE) {
		statuspkt.adcudpover++;	// queue full, drop this status packet
		return;
	}
	statuspkt.adcnoise = abs(meanwindiff) & 0xfff;	// agc
	statuspkt.adcbase = (globaladcavg & 0xfff) | (((pgagain > 7) ? (1 << 12) : 0));	// agc + boost gain
	statuspkt.auxstatus1 = (statuspkt.auxstatus1 & 0xffff0000) | (((jabbertimeout & 0xff) << 8) | adcbatchid);
	statuspkt.adctrigoff = ((trigthresh + trigcomp) & 0xFFF) | ((pgagain & 0xF) << 12);

	memcpy(sendqueuebuf[sq_head], (const void*) &statuspkt, sizeof(statuspkt));
	((uint8_t*) sendqueuebuf[sq_head])[3] = stype;	// status pkt type (ENDSEQ or TIMED)
	((uint8_t*) sendqueuebuf[sq_head])[0] = statuspkt.udppknum & 0xff;
	((uint8_t*) sendqueuebuf[sq_head])[1] = (statuspkt.udppknum & 0xff00) >> 8;
	((uint8_t*) sendqueuebuf[sq_head])[2] = (statuspkt.udppknum & 0xff0000) >> 16;
	statuspkt.udppknum++;
	submit_queued_item(sizeof(statuspkt));
}

// Lower-priority sender task: owns every sendudp() call in the firmware.
// Blocks (zero CPU) until the producer has something queued, then drains as
// fast as the network stack/hardware allow. Being strictly lower priority
// than the producer (see startudp()) is what guarantees it can never delay
// the ADC-facing path - the scheduler preempts it unconditionally the moment
// a new ADC notification needs servicing, even mid-send-prep.
static void netsendtask(void const *argument) {
	struct udp_pcb *pcb = (struct udp_pcb*) argument;
	senditem_t item;

	for (;;) {
		xQueueReceive(sendqueueq, &item, portMAX_DELAY);

		// Wait for hardware to fully release this slot's pbuf from whatever
		// it last sent from it. Bounded, short retry rather than a tight
		// spin - this task is never on the ADC's critical path, so a
		// millisecond of slack here costs nothing there. (Could be replaced
		// with something driven off HAL_ETH_TxFreeCallback later if this
		// retry ever shows up as a real bottleneck; not needed yet.)
		while (sendqueuepbuf[item.slot]->ref != 1) {
			vTaskDelay(1);
		}
		sendqueuepbuf[item.slot]->payload = sendqueuebuf[item.slot];
		sendqueuepbuf[item.slot]->len = item.len;
		sendqueuepbuf[item.slot]->tot_len = item.len;

		sendudp(pcb, sendqueuepbuf[item.slot], &udpdestip, UDP_PORT_NO);
		xSemaphoreGive(freeslots);	// data handed to lwIP; producer may reuse this slot now

		if (((uint8_t*) sendqueuebuf[item.slot])[3] == 4) {	// sample packets only, matches old drain_sendqueue's scope
			statuspkt.udpsent++;		// debug use adc udp sample packet sent count
			statuspkt.adcpktssent++;	// UDP sample packet counter
		}
	}
}

void myudp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
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
// queue a timed status packet if is time. Deliberately only ever checked
// from startudp()'s notify-timeout branch, i.e. only once the ADC side has
// been idle for a while - not a bug: during a trigger burst the
// end-of-sequence status packet already carries the same fields, so a timed
// status mid-burst would just be a redundant duplicate.
//
void sendtimedstatus(void) {
	static uint32_t talive = 0;

	if ((t1sec != talive) && (t1sec % STAT_TIME == 0)) { // this is a temporary mech to send timed status pkts...
		talive = t1sec;
		enqueue_status(TIMED);
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
	volatile int i, err = 0;

	if (xSemaphoreTake(dnssemHandle, 6000) == pdFALSE) {
		printf("dnslookup: Semaphore wait failed\n");
	}

//	printf("dnslookup: DNS Resolving %s\n", name);
	ip_ready = 0;
	err = dns_gethostbyname(name, ip, dnsfound, 0);

	xSemaphoreGive(dnssemHandle);

	switch (err) {
	case ERR_OK:		// a cached result already in *ip.addr
		break;
	case ERR_INPROGRESS:	// a callback result to dnsfound if it finds it
//		printf("dnslookup: IN PROGRESS\n");
		for (i = 0; i < 5; i++) {
			osDelay(1000);		// give it n seconds
//			printf(".");
			if (ip_ready) {		// is it done?
				if (ip_ready == -1) {
					IP4_ADDR(ip, 127, 0, 0, 1);	// safe ?
					printf("dnslookup: failed 1\n");
					return (ERR_TIMEOUT);	// not always timeout, but some error
				}
				ip->addr = ip_ready;
//				printf("dnslookup: returning OK\n");
				return (ERR_OK);
			}
		}
		IP4_ADDR(ip, 127, 0, 0, 1);	// safe ?
		printf("dnslookup: failed 2\n");
		return (-1);	// Timed out
		break;
	default:
		printf("dnslookup: failed 3\n");
		break;
	}
	if (err)
		printf("dnslookup: returning %d\n",err);
	return (err);
}

// lookup IP address from domain name
extern struct ip4_addr locateip(char *targetname) {
	volatile err_t err;
	struct ip4_addr iprec;
	uint32_t ip = 0;

	printf("locateip: Finding %s IP address\n", targetname);
	err = dnslookup(targetname, &iprec);
	if (err) {
		printf("locateip: FAILED on %s\n", targetname);
		printf("locateip: Trying again %s IP address\n", targetname);
		err = dnslookup(targetname, &iprec);
		if (err) {
			printf("locateip: FAILED on %s\n", targetname);
			iprec.addr = 0;
			return (iprec);
		}
	}

	ip = iprec.addr;
	sprintf(ips, "%lu.%lu.%lu.%lu", ip & 0xff, (ip & 0xff00) >> 8, (ip & 0xff0000) >> 16, (ip & 0xff000000) >> 24);
	printf("locateip: %s %s\n",targetname, ips);
	return (iprec);
}

void startudp() {		// destination UDP target IP address
	struct udp_pcb *pcb;
	struct pbuf *pd, *p1, *p2;
	uint32_t ulNotificationValue = 0;
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(1000);
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

	// dedicated, permanently-held pbuf per send queue slot (same pattern as p1/p2)
	for (i = 0; i < SEND_QUEUE_DEPTH; i++) {
		sendqueuepbuf[i] = pbuf_alloc(PBUF_TRANSPORT, UDPBUFSIZE, PBUF_REF);
		if (sendqueuepbuf[i] == NULL) {
			printf("startudp: sendqueue pbuf %d alloc failed!\n", i);
			return;
		}
		sendqueuepbuf[i]->payload = sendqueuebuf[i];
	}

	sendqueueq = xQueueCreate(SEND_QUEUE_DEPTH, sizeof(senditem_t));
	if (sendqueueq == NULL) {
		printf("startudp: sendqueueq create failed!\n");
		return;
	}
	freeslots = xSemaphoreCreateCounting(SEND_QUEUE_DEPTH, SEND_QUEUE_DEPTH);
	if (freeslots == NULL) {
		printf("startudp: freeslots semaphore create failed!\n");
		return;
	}

	// Lower priority than this task (osPriorityNormal) on purpose - see the
	// send queue header comment above. Started here, once, before the main
	// loop, same lifetime as pcb/the queue it drains.
	osThreadDef(NetSend, netsendtask, osPriorityBelowNormal, 0, 2048);
	if (osThreadCreate(osThread(NetSend), pcb) == NULL) {
		printf("startudp: netsendtask create failed!\n");
		return;
	}

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

				enqueue_sample(pd->payload);	// fast fixed-size copy; pd is free for the ADC again immediately

				/* queue end of sequence status packet if end of batch sequence */
				if (sendendstatus > 0) {
//					if (jabbertimeout == 0)	// terminate curtailed sequence???
					enqueue_status(ENDSEQ); // queue end of seq status - netsendtask() sends it in order
					sendendstatus = 0;	// cancel the flag
					statuspkt.adcpktssent = 0;	// end of sequence so start again at 0
				}
			} // if sigsend via wakeup
		}
//			printf("ulNotificationValue = %d\n",ulNotificationValue );
		/* The transmission ended as expected. */
		else {
			/* The call to ulTaskNotifyTake() timed out - ADC side has been
			 * idle. netsendtask() drains the queue independently now, so
			 * there's nothing to do here except check for a due timed
			 * status packet. */
			sendtimedstatus();
//			printf("ulNotificationValue = %d\n",ulNotificationValue );
		}

	} // forever while
}
