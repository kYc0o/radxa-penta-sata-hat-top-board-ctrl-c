/* 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Francisco Javier Acosta Padilla
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "oled.h"
#include "ssd1306.h"
#include "intf/i2c/ssd1306_i2c.h"
#include "thermal.h"

static void get_uptime(char *buffer, size_t size);
static void get_ip_address(char *buffer, size_t size);
static void get_cpu_load(char *buffer, size_t size);
static void get_memory_info(char *buffer, size_t size);

int oled_init(oled_t *oled) {
    memset(oled, 0, sizeof(oled_t));
    
    oled->i2c_bus = OLED_I2C_BUS;
    oled->i2c_addr = OLED_I2C_ADDR;
    oled->current_page = 0;
    oled->auto_scroll = 1;
    oled->scroll_interval = 10;
    oled->rotate_180 = 0;
    
    // Initialize I2C with explicit bus and address
    printf("Initializing OLED on I2C bus %d, addr 0x%02X\n", oled->i2c_bus, oled->i2c_addr);
    ssd1306_i2cInitEx2(oled->i2c_bus, -1, -1, oled->i2c_addr);
    
    // Initialize SSD1306 128x32 display
    ssd1306_128x32_init();
    ssd1306_clearScreen();
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    
    oled->initialized = 1;
    printf("OLED initialized successfully\n");
    
    return 0;
}

void oled_cleanup(oled_t *oled) {
    if (oled->initialized) {
        ssd1306_clearScreen();
        oled->initialized = 0;
    }
}

void oled_welcome(oled_t *oled) {
    if (!oled->initialized) return;
    
    ssd1306_clearScreen();
    ssd1306_printFixed(8, 4, "ROCKPI SATA HAT", STYLE_BOLD);
    ssd1306_printFixed(32, 20, "Loading...", STYLE_NORMAL);
    usleep(2000000); // 2 seconds
}

void oled_goodbye(oled_t *oled) {
    if (!oled->initialized) return;
    
    ssd1306_clearScreen();
    ssd1306_printFixed(24, 12, "Good Bye ~", STYLE_BOLD);
    usleep(2000000); // 2 seconds
    ssd1306_clearScreen();
}

static void get_uptime(char *buffer, size_t size) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) {
        snprintf(buffer, size, "Uptime: N/A");
        return;
    }
    
    double uptime_seconds;
    if (fscanf(fp, "%lf", &uptime_seconds) == 1) {
        int days = (int)(uptime_seconds / 86400);
        int hours = (int)((uptime_seconds - days * 86400) / 3600);
        int minutes = (int)((uptime_seconds - days * 86400 - hours * 3600) / 60);
        
        if (days > 0) {
            snprintf(buffer, size, "Up %dd %dh %dm", days, hours, minutes);
        } else if (hours > 0) {
            snprintf(buffer, size, "Up %dh %dm", hours, minutes);
        } else {
            snprintf(buffer, size, "Up %dm", minutes);
        }
    } else {
        snprintf(buffer, size, "Uptime: N/A");
    }
    fclose(fp);
}

static void get_ip_address(char *buffer, size_t size) {
    FILE *fp = popen("hostname -I | awk '{printf \"IP %s\", $1}'", "r");
    if (!fp || !fgets(buffer, (int)size, fp)) {
        snprintf(buffer, size, "IP: N/A");
    } else {
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline
    }
    if (fp) pclose(fp);
}

static void get_cpu_load(char *buffer, size_t size) {
    FILE *fp = popen("uptime | awk '{printf \"CPU: %.2f\", $(NF-2)}'", "r");
    if (!fp || !fgets(buffer, (int)size, fp)) {
        snprintf(buffer, size, "CPU Load: N/A");
    } else {
        buffer[strcspn(buffer, "\n")] = 0;
    }
    if (fp) pclose(fp);
}

static void get_memory_info(char *buffer, size_t size) {
    FILE *fp = popen("free -m | awk 'NR==2{printf \"Mem:%s/%sMB\", $3,$2}'", "r");
    if (!fp || !fgets(buffer, (int)size, fp)) {
        snprintf(buffer, size, "Memory: N/A");
    } else {
        buffer[strcspn(buffer, "\n")] = 0;
    }
    if (fp) pclose(fp);
}

void oled_show_page(oled_t *oled, oled_page_t page) {
    if (!oled->initialized) return;
    
    char line1[64], line2[320], line3[64];
    
    ssd1306_clearScreen();
    
    switch (page) {
        case PAGE_SYSTEM: {
            get_uptime(line1, sizeof(line1));
            
            double cpu_temp = thermal_read_cpu_temp();
            snprintf(line2, sizeof(line2), "CPU: %.1fC", cpu_temp);
            
            get_ip_address(line3, sizeof(line3));
            
            ssd1306_printFixed(0, 0, line1, STYLE_NORMAL);
            ssd1306_printFixed(0, 10, line2, STYLE_NORMAL);
            ssd1306_printFixed(0, 20, line3, STYLE_NORMAL);
            break;
        }
        
        case PAGE_RESOURCES: {
            get_cpu_load(line1, sizeof(line1));
            get_memory_info(line2, sizeof(line2));
            
            ssd1306_printFixed(0, 4, line1, STYLE_NORMAL);
            ssd1306_printFixed(0, 18, line2, STYLE_NORMAL);
            break;
        }
        
        case PAGE_DISKS: {
            int temps[4];
            int count = thermal_read_ssd_temps(temps, 4);
            
            if (count > 0) {
                int t0 = temps[0] > 0 ? temps[0] : 0;
                int t1 = temps[1] > 0 ? temps[1] : 0;
                int t2 = temps[2] > 0 ? temps[2] : 0;
                int t3 = temps[3] > 0 ? temps[3] : 0;
                snprintf(line1, sizeof(line1), "SDA:%dC SDB:%dC", t0, t1);
                snprintf(line2, sizeof(line2), "SDC:%dC SDD:%dC", t2, t3);
                
                ssd1306_printFixed(0, 2, line1, STYLE_NORMAL);
                ssd1306_printFixed(0, 14, line2, STYLE_NORMAL);
            } else {
                ssd1306_printFixed(0, 12, "No SSD data", STYLE_NORMAL);
            }
            break;
        }
        
        case PAGE_RAID: {
            FILE *fp = popen("df -h /dev/md0 2>/dev/null | awk 'NR==2 {printf \"RAID:%s/%s(%s)\", $3, $2, $5}'", "r");
            if (fp && fgets(line1, sizeof(line1), fp)) {
                line1[strcspn(line1, "\n")] = 0;
                ssd1306_printFixed(0, 12, line1, STYLE_NORMAL);
            } else {
                ssd1306_printFixed(0, 12, "RAID: N/A", STYLE_NORMAL);
            }
            if (fp) pclose(fp);
            break;
        }
        
        case PAGE_COUNT:
            // Not a real page, just used for counting
            break;
        
        default:
            break;
    }
}

void oled_next_page(oled_t *oled) {
    if (!oled->initialized) return;
    
    oled->current_page = (oled->current_page + 1) % PAGE_COUNT;
    oled_show_page(oled, (oled_page_t)oled->current_page);
}

void* oled_auto_scroll_thread(void *arg) {
    oled_t *oled = (oled_t *)arg;
    
    while (oled->initialized && oled->auto_scroll) {
        oled_show_page(oled, (oled_page_t)oled->current_page);
        sleep(oled->scroll_interval);
        oled_next_page(oled);
    }
    
    return NULL;
}
