/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "lwip.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "netif.h"
#include "adcstream.h"
#include "neo7m.h"
#include "mydebug.h"
#include "ip4.h"
#include "lwip.h"
#include "lwipopts.h"
#include "lwip/prot/dhcp.h"

#include "udpstream.h"
#include "version.h"
#include "www.h"
#include "dhcp.h"
#include "splat1.h"
//#define netif_dhcp_data(netif) ((struct dhcp*)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP))

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
const unsigned char phaser_wav[] = /* { 128, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,128,128,0,255,128 }; */
{ 114, 89, 141, 101, 114, 127, 102, 127, 89, 127, 128, 113, 141, 101, 128, 113, 128, 127, 128, 127, 153, 113, 114, 140,
		141, 140, 153, 113, 114, 127, 128, 127, 153, 140, 141, 152, 128, 113, 128, 127, 141, 141, 128, 127, 128, 101,
		128, 152, 140, 114, 101, 128, 127, 128, 113, 114, 128, 114, 128, 127, 102, 113, 102, 127, 127, 114, 113, 102,
		113, 114, 141, 102, 89, 101, 114, 89, 102, 102, 88, 141, 114, 128, 128, 114, 102, 113, 114, 127, 141, 127, 114,
		152, 141, 128, 114, 141, 141, 140, 128, 127, 128, 165, 141, 140, 102, 88, 102, 101, 89, 152, 128, 113, 102, 102,
		113, 102, 127, 114, 128, 113, 141, 127, 128, 113, 140, 166, 127, 114, 127, 141, 113, 128, 127, 114, 140, 153,
		102, 101, 102, 141, 127, 89, 88, 114, 101, 141, 113, 141, 152, 114, 113, 128, 152, 102, 101, 114, 141, 113, 89,
		101, 128, 101, 128, 140, 76, 113, 141, 89, 101, 114, 114, 140, 89, 101, 114, 114, 127, 141, 101, 114, 141, 152,
		128, 101, 152, 166, 127, 141, 113, 128, 152, 76, 140, 114, 141, 102, 128, 128, 113, 114, 152, 128, 113, 153,
		128, 88, 102, 140, 64, 88, 141, 88, 89, 101, 166, 102, 88, 179, 49, 114, 140, 64, 141, 113, 152, 102, 140, 141,
		101, 140, 166, 101, 153, 128, 88, 128, 127, 102, 128, 113, 102, 192, 128, 114, 140, 192, 114, 127, 141, 101,
		141, 88, 166, 75, 140, 166, 49, 166, 64, 127, 114, 76, 165, 102, 140, 102, 113, 166, 63, 179, 76, 179, 49, 153,
		75, 141, 102, 140, 114, 101, 166, 63, 191, 76, 165, 114, 178, 102, 128, 166, 114, 153, 76, 192, 49, 166, 37,
		178, 38, 152, 88, 102, 127, 76, 101, 76, 153, 64, 152, 64, 140, 76, 152, 102, 152, 76, 178, 76, 178, 89, 165,
		89, 152, 89, 204, 64, 191, 141, 114, 127, 153, 101, 179, 75, 205, 89, 141, 114, 114, 152, 64, 140, 141, 102,
		191, 64, 191, 89, 102, 152, 50, 204, 50, 191, 50, 101, 166, 63, 179, 24, 179, 88, 114, 152, 102, 140, 64, 128,
		64, 179, 75, 76, 179, 25, 205, 76, 101, 141, 88, 192, 89, 89, 178, 38, 165, 114, 113, 179, 11, 179, 166, 24,
		166, 140, 64, 205, 88, 102, 140, 64, 141, 178, 89, 101, 166, 37, 179, 140, 37, 205, 191, 38, 178, 153, 63, 205,
		75, 114, 230, 64, 114, 216, 49, 76, 204, 50, 88, 217, 89, 113, 205, 75, 114, 165, 76, 127, 153, 37, 141, 216,
		50, 128, 191, 50, 128, 179, 89, 101, 179, 88, 113, 205, 88, 89, 204, 102, 49, 192, 140, 75, 141, 165, 102, 140,
		192, 89, 88, 179, 89, 63, 166, 102, 64, 166, 140, 76, 140, 141, 140, 192, 88, 37, 166, 191, 50, 49, 179, 166,
		24, 64, 191, 128, 49, 152, 179, 37, 50, 178, 166, 50, 50, 178, 179, 63, 50, 166, 165, 25, 63, 205, 152, 50, 113,
		192, 152, 25, 49, 179, 152, 25, 64, 178, 166, 63, 64, 204, 192, 37, 25, 192, 191, 38, 25, 165, 230, 88, 12, 127,
		216, 128, 24, 102, 229, 192, 75, 64, 178, 205, 75, 25, 166, 255, 140, 12, 75, 205, 216, 76, 38, 127, 192, 127,
		64, 128, 216, 179, 64, 63, 179, 204, 102, 50, 140, 243, 165, 25, 37, 153, 242, 178, 76, 49, 114, 165, 128, 63,
		64, 141, 165, 102, 49, 114, 216, 153, 37, 38, 165, 243, 152, 38, 24, 140, 217, 152, 25, 25, 127, 192, 128, 25,
		25, 140, 230, 178, 50, 25, 101, 217, 204, 102, 38, 75, 179, 243, 178, 64, 24, 128, 242, 217, 101, 12, 50, 166,
		217, 166, 75, 38, 88, 153, 152, 101, 38, 50, 140, 192, 127, 24, 0, 75, 192, 243, 178, 76, 0, 50, 165, 217, 165,
		63, 12, 63, 166, 229, 179, 75, 25, 89, 191, 243, 204, 89, 25, 50, 141, 230, 229, 153, 63, 38, 101, 205, 243,
		191, 89, 11, 38, 127, 230, 229, 153, 38, 0, 50, 152, 230, 204, 114, 37, 50, 113, 179, 192, 128, 38, 12, 75, 166,
		204, 179, 88, 12, 37, 128, 216, 230, 179, 113, 64, 63, 114, 152, 166, 127, 89, 63, 64, 101, 153, 165, 127, 89,
		63, 89, 101, 141, 153, 140, 102, 89, 113, 141, 166, 152, 141, 113, 76, 75, 114, 165, 192, 165, 102, 38, 24, 76,
		165, 217, 216, 153, 75, 38, 49, 114, 191, 217, 192, 140, 89, 75, 114, 179, 205, 166, 114, 63, 38, 63, 128, 191,
		205, 165, 101, 50, 24, 64, 140, 205, 205, 140, 64, 12, 25, 89, 165, 230, 229, 192, 113, 50, 24, 38, 101, 179,
		217, 204, 153, 88, 38, 37, 76, 153, 205, 216, 179, 101, 38, 25, 49, 102, 153, 191, 179, 141, 88, 63, 50, 75,
		102, 140, 179, 165, 141, 101, 76, 75, 102, 140, 179, 178, 166, 114, 75, 50, 63, 114, 178, 217, 216, 166, 101,
		50, 24, 38, 101, 166, 204, 217, 179, 127, 76, 37, 50, 88, 153, 205, 216, 179, 140, 76, 37, 38, 88, 153, 205,
		230, 191, 128, 63, 12, 11, 49, 114, 178, 205, 191, 153, 102, 49, 25, 37, 76, 141, 191, 205, 179, 140, 76, 25,
		24, 50, 101, 166, 204, 217, 191, 141, 75, 38, 24, 64, 127, 179, 230, 229, 191, 128, 75, 38, 38, 49, 102, 141,
		178, 192, 166, 127, 102, 75, 76, 102, 140, 179, 178, 166, 140, 102, 75, 76, 101, 141, 165, 192, 191, 179, 127,
		88, 50, 49, 76, 113, 166, 204, 217, 178, 128, 64, 11, 0, 24, 76, 127, 192, 230, 229, 205, 152, 102, 50, 24, 38,
		75, 114, 165, 192, 192, 178, 141, 101, 49, 25, 24, 50, 101, 153, 191, 205, 191, 166, 113, 76, 37, 38, 63, 102,
		152, 205, 217, 217, 178, 128, 75, 38, 25, 38, 63, 114, 165, 205, 217, 216, 192, 153, 101, 50, 24, 25, 63, 102,
		152, 192, 216, 217, 178, 141, 76, 37, 25, 24, 50, 101, 153, 192, 217, 217, 192, 152, 102, 49, 25, 24, 38, 76,
		127, 179, 217, 216, 205, 165, 114, 63, 25, 11, 25, 63, 114, 166, 204, 230, 217, 204, 166, 113, 64, 38, 37, 50,
		88, 128, 178, 204, 217, 204, 166, 128, 76, 25, 12, 24, 50, 101, 140, 192, 216, 217, 192, 165, 128, 75, 38, 24,
		25, 49, 76, 127, 166, 204, 217, 204, 179, 141, 101, 64, 49, 50, 63, 89, 113, 166, 191, 205, 204, 192, 152, 114,
		76, 50, 49, 64, 88, 128, 165, 192, 204, 192, 178, 153, 128, 101, 76, 64, 64, 75, 102, 127, 166, 178, 192, 178,
		153, 127, 89, 49, 38, 38, 49, 76, 101, 128, 152, 179, 191, 192, 178, 153, 128, 88, 76, 63, 50, 63, 76, 102, 127,
		153, 166, 165, 166, 152, 128, 114, 76, 63, 50, 38, 49, 76, 101, 141, 165, 192, 205, 204, 192, 165, 141, 101, 76,
		64, 49, 50, 76, 89, 127, 153, 191, 205, 204, 191, 179, 152, 114, 88, 76, 63, 50, 64, 75, 102, 127, 153, 166,
		166, 165, 153, 140, 128, 113, 89, 75, 76, 76, 88, 102, 113, 141, 153, 166, 179, 178, 166, 140, 114, 101, 76, 64,
		64, 63, 76, 89, 127, 153, 178, 192, 204, 192, 192, 165, 128, 101, 64, 50, 37, 37, 50, 75, 102, 127, 166, 192,
		204, 205, 192, 178, 153, 127, 102, 75, 50, 38, 24, 38, 63, 89, 114, 140, 179, 191, 217, 204, 192, 178, 153, 140,
		102, 75, 38, 24, 12, 24, 50, 75, 102, 140, 166, 191, 205, 204, 205, 179, 166, 141, 114, 89, 63, 50, 50, 50, 76,
		89, 114, 140, 166, 191, 205, 205, 205, 191, 179, 140, 114, 75, 50, 38, 24, 25, 49, 64, 88, 102, 140, 166, 178,
		192, 204, 205, 191, 166, 140, 114, 75, 49, 25, 24, 25, 37, 64, 89, 113, 141, 165, 192, 205, 216, 217, 204, 179,
		166, 127, 102, 75, 50, 38, 37, 38, 64, 88, 102, 127, 153, 178, 192, 204, 205, 205, 192, 165, 141, 113, 102, 75,
		64, 49, 50, 49, 76, 88, 114, 140, 166, 178, 205, 216, 205, 192, 165, 141, 113, 89, 63, 38, 24, 25, 25, 37, 50,
		75, 102, 127, 153, 165, 192, 204, 205, 204, 192, 165, 153, 127, 102, 76, 64, 50, 38, 38, 37, 50, 76, 101, 128,
		152, 166, 191, 204, 217, 205, 204, 192, 165, 141, 127, 102, 75, 50, 38, 37, 38, 37, 64, 75, 102, 127, 153, 178,
		192, 205, 204, 192, 191, 179, 165, 140, 128, 101, 89, 63, 50, 50, 38, 37, 50, 75, 89, 113, 141, 165, 179, 191,
		204, 205, 204, 192, 165, 153, 127, 114, 88, 76, 50, 37, 38, 37, 50, 76, 89, 101, 128, 152, 166, 191, 192, 205,
		204, 205, 191, 179, 152, 141, 114, 88, 76, 49, 38, 38, 37, 38, 50, 63, 89, 101, 128, 152, 179, 192, 192, 205,
		204, 192, 178, 166, 152, 141, 113, 102, 75, 64, 49, 38, 38, 38, 50, 50, 63, 89, 101, 128, 152, 166, 179, 191,
		192, 192, 178, 179, 166, 152, 128, 113, 89, 75, 63, 50, 49, 50, 63, 63, 76, 88, 102, 113, 141, 152, 166, 192,
		191, 192, 191, 192, 178, 179, 165, 153, 128, 113, 89, 76, 63, 64, 49, 64, 64, 75, 89, 102, 113, 128, 152, 166,
		179, 192, 191, 192, 178, 179, 165, 153, 140, 128, 101, 89, 75, 64, 63, 49, 50, 64, 63, 76, 88, 102, 114, 127,
		141, 165, 179, 191, 192, 192, 192, 178, 179, 165, 153, 140, 128, 101, 89, 75, 50, 49, 50, 37, 38, 50, 63, 76,
		75, 89, 113, 128, 140, 153, 165, 179, 191, 192, 192, 191, 179, 178, 166, 152, 128, 113, 102, 89, 63, 64, 49, 38,
		38, 50, 50, 63, 76, 88, 114, 127, 141, 152, 166, 178, 192, 191, 192, 204, 192, 178, 166, 152, 153, 127, 114,
		101, 89, 75, 64, 63, 50, 49, 50, 49, 64, 75, 89, 101, 114, 127, 153, 165, 166, 178, 192, 204, 192, 192, 192,
		178, 166, 152, 141, 127, 114, 101, 89, 75, 64, 50, 49, 50, 63, 64, 76, 76, 89, 102, 127, 141, 140, 153, 166,
		179, 178, 179, 178, 179, 179, 166, 152, 141, 127, 114, 102, 88, 76, 63, 64, 49, 50, 49, 64, 63, 76, 75, 89, 102,
		114, 127, 141, 152, 166, 179, 178, 192, 191, 192, 192, 191, 179, 178, 166, 152, 141, 128, 113, 102, 89, 76, 63,
		64, 49, 50 };

