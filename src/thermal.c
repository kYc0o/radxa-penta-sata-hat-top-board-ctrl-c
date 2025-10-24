/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "thermal.h"

double thermal_read_cpu_temp(void) {
    FILE *fp = fopen(THERMAL_ZONE_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot read CPU temperature\n");
        return 0.0;
    }
    
    int temp_millicelsius;
    if (fscanf(fp, "%d", &temp_millicelsius) != 1) {
        fclose(fp);
        return 0.0;
    }
    
    fclose(fp);
    return (double)temp_millicelsius / 1000.0;
}

static int parse_smartctl_temp(const char *line) {
    // Look for temperature keywords
    if (strstr(line, "Temperature_Celsius") || 
        strstr(line, "Airflow_Temperature_Cel") ||
        strstr(line, "Composite Temperature")) {
        
        // Parse the line backwards for the temperature value
        char *tokens[20];
        int count = 0;
        char buf[256];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        char *token = strtok(buf, " \t");
        while (token != NULL && count < 20) {
            tokens[count++] = token;
            token = strtok(NULL, " \t");
        }
        
        // Temperature is usually in the last few fields
        for (size_t i = (size_t)count - 1; i < (size_t)count; i--) {
            int temp = atoi(tokens[i]);
            if (temp > 0 && temp < 200) { // Reasonable temp range
                return temp;
            }
        }
    }
    return -1;
}

int thermal_read_ssd_temps(int *temps, int max_count) {
    const char *devices[] = {"sda", "sdb", "sdc", "sdd"};
    int num_devices = sizeof(devices) / sizeof(devices[0]);
    int found = 0;
    
    for (int i = 0; i < num_devices && i < max_count; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), SMARTCTL_CMD, devices[i]);
        
        FILE *fp = popen(cmd, "r");
        if (!fp) {
            temps[i] = 0;
            continue;
        }
        
        char line[256];
        int temp_found = 0;
        while (fgets(line, sizeof(line), fp)) {
            int temp = parse_smartctl_temp(line);
            if (temp > 0) {
                temps[i] = temp;
                temp_found = 1;
                found++;
                break;
            }
        }
        
        if (!temp_found) {
            temps[i] = 0;
        }
        
        pclose(fp);
    }
    
    return found;
}

// Legacy simple duty cycle calculation (kept for compatibility)
double thermal_calculate_duty_cycle(config_t *cfg) {
    if (!cfg->fan_enabled) {
        return 0.0;
    }
    
    // Read CPU temperature
    double cpu_temp = thermal_read_cpu_temp();
    double dc_cpu = config_temp_to_dc(&cfg->fan, cpu_temp);
    
    // Read SSD temperatures
    int ssd_temps[MAX_DEVICES];
    int ssd_count = thermal_read_ssd_temps(ssd_temps, MAX_DEVICES);
    
    // Find max SSD temperature
    int max_ssd_temp = 0;
    for (int i = 0; i < ssd_count && i < MAX_DEVICES; i++) {
        if (ssd_temps[i] > max_ssd_temp) {
            max_ssd_temp = ssd_temps[i];
        }
    }
    
    double dc_ssd = config_temp_to_dc(&cfg->fan_ssd, (double)max_ssd_temp);
    
    // Use the higher duty cycle
    double dc = (dc_cpu > dc_ssd) ? dc_cpu : dc_ssd;
    
    static int log_counter = 0;
    if (log_counter++ % 30 == 0) { // Log every 30 seconds
        printf("[Fan] CPU: %.1f°C → DC %.2f | SSD: %d°C → DC %.2f | Final DC: %.2f\n",
               cpu_temp, dc_cpu, max_ssd_temp, dc_ssd, dc);
    }
    
    return dc;
}

void thermal_state_init(thermal_state_t *state) {
    memset(state, 0, sizeof(thermal_state_t));
    state->last_duty_cycle = 0.0;
    state->last_cpu_temp = 0.0;
    state->last_ssd_temp = 0;
    state->hold_until = 0;
}

static double calculate_moving_average(double *history, int count) {
    if (count == 0) return 0.0;
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += history[i];
    }
    return sum / count;
}

static int calculate_moving_average_int(int *history, int count) {
    if (count == 0) return 0;
    
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += history[i];
    }
    return sum / count;
}

static double calculate_temp_trend(double *history, int count) {
    if (count < 3) return 0.0;
    
    // Calculate trend: compare recent avg vs older avg
    int half = count / 2;
    double recent_sum = 0.0, older_sum = 0.0;
    
    for (int i = 0; i < half; i++) {
        int older_idx = i;
        int recent_idx = half + i;
        older_sum += history[older_idx];
        recent_sum += history[recent_idx];
    }
    
    double recent_avg = recent_sum / half;
    double older_avg = older_sum / half;
    
    return recent_avg - older_avg; // Positive = rising, negative = falling
}

