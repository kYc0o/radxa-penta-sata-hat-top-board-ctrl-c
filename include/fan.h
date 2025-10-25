/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#ifndef FAN_H
#define FAN_H

#include <gpiod.h>
#include "config.h"

#define PWM_PERIOD_US 40
#define GPIO_PERIOD_S 0.002f  // 2ms = 500 Hz (low PWM fan frequency)

typedef struct {
    int use_hardware_pwm;
    int pwm_chip;
    int gpio_chip;
    unsigned int gpio_line;
    
    // For hardware PWM
    char pwm_path[256];
    int pwm_period_ns;
    
    // For software PWM (GPIO)
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    double period_s;
    double duty_cycle;
    int running;
} fan_t;

int fan_init(fan_t *fan);
int fan_set_duty_cycle(fan_t *fan, double duty);
void fan_cleanup(fan_t *fan);
void* fan_control_loop(void *arg);

#endif // FAN_H
