/*
 * adcstream.c
 *
 *  Created on: 22Dec.,2017
 *      Author: bob
 */

#include <stdlib.h>
#include <math.h>
#include "stm32f7xx_hal.h"
#include "neo7m.h"
#include "adcstream.h"
#include "version.h"
#include "main.h"
#include "mydebug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "splat1.h"
#include "mydebug.h"

HAL_StatusTypeDef adcstat;

adcbuffer *adcbuf1;
adcbuffer *adcbuf2;
adcbuffer *pktbuf;

uint32_t t2cap[1];  // dma writes t2 capture value on 1pps edge

unsigned int dmabufno = 0;	// the last filled buffer 0 or 1
volatile uint32_t adcbufseq = 0;	// count of DMA buffer completions (buffer N is safe to read until N+1 completes)
// ADC_Conv_complete() cost in CPU cycles (DWT cycle counter), for the console status line:
// isrcyc_sum/isrcyc_n give the average, isrcyc_max the peak since the status line last cleared it.
// isrcyc_sum is in units of 16 cycles so a status interval of up to ~300 s can't wrap it even at 100% load.
// Budget per buffer is ADCBUF_CYCLES (adcstream.h). The early-return overrun path is not counted.
volatile uint32_t isrcyc_max = 0, isrcyc_sum = 0, isrcyc_n = 0;

// Detector selection (experiment): console Ctrl-T toggles; tuning variables are plain globals so they can be
// changed live over SWD during bench sweeps.
volatile uint8_t detector = DETECTOR_EDGE;
volatile uint8_t despike = 0;				// 0 off, 1 median, 2 median+blank, 3 jump-hold (cheap) - console Ctrl-F cycles
volatile uint16_t despike_k = 60;			// mode 2: |raw - median| above this marks a spike (ADC counts)
volatile uint8_t despike_n = 3;				// mode 2: samples held after a spike (covers its ringing)
static uint16_t despike_hv = 2048;			// mode 2: last good output, held while blanking
static uint8_t despike_hold = 0;
static uint16_t despike_a = 2048, despike_b = 2048;	// last two raw samples of the previous buffer
static uint16_t despike_buf[ADCBUFSIZE >> 1];		// median-filtered copy the edge loop scans when despike is on
volatile uint32_t stalta_ratio_q4 = 64;	// trigger when STA > LTA * ratio/16 (64 = 4.0x)
volatile uint32_t stalta_ks = 6;		// STA time constant 2^ks samples (6 = 64 samples = 24 us at 2.7 MSps)
volatile uint32_t stalta_kl = 7;		// LTA time constant 2^kl quiet buffers (7 = 128 buffers = 35 ms)
volatile uint32_t stalta_peak16 = 0;	// last buffer's peak STA/LTA ratio * 16 (for status / analysis)
static int32_t sl_dc_q8 = 2048 << 8;	// baseline, Q8
static uint32_t sl_sta_acc = 0;		// STA accumulator (= STA << ks)
static uint32_t sl_lta_q8 = 0;		// noise floor: mean |x - dc| per sample, Q8 (0 = not yet seeded)

unsigned int sigprev = 0;	// number of streams let after adc thresh exceeded
volatile uint16_t sigsend = 0;	// flag to tell udp to send sample packet
// Local-strike alert (LCD overlay + tone): only when a triggered buffer's peak deviation from the baseline
// reaches alert_counts (ADC counts, set by the LP task from alert_mv and the current PGA gain).
volatile uint32_t alert_mv = ALERT_MV_DEFAULT;	// alert level in mV at the PGA input; remote setting "al"
volatile uint16_t alert_counts = 1900;		// same level in ADC counts at the current gain (LP task keeps it updated)
volatile uint8_t alertreq = 0;				// set by the ISR, cleared by the LP task when it shows the alert
volatile uint16_t alertpeak = 0;			// peak deviation (counts) of the last alerting buffer
uint32_t globaladcavg = 0;		// adc average over milli-secs
uint32_t globaladcnoise = 0;	// adc noise peaks average over milli-secs
uint16_t pretrigthresh = TRIG_THRES;		// pretrigger threshold
uint16_t trigthresh = TRIG_THRES;		// dynamic trigger offset
uint8_t adcbatchid = 0;	// adc sequence number of a batch of 1..n consecutive triggered buffers
uint8_t sendendstatus = 0; 	// flag to send end of capture status
int jabbertimeout = 0;			// timeout for spamming trigger
uint32_t ledhang = 0;

