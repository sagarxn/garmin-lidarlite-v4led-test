#include "esp_check.h"

#include "board.h"
#include "sensor.h"

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());

    ESP_ERROR_CHECK(sensor_init());
}