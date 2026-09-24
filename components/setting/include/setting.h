#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SETTING_SENSOR_CONFIG_MODE          "sensorConfigMode"
#define SETTING_SENSOR_HIGH_ACCURACY_MODE   "sensorHighAccuracyMode"

#define SETTING_WIFI_STA_SSID               "wifiSTASSID"
#define SETTING_WIFI_STA_PASSWORD           "wifiSTAPassword"
#define SETTING_WIFI_STA_DHCP_ENABLED       "wifiSTADHCPEnabled"
#define SETTING_WIFI_STA_STATIC_IP          "wifiSTAStaticIP"

#define SETTING_WIFI_AP_SSID                "wifiAPSSID"
#define SETTING_WIFI_AP_PASSWORD            "wifiAPPassword"
#define SETTING_WIFI_AP_GATEWAY             "wifiAPGateway"
#define SETTING_WIFI_AP_NETMASK             "wifiAPNetmask"
#define SETTING_WIFI_AP_DHCP_ENABLED        "wifiAPDHCPEnabled"

#define SETTING_SCHEDULER_INTERVAL_SEC      "schedulerIntervalSec"
#define SETTING_SCHEDULER_START_YEAR        "schedulerStartYear"
#define SETTING_SCHEDULER_START_MONTH       "schedulerStartMonth"
#define SETTING_SCHEDULER_START_DAY         "schedulerStartDay"
#define SETTING_SCHEDULER_START_HOUR        "schedulerStartHour"
#define SETTING_SCHEDULER_START_MIN         "schedulerStartMin"

typedef struct
{
    uint8_t config_mode;
    uint8_t high_accuracy_mode;
} setting_sensor_t;

typedef struct
{
    char ssid[32];
    char password[64];
    bool dhcp_enabled; 
    char static_ip[16];
} setting_wifi_sta_t;

typedef struct
{
    char ssid[32];
    char password[64];
    char gateway[16];
    char netmask[16];
    bool dhcp_enabled;
} setting_wifi_ap_t;

typedef struct
{
    uint32_t interval_sec; 
    uint16_t start_year;
    uint8_t start_month;
    uint8_t start_day;
    uint8_t start_hour;
    uint8_t start_min;
} setting_scheduler_t;

typedef struct
{
    setting_sensor_t sensor;
    setting_wifi_sta_t wifi_sta;
    setting_wifi_ap_t wifi_ap;
    setting_scheduler_t scheduler;
} setting_t;
