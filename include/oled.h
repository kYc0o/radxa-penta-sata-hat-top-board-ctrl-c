/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "config.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_I2C_BUS 1
#define OLED_I2C_ADDR 0x3C

typedef enum {
    PAGE_SYSTEM,
    PAGE_RESOURCES,
    PAGE_DISKS,
    PAGE_RAID,
    PAGE_COUNT
} oled_page_t;

typedef struct {
    int initialized;
    int8_t i2c_bus;
    int8_t i2c_addr;
    int current_page;
    int auto_scroll;
    unsigned int scroll_interval;
    int rotate_180;
} oled_t;

int oled_init(oled_t *oled);
void oled_set_rotation(oled_t *oled, int rotate_180);
void oled_cleanup(oled_t *oled);
void oled_welcome(oled_t *oled);
void oled_goodbye(oled_t *oled);
void oled_show_page(oled_t *oled, oled_page_t page);
void oled_next_page(oled_t *oled);
void* oled_auto_scroll_thread(void *arg);

#endif // OLED_H
