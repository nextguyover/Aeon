/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>  //for va_list var arg functions
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "aeon.h"
#include "disp.h"
#include "fram.h"
#include "sd.h"
#include "slic.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

RNG_HandleTypeDef hrng;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

enum wake_reason_t wake_reason;

bool runtime_debug_mode = true;

uint32_t wake_cycle_count;
double batt_voltage;

#define DEBUG_OUTPUT_ARR_SIZE 20000
char debug_output_arr[DEBUG_OUTPUT_ARR_SIZE];
int debug_output_arr_index = 0;

FATFS FatFs;

FIL img_file_ptr;

#define PIXEL_BUF_SIZE 5000
uint8_t pixel_buf[PIXEL_BUF_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_RTC_Init(void);
static void MX_RNG_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int __io_putchar(int ch) {
    if ((runtime_debug_mode || DEBUG_MODE_FORCE_EN) &&
        debug_output_arr_index < DEBUG_OUTPUT_ARR_SIZE) {
        debug_output_arr[debug_output_arr_index++] = ch;
    }

    // Write character to ITM ch.0
    ITM_SendChar(ch);
    return (ch);
}

int img_slic_open_callback(const char *filename, SLICFILE *pFile) {
    FRESULT fres;

    fres = f_open(&img_file_ptr, filename, FA_READ);
    if (fres != FR_OK) {
        printf("f_open error (%i)\r\n", fres);
        return -1;
    }

    pFile->fHandle = &img_file_ptr;

    return 0;
}

int img_slic_read_callback(SLICFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    UINT bytesRead;
    FRESULT fres;

    fres = f_read(pFile->fHandle, pBuf, iLen, &bytesRead);
    if (fres != FR_OK) {
        printf("f_read error (%i)\r\n", fres);
        return -1;
    }

    return bytesRead;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    /* USER CODE BEGIN 1 */

    setvbuf(stdout, NULL, _IONBF, 0);  // disable line buffering for printf

    bool wakeup_by_refresh_btn = __HAL_PWR_GET_FLAG(
        PWR_FLAG_WUF1);  // check if wake up was caused by refresh button
    bool wakeup_by_rtc =
        __HAL_RTC_WAKEUPTIMER_EXTI_GET_FLAG();  // check if wake up was caused
                                                // by RTC

    /* USER CODE END 1 */

    /* MCU
     * Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the
     * Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_SPI1_Init();
    MX_FATFS_Init();
    MX_RTC_Init();
    MX_RNG_Init();
    /* USER CODE BEGIN 2 */

    // ================= Initialisation ================= //

    __HAL_PWR_CLEAR_FLAG(
        PWR_FLAG_WU);  // clear wake up flag:
                       // https://wiki.st.com/stm32mcu/wiki/Getting_started_with_PWR#Standby_mode
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();

    wake_reason = WAKE_REASON_RESET;

    if (wakeup_by_refresh_btn) {
        wake_reason = WAKE_REASON_STBY_REFRESH_BTN;
        if (DBG) printf("Wake from Standby (Refresh Btn)\n");
    } else if (wakeup_by_rtc) {
        wake_reason = WAKE_REASON_STBY_RTC;
        if (DBG) printf("Wake from Standby (RTC)\n");
    } else {
        if (DBG) printf("Wake from pwr off (Reset)\n");
    }

    SET_AUX_PWR(true);  // enable AUX power rail
    HAL_Delay(10);      // wait for AUX PWR to stabilise
    if (DBG) printf("Enabled AUX PWR\n");

    // check for previous unsafe shutdown, and set the flag to true
    bool previous_unsafe_shutdown = fram_sys_start_unsafe_shutdown_update();
    wake_cycle_count = fram_sys_wake_cycle_count_update();

    if (DBG) {
        printf("Starting iteration %lu\n", wake_cycle_count);
        printf("Previous shutdown was %s\n",
               previous_unsafe_shutdown ? "UNSAFE" : "safe");
    }

    bool sd_avail = sd_init(&FatFs);  // initialise SD card
    if (DBG && !sd_avail)
        printf("SD card not available, will be unable to refresh image\n");

    if (!batt_threshold_check(&batt_voltage)) {
        if (DBG)
            printf("Battery voltage is %.2fV, below threshold of %.2fV\n",
                   batt_voltage, BATT_THRESHOLD);
        fram_set_sleep_reason(SLEEP_REASON_LOW_BATT);
        enter_sleep(12 * 60 * 60);  // sleep for 12 hours
    }
    if (DBG)
        printf("Battery voltage is %.2fV, above threshold of %.2fV\n",
               batt_voltage, BATT_THRESHOLD);

    // check if BTN_IN is held down to enter debug mode
    if (!check_debug_mode_en()) {
        if (!previous_unsafe_shutdown) {
            runtime_debug_mode = false;
        } else {
            printf("Debug mode enabled due to unsafe shutdown!\n");
        }
    }

    // ================= Step 1 ================= //
    // Check whether to do an image refresh or go back to sleep.

    uint32_t remaining_sleep_duration = fram_get_sleep_duration();
    if (DBG)
        printf("Remaining sleep duration: %lu\n", remaining_sleep_duration);

    uint16_t previous_interval_value =
        fram_get_interval_sw_value();  // get previous interval value

    uint16_t current_toggle_sw_value =
        get_toggle_sw_bits();  // get current toggle switch value

    // extract config values from toggle switch value
    bool refresh_enabled = current_toggle_sw_value & 0x800;  // 12th bit
    bool shuffle_enabled = current_toggle_sw_value & 0x400;  // 11th bit
    uint16_t current_interval_value =
        current_toggle_sw_value & 0x3FF;  // interval value is the lower 10 bits
                                          // of the toggle switch value

    fram_set_interval_sw_value(current_interval_value);
    if (DBG)
        printf(
            "Previous interval switch value: %u, Current interval switch "
            "value: %u\n",
            previous_interval_value, current_interval_value);

    // Conditions to return to sleep
    // 1. Sleep duration not elapsed
    // 2. and Wake up not caused by refresh button
    // 3. and Interval switch value not changed
    if (remaining_sleep_duration != 0 && !wakeup_by_refresh_btn &&
        previous_interval_value == current_interval_value) {
        if (DBG)
            printf(
                "Sleep duration not elapsed... Conditions not met for refresh, "
                "going back to sleep.\n");

        uint32_t next_sleep_duration = calculate_update_sleep_duration(
            remaining_sleep_duration, current_interval_value);

        fram_set_sleep_reason(SLEEP_REASON_NORM_ITER);
        enter_sleep(next_sleep_duration);
    }

    // 4. Refresh not enabled (and not woken by refresh button)
    if (!refresh_enabled && !wakeup_by_refresh_btn) {
        if (DBG) printf("Refresh not enabled... Going back to sleep.\n");

        fram_set_sleep_reason(SLEEP_REASON_REFRESH_DISABLED);
        enter_sleep(12 * 60 * 60);  // sleep for 12 hours
    }

    // ================= Step 2 ================= //
    // If conditions are correct, we need to refresh the image, so next, need to
    // select the image to display.

    if (!sd_avail) {
        if (DBG)
            printf(
                "SD card not available to refresh image... Going back to "
                "sleep.\n");

        fram_set_sleep_reason(SLEEP_REASON_NO_SD);
        enter_sleep(12 * 60 * 60);  // sleep for 12 hours
    }

    char filename[41] = {0};
    bool image_available =
        sd_get_next_image_filename(filename, shuffle_enabled);

    if (!image_available) {
        if (DBG)
            printf("No image available to display... Going back to sleep.\n");

        fram_set_sleep_reason(SLEEP_REASON_NO_IMAGE);
        enter_sleep(12 * 60 * 60);  // sleep for 12 hours
    }

    f_chdir("/images");  // change to images directory

    if (DBG) printf("Opening image file: %s\n", filename);
    SLICSTATE slic_state;
    slic_init_decode(filename, &slic_state, NULL, 0, NULL,
                     img_slic_open_callback, img_slic_read_callback);

    // ================= Step 3 ================= //
    // Display the image

    spi_device_select(AEON_SPI_DISP);
    if (DBG) printf("Initialising display\n");
    disp_init();

    if (DBG) printf("Initialising display registers\n");
    disp_init_regs();

    if (DBG) printf("Clearing display\n");
    disp_clear(DISP_WHITE);

    if (DBG)
        printf("Transferring image data to disp (one dot is %i bytes) -> ",
               sizeof(pixel_buf));

    disp_send_command(0x10);

    int pix_buf_remaining = 0;

    int EDP_width = 400;
    int EDP_height = 480;
    for (int i = 0; i < EDP_height; i++) {
        for (int j = 0; j < EDP_width; j++) {
            if (pix_buf_remaining == 0) {
                spi_device_select(AEON_SPI_SD);

                slic_decode(&slic_state, (uint8_t *)&pixel_buf,
                            sizeof(pixel_buf));
                pix_buf_remaining = sizeof(pixel_buf);

                spi_device_select(AEON_SPI_DISP);

                printf(".");
            }

            uint8_t color = pixel_buf[PIXEL_BUF_SIZE - pix_buf_remaining];

            disp_send_data((color << 4) | color);
            pix_buf_remaining--;
        }
    }

    if (DBG) printf("\nCompleted image transfer\n");

    if (DBG) printf("Turning on display\n");
    disp_turn_on();

    spi_device_select(AEON_SPI_SD);

    f_close(&img_file_ptr);

    spi_device_select(AEON_SPI_DISP);

    if (DBG) printf("Display to sleep and shutdown\n");
    disp_sleep();  // put display to sleep
    disp_exit();

    // ================= Step 4 ================= //
    // Finalise stored data and go back to sleep

    uint32_t next_sleep_duration =
        calculate_update_sleep_duration(0, current_interval_value);

    fram_set_sleep_reason(SLEEP_REASON_FIRST_AFTER_REFRESH);
    enter_sleep(next_sleep_duration);

    /* USER CODE END 2 */

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
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) !=
        HAL_OK) {
        Error_Handler();
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI |
                                       RCC_OSCILLATORTYPE_LSI |
                                       RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {
    /* USER CODE BEGIN ADC1_Init 0 */

    /* USER CODE END ADC1_Init 0 */

    ADC_MultiModeTypeDef multimode = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */

    /** Common config
     */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /** Configure the ADC multi-mode
     */
    multimode.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel = ADC_CHANNEL_16;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN ADC1_Init 2 */

    /* USER CODE END ADC1_Init 2 */
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
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {
    /* USER CODE BEGIN RTC_Init 0 */

    /* USER CODE END RTC_Init 0 */

    /* USER CODE BEGIN RTC_Init 1 */

    /* USER CODE END RTC_Init 1 */

    /** Initialize RTC Only
     */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN RTC_Init 2 */

    /* USER CODE END RTC_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {
    /* USER CODE BEGIN SPI1_Init 0 */

    /* USER CODE END SPI1_Init 0 */

    /* USER CODE BEGIN SPI1_Init 1 */

    /* USER CODE END SPI1_Init 1 */
    /* SPI1 parameter configuration*/
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN SPI1_Init 2 */

    /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(
        GPIOA,
        DISP_RST_Pin | DISP_DC_Pin | SW_OUT_Pin | SW_OUTA11_Pin | SW_OUTA12_Pin,
        GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, DISP_CS_Pin | AUX_PWR_EN_Pin | FRAM_CS_Pin,
                      GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(BATT_VDIV_EN_GPIO_Port, BATT_VDIV_EN_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : BTN_IN_Pin DISP_BUSY_Pin */
    GPIO_InitStruct.Pin = BTN_IN_Pin | DISP_BUSY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : DISP_RST_Pin DISP_DC_Pin AUX_PWR_EN_Pin */
    GPIO_InitStruct.Pin = DISP_RST_Pin | DISP_DC_Pin | AUX_PWR_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : DISP_CS_Pin FRAM_CS_Pin */
    GPIO_InitStruct.Pin = DISP_CS_Pin | FRAM_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin : SD_CS_Pin */
    GPIO_InitStruct.Pin = SD_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : SW_OUT_Pin SW_OUTA11_Pin SW_OUTA12_Pin */
    GPIO_InitStruct.Pin = SW_OUT_Pin | SW_OUTA11_Pin | SW_OUTA12_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin : SW_IN_Pin */
    GPIO_InitStruct.Pin = SW_IN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SW_IN_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : SW_INB4_Pin SW_INB5_Pin SW_INB6_Pin */
    GPIO_InitStruct.Pin = SW_INB4_Pin | SW_INB5_Pin | SW_INB6_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : DEBUG_LED_Pin */
    GPIO_InitStruct.Pin = DEBUG_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DEBUG_LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : BATT_VDIV_EN_Pin */
    GPIO_InitStruct.Pin = BATT_VDIV_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BATT_VDIV_EN_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state
     */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
