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
#include "main.h"
#include "lcd.h"
#include "nextion.h"
#include "splat1.h"

// lcd state machine
#define LCD_IDLE 1
#define LCD_SENDCMD 2
#define LCD_WAITRESP 3
#define LCD_CMDTIMEOUT 4
#define LCD_GOTRESP 5
#define LCD_GOTRX 6
#define LCDXPIXELS 480

uint8_t lcdrxbuffer[LCDRXBUFSIZE] = { "" };
uint8_t dmarxbuffer[DMARXBUFSIZE] = { "" };

char sbuffer[40];		// lcd message building buffer
char lcdmodel[32];		// lcd model
struct tm timeinfo;		// lcd time
time_t localepochtime;	// lcd time

int lcdrxoutidx = 0;			// consumed index into lcdrxbuffer

volatile uint8_t lcd_currentpage = 0;		// The current LCD page number
volatile uint8_t our_currentpage = 0;		// The current page number we think we are on

volatile uint8_t lcdstatus = 0;		// response code, set to 0xff for not set
volatile uint8_t lcdtouched = 0;		// this gets set to 0xff when an autonomous event or cmd reply happens
volatile uint8_t lcdpevent = 0;		// lcd reported a page. set to 0xff for new report
unsigned int dimtimer = DIMTIME;	// lcd dim imer
unsigned int rxtimeout = 0;			// receive timeout, reset in lcd_getch
static int txdmadone = 0;			// Tx DMA complete flag (1=done, 0=waiting for complete)
volatile int lcd_initflag = 0;		// lcd and or UART needs re-initilising
volatile int lcduart_error = 0;		// lcd uart last err
volatile int lcdbright = 100;		// lcd brightness
volatile int lcd_txblocked = 0;		// flag to stop external callers writingto the LCD
int lastday = 0;		// the last date shown on the LCD
uint16_t lastsec = -1;	// the last second shown on the lcd

static int trigindex = 0; // index to next save data position for trigger charts
static int pressindex = 0; // index to next save data position for pressure charts
static unsigned char trigvec[LCDXPIXELS] = { 0 }, noisevec[LCDXPIXELS] = { 0 };
static unsigned char pressvec[LCDXPIXELS] = { 0 };
uint8_t retcode = 0;	// if 3 lots of 0xff follow then this contains the associated LCD return code

char nex_model[24];		// the Nextion model number read from the connected display
volatile int  lcd_sys0;	// used to store our firmware number of the LCd

inline int cycinc(int index, int limit) {
	if (++index >= limit)
		return (0);
	else
		return (index);
}

inline int cycdec(int index, int limit) {
	if (--index < 0)
		return (limit - 1);
	else
		return (0);
}

// wait for Tx DMA to complete - timeout if error
// then re-arm the wait flag
// returns -1 on timeout, 0 on okay
int wait_armtx(void) {
	volatile int timeoutcnt;

	timeoutcnt = 0;
	while (timeoutcnt < 150) {
		if (txdmadone == 1)		// its ready
			break;
//		printf("UART5 Wait Tx %d\n", timeoutcnt);
		timeoutcnt++;
#if 0

		{
			volatile int busywait;
			for (busywait = 0; busywait < 100000; busywait++)
				;
		}
#endif
		osDelay(1);		// wait 1ms +
	}

	if (timeoutcnt >= 250) {
		printf("UART5 Tx timeout\n");
		txdmadone = 1;	// re-arm the flag even though we have a problem
		return (-1);
	}
//	printf("UART5 Tx ARMED\n");

	return (0);
}

// UART 5 Rx DMA complete
void uart5_rxdone() {

//	printf("UART5 Rx Complete\n");
}

// Transmit completed callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	volatile uint32_t reg;

	if (huart->Instance == UART5) {

//	printf("UART5 Tx Complete\n");
#if 0
		while (1) {
			reg = (UART5->ISR);
			if ((reg & 0xD0) == 0xD0) // UART_IT_TXE and TC)
				break;
			else
				printf("HAL_UART_TxCpltCallback, TXE busy\n");
		}

#endif
		txdmadone = 1;		// its finished transmission
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void lcd_uart_init(int baud) {
	volatile HAL_StatusTypeDef stat;

//	printf("lcd_uart_init: LCD %d ***\n", baud);

	lcdrxoutidx = 0;		// buffer consumer index
#if 0
	HAL_UART_DMAStop(&huart5);
	HAL_UARTEx_DisableStopMode(&huart5);
#endif
#if 1
	HAL_UART_Abort(&huart5);
	HAL_UART_DeInit(&huart5);
#endif
	huart5.Instance = UART5;
	huart5.Init.BaudRate = baud;
#if 1
	huart5.Init.WordLength = UART_WORDLENGTH_8B;
	huart5.Init.StopBits = UART_STOPBITS_1;
	huart5.Init.Parity = UART_PARITY_NONE;
	huart5.Init.Mode = UART_MODE_TX_RX;
	huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart5.Init.OverSampling = UART_OVERSAMPLING_16;
	huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
#endif
	if (HAL_UART_Init(&huart5) != HAL_OK) {
		printf("lcd_init: Failed to change UART5 baud to %d\n", baud);
	}

#if 1
	stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// start Rx cyclic DMA
	if (stat != HAL_OK) {
		printf("lcd_init: 1 Err HAL_UART_Receive_DMA uart5 %d\n", stat);
	}
#endif
}

// lcd_init:  sends LCD reset command and them two set hi-speed commands
void lcd_init(int baud) {
	volatile HAL_StatusTypeDef stat;
	int i;

	const unsigned char lcd_reset[] = { "rest\xff\xff\xff" };
	const unsigned char lcd_fast[] = { "baud=230400\xff\xff\xff" };
	const unsigned char lcd_slow[] = { "baud=9600\xff\xff\xff" };
	int siz, page;
	volatile char *cmd;

//	printf("lcd_init: baud=%d\n", baud);
	if (!((baud == 9600) || (baud == 230400))) {
		printf("lcd_init: ***** bad baud rate requested %d **** \n", baud);
		return;
	}

	txdmadone = 0;	// TX is NOT free
	stat = HAL_UART_Transmit_DMA(&huart5, lcd_reset, sizeof(lcd_reset) - 1);  // current baud
	if (stat != HAL_OK) {
		printf("lcd_init: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	while (!(txdmadone)) {
//		printf("lcd_init: waiting for txdmadone\n");
		osDelay(1);		// wait for comms to complete
	}
	txdmadone = 0;	// TX is NOT free
	osDelay(800);

	if (baud == 9600)
		stat = HAL_UART_Transmit_DMA(&huart5, lcd_slow, sizeof(lcd_slow) - 1);		// if leading nulls on tx line
	else
		stat = HAL_UART_Transmit_DMA(&huart5, lcd_fast, sizeof(lcd_fast) - 1);		// if leading nulls on tx line
	if (stat != HAL_OK) {														// this cmd will be rejected
		printf("lcd_init: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	while (!(txdmadone)) {
//		printf("lcd_init: waiting1 for txdmadone\n");
		osDelay(1);		// wait for comms to complete
	}
	txdmadone = 0;	// TX is NOT free
	osDelay(120);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// put a char - non blocking using DMA
inline int lcd_putc(uint8_t ch) {
	HAL_StatusTypeDef stat;

	printf("lcd_putc:%c\n", ch);
	if (wait_armtx() == -1)
		return (-1);
	txdmadone = 0;	// TX in progress
	stat = HAL_UART_Transmit_DMA(&huart5, &ch, 1);
	if (stat != HAL_OK) {
		printf("lcd_putc: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
		return (stat);
	}
	return (stat);
}

// put a null terminated string
int lcd_puts(char *str) {
	HAL_StatusTypeDef stat;
	volatile int i;
	static char buffer[96];
	uint32_t reg;

	if (wait_armtx() == -1)
		return (-1);

	i = 0;
	while (str[i] != '\0') {
		buffer[i] = str[i];
		i++;
	}
	buffer[i] = '\0';

//	printf("lcd_puts: %s\n",buffer);

	txdmadone = 0;	// TX in progress
//	printf("lcd_puts: len=%d, [%s]\n", i, str);

	stat = HAL_UART_Transmit_DMA(&huart5, buffer, i);
	if (stat != HAL_OK) {
		printf("lcd_puts: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	return (stat);
}

// read the response to a general command from the lcd (not one needing return values)
// returns 0xff on timeout
// assumes 	lcdstatus has been prearmed with 0xff prior to call
uint8_t lcd_getlack() {
	static unsigned int trys = 0;

	processnex();
	while (lcdstatus == 0xff) {
		if (trys > 1000) {
			printf("getlcdack: Timeout waiting for LCD response\n\r");
			trys = 0;
			return (-1);
		}
		trys++;
		osDelay(1);
		processnex();
	} // while
	return (lcdstatus);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// get Rx chars if available - non blocking using DMA
// copies all dma rx'd chars into the lcd rx buffer
int lcd_rxdma() {
	HAL_StatusTypeDef stat;
	volatile int count = 0;
	volatile int dmaindex = 0;

	dmaindex = DMARXBUFSIZE - DMA1_Stream0->NDTR;  // next index position the DMA will fill
	if (dmaindex == 128) {
#if 1
		stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// restart Rx cyclic DMA
		if (stat != HAL_OK) {
			printf("lcd_rxdma: Err HAL_UART_Receive_DMA uart5 %d\n", stat);
		}
#endif
		dmaindex = 0;	// DMA count-to-go had zero
	}

	while (dmaindex != lcdrxoutidx) {		// dma in index has moved on from lcd rx out index
#if 0
		osDelay(10);
		printf("0x%02x ", dmarxbuffer[lcdrxoutidx]);
#endif
		lcdrxbuffer[lcdrxoutidx] = dmarxbuffer[lcdrxoutidx];	// copy the next char to lcd rx buffer
		count++;
		lcdrxoutidx = cycinc(lcdrxoutidx, LCDRXBUFSIZE);	// cyclic bump lcd rx index
	}
#if 0
	if (count > 0)
		printf("\n");
#endif
	return (count);
}

// get the next char from the lcdrx buffer if there is one available
// returns -1 if nothing
int lcd_getc() {
	static int lastidx = 0;
	int ch;

	ch = -1;
	if (lastidx != lcdrxoutidx) {		// something there
		ch = lcdrxbuffer[lastidx];
		lastidx = cycinc(lastidx, LCDRXBUFSIZE);
		rxtimeout = 100;
//  printf("lcd_getc() got %02x\n", ch);
	}

	return (ch);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// internal send a var string to the LCD (len max 255) - can't be blocked
// terminate with three 0xff's
// returns 0 if sent
int intwritelcdcmd(char *str) {
	char i = 0;
	char pkt[96];  //  __attribute__ ((aligned (16)));

	strcpy(pkt, str);
	strcat(pkt, "\xff\xff\xff");
	return (lcd_puts(pkt));
}

// send a var string to the LCD (len max 255) - can be blocked
// terminate with three 0xff's
// returns 0 if sent
int writelcdcmd(char *str) {
	char i = 0;
	char pkt[96];  //  __attribute__ ((aligned (16)));

	strcpy(pkt, str);
	strcat(pkt, "\xff\xff\xff");
	if (!(lcd_txblocked))
		return (lcd_puts(pkt));
	else
		return (-1);
}

// send some text to a lcd text object
int setlcdtext(char id[], char string[]) {
	int i;
	char str[96];
	volatile int result = 0;

	sprintf(str, "%s=\"%s\"", id, string);
//	printf("setcdtext: %s\n",str);
	result = writelcdcmd(str);
	return (result);
}

// send some numbers to a lcd obj.val object, param is binary long number
int setlcdbin(char *id, unsigned long value) {
	char buffer[32];
	volatile int result;

	sprintf(buffer, "%s=%lu", id, value);
	result = writelcdcmd(buffer);
	if (result == -1) {		// wait for response
		printf("setlcdbin: Cmd failed\n\r");  // never happens always 0
	}
	return (result);
}

// set the LCD bracklight brightness
void setlcddim(unsigned int level) {
	dimtimer = DIMTIME;
	if (level > 99)
		level = 99;
	setlcdbin("dim", level);
}

// request the current lcd page
// this is processed mostly by the ISR, this routine assumes success
// return -1 for error
int getlcdpage(void) {
	volatile int result;

//	printf("getlcdpage:\n");

	lcd_txblocked = 1;		// stop others sending to the LCD
	osDelay(150);			// wait for Tx queue to clear and hopefully Rx queue
	lcdstatus = 0xff;
	result = intwritelcdcmd("sendme");
	if (result == -1) {		// send err
		printf("getlcdpage: Cmd failed\n\r");
	}
	result = lcd_getlack();		// wait for a response
//	printf("getlcdpage: returned %d\n\r",result);

	while (result == -1) {	// try again
		result = intwritelcdcmd("sendme");
		if (result == -1) {		// send err
			printf("getlcdpage2: Cmd failed\n\r");
		}
		result = lcd_getlack();		// wait for a response
//		printf("getlcdpage2: returned %d\n\r",result);
	}
	lcd_txblocked = 0;		// allow others sending to the LCD
	return (result);
}

lcd_clearrxbuf() {
	int result;

	lcd_rxdma();			// clear the dma rx buffer
	result = lcd_getc();
	while (result != -1) {
		result = lcd_getc();	// consume anything in the copied rx buffer
	}
}

// find LCD model
lcd_getid(void) {
	int result;

	lcd_txblocked = 0;
	lcd_clearrxbuf();
	lcdstatus = 0xff;
	result = intwritelcdcmd("connect");
	if (result == -1) {		// send err
		printf("getid: Cmd failed\n\r");
	}
	result = lcd_getlack();		// wait for a response

	lcd_txblocked = 0;		// allow others sending to the LCD
	return (result);
}

// find LCD sys0 value
lcd_getsys0(void) {
	int result;

	printf("Getting SYS0\n");
	lcd_txblocked = 0;
	lcd_clearrxbuf();
	lcdstatus = 0xff;
	result = intwritelcdcmd("get sys0");
	if (result == -1) {		// send err
		printf("getsys0: Cmd failed\n\r");
	}
	result = lcd_getlack();		// wait for a response

	lcd_txblocked = 0;		// allow others sending to the LCD
	printf("getsys0: returned value=0x%08x\n",lcd_sys0);
	return (result);
}

// put LCD sys0 value
lcd_putsys0(void) {
	int value = 0x12345678;
	char cmd[16];
	int result;

	lcd_txblocked = 0;
	lcd_clearrxbuf();
	lcdstatus = 0xff;
	sprintf(cmd,"sys0=0x%08x",value);
	printf("Putting: %s\n",cmd);
	writelcdcmd(cmd);
	result = lcd_getlack();		// wait for a response (none expected)
	lcd_txblocked = 0;		// allow others sending to the LCD
}




// lcd page change event occurred
// lcd_currentpage is the current LCD page displayed
// newpage is the last one we have written
lcd_pagechange(uint8_t newpage) {
	unsigned char str[32];

	if (newpage == our_currentpage)			// we are already on the page the LCD is on
		return (our_currentpage);			// no action

	our_currentpage = newpage;
	switch (newpage) {
	case 0:		// changed to page 0
		lcd_time();
		lcd_date();
		break;
	case 1:		// changed to page 1 - statistics
		lcd_showvars();		// display vars on the lcd
		break;
	case 2:		// changed to page 2 - triger and noise plots
		lcd_trigcharts();		// display chart
		break;
	case 3:		// changed to page 3lid v
		// bring chart labels to the front
		lcd_presscharts();			// display pressure chart
		break;
	case 4:			// changed to page 4 (controls)
		lcd_controls();
		break;
	case 5:			// changed to page 5
		break;
	default:
		printf("Unknown page number\n");
		break;
	}
	return (our_currentpage);
}

// Check if this is an LCD packet
// try to get a single message packet from the LCD
// returns packet and end index (or 0 or -1)
int isnexpkt(unsigned char buffer[], uint8_t size) {
	static uint8_t termcnt = 0;
	static uint8_t i = 0;
	int index, rawchar;
	volatile unsigned char ch;

	rawchar = lcd_getc();		// get a char from the lcdrxbuffer if there is one
	if (rawchar >= 0) {
#if 0
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
//				printf(" # ");		// found terminator
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
	return (-2);  // no char available
}

// try to extract LCD type from what could be the connect string response
int decode_lcdtype(char *str) {
	int i, j, k;
	const char next[] = { "NX" };

	i = 0;
	j = 0;
	k = 0;
	nex_model[i] = '\0';

	while ((str[i] != '\0') && (str[i] != 0xff)) {
		if (str[i++] == next[j]) {
			j++;
			if (j >= 2) {		// found N...X
				nex_model[k++] = 'M';
				nex_model[k++] = 'X';
				while ((str[i] != '\0') && (str[i] != 0xff) && (str[i] != ',')) {
					nex_model[k++] = str[i++];
				}
				nex_model[i] = '\0';
				return (i);
			}
		}
	}
	return (0);
}

// try to extract LCD integer response from a 'Get' command
// eg 71 EC 5C BC 00 FF FF FF
int decode_int(char *str) {
	int i, number;

	i = 0;
	number = 0;

	if ((str[0] == 0x71) && (str[5] = 0xff) && (str[6] == 0xff) && (str[7] == 0xff)) {
		for (i = 1; i < 5; i++) {
			number = number >> 8;
			number = number | (str[i] << 24);
		}
		return (number);
	} else {
		return (0xffffffff);
	}
}

// Try to build an LCD RX Event packet
// returns: 0 nothing found (yet), > 0 good event decodes, -1 error
int lcd_event_process(void) {
	static unsigned char eventbuffer[96];
	volatile int i, result;
	char *str;

	result = isnexpkt(eventbuffer, sizeof(eventbuffer));
	if (result <= 0) {
		return (result);		// 0 = nothing found, -1 = timeout, -2=no char
	} else // got a packet of something
	{
		lcdstatus = eventbuffer[0];
		if ((eventbuffer[0] >= NEX_SINV) && (eventbuffer[0] <= NEX_SLEN)) {	// a status code packet - eg error
			if (eventbuffer[0] != NEX_SOK) {		// returned status from instruction was not OK

				printf("Nextion reported: ");
				switch (eventbuffer[0]) {
				case 0x00:
					printf("Invalid command\n");
					break;
				case 0x1A:
					printf("Invalid variable\n");		// so we might be on the wrong LCD page?
					getlcdpage();				// no point in waiting for result to come in the rx queue
					break;
				case 0x23:
					printf("Variable name too long\n");
					break;
				case 0x24:
					printf("Ser Buffer overflow\n");
					break;
				case 0x1e:
					printf("Invalid number of parameters\n");
					break;
				case 0x20:
					printf("Invalid escape char\n");
					break;
				case 0x1c:
					printf("Attribute assignment failed\n");
					break;
				case 0x12:
					printf("Invalid waveform ID\n");
					getlcdpage();				// no point in waiting for result to come in the rx queue
					break;
				case 0x01:
					printf("Successful execution\n");
					return (0);
					break;
				case 0x02:
					printf("Invalid Component ID\n");
					return (0);
					break;
				default:
					printf("Error status 0x%02x\n\r", eventbuffer[0]);
					break;
				}
				return (-1);		// some kindof error
			}
		} else  // this is either a touch event or a response to a query packet
		{
			switch (eventbuffer[0]) {
			case 0x24:
				printf("Serial Buffer Overflow!\n");
				return (1);
				break;

			case 0x63:	// This could be the start of the 'c' from "connect"
				decode_lcdtype(eventbuffer);
				if (nex_model[0] != '\0') {
					printf("Nextion LCD Model: %s\n", nex_model);
				}
				break;

			case 0x71:	// This is an integer variable from a "Get" command
				lcd_sys0 = decode_int(eventbuffer);
				if (nex_model[0] != '\0') {
					printf("Nextion LCD Integer: %d\n", lcd_sys0);
				}
				break;

			case NEX_ETOUCH:
				printf("lcd_event_process: Got Touch event %0x %0x %0x\n", eventbuffer[1], eventbuffer[2],
						eventbuffer[3]);

				if ((eventbuffer[1] == 4) && (eventbuffer[2] == 6)) {		// p4 id 6 brightness slider
					lcdbright = eventbuffer[3];
					if (lcdbright < 14)
						lcdbright = 14;		// prevent black
					setlcddim(lcdbright);
				}

				if ((eventbuffer[1] == 4) && (eventbuffer[2] == 8)) {		// p4 reset button
					printf("Reboot touch\n");
					osDelay(100);
					rebootme();
				}

				if ((eventbuffer[1] == 4) && (eventbuffer[2] == 2)) {		// p4 sound radio button
					if (eventbuffer[3] == 1) 		// sound on
						soundenabled = 1;
					else
						soundenabled = 0;
					printf("Sound touch\n");
				}

				if ((eventbuffer[1] == 4) && (eventbuffer[2] == 3)) {		// p4 LED radio button
					if (eventbuffer[3] == 1) 		// sound on
						ledsenabled = 1;
					else
						ledsenabled = 0;
					printf("LEDS touch\n");
				}
				break;

			case NEX_EPAGE:		// got page change event
//				printf("lcd_event_process: Got Page event, OldPage=%d, NewPage=%d\n", lcd_currentpage, eventbuffer[1]);
				setlcddim(lcdbright);
				if (((lcd_pagechange(eventbuffer[1]) < 0) || (lcd_pagechange(eventbuffer[1]) > 5)))	// page number limits
					printf("lcd_event_process: invalid page received %d\n", lcd_pagechange(eventbuffer[1]));
				else
					lcd_currentpage = lcd_pagechange(eventbuffer[1]);
				break;

			default:
				printf("lcd_event_process: unknown response received 0x%x\n", eventbuffer[0]);
				i = 0;
				while ((eventbuffer[i] != 0xff) && (i < sizeof(eventbuffer))) {
					printf(" 0x%02x", eventbuffer[i++]);
				}
				printf("\n");
				return (-1);
				break;
			} // end case
			return (0);
		}
	}
}

// processnex   process LCD errors and read from LCD
void processnex() {		// process Nextion - called at regular intervals
	volatile int result;
	static int i, delay = 0;

	switch (lcduart_error) {
	case HAL_UART_ERROR_NONE:
		break;
	case HAL_UART_ERROR_NE:
		printf("LCD UART NOISE\n");
		break;
	case HAL_UART_ERROR_FE:
		printf("LCD UART FRAMING\n");
		lcd_initflag = 1;		// assume display has dropped back to 9600
		break;
	case HAL_UART_ERROR_ORE:
		printf("LCD UART OVERRUN\n");
	default:
		break;
	}
//	printf("processnex: LCD UART_Err %0lx, lcd_initflag=%d\n", lcduart_error, lcd_initflag);
	lcduart_error = HAL_UART_ERROR_NONE;

	if (lcd_initflag == 1) {		// full init
		lcduart_error = HAL_UART_ERROR_NONE;
		printf("processnex: calling lcd_uart_init(9600)\n");
		lcd_uart_init(9600);	// switch us to 9600
		lcd_init(9600);		// try to reset LCD
		delay = 500;
		lcd_initflag = 2;		// request wait for lcd to process baud speedup command
		return;
	}

	if (lcd_initflag == 2) {	// wait after giving cmd for lcd to change LCD to fast
#if 0
		if (delay) {
			osDelay(1);
			delay--;
		} else
#else
		osDelay(500);
#endif
		lcd_initflag = 3;
		return;
	}

	if (lcd_initflag == 3) {	// uart only
		lcduart_error = HAL_UART_ERROR_NONE;
		printf("processnex: calling lcd_uart_init(230400)\n");
		lcd_uart_init(230400);
		lcd_init(230400);		// try to reset LCD
		lcd_initflag = 0;		// done
		osDelay(100);
		return;
	}

	lcd_rxdma();		// get any new characters received
	result = lcd_event_process();	// this can trigger the lcd_reinit flag
#if 0
	if (result > 0) {
		printf("processnex: Got something\n");
	}
#endif

	if (dimtimer > 50000) {
		dimtimer--;
	} else {
		dimtimer = 60000;
		i = lcdbright - (((lcdbright >> 1) + (lcdbright >> 2) + (lcdbright >> 4)));		// - 87.5% dim
		if (i < 2)
			i = 2;	// prevent black
//		printf("Auto Dimming now %d to %d\n", lcdbright, i);
		setlcddim(i);
	}
}

///////////////////////////////////////////////////////////////
//
//Application specific display stuff
//
//////////////////////////////////////////////////////////////

// send the time to t0.txt
void lcd_time() {
	unsigned char str[16];

	localepochtime = epochtime + (time_t) (10 * 60 * 60);		// add ten hours
	timeinfo = *localtime(&localepochtime);
	strftime(sbuffer, sizeof(sbuffer), "%H:%M:%S", &timeinfo);
	setlcdtext("t0.txt", sbuffer);

	if (gpslocked) {
		writelcdcmd("vis t3,0");	// hide warning
	} else {
		sprintf(str, "AQUIRE GPS:%d", statuspkt.NavPvt.numSV);
		setlcdtext("t3.txt", str);
		writelcdcmd("vis t3,1");
	}
}

// send the date to t1.txt (assumes timeinfo is current)
void lcd_date() {

	lastday = timeinfo.tm_yday;
	strftime(sbuffer, sizeof(sbuffer), "%a %e %h %Y ", &timeinfo);
	setlcdtext("t1.txt", sbuffer);
}

// populate the page2 vars
void lcd_showvars() {
	unsigned char str[96];
	unsigned long board;
	static uint16_t toggle = 0;

	switch (toggle) {
	case 0:
		sprintf(str, "%d.%d.%d.%d\n", myip & 0xFF, (myip & 0xFF00) >> 8, (myip & 0xFF0000) >> 16,
				(myip & 0xFF000000) >> 24);
		setlcdtext("t11.txt", str);
		sprintf(str, "%d", statuspkt.uid);
		setlcdtext("t10.txt", str);
		sprintf(str, "%d", statuspkt.adcpktssent);
		setlcdtext("t9.txt", str);
		sprintf(str, "%d", (globaladcavg & 0xfff));  // base
		setlcdtext("t8.txt", str);
		sprintf(str, "%d", abs(meanwindiff) & 0xfff);  // noise
		setlcdtext("t7.txt", str);
		sprintf(str, "%d", pgagain);	// gain
		setlcdtext("t6.txt", str);
		sprintf(str, "%d", statuspkt.adcudpover);	// overuns
		setlcdtext("t24.txt", str);
		toggle = 1;
		break;

	case 1:
		sprintf(str, "%d", statuspkt.NavPvt.numSV);	// satellites
		setlcdtext("t0.txt", str);
		sprintf(str, "%d", statuspkt.NavPvt.lat);	// latitude
		setlcdtext("t1.txt", str);
		sprintf(str, "%d", statuspkt.NavPvt.lon);	// longtitude
		setlcdtext("t2.txt", str);
		sprintf(str, "%d", statuspkt.NavPvt.height);	// height
		setlcdtext("t3.txt", str);
		toggle = 2;
		break;

	case 2:
		sprintf(str, "%d", statuspkt.trigcount);	// trigger count
		setlcdtext("t4.txt", str);
		sprintf(str, "%d", statuspkt.sysuptime);	// system up time
		setlcdtext("t5.txt", str);
		toggle = 3;
		break;

	case 3:
		sprintf(str, "Ver %d.%d Build:%d PCB=%d\\rUID=%lx %lx %lx", MAJORVERSION, MINORVERSION, BUILD, circuitboardpcb,
		STM32_UUID[0], STM32_UUID[1], STM32_UUID[2]);
//		sprintf(str, "Ver %d.%d, Build:%d\\rUID=%lx %lx %lx", MAJORVERSION, MINORVERSION, BUILD, STM32_UUID[0],
//				STM32_UUID[1], STM32_UUID[2]);
		setlcdtext("t26.txt", str);
		toggle = 0;
		break;
	}
}

// display / refresh  the entire trigger and noise chart
void lcd_trigcharts() {
	int i, buffi;
	unsigned char str[32];

#if 0
for (i=0; i<LCDXPIXELS; i++) {
	trigvec[i] = i % 120;
}
#endif

// refresh the labels as pior page queued commands can clobber them
	setlcdtext("t3.txt", "Triggers");
	setlcdtext("t18.txt", "Triggers");
	setlcdtext("t4.txt", "Noise");
	setlcdtext("t1.txt", "Noise");

	sprintf(str, "%d", statuspkt.trigcount);	// trigger count
	setlcdtext("t0.txt", str);
	sprintf(str, "%d", abs(meanwindiff) & 0xfff);  // noise
	setlcdtext("t2.txt", str);

//	writelcdcmd("tsw b2,0");	// disable touch controls
	writelcdcmd("b2.bco=123" /*23275*/);		// dark grey
	buffi = trigindex;
	for (i = 0; i < LCDXPIXELS; i++) {
		if (our_currentpage != 2)		// impatient user
			return;
		sprintf(str, "add 2,0,%d", trigvec[buffi]);
		writelcdcmd(str);
		osDelay(15);

		sprintf(str, "add 5,0,%d", noisevec[buffi]);
		writelcdcmd(str);
		osDelay(15);

		buffi++;
		if (buffi > LCDXPIXELS)
			buffi = 0;
	}
//	writelcdcmd("tsw b2,1");	// enable touch controls
	writelcdcmd("b2.bco=63422");		// normal grey
}

// called at regular intervals to add a point to the display
// update lcd trigger and noise plot memory,
// the page display may not be showing  ( 120 pix height)
void lcd_trigplot() {
	int val;
	static uint32_t lasttrig;
	unsigned char str[32];

// process the triggers
	val = statuspkt.trigcount - lasttrig;	// difference in trigs since last time
	lasttrig = statuspkt.trigcount;
	val = val * 32;		// scale up: n pixels per trigger

	if (val >= 120)
		val = 119;		// max Y
	trigvec[trigindex] = val;

// process the noise
	val = abs(meanwindiff) & 0xfff;
//	val = globaladcnoise;

	if (val < 0)
		val = 0;

	if (val >= 120)
		val = 119;		// max Y
	noisevec[trigindex] = val;

	if (our_currentpage == 2) {		// if currently displaying on LCD

		setlcdtext("t3.txt", "Triggers");
		setlcdtext("t18.txt", "Triggers");
		setlcdtext("t4.txt", "Noise");
		setlcdtext("t1.txt", "Noise");

		sprintf(str, "add 2,0,%d", trigvec[trigindex]);
		writelcdcmd(str);
		sprintf(str, "add 5,0,%d", noisevec[trigindex]);
		writelcdcmd(str);

		sprintf(str, "%d", statuspkt.trigcount);	// trigger count
		setlcdtext("t0.txt", str);
		sprintf(str, "%d", abs(meanwindiff) & 0xfff);  // noise
		setlcdtext("t2.txt", str);

		// bring chart labels to the front
		writelcdcmd("vis t3,1");
		writelcdcmd("vis t4,1");
	}

	trigindex++;
	if (trigindex >= LCDXPIXELS)
		trigindex = 0;
}

/// PRESSURE //////////////
// display / refresh  the entire pressure chart
void lcd_presscharts() {
	int i, buffi;
	unsigned char str[32];

// refresh the labels as pior page queued commands can clobber them
	setlcdtext("t3.txt", "Pressure");
	setlcdtext("t18.txt", "Pressure");

	sprintf(str, "%d.%03d kPa", pressure, pressfrac >> 2);	// pressure
	setlcdtext("t0.txt", str);

//	writelcdcmd("tsw b2,1");	// enable touch controls
	writelcdcmd("b2.bco=123");		// normal grey
	buffi = pressindex;
	for (i = 0; i < LCDXPIXELS; i++) {
		if (our_currentpage != 3)		// impatient user
			return;
		sprintf(str, "add 2,0,%d", pressvec[buffi]);
		writelcdcmd(str);
		osDelay(15);

		buffi++;
		if (buffi > LCDXPIXELS)
			buffi = 0;
	}
//	writelcdcmd("tsw b2,0");	// disable touch controls
	writelcdcmd("b2.bco=63422");		// normal grey
}

// called at regular intervals to add a point to the display
// update lcd pressure memory,
// the page display may not be showing  (240 pix height)
void lcd_pressplot() {
	volatile int p, pf, val;
	unsigned char str[32];

	p = pressure;
	pf = pressfrac >> 2;		// frac base was in quarters

	p = pressure * 1000 + pf;
	if (p < 93000)
		p = 93000;		// 93 HPa
	if (p > 103000)
		p - 103000;		// 103 HPa

	p = p - 93000;
	val = p / (10000 / 240);		// scale for 240 Y steps on chart

//	printf("pressure for LCD %d", val);

//	val = rand() & 0xFF;  // 0 - 255

	if (val < 0)
		val = 0;
	if (val >= 240)
		val = 239;		// max Y
	pressvec[pressindex] = val;

	if (our_currentpage == 3) {		// if currently displaying on LCD

		sprintf(str, "add 2,0,%d", pressvec[pressindex]);
		writelcdcmd(str);

		sprintf(str, "%d.%03d kPa", pressure, pressfrac >> 2);	// pressure
		setlcdtext("t0.txt", str);

		// bring chart labels to the front
		writelcdcmd("vis t3,1");
	}

	pressindex++;
	if (pressindex >= LCDXPIXELS)
		pressindex = 0;
}

// refresh the entire control page on the lcd
lcd_controls() {
	unsigned char str[48];

	osDelay(100);
	if (our_currentpage == 4) {		// if currently displaying on LCD
		setlcdtext("t0.txt", "Sound");
		setlcdtext("t1.txt", "LEDS");
		setlcdtext("t2.txt", "LCD Brightness");
//	sprintf(str,"%s Control Server IP: %lu.%lu.%lu.%lu", SERVER_DESTINATION, ip & 0xff, (ip & 0xff00) >> 8,
//			(ip & 0xff0000) >> 16, (ip & 0xff000000) >> 24);
		sprintf(str, "Target UDP host: %s\n", udp_target);
		setlcdtext("t3.txt", str);
	}
}

