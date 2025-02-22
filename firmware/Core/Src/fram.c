#include "fram.h"

#include <stdint.h>

#include "aeon.h"
#include "main.h"
#include "stm32l4xx_hal.h"

#define FRAM_IMG_COUNTER_ADDR 0
#define FRAM_IMG_COUNTER_SIZE 4

#define FRAM_SLEEP_DURATION_ADDR 8
#define FRAM_SLEEP_DURATION_SIZE 4

#define FRAM_WAKE_CYCLE_COUNT_ADDR 16
#define FRAM_WAKE_CYCLE_COUNT_SIZE 4

#define FRAM_INTERVAL_SW_ADDR 24
#define FRAM_INTERVAL_SW_SIZE 2

#define FRAM_STATUS_BYTE_ADDR 26

/**
 * @brief Write bytes to FRAM at the specified address.
 *
 * @param buffer: pointer to the data buffer to be written
 * @param address: address in FRAM to write to (0 - 511)
 * @param size: number of bytes to write
 */
bool fram_write_bytes(uint8_t* buffer, uint16_t address, int size) {
    spi_device_select(AEON_SPI_FRAM);

    uint8_t txBuffer[2];

    txBuffer[0] = 0x06;
    if (HAL_SPI_Transmit(&hspi1, txBuffer, 1, TIMEOUT_EEPROM) != HAL_OK) {
        return false;
    }  // write enable

    spi_device_select(AEON_SPI_NONE);
    spi_device_select(AEON_SPI_FRAM);

    txBuffer[0] = 0x02 | ((address & 0x100) >> 5);  // write command and address
    txBuffer[1] = (address & 0xFF);
    if (HAL_SPI_Transmit(&hspi1, txBuffer, 2, TIMEOUT_EEPROM) != HAL_OK) {
        return false;
    }

    if (HAL_SPI_Transmit(&hspi1, buffer, size, TIMEOUT_EEPROM) != HAL_OK) {
        return false;
    }  // write data

    spi_device_select(AEON_SPI_NONE);

    // HAL_Delay(1000);

    return true;
}

/**
 * @brief Read bytes from FRAM at the specified address.
 *
 * @param buffer: pointer to the buffer to store the read data
 * @param address: address in FRAM to read from (0 - 511)
 * @param size: number of bytes to read
 */
bool fram_read_bytes(uint8_t* buffer, uint16_t address, int size) {
    spi_device_select(AEON_SPI_FRAM);

    uint8_t txBuffer[2] = {0x03 | ((address & 0x100) >> 5), (address & 0xFF)};
    if (HAL_SPI_Transmit(&hspi1, txBuffer, 2, TIMEOUT_EEPROM) != HAL_OK) {
        return false;
    }  // write command and address

    if (HAL_SPI_Receive(&hspi1, buffer, size, TIMEOUT_EEPROM) != HAL_OK) {
        return false;
    }  // read data

    spi_device_select(AEON_SPI_NONE);

    return true;
}

/**
 * @brief Write image counter to FRAM.
 *
 * @param counter: image counter value to write
 */
void fram_set_image_counter(uint32_t counter) {
    fram_write_bytes((uint8_t*)&counter, FRAM_IMG_COUNTER_ADDR,
                     FRAM_IMG_COUNTER_SIZE);
}

/**
 * @brief Read image counter from FRAM and return value.
 */
uint32_t fram_get_image_counter() {
    uint32_t image_counter;
    fram_read_bytes((uint8_t*)&image_counter, FRAM_IMG_COUNTER_ADDR,
                    FRAM_IMG_COUNTER_SIZE);
    return image_counter;
}

/**
 * @brief Write interval switch value to FRAM.
 *
 * @param value: interval switch value to write
 */
void fram_set_interval_sw_value(uint16_t value) {
    fram_write_bytes((uint8_t*)&value, FRAM_INTERVAL_SW_ADDR,
                     FRAM_INTERVAL_SW_SIZE);
}

/**
 * @brief Read interval switch value from FRAM and return value.
 */
