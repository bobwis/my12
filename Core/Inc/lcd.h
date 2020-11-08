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
extern void lcd_puts(char * str);


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

extern uint8_t lcdrxbuffer[LCDRXBUFSIZE];

#endif /* INC_LCD_H_ */
