/*
 h * neo7m.h
 *
 *  Created on: 14Jan.,2018
 *      Author: bob
 */

#ifndef NEO7M_H_
#define NEO7M_H_

#include "stm32f7xx_hal.h"
#include <time.h>

/*
 * neo7m.h
 *
 * Created: 13/09/2017 5:40:43 PM
 *  Author: bob
 *  Modified from Freqref project 14/01/2018
 */

/**
 * Derived in part from UBX GPS Library
 * Created by Danila Loginov, July 23, 2016
 * https://github.com/1oginov/UBX-GPS-Library
 */

#define GPS_UTC_OFFSET 315964800		// GPS EPOCH to UTC UNIX EPOCH (not including leap seconds)

//const unsigned char UBXGPS_HEADER[] = { 0xB5, 0x62 };

extern struct statpkt {
	uint32_t udppknum;		// udp packet sent index (24 bits, other 8 bits are packet type)

	struct UbxGpsNavPvt {
		//        Type  Name           Unit   Description (scaling)
		unsigned long iTOW; // ms     GPS time of week of the navigation epoch. See the description of iTOW for details.                     0
		unsigned short year; // y      Year UTC																			4
		unsigned char month; // month  Month, range 1..12 UTC																6
		unsigned char day; // d      Day of month, range 1..31 UTC														7
		unsigned char hour; // h      Hour of day, range 0..23 UTC														8
		unsigned char min;        // min    Minute of hour, range 0..59 UTC									9
		unsigned char sec; // s      Seconds of minute, range 0..60 UTC													10
		char valid;      // -      Validity Flags (see graphic below)										11
		unsigned long tAcc;    // ns     Time accuracy estimate UTC													12
		long nano; // ns     Fraction of second, range -1e9..1e9 UTC																16
		unsigned char fixType; // -      GNSSfix Type, range 0..5															20
		char flags;      // -      Fix Status Flags (see graphic below)												21
		unsigned char reserved1;  // -      Reserved							22
		unsigned char numSV;      // -      Number of satellites used in Nav Solution					23
		long lon;        // deg    Longitude (1e-7)									24
		long lat;        // deg    Latitude (1e-7)												28
		long height;   // mm     Height above Ellipsoid																32
		long hMSL; // mm     Height above mean sea level																		36
		unsigned long hAcc; // mm     Horizontal Accuracy Estimate																			40
		unsigned long vAcc; // mm     Vertical Accuracy Estimate																	44
		long velN;       // mm/s   NED north velocity														48
		long velE;       // mm/s   NED east velocity															52
		long velD;       // mm/s   NED down velocity															56
		long gSpeed;     // mm/s   Ground Speed (2-D)														60
		long heading;    // deg    Heading of motion 2-D (1e-5)												64
		unsigned long sAcc;       // mm/s   Speed Accuracy Estimate													68
		unsigned long headingAcc; // deg    Heading Accuracy Estimate (1e-5)											72
		unsigned short pDOP;      // -      Position DOP (0.01)														76
		short reserved2;  // -      Reserved																	78
		unsigned long reserved3; // -      Reserved																	80
	} NavPvt;

	uint32_t clktrim;		// Nominal 108MHz clock is actually this frequency
	uint16_t uid;			// 16 bits used
	uint16_t adcpktssent;	// Number of ADC pks sent in this trigger event
	uint16_t adctrigoff;	// adc trigger threshold above noise. also 4 bits of gain >> 12
	uint16_t adcbase;		// average background level seen by ADC
	uint32_t sysuptime;		// number of seconds system up from boot uptime
	uint32_t netuptime;		// number of seconds network up
	uint32_t gpsuptime;		// number of seconds gps locked
	uint8_t majorversion;	// major version of STM32 detector
	uint8_t minorversion;	// minor version of STM32 detector
	uint16_t adcnoise;		// adc average peak noise
	uint32_t auxstatus1;	// spare 16 bits, jabbering 8 bits, adcbatchid 8 bits
	uint32_t adcudpover;	// adc -> udp send overruns
	uint32_t trigcount;		// adc trigger count
	uint32_t udpsent;		// udp sample packets sent
	uint16_t build; //reserved3;		// build number from build 1028, originally was peak trig level
	uint16_t jabcnt;		// jabber counter
	uint32_t temppress;		// temperature top 12 bits, pressure bottom 20 bits
	uint32_t epochsecs;
	//  bconf is board configuration
	// bits 0..2 are Splat revision present (0 == no Spalt board fitted)
	// bits 3..4 are Pressure / Temp Sensor type.  0 = none, 1 = MPL3115, 2 = MPL115A2
	uint32_t bconf;			// the board configuration
	uint32_t reserved2;		// used to flag GPS and PPS state
	uint32_t telltale1;		// end of packet marker
} __attribute__((aligned(4),packed))  volatile statuspkt;

//extern __attribute__((aligned(4),packed)) CHALLENGE;

extern uint8_t gpslocked;		// Gps locked flag;
extern uint16_t gpsfake;	// fake GPS when set
extern uint8_t epochvalid;		// have we have worked out the epoch seconds?;
// set up the newo7 as we want it
extern HAL_StatusTypeDef setupneo(void);

// Update NavPvt
extern void updategps(void);

extern time_t calcepoch();

extern struct tm now;		// gps time updated every second
extern time_t epochtime;	// gps time updated every second
extern char trigtimestr[];	// last trig time as a string
extern char nowtimestr[];		// current time as as string
extern uint8_t muxdat[];

extern uint32_t ledhang;
extern int16_t meanwindiff;	// sliding mean of window differences

extern struct netif gnetif;	// LWIP network interface status struc

extern unsigned int gpsgood;	// GPS comms status
extern unsigned int globalfreeze;	// freeze UDP streaming
extern unsigned int circuitboardpcb;		// run-time determination of what daughterboard have we?   eg SPLATBOARD1
extern unsigned int newbuild;	// the new firmware build number from the server
extern unsigned int srvlcdbld;	// the (later) LCD firmware build number on the server
extern char lcdfile[32];
extern char fwfilename[32];
extern char loaderhost[48];
extern uint32_t lcdlen;
extern volatile uint32_t trigcomp;		// trigger threshold compensation provided by the server
extern uint32_t dl_filecrc;
extern int lptask_init_done;		// zero when lptask is not finished its init
extern char lcd_err_msg[16];

#endif /* NEO7M_H_ */

