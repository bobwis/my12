/*
 printf("******* ps->ref = %d *******\n", ps->ref); * udpstream.c
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */
#include "lwip.h"
#include "udpstream.h"
#include "netfix.h"
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
extern void rebootme(int why);
extern struct netif gnetif;
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
static uint8_t sq_head = 0;	// next slot to fill; see reserve_queue_slot() for why this now needs protecting

typedef struct {
	uint8_t slot;	// which sendqueuebuf/sendqueuepbuf slot this item lives in
	uint16_t len;	// actual payload length for this item (sample: UDPBUFSIZE, status: sizeof(statuspkt))
} senditem_t;

static QueueHandle_t sendqueueq;	// producer/sender -> sender transport; also the sender's blocking wake signal
static SemaphoreHandle_t freeslots;	// counts physical slots currently safe to write into

/* ---- UDP send-path stall guard --------------------------------------------
 * Field detectors on build 10047 (this send-queue code) were observed to stop
 * sending ALL UDP - heartbeats included - while HTTP/TCP kept working and the
 * console stayed silent; only a reboot cured it. The sender used to wait for a
 * slot's pbuf with an unbounded, silent loop, which is exactly that failure if
 * a pbuf reference is ever never released. Two layers now:
 *
 *  1. netsendtask()'s wait for a slot's pbuf is bounded. It reports once after
 *     STALL_SNAPSHOT_MS, again every STALL_REPORT_EVERY_MS, and after
 *     STALL_REBOOT_MS gives up and reboots.
 *  2. udp_stall_watchdog(), run from the ADC-facing producer task (a different
 *     task, so it still works if the sender is stuck ANYWHERE - the pbuf wait,
 *     sendudp(), or the Ethernet output path), reboots if the sender has been
 *     inside one item for STALL_SENDER_BUSY_SECS, if the queue holds items but
 *     the sender's loop hasn't come round for that long (sq_alive_sec - NOT the
 *     last send: since samples are queued by the ADC ISR, which then wakes this
 *     higher-priority task, the watchdog routinely sees a just-queued sample
 *     before the sender has run, and after 30 s of no triggers that read as a
 *     stall and rebooted the detector), or if the link is up but no packet of
 *     any kind has been sent for STALL_NO_SEND_SECS (a heartbeat is due every
 *     STAT_TIME). Only armed after STALL_ARM_UPTIME_SECS so a fault can never
 *     cause a rapid reboot loop.
 *
 * Every report goes to the console only (the status/sample packet formats are
 * fixed and shared with downstream software), prefixed udpstall: so it is easy
 * to find in a serial capture. rebootme() is the project's standard reboot.
 */
#define STALL_SNAPSHOT_MS		250		// sender's wait for a slot's pbuf before the first report
#define STALL_REPORT_EVERY_MS	1000	// repeat interval for further short reports
#define STALL_REBOOT_MS			5000	// sender gives up waiting for the pbuf and reboots
#define STALL_SENDER_BUSY_SECS	30		// watchdog: sender inside one item / queue not draining
#define STALL_NO_SEND_SECS		(5 * STAT_TIME)	// watchdog: link up but nothing sent at all
#define STALL_ARM_UPTIME_SECS	600		// watchdog: don't act during the first 10 minutes
#define STALL_REBOOT_WHY		9		// err_leds()/rebootme() code: UDP send path stalled

static volatile uint32_t sq_busy_since;		// t1sec when the sender took its current item; 0 = idle
static volatile uint32_t sq_last_send_sec;	// t1sec of the last completed sendudp()
static volatile uint32_t sq_alive_sec;		// t1sec the sender last came back from its queue wait (item or 1 s timeout)
static volatile uint32_t sq_sent_total;		// completed sendudp() calls since boot (samples + status)
static volatile uint8_t sq_cur_slot;		// the item the sender is/was working on
static volatile uint8_t sq_cur_type;		//   (packet type byte: 4 = sample, ENDSEQ/TIMED = status)
static volatile uint16_t sq_cur_len;
static volatile uint32_t sq_cur_pknum;

