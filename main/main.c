#include "esp_check.h"

#include "board.h"
#include "storage.h"
#include "sensor.h"
#include "cm.h"

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());

    ESP_ERROR_CHECK(storage_init());
    
    ESP_ERROR_CHECK(cm_init());

    ESP_ERROR_CHECK(sensor_init());
}
