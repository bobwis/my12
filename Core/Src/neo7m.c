/*
 * neo7m.c
 *
 *  Created on: 14Jan.,2018
 *      Author: bob
 */

/*
 * neo7m.c
 *
 * Created: 13/09/2017 5:40:10 PM
 *  Author: bob
 *  Modified from Freqref project 14/01/2018
 *
 * Defines originated from
 * UBX GPS Library
 * Created by Danila Loginov, July 23, 2016
 * https://github.com/1oginov/UBX-GPS-Library
 */
//#include <atmel_start.h>
//#include "timeutils.h"
//#include "usart_basic.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "stm32f7xx_hal.h"
#include "version.h"
#include "neo7m.h"
#include "adcstream.h"
#include <time.h>
#include "lcd.h"

typedef uint8_t byte;

#define PC_SERIAL   Serial
#define PC_BAUDRATE 9600L

extern UART_HandleTypeDef huart6, huart5;
extern void uart5_rxdone();

struct tm now;		// gps time updated every second
time_t epochtime;	// gps time updated every second
time_t last_trig_time = 0;	// last trigger time (to one second)

unsigned int gpsgood = 0;	// GPS comms status
static const unsigned char UBXGPS_HEADER[] = { 0xB5, 0x62, 0x01, 0x07 };
static const unsigned char UBXGPS_HEADER2[] = { 0xB5, 0x62, 0x0a, 0x04 };

unsigned char PACKETstore[128];  //TODO, whats the max size of packet?

unsigned char lastGoodPacket[92];
int IsPacketReady(unsigned char c);

static struct /*UbxGps*/{
	unsigned char offsetClassProperties;
	unsigned char offsetHeaders;
	unsigned char size;
	unsigned char carriagePosition;
	unsigned char checksum[2];

	// Headers (common)
	unsigned char headerClass;
	unsigned char headerId;
	unsigned short headerLength;
} UbxGpsv = { .offsetClassProperties = 8, .offsetHeaders = 4, .carriagePosition = 0, };

#if 0	// old version
// calculate days elapsed since 1 Jan this year
uint32_t calcedays(int thisyear, int thismonth, int thisdom)
{
	volatile int i, j = 0, leap = 0;
	uint8_t mnths[] = {31,28,31,30,31,30,31,31,30,31,30,31};

	if ((thisyear % 100 == 0) && (thisyear % 400 == 0))		// its is leap year
		leap = 1;
	if ((thisyear % 4 == 0) && (thisyear % 100 != 0))		// its a leap year
		leap = 1;

	if (leap)
		mnths[1] = 29;
	else
		mnths[1] = 28;	// just in case we have been running for four years continuously!

	for (i=0; i<thismonth-1; i++)
		j = j + mnths[i];			// find number of days since 1 jan this year to start of this month

	j += thisdom-1;				// add in the days from this month

	return(j);
}


// calculate epoch seconds from 1970 to now using GPS date time fields
// the number of seconds that have elapsed since January 1, 1970 (midnight UTC/GMT), not counting leap seconds
uint32_t calcepoch()
{
	uint32_t tm_sec, tm_min, tm_hour, tm_year, tm_yday;
	volatile uint32_t result;
// should really prevent gps irq while copying the time
	tm_sec = statuspkt.NavPvt.sec;
	tm_min = statuspkt.NavPvt.min;
	tm_hour = statuspkt.NavPvt.hour;
	tm_year = statuspkt.NavPvt.year - 1900;

	tm_yday = calcedays(statuspkt.NavPvt.year,statuspkt.NavPvt.month,statuspkt.NavPvt.day);

	// calculate seconds between 1st Jan of this year and 1970
	result = tm_sec + tm_min*60 + tm_hour*3600 + tm_yday*86400 +
	     (tm_year-70)*31536000 + ((tm_year-69)/4)*86400 -
	     ((tm_year-1)/100)*86400 + ((tm_year+299)/400)*86400;
 //   printf("Epoch1=%ld\n", (long) result);
   return(result);
}
#endif

#if 1	// new version below
struct tm* getgpstime() {
	now.tm_year = statuspkt.NavPvt.year - 1900;
	now.tm_mon = statuspkt.NavPvt.month - 1;           // Month, 0 - jan
	now.tm_mday = statuspkt.NavPvt.day;          // Day of the month
	now.tm_hour = statuspkt.NavPvt.hour;
	now.tm_min = statuspkt.NavPvt.min;
	now.tm_sec = statuspkt.NavPvt.sec;
	now.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
	return (&now);
}

