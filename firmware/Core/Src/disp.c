#include <stdint.h>
#include <stdio.h>

#include "aeon.h"
#include "main.h"
#include "stm32l4xx_hal.h"

// extern SPI_HandleTypeDef hspi1;

static void disp_write_byte(uint8_t value) {
    HAL_SPI_Transmit(&hspi1, &value, 1, 1000);
}

void disp_send_command(uint8_t reg) {
    SET_DISP_DC(0);
    SET_DISP_CS(0);
    disp_write_byte(reg);
    SET_DISP_CS(1);
}

void disp_send_data(uint8_t data) {
    SET_DISP_DC(1);
    SET_DISP_CS(0);
    disp_write_byte(data);
    SET_DISP_CS(1);
}

void disp_init() {
    SET_DISP_DC(0);
    spi_device_select(AEON_SPI_DISP);
    SET_DISP_RST(1);
}

static void disp_reset(void) {
    SET_DISP_RST(1);
    HAL_Delay(20);
    SET_DISP_RST(0);
    HAL_Delay(2);
    SET_DISP_RST(1);
    HAL_Delay(20);
}

static void disp_wait_busy(void) {
    int tick = HAL_GetTick();

    while (!GET_DISP_BUSY()) {  // LOW: busy, HIGH: idle
        HAL_Delay(1);
        if (HAL_GetTick() - tick > 60000) {
            if (DBG) printf("ERROR: Display busy timeout\n");
            break;
        }
    }
}

void disp_init_regs(void) {
    disp_reset();
    disp_wait_busy();
    HAL_Delay(30);

    disp_send_command(0xAA);  // CMDH
    disp_send_data(0x49);
    disp_send_data(0x55);
    disp_send_data(0x20);
    disp_send_data(0x08);
    disp_send_data(0x09);
    disp_send_data(0x18);

    disp_send_command(0x01);
    disp_send_data(0x3F);
    disp_send_data(0x00);
    disp_send_data(0x32);
    disp_send_data(0x2A);
    disp_send_data(0x0E);
    disp_send_data(0x2A);

    disp_send_command(0x00);
    disp_send_data(0x5F);
    disp_send_data(0x69);

    disp_send_command(0x03);
    disp_send_data(0x00);
    disp_send_data(0x54);
    disp_send_data(0x00);
    disp_send_data(0x44);

    disp_send_command(0x05);
    disp_send_data(0x40);
    disp_send_data(0x1F);
    disp_send_data(0x1F);
    disp_send_data(0x2C);

    disp_send_command(0x06);
    disp_send_data(0x6F);
    disp_send_data(0x1F);
    disp_send_data(0x1F);
    disp_send_data(0x22);

    disp_send_command(0x08);
    disp_send_data(0x6F);
    disp_send_data(0x1F);
    disp_send_data(0x1F);
    disp_send_data(0x22);

    disp_send_command(0x13);  // IPC
    disp_send_data(0x00);
    disp_send_data(0x04);

    disp_send_command(0x30);
    disp_send_data(0x3C);

    disp_send_command(0x41);  // TSE
    disp_send_data(0x00);

    disp_send_command(0x50);
    disp_send_data(0x3F);

    disp_send_command(0x60);
    disp_send_data(0x02);
    disp_send_data(0x00);

    disp_send_command(0x61);
    disp_send_data(0x03);
    disp_send_data(0x20);
    disp_send_data(0x01);
    disp_send_data(0xE0);

    disp_send_command(0x82);
    disp_send_data(0x1E);

    disp_send_command(0x84);
    disp_send_data(0x00);

    disp_send_command(0x86);  // AGID
    disp_send_data(0x00);

    disp_send_command(0xE3);
    disp_send_data(0x2F);

    disp_send_command(0xE0);  // CCSET
    disp_send_data(0x00);

    disp_send_command(0xE6);  // TSSET
    disp_send_data(0x00);
}

void disp_turn_on() {
    disp_send_command(0x04);  // POWER_ON
    disp_wait_busy();

    disp_send_command(0x12);  // DISPLAY_REFRESH
    disp_send_data(0x00);
    disp_wait_busy();

    disp_send_command(0x02);  // POWER_OFF
    disp_send_data(0X00);
    disp_wait_busy();
}

void disp_clear(uint8_t color) {
    int width = 400;
    int height = 480;

    disp_send_command(0x10);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            disp_send_data((color << 4) | color);
        }
    }

    disp_turn_on();
}

void disp_sleep(void) {
    disp_send_command(0x07);  // DEEP_SLEEP
    disp_send_data(0XA5);
    HAL_Delay(2000);
}

void disp_exit() {
    SET_DISP_DC(0);
    spi_device_select(AEON_SPI_NONE);
    SET_DISP_RST(0);
}
