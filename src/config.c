/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

static char* trim(char *str) {
    char *end;
    // Trim leading whitespace
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    // Trim trailing whitespace
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static int parse_line(const char *line, char *section, char *key, char *value) {
    char buf[MAX_LINE];
    strncpy(buf, line, MAX_LINE - 1);
    buf[MAX_LINE - 1] = '\0';
    char *trimmed_buf = trim(buf);
    
    if (trimmed_buf[0] == '#' || trimmed_buf[0] == ';' || trimmed_buf[0] == '\0') {
        return 0; // Comment or empty
    }
    
    if (trimmed_buf[0] == '[') {
        char *end = strchr(trimmed_buf, ']');
        if (end) {
            *end = '\0';
            strcpy(section, trimmed_buf + 1);
            return 1;
        }
    }
    
    char *eq = strchr(trimmed_buf, '=');
    if (eq) {
        *eq = '\0';
        strcpy(key, trimmed_buf);
        strcpy(value, eq + 1);
        char *trimmed_key = trim(key);
        char *trimmed_value = trim(value);
        strcpy(key, trimmed_key);
        strcpy(value, trimmed_value);
        return 2;
    }
    
    return 0;
}

int config_load(config_t *cfg) {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot open config file %s, using defaults\n", CONFIG_FILE);
        // Set defaults optimized for Raspberry Pi 5
        cfg->fan.lv0 = 55.0;
        cfg->fan.lv1 = 62.0;
        cfg->fan.lv2 = 70.0;
        cfg->fan.lv3 = 78.0;
        
        cfg->fan_ssd.lv0 = 45.0;
        cfg->fan_ssd.lv1 = 50.0;
        cfg->fan_ssd.lv2 = 55.0;
        cfg->fan_ssd.lv3 = 60.0;
        
        cfg->fan_enabled = 1;

        // OLED defaults
        cfg->oled_rotate = 0;

        // Thermal tunable defaults
        cfg->thermal.hysteresis_c = 3.0;
        cfg->thermal.deadband_c = 1.5;
        cfg->thermal.trend_heat_c = 0.3;
        cfg->thermal.trend_fast_heat_c = 1.0;
        cfg->thermal.max_dc_change_per_cycle = 0.10; // legacy cap
        // New asymmetric/adaptive ramp defaults
        cfg->thermal.up_rate_base_per_cycle = 0.07;   // 7% per cycle base
        cfg->thermal.up_rate_trend_gain = 0.20;       // +20% per +1°C trend (responsive to rapid heating)
        cfg->thermal.up_rate_max_per_cycle = 0.30;    // cap at 30% per cycle
        cfg->thermal.down_rate_per_cycle = 0.05;      // 5% per cycle down (gentle deceleration)
        cfg->thermal.cooldown_hold_sec = 20.0;        // 20s hold before decreasing
        return 0;
    }
    
    char line[MAX_LINE];
    char section[64] = "";
    char key[64], value[64];
    
    // Set defaults first (RPi 5 optimized)
    cfg->fan.lv0 = 55.0;
    cfg->fan.lv1 = 62.0;
    cfg->fan.lv2 = 70.0;
    cfg->fan.lv3 = 78.0;
    
    cfg->fan_ssd.lv0 = 45.0;
    cfg->fan_ssd.lv1 = 50.0;
    cfg->fan_ssd.lv2 = 55.0;
    cfg->fan_ssd.lv3 = 60.0;
    
    cfg->fan_enabled = 1;

    // OLED defaults
    cfg->oled_rotate = 0;

    // Thermal tunable defaults
    cfg->thermal.hysteresis_c = 3.0;
    cfg->thermal.deadband_c = 1.5;
    cfg->thermal.trend_heat_c = 0.3;
    cfg->thermal.trend_fast_heat_c = 1.0;
    cfg->thermal.max_dc_change_per_cycle = 0.10; // legacy cap
    cfg->thermal.up_rate_base_per_cycle = 0.07;
    cfg->thermal.up_rate_trend_gain = 0.20;
    cfg->thermal.up_rate_max_per_cycle = 0.30;
    cfg->thermal.down_rate_per_cycle = 0.05;
    cfg->thermal.cooldown_hold_sec = 20.0;
    
    while (fgets(line, sizeof(line), fp)) {
        int result = parse_line(line, section, key, value);
        
        if (result == 2) { // Key-value pair
            if (strcmp(section, "fan") == 0) {
                if (strcmp(key, "lv0") == 0) cfg->fan.lv0 = strtod(value, NULL);
                else if (strcmp(key, "lv1") == 0) cfg->fan.lv1 = strtod(value, NULL);
                else if (strcmp(key, "lv2") == 0) cfg->fan.lv2 = strtod(value, NULL);
                else if (strcmp(key, "lv3") == 0) cfg->fan.lv3 = strtod(value, NULL);
            } else if (strcmp(section, "fan_ssd") == 0) {
                if (strcmp(key, "lv0") == 0) cfg->fan_ssd.lv0 = strtod(value, NULL);
                else if (strcmp(key, "lv1") == 0) cfg->fan_ssd.lv1 = strtod(value, NULL);
                else if (strcmp(key, "lv2") == 0) cfg->fan_ssd.lv2 = strtod(value, NULL);
                else if (strcmp(key, "lv3") == 0) cfg->fan_ssd.lv3 = strtod(value, NULL);
            } else if (strcmp(section, "thermal") == 0) {
                if (strcmp(key, "hysteresis") == 0) cfg->thermal.hysteresis_c = strtod(value, NULL);
                else if (strcmp(key, "deadband") == 0) cfg->thermal.deadband_c = strtod(value, NULL);
                else if (strcmp(key, "trend_heat") == 0) cfg->thermal.trend_heat_c = strtod(value, NULL);
                else if (strcmp(key, "trend_fast_heat") == 0) cfg->thermal.trend_fast_heat_c = strtod(value, NULL);
                else if (strcmp(key, "max_dc_change") == 0) cfg->thermal.max_dc_change_per_cycle = strtod(value, NULL);
                else if (strcmp(key, "up_rate_base") == 0) cfg->thermal.up_rate_base_per_cycle = strtod(value, NULL);
                else if (strcmp(key, "up_rate_trend_gain") == 0) cfg->thermal.up_rate_trend_gain = strtod(value, NULL);
                else if (strcmp(key, "up_rate_max") == 0) cfg->thermal.up_rate_max_per_cycle = strtod(value, NULL);
                else if (strcmp(key, "down_rate") == 0) cfg->thermal.down_rate_per_cycle = strtod(value, NULL);
                else if (strcmp(key, "cooldown_hold_sec") == 0) cfg->thermal.cooldown_hold_sec = strtod(value, NULL);
            } else if (strcmp(section, "oled") == 0) {
                if (strcmp(key, "rotate") == 0) {
                    cfg->oled_rotate = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
                    if (getenv("RADXA_DEBUG")) {
                        fprintf(stderr, "[Config] OLED rotate: '%s' -> %d\n", value, cfg->oled_rotate);
                    }
                }
            }
        }
    }
    
    fclose(fp);

    return 0;
}

double config_temp_to_dc(fan_config_t *fan_cfg, double temp) {
    if (temp >= fan_cfg->lv3) return 1.00;
    if (temp >= fan_cfg->lv2) return 0.75;
    if (temp >= fan_cfg->lv1) return 0.50;
    if (temp >= fan_cfg->lv0) return 0.25;
    return 0.0;
}