uint16_t lastmeanwindiff = 0;
int16_t winmean = 0;	// sliding window mean
int16_t meanwindiff = 0;	// sliding mean of window differences
uint32_t pretrigcnt = 0;  // count of pre trigger (sensitive) events
volatile uint16_t sigsuppress = 0;		// count down timer for suppresion of trigger (when changing gain)
volatile uint32_t timestamp;	// ADC DMA complete timestamp
volatile ADC_HandleTypeDef *globalhadc;	// dummy


/* blow are vars used by the ADC capture function */

// rolling window size, could be 32, 64, 128 etc
#define WINSHIFT 5
#define WINSIZE (1<<WINSHIFT)

static uint32_t adcbgbaseacc = 0;		// avg adc level per buffer
static uint32_t samplecnt = 0;
static uint8_t adcbufnum = 0;		// adc sequence number
static int32_t windiff[WINSIZE] = { 0 };		// past window differences from the window mean
static uint16_t lastsamp[WINSIZE] = { 0 };		// last sample saved to calc global mean

static int32_t wdacc = 0;	// window difference accumulator
static int32_t wmeanacc = 0;	// window mean accumulator
static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

/* Stores the handle of the task that will be notified when the
 transmission is complete. */
volatile TaskHandle_t xTaskToNotify = NULL;

// the two vars below should be moved to more appropriate file
uint8_t netup = 0;				// state of LAN up / down
uint8_t rtseconds = 0;			// real time seconds

/**
 * @brief  DMA transfer complete callback.
 * @param  hdma: pointer to a DMA_HandleTypeDef structure that contains
 *                the configuration information for the specified DMA module.
 * @retval None
 */
void ADC_MultiModeDMAConvCplt(DMA_HandleTypeDef *hdma) {
	/* Retrieve ADC handle corresponding to current DMA handle */
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) ((DMA_HandleTypeDef*) hdma)->Parent;
//DMA2->HIFCR |= (uint32_t)0x0000001F;
	/* Update state machine on conversion status if not in error state */
	if (HAL_IS_BIT_CLR(hadc->State, HAL_ADC_STATE_ERROR_INTERNAL | HAL_ADC_STATE_ERROR_DMA)) {
		/* Update ADC state machine */
		SET_BIT(hadc->State, HAL_ADC_STATE_REG_EOC);

		/* Determine whether any further conversion upcoming on group regular   */
		/* by external trigger, continuous mode or scan sequence on going.      */
		/* Note: On STM32F7, there is no independent flag of end of sequence.   */
		/*       The test of scan sequence on going is done either with scan    */
		/*       sequence disabled or with end of conversion flag set to        */
		/*       of end of sequence.                                            */

		if (ADC_IS_SOFTWARE_START_REGULAR(hadc) && (hadc->Init.ContinuousConvMode == DISABLE)
				&& (HAL_IS_BIT_CLR(hadc->Instance->SQR1, ADC_SQR1_L)
						|| HAL_IS_BIT_CLR(hadc->Instance->CR2, ADC_CR2_EOCS))) {
			/* Disable ADC end of single conversion interrupt on group regular */
			/* Note: Overrun interrupt was enabled with EOC interrupt in          */
			/* HAL_ADC_Start_IT(), but is not disabled here because can be used   */
			/* by overrun IRQ process below.                                      */
			__HAL_ADC_DISABLE_IT(hadc, ADC_IT_EOC);

			/* Set ADC state */
			CLEAR_BIT(hadc->State, HAL_ADC_STATE_REG_BUSY);

			if (HAL_IS_BIT_CLR(hadc->State, HAL_ADC_STATE_INJ_BUSY)) {
				SET_BIT(hadc->State, HAL_ADC_STATE_READY);
			}
		}

		/* Conversion complete callback */
		HAL_ADC_ConvCpltCallback(hadc);
	} else {
		/* Call DMA error callback */
		hadc->DMA_Handle->XferErrorCallback(hdma);
	}
}

/**
 * @brief  DMA half transfer complete callback.
 * @param  hdma: pointer to a DMA_HandleTypeDef structure that contains
 *                the configuration information for the specified DMA module.
 * @retval None
 */
void ADC_MultiModeDMAHalfConvCplt(DMA_HandleTypeDef *hdma) {
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) ((DMA_HandleTypeDef*) hdma)->Parent;
	/* Conversion complete callback */

//HAL_ADCEx_MultiModeStop_DMA(hadc);		// freeze
//HAL_ADC_Stop(&hadc1);
//HAL_DMA_Abort(&hadc1);
//DMA2->HIFCR |= (uint32_t)0x0000001F;
//	myhalfcomplete++;
	printf("ADC Half ConvCplt\n");
	HAL_ADC_ConvHalfCpltCallback(hadc);
}