uint16_t fram_get_interval_sw_value() {
    uint16_t value;
    fram_read_bytes((uint8_t*)&value, FRAM_INTERVAL_SW_ADDR,
                    FRAM_INTERVAL_SW_SIZE);
    return value;
}

/**
 * @brief Write remaining sleep iterations to FRAM.
 *
 * @param remaining_iterations: remaining sleep iterations to write
 */
void fram_set_sleep_duration(uint32_t remaining_iterations) {
    fram_write_bytes((uint8_t*)&remaining_iterations, FRAM_SLEEP_DURATION_ADDR,
                     FRAM_SLEEP_DURATION_SIZE);
}

/**
 * @brief Read remaining sleep iterations from FRAM and return value.
 */
uint32_t fram_get_sleep_duration() {
    uint32_t remaining_iterations;
    fram_read_bytes((uint8_t*)&remaining_iterations, FRAM_SLEEP_DURATION_ADDR,
                    FRAM_SLEEP_DURATION_SIZE);
    return remaining_iterations;
}

/**
 * @brief Get the status byte from FRAM.
 */
uint8_t fram_get_status_byte() {
    uint8_t status_byte = 0;
    fram_read_bytes((uint8_t*)&status_byte, FRAM_STATUS_BYTE_ADDR, 1);
    return status_byte;
}

/**
 * @brief Set the status byte in FRAM.
 *
 * @param status_byte: status byte to write
 */
void fram_set_status_byte(uint8_t status_byte) {
    fram_write_bytes((uint8_t*)&status_byte, FRAM_STATUS_BYTE_ADDR, 1);
}

/**
 * @brief Set the unsafe shutdown flag in the status byte.
 *
 * @param unsafe_shutdown: true to set the flag, false to clear it
 */
void fram_set_unsafe_shutdown(bool unsafe_shutdown) {
    uint8_t status_byte = fram_get_status_byte();
    status_byte = unsafe_shutdown ? status_byte | 0x01 : status_byte & 0xFE;
    fram_set_status_byte(status_byte);
}

/**
 * @brief Get the unsafe shutdown flag from the status byte, and set stored
 * value to true.
 */
bool fram_sys_start_unsafe_shutdown_update() {
    uint8_t status_byte = fram_get_status_byte();
    bool prev_unsafe_shutdown = status_byte & 0x01;
    fram_set_status_byte(status_byte | 0x01);

    return prev_unsafe_shutdown;
}

// bool fram_get_unsafe_shutdown(){
//     uint8_t status_byte = fram_get_status_byte();
//     return status_byte & 0x01;
// }

/**
 * @brief Set the sleep reason in the status byte.
 *
 * @param reason: sleep reason to write
 */
void fram_set_sleep_reason(enum sleep_reason_t reason) {
    uint8_t status_byte = fram_get_status_byte() & 0b11110001;
    status_byte = (status_byte & 0b00001110) | (reason << 1);
    fram_set_status_byte(status_byte);
}

/**
 * @brief Get the sleep reason from the status byte.
 */
enum sleep_reason_t fram_get_sleep_reason() {
    uint8_t status_byte = fram_get_status_byte();
    return (status_byte & 0b00001110) >> 1;
}

/**
 * Get the wake cycle count from FRAM, store incremented value and return the
 * value for current iteration.
 */
uint32_t fram_sys_wake_cycle_count_update() {
    uint32_t wake_cycle_iteration;

    fram_read_bytes((uint8_t*)&wake_cycle_iteration, FRAM_WAKE_CYCLE_COUNT_ADDR,
                    FRAM_WAKE_CYCLE_COUNT_SIZE);

    uint32_t new_wake_cycle_iteration = wake_cycle_iteration + 1;
    fram_write_bytes((uint8_t*)&new_wake_cycle_iteration,
                     FRAM_WAKE_CYCLE_COUNT_ADDR, FRAM_WAKE_CYCLE_COUNT_SIZE);

    return wake_cycle_iteration;
}