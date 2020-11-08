/*
 * nextion.h
 *
 *  Created on: 5 Nov 2020
 *      Author: bob
 *      from my Freqref project 15/09/17
 */

#ifndef INC_NEXTION_H_
#define INC_NEXTION_H_

 // Nextion LCD definitions


volatile extern uint8_t	lcdstatus;
volatile extern uint8_t lcdtouched;
extern uint8_t lcdrxbuffer[];
volatile extern uint8_t lcdpevent;		// lcd reported a page. set to 0xff for new report
volatile extern uint8_t pagenum;		// binary LCD page number
unsigned int dimtimer;

// try to get one packet from the LCD
extern void decodelcd(void);

// write a number digit on the LCD to a num object
extern void setndig(char *, uint8_t);

// interrogate the lcd for its current display page
extern int getlcdpage(void);

// display a chosen page
extern int setlcdpage(char *, bool);

// read a lcd named variable (unsigned long) expects numeric value
// return -1 for error
extern char getlcdnvar(char *, unsigned long *);

// write to a text object
extern void setlcdtext(char *, char *);

// send some numbers to a lcd obj.val object, param is text
extern void setlcdnum(char *, char *);

// send some numbers to a lcd obj.val object, param is binary long number
extern void setlcdbin(char *, unsigned long);

// send a string to the LCD (len max 255)
// write to the lcd
extern void writelcd(char *);

//write a lcd cmd
extern int writelcdcmd(char *);


// Nextion return status codes
// Device only returns error codes unless sys var bkcmd is non zero

#define NEX_SINV	0x00	// Invalid Instruction
#define NEX_SOK		0x01	// successful Instruction
#define NEX_SID		0x02	// Component ID invalid
#define NEX_SPAGE	0x03	// Page ID invalid
#define NEX_SPIC	0x04	// Picture ID Invalid
#define NEX_SFONT	0x05	// Font ID Invalid
#define NEX_SBAUD	0x11	// Baud Invalid
#define NEX_SCURVE	0x12	// Curve ControlInvalid
#define NEX_SVAR	0x1A	// Variable Name Invalid
#define NEX_SVOP	0x1B	// Variable Operation Invalid
#define NEX_SASS	0x1c	// Failed to assign
#define NEX_SEEP	0x1d	// EEPROM op failed
#define NEX_SPAR	0x1e	// Parameter quantity wrong
#define NEX_SIO		0x1f	// IO Operation failed
#define NEX_SESC	0x20	// Undefined escape char used
#define NEX_SLEN	0x23	// Too long variable name

#define NEX_ETOUCH	0x65	// Touch event
#define NEX_EPAGE	0x66	// Current Page ID from sendme cmd
#define NEX_ECOOR	0x67	// Touch coord data event
#define NEX_ETSLP	0x68	// Touch event in sleep mode
#define NEX_ESTR	0x70	// String variable data returns
#define NEX_ENUM	0x71	// Numeric variable dat returns
#define NEX_ESLEEP	0x86	// Device entered auto sleep
#define NEX_EWAKE	0x87	// Device automatically woke up
#define NEX_EUP		0x88	// Successful system start-up
#define NEX_ESDUG	0x89	// SD card starting upgrade
#define NEX_EDFIN	0xfd	// Data transparent transmit finished
#define NEX_EDRDY	0xfe	// Data transparent transmit ready

#define NEX_CRED 	63488	// Red
#define NEX_CBLUE 	31		// Blue
#define NEX_CGREY 	33840	// Gray
#define NEX_CBLACK	0		// Black
#define NEX_CWHITE	65535	// White
#define NEX_CGREEN	2016	// Green
#define NEX_CBROWN 	48192 	// Brown
#define NEX_CYELLOW 65504 	// Yellow

#define NEX_TRED 	"63488"	// Red
#define NEX_TBLUE 	"31"		// Blue
#define NEX_TGREY 	"33840"	// Gray
#define NEX_TBLACK	"0"		// Black
#define NEX_TWHITE	"65535"	// White
#define NEX_TGREEN	"2016"	// Green
#define NEX_TBROWN 	"48192" 	// Brown
#define NEX_TYELLOW "65504" 	// Yellow


#endif /* INC_NEXTION_H_ */
