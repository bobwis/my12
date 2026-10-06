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
#define LCD_DIM_DEFAULT 24		// idle backlight level when the LCD has none stored (dims)
#define LCD_DIM_MIN 1			// lowest idle level the slider can set (never black)
#define LCD_SLIDER_MIN 15		// the LCD design's slider bottoms out about here; firmware maps 15..100 -> idle 1..100
#define LCD_BRIGHT_OFFSET 66	// bright level = idle + this, capped at 100 (idle 24 -> 90, idle 34 -> 100)
#define LCD_BRIGHT_MS 10000		// stay bright this long after an alert / touch, then back to idle

#define COMMAND_TIMEOUT_TICKS 20	//  general command timeout


extern UART_HandleTypeDef huart5;


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
int lcd_getid(void);			// get the LCD's ID
int lcd_getsys0(void);		// read LCD's sys0 var
int lcd_waitint(uint32_t ms);	// wait for the reply to a get
#define LCD_GET_MS 500		// how long a get waits for its reply
void lcd_putsys0(uint32_t value);	// write the sys0 variable
void calcLocator(char *dst, double lat, double lon);

extern char nex_model[24];		// the Nextion model number read from the connected display
extern struct tm timeinfo;		// lcd time
extern time_t localepochtime;	// lcd time

extern uint8_t lcdrxbuffer[LCDRXBUFSIZE];

extern volatile int lcd_initflag;			// lcd and or UART needs re-initilising
extern volatile int lcduart_error;			// last uart err
extern int lastday;		// the last date sown on the LCD
extern uint16_t lastsec;	// the last second shown on the lcd
extern volatile uint8_t lcd_currentpage;  // current LCD page
extern int main_init_done;		// flag for main_init finished
extern volatile int lcd_sys0;
extern int txdmadone;
extern volatile int lcd_txblocked;

void lcd_trigplot();
void lcd_gps(void);
void lcd_pressplot();
void lcd_starting();
int writelcdcmd(char *str);
void lcd_showvars(void);
void init_nextion();
void lcd_wake(void);
void lcd_getdims(void);
void lcd_startdl(int filesize);
int lcd_writeblock(uint8_t *buf, int len);


#endif /* INC_LCD_H_ */