const unsigned int phaser_wav_len = 1792;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc1;

CRC_HandleTypeDef hcrc;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef hi2c4;

IWDG_HandleTypeDef hiwdg;

MDIOS_HandleTypeDef hmdios;

RNG_HandleTypeDef hrng;

SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi4;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
DMA_HandleTypeDef hdma_tim2_up_ch3;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart7;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_rx;

osThreadId defaultTaskHandle;
osThreadId LPTaskHandle;
osMessageQId myQueue01Handle;
osTimerId myTimer01Handle;
osMutexId myMutex01Handle;
osSemaphoreId ssicontentHandle;
/* USER CODE BEGIN PV */
struct netif *netif, *netif2;
extern const ip4_addr_t *ipaddr;
uint32_t t1sec = 0;
extern uint32_t t2cap[1];
int main_init_done = 0;
int lptask_init_done = 0;
char trigtimestr[128] = { "No Triggers" };
char nowtimestr[64] = { "\"No Time\"" };
char pressstr[10] = { "0" };
char tempstr[6] = { "0" };
char snstr[160] = { "\"No S/N\"" };
char statstr[172] = { "\"No status\"" };
char gpsstr[128] = { "\"No GPS data\"" };
uint16_t agc = 1;	// agc enable > 0
uint32_t myip;
uint32_t uip;		// udp target