// calculate epoch seconds from 1970 to now using GPS date time fields (32 bit unsigned, not 64 bit time_t as used by the library)
// the number of seconds that have elapsed since January 1, 1970 (midnight UTC/GMT), not counting leap seconds
uint32_t calcepoch32() {

	epochtime = mktime(getgpstime());

//    printf("Epoch=%ld\n", (long) epochtime);
#ifdef LOCALTIME
	return (uint32_t) (epochtime + (time_t) (10 * 60 * 60));		// add ten hours
#else
    return (uint32_t)(epochtime);
#endif

}
#endif
/**
 *Extracts from  UBX GPS Library
 * Created by Danila Loginov, July 2, 2016
 * https://github.com/1oginov/UBX-GPS-Library
 *
 * Sketch, restoring the receiver's default configuration and configure it to get
 * NAV-PVT messages with 100 ms frequency and 115200 baudrate. After the auto-
 * configuration transmits the data from the receiver to the PC and vice versa.
 *
 * U-blox NEO-7M - Arduino Mega
 * VCC - 5V
 * RX - TX3
 * TX - RX3
 * GND - GND
 */

// Default baudrate is determined by the receiver's manufacturer
#define GPS_DEFAULT_BAUDRATE    9600L

// Wanted buadrate can be 9600L (not changed after defaults) or 115200L (changed
// by the changeBaudrate function with prepared message) at the moment

#define GPS_WANTED_BAUDRATE     9600L

// Array of possible baudrates that can be used by receiver, sorted descending
// to prevent excess Serial flush/begin after restoring defaults. You can uncomment
// values that can be used by your receiver before the auto-configuration.
//

const long possibleBaudrates[] = {
//921600L,
//460800L,
//230400L,
		115200L,
		//57600L,
		//38400L,
		//19200L,
		9600L,
//4800L,
		};

// Function, printing packet to the PC's serial in hexadecimal form
void printPacket(byte *msg, byte *packet, byte len) {
	char temp[3];
	static int cnt = 0;

	for (byte i = 0; i < len; i++) {
		if (i % 16 == 0) {
			printf("\n\r%d %s:", cnt++, msg);
		} else {
			printf(" ");
		}
		sprintf(temp, "%.2X", packet[i]);
		printf(temp);
	}
	printf("\n\r");
}

// Function, sending specified packed to the GPS receiver
void sendPacket(byte *packet, byte len) {

	HAL_UART_Transmit(&huart6, packet, len, 100);
#if 0
	printPacket("Tx", packet, len);
#endif
}

// UBLOX revert to defaults
// B5 62 06 09 0D 00 FF FF 00 00 00 00 00 00 FF FF 00 00 03 1B 9A

// Function, sending packet to the receiver to restore default configuration
void restoreDefaults() {
	// CFG-CFG packet
#if 0
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x09, // id
			0x0D, // length
			0x00, // length
			0xFF, // payload
			0xFF, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0xFF, // payload
			0xFF, // payload
			0x00, // payload
			0x00, // payload
			0x17, // payload
			0x2F, // CK_A
			0xAE, // CK_B
			};
#endif
	byte packet[] = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
			0x00, 0x00, 0x03, 0x1B, 0x9A };

	sendPacket(packet, sizeof(packet));
}

void askneo_ver() {
	const byte packet[] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };  // MON_VER get receiver/software version

	printf("Checking for Neo GPS...\n");
	sendPacket(packet, sizeof(packet));
}

// Function, sending set of packets to the receiver to disable NMEA messages
void disableNmea() {
	// Array of two bytes for CFG-MSG packets payload
	byte messages[][2] = { { 0xF0, 0x0A }, { 0xF0, 0x09 }, { 0xF0, 0x00 }, { 0xF0, 0x01 }, { 0xF0, 0x0D },
			{ 0xF0, 0x06 }, { 0xF0, 0x02 }, { 0xF0, 0x07 }, { 0xF0, 0x03 }, { 0xF0, 0x04 }, { 0xF0, 0x0E },
			{ 0xF0, 0x0F }, { 0xF0, 0x05 }, { 0xF0, 0x08 }, { 0xF1, 0x00 }, { 0xF1, 0x01 }, { 0xF1, 0x03 },
			{ 0xF1, 0x04 }, { 0xF1, 0x05 }, { 0xF1, 0x06 }, };

	// CFG-MSG packet buffer
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x01, // id
			0x03, // length
			0x00, // length
			0x00, // payload (first byte from messages array element)
			0x00, // payload (second byte from messages array element)
			0x00, // payload (not changed in the case)
			0x00, // CK_A
			0x00, // CK_B
			};
	byte packetSize = sizeof(packet);

	// Offset to the place where payload starts
	byte payloadOffset = 6;

	// Iterate over the messages array
	for (byte i = 0; i < sizeof(messages) / sizeof(*messages); i++) {
		// Copy two bytes of payload to the packet buffer
		for (byte j = 0; j < sizeof(*messages); j++) {
			packet[payloadOffset + j] = messages[i][j];
		}

		// Set checksum bytes to the null
		packet[packetSize - 2] = 0x00;
		packet[packetSize - 1] = 0x00;

		// Calculate checksum over the packet buffer excluding sync (first two)
		// and checksum chars (last two)
		for (byte j = 0; j < packetSize - 4; j++) {
			packet[packetSize - 2] += packet[2 + j];
			packet[packetSize - 1] += packet[packetSize - 2];
		}

		sendPacket(packet, packetSize);
	}
}

