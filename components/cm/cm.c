#include "cm.h"

#include "esp_err.h"
#include "esp_log.h"

#include "wifi.h"
#include "time_sync.h"

static const char *TAG = "[cm]";

esp_err_t cm_init(void)
{
    esp_err_t status = ESP_OK;

    status = wifi_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(status));
        status = ESP_FAIL;
        goto exit;
    }
    
    status = time_sync_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(status));
        status = ESP_FAIL;
    }

exit:
    return status;
}