/**
 * @brief  DMA error callback
 * @param  hdma: pointer to a DMA_HandleTypeDef structure that contains
 *                the configuration information for the specified DMA module.
 * @retval None
 */
void ADC_MultiModeDMAError(DMA_HandleTypeDef *hdma) {
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) ((DMA_HandleTypeDef*) hdma)->Parent;
	hadc->State = HAL_ADC_STATE_ERROR_DMA;
	/* Set ADC error code to DMA error */
	hadc->ErrorCode |= HAL_ADC_ERROR_DMA;

	printf("Multi-mode DMA Error\n");
	HAL_ADC_ErrorCallback(hadc);
}

/**
 * @brief  Enables ADC DMA request after last transfer (Multi-ADC mode) and enables ADC peripheral
 *
 * @note   Caution: This function must be used only with the ADC master.
 *
 * @param  hadc: pointer to a ADC_HandleTypeDef structure that contains
 *         the configuration information for the specified ADC.
 * @param  pData:   Pointer to buffer in which transferred from ADC peripheral to memory will be stored.
 * @param  Length:  The length of data to be transferred from ADC peripheral to memory.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_ADCEx_MultiModeStart_DBDMA(ADC_HandleTypeDef *hadc, uint32_t *pData, uint32_t *pData2,
		uint32_t Length) {
	__IO uint32_t counter = 0;

	/* Check the parameters */
	assert_param(IS_FUNCTIONAL_STATE(hadc->Init.ContinuousConvMode));
	assert_param(IS_ADC_EXT_TRIG_EDGE(hadc->Init.ExternalTrigConvEdge));
	assert_param(IS_FUNCTIONAL_STATE(hadc->Init.DMAContinuousRequests));

	/* Process locked */
	__HAL_LOCK(hadc);

	/* Check if ADC peripheral is disabled in order to enable it and wait during
	 Tstab time the ADC's stabilization */
	if ((hadc->Instance->CR2 & ADC_CR2_ADON) != ADC_CR2_ADON) {
		/* Enable the Peripheral */
		__HAL_ADC_ENABLE(hadc);

		/* Delay for temperature sensor stabilization time */
		/* Compute number of CPU cycles to wait for */
		counter = (ADC_STAB_DELAY_US * (SystemCoreClock / 1000000));
		while (counter != 0) {
			counter--;
		}
	}

	/* Start conversion if ADC is effectively enabled */
	if (HAL_IS_BIT_SET(hadc->Instance->CR2, ADC_CR2_ADON)) {
		/* Set ADC state                                                          */
		/* - Clear state bitfield related to regular group conversion results     */
		/* - Set state bitfield related to regular group operation                */
		ADC_STATE_CLR_SET(hadc->State,
				HAL_ADC_STATE_READY | HAL_ADC_STATE_REG_EOC | HAL_ADC_STATE_REG_OVR,
				HAL_ADC_STATE_REG_BUSY);

		/* If conversions on group regular are also triggering group injected,    */
		/* update ADC state.                                                      */
		if (READ_BIT(hadc->Instance->CR1, ADC_CR1_JAUTO) != RESET) {
			ADC_STATE_CLR_SET(hadc->State, HAL_ADC_STATE_INJ_EOC, HAL_ADC_STATE_INJ_BUSY);
		}

		/* State machine update: Check if an injected conversion is ongoing */
		if (HAL_IS_BIT_SET(hadc->State, HAL_ADC_STATE_INJ_BUSY)) {
			/* Reset ADC error code fields related to conversions on group regular */
			CLEAR_BIT(hadc->ErrorCode, (HAL_ADC_ERROR_OVR | HAL_ADC_ERROR_DMA));
		} else {
			/* Reset ADC all error code fields */
			ADC_CLEAR_ERRORCODE(hadc);
		}

		/* Process unlocked */
		/* Unlock before starting ADC conversions: in case of potential           */
		/* interruption, to let the process to ADC IRQ Handler.                   */
		__HAL_UNLOCK(hadc);

		/* Set the DMA transfer complete callback */
		hadc->DMA_Handle->XferCpltCallback = ADC_MultiModeDMAConvM0Cplt;
		hadc->DMA_Handle->XferM1CpltCallback = ADC_MultiModeDMAConvM1Cplt;

		/* Set the DMA half transfer complete callback */
		hadc->DMA_Handle->XferM1HalfCpltCallback = NULL;
		hadc->DMA_Handle->XferHalfCpltCallback = NULL;

		/* Set the DMA error callback */
		hadc->DMA_Handle->XferErrorCallback = ADC_MultiModeDMAError;

		/* Manage ADC and DMA start: ADC overrun interruption, DMA start, ADC     */
		/* start (in case of SW start):                                           */

		/* Clear regular group conversion flag and overrun flag */
		/* (To ensure of no unknown state from potential previous ADC operations) */
		__HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_EOC);

		/* Enable ADC overrun interrupt */
		__HAL_ADC_ENABLE_IT(hadc, ADC_IT_OVR);

		if (hadc->Init.DMAContinuousRequests != DISABLE) {
			/* Enable the selected ADC DMA request after last transfer */
			ADC->CCR |= ADC_CCR_DDS;
		} else {
			/* Disable the selected ADC EOC rising on each regular channel conversion */
			ADC->CCR &= ~ADC_CCR_DDS;
		}

		/* Enable the DMA Stream */
		//HAL_DMA_Start_IT(hadc->DMA_Handle, (uint32_t)&ADC->CDR, (uint32_t)pData, Length);
		HAL_DMAEx_MultiBufferStart_IT(hadc->DMA_Handle, (uint32_t) &ADC->CDR, (uint32_t) pData, (uint32_t) pData2,
				Length);
		/* if no external trigger present enable software conversion of regular channels */
		if ((hadc->Instance->CR2 & ADC_CR2_EXTEN) == RESET) {
			/* Enable the selected ADC software conversion for regular group */
			hadc->Instance->CR2 |= (uint32_t) ADC_CR2_SWSTART;
		}
	}

	/* Return function status */
	return HAL_OK;
}

