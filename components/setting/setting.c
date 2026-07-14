#include "setting.h"

#include "esp_err.h"
#include "esp_log.h"

// static const char *TAG = "[setting]";

static const setting_sensor_t DEFAULT_SENSOR_SETTING = {
    .config = 0U,
    .read_period_ms = 1000UL
};

static const setting_wifi_t DEFAULT_WIFI_AP_SETTING = {
    .ssid = "LidarLite_AP",
    .password = "1234567890"
};

static const setting_wifi_t DEFAULT_WIFI_STA_SETTING = {
    .ssid = "ASUS_Intern",
    .password = "iknowyouknow"
};

static setting_t gs_setting = {
    .sensor = DEFAULT_SENSOR_SETTING,
    .wifi_ap = DEFAULT_WIFI_AP_SETTING,
    .wifi_sta = DEFAULT_WIFI_STA_SETTING
};

esp_err_t setting_init(void)
{
    // TODO: read from littlefs
    return ESP_OK;
}

setting_sensor_t *setting_get_sensor(void)
{
    return &gs_setting.sensor;
}

setting_wifi_t *setting_get_wifi_sta(void)
{
    return &gs_setting.wifi_sta;
}

setting_wifi_t *setting_get_wifi_ap(void)
{
    return &gs_setting.wifi_ap;
}
