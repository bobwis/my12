/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : Target/lwipopts.h
  * Description        : This file overrides LwIP stack default configuration
  *                      done in opt.h file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion --------------------------------------*/
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "main.h"

/*-----------------------------------------------------------------------------*/
/* Current version of LwIP supported by CubeMx: 2.1.2 -*/
/*-----------------------------------------------------------------------------*/

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */
#include "version.h"
/* USER CODE END 0 */

#ifdef __cplusplus
 extern "C" {
#endif

/* STM32CubeMX Specific Parameters (not defined in opt.h) ---------------------*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- WITH_RTOS enabled (Since FREERTOS is set) -----*/
#define WITH_RTOS 1
/* Temporary workaround to avoid conflict on errno defined in STM32CubeIDE and lwip sys_arch.c errno */
#undef LWIP_PROVIDE_ERRNO
/*----- CHECKSUM_BY_HARDWARE enabled -----*/
#define CHECKSUM_BY_HARDWARE 1
/*-----------------------------------------------------------------------------*/

/* LwIP Stack Parameters (modified compared to initialization value in opt.h) -*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- Value in opt.h for LWIP_DHCP: 0 -----*/
#define LWIP_DHCP 1
/*----- Default Value for LWIP_DNS: 0 ---*/
#define LWIP_DNS 1
/*----- Default Value for MEMP_NUM_TCP_PCB: 5 ---*/
#define MEMP_NUM_TCP_PCB 64
/*----- Default Value for LWIP_TCPIP_CORE_LOCKING: 0 ---*/
#define LWIP_TCPIP_CORE_LOCKING 1
/*----- Default Value for SYS_LIGHTWEIGHT_PROT: 1 ---*/
#define SYS_LIGHTWEIGHT_PROT 1
/*----- Value in opt.h for MEM_ALIGNMENT: 1 -----*/
#define MEM_ALIGNMENT 4
/*----- Default Value for MEM_SIZE: 1600 ---*/
#define MEM_SIZE 10000
/*----- Default Value for MEMP_OVERFLOW_CHECK: 0 ---*/
#define MEMP_OVERFLOW_CHECK 1	// not 2: memp_overflow_check_all() walks every pool element under SYS_ARCH_PROTECT on each alloc/free,
								// masking the ADC scan IRQ (TIM5, prio 5) for a whole buffer - ~1.8% of buffers went unscanned (10054)
/*----- Default Value for MEMP_SANITY_CHECK: 0 ---*/
#define MEMP_SANITY_CHECK 1
/*----- Default Value for MEM_OVERFLOW_CHECK: 0 ---*/
#define MEM_OVERFLOW_CHECK 2
/*----- Default Value for MEM_SANITY_CHECK: 0 ---*/
#define MEM_SANITY_CHECK 1
/*----- Default Value for LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT: 0 ---*/
#define LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT 1
/* LWIP_RAM_HEAP_POINTER removed: it pinned lwIP's mem.c heap to a hardcoded
 * address (0x20048000) *inside* the same internal SRAM everything else uses -
 * mem.c's own comment says this mechanism is for relocating the heap to
 * EXTERNAL memory, which this board doesn't have. The linker has no idea
 * that address is "reserved", so as .bss has grown over time the margin
 * between the end of .bss and this fixed address shrank silently with no
 * build-time warning, until they collided and corrupted both regions
 * (see the "heap element ..." assertions in mem.c). Removing this define
 * falls back to mem.c's own default (a normal, linker-placed static array,
 * mem.c:378-380) which can never overlap anything else, no matter how much
 * static RAM the firmware uses in the future. */
/*----- Default Value for MEMP_NUM_PBUF: 16 ---*/
#define MEMP_NUM_PBUF 24
/*----- Default Value for MEMP_NUM_RAW_PCB: 4 ---*/
#define MEMP_NUM_RAW_PCB 8
/*----- Default Value for MEMP_NUM_TCP_PCB_LISTEN: 8 ---*/
#define MEMP_NUM_TCP_PCB_LISTEN 16
/*----- Default Value for MEMP_NUM_TCP_SEG: 16 ---*/
#define MEMP_NUM_TCP_SEG 32
/*----- Default Value for PBUF_POOL_SIZE: 16 ---*/
#define PBUF_POOL_SIZE 42
/*----- Default Value for ARP_TABLE_SIZE: 10 ---*/
#define ARP_TABLE_SIZE 32
/*----- Default Value for ARP_QUEUEING: 0 ---*/
#define ARP_QUEUEING 1
/*----- Default Value for ARP_QUEUE_LEN: 3 ---*/
#define ARP_QUEUE_LEN 32
/*----- Value in opt.h for LWIP_ETHERNET: LWIP_ARP || PPPOE_SUPPORT -*/
#define LWIP_ETHERNET 1
/*----- Default Value for LWIP_RAW: 0 ---*/
#define LWIP_RAW 1
/*----- Value in opt.h for LWIP_DNS_SECURE: (LWIP_DNS_SECURE_RAND_XID | LWIP_DNS_SECURE_NO_MULTIPLE_OUTSTANDING | LWIP_DNS_SECURE_RAND_SRC_PORT) -*/
#define LWIP_DNS_SECURE 7
/*----- Value in opt.h for TCP_SND_QUEUELEN: (4*TCP_SND_BUF + (TCP_MSS - 1))/TCP_MSS -----*/
#define TCP_SND_QUEUELEN 9
/*----- Value in opt.h for TCP_SNDLOWAT: LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1) -*/
#define TCP_SNDLOWAT 1071
/*----- Value in opt.h for TCP_SNDQUEUELOWAT: LWIP_MAX(TCP_SND_QUEUELEN)/2, 5) -*/
#define TCP_SNDQUEUELOWAT 5
/*----- Value in opt.h for TCP_WND_UPDATE_THRESHOLD: LWIP_MIN(TCP_WND/4, TCP_MSS*4) -----*/
#define TCP_WND_UPDATE_THRESHOLD 536
/*----- Default Value for LWIP_NETIF_STATUS_CALLBACK: 0 ---*/
#define LWIP_NETIF_STATUS_CALLBACK 1
/*----- Value in opt.h for LWIP_NETIF_LINK_CALLBACK: 0 -----*/
#define LWIP_NETIF_LINK_CALLBACK 1
/*----- Value in opt.h for TCPIP_THREAD_STACKSIZE: 0 -----*/
#define TCPIP_THREAD_STACKSIZE 1024
/*----- Value in opt.h for TCPIP_THREAD_PRIO: 1 -----*/
#define TCPIP_THREAD_PRIO osPriorityNormal
/*----- Value in opt.h for TCPIP_MBOX_SIZE: 0 -----*/
#define TCPIP_MBOX_SIZE 6
/*----- Value in opt.h for SLIPIF_THREAD_STACKSIZE: 0 -----*/
#define SLIPIF_THREAD_STACKSIZE 1024
/*----- Value in opt.h for SLIPIF_THREAD_PRIO: 1 -----*/
#define SLIPIF_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_THREAD_STACKSIZE: 0 -----*/
#define DEFAULT_THREAD_STACKSIZE 1024
/*----- Value in opt.h for DEFAULT_THREAD_PRIO: 1 -----*/
#define DEFAULT_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_UDP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_UDP_RECVMBOX_SIZE 6
/*----- Value in opt.h for DEFAULT_TCP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_TCP_RECVMBOX_SIZE 6
/*----- Value in opt.h for DEFAULT_ACCEPTMBOX_SIZE: 0 -----*/
#define DEFAULT_ACCEPTMBOX_SIZE 6
/*----- Default Value for LWIP_TCPIP_TIMEOUT: 0 ---*/
#define LWIP_TCPIP_TIMEOUT 1
/*----- Default Value for LWIP_SO_RCVTIMEO: 0 ---*/
#define LWIP_SO_RCVTIMEO 1
/*----- Default Value for LWIP_SO_LINGER: 0 ---*/
#define LWIP_SO_LINGER 1
/*----- Value in opt.h for RECV_BUFSIZE_DEFAULT: INT_MAX -----*/
#define RECV_BUFSIZE_DEFAULT 20000
/*----- Default Value for LWIP_HTTPD: 0 ---*/
#define LWIP_HTTPD 1
/*----- Default Value for LWIP_HTTPD_CGI_SSI: 0 ---*/
#define LWIP_HTTPD_CGI_SSI 1
/*----- Default Value for LWIP_HTTPD_SSI: 0 ---*/
#define LWIP_HTTPD_SSI 1
/*----- Default Value for LWIP_HTTPD_SUPPORT_POST: 0 ---*/
#define LWIP_HTTPD_SUPPORT_POST 1
/*----- Default Value for LWIP_HTTPD_MAX_TAG_INSERT_LEN: 192 ---*/
#define LWIP_HTTPD_MAX_TAG_INSERT_LEN 254
/*----- Default Value for LWIP_HTTPD_SUPPORT_EXTSTATUS: 0 ---*/
#define LWIP_HTTPD_SUPPORT_EXTSTATUS 1
/*----- Default Value for LWIP_HTTPD_SUPPORT_11_KEEPALIVE: 0 ---*/
#define LWIP_HTTPD_SUPPORT_11_KEEPALIVE 1
/*----- Default Value for LWIP_HTTPD_SSI_INCLUDE_TAG: 1 ---*/
#define LWIP_HTTPD_SSI_INCLUDE_TAG 0
/*----- Default Value for LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR: 0 ---*/
#define LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR 1
/*----- Value in opt.h for HTTPD_USE_CUSTOM_FSDATA: 0 -----*/
#define HTTPD_USE_CUSTOM_FSDATA 1
/*----- Value in opt.h for LWIP_STATS: 1 -----*/
#define LWIP_STATS 0
/* CHECKSUM_GEN_UDP must be done in software: HW checksum insertion is per-Ethernet-frame and
 * produces a wrong checksum once a datagram is split into multiple IP fragments (see ethernetif.c
 * TxConfig.ChecksumCtrl, now ETH_CHECKSUM_IPHDR_INSERT). Software computes it once, correctly,
 * over the whole datagram before fragmentation. */
#define CHECKSUM_GEN_UDP 1
/*----- Value in opt.h for CHECKSUM_GEN_ICMP6: 1 -----*/
#define CHECKSUM_GEN_ICMP6 0
/*----- Value in opt.h for CHECKSUM_CHECK_IP: 1 -----*/
#define CHECKSUM_CHECK_IP 0
/*----- Value in opt.h for CHECKSUM_CHECK_UDP: 1 -----*/
#define CHECKSUM_CHECK_UDP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP6: 1 -----*/
#define CHECKSUM_CHECK_ICMP6 0
/*----- Default Value for LWIP_DBG_MIN_LEVEL: LWIP_DBG_LEVEL_ALL ---*/
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_WARNING
/*----- Default Value for LWIP_DBG_TYPES_ON: LWIP_DBG_ON ---*/
#define LWIP_DBG_TYPES_ON LWIP_DBG_OFF
/*-----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
#include "netfix.h"

/* ---- lwIP inter-task protection (see Core/Inc/netfix.h, switch B) ------------------
 * SYS_LIGHTWEIGHT_PROT was 0 from the initial commit (also in the .ioc). opt.h: "This is
 * required when using lwIP from more than one context!" - pbuf_ref()/pbuf_free() reference
 * counts, the memp pools and mem_free()/mem_malloc() (LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
 * is 1, which makes mem_free() rely on this protection alone) were all unprotected while the
 * sender, the tcpip thread, the low-priority task and the link thread all use them.
 * We supply the three macros ourselves rather than use ST's sys_arch_protect(), which takes
 * a (non-recursive) FreeRTOS mutex per call: a BASEPRI critical section costs a few cycles,
 * nests by saving the previous mask, and only holds off interrupts at or below
 * configMAX_SYSCALL_INTERRUPT_PRIORITY for the ~100 ns of a reference-count update. The
 * regions lwIP protects are short and nothing calls these from an ISR that outranks the
 * ceiling. Setting NETFIX_LWIP_PROTECT to 0 restores the old (unprotected) configuration. */
#if NETFIX_LWIP_PROTECT
#include "FreeRTOS.h"
#include "task.h"
#define SYS_ARCH_DECL_PROTECT(lev)	uint32_t lev
#define SYS_ARCH_PROTECT(lev)		lev = (uint32_t) portSET_INTERRUPT_MASK_FROM_ISR()
#define SYS_ARCH_UNPROTECT(lev)		portCLEAR_INTERRUPT_MASK_FROM_ISR(lev)
#else
#undef SYS_LIGHTWEIGHT_PROT
#define SYS_LIGHTWEIGHT_PROT 0
#endif

#if NETFIX_DIAG
/* Richer assertion handler (ethernetif.c): same first line as before, plus which task hit
 * it and a dump of recent Ethernet TX/free events. arch/cc.h defines the stock printf
 * version only if this is not defined. */
void net_lwip_assert(const char *msg, int line, const char *file);
#undef LWIP_PLATFORM_ASSERT			/* a few sources (sys_arch.c) pull in arch/cc.h before this file */
#define LWIP_PLATFORM_ASSERT(x)		net_lwip_assert(x, __LINE__, __FILE__)
#endif

#define MEMP_NUM_SYS_TIMEOUT 	(LWIP_NUM_SYS_TIMEOUT_INTERNAL)+1
#define MEMP_NUM_UDP_PCB        8
#define IP_REASS_MAX_PBUFS     20
#define TCPIP_MBOX_SIZE 64
#define DEFAULT_TCP_RECVMBOX_SIZE 64
#define DEFAULT_RAW_RECVMBOX_SIZE 64
/* MEMP_NUM_PBUF sizes the pool backing PBUF_REF/PBUF_ROM header-only pbuf
 * structs (pbuf_alloc's PBUF_RAM/PBUF_POOL types use separate pools/heap and
 * don't count against this). Permanently-held REF/ROM pbufs in this project:
 * p1, p2 (udpstream.c, 2 - ps was retired when status packets moved onto the
 * shared send queue) plus one dedicated pbuf per SEND_QUEUE_DEPTH send queue
 * slot (udpstream.c, 96) = 98; sized to 128 for margin - this pool is shared
 * with lwIP's own internal pbuf use (RX, ARP, DNS, DHCP, httpd), which gets
 * none of that margin if this is sized right at the udpstream.c minimum. */
#define MEMP_NUM_PBUF 128
#define LWIP_TCPIP_TIMEOUT 100
#define LWIP_SO_RCVTIMEO 100
#define ETH_RX_BUFFER_SIZE 1536

/* Netif MTU override: our ~1500-byte UDP status/data packets (see UDPBUFSIZE
 * in Core/Inc/adcstream.h) were being dropped by some hops on the path to the
 * server. Lowering the netif's MTU below that packet size makes lwIP's
 * IP_FRAG (enabled by default - see opt.h) split each one into two smaller
 * IP fragments before it ever reaches the Ethernet driver, with no change to
 * any application code. 1000 gives ~500 bytes of margin under the RFC 8200
 * IPv6 minimum-MTU floor (1280 bytes) on both resulting fragments (996 and
 * 524 bytes on the wire). Applied in LWIP/Target/ethernetif.c, in
 * low_level_init(): netif->mtu = NETIF_MTU_OVERRIDE; - change the value here,
 * not there. */
#define NETIF_MTU_OVERRIDE 1000

#ifdef TESTING
#define LWIP_DEBUG
#define LWIP_PLATFORM_DIAG(x) do {printf x;} while(0)
#define LWIP_DBG_TYPES_ON LWIP_DBG_ON
#define LWIP_DEBUG
#define LWIP_STATS_DISPLAY 1
#define LWIP_SO_RCVTIMEO                  1              // default is 0

 /**
  * TCP_SND_BUF: TCP sender buffer space (bytes).
  * To achieve good performance, this should be at least 2 * TCP_MSS.
  */
#define TCP_SND_BUF                     (4 * TCP_MSS)
#endif
/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */
