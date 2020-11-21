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
volatile uint8_t lcd_read_page = 0;		// binary LCD page number

volatile uint8_t lcdstatus = 0;		// response code, set to 0xff for not set
volatile uint8_t lcdtouched = 0;		// this gets set to 0xff when an autonomous event or cmd reply happens
volatile uint8_t lcdpevent = 0;		// lcd reported a page. set to 0xff for new report
unsigned int dimtimer = DIMTIME;	// lcd dim imer
unsigned int cmdtimeout = 0;		// max timeout after issueing command
unsigned int rxtimeout = 0;			// receive timeout, reset in lcd_getch
static int txdmadone = 0;			// Tx DMA complete flag (1=done, 0=waiting for complete)
volatile int lcd_initflag = 0;		// lcd and or UART needs re-initilising
volatile int lcduart_error = 0;		// lcd uart last err
int lastday = 0;		// the last date sown on the LCD
uint16_t lastsec = -1;	// the last second shown on the lcd

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

// wait for Tx DMA to complete - timeout if error
// then re-arm the wait flag
// returns -1 on timeout, 0 on okay
int wait_armtx(void) {
	volatile int timeoutcnt;

	timeoutcnt = 0;
	while (timeoutcnt < 50) {
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

	if (timeoutcnt >= 50) {
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

	printf("lcd_uart_init: LCD %d ***\n", baud);

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

void lcd_init() {
	volatile HAL_StatusTypeDef stat;

	const unsigned char lcd_reset[] = { "rest\xff\xff\xff" };
	const unsigned char lcd_fast[] = { "baud=230400\xff\xff\xff" };
	const unsigned char lcd_slow[] = { "baud=9600\xff\xff\xff" };
	int page;

	txdmadone = 0;	// TX is NOT free
#if 0
	stat = HAL_UART_Transmit_DMA(&huart5, lcd_reset, sizeof(lcd_reset)-1);
	if (stat != HAL_OK) {
		printf("lcd_init: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	while (!(txdmadone)) {
		printf("lcd_init: waiting for txdmadone\n");
		osDelay(1);		// wait for comms to complete
	}

	txdmadone = 0;	// TX is NOT free
	osDelay(600);
#endif

	stat = HAL_UART_Transmit_DMA(&huart5, lcd_fast, sizeof(lcd_fast)-1);		// if leading nulls on tx line
	if (stat != HAL_OK) {														// this cmd will be rejected
		printf("lcd_init: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	while (!(txdmadone)) {
		printf("lcd_init: waiting for txdmadone\n");
		osDelay(1);		// wait for comms to complete
	}
	txdmadone = 0;	// TX is NOT free
	osDelay(20);

	stat = HAL_UART_Transmit_DMA(&huart5, lcd_fast, sizeof(lcd_fast)-1);
	if (stat != HAL_OK) {
		printf("lcd_init: Err %d HAL_UART_Transmit_DMA uart5\n", stat);
	}
	while (!(txdmadone)) {
		printf("lcd_init: waiting for txdmadone\n");
		osDelay(1);		// wait for comms to complete
	}
	lastday = 0;		// the last date shown on the LCD - trigger a refresh
	lastsec = -1;	// the last second shown on the lcd
	osDelay(20);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// put a char - non blocking using DMA
inline int lcd_putc(uint8_t ch) {
	HAL_StatusTypeDef stat;

	printf("lcd_putc:%c\n", ch);
	wait_armtx();
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
	static char buffer[64];
	uint32_t reg;

	wait_armtx();			// dma still using buffer
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
// assumes 	lcdstatus = 0xff prior to call
int getlcdack() {
	static unsigned int trys = 0;

	trys++;
	if ((trys > 10) && (lcdstatus == 0xff)) {
		printf("getlcdack: Timeout waiting for LCD response\n\r");
		trys = 0;
		return (-1);
	}
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
	if (dmaindex == 128)  {
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
int lcd_getc() {
	static int lastidx = 0;
	int ch;

	ch = -1;
	if (lastidx != lcdrxoutidx) {		// something there
		ch = lcdrxbuffer[lastidx];
		lastidx = cycinc(lastidx, LCDRXBUFSIZE);
		rxtimeout = 100;
// printf("lcd_getc() got %02x\n", ch);
	}

	return (ch);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// send a var string to the LCD (len max 255)
// terminate with three 0xff's
int writelcdcmd(char *str) {
	char i = 0;
	char pkt[64];  //  __attribute__ ((aligned (16)));

	strcpy(pkt, str);
	strcat(pkt, "\xff\xff\xff");
	lcd_puts(pkt);
	return (0);
}

// write a number digit on the LCD to a num object
void setndig(char *id, uint8_t val) {
	char msg[32];

	sprintf(msg, "%s.val=%1d", id, val);
	lcd_puts(msg);
}

// request to read a lcd named variable (unsigned long) expects numeric value
// return -1 for error
int getlcdnvar(char *id) {
	volatile int result = 0;

	lcd_puts("get ");
	result = writelcdcmd(id);
	if (result == -1) {		// wait for response
		printf("getlcdnvar: Cmd failed\n\r");
	}
	return (result);
}

// send some text to a lcd text object
int setlcdtext(char id[], char string[]) {
	int i;
	char str[64];
	volatile int result = 0;

	sprintf(str, "%s=\"%s\"", id, string);
//	printf("setcdtext: %s\n",str);
	result = writelcdcmd(str);
//	osDelay(50);
	if (result == -1) {
		printf("setlcdtext: Cmd failed\n\r");
	}
	return (result);
}

// send some numbers to a lcd obj.val object, param is binary long number
int setlcdbin(char *id, unsigned long value) {
	char buffer[32];
	volatile int result;

	sprintf(buffer, "%s=%lu", id, value);
	result = writelcdcmd(buffer);
	if (result == -1) {		// wait for response
		printf("setlcdbin: Cmd failed\n\r");
	}
	return (result);
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
				lcd_read_page = getlcdpage();		// associate with its number
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
				printf(" # ");		// found terminator
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

// Try to build an LCD RX Event packet
// returns: 0 nothing found (yet), > good event decodes, -1 error
int lcd_event_process(void) {
	static unsigned char eventbuffer[32];
	volatile int i, result;

	result = isnexpkt(eventbuffer, sizeof(eventbuffer));
	if (result <= 0) {
		return (result);		// 0 = nothing found, -1 = timeout
	} else // got a packet of something
	{

		if ((eventbuffer[0] >= NEX_SINV) && (eventbuffer[0] <= NEX_SLEN)) {	// a status code packet - eg error
			lcdtouched = 0;
			lcdpevent = 0;
			lcdstatus = eventbuffer[0];
			if (eventbuffer[0] != NEX_SOK) {		// returned status from instruction was not OK

				printf("Nextion sent: ");
				switch (eventbuffer[0]) {
				case 0x1A:
					printf("Invalid variable\n");
					break;
				case 0x23:
					printf("Variable name too long\n");
					break;
				case 0x1e:
					printf("Invalid number of parameters\n");
					break;
				case 0x20:
					printf("Invalid Escape Char\n");
					break;
				case 0x1c:
					printf("Attribute assignment failed\n");
					break;
				default:
					printf("Error status 0x%02x\n\r", eventbuffer[0]);
					break;
				}
			}

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
				lcd_read_page = eventbuffer[1];
				printf("lcd_event_process: Got Page event, Page=%d\n", lcd_read_page);
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

void processnex() {		// process Nextion - called at regular intervals
	volatile int result;
	static int delay = 0;

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
		printf("LCD UART_Err %0lx, ", lcduart_error);
	}
	lcduart_error = 0;

	if (lcd_initflag == 1) {		// full init
//		osDelay(100);
		printf("calling lcd_init (9600)\n");
		lcd_uart_init(9600);

		lcd_init();		// has 100 + 100 ms pause
		delay = 100;
		lcd_initflag = 2;		// request wait for lcd to process baud speedup command
//		result = DMARXBUFSIZE - (DMA1_Stream0->NDTR + 1);  // next index position the DMA will fill
//		lcdrxoutidx = result;
		return;
	}

	if (lcd_initflag == 2) {	// wait after giving cmd for lcd to change speed
		osDelay(1);
		if (delay) {
			delay--;
		}
			else
			lcd_initflag = 3;
		return;
	}

	if (lcd_initflag == 3) {	// uart only
		printf("calling lcd_uart_init(230400)\n");
		lcd_uart_init(230400);
//		result = DMARXBUFSIZE - (DMA1_Stream0->NDTR + 1);   // next index position the DMA will fill
//		lcdrxoutidx = result;
		lcd_initflag = 0;		// done
		return;
	}

	lcd_rxdma();		// get any new characters received
	result = lcd_event_process();	// this can trigger the lcd_reinit flag
#if 0
	if (result > 0) {
		printf("processnex: Got something\n");
	}
#endif

	if (dimtimer > 0) {
		dimtimer--;
		if (dimtimer == 0) {
			printf("Auto Dim on\n");
			writelcdcmd("dim=22");
		}
	}
}