// this does some quick stats on the adc values and overloads them in the packet time fields
// to help evauate debug adc noise
#if 0
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)	// adc conversion done
{
	register uint32_t hi = 0, lo = 0xffffffff, avg = 0;
	int i;
	adcbuffer *buf;

	if ((myfullcomplete & 1) == 0) {
		buf = pktbuf;
	} else {
		buf = &((*pktbuf)[UDPBUFSIZE / 4]);
	}

	for (i = 8; i < (UDPBUFSIZE / 2); i++) {		// uint16
		hi = (hi > ((uint16_t *) *buf)[i]) ? hi : ((uint16_t *) *buf)[i];
		lo = (lo < ((uint16_t *) *buf)[i]) ? lo : ((uint16_t *) *buf)[i];
		avg = avg + ((uint16_t *) *buf)[i];
	}

	(*buf)[0] = ((uint16_t *) *buf)[4];	// first two adc readings
	(*buf)[1] = hi;
	(*buf)[2] = lo;
	(*buf)[3] = avg / ((UDPBUFSIZE / 2) - 8);

	myfullcomplete++;
//	HAL_ADCEx_MultiModeStop_DMA(hadc);		// freeze
//	HAL_ADC_Stop(&hadc1);
//	HAL_DMA_Abort(&hadc1);
}
#endif

#if 0
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)// adc conversion done (DMA half complete)
{
	volatile uint32_t sr;

	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_1);

	sr = DMA2_Stream4->CR & (1<<19);
//	while (DMA2_Stream4->CR & (1<<19) == sr)		// still doing buffer
//		;		// wait
//	while ((DMA2_Stream4->CR & (1<<19)) == 1)		// still doing buffer
//	while ((DMA2_Stream4->CR & (1<<19)) == 0) // (1<<19))		// still doing buffer

	myhalfcomplete = 1;// second buffer has completed

//	HAL_ADC_ConvCpltCallback(hadc); 	// then process
}
#endif

