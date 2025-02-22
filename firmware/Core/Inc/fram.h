#ifndef FRAM_H
#define FRAM_H

#define TIMEOUT_EEPROM 100

#include <stdbool.h>
#include <stdint.h>

#include "aeon.h"
#include "stm32l4xx_hal.h"


void fram_set_image_counter(uint32_t counter);
uint32_t fram_get_image_counter();

void fram_set_interval_sw_value(uint16_t value);
uint16_t fram_get_interval_sw_value();

void fram_set_sleep_duration(uint32_t remaining_iterations);
uint32_t fram_get_sleep_duration();

void fram_set_unsafe_shutdown(bool unsafe_shutdown);
bool fram_sys_start_unsafe_shutdown_update();

void fram_set_sleep_reason(enum sleep_reason_t reason);
enum sleep_reason_t fram_get_sleep_reason();

uint32_t fram_sys_wake_cycle_count_update();

#endif  // FRAM_H