// One-shot dump of everything that could show why the send path stopped.
static void udp_stall_snapshot(const char *where, uint32_t waited_ms) {
	int i, bad = 0;
	struct pbuf *pb = sendqueuepbuf[sq_cur_slot];

	printf("udpstall: %s\n", where);
	printf("udpstall: uptime=%lus sender_busy=%lus last_send_ago=%lus alive_ago=%lus waited=%lums sent=%lu\n", (unsigned long) t1sec,
			(unsigned long) (sq_busy_since ? (t1sec - sq_busy_since) : 0), (unsigned long) (t1sec - sq_last_send_sec),
			(unsigned long) (t1sec - sq_alive_sec), (unsigned long) waited_ms, (unsigned long) sq_sent_total);
	printf("udpstall: item slot=%u type=%u len=%u pknum=%lu\n", sq_cur_slot, sq_cur_type, sq_cur_len, (unsigned long) sq_cur_pknum);
	if (pb != NULL) {
		printf("udpstall: pbuf ref=%u len=%u tot_len=%u flags=0x%02x type=0x%02x next=0x%08lx payload=0x%08lx\n", (unsigned) pb->ref,
				(unsigned) pb->len, (unsigned) pb->tot_len, (unsigned) pb->flags, (unsigned) pb->type_internal,
				(unsigned long) (uintptr_t) pb->next, (unsigned long) (uintptr_t) pb->payload);
	}
	printf("udpstall: freeslots=%u/%d queued=%u udpsent=%lu overruns=%lu heap_free=%lu\n", (unsigned) uxSemaphoreGetCount(freeslots),
			SEND_QUEUE_DEPTH, (unsigned) uxQueueMessagesWaiting(sendqueueq), (unsigned long) statuspkt.udpsent,
			(unsigned long) statuspkt.adcudpover, (unsigned long) xPortGetFreeHeapSize());
	printf("udpstall: netif_up=%d link_up=%d eth_dmasr=0x%08lx tx_cur_desc=%lu tx_buffers_in_use=%lu\n", (int) netif_is_up(&gnetif),
			(int) netif_is_link_up(&gnetif), (unsigned long) ETH->DMASR, (unsigned long) heth.TxDescList.CurTxDesc,
			(unsigned long) heth.TxDescList.BuffersInUse);
	printf("udpstall: ring ref!=1 (slot:ref):");
	for (i = 0; i < SEND_QUEUE_DEPTH; i++) {
		if (sendqueuepbuf[i] != NULL && sendqueuepbuf[i]->ref != 1) {
			printf(" %d:%u", i, (unsigned) sendqueuepbuf[i]->ref);
			bad++;
		}
	}
	printf(" (%d of %d)\n", bad, SEND_QUEUE_DEPTH);
	nettx_diag_print("udpstall", 16);	// Ethernet TX ring / mutex counters / last TX and free events (netfix.h)
}

// Runs from the ADC-facing producer, at most once a second. See the block comment above.
static void udp_stall_watchdog(void) {
	static uint32_t lastcheck = 0;
	static uint32_t link_up_since = 0;	// t1sec when the link last came up; 0 = down/unknown
	uint32_t now = t1sec;
	uint32_t busy, linkup_for;
	const char *why = NULL;

	if (now == lastcheck || now < STALL_ARM_UPTIME_SECS) {
		return;
	}
	lastcheck = now;

	// Only judge the send path while the Ethernet link has been up: with the
	// cable unplugged a stuck or silent sender is expected, and rebooting then
	// would only throw away GPS lock for nothing. The failure this guards
	// against happened with the link (and TCP) fully up.
	if (!netif_is_link_up(&gnetif)) {
		link_up_since = 0;
		return;
	}
	if (link_up_since == 0) {
		link_up_since = now;
		return;
	}
	linkup_for = now - link_up_since;

	busy = sq_busy_since;
	if (busy != 0 && (now - busy) > STALL_SENDER_BUSY_SECS && linkup_for > STALL_SENDER_BUSY_SECS) {
		why = "watchdog: sender stuck inside one item";
	} else if ((now - sq_alive_sec) > STALL_SENDER_BUSY_SECS && linkup_for > STALL_SENDER_BUSY_SECS
			&& uxQueueMessagesWaiting(sendqueueq) > 0) {
		why = "watchdog: queue not draining";
	} else if ((now - sq_last_send_sec) > STALL_NO_SEND_SECS && linkup_for > STALL_NO_SEND_SECS) {
		why = "watchdog: link up but no UDP packet sent";
	}

	if (why != NULL) {
		udp_stall_snapshot(why, 0);
		printf("udpstall: rebooting\n");
		osDelay(50);	// let the UART drain
		rebootme(STALL_REBOOT_WHY);
	}
}

