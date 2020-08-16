/*
 * version.h
 *
 *  Created on: 20Mar.,2018
 *      Author: bob
 */

#ifndef VERSION_H_
#define VERSION_H_

#define MAJORVERSION 0
#define MINORVERSION 8

// TESTING Speeds up the frequency of status packets
// and uses different target IP addresses
#if		0
#define TESTING
#if 0
#define configGENERATE_RUN_TIME_STATS 1
#endif
#endif

// piggy back splat board ver 1 present
#if 1
#define SPLAT1
#endif

#ifdef TESTING
#define MY_UID 1055	// 16 bits  "serial number" of the lightning detector
#else
#define MY_UID 1005	// 16 bits  "serial number" of the lightning detector
#endif

#ifdef SPLAT1
// Pressures sensor type fitted
#define MPL115A2	2
#define MPL3115A2  1
#define PNONE 0

#endif	/* SPLAT1 */
#endif /* VERSION_H_ */

// If we want localtime (+10H) not UTC, define LOCALTIME
#if 1
#define LOCALTIME
#endif
