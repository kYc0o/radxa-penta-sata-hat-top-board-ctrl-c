/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#ifndef BUTTON_H
#define BUTTON_H

#include "oled.h"

typedef struct {
    int gpio_chip;
    unsigned int gpio_line;
    int initialized;
    struct gpiod_chip *chip;
    struct gpiod_line_request *request;
    oled_t *oled;  // Reference to OLED for page changes
} button_t;

int button_init(button_t *button, int gpio_chip, unsigned int gpio_line, oled_t *oled);
void button_cleanup(button_t *button);
void* button_watch_thread(void *arg);

#endif // BUTTON_H
