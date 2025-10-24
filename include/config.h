/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_FILE "/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.conf"
#define MAX_LINE 256
#define MAX_DEVICES 8

typedef struct {
    double lv0;
    double lv1;
    double lv2;
    double lv3;
} fan_config_t;

typedef struct {
    // Thermal control tunables (defaults chosen for stability)
    double hysteresis_c;            // Cooling hysteresis in °C (default 3.0)
    double deadband_c;              // Temperature dead-band in °C (default 1.5)
    double trend_heat_c;            // Threshold for considering system "heating" (default 0.3)
    double trend_fast_heat_c;       // Threshold for fast ramp allowance (default 1.0)
    double max_dc_change_per_cycle; // Max duty change per cycle (legacy, kept for compat)
    double min_effective_dc;        // Deprecated: no longer enforces minimum duty (kept for config compatibility)

    // New: Asymmetric and adaptive ramp controls
    // Base upward ramp per control cycle (e.g., 0.07 = 7% per second)
    double up_rate_base_per_cycle;
    // Additional upward ramp added per degree of positive trend (°C over window)
    double up_rate_trend_gain;
    // Maximum upward ramp per cycle when heating fast
    double up_rate_max_per_cycle;
    // Downward ramp limit per cycle (cooling). Typically smaller than upward.
    double down_rate_per_cycle;
    // After any increase, keep fan from decreasing for this many seconds
    double cooldown_hold_sec;
} thermal_tunables_t;

typedef struct {
    fan_config_t fan;
    fan_config_t fan_ssd;
    int fan_enabled;
    thermal_tunables_t thermal;    // New thermal tunables
    int oled_rotate;                // OLED 180 degree rotation (default 0)
} config_t;

int config_load(config_t *cfg);
double config_temp_to_dc(fan_config_t *fan_cfg, double temp);

#endif // CONFIG_H