// Reserve the next ring slot and packet sequence number, each atomically.
// Needed on both sides: the ADC-facing producer enqueues samples and
// end-of-sequence status, but netsendtask() enqueues timed status itself
// (see below) - so sq_head and statuspkt.udppknum each have two possible
// writers and need a consistent, race-free view across both.
//
// Deliberately protects ONLY these small integer read-modify-writes, not
// the (much larger, payload-sized) memcpy that follows using the reserved
// values - once a slot index is reserved here, no other caller will pick the
// same one again until this item's descriptor is sent and freeslots is given
// back, so the memcpy itself needs no further protection. Keeping the
// critical section this small means it never holds interrupts disabled for
// more than a few instructions, regardless of packet size - protecting the
// whole enqueue instead would put a payload-sized interrupts-disabled window
// directly in front of the ADC ISR, which is exactly what this queue exists
// to avoid.
//
// Slot index and packet number are reserved separately (two tiny critical
// sections) so that a sample dropped after its copy - see enqueue_sample() -
// gives back its slot without leaving a gap in the packet numbering. Skipping
// a slot index is harmless: freeslots, not the index, bounds what is in flight.
static uint8_t reserve_queue_slot(void) {
	uint8_t slot;
	taskENTER_CRITICAL();
	slot = sq_head;
	sq_head = (sq_head + 1) % SEND_QUEUE_DEPTH;
	taskEXIT_CRITICAL();
	return slot;
}

static uint32_t take_packet_number(void) {
	uint32_t pknum;
	taskENTER_CRITICAL();
	pknum = statuspkt.udppknum;
	statuspkt.udppknum++;
	taskEXIT_CRITICAL();
	return pknum;
}

uint32_t trigbuflate = 0;	// triggered buffers dropped because DMA had moved on before they were copied
static volatile uint8_t samplesarmed = 0;	// set by startudp() once the queue exists and sending is armed

// Copy the ADC buffer that just triggered into the send queue. Called from ADC_Conv_complete()
// (TIM5 IRQ, priority 5, FreeRTOS-safe) straight after that buffer was scanned, while the DMA is
// filling the *other* half of the double buffer - so the copy cannot race the DMA. When this copy
// was done later by the startudp() task, the ADC ISR load (83% on the bench) meant the task was
// routinely preempted mid-copy while the DMA refilled the buffer: torn or wrong buffers were sent.
//
// Tasks update sq_head and statuspkt.udppknum under taskENTER_CRITICAL(), which masks this IRQ,
// so plain access is safe here. Never blocks; drops and counts if the queue is full (adcudpover),
// not yet created, or if this ISR itself overran into the next buffer (trigbuflate).
void enqueue_sample_isr(void *payload, uint32_t bufseq, BaseType_t *woken) {
	uint8_t slot;
	uint32_t pknum;
	senditem_t item;

	if (!samplesarmed)		// ADC starts before startudp() has created the queue and armed
		return;
	if ((!gpslocked) || (jabbertimeout != 0) || (globalfreeze))	// same send conditions the task used
		return;
	if (xSemaphoreTakeFromISR(freeslots, woken) != pdTRUE) {
		statuspkt.adcudpover++;	// queue full, drop this sample
		return;
	}
	slot = sq_head;
	sq_head = (sq_head + 1) % SEND_QUEUE_DEPTH;
	memcpy(sendqueuebuf[slot], payload, UDPBUFSIZE);
	if (adcbufseq != bufseq) {	// this ISR ran past the next DMA completion: the copy may be torn
		trigbuflate++;
		xSemaphoreGiveFromISR(freeslots, woken);
		return;
	}
	pknum = statuspkt.udppknum++;
	((uint8_t*) sendqueuebuf[slot])[3] = 4;	// pkt type (was 0, changed to 4 29-oct-22)
	((uint8_t*) sendqueuebuf[slot])[0] = pknum & 0xff;
	((uint8_t*) sendqueuebuf[slot])[1] = (pknum & 0xff00) >> 8;
	((uint8_t*) sendqueuebuf[slot])[2] = (pknum & 0xff0000) >> 16;

	item.slot = slot;
	item.len = UDPBUFSIZE;
	xQueueSendFromISR(sendqueueq, &item, woken);
}