//char snstr[4] = {"No"};
//char statstr[4] = {"No"};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_MDIOS_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_RNG_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM7_Init(void);
static void MX_TIM1_Init(void);
static void MX_CRC_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_DAC_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART4_Init(void);
static void MX_UART5_Init(void);
static void MX_UART7_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI4_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C4_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM4_Init(void);
static void MX_IWDG_Init(void);
void StartDefaultTask(void const *argument);
void StarLPTask(void const *argument);
void Callback01(void const *argument);

static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) {
	int DataIdx;

	for (DataIdx = 0; DataIdx < len; DataIdx++) {
		__io_putchar(*ptr++);
	}
	return len;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* Enable I-Cache---------------------------------------------------------*/
	SCB_EnableICache();

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_MDIOS_Init();
	MX_USART2_UART_Init();
	MX_ADC1_Init();
	MX_ADC2_Init();
	MX_ADC3_Init();
	MX_RNG_Init();
	MX_TIM6_Init();
	MX_TIM3_Init();
	MX_TIM7_Init();
	MX_TIM1_Init();
	MX_CRC_Init();
	MX_TIM2_Init();
	MX_USART6_UART_Init();
	MX_DAC_Init();
	MX_I2C1_Init();
	MX_UART4_Init();
	MX_UART5_Init();
	MX_UART7_Init();
	MX_USART3_UART_Init();
	MX_SPI4_Init();
	MX_SPI3_Init();
	MX_SPI2_Init();
	MX_I2C4_Init();
	MX_I2C2_Init();
	MX_TIM4_Init();
	MX_IWDG_Init();

	/* Initialize interrupts */
	MX_NVIC_Init();
	/* USER CODE BEGIN 2 */

	/* USER CODE END 2 */

	/* Create the mutex(es) */
	/* definition and creation of myMutex01 */
	osMutexDef(myMutex01);
	myMutex01Handle = osMutexCreate(osMutex(myMutex01));

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* Create the semaphores(s) */
	/* definition and creation of ssicontent */
	osSemaphoreDef(ssicontent);
	ssicontentHandle = osSemaphoreCreate(osSemaphore(ssicontent), 1);

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* Create the timer(s) */
	/* definition and creation of myTimer01 */
	osTimerDef(myTimer01, Callback01);
	myTimer01Handle = osTimerCreate(osTimer(myTimer01), osTimerPeriodic, NULL);

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* Create the queue(s) */
	/* definition and creation of myQueue01 */
	osMessageQDef(myQueue01, 256, uint16_t);
	myQueue01Handle = osMessageCreate(osMessageQ(myQueue01), NULL);

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* definition and creation of defaultTask */
	osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 1024);
	defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

	/* definition and creation of LPTask */
	osThreadDef(LPTask, StarLPTask, osPriorityLow, 0, 1024);
	LPTaskHandle = osThreadCreate(osThread(LPTask), NULL);

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
	/* USER CODE END RTOS_THREADS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };

	/** Configure LSE Drive Capability
	 */
	HAL_PWR_EnableBkUpAccess();
	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 216;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 6;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Activate the Over-Drive mode
	 */
	if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_USART3 | RCC_PERIPHCLK_USART6
			| RCC_PERIPHCLK_UART4 | RCC_PERIPHCLK_UART5 | RCC_PERIPHCLK_UART7 | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_I2C2
			| RCC_PERIPHCLK_I2C4 | RCC_PERIPHCLK_CLK48;
	PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
	PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
	PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
	PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
	PeriphClkInitStruct.PLLSAIDivQ = 1;
	PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
	PeriphClkInitStruct.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
	PeriphClkInitStruct.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
	PeriphClkInitStruct.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
	PeriphClkInitStruct.Uart5ClockSelection = RCC_UART5CLKSOURCE_PCLK1;
	PeriphClkInitStruct.Usart6ClockSelection = RCC_USART6CLKSOURCE_PCLK2;
	PeriphClkInitStruct.Uart7ClockSelection = RCC_UART7CLKSOURCE_PCLK1;
	PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	PeriphClkInitStruct.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
	PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PCLK1;
	PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief NVIC Configuration.
 * @retval None
 */
static void MX_NVIC_Init(void) {
	/* USART2_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
	/* USART6_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(USART6_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(USART6_IRQn);
	/* ADC_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(ADC_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(ADC_IRQn);
	/* EXTI15_10_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_MultiModeTypeDef multimode = { 0 };
	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */
	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.ContinuousConvMode = ENABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}
	/** Configure the ADC multi-mode
	 */
	multimode.Mode = ADC_TRIPLEMODE_INTERL;
	multimode.DMAAccessMode = ADC_DMAACCESSMODE_2;
	multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES;
	if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
		Error_Handler();
	}
	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_3;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief ADC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC2_Init(void) {

	/* USER CODE BEGIN ADC2_Init 0 */

	/* USER CODE END ADC2_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC2_Init 1 */

	/* USER CODE END ADC2_Init 1 */
	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc2.Instance = ADC2;
	hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc2.Init.Resolution = ADC_RESOLUTION_12B;
	hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc2.Init.ContinuousConvMode = ENABLE;
	hadc2.Init.DiscontinuousConvMode = DISABLE;
	hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc2.Init.NbrOfConversion = 1;
	hadc2.Init.DMAContinuousRequests = DISABLE;
	hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	if (HAL_ADC_Init(&hadc2) != HAL_OK) {
		Error_Handler();
	}
	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_3;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC2_Init 2 */

	/* USER CODE END ADC2_Init 2 */

}

/**
 * @brief ADC3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC3_Init(void) {

	/* USER CODE BEGIN ADC3_Init 0 */

	/* USER CODE END ADC3_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC3_Init 1 */

	/* USER CODE END ADC3_Init 1 */
	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc3.Instance = ADC3;
	hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc3.Init.Resolution = ADC_RESOLUTION_12B;
	hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc3.Init.ContinuousConvMode = ENABLE;
	hadc3.Init.DiscontinuousConvMode = DISABLE;
	hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc3.Init.NbrOfConversion = 1;
	hadc3.Init.DMAContinuousRequests = DISABLE;
	hadc3.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	if (HAL_ADC_Init(&hadc3) != HAL_OK) {
		Error_Handler();
	}
	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_3;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC3_Init 2 */

	/* USER CODE END ADC3_Init 2 */

}