// Function, sending packet to the receiver to change baudrate to 115200
void changeBaudrate() {
	// CFG-PRT packet
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x00, // id
			0x14, // length
			0x00, // length
			0x01, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0xD0, // payload
			0x08, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0xC2, // payload
			0x01, // payload
			0x00, // payload
			0x07, // payload
			0x00, // payload
			0x03, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0x00, // payload
			0xC0, // CK_A
			0x7E, // CK_B
			};

	sendPacket(packet, sizeof(packet));
}

// Function, sending packet to the receiver to change frequency to 100 ms
#define SEC 1
void changeFrequency() {
	// CFG-RATE packet
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x08, // id
			0x06, // length
			0x00, // length
#ifndef SEC
			0x64, // payload
			0x00,// payload
#else
			0xE8, // payload
			0x03, // payload
#endif
			0x01, // payload
			0x00, // payload
			0x01, // payload
			0x00, // payload
#ifndef SEC
			0x7A, // CK_A
			0x12,// CK_B
#else
			0x01, // CK_A
			0x39, // CK_B
#endif
			};
	sendPacket(packet, sizeof(packet));
}

// Function, sending packet to the receiver to disable unnecessary channels
void disableUnnecessaryChannels() {
	// CFG-GNSS packet
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x3E, // id
			0x24, // length
			0x00, // length

			0x00, 0x00, 0x16, 0x04, 0x00, 0x04, 0xFF, 0x00, // payload
			0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x03, 0x00, // payload
			0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x03, 0x00, // payload
			0x00, 0x00, 0x00, 0x01, 0x06, 0x08, 0xFF, 0x00, // payload
			0x00, 0x00, 0x00, 0x01, // payload

			0xA4, // CK_A
			0x25, // CK_B
			};

	sendPacket(packet, sizeof(packet));
}

// Function, sending packet to the receiver to enable NAV-PVT messages
void enableNavPvt() {
	// CFG-MSG packet
	byte packet[] = { 0xB5, // sync char 1
			0x62, // sync char 2
			0x06, // class
			0x01, // id
			0x03, // length
			0x00, // length
			0x01, // payload
			0x07, // payload
			0x01, // payload
			0x13, // CK_A
			0x51, // CK_B
			};

	sendPacket(packet, sizeof(packet));
}

// Function, to set time pulse2 to interval of 1pps
// pulse only running when GPS locked to UTC
void enableNaTP5() {
	// CFG-MSG packet
	byte packet[] = { 0xB5, 0x62, 0x06, 0x31, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x40, 0x42,
			0x0F, 0x00, 0x40, 0x42, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xF7, 0x00, 0x00, 0x00, 0xC9, 0x97 };

	sendPacket(packet, sizeof(packet));
}

//#define GPSOUT  // uncomment if you want to use u-Center

// Read chars from the GPS port
// If there is a PVT packet ready, copy it to the global strct

static uint8_t rxdatabuf[8];

bool isGoodChecksum(int len) {
	unsigned char CK_A = 0;
	unsigned char CK_B = 0;

	for (int i = 2; i < (2 + 4 + len); i++)		// skip sync bytes
			{
		CK_A = CK_A + PACKETstore[i];
		CK_B = CK_B + CK_A;
	}
	return ((CK_A == PACKETstore[len + 6]) && (CK_B == PACKETstore[len + 7]));
}