// ADC DMA conversion complete, called via Timer 5 interrupt as a proxy IRQ
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)	// adc conversion done (DMA complete)
void ADC_Conv_complete(void) {
	register int16_t i;
	register uint8_t j;
	register adcbuffer *buf;
	register adc16buffer *adcbuf16;
	uint16_t thiswindiff;
	uint16_t thissamp = 0;
	uint16_t lastthresh;
	const uint8_t bufno = dmabufno;		// the buffer this call scans (DMA callbacks may advance dmabufno later)
	const uint32_t bufseq = adcbufseq;
	const uint32_t cyc0 = DWT->CYCCNT;	// ISR cost measurement, see isrcyc_*
	uint8_t endnow = 0;					// a batch ended in this buffer: wake the sender for ENDSEQ

//	timestamp = TIM2->CNT;			// real time
//	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET /*PE0*/);	// debug pin
//	gpioeset(GPIO_PIN_0);

	if (bufno == 1) {		// second buffer is ready
		buf = (adcbuffer*) &((*pktbuf)[(UDPBUFSIZE / 4)]);
	} else {
		buf = pktbuf;
	}

	adcbuf16 = (adc16buffer*) &((uint16_t*) *buf)[8];
	(*buf)[3] = timestamp;		// this may not get set until now
	(*buf)[1] = (statuspkt.uid << 16) | (adcbatchid << 8) | (rtseconds << 2) | (adcbufnum++ & 3);// ADC completed packet counter (24 bits)
	(*buf)[2] = statuspkt.epochsecs; // statuspkt.NavPvt.iTOW;

	if (sigsuppress) {
		sigsend = 0;
		sigsuppress--;
	} else {
		if (sigsend) {
			statuspkt.adcudpover++;		// debug adc overruning the udp railgun
			return;						// skip detecting another trigger
		}
		if (detector == DETECTOR_STALTA) {
			// STA/LTA energy detector (experiment). dc = slow baseline (EMA of buffer means), e = |x - dc|,
			// STA = per-sample EMA of e over 2^stalta_ks samples, LTA = EMA over 2^stalta_kl buffers of the
			// buffer-mean of e. Trigger when STA > LTA * stalta_ratio_q4 / 16. Unlike the edge detector (32-sample
			// window high-pass) this responds to energy relative to the noise floor, not to edge sharpness, so
			// smooth distant waveforms are not penalised. Threshold work is per buffer; per sample is ~15 ops.
			const uint32_t ks = stalta_ks;
			const int32_t dc = sl_dc_q8 >> 8;
			const uint32_t lta_q8 = sl_lta_q8;
			const uint32_t thr_acc = ((lta_q8 * stalta_ratio_q4) >> 12) << ks;	// compare against STA accumulator
			uint32_t sta = sl_sta_acc, peak = 0, esum = 0;
			uint32_t bgacc = adcbgbaseacc;
			const uint32_t bg0 = bgacc;
			uint16_t trig = 0;

			for (i = 0; i < (ADCBUFSIZE >> 1); i++) {
				const int32_t x = (*adcbuf16)[i];
				const uint32_t e = (uint32_t) abs(x - dc);
				bgacc += x;
				esum += e;
				sta += e - (sta >> ks);
				if (sta > peak)
					peak = sta;
			}
			if ((lta_q8 != 0) && (peak > thr_acc))	// no triggering until the LTA has been seeded
				trig = 1;

			{
				const uint32_t n = ADCBUFSIZE >> 1;
				const int32_t bufmean_q8 = (int32_t) (((bgacc - bg0) << 8) / n);
				const uint32_t emean_q8 = (esum << 8) / n;
				sl_dc_q8 += (bufmean_q8 - sl_dc_q8) >> 3;	// baseline follows over ~8 buffers (~2 ms)
				// Learn the noise floor from every buffer, 4x slower on triggered ones. (Learning only from quiet
				// buffers froze the LTA when the noise stepped up - e.g. a gain change - so every buffer triggered
				// and it never recovered: bench sweep went to ~46 triggers/s.)
				if (lta_q8 == 0)
					sl_lta_q8 = emean_q8;
				else
					sl_lta_q8 += ((int32_t) emean_q8 - (int32_t) lta_q8) >> (trig ? (stalta_kl + 2) : stalta_kl);
				stalta_peak16 = (lta_q8 != 0) ? (uint32_t) (((uint64_t) peak << 12) / ((uint64_t) lta_q8 << ks)) : 0;	// peak STA/LTA * 16
			}
			sl_sta_acc = sta;
			adcbgbaseacc = bgacc;
			meanwindiff = lastmeanwindiff = sl_lta_q8 >> 8;	// noise floor for globaladcnoise / gain AGC / status
			if (trig)
				sigsend = 1;
		} else {
			// Original edge detector.
			// Hot-loop state in locals so it stays in registers; the globals are read once and written back
			// once per buffer. Same arithmetic and integer types as before. trigcomp is volatile (set by the
			// server) and in SRAM1, and was being re-read twice per sample.
			const uint16_t thr = trigthresh + trigcomp;		// trigger: rise > trigthresh + trigcomp
			const int32_t pthr = pretrigthresh + trigcomp;	// near miss: rise > pretrigthresh + trigcomp
			int32_t wma = wmeanacc, wda = wdacc;
			uint32_t bgacc = adcbgbaseacc;
			uint16_t lastmwd = lastmeanwindiff;
			int16_t wmean = winmean, mwd = meanwindiff;
			uint32_t nearmiss = 0;
			uint16_t trig = 0;

			// Impulse filter (experiment): a separate pre-pass so the hot loop below keeps its register allocation.
			// 3-sample median = each sample clamped between the previous two: removes single-sample spikes
			// (switching-converter interference, detector 13) and barely changes a stroke, which rises over many samples.
			const uint16_t *src = &(*adcbuf16)[0];
			if (despike) {
				uint16_t a = despike_a, b = despike_b;
				if (despike == 1) {
					for (i = 0; i < (ADCBUFSIZE >> 1); i++) {
						const uint16_t r = src[i];
						const uint16_t lo = (a < b) ? a : b, hi = (a < b) ? b : a;
						despike_buf[i] = (r < lo) ? lo : ((r > hi) ? hi : r);
						a = b;
						b = r;
					}
				} else if (despike == 3) {
					// Mode 3 (cheap compromise): a jump of more than despike_k from the last accepted value starts a hold of
					// despike_n samples at that value. At the end of the hold the current sample is accepted, so a single-sample
					// spike and most of its ringing disappear, while a real step (stroke) comes through n samples late.
					const int32_t k = despike_k;
					const uint32_t n = despike_n;
					int32_t hv = despike_hv;
					uint32_t hold = despike_hold;
					for (i = 0; i < (ADCBUFSIZE >> 1); i++) {
						const int32_t r = src[i];
						if (hold) {
							despike_buf[i] = hv;
							if (--hold == 0)
								hv = r;				// accept the level the signal has settled to
						} else if ((uint32_t) (r - hv + k) > (uint32_t) (2 * k)) {	// |r - hv| > k, one compare
							despike_buf[i] = hv;
							hold = n;
						} else {
							despike_buf[i] = r;
							hv = r;
						}
					}
					despike_hv = hv;
					despike_hold = hold;
					a = src[(ADCBUFSIZE >> 1) - 2];
					b = src[(ADCBUFSIZE >> 1) - 1];
				} else {
					// Mode 2: as mode 1, and when a sample stands out from its median by more than despike_k, hold the
					// last good value for despike_n more samples so the spike's ringing doesn't reach the edge detector.
					const int32_t k = despike_k;
					const uint8_t n = despike_n;
					uint16_t hv = despike_hv;
					uint8_t hold = despike_hold;
					for (i = 0; i < (ADCBUFSIZE >> 1); i++) {
						const uint16_t r = src[i];
						const uint16_t lo = (a < b) ? a : b, hi = (a < b) ? b : a;
						const uint16_t m = (r < lo) ? lo : ((r > hi) ? hi : r);
						if ((int32_t) r - m > k || (int32_t) m - r > k)
							hold = n + 1;
						if (hold) {
							hold--;
							despike_buf[i] = hv;
						} else {
							despike_buf[i] = m;
							hv = m;
						}
						a = b;
						b = r;
					}
					despike_hv = hv;
					despike_hold = hold;
				}
				despike_a = a;
				despike_b = b;
				src = despike_buf;
			} else {
				despike_a = src[(ADCBUFSIZE >> 1) - 2];
				despike_b = src[(ADCBUFSIZE >> 1) - 1];
			}

			for (i = 0; i < (ADCBUFSIZE >> 1); i++) {	// 2 // scan the buffer content
				j = i & (WINSIZE - 1);			// j = the index of oldest saved sample
				thissamp = src[i];

				bgacc += thissamp; // accumulator used to find avg level of signal over long time (for base)

				wma = wma + thissamp - lastsamp[j];		// window mean acc
				wmean = wma >> (WINSHIFT);		// divide to find the new window mean
				lastsamp[j] = thissamp;			// save last samples

				thiswindiff = abs(thissamp - wmean);			// find difference from window mean
				wda = wda - windiff[j] + thiswindiff; // difference accumulator for WINSIZE samples

				mwd = wda >> (WINSHIFT); // sliding mean of window differences (used for globalnoise)
				windiff[j] = mwd;	// store latest window mean of differences

				lastthresh = lastmwd + thr;		// trigger threshold compensation provided by the server

				if (abs(mwd) > (lastthresh)) { // if new mean diff > last mean diff + trig offset
					trig = 1; // the real trigger
				} else if (abs(mwd) > (lastmwd + pthr)) {
					// near miss: above the pre-trigger level (pretrigthresh = trigthresh - 2, set in the LP task)
					// but not the trigger. Was "|m| + pretrigthresh + trigcomp > lastthresh", which reduces to a
					// fixed "rise > 2" independent of trigthresh, so raising the threshold could never reduce
					// the near-miss count and the AGC pinned trigthresh at its clamp.
					nearmiss++;
				}
				lastmwd = abs(mwd);
			} // end for i

			wmeanacc = wma;
			wdacc = wda;
			adcbgbaseacc = bgacc;
			lastmeanwindiff = lastmwd;
			winmean = wmean;
			meanwindiff = mwd;
			pretrigcnt += nearmiss;
			if (trig)
				sigsend = 1;
			if (sigsend) {
				trigthresh += 2;
				pretrigcnt += 201;
			}
		}
	}
//sigsend = ((samplecnt & 0x1ff) == 0) ? 1 : 0;			// for testing create continual spaced triggers
	if (sigsend) {
#ifndef SPLAT1
			HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_SET);	// blue led
#endif
		if (sigprev == 0) {		// no trigger last time, so this is a new event
			++adcbatchid; // start a new adc batch number
//			(*buf)[1] = (*buf)[1] & 0xffff00ff | (adcbatchid << 8);	//update batch number in sample pkt (redundant see 331)
		}
		sigprev = 1;	// remember this trigger for next packet
		enqueue_sample_isr(buf, bufseq, &xHigherPriorityTaskWoken);	// queue this buffer now, before the DMA can refill it
		{	// alert check, triggered buffers only (~2k cycles): peak deviation from the long-term baseline
			const uint16_t *s = &(*adcbuf16)[0];
			uint16_t lo = 4095, hi = 0;
			for (i = 0; i < (ADCBUFSIZE >> 1); i++) {
				const uint16_t v = s[i] & 0x0fff;
				if (v < lo)
					lo = v;
				if (v > hi)
					hi = v;
			}
			const int32_t base = (int32_t) globaladcavg;
			const int32_t up = hi - base, dn = base - lo;
			const uint16_t pk = (uint16_t) ((up > dn) ? up : dn);
			if (pk >= alert_counts) {
				alertpeak = pk;
				alertreq = 1;
			}
		}
		ledhang = 15;		// 15 x 10ms in Idle proc
		statuspkt.trigcount++;	//  no of triggered packets detected

	} else {			// no trigger
		if (sigprev) {		// but there was a trigger the last packet
			sendendstatus = 1;		// so tell udpstream to send the end of sequence status packet
			endnow = 1;				// and wake it now: it used to wait for the NEXT batch's first trigger,
									// so ENDSEQ arrived late and carried the next batch's id
		}
		sigprev = 0;

#ifndef SPLAT1
			HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);	// blue led
