/*
 * version.h
 *
 *  Created on: 20Mar.,2018
 *      Author: bob
 */

#ifndef VERSION_H_
#define VERSION_H_

#define MAJORVERSION 0
#define MINORVERSION 33

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
#define BUILD 10056	// 10056 (fw 0.33, Oct 8, EXPERIMENT for the detector-18 soak only): dual detector (edge OR STA/LTA, dual at boot), adaptive STA/LTA ratio, 32-sample STA, filter feeds both detectors; status ra= so= eo=; server keys det slk slr slm. 10055 (fw 0.33, Oct 7): raw lwIP calls outside tcpip_thread take the core lock (netcore.h, netfix.h E/F - UDP sends, HTTP client, DNS, httpd_init, link thread), MTU back to 1500 (no more fragmenting every sample packet), console S... status line off at boot (Ctrl-E: off/60 s/1 s), lwipopts redefinition tidy-ups. 10054 (fw 0.33, Oct 7): MEMP_OVERFLOW_CHECK 2 -> 1 - the lwIP pool check masked the ADC scan IRQ, so ~1.8% of buffers were never scanned and triggered ones dropped (lt); status line adds lat= (scan IRQ entry latency, us) and sk= (unscanned buffers). 10053 (fw 0.33, Oct 7): local-strike alert check moved from the ADC ISR to the UDP send task (ISR peak back down). 10052 (fw 0.33, Oct 7): fix the 10051 LCD reload loop (a late "get sys0" reply was lost, so the LCD looked out of date). 10051 (fw 0.33, Oct 7): impulse filter (jump-hold, on by default) against switching-converter spikes, LCD TRIGGER overlay/tone only for local strikes (alert_mv 100), backlight idle/bright rework with the level stored in the LCD, remote settings table (keys al, dsp, dsk, dsn added), STA/LTA detector available (Ctrl-T, off). 10050 (fw 0.32, Oct 6): AGC lockout fix (near-miss test tracks the threshold), triggering buffer copied in the ADC ISR (no wrong/torn buffers), ADC ISR load 83% -> ~40% (DMA buffer in DTCM, scan loop in registers), prompt ENDSEQ with the right batch id, console input re-arm, console status line (ISR load). 10049 (fw 0.31, Sep 27): UDP send-path stall guard (10048) plus the Ethernet/lwIP thread-safety fixes in netfix.h. 10048 = stall guard only. 10047 = Sep 23 send-queue redesign, same number as the Sep 18 release. The detector self-updates whenever the server advertises a different build, so only change this when publishing.
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

// One-line console status (S ... line) every N seconds, 0 = off. Off at boot; console Ctrl-E cycles
// off -> 60 s -> 1 s (bench scripts turn it on). Each line is ~100 bytes of blocking UART output from the
// LP task (~9 ms), which also runs the 10 ms / 100 ms AGC timing, so 1 s is the fastest setting.
#define CONSOLE_STATUS_SECS 0

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