// start/complete filling in the current packet
int IsPacketReady(unsigned char c) {
	// get current position in packet
	unsigned char p = UbxGpsv.carriagePosition;
	static volatile int len = 0;

	if (p < 4)     // this looks for PVT messages
			{
		// are we starting a packet?
		if ((c == UBXGPS_HEADER[p]) || (c == UBXGPS_HEADER2[p])) {
			PACKETstore[p++] = c;
		} else {
			p = 0;
			len = 0;
		}
	} else  // found a packet header, start filling
	{

		if (p < 6) {
			PACKETstore[p++] = c;
			UbxGpsv.carriagePosition = p;
			return (0);
		}

		if (p == 6) {
//			printf("RR: len=%d\n",len);
			len = PACKETstore[4] + (PACKETstore[5] * 256);
			if (len >= sizeof(PACKETstore)) {	// oversize
				p = 0;
				return 0;
			}
		}

		//if we are here, we've got some of the right packet.  Lets just try getting all the bytes
		if (p < (2 + 4 + len + 2)) {
			PACKETstore[p++] = c;
			if (p == (2 + 4 + len + 2)) {
				p = 0;
				UbxGpsv.carriagePosition = p;
				if (isGoodChecksum(len)) {
					gpsgood = 1;
#if 0
					printPacket("Rx", PACKETstore, (2 + 4 + 84 + 2));
#endif
					return len;
				}
			}
		}
	}
	UbxGpsv.carriagePosition = p;
	return 0;
}

// test stuff
int flag = 0;
uint8_t data;
void rx() {
	HAL_StatusTypeDef stat = HAL_OK;

	printf("Rx Loop\n");
	stat = HAL_UART_Receive_DMA(&huart6, rxdatabuf, 1);
	if ((stat != HAL_OK) && (stat != HAL_BUSY)) {
		printf("Err HAL_UART_Receive_DMA usart6 stat=%d\n", stat);
	}
	while (1) {
		while (flag == 0) {
			osDelay(1);
		}
		flag = 0;
		printf("0x%02x ", data);
	}
}

// init neo7
HAL_StatusTypeDef setupneo() {
	HAL_StatusTypeDef stat;

#if 0
	// Switching receiver's serial to the wanted baudrate
	if (GPS_WANTED_BAUDRATE != GPS_DEFAULT_BAUDRATE) {
		printf("Switching receiver to the wanted baudrate which is ");
		printf("%ld",GPS_WANTED_BAUDRATE);
		printf("...\n\r");

		changeBaudrate();

		fastdelay_ms(100); // Little delay before flushing
	}
#endif

	// set up GPS RX
	/**
	 * @brief Receive an amount of data in DMA mode.
	 * @param huart: UART handle.
	 * @param pData: pointer to data buffer.
	 * @param Size: amount of data to be received.
	 * @note   When the UART parity is enabled (PCE = 1), the received data contain
	 *         the parity bit (MSB position).
	 * @retval HAL status
	 */
	stat = HAL_UART_Receive_DMA(&huart6, rxdatabuf, 1);

	if (stat != HAL_OK) {
		printf("Err HAL_UART_Receive_DMA1 %d usart6\n", stat);
		return (stat);
	}

	// Disabling NMEA messages by sending appropriate packets
//		printf("Disabling NMEA messages...\n\r");
	disableNmea();
	osDelay(500);

	// is there a device - what is it running?
	askneo_ver();
	osDelay(500);

	restoreDefaults();
	osDelay(1500);

	// 	Set reporting frequency to 1 Sec
	printf("NEO: Changing receiving frequency to 1 Sec...\n\r");

	changeFrequency();
	osDelay(500);

	//rx();		// debugging

	// Disabling unnecessary channels like SBAS or QZSS
	printf("NEO: Disabling unnecessary channels...\r\n");
	disableUnnecessaryChannels();
	osDelay(500);

	// Enabling NAV-PVT messages
	printf("NEO: Enabling NAV-PVT messages...\n\r");
	enableNavPvt();
	osDelay(500);

// Enable Time pulse
	enableNaTP5();
	osDelay(500);

	statuspkt.NavPvt.flags = 0;		// make sure gps not showing as locked
	printf("NEO: Auto-configuration is complete\n\r");

#if 0
	// this is not needed
	stat = HAL_UART_Receive_DMA(&huart6, rxdatabuf, 1);		// zzz redundant
	if (stat != HAL_OK) {
		printf("Err HAL_UART_Receive_DMA2 %d usart6\n", stat);
	}
#endif
//		fastdelay_ms(100); // Little delay before flushing
	return (stat);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	const unsigned char offset = 6;
//	unsigned char data;
	HAL_StatusTypeDef stat;
	int len;

//	printf("UART6 RxCpl");
	if (huart->Instance == USART6) { //our UART
		data = rxdatabuf[0];
		flag = 1;

		stat = HAL_UART_Receive_DMA(&huart6, rxdatabuf, 1);
		if (stat != HAL_OK) {
			printf("Err HAL_UART_Receive_DMA3 %d usart6\n", stat);
			__HAL_UART_CLEAR_FEFLAG(huart);
			__HAL_UART_CLEAR_NEFLAG(huart);
			__HAL_UART_CLEAR_OREFLAG(huart);
			__HAL_UART_CLEAR_PEFLAG(huart);
		}

		if ((len = IsPacketReady(data)) > 0) {
			switch (len) {
			case 84:				// NAV Packet

				for (unsigned int i = offset; i < sizeof(statuspkt.NavPvt); i++) {
					*((char*) (&(statuspkt.NavPvt)) + (i - offset)) = PACKETstore[i]; // copy into global struct
				}
				statuspkt.epochsecs = calcepoch32(); // should not be needed if our 1 sec timer was accurate, also dbg desyncs this
//printf("*%d ",statuspkt.epochsecs % 60);
				if (statuspkt.NavPvt.flags & 1) { // locked
					gpslocked = 1;
				} else
					gpslocked = 0;
				break;
			case 100:		// assume MON_VER status response
				printf("NEO Reports versions: sw=%s, hw=%s, ext=%s\n", &PACKETstore[6], &PACKETstore[36], &PACKETstore[46]);
				break;
			default:
				printPacket("***** GPS: Unknown pkt Rx", PACKETstore, len);
				break;
			}
		}
	} else {
		if (huart->Instance == UART5) {
			uart5_rxdone();
		} else
			printf("USART unknown uart int\n");
	}
}