#endif
	}

	if (++samplecnt == 2048) {		// 2k adc bufffers sampled approx 0.5 sec
		globaladcavg = (adcbgbaseacc / (ADCBUFSIZE / 2)) >> 11;
		adcbgbaseacc = 0;
		samplecnt = 0;
	}

	if (xTaskToNotify == NULL) {
		printf("Notify task null\n");
	} else if (sigsend || endnow) {
		vTaskNotifyGiveFromISR(xTaskToNotify, &xHigherPriorityTaskWoken);
		// signal the detection processing of this packet to the back-end
		/* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
		 should be performed to ensure the interrupt returns directly to the highest
		 priority task.  The macro used for this purpose is dependent on the port in
		 use and may be called portEND_SWITCHING_ISR(). */
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
//	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_RESET /*PE0*/);	// debug pin
//	gpioeclr(GPIO_PIN_0 | GPIO_PIN_12);

	{	// ISR cost (full-scan path only)
		uint32_t c = DWT->CYCCNT - cyc0;
		if (c > isrcyc_max)
			isrcyc_max = c;
		isrcyc_sum += (c + 8) >> 4;		// units of 16 cycles: whole cycles wrapped in ~50 s at 38% load
		isrcyc_n++;
	}
}

// handle the highest priority interrupt to capture the true DMA conversion complete time (below RTOSOS level)
extern TIM_HandleTypeDef htim5;
void ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)	// adc conversion done (DMA complete)
{
	timestamp = TIM2->CNT;			// real time

#if 0
	{
		static uint32_t last = 0;
		static int index = 0;
		static uint32_t samples[32] = { 0 };

		if (timestamp > samples[index]) {			// debug looking at latency jitter
			samples[index++] = timestamp - last;
			if (index > 31)
				index = 0;
		} else {
			index = 0;
			samples[0] = 0;
			last = 0;
		}

		last = timestamp;
	}
#endif
#if 0	// debug looking gofr correlation between GPS time in seconds and PPS puls
	static unsigned char gpssec, gpsnow;
	volatile static unsigned int i = 0;
	volatile static uint32_t avg = 0, min = -1, max = 0, sum = 0;

	if ((gpsnow = statuspkt.NavPvt.sec) != gpssec) {		// seonds has changed
		i++;
		if (i > (256 + 60)) {
			avg = sum >> 8;
			sum = 0;
			i = 0;
		} else {
			if (i >= 60) {
				if (timestamp > max)
					max = timestamp;
				if (timestamp < min)
					min = timestamp;
				sum += timestamp;
				gpssec = gpsnow;
			}
		}
	}
#endif
	TIM5->DIER = 0x01;
	TIM5->CR1 = 0x19;// restart timer to generate a follow-on interrupt for the *real* dma conversion complete processing at IRQ level 5

//	HAL_TIM_Base_Start_IT(&htim5);
}