/**
 * @brief CRC Initialization Function
 * @param None
 * @retval None
 */
static void MX_CRC_Init(void) {

	/* USER CODE BEGIN CRC_Init 0 */

	/* USER CODE END CRC_Init 0 */

	/* USER CODE BEGIN CRC_Init 1 */

	/* USER CODE END CRC_Init 1 */
	hcrc.Instance = CRC;
	hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
	hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
	hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
	hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
	hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
	if (HAL_CRC_Init(&hcrc) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN CRC_Init 2 */

	/* USER CODE END CRC_Init 2 */

}

/**
 * @brief DAC Initialization Function
 * @param None
 * @retval None
 */
static void MX_DAC_Init(void) {

	/* USER CODE BEGIN DAC_Init 0 */

	/* USER CODE END DAC_Init 0 */

	DAC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN DAC_Init 1 */

	/* USER CODE END DAC_Init 1 */
	/** DAC Initialization
	 */
	hdac.Instance = DAC;
	if (HAL_DAC_Init(&hdac) != HAL_OK) {
		Error_Handler();
	}
	/** DAC channel OUT1 config
	 */
	sConfig.DAC_Trigger = DAC_TRIGGER_T7_TRGO;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN DAC_Init 2 */

	/* USER CODE END DAC_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x20404768;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {

	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.Timing = 0x20404768;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */

}

/**
 * @brief I2C4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C4_Init(void) {

	/* USER CODE BEGIN I2C4_Init 0 */

	/* USER CODE END I2C4_Init 0 */

	/* USER CODE BEGIN I2C4_Init 1 */

	/* USER CODE END I2C4_Init 1 */
	hi2c4.Instance = I2C4;
	hi2c4.Init.Timing = 0x20404768;
	hi2c4.Init.OwnAddress1 = 0;
	hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c4.Init.OwnAddress2 = 0;
	hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c4) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		Error_Handler();
	}
	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C4_Init 2 */

	/* USER CODE END I2C4_Init 2 */

}

/**
 * @brief IWDG Initialization Function
 * @param None
 * @retval None
 */
static void MX_IWDG_Init(void) {

	/* USER CODE BEGIN IWDG_Init 0 */

	/* USER CODE END IWDG_Init 0 */

	/* USER CODE BEGIN IWDG_Init 1 */
#ifndef TESTING
  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */
#endif
	/* USER CODE END IWDG_Init 2 */

}

/**
 * @brief MDIOS Initialization Function
 * @param None
 * @retval None
 */
static void MX_MDIOS_Init(void) {

	/* USER CODE BEGIN MDIOS_Init 0 */

	/* USER CODE END MDIOS_Init 0 */

	/* USER CODE BEGIN MDIOS_Init 1 */

	/* USER CODE END MDIOS_Init 1 */
	hmdios.Instance = MDIOS;
	hmdios.Init.PortAddress = 0;
	hmdios.Init.PreambleCheck = MDIOS_PREAMBLE_CHECK_ENABLE;
	if (HAL_MDIOS_Init(&hmdios) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN MDIOS_Init 2 */

	/* USER CODE END MDIOS_Init 2 */

}

/**
 * @brief RNG Initialization Function
 * @param None
 * @retval None
 */
static void MX_RNG_Init(void) {

	/* USER CODE BEGIN RNG_Init 0 */

	/* USER CODE END RNG_Init 0 */

	/* USER CODE BEGIN RNG_Init 1 */

	/* USER CODE END RNG_Init 1 */
	hrng.Instance = RNG;
	if (HAL_RNG_Init(&hrng) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN RNG_Init 2 */

	/* USER CODE END RNG_Init 2 */

}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

	/* USER CODE BEGIN SPI2_Init 0 */

	/* USER CODE END SPI2_Init 0 */

	/* USER CODE BEGIN SPI2_Init 1 */

	/* USER CODE END SPI2_Init 1 */
	/* SPI2 parameter configuration*/
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 7;
	hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI2_Init 2 */

	/* USER CODE END SPI2_Init 2 */

}

/**
 * @brief SPI3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI3_Init(void) {

	/* USER CODE BEGIN SPI3_Init 0 */

	/* USER CODE END SPI3_Init 0 */

	/* USER CODE BEGIN SPI3_Init 1 */

	/* USER CODE END SPI3_Init 1 */
	/* SPI3 parameter configuration*/
	hspi3.Instance = SPI3;
	hspi3.Init.Mode = SPI_MODE_SLAVE;
	hspi3.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3.Init.DataSize = SPI_DATASIZE_4BIT;
	hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3.Init.NSS = SPI_NSS_SOFT;
	hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3.Init.CRCPolynomial = 7;
	hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	if (HAL_SPI_Init(&hspi3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI3_Init 2 */

	/* USER CODE END SPI3_Init 2 */

}

/**
 * @brief SPI4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI4_Init(void) {

	/* USER CODE BEGIN SPI4_Init 0 */

	/* USER CODE END SPI4_Init 0 */

	/* USER CODE BEGIN SPI4_Init 1 */

	/* USER CODE END SPI4_Init 1 */
	/* SPI4 parameter configuration*/
	hspi4.Instance = SPI4;
	hspi4.Init.Mode = SPI_MODE_MASTER;
	hspi4.Init.Direction = SPI_DIRECTION_2LINES;
	hspi4.Init.DataSize = SPI_DATASIZE_4BIT;
	hspi4.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi4.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi4.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi4.Init.CRCPolynomial = 7;
	hspi4.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi4.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi4) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI4_Init 2 */

	/* USER CODE END SPI4_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 0;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_SlaveConfigTypeDef sSlaveConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_IC_InitTypeDef sConfigIC = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 4000000000;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_IC_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
	sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
	sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
	sSlaveConfig.TriggerFilter = 0;
	if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
	sConfigIC.ICFilter = 0;
	if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK) {
		Error_Handler();
	}
	sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
	if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK) {
		Error_Handler();
	}
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 10800;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 10000;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 10;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 0;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 1100;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_OC_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC1;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
	sConfigOC.Pulse = 550;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_OC_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */
	HAL_TIM_MspPostInit(&htim4);

}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void) {

	/* USER CODE BEGIN TIM6_Init 0 */

	/* USER CODE END TIM6_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM6_Init 1 */

	/* USER CODE END TIM6_Init 1 */
	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 10800;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 10000;
	htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM6_Init 2 */

	/* USER CODE END TIM6_Init 2 */

}

/**
 * @brief TIM7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM7_Init(void) {

	/* USER CODE BEGIN TIM7_Init 0 */

	/* USER CODE END TIM7_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM7_Init 1 */

	/* USER CODE END TIM7_Init 1 */
	htim7.Instance = TIM7;
	htim7.Init.Prescaler = 0;
	htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim7.Init.Period = 9600;
	htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM7_Init 2 */

	/* USER CODE END TIM7_Init 2 */

}