#if 0
// error codes for memory
#define  HAL_UART_ERROR_NONE             ((uint32_t)0x00000000U)    /*!< No error                */
#define  HAL_UART_ERROR_PE               ((uint32_t)0x00000001U)    /*!< Parity error            */
#define  HAL_UART_ERROR_NE               ((uint32_t)0x00000002U)    /*!< Noise error             */
#define  HAL_UART_ERROR_FE               ((uint32_t)0x00000004U)    /*!< Frame error             */
#define  HAL_UART_ERROR_ORE              ((uint32_t)0x00000008U)    /*!< Overrun error           */
#define  HAL_UART_ERROR_DMA              ((uint32_t)0x00000010U)    /*!< DMA transfer error      */
#define  HAL_UART_ERROR_RTO              ((uint32_t)0x00000020U)    /*!< Receiver Timeout error  */
#endif
HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	HAL_StatusTypeDef stat;
	uint8_t ch;
	volatile uint32_t reg;

	// whatever the error try to clear it blindly
	__HAL_UART_CLEAR_FEFLAG(huart);
	__HAL_UART_CLEAR_NEFLAG(huart);
	__HAL_UART_CLEAR_OREFLAG(huart);
	__HAL_UART_CLEAR_PEFLAG(huart);

	if (huart->Instance == USART6) { 		// GPS  UART
		printf("GPS UART_Err Callback %0lx, ", huart->ErrorCode);
		// restart
		stat = HAL_UART_Receive_DMA(&huart6, rxdatabuf, 1);
		if ((stat != HAL_OK) && (stat != HAL_BUSY)) {
			printf("Err HAL_UART_Receive_DMA4 usart6 stat=%d\n", stat);
		}

		return;
	}

	if (huart->Instance == UART5) { 			//LCD UART
		if (!(lcd_initflag)) {

			lcduart_error = huart->ErrorCode;
//		printf("LCD UART_Err Callback %0lx ", huart->ErrorCode);
			if (UART5->ISR & USART_ISR_ORE) // Overrun Error
				UART5->ICR = USART_ICR_ORECF;

			if (UART5->ISR & USART_ISR_NE) // Noise Error
				UART5->ICR = USART_ICR_NCF;

			if (UART5->ISR & USART_ISR_FE) // Framing Error
				UART5->ICR = USART_ICR_FECF;
		}
		return;
	}

#if 0
#define  HAL_UART_ERROR_NONE       ((uint32_t)0x00000000U)    /*!< No error            */
#define  HAL_UART_ERROR_PE         ((uint32_t)0x00000001U)    /*!< Parity error        */
#define  HAL_UART_ERROR_NE         ((uint32_t)0x00000002U)    /*!< Noise error         */
#define  HAL_UART_ERROR_FE         ((uint32_t)0x00000004U)    /*!< frame error         */
#define  HAL_UART_ERROR_ORE        ((uint32_t)0x00000008U)    /*!< Overrun error       */
#define  HAL_UART_ERROR_DMA        ((uint32_t)0x00000010U)    /*!< DMA transfer error  */
#endif
}

void neotime() {
	static int oldsec = 0;

	if (statuspkt.NavPvt.sec != oldsec) {
		oldsec = statuspkt.NavPvt.sec;
		printf("sec: %02d\n", oldsec);
	}
}
