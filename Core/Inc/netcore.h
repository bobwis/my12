/*
 * netcore.h - lwIP core lock for raw API calls made outside tcpip_thread (build 10055, netfix.h switch E)
 *
 * lwIP's raw API is single-threaded: every call must come from tcpip_thread or from a task holding
 * the core lock (LWIP_TCPIP_CORE_LOCKING 1). netsendtask's udp_sendto(), the HTTP client started from
 * the LP and default tasks, the DNS lookups, httpd_init() and the Ethernet link thread used to call
 * it without the lock, racing tcpip_thread over the ARP table, TCP PCB lists and DNS state.
 *
 * The lock is a plain (non-recursive) FreeRTOS mutex and some of these paths are also reached from
 * tcpip_thread callbacks, which already hold it (e.g. returnpage() -> httploader() -> http_dlclient(),
 * myudp_recv() -> sendudp()). netcore_lock() therefore takes it only when the calling task doesn't
 * hold it yet, and tells netcore_unlock() whether to give it back:
 *
 *     const int took = netcore_lock();
 *     udp_sendto(...);
 *     netcore_unlock(took);
 *
 * It is a mutex with priority inheritance, not an interrupt mask, so the ADC IRQs are unaffected.
 * Lock order is core lock, then the TX mutex (as tcpip_thread already does): never take the core
 * lock while holding the TX mutex. Never wait (osDelay, semaphores) while holding it.
 */
#ifndef NETCORE_H_
#define NETCORE_H_

#include "netfix.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lwip/tcpip.h"

#if NETFIX_CORE_LOCK
static inline int netcore_lock(void) {
	if ((lock_tcpip_core == NULL)	// before tcpip_init()
	|| (xSemaphoreGetMutexHolder((SemaphoreHandle_t) lock_tcpip_core) == xTaskGetCurrentTaskHandle()))
		return (0);
	LOCK_TCPIP_CORE();
	return (1);
}

static inline void netcore_unlock(int took) {
	if (took)
		UNLOCK_TCPIP_CORE();
}
#else
static inline int netcore_lock(void) {
	return (0);
}

static inline void netcore_unlock(int took) {
	(void) took;
}
#endif

#endif /* NETCORE_H_ */
