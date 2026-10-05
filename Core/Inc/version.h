/*
 * version.h
 *
 *  Created on: 20Mar.,2018
 *      Author: bob
 */

#ifndef VERSION_H_
#define VERSION_H_

#define MAJORVERSION 0
#define MINORVERSION 32

// TESTING Speeds up the frequency of status packets
// and uses different target IP addresses
#if	0
#define TESTING
#if 0
#define configGENERATE_RUN_TIME_STATS 1

#define USE_TRACE_FACILITY 1
#define USE_STATS_FORMATTING_FUNCTIONS 1
#endif
#endif

// piggy back splat board ver 1 present
#if 1
#define SPLAT1
#endif

// 21 May 2023 - updated IDE
#define BUILD 10050	// 10050 (fw 0.32, Oct 6): AGC lockout fix (near-miss test tracks the threshold), triggering buffer copied in the ADC ISR (no wrong/torn buffers), ADC ISR load 83% -> ~40% (DMA buffer in DTCM, scan loop in registers), prompt ENDSEQ with the right batch id, console input re-arm, console status line (ISR load). 10049 (fw 0.31, Sep 27): UDP send-path stall guard (10048) plus the Ethernet/lwIP thread-safety fixes in netfix.h. 10048 = stall guard only. 10047 = Sep 23 send-queue redesign, same number as the Sep 18 release. The detector self-updates whenever the server advertises a different build, so only change this when publishing.
#ifndef TESTING
#define BUILDNO BUILD	// 16 bits  "S/W build number" of the lightning detector
#else
#define BUILDNO BUILD+1000	// 16 bits  "S/W build number" of the lightning detector
#endif
/*
 * see 	circuitboardpcb = LIGHTNINGBOARD1;		// prototype 1	line 2063 in main.c
 *  circuitboardpcb = LIGHTNINGBOARD2;		// Rev 1A and Rev
 */
#ifdef SPLAT1
// Pressures sensor type fitted
#define MPL115A2	1
#define MPL3115A2  2
#define PNONE 0

#endif	/* SPLAT1 */
#endif

// If we want localtime (+10H) not UTC, define LOCALTIME
#if 1
#define LOCALTIME
#endif

// Time between sending timed status packets (seconds)
#ifdef TESTING
#define STAT_TIME 2
#else
#define STAT_TIME 120
#endif

// One-line console status every N seconds (0 = off). Independent of TESTING.
// Each line is ~80 bytes of blocking UART output from the LP task (~7 ms),
// which also runs the 10 ms / 100 ms AGC timing, so keep this >= 1.
#define CONSOLE_STATUS_SECS 1	// experiment branch: bench scripts (tools/bench/sweep.py) parse one line per second; 60 in releases

// CPU half clock speed
#define  CCLK  108000000

#ifndef TESTING
#if 1
#define HARDWARE_WATCHDOG 1
#endif


#define SPLATBOARD1	11
#define SPLATBOARD2 12
// see 2136 in main.c - needs manual substitution of LIGHTNINGBOARD version
#define LIGHTNINGBOARD1	21
#define LIGHTNINGBOARD2	22
#define UNKNOWNPCB 0


#ifdef TESTING
//#define SERVER_DESTINATION "lightning.local"
//#define SERVER_DESTINATION "10.10.201.240"
#define HTTP_CONTROL_SERVER "lsrv.vk4ya.com"
#else
#define HTTP_CONTROL_SERVER "lsrv.vk4ya.com"
#endif


/*
#define LWIP_DEBUG
#define LWIP_PLATFORM_DIAG(message) 	printf("mydebug LWIP: %s\n", message);
*/

#endif /* VERSION_H_ */