/**
 * @brief UART4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART4_Init(void) {

	/* USER CODE BEGIN UART4_Init 0 */

	/* USER CODE END UART4_Init 0 */

	/* USER CODE BEGIN UART4_Init 1 */

	/* USER CODE END UART4_Init 1 */
	huart4.Instance = UART4;
	huart4.Init.BaudRate = 115200;
	huart4.Init.WordLength = UART_WORDLENGTH_8B;
	huart4.Init.StopBits = UART_STOPBITS_1;
	huart4.Init.Parity = UART_PARITY_NONE;
	huart4.Init.Mode = UART_MODE_TX_RX;
	huart4.Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS;
	huart4.Init.OverSampling = UART_OVERSAMPLING_16;
	huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart4) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART4_Init 2 */

	/* USER CODE END UART4_Init 2 */

}

/**
 * @brief UART5 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART5_Init(void) {

	/* USER CODE BEGIN UART5_Init 0 */

	/* USER CODE END UART5_Init 0 */

	/* USER CODE BEGIN UART5_Init 1 */

	/* USER CODE END UART5_Init 1 */
	huart5.Instance = UART5;
	huart5.Init.BaudRate = 115200;
	huart5.Init.WordLength = UART_WORDLENGTH_8B;
	huart5.Init.StopBits = UART_STOPBITS_1;
	huart5.Init.Parity = UART_PARITY_NONE;
	huart5.Init.Mode = UART_MODE_TX_RX;
	huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart5.Init.OverSampling = UART_OVERSAMPLING_16;
	huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart5) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART5_Init 2 */

	/* USER CODE END UART5_Init 2 */

}

