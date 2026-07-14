#include "sensor.h"

#include <stdio.h>
#include <string.h>
#include "sys/time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board.h"
#include "lidarlite_v4led.h"
#include "storage.h"

#define TIMER_PERIOD_MS             1000UL
#define LIDAR_BUSY_WAIT_TIMEOUT_MS  500UL

static const char *TAG = "[sensor]";

static TaskHandle_t _gp_s_task_handle = NULL;
static TimerHandle_t _gp_s_timer_handle = NULL;

static void _read_task(void *pvParameters);
static void _timer_clbk(TimerHandle_t xTimer);
static void _read_sensor(void);

esp_err_t sensor_init(void)
{
    esp_err_t status = ESP_OK;

    vTaskDelay(pdMS_TO_TICKS(1000)); /* Wait for lidar to start */

    BaseType_t task_status = xTaskCreate(_read_task,
                                    "loop",
                                    4096,
                                    NULL,
                                    5,
                                    &_gp_s_task_handle);
    if (pdPASS != task_status)
    {
        ESP_LOGE(TAG, "Failed to create loop task");
        status = ESP_FAIL;
        goto exit;
    }

    _gp_s_timer_handle = xTimerCreate("sensor timer",
                                       pdMS_TO_TICKS(TIMER_PERIOD_MS),
                                       pdTRUE,
                                       NULL,
                                       _timer_clbk);
    if (NULL != _gp_s_timer_handle)
    {
        xTimerStart(_gp_s_timer_handle, 0);
    }
    else
    {
        status = ESP_FAIL;
        ESP_LOGE(TAG, "Failed to create timer");
    }

exit:
    return status;
}

static void _read_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        _read_sensor(); 
    }
}

static void _timer_clbk(TimerHandle_t xTimer)
{
    (void)xTimer;
    xTaskNotify(_gp_s_task_handle, 0, eNoAction);
}

static void _read_sensor(void)
{
    i2c_master_dev_handle_t ps_lidar_i2c_dev_handle = board_get_lidar_i2c_dev_handle();

    time_t now;
    time(&now);
    struct tm timeinfo_now;
    localtime_r(&now, &timeinfo_now);

    char str_now[32];
    strftime(str_now, sizeof(str_now), "%Y:%m:%d-%H:%M:%S", &timeinfo_now);

    uint32_t start_time = xTaskGetTickCount();
    lidarlite_v4led_take_range(ps_lidar_i2c_dev_handle);
    lidarlite_v4led_wait_for_busy(ps_lidar_i2c_dev_handle, 10000);

    uint16_t distance_cm = lidarlite_v4led_read_distance(ps_lidar_i2c_dev_handle);
    uint32_t period_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start_time);

    FILE *f = fopen(LITTLEFS_MOUNT_POINT "/lidar_data.csv", "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Current time, measurement period in ms, lidar distance in m
    fprintf(f, "%s,%lu,%.2f\n", str_now, period_ms, distance_cm / 100.0f);
    ESP_LOGI(TAG, "%s, %lu, %.2f", str_now, period_ms, distance_cm / 100.0f);

    fclose(f);
}