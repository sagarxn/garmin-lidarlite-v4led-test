#include <stdio.h>
#include "WSCADA.h"

esp_err_t wscada_init(void)
{
    esp_err_t status = ESP_OK;

    status = udi_init();
    if (ESP_OK != status)
    {
        ESP_LOGE("[WSCADA]", "Failed to initialize UDI module: %s", esp_err_to_name(status));
        goto exit;
    }

exit:
    return ESP_OK;
}
