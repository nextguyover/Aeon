
#include "aeon.h"

#include <stdio.h>

#include "fram.h"
#include "main.h"
#include "sd.h"
#include "stm32l4xx_hal.h"

const char *wake_reason_t_str[] = {"WAKE_REASON_RESET",
                                   "WAKE_REASON_STBY_REFRESH_BTN",
                                   "WAKE_REASON_STBY_RTC"};

const char *sleep_reason_t_str[] = {"SLEEP_REASON_NULL",
                                    "SLEEP_REASON_FIRST_AFTER_REFRESH",
                                    "SLEEP_REASON_NORM_ITER",
                                    "SLEEP_REASON_LOW_BATT",
                                    "SLEEP_REASON_NO_IMAGE",
                                    "SLEEP_REASON_NO_SD",
                                    "SLEEP_REASON_REFRESH_DISABLED"};

/**
 * @brief Select the SPI device to communicate with.
 *
 * @param spi_device SPI device to select
 */
void spi_device_select(enum spi_device_t spi_device) {
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin,
                      spi_device == AEON_SPI_SD || spi_device == AEON_SPI_OFF
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin,
                      spi_device == AEON_SPI_DISP || spi_device == AEON_SPI_OFF
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(FRAM_CS_GPIO_Port, FRAM_CS_Pin,
                      spi_device == AEON_SPI_FRAM || spi_device == AEON_SPI_OFF
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
}

void set_aux_pwr(bool enable) {
    HAL_GPIO_WritePin(AUX_PWR_EN_GPIO_Port, AUX_PWR_EN_Pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * PA12, PA11, PA10 should be set to 1 one at at time.
 *
 * PB6, PB5, PB4, PA15 are read each time that one of the above are enabled.
 *
 * When PA12 is enabled, PB6, PB5, PB4, PA15 values correspond to the bits 11,
 * 10, 9, 8 of the toggle switch. When PA11 is enabled, PB6, PB5, PB4, PA15
 * values correspond to the bits 7, 6, 5, 4 of the toggle switch. When PA10 is
 * enabled, PB6, PB5, PB4, PA15 values correspond to the bits 3, 2, 1, 0 of the
 * toggle switch.
 */
uint16_t get_toggle_sw_bits() {
    uint16_t bits = 0;

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_Delay(1);

    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) << 11;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) << 10;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) << 9;
    bits |= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) << 8;

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_Delay(1);

    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) << 7;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) << 6;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) << 5;
    bits |= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) << 4;

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_Delay(1);

    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) << 3;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) << 2;
    bits |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) << 1;
    bits |= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) << 0;

    bits = (~bits) & 0x0FFF;

    return bits;
}

/**
 * Enable VDIV switch and read the battery voltage with the ADC.
 */
bool batt_threshold_check(double *batt_voltage_out) {
    HAL_GPIO_WritePin(BATT_VDIV_EN_GPIO_Port, BATT_VDIV_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint32_t adcValue = HAL_ADC_GetValue(&hadc1);
    double batt_voltage = (double)adcValue / 4095.0 *
                          6.14;  // this value has to be manually calibrated
    HAL_ADC_Stop(&hadc1);

    HAL_GPIO_WritePin(BATT_VDIV_EN_GPIO_Port, BATT_VDIV_EN_Pin, GPIO_PIN_RESET);

    if (batt_voltage_out != NULL) {
        *batt_voltage_out = batt_voltage;
    }

    return batt_voltage > BATT_THRESHOLD;
}

bool check_debug_mode_en() {
    if (GET_BTN_IN() == GPIO_PIN_SET) {
        SET_DEBUG_LED(true);
        HAL_Delay(1000);
        if (GET_BTN_IN() == GPIO_PIN_SET) {
            for (int i = 0; i < 10; i++) {
                SET_DEBUG_LED(true);
                HAL_Delay(50);
                SET_DEBUG_LED(false);
                HAL_Delay(50);
            }
            printf("Debug mode enabled due to button press!\n");
            return true;
        }
        SET_DEBUG_LED(false);
    }
    return false;
}

uint32_t calculate_update_sleep_duration(uint32_t remaining_duration,
                                         uint16_t current_interval_sw_value) {
    uint32_t new_duration;

    // if remaining duration is not 0, sleep for the remaining duration or the
    // default duration, whichever is smaller
    if (remaining_duration != 0) {
        if (remaining_duration > SLEEP_DURATION_DEFAULT) {
            fram_set_sleep_duration(remaining_duration -
                                    SLEEP_DURATION_DEFAULT);
            new_duration = SLEEP_DURATION_DEFAULT;
        } else {
            fram_set_sleep_duration(0);
            new_duration = remaining_duration;
        }
        return new_duration;
    }

    // else, calculate sleep duration based on the interval switch value
    // convert toggle switch value directly to hours (TODO: consider changing
    // this)
    current_interval_sw_value =
        current_interval_sw_value == 0
            ? 24
            : current_interval_sw_value;  // if 0, set to 24 hours
    new_duration =
        current_interval_sw_value * 60 * 60;  // convert hours to seconds
    if (new_duration > SLEEP_DURATION_DEFAULT) {
        fram_set_sleep_duration(new_duration - SLEEP_DURATION_DEFAULT);
        new_duration = SLEEP_DURATION_DEFAULT;
    } else {
        fram_set_sleep_duration(0);
    }
    return new_duration;
}

/**
 * Enter sleep mode.
 */
void enter_sleep(int sleep_seconds) {
    if (DBG) {
        printf("Sleeping for %d seconds...\n", sleep_seconds);
    }

    spi_device_select(AEON_SPI_SD);
    if (BATT_LOGGING)
        sd_append_batt_charge_log(sleep_seconds);  // append battery charge log
                                                   // to SD card
    if (runtime_debug_mode || DEBUG_MODE_FORCE_EN)
        sd_write_logfile();  // write debug log to SD card
    sd_close();              // unmount SD card

    fram_set_unsafe_shutdown(false);  // clear unsafe shutdown flag

    SET_AUX_PWR(false);               // disable AUX PWR
    spi_device_select(AEON_SPI_OFF);  // disable all CS lines to prevent
                                      // powering AUX via CS pull-up resistors

    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, sleep_seconds,
                                RTC_WAKEUPCLOCK_CK_SPRE_16BITS,
                                0);  // set wake up time

    // HAL_PWREx_EnablePullUpPullDownConfig(); // enable pull-ups
    // enabling internal pullups actually uses about 0.5uA more current, so not
    // using it

    // HAL_PWREx_EnableGPIOPullUp(AUX_PWR_EN_GPIO_Port, AUX_PWR_EN_Pin); //
    // enable pull-up on AUX PWR EN pin

    // Disable wake-up pin first to clear any pending wake-up event
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
    
    // Clear all wake-up flags before enabling wake-up pin
    // This prevents immediate re-wake if button is still pressed
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);

    HAL_PWR_EnterSTANDBYMode();  // go to low power standby mode

    printf("Should not reach here, something has gone wrong...\n");

    while (true) {
        SET_DEBUG_LED(true);
        HAL_Delay(200);
        SET_DEBUG_LED(false);
        HAL_Delay(200);

        HAL_PWR_EnterSTANDBYMode();
    }
}
