/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

#include "aeon.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_IN_Pin GPIO_PIN_0
#define BTN_IN_GPIO_Port GPIOA
#define DISP_RST_Pin GPIO_PIN_1
#define DISP_RST_GPIO_Port GPIOA
#define DISP_DC_Pin GPIO_PIN_2
#define DISP_DC_GPIO_Port GPIOA
#define DISP_BUSY_Pin GPIO_PIN_3
#define DISP_BUSY_GPIO_Port GPIOA
#define DISP_CS_Pin GPIO_PIN_4
#define DISP_CS_GPIO_Port GPIOA
#define SD_CS_Pin GPIO_PIN_0
#define SD_CS_GPIO_Port GPIOB
#define AUX_PWR_EN_Pin GPIO_PIN_8
#define AUX_PWR_EN_GPIO_Port GPIOA
#define FRAM_CS_Pin GPIO_PIN_9
#define FRAM_CS_GPIO_Port GPIOA
#define SW_OUT_Pin GPIO_PIN_10
#define SW_OUT_GPIO_Port GPIOA
#define SW_OUTA11_Pin GPIO_PIN_11
#define SW_OUTA11_GPIO_Port GPIOA
#define SW_OUTA12_Pin GPIO_PIN_12
#define SW_OUTA12_GPIO_Port GPIOA
#define SW_IN_Pin GPIO_PIN_15
#define SW_IN_GPIO_Port GPIOA
#define SW_INB4_Pin GPIO_PIN_4
#define SW_INB4_GPIO_Port GPIOB
#define SW_INB5_Pin GPIO_PIN_5
#define SW_INB5_GPIO_Port GPIOB
#define SW_INB6_Pin GPIO_PIN_6
#define SW_INB6_GPIO_Port GPIOB
#define DEBUG_LED_Pin GPIO_PIN_7
#define DEBUG_LED_GPIO_Port GPIOB
#define BATT_VDIV_EN_Pin GPIO_PIN_3
#define BATT_VDIV_EN_GPIO_Port GPIOH

/* USER CODE BEGIN Private defines */
extern SPI_HandleTypeDef hspi1;
extern RTC_HandleTypeDef hrtc;
extern ADC_HandleTypeDef hadc1;
extern RNG_HandleTypeDef hrng;

#define SD_SPI_HANDLE hspi1

#define DBG_SWO_EN true  // enable SWO debug output
#define DEBUG_MODE_FORCE_EN \
    false  // force debug mode (should only be enabled for testing, as full logs
           // will be written to SD card)

#define STR(x) #x

#define DBG (DBG_SWO_EN || runtime_debug_mode)

#define DISABLE_BATT_THRESHOLD_CHECK \
    false  // set to true to disable functionality where device relies on ADC
           // battery measurement to return to sleep when battery is low

#define BATT_LOGGING true  // enable battery logging to SD card

#define SET_DEBUG_LED(x)                                  \
    HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, \
                      (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define SET_AUX_PWR(x)                                      \
    HAL_GPIO_WritePin(AUX_PWR_EN_GPIO_Port, AUX_PWR_EN_Pin, \
                      (x) ? GPIO_PIN_RESET : GPIO_PIN_SET)

#define SET_DISP_CS(x)                                \
    HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, \
                      (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define SET_DISP_DC(x)                                \
    HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, \
                      (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define SET_DISP_RST(x)                                 \
    HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, \
                      (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GET_DISP_BUSY() HAL_GPIO_ReadPin(DISP_BUSY_GPIO_Port, DISP_BUSY_Pin)

#define GET_BTN_IN() HAL_GPIO_ReadPin(BTN_IN_GPIO_Port, BTN_IN_Pin)

extern enum wake_reason_t wake_reason;

extern bool runtime_debug_mode;  // when true, full log is written to SD card
                                 // is set true initially, will set false after
                                 // initialisation

extern uint32_t wake_cycle_count;

extern char debug_output_arr[20000];
extern int debug_output_arr_index;

extern uint32_t wake_cycle_count;
extern double batt_voltage;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
