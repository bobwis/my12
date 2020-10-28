/*
 * version.h
 *
 *  Created on: 20Mar.,2018
 *      Author: bob
 */

#ifndef VERSION_H_
#define VERSION_H_

#define MAJORVERSION 0
#define MINORVERSION 10

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

#ifdef TESTING
#define BUILDNO 20003	// 16 bits  "S/W build number" of the lightning detector
#else
#define BUILDNO 10003	// 16 bits  "S/W build number" of the lightning detector
#endif

#ifdef SPLAT1
// Pressures sensor type fitted
#define MPL115A2	1
#define MPL3115A2  2
#define PNONE 0

#endif	/* SPLAT1 */
#endif /* VERSION_H_ */

// If we want localtime (+10H) not UTC, define LOCALTIME
#if 1
#define LOCALTIME
#endif

// Time between sending timesd status packets (seconds)
#ifdef TESTING
#define STAT_TIME 2
#else
#define STAT_TIME 120
#endif

#ifndef TESTING
#if 1
#define HARDWARE_WATCHDOG 1
#endif
#endif


/*
#define LWIP_DEBUG
#define LWIP_PLATFORM_DIAG(message) 	printf("mydebug LWIP: %s\n", message);
*/



