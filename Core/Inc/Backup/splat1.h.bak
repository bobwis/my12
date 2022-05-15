/*
 * splat1.h
 *
 *  Created on: 26Sep.,2019
 *      Author: bob
 */

#ifndef SPLAT1_H_
#define SPLAT1_H_

#include "version.h"

extern void initsplat(void);
extern uint16_t getpgagain(void);
extern int bumppga(int);

extern void setpgagain(int gain);
extern uint32_t pressure, pressfrac, temperature, tempfrac;

extern uint32_t logampmode;

//extern time_t epochtime;	// gps time updated every second
extern char trigtimestr[];	// last trig time as a string
extern char nowtimestr[];		// current time as as string
extern char pressstr[];	// current pressure as a string
extern char tempstr[];	// current temperature as as string
extern char snstr[]; // current serial number id stuff as a string
extern char statstr[]; // current status as a string
extern char gpsstr[];	// gps status as a string

extern uint32_t pressure, pressfrac, temperature, tempfrac;
extern int16_t pgagain;
extern uint16_t boosttrys;	// prevent boost mode oscillating inside 3 min timer
extern uint16_t agc;

extern int psensor;		// pressure sensor type

extern const int pgamult[];		// maps from 0..7 gain control to the PGA

#endif /* SPLAT1_H_ */
