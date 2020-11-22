/*
 * lcd.h
 *
 *  Created on: Nov 3, 2020
 *      Author: bob
 */

#ifndef INC_LCD_H_
#define INC_LCD_H_

#define LCDRXBUFSIZE (1<<7)		// LCD serial Rx buffer (power of two)
#define DMARXBUFSIZE (1<<7)		// DMA serial Rx buffer (power of two)
#define DIMTIME 60000		// Dim the LCD after activity time

#define COMMAND_TIMEOUT_TICKS 20	//  general command timeout


extern UART_HandleTypeDef huart5;


// put a char
extern inline int lcd_putc(uint8_t ch);


// put a null terminated string
extern int lcd_puts(char * str);


// Transmit completed callback
extern void HAL_UART_TxCpltCallback (UART_HandleTypeDef *huart);


// get a char
extern int lcd_getc();

// UART 5 Rx complete
extern void uart5_rxdone();

// init LCD Rx DMA + other things
extern void  lcd_init();

// tick call to process nextion IO
extern void processnex(void);

// Application specific stuff
void lcd_time(void);		// send the time
void lcd_date(void);		// send the date


extern struct tm timeinfo;		// lcd time
extern time_t localepochtime;	// lcd time

extern uint8_t lcdrxbuffer[LCDRXBUFSIZE];

extern volatile int lcd_initflag;			// lcd and or UART needs re-initilising
extern volatile int lcduart_error;			// last uart err
extern int lastday;		// the last date sown on the LCD
extern uint16_t lastsec;	// the last second shown on the lcd
extern volatile uint8_t lcd_currentpage;  // current LCD page

#endif /* INC_LCD_H_ */
