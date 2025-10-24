/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <gpiod.h>
#include <math.h>
#include "fan.h"

// Check gpiod version
#ifndef GPIOD_API_VERSION
#define GPIOD_API_VERSION 1
#endif

static void* gpio_pwm_thread(void *arg);

int fan_init(fan_t *fan) {
    memset(fan, 0, sizeof(fan_t));
    
    // Read environment variables
    const char *hwpwm = getenv("HARDWARE_PWM");
    const char *pwmchip = getenv("PWMCHIP");
    const char *fan_chip = getenv("FAN_CHIP");
    const char *fan_line = getenv("FAN_LINE");
    const char *dbg = getenv("RADXA_DEBUG");
    int debug_verbose = (dbg && strcmp(dbg, "2") == 0);
    
    fan->use_hardware_pwm = (hwpwm && strcmp(hwpwm, "1") == 0);
    fan->pwm_chip = pwmchip ? atoi(pwmchip) : 0;
    fan->gpio_chip = fan_chip ? atoi(fan_chip) : 0;
    fan->gpio_line = fan_line ? (unsigned int)strtoul(fan_line, NULL, 10) : 27u;
    fan->period_s = GPIO_PERIOD_S;
    fan->duty_cycle = 0.0;
    fan->running = 1;
    
    if (fan->use_hardware_pwm) {
        // Hardware PWM setup
        snprintf(fan->pwm_path, sizeof(fan->pwm_path), 
                 "/sys/class/pwm/pwmchip%d/pwm0", fan->pwm_chip);
        
        char export_path[256];
        snprintf(export_path, sizeof(export_path), 
                 "/sys/class/pwm/pwmchip%d/export", fan->pwm_chip);
        
        // Try to export PWM
        FILE *fp = fopen(export_path, "w");
        if (fp) {
            fprintf(fp, "0");
            fclose(fp);
        }
        
        // Set period
        char period_path[300];
        snprintf(period_path, sizeof(period_path), "%s/period", fan->pwm_path);
        fp = fopen(period_path, "r");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open PWM period file\n");
            return -1;
        }
        fan->pwm_period_ns = PWM_PERIOD_US * 1000;
        fprintf(fp, "%d", fan->pwm_period_ns);
        fclose(fp);
        
        // Enable PWM
        char enable_path[300];
        snprintf(enable_path, sizeof(enable_path), "%s/enable", fan->pwm_path);
        fp = fopen(enable_path, "w");
        if (fp) {
            fprintf(fp, "1");
            fclose(fp);
        }
        
        printf("Fan initialized with hardware PWM (pwmchip%d, period %dus)\n", 
               fan->pwm_chip, PWM_PERIOD_US);
        if (debug_verbose) {
            char dbg_period_path[300];
            snprintf(dbg_period_path, sizeof(dbg_period_path), "%s/period", fan->pwm_path);
            FILE *pf = fopen(dbg_period_path, "r");
            long cur_period = -1;
            if (pf) { if (fscanf(pf, "%ld", &cur_period) == 1) {} fclose(pf); }
            char dbg_enable_path[300];
            snprintf(dbg_enable_path, sizeof(dbg_enable_path), "%s/enable", fan->pwm_path);
            pf = fopen(dbg_enable_path, "r");
            int enabled = -1; if (pf) { if (fscanf(pf, "%d", &enabled) == 1) {} fclose(pf); }
            printf("[DEBUG][PWM/HW] path=%s period_ns=%ld enabled=%d\n", fan->pwm_path, cur_period, enabled);
        }
    } else {
        // GPIO software PWM setup
        char chip_path[64];
        snprintf(chip_path, sizeof(chip_path), "/dev/gpiochip%d", fan->gpio_chip);
        
        fan->chip = gpiod_chip_open(chip_path);
        if (!fan->chip) {
            fprintf(stderr, "Error: Cannot open GPIO chip %s\n", chip_path);
            return -1;
        }
        
        // gpiod 2.x API
        struct gpiod_line_settings *settings = gpiod_line_settings_new();
        if (!settings) {
            fprintf(stderr, "Error: Cannot create line settings\n");
            gpiod_chip_close(fan->chip);
            return -1;
        }
        
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
        
        struct gpiod_line_config *line_cfg = gpiod_line_config_new();
        if (!line_cfg) {
            fprintf(stderr, "Error: Cannot create line config\n");
            gpiod_line_settings_free(settings);
            gpiod_chip_close(fan->chip);
            return -1;
        }
        
        gpiod_line_config_add_line_settings(line_cfg, &fan->gpio_line, 1, settings);
        
        struct gpiod_request_config *req_cfg = gpiod_request_config_new();
        if (!req_cfg) {
            fprintf(stderr, "Error: Cannot create request config\n");
            gpiod_line_config_free(line_cfg);
            gpiod_line_settings_free(settings);
            gpiod_chip_close(fan->chip);
            return -1;
        }
        
    gpiod_request_config_set_consumer(req_cfg, "radxa-penta-fan-ctrl-fan");
        
        fan->line = (struct gpiod_line *)gpiod_chip_request_lines(fan->chip, req_cfg, line_cfg);
        
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        
        if (!fan->line) {
            fprintf(stderr, "Error: Cannot request GPIO line %d\n", fan->gpio_line);
            gpiod_chip_close(fan->chip);
            return -1;
        }
        
        // Start PWM thread
        pthread_t thread;
        if (pthread_create(&thread, NULL, gpio_pwm_thread, fan) != 0) {
            fprintf(stderr, "Error: Cannot create PWM thread\n");
            gpiod_chip_close(fan->chip);
            return -1;
        }
        pthread_detach(thread);
        
        printf("Fan initialized with software PWM (GPIO chip %d, line %d)\n", 
               fan->gpio_chip, fan->gpio_line);
        if (debug_verbose) {
            printf("[DEBUG][PWM/SW] period_s=%.3f initial_duty=%.0f%%\n", fan->period_s, fan->duty_cycle * 100.0);
        }
    }
    
    return 0;
}

