/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#ifndef THERMAL_H
#define THERMAL_H

#include "config.h"
#include <time.h>

#define THERMAL_ZONE_PATH "/sys/class/thermal/thermal_zone0/temp"
#define SMARTCTL_CMD "smartctl -A /dev/%s 2>/dev/null"
#define SSD_TEMP_CACHE_SEC 5  // Only read SSD temps every 5 seconds

// Temperature history for moving average and trend analysis
#define TEMP_HISTORY_SIZE 10
// The values above are now tunable via config/env; defaults are initialized in config.c
// and consumed in thermal.c through cfg->thermal.*

typedef struct {
    double cpu_temps[TEMP_HISTORY_SIZE];
    int ssd_temps[TEMP_HISTORY_SIZE];
    int history_index;
    int history_count;
    double last_duty_cycle;
    double last_cpu_temp;
    int last_ssd_temp;
    int stable_cycles;  // Count of cycles at same duty cycle
    time_t hold_until;  // Do not decrease duty while now < hold_until
} thermal_state_t;

double thermal_read_cpu_temp(void);
int thermal_read_ssd_temps(int *temps, size_t max_count);
double thermal_calculate_duty_cycle(config_t *cfg);
double thermal_calculate_duty_cycle_smart(config_t *cfg, thermal_state_t *state);
void thermal_state_init(thermal_state_t *state);

#endif // THERMAL_H
