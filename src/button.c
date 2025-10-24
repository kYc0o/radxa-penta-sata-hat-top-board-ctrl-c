/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>
#include "button.h"
#include "oled.h"

#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_POLL_MS 100

int button_init(button_t *button, int gpio_chip, unsigned int gpio_line, oled_t *oled) {
    memset(button, 0, sizeof(button_t));
    
    button->gpio_chip = gpio_chip;
    button->gpio_line = gpio_line;
    button->oled = oled;
    
    // Open GPIO chip
    char chip_path[32];
    snprintf(chip_path, sizeof(chip_path), "/dev/gpiochip%d", gpio_chip);
    button->chip = gpiod_chip_open(chip_path);
    if (!button->chip) {
        fprintf(stderr, "Failed to open GPIO chip %d for button\n", gpio_chip);
        return -1;
    }
    
    // Configure line settings (input with pull-up)
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) {
        fprintf(stderr, "Failed to create line settings\n");
        gpiod_chip_close(button->chip);
        return -1;
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    
    // Configure line config
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        fprintf(stderr, "Failed to create line config\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(button->chip);
        return -1;
    }
    
    unsigned int offsets[] = {gpio_line};
    gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);
    
    // Request the line
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "radxa-penta-fan-ctrl-button");
    
    button->request = gpiod_chip_request_lines(button->chip, req_cfg, line_cfg);
    
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    
    if (!button->request) {
        fprintf(stderr, "Failed to request GPIO line for button\n");
        gpiod_chip_close(button->chip);
        return -1;
    }
    
    button->initialized = 1;
    printf("Button initialized (GPIO chip %d, line %d)\n", gpio_chip, gpio_line);
    
    return 0;
}

void button_cleanup(button_t *button) {
    if (button->initialized) {
        if (button->request) {
            gpiod_line_request_release(button->request);
        }
        if (button->chip) {
            gpiod_chip_close(button->chip);
        }
        button->initialized = 0;
    }
}

void* button_watch_thread(void *arg) {
    button_t *button = (button_t *)arg;
    int last_value = 1;  // Pull-up means default is HIGH (1)
    
    printf("Button watch thread started\n");
    
    while (button->initialized) {
        // Read button value
        enum gpiod_line_value value = gpiod_line_request_get_value(
            button->request, 
            button->gpio_line
        );
        
        int current_value = (value == GPIOD_LINE_VALUE_ACTIVE) ? 0 : 1;
        
        // Detect button press (transition from HIGH to LOW)
        if (last_value == 1 && current_value == 0) {
            printf("Button pressed! Advancing to next page\n");
            if (button->oled && button->oled->initialized) {
                button->oled->current_page = (button->oled->current_page + 1) % PAGE_COUNT;
                printf("Switched to page %d\n", button->oled->current_page);
                // Immediately update the display with the new page
                oled_show_page(button->oled, (oled_page_t)button->oled->current_page);
            }
            
            // Wait for button release with debouncing
            // Keep reading until button is released (returns to HIGH)
            do {
                usleep((unsigned int)(BUTTON_POLL_MS * 1000));
                value = gpiod_line_request_get_value(button->request, button->gpio_line);
                current_value = (value == GPIOD_LINE_VALUE_ACTIVE) ? 0 : 1;
            } while (current_value == 0 && button->initialized);
            
            // Additional debounce delay after release to ensure stable state
            usleep((unsigned int)(BUTTON_DEBOUNCE_MS * 1000));
        }
        
        last_value = current_value;
        usleep((unsigned int)(BUTTON_POLL_MS * 1000));
    }
    
    printf("Button watch thread stopped\n");
    return NULL;
}
