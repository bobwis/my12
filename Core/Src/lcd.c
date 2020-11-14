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
int inlcd_init = 0; 				// recursion flag for init
static int txdmadone = 0;			// Tx DMA complete flag (1=done, 0=waiting for complete)

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
	while (timeoutcnt < 2) {
		if (txdmadone == 1)		// its ready
			break;
//		printf("UART5 Wait Tx %d\n",timeoutcnt);
		timeoutcnt++;
		osDelay(1);		// give up timeslice
	}
	if (timeoutcnt >= 2) {
		printf("UART5 Tx timeout\n");
		return (-1);
	}
	txdmadone = 1;	// re-arm the flag to indicate its okay to Tx
	return (0);
}

// UART 5 Rx DMA complete
void uart5_rxdone() {

	printf("UART5 Rx Complete\n");
}

// Transmit completed callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	volatile uint32_t reg;

	if (huart->Instance == UART5) {
		txdmadone = 1;		// its finished transmission
//	printf("UART5 Tx Complete\n");
#if 0
		while (1) {
		reg = (UART5->ISR);
		if ((reg &  0xD0) == 0xD0) // UART_IT_TXE and TC)
			break;
		else
			printf("HAL_UART_TxCpltCallback, TXE busy\n");
		}
#endif
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void lcd_init() {
	volatile HAL_StatusTypeDef stat;
	const unsigned char lcd_fast[] = { "baud=230400\xff\xff\xff" };
//	const unsigned char lcd_fast[] = { "baud=19200\xff\xff\xff" };
	const unsigned char lcd_slow[] = { "baud=9600\xff\xff\xff" };

	inlcd_init = 1; 				// recursion flag for init
	printf("***Init LCD 9600 ***\n");

	lcdrxoutidx = 0;		// consumer index

	osDelay(200);
	stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// start Rx cyclic DMA
	if (stat != HAL_OK) {
		printf("lcd_init: 0 Err HAL_UART_Receive_DMA uart5 %d\n",stat);
	}

	HAL_UART_DMAStop(&huart5);
//	HAL_UART_Abort(&huart5);
	HAL_UART_DeInit(&huart5);

	huart5.Instance = UART5;
	huart5.Init.BaudRate = 9600;
	huart5.Init.WordLength = UART_WORDLENGTH_8B;
	huart5.Init.StopBits = UART_STOPBITS_1;
	huart5.Init.Parity = UART_PARITY_NONE;
	huart5.Init.Mode = UART_MODE_TX_RX;
	huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart5.Init.OverSampling = UART_OVERSAMPLING_16;
	huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart5) != HAL_OK) {
		printf("lcd_init: Failed to change UART5 to 9600\n");
	}

	osDelay(200);
	stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// start Rx cyclic DMA
	if (stat != HAL_OK) {
		printf("lcd_init: 1 Err HAL_UART_Receive_DMA uart5 %d\n",stat);
	}

	osDelay(200);
	txdmadone = 1;	// TX is free
	printf("Sending sendme 1\n");
	writelcdcmd("sendme"); 	// try to read page
	wait_armtx();

#if 1
	osDelay(200);
	txdmadone = 1;	// TX is free
	lcd_puts(lcd_fast); 	// try to change Nextion baud rate
	wait_armtx();			// wait for tx to complete
	osDelay(500);

	HAL_UART_DeInit(&huart5);
	if (stat != HAL_OK) {
		printf("lcd_init: Err UART_Deinit failed\n");
	}
	osDelay(500);
	printf("***Init LCD 230400 ***\n");

	huart5.Instance = UART5;
	huart5.Init.BaudRate = 230400;
	huart5.Init.WordLength = UART_WORDLENGTH_8B;
	huart5.Init.StopBits = UART_STOPBITS_1;
	huart5.Init.Parity = UART_PARITY_NONE;
	huart5.Init.Mode = UART_MODE_TX_RX;
	huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart5.Init.OverSampling = UART_OVERSAMPLING_16;
	huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart5) != HAL_OK) {
		printf("lcd_init: Failed to change UART5 to 230400\n");
	}
	stat = HAL_UART_Receive_DMA(&huart5, dmarxbuffer, DMARXBUFSIZE);	// start Rx cyclic DMA
	if (stat != HAL_OK) {
		printf("lcd_init: 2 Err HAL_UART_Receive_DMA uart5 %d\n",stat);
	}

	osDelay(500);
	printf("Sending sendme 2\n");
	writelcdcmd("sendme"); 	// try to read page
	wait_armtx();
#endif
	inlcd_init = 0; 				// recursion flag for init
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
		printf("lcd_putc: Err %d HAL_UART_Transmit_DMA uart5\n",stat);
		return (stat);
	}
	return (stat);
}

// put a null terminated string
int lcd_puts(char *str) {
	HAL_StatusTypeDef stat;
	volatile int i;
	static char buffer[64];


#if 0
	while (str[i] != '\0') {
		lcd_putc(str[i++]);
	}
	return 0;
#else
#endif

	i = 0;
	while (str[i] != '\0') {
			buffer[i] = str[i];
			i++;
	}
	buffer[i] = '\0';

	wait_armtx();
	txdmadone = 0;	// TX in progress
//	printf("lcd_puts: len=%d, [%s]\n", i, str);

	stat = HAL_UART_Transmit_DMA(&huart5, buffer, i);
	if (stat != HAL_OK) {
		printf("lcd_puts: Err %d HAL_UART_Transmit_DMA uart5\n",stat);
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
	volatile int count = 0;
	volatile int dmaindex = 0;

	dmaindex = DMARXBUFSIZE - DMA1_Stream0->NDTR;  // next index position the DMA will fill

	while (dmaindex != lcdrxoutidx) {		// dma in index has moved on from lcd rx out index
		lcdrxbuffer[lcdrxoutidx] = dmarxbuffer[lcdrxoutidx];	// copy the next char to lcd rx buffer
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
printf("lcd_getc() got %02x\n", ch);
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
	return(result);
}

// send some text to a lcd text object
int setlcdtext(char id[], char string[]) {
	int i;
	char *str;
	volatile int result = 0;

	str = malloc(64);
	if (str == NULL)
		printf("setlcdtext: malloc failed\n");
	sprintf(str, "%s=\"%s\"", id, string);
//	printf("setcdtext: %s\n",str);
	result = writelcdcmd(str);
	osDelay(50);
	free(str);
	if (result == -1) {
		printf("setlcdtext: Cmd failed\n\r");
	}
	return(result);
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

void processnex() {		// process Nextion - called at regular intervals
	volatile int result;

	lcd_rxdma();		// get any new characters received
	result = lcd_event_process();
	if (result > 0) {
		printf("processnex: Got something\n");
	}

}

