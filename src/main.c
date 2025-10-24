/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "fan.h"
#include "thermal.h"
#include "oled.h"
#include "button.h"

static volatile int running = 1;
static int use_oled = 0;
static int use_button = 0;

static void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    running = 0;
}

static void load_env_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot open environment file %s\n", path);
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // Find '='
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *value = eq + 1;
            setenv(key, value, 1);
        }
    }
    
    fclose(fp);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    // Disable stdout buffering for immediate log output to systemd/journald
    setbuf(stdout, NULL);
    
    config_t cfg;
    fan_t fan;
    oled_t oled;
    button_t button;
    thermal_state_t thermal_state;
    pthread_t oled_thread;
    pthread_t button_thread;
    
    printf("Radxa Penta Fan Controller v1.0\n");
    printf("===============================\n\n");
    
    // Load environment variables
    const char *env_file = "/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.env";
    if (access(env_file, F_OK) != 0) {
        // Try local file
        env_file = "radxa-penta-fan-ctrl.env";
    }
    load_env_file(env_file);
    
    // Load configuration
    if (config_load(&cfg) < 0) {
        fprintf(stderr, "Error loading configuration\n");
        return 1;
    }
    
    printf("Configuration loaded:\n");
    printf("  CPU Fan: %.1f°C/%.1f°C/%.1f°C/%.1f°C\n", 
           cfg.fan.lv0, cfg.fan.lv1, cfg.fan.lv2, cfg.fan.lv3);
    printf("  SSD Fan: %.1f°C/%.1f°C/%.1f°C/%.1f°C\n\n", 
           cfg.fan_ssd.lv0, cfg.fan_ssd.lv1, cfg.fan_ssd.lv2, cfg.fan_ssd.lv3);
    
    // Initialize thermal state for smart control
    thermal_state_init(&thermal_state);
    printf("Smart thermal control enabled\n");
    printf("  - Moving average filter (10 samples)\n");
    printf("  - Hysteresis (%.1f°C cooling)\n", cfg.thermal.hysteresis_c);
    printf("  - Dead-band zone (±%.1f°C)\n", cfg.thermal.deadband_c);
    printf("  - Rate limiting (max %.0f%%/cycle)\n", cfg.thermal.max_dc_change_per_cycle * 100.0);
    printf("  - Minimum effective duty: %.0f%%\n", cfg.thermal.min_effective_dc * 100.0);
    printf("  - Temperature trend analysis (heat>%.2f°C, fast>%.2f°C)\n\n", 
           cfg.thermal.trend_heat_c, cfg.thermal.trend_fast_heat_c);
    
    // Try to initialize OLED
    if (oled_init(&oled) == 0) {
        use_oled = 1;
        oled_welcome(&oled);
        
        // Start OLED auto-scroll thread
        if (pthread_create(&oled_thread, NULL, oled_auto_scroll_thread, &oled) != 0) {
            fprintf(stderr, "Warning: Failed to create OLED thread\n");
            use_oled = 0;
        } else {
            pthread_detach(oled_thread);
        }
        
        // Initialize button (requires OLED reference)
        const char *button_chip_env = getenv("BUTTON_CHIP");
        const char *button_line_env = getenv("BUTTON_LINE");
        
        if (button_chip_env && button_line_env) {
            int button_chip = atoi(button_chip_env);
            unsigned int button_line = (unsigned int)atoi(button_line_env);
            
            if (button_init(&button, button_chip, button_line, &oled) == 0) {
                use_button = 1;
                
                // Start button watch thread
                if (pthread_create(&button_thread, NULL, button_watch_thread, &button) != 0) {
                    fprintf(stderr, "Warning: Failed to create button thread\n");
                    button_cleanup(&button);
                    use_button = 0;
                } else {
                    pthread_detach(button_thread);
                }
            }
        } else {
            printf("Button GPIO not configured in environment\n");
        }
    } else {
        printf("OLED not available, continuing without display\n\n");
        use_oled = 0;
    }
    
    // Initialize fan
    if (fan_init(&fan) < 0) {
        fprintf(stderr, "Error initializing fan\n");
        if (use_oled) oled_cleanup(&oled);
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Fan control started. Press Ctrl+C to stop.\n\n");
    
    // Main control loop - use smart thermal control
    double last_dc = -1.0;
    while (running) {
        double dc = thermal_calculate_duty_cycle_smart(&cfg, &thermal_state);
        
        if (dc != last_dc) {
            if (fan_set_duty_cycle(&fan, dc) < 0) {
                fprintf(stderr, "Warning: Failed to set duty cycle\n");
            }
            last_dc = dc;
        }
        
        sleep(1);
    }
    
    // Cleanup
    printf("\nStopping fan...\n");
    fan_set_duty_cycle(&fan, 0.0);
    fan_cleanup(&fan);
    
    if (use_button) {
        button_cleanup(&button);
    }
    
    if (use_oled) {
        oled_goodbye(&oled);
        oled_cleanup(&oled);
    }
    
    printf("Shutdown complete.\n");
    return 0;
}
