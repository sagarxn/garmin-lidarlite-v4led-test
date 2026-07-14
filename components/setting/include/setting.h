#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t config;
    uint32_t read_period_ms;
} setting_sensor_t;

typedef struct
{
    char ssid[32];
    char password[64];
} setting_wifi_t;


typedef struct
{
    setting_sensor_t sensor;
    setting_wifi_t wifi_sta;
    setting_wifi_t wifi_ap;
} setting_t;

setting_sensor_t *setting_get_sensor(void);
setting_wifi_t *setting_get_wifi_sta(void);
setting_wifi_t *setting_get_wifi_ap(void);