/*
 * lcd.c
 *
 *  Created on: Nov 3, 2020
 *      Author: bob
 *
 * Includes extracts from  nextion.c IO lib derived from Freqref project
 */

// Nextion LCD support
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
//#include "stm32f7xx_hal.h"
#include "version.h"
#include "neo7m.h"
#include "adcstream.h"
#include <time.h>
#include "lcd.h"
#include "nextion.h"

// lcd state machine
#define LCD_IDLE 1
#define LCD_SENDCMD 2
#define LCD_WAITRESP 3
#define LCD_CMDTIMEOUT 4
#define LCD_GOTRESP 5
#define LCD_GOTRX 6

static int lcd_state = LCD_IDLE;	// state variable

uint8_t lcdrxbuffer[LCDRXBUFSIZE] = { "" };
uint8_t dmarxbuffer[DMARXBUFSIZE] = { "" };
int lcdrxoutidx = 0;			// consumed index into lcdrxbuffer

char currentpage[16] = { "" };
volatile uint8_t pagenum = 0;		// binary LCD page number

volatile uint8_t lcdstatus = 0;		// response code, set to 0xff for not set
volatile uint8_t lcdtouched = 0;		// this gets set to 0xff when an autonomous event or cmd reply happens
volatile uint8_t lcdpevent = 0;		// lcd reported a page. set to 0xff for new report
unsigned int dimtimer = DIMTIME;	// lcd dim imer
unsigned int cmdtimeout = 0;		// max timeout after issueing command
unsigned int rxtimeout = 0;			// receive timeout, reset in lcd_getch

uint8_t retcode = 0;	// if 3 lots of 0xff follow then this contains the associated LCD return code

int cycinc(int index, int limit) {
	if (++index >= limit)
		return (0);
	else
		return (index);
}

int cycdec(int index, int limit) {
	if (--index < 0)
		return (limit - 1);
	else
		return (0);
}