// these two are the real DMA Conversion complete interrupts
void ADC_MultiModeDMAConvM0Cplt(DMA_HandleTypeDef *hdma) {
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) hdma->Parent;
	dmabufno = 0;
	adcbufseq++;
	ADC_ConvCpltCallback(hadc);
}

void ADC_MultiModeDMAConvM1Cplt(DMA_HandleTypeDef *hdma) {
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) hdma->Parent;

	dmabufno = 1;
	adcbufseq++;
	ADC_ConvCpltCallback(hadc);
}

void startadc() {
	int i;
//	uint16_t *adcbufdum1, *adcbufdum2;		// debug
//	adcbufdum1 = pvPortMalloc(UDPBUFSIZE);	//  dummy buffer
//	adcbufdum2 = pvPortMalloc(UDPBUFSIZE);	//  dummy buffer

	statuspkt.clktrim = CCLK;
	statuspkt.adcpktssent = 0;

	printf("Starting ADC DMA\n");
	// enable the DWT cycle counter for ISR cost measurement (isrcyc_*). The Cortex-M7 DWT has a software
	// lock: without the LAR unlock the CTRL write is silently ignored (seen on the bench: CYCCNT stuck at 0).
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->LAR = 0xC5ACCE55;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	osDelay(100);
// ADC stream DMA double buffer (two buffers concatenated). Static rather than pvPortMalloc so it
// lands in zero-wait DTCM (start of .bss) instead of the FreeRTOS heap, which now sits in SRAM1:
// with the D-cache off every one of the 728 sample loads per buffer in ADC_Conv_complete() was an
// uncached SRAM1 access. DMA2 can write DTCM on the F7.
	static uint32_t adcdmabuf[(UDPBUFSIZE * 2) / 4] __attribute__((aligned(32)));
	pktbuf = (adcbuffer*) adcdmabuf;
	if (((uint32_t) pktbuf) >= 0x20020000U) {
		printf("******** ADC DMA buffer not in DTCM (0x%08lx) *********\n", (unsigned long) pktbuf);
	}

//	printf("(&(*pktbuf)[0])=0x%x ", &((*pktbuf)[0]));
//	printf("(&(*pktbuf)[UDPBUFSIZE / 4])=0x%x\n", &((*pktbuf)[UDPBUFSIZE / 4]));

	for (i = 0; i < UDPBUFSIZE / 4; i++) {	// fill buffers, 4 bytes at a time
		(*pktbuf)[i] = 0x55555555;
	}
	for (i = UDPBUFSIZE / 4; i < UDPBUFSIZE / 2; i++) {	// fill buffers, 4 bytes at a time
		(*pktbuf)[i] = 0xaaaaaaaa;
	}

	adcbuf1 = (adcbuffer*) &(*pktbuf)[ADCBUFHEAD / 4];	// leave room in start of first buffer
	adcbuf2 = (adcbuffer*) &(*pktbuf)[(ADCBUFHEAD / 4) + (ADCBUFSIZE / 4) + (ADCBUFHEAD / 4)];	// leave room in start of 2nd buffer

	adcstat = HAL_ADCEx_MultiModeStart_DBDMA(&hadc1, (uint32_t*) adcbuf1, (uint32_t*) adcbuf2, (ADCBUFSIZE / 2));	// len in 16bit words

//	adcstat = HAL_ADCEx_MultiModeStart_DBDMA(&hadc1, adcbufdum1, adcbufdum2, (ADCBUFSIZE / 4));		// DEBUG
//		printf("ADC_MM_Start returned %u\r\n", adcstat);

	if (HAL_ADC_Start(&hadc3) != HAL_OK)
		printf("ADC3 failed start\r\n");
	if (HAL_ADC_Start(&hadc2) != HAL_OK)
		printf("ADC2 failed start\r\n");
	if (HAL_ADC_Start(&hadc1) != HAL_OK)
		printf("ADC1 failed start\r\n");
#if 0
		while (1) {
//			HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);	 // red led off
			HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_SET);// blue led on
			while (lastbuf == myfullcomplete)// wait for buf1 to fill
			vTaskDelay(0);// wait
//timer			((uint16_t *)*pktbuf)[UDPBUFSIZE/2] = i; // 0xaaaa5555;
			lastbuf = myfullcomplete;
			HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);// blue led off
//			HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_SET);		// red led on

			//	myhexDump ("INITBUFF1---------------------------------------", *adcbuf1, ADCBUFLEN*2);
		}
#endif
}