// Snapshot the current status packet fields into the send queue as an
// end-of-sequence or timed status packet (stype: ENDSEQ or TIMED). Same
// cost/blocking profile as enqueue_sample() - a fixed ~156-byte memcpy, no
// lwIP call. Taking a snapshot here (rather than the old zero-copy pointer
// straight at the live statuspkt struct) also closes a latent race: the live
// struct can keep being mutated by other tasks (AGC etc.) for as long as it
// takes netsendtask() to actually get around to sending it. Called from both
// startudp() (ENDSEQ, event-driven off the actual ADC batch-end) and
// netsendtask() (TIMED, time-driven - see there for why it lives there now).
static void enqueue_status(int stype) {
	uint8_t slot;
	uint32_t pknum;
	senditem_t item;

	if (xSemaphoreTake(freeslots, 0) != pdTRUE) {
		statuspkt.adcudpover++;	// queue full, drop this status packet
		return;
	}
	statuspkt.adcnoise = abs(meanwindiff) & 0xfff;	// agc
	statuspkt.adcbase = (globaladcavg & 0xfff) | (((pgagain > 7) ? (1 << 12) : 0));	// agc + boost gain
	statuspkt.auxstatus1 = (statuspkt.auxstatus1 & 0xffff0000) | (((jabbertimeout & 0xff) << 8) | adcbatchid);
	statuspkt.adctrigoff = ((trigthresh + trigcomp) & 0xFFF) | ((pgagain & 0xF) << 12);

	slot = reserve_queue_slot();
	pknum = take_packet_number();
	memcpy(sendqueuebuf[slot], (const void*) &statuspkt, sizeof(statuspkt));
	((uint8_t*) sendqueuebuf[slot])[3] = stype;	// status pkt type (ENDSEQ or TIMED)
	((uint8_t*) sendqueuebuf[slot])[0] = pknum & 0xff;
	((uint8_t*) sendqueuebuf[slot])[1] = (pknum & 0xff00) >> 8;
	((uint8_t*) sendqueuebuf[slot])[2] = (pknum & 0xff0000) >> 16;

	item.slot = slot;
	item.len = sizeof(statuspkt);
	xQueueSend(sendqueueq, &item, 0);
}