// UART 5 Rx DMA complete
void uart5_rxdone() {

	printf("UART5 Rx Complete\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void lcd_init() {
	HAL_StatusTypeDef stat;

	lcdrxoutidx = 0;		// consumer index
	stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// start Rx cyclic DMA
	if (stat != HAL_OK) {
		printf("Err HAL_UART_Receive_DMA uart5\n");
		return (stat);
	}
}

// request a new state in the LCD state machine
int lcd_newstate(int newstate) {

	if (lcd_state == LCD_IDLE) {		// anything is allowed
		lcd_state = newstate;
		return (newstate);
	}

	if ((lcd_state == LCD_SENDCMD) && (newstate == LCD_IDLE)) {		// sending a var doesn't get ack'd
		lcd_state = LCD_IDLE;
		return (newstate);
	}

	if ((lcd_state == LCD_SENDCMD) && (newstate == LCD_WAITRESP)) {

		printf("lcd_newstate: refused change from %d to %d\n", lcd_state, newstate);
		return (0);	// 0 means no state changed
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// put a char - non blocking using DMA
inline int lcd_putc(uint8_t ch) {
	HAL_StatusTypeDef stat;

//	printf("lcd_putc:%c\n", ch);
	osDelay(4);		// zzz

	stat = HAL_UART_Transmit_DMA(&huart5, &ch, 1);
	if (stat != HAL_OK) {
		printf("lcd_putc: Err HAL_UART_Transmoit_DMA uart5\n");
		return (stat);
	}
	return (stat);
}

// put a null terminated string
void lcd_puts(char *str) {
	int i;

	i = 0;
	while (str[i] != '\0') {
		lcd_putc(str[i++]);
	}
}

// Transmit completed callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {

	if (huart->Instance == UART5)
		;
//		printf("UART5 Tx Complete\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// get Rx chars if available - non blocking using DMA
// copies all dma rx'd chars into the lcd rx buffer
int lcd_rxdma() {
	int count = 0;

	while (DMARXBUFSIZE - DMA1_Stream0->NDTR != lcdrxoutidx) {		// something waiting in Rx buffer
		lcdrxbuffer[lcdrxoutidx] = dmarxbuffer[lcdrxoutidx];	// copy it to lcd rx buffer
		count++;
		lcdrxoutidx = cycinc(lcdrxoutidx, LCDRXBUFSIZE);	// cyclic bump lcd rx index
		return (count);
	}

	return (-1);
}

// get the next char from the lcdrx buffer if there is one available
int lcd_getc() {
	static int lastidx = 0;
	int ch;

	ch = -1;
	if (lastidx != lcdrxoutidx) {		// something there
		ch = lcdrxbuffer[lastidx];
		lastidx = cycinc(lastidx, LCDRXBUFSIZE);
		rxtimeout = 100;
	}
//	printf("lcd_getc() got %x\n", ch);
	return (ch);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// send a var string to the LCD (len max 255)
// terminate with three 0xff's
int writelcdvar(char *str) {
	char i = 0;
	volatile int result;

	if (lcd_newstate(LCD_SENDCMD) == LCD_SENDCMD) {
		lcd_puts(str);
		for (i = 0; i < 3; i++) {
			lcd_putc(0xff);
		}
		if (lcd_newstate(LCD_IDLE) == LCD_IDLE) {		// we dont expect a response to a command
			return (0);
		}
		printf("writelcdvar: LCD_SENDVAR failed\n");
	}
	return (-1);
}

// read the response to a general command from the lcd (not one needing return values)
// returns 0xff on timeout
// assumes 	lcdstatus = 0xff prior to call
int getlcdack() {
	static unsigned int trys = 0;

	if (lcd_newstate(LCD_WAITRESP) == LCD_WAITRESP) {			// we need the "ACK' from the LCD
		trys++;
		if ((trys > 10) && (lcdstatus == 0xff)) {
			printf("getlcdack: Timeout waiting for LCD response\n\r");
			trys = 0;
			return (-1);
		}
	}
	return (lcdstatus);
}

// send a cmd string to the LCD (len max 255)
// terminate with three 0xff's
int writelcdcmd(char *str) {
	char i = 0;
	volatile int result;

	if (lcd_newstate(LCD_SENDCMD)) {
		lcd_puts(str);
		for (i = 0; i < 3; i++) {
			lcd_putc(0xff);
		}
		if (lcd_newstate(LCD_WAITRESP) == LCD_WAITRESP) {		// we expect a response to a command
			return (0);
		}
		printf("writelcdcmd: LCD_SENDCMD failed\n");
	}
	return (-1);
}

// send two strings to the LCD (len max 255)
// terminate with three 0xff's
void writelcdcmd2(char *str, char *str2) {
	char i = 0;

	if (lcd_newstate(LCD_SENDCMD)) {

		lcd_puts(str);
		lcd_puts(str2);
		for (i = 0; i < 3; i++) {
			lcd_putc(0xff);
		}
		lcdstatus = 0xff;			// arm response variable - 0xff is unused status code
	}

}

// write a number digit on the LCD to a num object
void setndig(char *id, uint8_t val) {
	char msg[32];

	sprintf(msg, "%s.val=%1d", id, val);
	lcd_puts(msg);
}

// read a lcd named variable (unsigned long) expects numeric value
// return -1 for error
char getlcdnvar(char *id, unsigned long *data) {
	volatile unsigned char ack;

	lcd_puts("get ");
	writelcdcmd(id);
	ack = getlcdack();		// wait for response
	if (ack == 0xff) {
		printf("No response from getlcdnvar cmd\n\r");
	}
	if (lcdrxbuffer[0] == NEX_ENUM)		// numeric response: 4 bytes following
	{
		*data = *(unsigned long*) &lcdrxbuffer[1];
//		printf("getlcdnvar returned %li\n\r",*data);
		return (lcdrxbuffer[0]);
	} else {
		if (lcdrxbuffer[0] == NEX_SVAR)		// Variable Name Invalid
		{
			printf("getlcdnvar: var name %s invalid\n\r", id);
		}
	}
	return (-1);
}

// send some text to a lcd text object
void setlcdtext(char id[], char string[]) {
	char str[64];
	volatile int result;

	sprintf(str, "%s=\"%s\"", id, string);
	result = writelcdvar(str);
	if (result == -1) {		// wait for response
		printf("setlcdtext: Cmd failed\n\r");
	}
	return (result);
}

// send some numbers to a lcd obj.val object, param is binary long number
void setlcdbin(char *id, unsigned long value) {
	char buffer[16];
	volatile int result;

	sprintf(buffer, "%lu", value);
	lcd_puts(id);
	lcd_putc('=');
	result = writelcdcmd(buffer);
	if (result == -1) {		// wait for response
		printf("setlcdbin: Cmd failed\n\r");
	}
	return (result);
}

// send some numbers to a lcd obj.val object
void setlcdnum(char id[], char string[]) {
//printf("%s=%s",id,string);
	lcd_puts(id);
	lcd_putc('=');
	writelcdcmd(string);
}

// request the current lcd page
// this is processed mostly by the ISR, this routine assumes success
// return -1 for error

int getlcdpage() {
	volatile int result;

	printf("getlcdpage:\n");

	result = writelcdcmd("sendme");
	if (result == -1) {		// wait for response
		printf("getlcdpage: Cmd failed\n\r");
	}
	return (result);
}

// display a chosen page
// use cached pagename to skip if matched current page unless forced
// returns lcd response code
int setlcdpage(char *pagename, bool force) {

	if (lcd_newstate(LCD_SENDCMD)) {

		if (*pagename)		// not null
		{
			if (!(strncmp(pagename, currentpage, sizeof(currentpage))) || (force)) {	// not already on this page
				writelcdcmd2("page", pagename);
				strncpy(currentpage, pagename, sizeof(currentpage));
				getlcdack();
//				osDelay(100);		// wait a bit
				pagenum = getlcdpage();		// associate with its number
			}
		}
		return (0);
	}
	return (-1);
}

// Check if this is an LCD packet
// try to get a single message packet from the LCD
// returns packet and end index (or 0 or -1)
int isnexpkt(unsigned char buffer[], uint8_t size) {
	static uint8_t termcnt = 0;
	static uint8_t i = 0;
	int index, rawchar;
	volatile unsigned char ch;

	rawchar = lcd_getc();
	if (rawchar >= 0) {
#if 1
		if ((rawchar >= '0') && (rawchar <= 'z'))
			printf("rawch=0x%02x %c\n", rawchar, rawchar);
		else
			printf("rawch=0x%02x\n", rawchar);
#endif
		ch = rawchar & 0xff;
		buffer[i++] = ch;
		if (ch == 0xff) {
			termcnt++;
//			printf("isnexpkt: termcount=%d\n",termcnt);
			if (termcnt == 3) {
				printf("isnexpkt: Found terminator\n");
				index = i;
				i = 0;
				termcnt = 0;
				return (index);
			}
		} else {
			retcode = ch;	// remember ch prior to 0xff 0xff 0xff
			termcnt = 0;
		}

		if (i == size) { // overrun
			i = 0;
			termcnt = 0;
		}
	}
	if (rxtimeout > 0)
		rxtimeout--;
	if (rxtimeout == 0) {
		termcnt = 0;
		for (i = 0; i < size; buffer[i++] = 0)
			;
		i = 0;
		return (-1);
	}
	return (0);  // no char available
}

// Try to build an LCD RX Event packet
// returns: 0 nothing found (yet), > good event decodes, -1 error
int lcd_event_process(void) {
	static unsigned char eventbuffer[32];
	int i, result;

	result = isnexpkt(eventbuffer, sizeof(eventbuffer));
	if (result <= 0) {
		return (result);		// 0 = nothing found, -1 = timeout
	} else // got a packet of something
	{

		if ((eventbuffer[0] >= NEX_SINV) && (eventbuffer[0] <= NEX_SLEN)) {	// a status code packet - eg error
			lcdtouched = 0;
			lcdpevent = 0;
			lcdstatus = eventbuffer[0];
			if (eventbuffer[0] != NEX_SOK)		// returned status from instruction was not OK
				printf("lcd_event_process: LCD Sent Error status 0x%02x\n\r", eventbuffer[0]);
		} else  // this is either a touch event or a response to a query packet
		{
			switch (eventbuffer[0]) {
			case NEX_ETOUCH:
				printf("lcd_event_process: Got Touch event\n");
				dimtimer = DIMTIME;
				lcdtouched = 0xff;		// its a touch
				lcdpevent = 0;
				break;
			case NEX_EPAGE:

				lcdtouched = 0;
				lcdpevent = 0xff;		// notify lcd page event happened
				pagenum = eventbuffer[1];
				printf("lcd_event_process: Got Page event, Page=%d\n", pagenum);
				break;
			default:
				printf("lcd_event_process: unknown response received\n");
				i = 0;
				while ((eventbuffer[i] != 0xff) && (i < sizeof(eventbuffer))) {
					printf(" 0x%02x", eventbuffer[i++]);
				}
				printf("\n");
				break;
			} // end case
		}
	}
}

// state machine for nextion
void processnex(void) {

	char ackbuf[24];
	volatile int count, result;

	if (dimtimer)		// lcd auto dim
		dimtimer--;

	count = lcd_rxdma();		// copy any dma'd rx chars into lcdbuf

//		printf("lcd_rxdma copied %d\n", count);

	switch (lcd_state) {

	case 0:				/// from init
		lcd_state = LCD_IDLE;
	case LCD_IDLE:
		if (count > 0) {		// new data in the rx buffer
			printf("LCD_IDLE -> LCD_GOTRX\n");
			lcd_state = LCD_GOTRX;
		}
		break;

	case LCD_GOTRX:
		result = lcd_event_process();
		if (result != 0) {	// wait until a complete event packet or timeout
			printf("LCD_GOTRX -> LCD_IDLE\n");
			lcd_state = LCD_IDLE;
		}
		break;

	case LCD_SENDCMD:
		printf("LCD_SENDCMD -> LCD_WAITRESP\n");
		lcd_state = LCD_WAITRESP;
		break;

	case LCD_CMDTIMEOUT:
		printf("LCD_CMDTIMEOUT\n");
		printf("LCD_CMDTIMEOUT -> LCD_IDLE\n");
		lcd_state = LCD_IDLE;
		break;

	case LCD_WAITRESP: {
		result = lcd_event_process();
		if (result > 0) {	// wait until a complete event packet or timeout
			printf("LCD_WAITRESP -> LCD_IDLE\n");
			lcd_state = LCD_IDLE;
//			gotresponse = 1;
		} else {
			if (result < 0) {	// timeout or error
				printf("LCD_WAITRESP TIMEOUT\n");
				printf("LCD_WAITRESP -> LCD_IDLE\n");
				lcd_state = LCD_IDLE;
			}
		}  // result == 0, wait
		break;
	}
	default:
		printf("processnex case lost\n");
		break;
	}
}
