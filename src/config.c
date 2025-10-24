/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

static double read_env_float(const char *name, double fallback) {
    const char *v = getenv(name);
    if (v && *v) {
        char *endp = NULL;
        double val = strtod(v, &endp);
        if (endp != v) return val;
    }
    return fallback;
}

static void trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static int parse_line(const char *line, char *section, char *key, char *value) {
    char buf[MAX_LINE];
    strncpy(buf, line, MAX_LINE - 1);
    buf[MAX_LINE - 1] = '\0';
    trim(buf);
    
    if (buf[0] == '#' || buf[0] == ';' || buf[0] == '\0') {
        return 0; // Comment or empty
    }
    
    if (buf[0] == '[') {
        char *end = strchr(buf, ']');
        if (end) {
            *end = '\0';
            strcpy(section, buf + 1);
            return 1;
        }
    }
    
    char *eq = strchr(buf, '=');
    if (eq) {
        *eq = '\0';
        strcpy(key, buf);
        strcpy(value, eq + 1);
        trim(key);
        trim(value);
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

        // Apply environment overrides even if file missing
        cfg->thermal.hysteresis_c = read_env_float("RADXA_HYSTERESIS_C", cfg->thermal.hysteresis_c);
        cfg->thermal.deadband_c = read_env_float("RADXA_DEADBAND_C", cfg->thermal.deadband_c);
        cfg->thermal.trend_heat_c = read_env_float("RADXA_TREND_HEAT_C", cfg->thermal.trend_heat_c);
        cfg->thermal.trend_fast_heat_c = read_env_float("RADXA_TREND_FAST_HEAT_C", cfg->thermal.trend_fast_heat_c);
        cfg->thermal.max_dc_change_per_cycle = read_env_float("RADXA_MAX_DC_CHANGE", cfg->thermal.max_dc_change_per_cycle);
        cfg->thermal.up_rate_base_per_cycle = read_env_float("RADXA_UP_RATE_BASE", cfg->thermal.up_rate_base_per_cycle);
        cfg->thermal.up_rate_trend_gain = read_env_float("RADXA_UP_RATE_TREND_GAIN", cfg->thermal.up_rate_trend_gain);
        cfg->thermal.up_rate_max_per_cycle = read_env_float("RADXA_UP_RATE_MAX", cfg->thermal.up_rate_max_per_cycle);
        cfg->thermal.down_rate_per_cycle = read_env_float("RADXA_DOWN_RATE", cfg->thermal.down_rate_per_cycle);
        cfg->thermal.cooldown_hold_sec = read_env_float("RADXA_COOLDOWN_HOLD_SEC", cfg->thermal.cooldown_hold_sec);
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
            }
        }
    }
    
    fclose(fp);

    // Apply environment overrides (higher precedence than file)
    cfg->thermal.hysteresis_c = read_env_float("RADXA_HYSTERESIS_C", cfg->thermal.hysteresis_c);
    cfg->thermal.deadband_c = read_env_float("RADXA_DEADBAND_C", cfg->thermal.deadband_c);
    cfg->thermal.trend_heat_c = read_env_float("RADXA_TREND_HEAT_C", cfg->thermal.trend_heat_c);
    cfg->thermal.trend_fast_heat_c = read_env_float("RADXA_TREND_FAST_HEAT_C", cfg->thermal.trend_fast_heat_c);
    cfg->thermal.max_dc_change_per_cycle = read_env_float("RADXA_MAX_DC_CHANGE", cfg->thermal.max_dc_change_per_cycle);
    cfg->thermal.up_rate_base_per_cycle = read_env_float("RADXA_UP_RATE_BASE", cfg->thermal.up_rate_base_per_cycle);
    cfg->thermal.up_rate_trend_gain = read_env_float("RADXA_UP_RATE_TREND_GAIN", cfg->thermal.up_rate_trend_gain);
    cfg->thermal.up_rate_max_per_cycle = read_env_float("RADXA_UP_RATE_MAX", cfg->thermal.up_rate_max_per_cycle);
    cfg->thermal.down_rate_per_cycle = read_env_float("RADXA_DOWN_RATE", cfg->thermal.down_rate_per_cycle);
    cfg->thermal.cooldown_hold_sec = read_env_float("RADXA_COOLDOWN_HOLD_SEC", cfg->thermal.cooldown_hold_sec);
    return 0;
}

double config_temp_to_dc(fan_config_t *fan_cfg, double temp) {
    if (temp >= fan_cfg->lv3) return 1.00;
    if (temp >= fan_cfg->lv2) return 0.75;
    if (temp >= fan_cfg->lv1) return 0.50;
    if (temp >= fan_cfg->lv0) return 0.25;
    return 0.0;
}