/**
 * @brief UART7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART7_Init(void) {

	/* USER CODE BEGIN UART7_Init 0 */

	/* USER CODE END UART7_Init 0 */

	/* USER CODE BEGIN UART7_Init 1 */

	/* USER CODE END UART7_Init 1 */
	huart7.Instance = UART7;
	huart7.Init.BaudRate = 115200;
	huart7.Init.WordLength = UART_WORDLENGTH_8B;
	huart7.Init.StopBits = UART_STOPBITS_1;
	huart7.Init.Parity = UART_PARITY_NONE;
	huart7.Init.Mode = UART_MODE_TX_RX;
	huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart7.Init.OverSampling = UART_OVERSAMPLING_16;
	huart7.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_RS485Ex_Init(&huart7, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART7_Init 2 */

	/* USER CODE END UART7_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_RS485Ex_Init(&huart2, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_MultiProcessor_Init(&huart3, 0, UART_WAKEUPMETHOD_IDLELINE) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void) {

	/* USER CODE BEGIN USART6_Init 0 */

	/* USER CODE END USART6_Init 0 */

	/* USER CODE BEGIN USART6_Init 1 */

	/* USER CODE END USART6_Init 1 */
	huart6.Instance = USART6;
	huart6.Init.BaudRate = 9600;
	huart6.Init.WordLength = UART_WORDLENGTH_8B;
	huart6.Init.StopBits = UART_STOPBITS_1;
	huart6.Init.Parity = UART_PARITY_NONE;
	huart6.Init.Mode = UART_MODE_TX_RX;
	huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart6.Init.OverSampling = UART_OVERSAMPLING_16;
	huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXOVERRUNDISABLE_INIT | UART_ADVFEATURE_DMADISABLEONERROR_INIT;
	huart6.AdvancedInit.OverrunDisable = UART_ADVFEATURE_OVERRUN_DISABLE;
	huart6.AdvancedInit.DMADisableonRxError = UART_ADVFEATURE_DMA_DISABLEONRXERROR;
	if (HAL_RS485Ex_Init(&huart6, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART6_Init 2 */

	/* USER CODE END USART6_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Stream1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
	/* DMA1_Stream5_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
	/* DMA2_Stream1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
	/* DMA2_Stream4_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(probe1_GPIO_Port, probe1_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, LD1_Pin | GPIO_PIN_11 | LD3_Pin | LD2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOE, XBEE_DTR_Pin | LP_FILT_Pin | XBEE_RST_Pin | GPIO_PIN_0, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD, LED_D1_Pin | LED_D2_Pin | LED_D3_Pin | LED_D4_Pin | LED_D5_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOG, CS_PGA_Pin | USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(probe2_GPIO_Port, probe2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : PE3 PE7 PE8 PE11
	 PE12 PE13 PE1 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pin : USER_Btn_Pin */
	GPIO_InitStruct.Pin = USER_Btn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PF2 PF3 PF4 PF5
	 PF10 PF11 PF12 PF13 */
	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12
			| GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : probe1_Pin */
	GPIO_InitStruct.Pin = probe1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(probe1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PC0 PC8 PC9 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : LD1_Pin PB11 LD3_Pin LD2_Pin */
	GPIO_InitStruct.Pin = LD1_Pin | GPIO_PIN_11 | LD3_Pin | LD2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PB1 PB5 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PG0 PG1 PG3 PG4
	 PG5 PG10 PG12 PG14
	 PG15 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_12
			| GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pin : PE9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : XBEE_DTR_Pin LP_FILT_Pin XBEE_RST_Pin PE0 */
	GPIO_InitStruct.Pin = XBEE_DTR_Pin | LP_FILT_Pin | XBEE_RST_Pin | GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : PD10 PD0 PD2 PD7 */
	GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : LED_D1_Pin LED_D2_Pin LED_D3_Pin LED_D4_Pin
	 LED_D5_Pin */
	GPIO_InitStruct.Pin = LED_D1_Pin | LED_D2_Pin | LED_D3_Pin | LED_D4_Pin | LED_D5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : CS_PGA_Pin USB_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = CS_PGA_Pin | USB_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pin : USB_OverCurrent_Pin */
	GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : probe2_Pin */
	GPIO_InitStruct.Pin = probe2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(probe2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void rebootme() {
	while (1) {
		osDelay(2000);
		__NVIC_SystemReset();   // reboot
	}
}

void netif_status_callbk_fn(struct netif *netif) {

	printf("netif_status changed\n");
	/*	printf("netif: IP address of '%c%c' set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n", netif->name[0],
	 netif->name[1], ip4_addr1_16(netif_ip4_addr(netif)), ip4_addr2_16(netif_ip4_addr(netif)),
	 ip4_addr3_16(netif_ip4_addr(netif)), ip4_addr4_16(netif_ip4_addr(netif)));
	 */

	/*	printf("netif: Gateway %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
	 ip4_addr1_16(netif_ip4_gw(netif)),
	 ip4_addr2_16(netif_ip4_gw(netif)),
	 ip4_addr3_16(netif_ip4_gw(netif)),
	 ip4_addr4_16(netif_ip4_gw(netif)));
	 printf("netif: NetMask %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
	 ip4_addr1_16(netif_ip4_netmask(netif)),
	 ip4_addr2_16(netif_ip4_netmask(netif)),
	 ip4_addr3_16(netif_ip4_netmask(netif)),
	 ip4_addr4_16(netif_ip4_netmask(netif)));
	 */
//	netif_set_addr(netif,netif_ip4_addr(netif),netif_ip4_gw(netif),netif_ip4_netmask(netif));
}

void netif_link_callbk_fn(struct netif *netif) {

	if (netif->flags & NETIF_FLAG_LINK_UP) {
		printf("netif_link UP, flags=0x%02x\n", netif->flags);
	} else {
		printf("netif_link DOWN, flags=0x%02x\n", netif->flags);
		if (!(netif_is_link_up(netif))) {
			printf("LAN interface appears disconnected, rebooting...\n");
			rebootme();
		}
	}
}

uint32_t movavg(uint32_t new) {
	static uint32_t data[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int i;
	uint32_t sum = 0;

	for (i = 0; i < 15; i++) {
		data[i] = data[i + 1];		// old data is low index
		sum += data[i];
	}
	data[15] = new;		// new data at the end
	sum += new;

	return (sum >> 4);
}

// dma for DAC finished callback
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
	HAL_DAC_Stop_DMA(hdac, DAC_CHANNEL_1);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) { // every second 1 pps (on external signal)
	uint32_t diff;
	static uint32_t lastcap = 0;

#ifdef SPLAT1
	HAL_GPIO_TogglePin(GPIOD, LED_D1_Pin);
#else
	HAL_GPIO_TogglePin(GPIOB, LD1_Pin);		// green led
#endif

	if (htim->Instance == TIM2) {
		rtseconds = (statuspkt.NavPvt.sec + 1) % 60;// assume we get here before serial comms updates gps seconds field
//printf(":%d ",rtseconds);
//		statuspkt.epochsecs++;
//		neotime();
#if 0		// clktrim with non-resetting pps counter
				if (lastcap > t2cap[0]) {		// counter wrapped
					diff = 4000000000 /*UINT_MAX*/- lastcap + t2cap[0];
				} else {
					diff = t2cap[0] - lastcap;
				}
#else
		diff = lastcap;
#endif
		statuspkt.clktrim = movavg(diff);
		/*		printf("TIM2 IC Callback CCR3=%08x, [0]%08xu, [1]%08x diff=%u, clktrim=%08x",
		 htim->Instance->CCR3, t2cap[0], lastcap, diff, statuspkt.clktrim);
		 printf(" globaladcavg=%u\n",globaladcavg);
		 */
	}
	if (htim->Instance == TIM4) {
		printf("Timer4 callback\n");
	}
	lastcap = t2cap[0];
//	htim->Instance->SR = 0;		// cheat

	/* USER CODE END Callback 1 */
}

void setupnotify() {
	/* Store the handle of the calling task. */
	xTaskToNotify = xTaskGetCurrentTaskHandle();
}

#if 0
// caclulate and set the pga gain - gets called once per second
// uses agc noise but this may be too simplistic...
#define GAININT 20	/* seconds */
void calcagc() {
	static uint32_t noiseacc = 0;
	static int samples = 0;
	uint16_t newgain;

	noiseacc += globaladcnoise;
	samples++;

	if (samples > GAININT) {
		newgain = (pgagain & 0x07);
		if (noiseacc > (40 * GAININT))	// too noisy
				{
			if (newgain > 0) {
				newgain--;			// decrease the gain
				setpgagain(newgain);
			}
		} else if (noiseacc < (3 * GAININT))	// too quiet
				{
			if (newgain < 7) {
				newgain++;			// increase the gain
				setpgagain(newgain);
			}
		}
		samples = 0;
		noiseacc = 0;
	}
}
#endif
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {

	/* init code for USB_DEVICE */
	MX_USB_DEVICE_Init();

	/* init code for LWIP */
	MX_LWIP_Init();

	/* init code for FATFS */
	MX_FATFS_Init();

	/* USER CODE BEGIN 5 */
	HAL_StatusTypeDef err;
	struct dhcp *dhcp;
	int i;
	HAL_StatusTypeDef stat;
//		uint16_t dacdata[64];
	char stmuid[64] = { 0 };

	printf("\n\n-------------------------------------------------------------------\n");
	printf("Detector STM_UUID=%lx %lx %lx, SW Ver=%d.%d, SW S/n=%d\n", STM32_UUID[0], STM32_UUID[1], STM32_UUID[2],
	MAJORVERSION, MINORVERSION, MY_UID);
//	printf("STM_UUID=%lx %lx %lx\n", STM32_UUID[0], STM32_UUID[1],	STM32_UUID[2]);

	if (!(netif_is_link_up(&gnetif))) {
		printf("LAN interface appears disconnected\n\r");
		for (i = 0; i < 50; i++) {
			osDelay(50);
			HAL_GPIO_WritePin(GPIOD, LED_D5_Pin, GPIO_PIN_SET);	// Splat D5 led on
			osDelay(50);
			HAL_GPIO_WritePin(GPIOD, LED_D5_Pin, GPIO_PIN_RESET);	// Splat D5 led off
		}
		printf("************* REBOOTING **************\n");
		rebootme();
	}

	netif = netif_default;
	netif_set_link_callback(netif, netif_link_callbk_fn);
	netif_set_status_callback(netif, netif_status_callbk_fn);

	t2cap[0] = 44444444;

	statuspkt.uid = MY_UID;		// 16 bits
	statuspkt.majorversion = MAJORVERSION;
	statuspkt.minorversion = MINORVERSION;
	statuspkt.udppknum = 0;
	statuspkt.sysuptime = 0;
	statuspkt.netuptime = 0;
	statuspkt.gpsuptime = 0;

	statuspkt.adcpktssent = 0;
	statuspkt.adctrigoff = TRIG_THRES;

//		osDelay(100);

#ifdef TESTING
	printf("*** TESTING BUILD USED ***\n");
//		testeeprom();
#endif

	stat = setupneo();
	if (stat != HAL_OK) {
		printf("Neo7 setup returned HAL error\n\r");	// but don't reboot
	}

#ifdef SPLAT1
	initsplat();
#endif
	printf("Setting up timers\n");

	if (xSemaphoreGive(ssicontentHandle) != pdTRUE) {	// give the ssi generation semaphore
		/*printf("Initial semaphoregive failed\n")*/;	// expect this to fail as part of the normal setup
	}

	HAL_TIM_Base_Start_IT(&htim6);		// basic packet timestamp 32 bits
//		HAL_TIM_Base_Start_IT(&htim3);		// test timebase 1Hz pulse output
//		HAL_TIM_OC_Start_IT(&htim3, TIM_CHANNEL_2);		// 1mS

	TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_1, TIM_CCx_DISABLE);		// precision uS timer
	TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_2, TIM_CCx_DISABLE);		// precision uS timer
	TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_4, TIM_CCx_DISABLE);		// precision uS timer

	HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_1);		// precision uS timer
	HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_2);		// precision uS timer
	HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_4);		// precision uS timer

	//HAL_TIM_IC_Init and HAL_TIM_IC_ConfigChannel
	//  (++) Input Capture :  HAL_TIM_IC_Start(), HAL_TIM_IC_Start_DMA(), HAL_TIM_IC_Start_IT()

	// capture on PB10 input
	if ((err = HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_3, t2cap, (sizeof(t2cap) / 4))) != HAL_OK) {
		printf("TIM_Base_Start_DMA err %i", err);
		Error_Handler();
	}
	TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_3, TIM_CCx_ENABLE);	// capture precision timer

	dhcp = netif_dhcp_data(netif);		// do not call this too early
#if 0
	i = 0;
	printf("DHCP in progress..\n");
	while ((dhcp->state != DHCP_STATE_BOUND)) {
		printf("Waiting %d for DHCP...\n", i);
		osDelay(2000);
		i++;

		if (i > 10) {
			printf("*****************************************\n");
			printf("************* DHCP ABORTED **************\n");
			printf("*****************************************\n");
			rebootme();
		} // end if
	} // end while
