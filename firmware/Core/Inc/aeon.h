#ifndef AEON_H
#define AEON_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"

#define BATT_THRESHOLD 3.65
#define SLEEP_DURATION_DEFAULT 60 * 60 * 18  // 18 hours

enum spi_device_t {
    AEON_SPI_NONE = 0,  // no device selected, all CS pins high
    AEON_SPI_DISP = 1,  // display
    AEON_SPI_SD = 2,    // SD card
    AEON_SPI_FRAM = 3,  // FRAM
    AEON_SPI_OFF = 4,   // SPI off, all CS pins low
};

enum wake_reason_t {
    WAKE_REASON_RESET = 0,  // first wake from power off (or reset btn pressed)
    WAKE_REASON_STBY_REFRESH_BTN = 1,  // wake from standby by refresh button
    WAKE_REASON_STBY_RTC = 2,          // wake from standby by RTC
};

enum sleep_reason_t {
    SLEEP_REASON_NULL = 0,
    SLEEP_REASON_FIRST_AFTER_REFRESH = 1,  // first sleep after display refresh
    SLEEP_REASON_NORM_ITER = 2,            // normal sleep iteration
    SLEEP_REASON_LOW_BATT = 3,             // low battery
    SLEEP_REASON_NO_IMAGE = 4,             // no image available
    SLEEP_REASON_NO_SD = 5,                // SD card not available
    SLEEP_REASON_REFRESH_DISABLED = 6,     // refresh disabled
};

extern const char* sleep_reason_t_str[];
extern const char* wake_reason_t_str[];

void spi_device_select(enum spi_device_t spi_device);

void set_aux_pwr(bool enable);

uint16_t get_toggle_sw_bits();

bool batt_threshold_check(double* batt_voltage_out);

uint32_t calculate_update_sleep_duration(uint32_t remaining_duration,
                                         uint16_t current_interval_sw_value);
void enter_sleep(int sleep_seconds);

bool check_debug_mode_en();

#endif  // AEON_H