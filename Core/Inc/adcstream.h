/*
 * adcstream.h
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */

#ifndef ADCSTREAM_H_
#define ADCSTREAM_H_

#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define UDPBUFSIZE (1472)
#define ADCBUFHEAD 16
#define ADCBUFSIZE (UDPBUFSIZE-ADCBUFHEAD)

#define TRIG_THRES 100		// adc trigger level above avg noise (is calculated when running, not fixed)
#define MINTRIGTHRES 2		// minimum trigger threshold
#define PRETRIGCOUNTLIM 768	// number of pretriggers before reducinggain (every 100mS)
#define PRETRIGOFFSET 2		// offset below trigthresh
#define MAXPGANOISE 12		// PGA wont step up if noise > this

typedef uint32_t adcbuffer[ADCBUFSIZE / 2];
typedef uint16_t adc16buffer[ADCBUFSIZE];
typedef uint8_t adc8buffer[ADCBUFSIZE * 2];

extern adcbuffer *adcbuf1;
extern adcbuffer *adcbuf2;
extern adcbuffer *pktbuf;

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern DMA_HandleTypeDef hdma_adc1;

void ADC_MultiModeDMAConvCplt(DMA_HandleTypeDef *hdma);
void ADC_MultiModeDMAError(DMA_HandleTypeDef *hdma);
void ADC_MultiModeDMAHalfConvCplt(DMA_HandleTypeDef *hdma);

void ADC_MultiModeDMAConvM0Cplt(DMA_HandleTypeDef *hdma);
void ADC_MultiModeDMAConvM1Cplt(DMA_HandleTypeDef *hdma);

void startadc(void);

extern unsigned int dmabufno;
extern volatile uint32_t adcbufseq;
#define DETECTOR_EDGE 0		// original 32-sample-window edge detector
#define DETECTOR_STALTA 1	// STA/LTA energy detector (experiment)
#define DETECTOR_DUAL 2		// both: a buffer triggers if either detector fires (experiment)
extern volatile uint8_t detector;
extern volatile uint8_t despike;	// impulse filter: 0 off, 1 median, 2 median+blank (experiment)
extern volatile uint16_t despike_k;
extern volatile uint8_t despike_n;
extern volatile uint32_t stalta_ratio_q4, stalta_ks, stalta_kl, stalta_peak16;
extern volatile uint32_t stalta_margin_q4, stalta_eff_q4, trig_sl_only, trig_edge_only;

void alert_check(const uint16_t *s);	// local-strike alert on a triggered buffer's samples (send task, not the ISR)
void enqueue_sample_isr(void *payload, uint32_t bufseq, BaseType_t *woken);	// udpstream.c, called from the ADC ISR
extern volatile uint32_t isrlat_max, isrskip;	// ADC_Conv_complete() entry latency (cycles) and skipped buffers
extern volatile uint32_t isrcyc_max, isrcyc_sum, isrcyc_n;	// ADC_Conv_complete() cost, DWT cycles (sum in 16-cycle units)

// CPU cycles available per ADC buffer: 216 MHz * 728 samples / 2.7 MSps (measured: 3708 buffers/s)
#define ADCBUF_CYCLES 58240U

extern unsigned int sigprev;		// number of streams let after adc thresh exceeded
volatile extern uint16_t  sigsend;	// flag to tell udp to send sample packet
extern uint16_t trigthresh;	// dynamic trigger threshold

#define ALERT_MV_DEFAULT 100		// local-strike alert level, mV at the PGA input (remote setting "al")
extern volatile uint32_t alert_mv;
extern volatile uint16_t alert_counts;
extern volatile uint8_t alertreq;
extern volatile uint16_t alertpeak;
extern uint32_t globaladcavg;		// adc global average level over 100-200msec
extern uint32_t t2avg;				// cpu clock trim variable

extern uint8_t netup;				// state of LAN up / down
extern uint16_t padding1;			// unused
extern uint8_t rtseconds;			// real time seconds, synced to the gps 1pps pulse
extern uint8_t adcbatchid;	// adc sequence number of a batch of 1..n consecutive triggered buffers
extern int jabbertimeout;			// jabber timeout for spamming detections
extern uint8_t sendendstatus;	// flag from adc to udp to send status

extern uint32_t globaladcnoise;
extern uint16_t pretrigthresh;		// pretrigger threshold
extern int16_t meanwindiff;	// sliding mean of window differences
extern int16_t winmean;	// sliding window mean
extern uint16_t lastmeanwindiff;
extern uint16_t trigthresh;	// trigger threshold

extern uint32_t pretrigcnt;	// counter of pretrigger detections
extern volatile uint16_t sigsuppress;		// countdown timer to suppress trigger

extern volatile ADC_HandleTypeDef *globalhadc;	// dummy

#endif /* ADCSTREAM_H_ */