#endif
	ip_addr_t ip = { 0 };

	ip = dhcp->offered_ip_addr;
	myip = ip.addr;
	printf("*****************************************\n");
	printf("This unit's IP address is %d:%d:%d:%d\n", myip & 0xFF, (myip & 0xFF00) >> 8, (myip & 0xFF0000) >> 16,
			(myip & 0xFF000000) >> 24);
	printf("*****************************************\n");

// start the web stuff
	sprintf(stmuid, "api/Device/%lx%lx%lx", STM32_UUID[0], STM32_UUID[1], STM32_UUID[2]);
	i = 1;
	while (statuspkt.uid == MY_UID)		// not yet found new S/N from server
	{
		printf("Try to get new S/N using http client. Try=%d\n", i++);
		httpclient(stmuid);
		osDelay(5000);
#ifdef TESTING
		stats_display() ; // this needs stats in LwIP enabling to do anything
#endif
		if (i++ > 10) {
			printf("************* ABORTED **************\n");
			rebootme();
		}
	}

	osDelay(1000);
	printf("Starting httpd\n");
	httpd_init();		// start the www server
	init_httpd_ssi();	// set up the embedded tag handler

// tim7 drives DAC
#if 1
	printf("Warming up phasors\n");
//		HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, dacdata, sizeof(dacdata) >> 1, DAC_ALIGN_12B_L);
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, phaser_wav, sizeof(phaser_wav),
	DAC_ALIGN_8B_R /*DAC_ALIGN_12B_R*/);
	HAL_TIM_Base_Start(&htim7);		// fast interval DAC timer sample rate
#endif

	setupnotify();
	startadc();
	uip = locateudp();
	main_init_done = 1; // let lptask now main has initialised
	printf("Waiting for lptask\n");
	while (lptask_init_done == 0)
		osDelay(100); // hold off starting udp railgun until LPtask has initalised

	while (1) {	//
		startudp(uip);		// should never return
		printf("UDP stream exited!!!\n\r");
		rebootme();
	}

	/* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StarLPTask */
/**
 * @brief Function implementing the LPTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StarLPTask */
void StarLPTask(void const *argument) {
	/* USER CODE BEGIN StarLPTask */
	static uint32_t lastpretrigcnt, trigs = 0;
	uint32_t ip, reqtimer = 8000;
	uint16_t tenmstimer = 0;
	uint16_t onesectimer = 0;
	uint16_t gainchanged;
	int i, n, counter = 0;
	char str[32] = { "empty" };
	uint32_t lasttrigcount = 0;

	statuspkt.adcudpover = 0;		// debug use count overruns
	statuspkt.trigcount = 0;		// debug use adc trigger count
	statuspkt.udpsent = 0;	// debug use adc udp sample packet sent count

	while (main_init_done == 0)	// wait from main to complete the initilisation
		osDelay(100);
#if 1
#ifdef TESTING
	sprintf(snstr, "\"STM_UUID=%lx %lx %lx, Assigned S/N=%lu, TESTING Sw S/N=%d, Ver %d.%d, UDP Target=%s %s\"",
	STM32_UUID[0], STM32_UUID[1], STM32_UUID[2], statuspkt.uid, MY_UID, statuspkt.majorversion, statuspkt.minorversion,
			udp_target, udp_ips);
#else
	sprintf(snstr, "\"STM_UUID=%lx %lx %lx, Assigned S/N=%lu, Ver %d.%d, UDP Target=%s %s\"",
	STM32_UUID[0], STM32_UUID[1], STM32_UUID[2], statuspkt.uid, statuspkt.majorversion,
			statuspkt.minorversion, udp_target, udp_ips);
#endif
#endif

	HAL_TIM_Base_Start(&htim7);	// start audio synth sampling interval timer

// timer 4 used to generate audio monotone to splat speaker, but PCB fault prevents it working
//HAL_TIM_OC_Start (&htim4, TIM_CHANNEL_3);		// HAL_TIM_OC_Start (TIM_HandleTypeDef* htim, uint32_t Channel)

	lptask_init_done = 1;		// this lp task has done its initialisation

	for (;;) {							// The Low Priority Task forever loop
		HAL_IWDG_Refresh(&hiwdg);							// refresh the hardware watchdog reset system timer
		osDelay(10);		// 10mSec
//		tcp_tmr();
		reqtimer++;
		tenmstimer++;

		if (ledhang) {	// trigger led
			ledhang--;
#ifdef SPLAT1
			HAL_GPIO_WritePin(GPIOD, LED_D5_Pin, GPIO_PIN_SET);	// Splat D5 led on
		} else {
			HAL_GPIO_WritePin(GPIOD, LED_D5_Pin, GPIO_PIN_RESET);	// Splat D5 led off
		}
#else
		HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_SET);		// onboard red led on
	} else {
		HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);	 // onboard red led off
	}
#endif

//		globaladcnoise = meanwindiff;
//		if (globaladcnoise == 0)
//			globaladcnoise = statuspkt.adcbase;		// dont allow zero peaks

		if (statuspkt.trigcount > (360 + trigs)) { // spamming: 3600 packets sent in 2 Sec (out of approx 7.2K packets)
			jabber = 200;		// 2 seconds pause
			statuspkt.jabcnt++;
		} else {
			if (jabber)
				jabber--;		// de-arm count
		}

		if (trigs != statuspkt.trigcount) {		// another tigger(s) has occured
			trigs = statuspkt.trigcount;
//			HAL_TIM_OC_Start (&htim4, TIM_CHANNEL_3);		// start audio buzz - broken on splat 1
///			HAL_TIM_Base_Start(&htim7);	// audio synth sampling interval timer
			HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, phaser_wav, sizeof(phaser_wav),
			DAC_ALIGN_8B_R /*DAC_ALIGN_12B_R*/);		// start phaser noise

#if 1
			while (!(xSemaphoreTake(ssicontentHandle, (TickType_t ) 1) == pdTRUE)) {// take the ssi generation semaphore (portMAX_DELAY == infinite)
				printf("sem wait 1a\n");
			}
			strcpy(trigtimestr, ctime(&epochtime));		// ctime
			trigtimestr[strlen(trigtimestr) - 1] = '\0';	// replace newline with terminator
//			sprintf(trigtimestr, "%u", epochtime);

			if (xSemaphoreGive(ssicontentHandle) != pdTRUE) {		// give the ssi generation semaphore
				printf("semaphore 1a release failed\n");
			}
#endif

//			strcpy(trigtimestr,"There goes the time");		// copy current time into trig time string

#if 1
			strcpy(str, ctime(&epochtime));
			str[strlen(str) - 1] = '\0';	// replace newline with terminator
			sprintf(nowtimestr, "\"%s\"", str);
#else
			sprintf(nowtimestr, "\"%u\"", epochtime);
#endif
			sprintf(tempstr, "%d.%d", temperature, tempfrac);
			sprintf(pressstr, "%d.%d", pressure, pressfrac);
