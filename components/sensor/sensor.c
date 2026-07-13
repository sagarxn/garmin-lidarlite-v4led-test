#include "sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board.h"
#include "lidarlite_v4led.h"

#define TIMER_PERIOD_MS             1000UL
#define LIDAR_BUSY_WAIT_TIMEOUT_MS  500UL

const char *TAG = "[sensor]";

static TaskHandle_t _gp_s_task_handle = NULL;
static TimerHandle_t _gp_s_timer_handle = NULL;

static void _read_task(void *pvParameters);
static void _timer_clbk(TimerHandle_t xTimer);
static void _read_sensor(void);

esp_err_t sensor_init(void)
{
    esp_err_t status = ESP_OK;

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
    i2c_master_dev_handle_t lidar_i2c_dev_handle = board_get_lidar_i2c_dev_handle();

    uint32_t start_time = xTaskGetTickCount();
    lidarlite_v4led_take_range(lidar_i2c_dev_handle);
    lidarlite_v4led_wait_for_busy(lidar_i2c_dev_handle, 10000);

    uint16_t distance_cm = lidarlite_v4led_read_distance(lidar_i2c_dev_handle);
    uint32_t period_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start_time);

    ESP_LOGI(TAG, "Distance: %d cm, Period: %lu ms", distance_cm, period_ms);
}