int fan_set_duty_cycle(fan_t *fan, double duty) {
    const char *dbg = getenv("RADXA_DEBUG");
    int debug_verbose = (dbg && strcmp(dbg, "2") == 0);
    double requested = duty;
    if (duty < 0.0) duty = 0.0;
    if (duty > 1.0) duty = 1.0;
    
    fan->duty_cycle = duty;
    
    if (fan->use_hardware_pwm) {
        char duty_path[300];
        snprintf(duty_path, sizeof(duty_path), "%s/duty_cycle", fan->pwm_path);
        
        FILE *fp = fopen(duty_path, "w");
        if (!fp) {
            return -1;
        }
        
    int duty_ns = (int)((double)fan->pwm_period_ns * duty);
        fprintf(fp, "%d", duty_ns);
        fclose(fp);
        if (debug_verbose) {
            // Read back values for verification
            long read_duty = -1;
            fp = fopen(duty_path, "r");
            if (fp) { if (fscanf(fp, "%ld", &read_duty) == 1) {} fclose(fp); }
            char enable_path[300];
            snprintf(enable_path, sizeof(enable_path), "%s/enable", fan->pwm_path);
            fp = fopen(enable_path, "r");
            int enabled = -1; if (fp) { if (fscanf(fp, "%d", &enabled) == 1) {} fclose(fp); }
         printf("[DEBUG][PWM/HW] req=%.0f%% clamp=%.0f%% duty_ns=%d read_duty_ns=%ld enabled=%d\n",
             requested * 100.0, duty * 100.0, duty_ns, read_duty, enabled);
        }
    }
    // For GPIO PWM, the thread will pick up the new duty_cycle value
    else if (debug_verbose) {
     printf("[DEBUG][PWM/SW] req=%.0f%% clamp=%.0f%% period=%.0fms hi=%.1fms lo=%.1fms\n",
         requested * 100.0, duty * 100.0,
         fan->period_s * 1000.0,
         fan->duty_cycle * fan->period_s * 1000.0,
         (1.0 - fan->duty_cycle) * fan->period_s * 1000.0);
    }
    
    return 0;
}

static void* gpio_pwm_thread(void *arg) {
    fan_t *fan = (fan_t *)arg;
    struct gpiod_line_request *request = (struct gpiod_line_request *)fan->line;
    const char *dbg = getenv("RADXA_DEBUG");
    int debug_verbose = (dbg && strcmp(dbg, "2") == 0);
    double last_dc = -1.0;
    
    while (fan->running) {
        if (debug_verbose && fabs(fan->duty_cycle - last_dc) > 0.001) {
            printf("[DEBUG][PWM/SW-LOOP] duty=%.0f%% period=%.0fms\n", fan->duty_cycle * 100.0, fan->period_s * 1000.0);
            last_dc = fan->duty_cycle;
        }
        if (fan->duty_cycle <= 0.001) {
            gpiod_line_request_set_value(request, fan->gpio_line, GPIOD_LINE_VALUE_INACTIVE);
            usleep((unsigned int)(fan->period_s * 1000000.0));
        } else {
            double high_time = fan->duty_cycle * fan->period_s;
            double low_time = (1.0 - fan->duty_cycle) * fan->period_s;
            
            gpiod_line_request_set_value(request, fan->gpio_line, GPIOD_LINE_VALUE_ACTIVE);
            usleep((unsigned int)(high_time * 1000000.0));
            
            gpiod_line_request_set_value(request, fan->gpio_line, GPIOD_LINE_VALUE_INACTIVE);
            usleep((unsigned int)(low_time * 1000000.0));
        }
    }
    
    return NULL;
}

void fan_cleanup(fan_t *fan) {
    fan->running = 0;
    usleep(100000); // Give thread time to exit
    
    if (!fan->use_hardware_pwm && fan->chip) {
        if (fan->line) {
            gpiod_line_request_release((struct gpiod_line_request *)fan->line);
        }
        gpiod_chip_close(fan->chip);
    }
}