#if 1
			// construct detector status for webpage
			sprintf(statstr,
					"\"<b>Uptime</b> %d <b>secs<br><br>Last trigger</b> %s<br><br><b>Triggers</b> %d<br><br><b>Noise</b> %d<br><br><b>ADC Base</b> %d<br><br>\"",
					statuspkt.sysuptime, trigtimestr, statuspkt.trigcount, statuspkt.adcnoise, statuspkt.adcbase);

			if (gpslocked) {
				sprintf(gpsstr, "\"Locked: %d Sats<br>Lon: %d<br>Lat: %d\"", statuspkt.NavPvt.numSV,
						statuspkt.NavPvt.lon, statuspkt.NavPvt.lat);
			} else {
				strcpy(gpsstr, "\"<font color=red>**Lost GPS**<\/font>\"");
			}
#endif
		}  // if (trigs != statuspkt.trigcount)

		/**********************  Every 100mSec   *******************************/

#ifdef SPLAT1
		if (tenmstimer % 10 == 0) {
			gainchanged = 0;
			if (agc) {
				// set the PGA to provide a sensible noise level
				if (globaladcnoise < 200)
					gainchanged = bumppga(1);
				else if (globaladcnoise > 550)
					gainchanged = bumppga(-1);
			}

			// when no pga change needed - see if trig level is sensible and adjust it
			if (gainchanged == 0) {		// gain not just changed
				n = pretrigcnt - lastpretrigcnt;		// count pretriggers
				if (n > 5) {				// too many triggers in 100mS
					if (trigthresh < 4095)
						trigthresh++;
				}
				if (n == 0) {		// no triggers in last 100mS
					if (trigthresh > 5)	// dont permit trigthresh < 5
						trigthresh--;
				}
				lastpretrigcnt = pretrigcnt;	// (dont worry about 2^32 wrap)
			}
		}
#endif

		/**********************  Every 1 Sec   *******************************/

		if (tenmstimer % 100 == 0) {		// every second

			char s[32];
			HAL_GPIO_TogglePin(GPIOD, LED_D2_Pin);
			//			strcpy(trigtimestr,"There goes the time");		// copy current time into trig time string

#if 1
			strcpy(str, ctime(&epochtime));
			str[strlen(str) - 1] = '\0';	// replace newline with terminator
			sprintf(nowtimestr, "\"%s\"", str);
#else
						sprintf(nowtimestr, "\"%u\"", epochtime);
			#endif
			sprintf(tempstr, "%d.%d", temperature, tempfrac);
			sprintf(pressstr, "%d.%d", pressure, pressfrac);
#if 1
			// construct detector status for webpage
			sprintf(statstr,
					"\"<b>Uptime</b> %d <b>secs<br><br>Last trigger</b> %s<br><br><b>Triggers</b> %d<br><br><b>Noise</b> %d<br><br><b>ADC Base</b> %d<br><br>\"",
					statuspkt.sysuptime, trigtimestr, statuspkt.trigcount, statuspkt.adcnoise, statuspkt.adcbase);

			if (gpslocked) {
				sprintf(gpsstr, "\"Locked: %d Sats<br>Lon: %d<br>Lat: %d\"", statuspkt.NavPvt.numSV,
						statuspkt.NavPvt.lon, statuspkt.NavPvt.lat);
			} else {
				strcpy(gpsstr, "\"<font color=red>**Lost GPS**<\/font>\"");
			}
#endif

#if 0
				if (agc) {
					calcagc();		// try to set the gain automatically
				}
#endif

			onesectimer++;

			//////////// generate web page data
#if 0
				if (xSemaphoreTake(ssicontentHandle,( TickType_t ) 100 ) == pdTRUE) {	// take the ssi generation semaphore (portMAX_DELAY == infinite)
					/*printf("We have the semaphore\n")*/;
				} else {
					printf("semaphore take failed\n");
				}
#endif
			while (!(xSemaphoreTake(ssicontentHandle, (TickType_t ) 25) == pdTRUE)) {// give the ssi generation semaphore (portMAX_DELAY == infinite)
				printf("sem wait 1\n");
			}
			{
				//printf("sem1 wait done\n");
			}

			if (xSemaphoreGive(ssicontentHandle) != pdTRUE) {	// give the ssi generation semaphore
				printf("semaphore release failed\n");
			}
		}		// if seconds timer hit

		/**********************  Every 10 Secs   *******************************/

		if (tenmstimer % 1000 == 0) {		// every 10 seconds

			/**********************  Every 30 Secs   *******************************/
			if (tenmstimer > 3000) {		// reset timer after 30 seconds
				tenmstimer = 0;
				if (gpsgood == 0) {	// gps is not talking to us
					printf("GPS serial comms problem?\n");
				}
				gpsgood = 0;			// reset the good flag

#ifdef SPLAT1
				if (psensor == MPL115A2) {
					if (getpressure115() != HAL_OK) {
						printf("MPL115A2 error\n\r");
					}
				} else if (psensor == MPL3115A2) {
					if (getpressure3115() != HAL_OK) {
						printf("MPL3115A2 error\n\r");
					}
				}

				printf("SN:%d/%lu %d:%d:%d:%d ", MY_UID, statuspkt.uid, myip & 0xFF, (myip & 0xFF00) >> 8,
						(myip & 0xFF0000) >> 16, (myip & 0xFF000000) >> 24);
				printf("triggers:%04d, pressure:%03d.%d, temp:%02d.%d, time:%s\n", trigs, pressure, pressfrac,
						temperature, tempfrac / 1000, nowtimestr);
#else
			printf("triggers=%04d,    ------------------------------------------- %s", trigs,ctime(&epochtime));
#endif
			} // end if 30 second timer

			if (onesectimer > 900) {			// 15 mins
				onesectimer = 0;
				if (locateudp() != uip)		// periodic check
					rebootme();	// target udp host has changed or network has gone away
#ifdef TESTING
				stats_display() ; // this needs stats in LwIP enabling to do anything
#endif
			}
		} // end 10 sec
	}
	/* USER CODE END StarLPTask */
}

/* Callback01 function */
void Callback01(void const *argument) {
	/* USER CODE BEGIN Callback01 */
	printf("Callback01\n");
	/* USER CODE END Callback01 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM12 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */
	if (htim->Instance == TIM2) {
		printf("T2P PeriodElapsedCallback %lu %lu\n", t2cap[0], statuspkt.clktrim);
		return;
	}
	if (htim->Instance == TIM3) {
		printf("T3 PeriodElapsedCallback\n");
		return;
	}

	if (htim->Instance == TIM6) { // 1 second (internally timed, not compensated by GPS)
//		printf("T6 PeriodElapsedCallback %u SR=%u\n", myfullcomplete, TIM6->SR);
		t1sec++;
		statuspkt.sysuptime++;
		if (netup)
			statuspkt.netuptime++;
		if (gpslocked) {
			statuspkt.gpsuptime++;
			/*	if (epochvalid == 0) */{
				statuspkt.epochsecs = calcepoch();
				epochvalid = 1;
			}
		} else {
			statuspkt.gpsuptime = 0;	// gps uptime is zero
			statuspkt.epochsecs = 0;	// make epoch time obviously wrong
			epochvalid = 0;
		}
		return;
	}

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM12) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */
	else {
		printf("Unknown Timer Period Elapsed callback\n");
	}
	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	while (1) {
		printf("HAL error (main.c 2343)\n");
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