// Lower-priority sender task: owns every sendudp() call in the firmware, and
// also now owns the decision to send a timed status packet (see below).
// Blocks up to 1 second at a time - long enough to stay effectively idle
// (zero CPU) whenever there's nothing to do, short enough to notice a due
// timed status without depending on ADC activity - then drains as fast as
// the network stack/hardware allow. Being strictly lower priority than the
// producer (see startudp()) is what guarantees it can never delay the
// ADC-facing path - the scheduler preempts it unconditionally the moment a
// new ADC notification needs servicing, even mid-send-prep.
static void netsendtask(void const *argument) {
	struct udp_pcb *pcb = (struct udp_pcb*) argument;
	senditem_t item;
	uint32_t laststatussecond = t1sec;	// don't fire a timed status right at boot

	sq_last_send_sec = t1sec;	// stall guard: measure "nothing sent" from the start of this task
	sq_alive_sec = t1sec;

	for (;;) {
		BaseType_t got = xQueueReceive(sendqueueq, &item, pdMS_TO_TICKS(1000));
		sq_alive_sec = t1sec;	// stall guard: this loop is still turning
		if (got != pdTRUE) {
//			nettx_diag_periodic();	// (disabled) one line when the TX mutex was contended - only showed the mutex working (netfix.h)
			// Nothing queued within 1 second - check whether a timed status
			// is due. Moved here (off the ADC-facing producer) because it
			// isn't tied to any ADC event, only to elapsed time; tracking
			// "seconds since the last status packet of any type" (rather
			// than the previous fixed t1sec % STAT_TIME grid) means a timed
			// status can no longer land almost back-to-back with an
			// end-of-sequence one - see UDP_SEND_QUEUE.md. laststatussecond
			// only needs updating here, in the one task that dispatches
			// every packet - no cross-task protection needed for it.
			if ((t1sec - laststatussecond) >= STAT_TIME) {
				enqueue_status(TIMED);
			}
			continue;
		}

		// Stall guard bookkeeping: what the sender is working on, and since when.
		sq_cur_slot = item.slot;
		sq_cur_len = item.len;
		sq_cur_type = ((uint8_t*) sendqueuebuf[item.slot])[3];
		sq_cur_pknum = ((uint32_t) ((uint8_t*) sendqueuebuf[item.slot])[0]) | ((uint32_t) ((uint8_t*) sendqueuebuf[item.slot])[1] << 8)
				| ((uint32_t) ((uint8_t*) sendqueuebuf[item.slot])[2] << 16);
		sq_busy_since = (t1sec != 0) ? t1sec : 1;	// 0 means idle

		// Wait for hardware to fully release this slot's pbuf from whatever
		// it last sent from it. Normally this is already true (the slot was
		// last used a whole ring - SEND_QUEUE_DEPTH sends - ago). Bounded:
		// this used to be an endless silent loop, which is what a leaked pbuf
		// reference turned into - all UDP stopped with nothing on the console.
		// Now it reports after STALL_SNAPSHOT_MS, keeps reporting, and gives
		// up and reboots after STALL_REBOOT_MS (see the stall guard block).
		// Still a short 1 ms retry, not a tight spin - this task is never on
		// the ADC's critical path.
		if (sendqueuepbuf[item.slot]->ref != 1) {
			const TickType_t waitstart = xTaskGetTickCount();
			uint32_t next_report = STALL_SNAPSHOT_MS;
			uint32_t waited = 0;
			int reported = 0;

			while (sendqueuepbuf[item.slot]->ref != 1) {
				waited = (uint32_t) (xTaskGetTickCount() - waitstart) * portTICK_PERIOD_MS;
				if (waited >= STALL_REBOOT_MS) {
					udp_stall_snapshot("sender: pbuf never released - giving up", waited);
					printf("udpstall: rebooting\n");
					osDelay(50);	// let the UART drain
					rebootme(STALL_REBOOT_WHY);
				}
				if (waited >= next_report) {
					if (!reported) {
						udp_stall_snapshot("sender: slow pbuf release", waited);
						reported = 1;
					} else {
						printf("udpstall: still waiting slot=%u ref=%u waited=%lums\n", item.slot, (unsigned) sendqueuepbuf[item.slot]->ref,
								(unsigned long) waited);
					}
					next_report += STALL_REPORT_EVERY_MS;
				}
				vTaskDelay(1);
			}
			if (reported) {
				printf("udpstall: slot %u released after %lums (slow release, not a leak)\n", item.slot, (unsigned long) waited);
			}
		}
		sendqueuepbuf[item.slot]->payload = sendqueuebuf[item.slot];
		sendqueuepbuf[item.slot]->len = item.len;
		sendqueuepbuf[item.slot]->tot_len = item.len;

		sendudp(pcb, sendqueuepbuf[item.slot], &udpdestip, UDP_PORT_NO);
		xSemaphoreGive(freeslots);	// data handed to lwIP; producer may reuse this slot now

		sq_sent_total++;
		sq_last_send_sec = t1sec;
		sq_busy_since = 0;	// idle again

		if (((uint8_t*) sendqueuebuf[item.slot])[3] == 4) {	// sample packet
			statuspkt.udpsent++;		// debug use adc udp sample packet sent count
			statuspkt.adcpktssent++;	// UDP sample packet counter
		} else {						// status packet (ENDSEQ or TIMED)
			laststatussecond = t1sec;
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

// Delayed DNS lookup result callback

void dnsfound(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
	if (ipaddr == NULL || ipaddr->addr == 0) {	// lwIP passes ipaddr == NULL itself when the lookup fails
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
	struct pbuf *p1, *p2;
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
	samplesarmed = 1;	// from here the ADC ISR may queue triggered samples (as the old task-side copy did)
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

		udp_stall_watchdog();	// cheap: acts at most once a second; reboots only if the sender is stuck (see stall guard)

		if (ulNotificationValue > 0) {		// we were notified
			sigsend = 0;
			// the triggered sample itself was already queued by the ADC ISR (enqueue_sample_isr)
			if ((gpslocked) && (jabbertimeout == 0) && (!(globalfreeze))) {
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
			 * idle. netsendtask() drains the queue and checks for a due
			 * timed status packet independently now (see there), so there
			 * is genuinely nothing left to do here - this branch is kept,
			 * rather than switching to a portMAX_DELAY wait, only because
			 * that's a separate decision (whether this task should ever
			 * wake without a real ADC notification for some other future
			 * reason) that hasn't been made yet. */
//			printf("ulNotificationValue = %d\n",ulNotificationValue );
		}

	} // forever while
}