static double calculate_temp_trend_int(int *history, int count) {
    if (count < 3) return 0.0;
    
    // Calculate trend: compare recent avg vs older avg
    int half = count / 2;
    int recent_sum = 0, older_sum = 0;
    
    for (int i = 0; i < half; i++) {
        int older_idx = i;
        int recent_idx = half + i;
        older_sum += history[older_idx];
        recent_sum += history[recent_idx];
    }
    
    double recent_avg = (double)recent_sum / half;
    double older_avg = (double)older_sum / half;
    
    return recent_avg - older_avg; // Positive = rising, negative = falling
}

// Calculate duty cycle with hysteresis (different thresholds for heating/cooling)
static double config_temp_to_dc_with_hysteresis(fan_config_t *fan_cfg, double temp, double hysteresis_c, int is_heating) {
    // Use larger hysteresis when cooling down to prevent oscillation
    double hysteresis = is_heating ? 0.0 : hysteresis_c;

    if (temp >= fan_cfg->lv3 - hysteresis) return 1.00;
    if (temp >= fan_cfg->lv2 - hysteresis) return 0.75;
    if (temp >= fan_cfg->lv1 - hysteresis) return 0.50;
    if (temp >= fan_cfg->lv0 - hysteresis) return 0.25;

    // Below lv0: return 0 and let the control algorithm ramp smoothly from current duty
    return 0.0;
}

