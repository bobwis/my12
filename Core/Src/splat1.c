/*
 * splat1.c
 *
 *  Created on: 26Sep.,2019
 *      Author: bob
 */

/*
 * splat1.c
 *
 *  Created on: 28 Sept.2019
 *      Author: bob
 */

/*
 * splat1.c
 *
 * Splat1 daughter board support
 *
 * Splat1 contains Bandpass filter,  RF switches, PGA Amp, Output Muxes, LEDS
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include "stm32f7xx_hal.h"
#include "version.h"
#include "neo7m.h"
#include "adcstream.h"
#include "main.h"
#include <time.h>

#ifdef SPLAT1

#include "splat1.h"

int psensor = PNONE;		// pressure sensor type
extern SPI_HandleTypeDef hspi2;
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart6;

extern inline int cycinc(int index, int limit);

// user interface
uint16_t ledsenabled = 1, soundenabled = 1;

// globs for pressure sensor results
uint32_t pressure, pressfrac, temperature, tempfrac;

// MPL115A2 (cheapo pressure sensor compensation coeficients)
static double a0, b1, b2, c12;

// SPI based PGA
const uint16_t spicmdchan[] = { 0x4100 };	// set chan reg 0
int16_t pgagain = 0; 	 // initial gain set to 0 (gain 1)
uint16_t boosttrys = 0;  	// N attempts at boost mode inside a time window

// dual mux
uint8_t muxdat[] = { 0x81 };		// sw1A (AMPout -> ADC) sw2D (DAC->Spker)
uint32_t logampmode = 0;	// log amp mode flag

// Programmable gain amplifier

extern const int pgamult[] = { 1, 2, 4, 5, 8, 10, 16, 32, 48, 96 };		// maps from 0..9 gain control to the PGA
extern const uint8_t pgaset[] = { 0, 1, 2, 3, 4, 5, 6, 7, 6, 7 };		// maps from 0..9 gain control to the PGA

//////////////////////////////////////////////
//
// Initialise and test the LEDS by cycling them
//
//////////////////////////////////////////////
void cycleleds(void) {
	const uint16_t pattern[] = {
	LED_D1_Pin,
	LED_D1_Pin | LED_D2_Pin,
	LED_D1_Pin | LED_D2_Pin | LED_D3_Pin,
	LED_D1_Pin | LED_D2_Pin | LED_D3_Pin | LED_D4_Pin,
	LED_D1_Pin | LED_D2_Pin | LED_D3_Pin | LED_D4_Pin | LED_D5_Pin };

	int i;

	for (i = 0; i < 5; i++) {
		HAL_GPIO_WritePin(GPIOD, pattern[i], GPIO_PIN_RESET);
		osDelay(140);
	}
	osDelay(600);
	for (i = 0; i < 5; i++) {
		HAL_GPIO_WritePin(GPIOD, pattern[i], GPIO_PIN_SET);
		osDelay(140);
	}
	osDelay(500);
	for (i = 0; i < 5; i++) {
		HAL_GPIO_WritePin(GPIOD, pattern[i], GPIO_PIN_RESET);
		osDelay(140);
	}
}

//////////////////////////////////////////////
//
// Init RF Switch (low pass filter or bypass input)
//
//////////////////////////////////////////////
void initrfswtch(void) {
	HAL_GPIO_WritePin(GPIOE, LP_FILT_Pin, GPIO_PIN_RESET);	// select RF Switches to LP filter (normal route)
}

//////////////////////////////////////////////
//
// Set the Programmable Gain Amplifier GAIN
//
// added switch for 10dB boost for gain 0x1X (uses channel B input on PGA)
//////////////////////////////////////////////
void setpgagain(int gain) {		// this takes gain 0..9
	uint16_t pgacmd[1];
	HAL_StatusTypeDef stat;

	osDelay(5);
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PGA
	osDelay(5);
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_RESET);	// select the PGA
	osDelay(5);

	pgacmd[0] = 0x4000 | (pgaset[gain]);		// write to gain register
//	printf("setpgagain: gain=%d pgacmd[0]=0x%0x\n",gain,pgacmd[0]);

	if ((stat = HAL_SPI_Transmit(&hspi2, &pgacmd[0], 1, 1000)) != HAL_OK) {	// select gain
		printf("setpgagain: SPI Error1: %d pgacmd[0]=0x%0x\n", stat, pgacmd[0]);
	}
	osDelay(5);
//printf("PGA Gain set to %d\n",pgagain & 7);
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PGA

	osDelay(5);
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_RESET);	// select the PGA
	osDelay(5);

	if (gain > 7) {
		pgacmd[0] = 0x4101;			// write to channel reg select ch1
	} else {
		pgacmd[0] = 0x4100;		// write to channel reg select ch0
	}
//	printf("setpgagain: channel pgacmd[0]=0x%0x\n",pgacmd[0]);

	if ((stat = HAL_SPI_Transmit(&hspi2, &pgacmd[0], 1, 1000)) != HAL_OK) {	// write it out
		printf("setpgagain: SPI Error2: %d\n", stat);
	}

	osDelay(5);
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PG

	pgagain = gain;		// update global gain
}

//////////////////////////////////////////////
//
// Initialise the Programmable Gain Amplifier MCP6S93
//
//////////////////////////////////////////////
int initpga() {
	HAL_StatusTypeDef stat;

	// init spi based single ended PG Amp
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PGA

	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_RESET);	// reset the PGA seq
	osDelay(50);

	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PGA
	osDelay(5);

	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_RESET);	// select the PGA
	if ((stat = HAL_SPI_Transmit(&hspi2, (uint16_t[] ) { 0 }, 1, 1000)) != HAL_OK) {	// nop cmd
		printf("initpga: SPI error 2: %d\n\r", stat);
		return (1);
	}
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_SET);	// deselect the PG
	osDelay(5);

	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin, GPIO_PIN_RESET);	// select the PGA
	osDelay(5);
	if ((stat = HAL_SPI_Transmit(&hspi2, (uint16_t[] ) { 0x4100 }, 1, 1000)) != HAL_OK) {	// set the channel to ch0
		printf("initpga: SPI error 2: %d\n\r", stat);
		return (1);
	}
	setpgagain(0);			// 0 == gain of 1x
	return (0);
}

// bump the pga by one step (up or down)
int bumppga(int i) {
	volatile int gain;

	gain = pgagain;
	if (!((i == 1) || (i == -1))) {
//		printf("bumppga: invalid step %d\n", i);
	}
	if ((pgagain > 9) || (pgagain < 0)) {
		printf("bumppga: invalid gain %d\n", pgagain);
		pgagain = 0;
	}
	if (pgagain < 0)		// safety
		pgagain = 0;
	if (pcb == SPLATBOARD1) {		/// this doesn't have the boost function
		if (pgagain > 7) {
			pgagain = 7;			// reached max gain
		}

		if (!(((gain <= 0) && (i < 0)) || ((gain >= 7) && (i > 0)))) {	// there is room to change
			gain = gain + i;
			setpgagain(gain);
			return (i);
		}
	} else { // not SPLAT1
//	printf("bumppga: req: %d, gain=%d\n", i, gain);
		if (pgagain > 9) {
			pgagain = 9;			// reached max gain
		}
		if (!(((gain <= 0) && (i < 0)) || ((gain >= 9) && (i > 0)))) {	// there is room to change
			gain = gain + i;
			setpgagain(gain);
			return (i);
		}
	}
	return (0);
}

//////////////////////////////////////////////
//
// Initialise the dual mux ADG729
//
//////////////////////////////////////////////
void initdualmux(void) {
	//HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)

	if (HAL_I2C_Master_Transmit(&hi2c1, 0x44 << 1, &muxdat[0], 1, 1000) != HAL_OK) {	// RF dual MUX
		printf("I2C HAL returned error 1\n\r");
	}
}

// MPL115 low precision pressure sensor, uses floating point, crashes!!
HAL_StatusTypeDef getpressure115(void) {
	uint8_t dat, data[8], dataout[8];
	int i;
	HAL_StatusTypeDef result;
	volatile double ffrac, p, t, n;
	uint16_t pr, tr;
	uint8_t testdat[8];

	result = HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) { 0x12, 0x00 }, 2, 1000);
	// CMD Start Conversion
	if (result != HAL_OK) {
		printf("I2C MPL115 HAL returned error 7\n\r");
		return (result);
	}

	osDelay(4);		// conversion time max 3mS

	for (i = 0; i < 4; i++) {
		result = HAL_I2C_Mem_Read(&hi2c1, (0x60 << 1) | 1, i, 1, &data[i], 1, 1000);	// rd pressure and temp regs
		if (result != HAL_OK) {
			printf("I2C MPL115 HAL returned error %d\n\r", result);
			if (i == 3)
				return (result);
		}
	}

#if 0
	data[0]= 0x66; data[1] = 0x80; data[2] = 0x7e; data[3] = 0xc0;// force fixed temp and pressure reading
	printf("Pressure, Temp: ");
	for (i = 0; i < 4; i++) {
		dat = data[i];
		printf(" %hx", dat);
	}
	printf("\n");
#endif

	pr = (data[0] * 256 + data[1]) >> 6;
	tr = (data[2] * 256 + data[3]) >> 6;

//a0 = 2009.75; b1 = -2.37585; b2 = -0.92047; c12 = 0.000790;
//	printf("Raw: Press=0x%hx, %d, Temp=0x%hx %d\n", pr, pr, tr, tr);

	t = tr;
	p = pr;

// Pcomp = a0 + (b1 + c12 x Tadc) x Padc + b2 x Tadc

	p = (a0 + ((b1 + (c12 * t)) * p)) + (b2 * t);

//	printf("Comp: Press = %f\n", p);

	p = (p * ((115.0 - 50.0) / 1023.0)) + 50.0;
//	printf("kPA Press = %f\n", p);

	ffrac = modf(p, &n);
	pressure = (uint32_t) n;
	pressfrac = (uint32_t) (ffrac * 100);  // eg frac 101.03 = frac 3, 101.52 = 52

//	printf("\npressure = %d.%02d  ", pressure, pressfrac);

	t = tr * -0.1706 + 112.27; //C
	temperature = t;
	tempfrac = (t - temperature) * 100;

//	printf("\ntemperature1 = %d.%d  ", temperature, tempfrac);
//	printf("\ntemperature2 = %f  ", t);
	tempfrac = tempfrac * 100;	// now 10,000
#if 0
			{
				volatile uint32_t p1, p2, p3, p4, r1, r2, r3;
				volatile double g1;

				p3 = pressure;
				p4 = pressfrac;
				g1 = p * 4.0;
				printf("g1=%f\n\r",g1);
				p1 = round(g1);	// how many quarters
				p2 = p1 * 1000;
				printf("press=%d, pfrac=%d, p1=0x%x, %d,  p2=0x%x, %d\n\r",p3,p4,p1,p1,p2,p2);
				printf("res press=p2: 0x%x, %d.%d\n\r",p2, (p2 / 4),(p2 % 4));

				p2 = round(p * 4000.0);
//	statuspkt.temppress = ((uint32_t)(t) << 24) | (((tempfrac / 625) << 20) & 0x00F00000) | ((((pressure*1000) << 2) | (pressfrac*1000/25)));
				statuspkt.temppress = ((uint32_t)(t) << 24) | (((tempfrac / 625) << 20) & 0x00F00000) | p2;
			}
//	statuspkt.temppress = ((uint32_t)(t) << 24) | (((tempfrac / 625) << 20) & 0x00F00000) | (uint32_t)(round(p * 4000.0));
			{	volatile uint32_t myt;
				volatile double myf;

				myf = t * 16;
				myt = (uint32_t)(round(myf)) << 20;

				printf("myt 0x%x, %d, t=%f, myf=%f\n\r",myt,myt,t,myf);

			}
#endif
	statuspkt.temppress = (uint32_t) (round(t * 16)) << 20 | (uint32_t) (round(p * 4000.0));
//	printf("statuspkt.temppress temp=%f, press=%f\n\r", (float) ((statuspkt.temppress >> 20)) / 16.0,
//			(float) ((statuspkt.temppress & 0x000FFFFF) / 4000.0));
	return (HAL_OK);
}

// the cheap pressure sensor
HAL_StatusTypeDef initpressure115(void) {
	uint8_t data[8];
	int16_t wdata[4];
	int16_t a0co, b1co, b2co, c12co;
	HAL_StatusTypeDef result;
#if 0
	const uint8_t testcoef[] = {0x3E, 0xCE, 0xb3, 0xF9, 0xC5, 0x17, 0x33, 0xC8};
#endif
	int i;

	for (i = 0; i < 8; i++)
		data[i] = 0x5A;

//	if (HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) { 0x04 }, 1, 1000) != HAL_OK) {	// CMD Read �Coefficient data byte 1 High byte� = 0x04
//		printf("I2C 115 HAL returned error 5\n\r");
//	}

	for (i = 0; i < 8; i++) {
		result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, i + 4, 1, &data[i], 1, 1000);	// rd coeficients reg
		if (result != HAL_OK) {
			printf("Splat1-2 MPL115A2 I2C HAL returned error %d\n\r", result);
			return (result);
		}
	}

#if 0
	for (i = 0; i < 8; i++)
	data[i] = testcoef[i];  // force fixed coeficients

	printf("115 Init: Coef data : ");
	for (i = 0; i < 8; i++) {
		printf(" %x", data[i]);
	}
#endif

	if (data[0] == 0x5a) {
		printf("Splat1-2 MPL115A2 I2C not present?\n\r");
		return (HAL_ERROR);		// expected a changed reading - is device present?
	}

	a0co = (data[0] << 8) | data[1];
	b1co = (data[2] << 8) | data[3];
	b2co = (data[4] << 8) | data[5];
	c12co = (data[6] << 8) | data[7];
	c12co >>= 2;

//a0co = 0x3ECE ; b1co = 0xB3F9; b2co = 0xC517; c12co = 0x33C8;  // force fixed coeficients

//	printf("\na0co=%hx, b1co=%hx, b2co=%hx, c12co=%hx\n", a0co, b1co, b2co, c12co);

	a0 = (double) a0co / 8;
	b1 = (double) b1co / 8192;
	b2 = (double) b2co / 16384;
	c12 = (double) c12co;
	c12 /= (double) 4194304.0;

//	printf("a0=%f, b1=%f, b2=%f, c12=%f\n", a0, b1, b2, c12);
	getpressure115();
	return (HAL_OK);
}

//////////////////////////////////////////////
//
// get the pressure and put in globals Sensor MPL3115A2
//
//////////////////////////////////////////////
HAL_StatusTypeDef getpressure3115(void) {
	uint8_t data[8], dataout[8];
	int i, trys;
	HAL_StatusTypeDef result;
	volatile uint32_t p, t;
//	double ffp, ffn, ffrac;
	volatile uint32_t ifp, ifn, ifrac;

	data[0] = 0x55;
	for (trys = 0; trys < 4; trys++) {
		osDelay(10);
		result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, 0, 1, &data[0], 1, 1000); // rd status reg pressure sense
		if (result != HAL_OK) {
			printf("Splat1-1 I2C HAL returned error %d\n\r", result);
			if (trys == 3)
				return (result);
		} // no HAL error
		if (data[0] & 0x08)
			break;		// data is ready
	} // for
//		printf("Press stat: 0x%0x\n", data[0]);

	for (i = 1; i < 6; i++) {
		result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, i, 1, &data[0], 1, 1000); // rd status reg pressure sense
		if (result != HAL_OK) {
			printf("Splat1-2 I2C HAL returned error %d\n\r", result);
			return (result);
		}
		dataout[i - 1] = data[0];
//				printf("[0x%02x] ", data[0]);
	}  // for

	p = ((dataout[0] << 16) | (dataout[1] << 8) | (dataout[2] & 0xF0)) >> 4;	// 20 bits
	t = ((dataout[3] << 8) | (dataout[4] & 0xF0)) >> 4;

	statuspkt.temppress = t << 20 | p;								// update status packet

//	pressure = p >> 2;  	// these are in Pascals not KPa as required
//	pressfrac = (p & 3) * 25;		// these are in Pascals not KPa as required

#if 0
	ffp = p / 4000.0;
	ffrac = modf(ffp, &ffn);
	pressure = (uint32_t) ffn;
	pressfrac = (uint32_t) (ffrac * 100000);  // eg frac 101.03 = frac 3, 101.52 = 52
#else

#endif

	// convert quarterpascals to kilopascals
	ifn = p / 4000;		// kilopascals
	ifrac = (p % 4000);		// fractions of a kilopascal

//	ifn = ifn >> 2;		// kilopascals
//	ifrac = ifrac >> 2;	// fractions of a kilo pascal

	pressure = ifn;
	pressfrac = ifrac;  // eg frac 101.03 = frac 3, 101.52 = 52

	temperature = t >> 4;
	tempfrac = (t & 0x0F) * 625 * 100;

#if 0
	{
	float m;
	printf("raw pressure = 0x%0x, %d\n",p,p);
	printf("raw temp     = 0x%0x, %d\n",t,t);
	m = p / 4.0;
	printf("real pressure=%f\n\r",m);
	m = t / 16.0;
	printf("real temp    =%f\n\r",m);

	printf("\ntemperature 1 = <%d>.<%d>  ", temperature, tempfrac);
	printf("temp = %d.%d\n", temperature, tempfrac);
	printf("statuspkt.temppress temp=%f, press=%f\n\r", (float) ((statuspkt.temppress >> 20)) / 16.0,
			(float) ((statuspkt.temppress & 0x000FFFFF) / 4000.0));
	}
#endif

	return (result);
}

HAL_StatusTypeDef initpressure3115(void)	// returns 1 on bad MPL3115, 0 on good.
{
	int i, step;
	uint8_t data[8];
	HAL_StatusTypeDef result;

	result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, 0x0c, 1, &data[0], 1, 1000); // rd who am i register
	if (result != HAL_OK) {
		printf("I2C HAL returned error 1\n\r");
		return (result);
	}
	if (data[0] != 0xc4)		// not the default MPL3115 ID
		return (HAL_ERROR);

//HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
#if 0
	if (HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) {0x26, 0x3}, 2, 1000) != HAL_OK) {// set force software reset
		printf("I2C HAL returned error 2a\n\r");
	}

	step = 0;
	for (i = 0; i < 50; i++) {
		result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, 0x26, 1, &data[0], 1, 1000);	// rd control reg 1
		if (result != HAL_OK) {
			printf("Splat1-1 I2C HAL returned error %d\n\r", result);
			return (result);
		}
		if (data[0] & 0x8) {
			step = 1;
			break;
		}
		osDelay(1);
	} // for
//	if (step == 0) {
//		printf("MPL3115A2 Pressure sensor not found\n\r");
//		return (1);
//	}
#endif
	result = HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) { 0x26, 0x38 }, 2, 1000);
	// set pressure mode OSR=128 pressure sense
	if (result != HAL_OK) {
		printf("I2C HAL returned error 2b\n\r");
		return (result);
	}

	result = HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) { 0x13, 0x07 }, 2, 1000); // enbl data flags pressure sense
	if (result != HAL_OK) {
		printf("I2C HAL returned error 3\n\r");
		return (result);
	}
	result = HAL_I2C_Master_Transmit(&hi2c1, 0x60 << 1, (uint8_t[] ) { 0x26, 0x39 }, 2, 1000); // set active pressure sense
	if (result != HAL_OK) {
		printf("I2C HAL returned error 4\n\r");
		return (result);
	}

	osDelay(100);	// allow chip to start up sampling

	result = HAL_I2C_Mem_Read(&hi2c1, 0x60 << 1, 1, 1, &data[0], 1, 1000); // rd msb of press reg to clear ready flags in SR
	if (result != HAL_OK) {
		printf("I2C HAL returned error 5\n\r");
		return (result);
	}

	result = getpressure3115();
	if (result != HAL_OK) {
		printf("MPL3115A2 getpressure failed\n\r");
	}

	return (result);
}

////////////////////////////////////////////////////////////////////////////
//  ESP32 C3-13 WiFi Hybrid
////////////////////////////////////////////////////////////////////////////
char espch, esprxdatabuf[96];
static int esprxindex = 0;
static int espoutindex = 0;

void init_esp() {
	HAL_StatusTypeDef stat;
	int waitforoutput;

	printf("init_esp32_c3_13\n");

	stat = HAL_UART_Receive_DMA(&huart6, &espch, 1);		// set up RX
	if (stat != HAL_OK) {
		printf("init_esp: huart6 error\n");
	}

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);		// put ESP into reset
	osDelay(20);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_SET);		// make sure ESP reset is high (i.e. ESP run)

	for (waitforoutput = 0; waitforoutput < 2000; waitforoutput++) {
		printfromesp();		// try to empty anything in the buffer
		osDelay(1);
	}
	osDelay(200);	// wait for prnt to finish
	printf("\n");
}

uart6_rxdone() {
	HAL_StatusTypeDef stat;
	int i;

	i = esprxindex;
	esprxdatabuf[esprxindex++] = espch;
	if (esprxindex >= sizeof(esprxdatabuf))
		esprxindex = 0;
	if (esprxindex == espoutindex) {	// overrun
		printf("*** ESP RX overrun......\n");
		esprxindex = i;
	}
}

void esp_cmd(unsigned char *buffer) {
	unsigned char txbuf[16];\
	volatile int len;
	HAL_StatusTypeDef stat;

	strcpy(txbuf, buffer);
	strcat(txbuf, "\r\n");
	len = strlen(txbuf);
	printf("Sending ESP: %s\n", txbuf);

	stat = HAL_UART_Transmit_DMA(&huart6, &txbuf[0], len);	// send the command
//	stat = HAL_UART_Transmit(&huart6, &txbuf[0], len, 1000);	// send the command
	if (stat != HAL_OK) {
		printf("esp_cmd: Tx uart6 error 0x%0x\n", stat);
	}
}

void test_esp() {
	static unsigned char getstatus[] = "AT+GMR";
	HAL_StatusTypeDef stat;
	int waitforoutput;

	printf("Testing if ESP responds to command:-\n");
	osDelay(200);
	esp_cmd(getstatus);	// send the command

	for (waitforoutput = 0; waitforoutput < 1000; waitforoutput++) {
		printfromesp();		// try to empty anything in the buffer
		osDelay(1);
	}
}

printfromesp() {
	while (espoutindex != esprxindex) {
		putchar(esprxdatabuf[espoutindex++]);
		if (espoutindex > sizeof(esprxdatabuf))
			espoutindex = 0;
	}
}

////////////////////////////////////////////////////////////////////////////
//  DS2485 1 wire bus controller
////////////////////////////////////////////////////////////////////////////

extern I2C_HandleTypeDef hi2c1;

void init_ds2485(void) {
	uint8_t data[16];
	int i;
	HAL_StatusTypeDef stat;

//HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)

	printf("init_ds2485\n");

	data[0] = 0xAA;		// Read status cmd
	data[1] = 0x01;		// cmd len
	data[2] = 0x01;		// for man id
	if ((stat = HAL_I2C_Master_Transmit(&hi2c1, 0x40 << 1, &data[0], 3, 1000)) != HAL_OK) {	// DS2485
		printf("I2C ds2485 HAL returned error %d\n\r", stat);
	}

	osDelay(10);
	for (i = 0; i < 1; i++) {
		data[i] = 0xA5 + i;
	}

//	HAL_StatusTypeDef HAL_I2C_Mem_Read	(I2C_HandleTypeDef * hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t * pData, uint16_t	Size, uint32_t Timeout)

	for (i = 0; i < 1; i++) {
		stat = HAL_I2C_Master_Receive(&hi2c1, ((0x40 << 1) | 1), &data[0], 4, 1000);	// read ack + len + 1 bytes data
		if (stat != HAL_OK) {
			printf("I2C ds2485 HAL returned error %d\n\r", stat);
		}
	}
#if 0
	printf("init_ds2485: read status manid[0] = 0x%02x\n", data[0]);
	printf("init_ds2485: read status manid[1] = 0x%02x\n", data[1]);
	printf("init_ds2485: read status manid[2] = 0x%02x\n", data[2]);
	printf("init_ds2485: read status manid[3] = 0x%02x\n", data[3]);
#endif
}

// read protection status
void readp_ds2485(int b) {
	uint8_t data[12];
	int i;
	HAL_StatusTypeDef stat;

//HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)

	printf("read protection ds2485\n");

	data[0] = 0xAA;		// Read status cmd
	data[1] = 0x1;		// cmd len
	data[2] = 0x00;		// cmd: for protection status
	if (HAL_I2C_Master_Transmit(&hi2c1, 0x40 << 1, &data[0], 3, 1000) != HAL_OK) {	// DS2485
		printf("I2C ds2485 tx returned error 1\n\r");
	}

	osDelay(30);

//	HAL_StatusTypeDef HAL_I2C_Mem_Read	(I2C_HandleTypeDef * hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t * pData, uint16_t	Size, uint32_t Timeout)
// HAL_StatusTypeDef HAL_I2C_Master_Receive (I2C_HandleTypeDef * hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
	for (i = 0; i < 1; i++) {
		data[i] = 0x5A + i;
	}

	stat = HAL_I2C_Master_Receive(&hi2c1, ((0x40 << 1) | 1), &data[0], b, 1000);	// read ack + len + 6 bytes data
//		stat = HAL_I2C_Mem_Read(&hi2c1, ((0x40 << 1) | 1), 0x55, 1, &data[i], b, 1000);	// read 7 byte
	if (stat != HAL_OK) {
		printf("I2C ds2485 rx  returned error %d\n\r", stat);
	}

	printf("init_ds2485: read status protection= ");
	for (i = 0; i < 8; i++) {
		printf("0x%02x ", data[i]);
	}
	printf("\n");
}

void test_ds2485() {
	int d;

	init_ds2485();
	osDelay(80);
	readp_ds2485(8);
}

//////////////////////////////////////////////
//
// Initialise the splat board
//
//////////////////////////////////////////////
void initsplat(void) {
	int i, j, k;

	cycleleds();
	osDelay(500);
	printf("Initsplat: LED cycle\n");

	if (pcb == SPLATBOARD1) {		// only SPLAT1 has Muxes
		printf("Initsplat: Dual Mux\n\r");
		initdualmux();
		osDelay(500);
	}
	printf("Initsplat: Programmable Gain Amp\n");
	initpga();

	osDelay(500);
	printf("initsplat: Pressure sensor\n\r");
	psensor = PNONE;
	if (initpressure3115() == HAL_OK) {	// non zero result means MPL3115 nogood
		printf("MPL3115A2 pressure sensor present\n\r");
		psensor = MPL3115A2;
		statuspkt.bconf |= (MPL3115A2 << 3);
	} else {
		if (initpressure115() == HAL_OK) {
			printf("MPL115A2 pressure sensor present\n\r");
			psensor = MPL115A2;		// assume MPL115 fitted instead
			statuspkt.bconf |= (MPL115A2 << 3);
		} else {
			printf("NO pressure sensor present\n\r");
		}
	}
	osDelay(500);

	if ((pcb == LIGHTNINGBOARD1) || (pcb == LIGHTNINGBOARD2)) {
		huart6.Init.BaudRate = 115200;
		if (HAL_UART_Init(&huart6) != HAL_OK)		// UART6 is ESP, was GPS on Splat1
		{
			Error_Handler();
		}

		test_ds2485();
		init_esp();
		osDelay(500);
		test_esp();
		osDelay(200);
	}

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);		// inhibit the ESP - put it into reset
}
#endif