// Smart thermal control with hysteresis, rate limiting, and trend analysis
double thermal_calculate_duty_cycle_smart(config_t *cfg, thermal_state_t *state) {
    if (!cfg->fan_enabled) {
        return 0.0;
    }
    time_t now = time(NULL);
    
    // Read current temperatures
    double cpu_temp = thermal_read_cpu_temp();
    
    int ssd_temps[MAX_DEVICES];
    int ssd_count = thermal_read_ssd_temps(ssd_temps, MAX_DEVICES);
    
    int max_ssd_temp = 0;
    for (int i = 0; i < ssd_count && i < MAX_DEVICES; i++) {
        if (ssd_temps[i] > max_ssd_temp) {
            max_ssd_temp = ssd_temps[i];
        }
    }
    
    // Update temperature history
    state->cpu_temps[state->history_index] = cpu_temp;
    state->ssd_temps[state->history_index] = max_ssd_temp;
    state->history_index = (state->history_index + 1) % TEMP_HISTORY_SIZE;
    if (state->history_count < TEMP_HISTORY_SIZE) {
        state->history_count++;
    }
    
    // Calculate moving averages
    double cpu_avg = calculate_moving_average(state->cpu_temps, state->history_count);
    int ssd_avg = calculate_moving_average_int(state->ssd_temps, state->history_count);
    
    // Calculate temperature trends (positive = heating, negative = cooling)
    double cpu_trend = calculate_temp_trend(state->cpu_temps, state->history_count);
    double ssd_trend = calculate_temp_trend_int(state->ssd_temps, state->history_count);
    
    // Determine if system is heating or cooling
    int cpu_is_heating = (cpu_trend > cfg->thermal.trend_heat_c);
    int ssd_is_heating = (ssd_trend > cfg->thermal.trend_heat_c);
    
    // Calculate target duty cycles with hysteresis
    double dc_cpu_target = config_temp_to_dc_with_hysteresis(&cfg->fan, cpu_avg, cfg->thermal.hysteresis_c, cpu_is_heating);
    double dc_ssd_target = config_temp_to_dc_with_hysteresis(&cfg->fan_ssd, (double)ssd_avg, cfg->thermal.hysteresis_c, ssd_is_heating);
    
    // Use the higher duty cycle
    double dc_target = (dc_cpu_target > dc_ssd_target) ? dc_cpu_target : dc_ssd_target;
    
    // Dead-band zone: Only change if temperature difference is significant
    // or if we're at 0% and need to start the fan
    double temp_change_cpu = cpu_avg - state->last_cpu_temp;
    int temp_change_ssd = ssd_avg - state->last_ssd_temp;
    double max_temp_change = (temp_change_cpu > (double)temp_change_ssd) ? temp_change_cpu : (double)temp_change_ssd;
    
    // Skip adjustment if temperature change is small and we're stable
    int skip_adjustment = 0;
    if (state->stable_cycles > 5 && 
        fabs(max_temp_change) < cfg->thermal.deadband_c && 
        fabs(dc_target - state->last_duty_cycle) < 0.15) {
        skip_adjustment = 1;
        dc_target = state->last_duty_cycle;  // Keep current duty cycle
    }
    
    // Apply asymmetric, adaptive rate limiting for smooth transitions
    double dc_change = dc_target - state->last_duty_cycle;
    double heat_trend = (cpu_trend > ssd_trend) ? cpu_trend : ssd_trend;

    // Upward rate scales with positive trend, capped by up_rate_max
    double up_rate = cfg->thermal.up_rate_base_per_cycle;
    if (heat_trend > 0.0) {
        up_rate += cfg->thermal.up_rate_trend_gain * heat_trend;
    }
    // Respect legacy cap if set smaller, then clamp to new explicit max
    if (cfg->thermal.max_dc_change_per_cycle > 0.0 && cfg->thermal.max_dc_change_per_cycle < up_rate) {
        up_rate = cfg->thermal.max_dc_change_per_cycle;
    }
    if (up_rate > cfg->thermal.up_rate_max_per_cycle) {
        up_rate = cfg->thermal.up_rate_max_per_cycle;
    }

    double down_rate = cfg->thermal.down_rate_per_cycle;
    // Cooldown hold: prevent decreases while hold is active
    int hold_active = (state->hold_until != 0 && now < state->hold_until);

    if (dc_change > up_rate) {
        dc_change = up_rate;
    } else if (dc_change < 0.0) {
        if (hold_active) {
            dc_change = 0.0; // do not decrease yet
        } else if (dc_change < -down_rate) {
            dc_change = -down_rate;
        }
    }
    
    double dc_new = state->last_duty_cycle + dc_change;
    
    // Ensure bounds (allow any duty cycle from 0 to 100%)
    if (dc_new < 0.0) dc_new = 0.0;
    if (dc_new > 1.0) dc_new = 1.0;
    
    // Count stable cycles (no change in duty cycle)
    if (dc_new == state->last_duty_cycle) {
        state->stable_cycles++;
    } else {
        state->stable_cycles = 0;
    }
    
    // Update state
    // If we increased duty, extend the cooldown hold to keep airflow going
    if (dc_new > state->last_duty_cycle) {
        state->hold_until = now + (time_t)(cfg->thermal.cooldown_hold_sec);
    }
    state->last_duty_cycle = dc_new;
    state->last_cpu_temp = cpu_avg;
    state->last_ssd_temp = ssd_avg;
    
    // Optional verbose debug block (only with RADXA_DEBUG=2)
    const char *dbg = getenv("RADXA_DEBUG");
    int debug_verbose = (dbg && strcmp(dbg, "2") == 0);
    if (debug_verbose) {
        printf("[DEBUG][THERM] raw CPU=%.1fC SSDmax=%dC | avg CPU=%.1fC SSD=%dC | trend CPU=%+.2f SSD=%+.2f\n",
               cpu_temp, max_ssd_temp, cpu_avg, ssd_avg, cpu_trend, ssd_trend);
        printf("[DEBUG][THERM] heat CPU=%d SSD=%d | thresholds CPU[%.0f/%.0f/%.0f/%.0f] SSD[%.0f/%.0f/%.0f/%.0f] hys=%.1f deadband=%.1f trend_heat=%.2f fast_heat=%.2f up_base=%.0f%% up_gain=%.0f%%/C up_max=%.0f%% down=%.0f%% hold=%ds min_eff=%.0f%%\n",
               cpu_is_heating, ssd_is_heating,
               cfg->fan.lv0, cfg->fan.lv1, cfg->fan.lv2, cfg->fan.lv3,
               cfg->fan_ssd.lv0, cfg->fan_ssd.lv1, cfg->fan_ssd.lv2, cfg->fan_ssd.lv3,
               cfg->thermal.hysteresis_c, cfg->thermal.deadband_c,
               cfg->thermal.trend_heat_c, cfg->thermal.trend_fast_heat_c,
               cfg->thermal.up_rate_base_per_cycle * 100.0,
               cfg->thermal.up_rate_trend_gain * 100.0,
               cfg->thermal.up_rate_max_per_cycle * 100.0,
               cfg->thermal.down_rate_per_cycle * 100.0,
               (int)cfg->thermal.cooldown_hold_sec,
               cfg->thermal.min_effective_dc * 100.0);
        printf("[DEBUG][THERM] dc_cpu_tgt=%.0f%% dc_ssd_tgt=%.0f%% dc_target=%.0f%% | trend=%.2f up_rate=%.0f%% down_rate=%.0f%% hold=%s -> dc_delta=%+.0f%% dc_new=%.0f%%\n",
               dc_cpu_target * 100.0, dc_ssd_target * 100.0, dc_target * 100.0,
               heat_trend,
               up_rate * 100.0, down_rate * 100.0,
               hold_active ? "ON" : "off",
               dc_change * 100.0, dc_new * 100.0);
    }
    
    // Logging (every 30 seconds or when duty cycle changes)
    static int log_counter = 0;
    int should_log = (log_counter++ % 30 == 0) || (state->stable_cycles == 0);
    
    if (should_log) {
        printf("[Fan] CPU: %.1f°C (Δ%+.1f°C) → DC %.0f%% | SSD: %d°C (Δ%+.1f°C) → DC %.0f%% | Active: %.0f%%%s%s%s\n",
               cpu_avg, cpu_trend, dc_cpu_target * 100.0, 
               ssd_avg, ssd_trend, dc_ssd_target * 100.0, 
               dc_new * 100.0,
               (state->stable_cycles == 0) ? " [ADJUSTING]" : "",
               skip_adjustment ? " [DEADBAND]" : "",
               (state->hold_until != 0 && now < state->hold_until) ? " [HOLD]" : "");
    }
    
    return dc_new;
}
