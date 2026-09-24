#include "sensor.h"

#include <stdio.h>
#include <string.h>
#include "sys/time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lidarlite_v4led.h"
#include "lidar_lite.h"
#include "board.h"
#include "storage.h"
#include "wscada.h"
#include "util.h"

#define TSS_EN                      0

#define TARGET_INTERVAL_MS          (1UL * 1000UL) 

#define START_YEAR                  2026 
#define START_MONTH                 7      // 1 to 12
#define START_DAY                   15     // 1 to 31
#define START_HOUR                  0      // 0 to 23
#define START_MINUTE                0      // 0 to 59
#define START_SECOND                0      // 0 to 59

static const char *TAG = "[sensor]";
static TaskHandle_t _gp_s_task_handle = NULL;

static void _read_task(void *pvParameters);
static void _read_sensor(void);
static void _flush_wscada_queue(void);

esp_err_t sensor_init(void)
{
    esp_err_t status = ESP_OK;

    vTaskDelay(pdMS_TO_TICKS(1000)); /* Wait for lidar to start */

    i2c_master_bus_handle_t ps_lidar_i2c_bus_handle = board_get_lidar_i2c_bus_handle();
    i2c_master_dev_handle_t ps_lidar_v4_i2c_dev_handle = board_get_lidar_v4_i2c_dev_handle();
    i2c_master_dev_handle_t ps_lidar_v3_i2c_dev_handle = board_get_lidar_v3_i2c_dev_handle();

    // ESP_ERROR_CHECK(lidarlite_v4led_reset(ps_lidar_i2c_bus_handle, &ps_lidar_v4_i2c_dev_handle));
    // board_set_lidar_v4_i2c_dev_handle(ps_lidar_v4_i2c_dev_handle);
    // ESP_ERROR_CHECK(lidarlite_v4led_set_i2c_addr(ps_lidar_i2c_bus_handle, &ps_lidar_v4_i2c_dev_handle, 0x63, 1));
    // board_set_lidar_v4_i2c_dev_handle(ps_lidar_v4_i2c_dev_handle);
    
    ESP_ERROR_CHECK(lidarlite_v4led_configure(ps_lidar_v4_i2c_dev_handle, 0));
    // lidar_lite_configure(ps_lidar_v3_i2c_dev_handle, 0);

    uint8_t acc_mode;
    ESP_ERROR_CHECK(lidarlite_v4led_read_high_accuracy_mode(ps_lidar_v4_i2c_dev_handle, &acc_mode));
    ESP_LOGI(TAG, "LIDAR V4 High Accuracy Mode: %u", acc_mode);

    acc_mode = 0xFF;
    ESP_ERROR_CHECK(lidarlite_v4led_set_high_accuracy_mode(ps_lidar_v4_i2c_dev_handle, acc_mode));

    ESP_ERROR_CHECK(lidarlite_v4led_read_high_accuracy_mode(ps_lidar_v4_i2c_dev_handle, &acc_mode));
    ESP_LOGI(TAG, "LIDAR V4 High Accuracy Mode: %u", acc_mode);

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
    }

    return status;
}

static void _read_task(void *pvParameters)
{
    (void)pvParameters;

    struct tm start_tm = {
        .tm_sec   = START_SECOND,
                .tm_min   = START_MINUTE,
        .tm_hour  = START_HOUR,
        .tm_mday  = START_DAY,
        .tm_mon   = START_MONTH - 1,
        .tm_year  = START_YEAR - 1900,
        .tm_isdst = -1
    };

    time_t start_sec = mktime(&start_tm);
    uint64_t start_ms = (uint64_t)start_sec * 1000ULL;

    TickType_t last_read_time = xTaskGetTickCount();

    while (1)
    {

        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        uint64_t now_ms = ((uint64_t)tv_now.tv_sec * 1000ULL) + (tv_now.tv_usec / 1000);

        uint32_t ms_until_next_boundary = 0;

        if (now_ms < start_ms)
        {
            ms_until_next_boundary = (uint32_t)(start_ms - now_ms);
        }
        else
        {
            uint64_t elapsed_since_start = now_ms - start_ms;
            ms_until_next_boundary = TARGET_INTERVAL_MS - (elapsed_since_start % TARGET_INTERVAL_MS);
        }

        TickType_t ticks_to_delay = pdMS_TO_TICKS(ms_until_next_boundary);
        
        time_t next_sec = (time_t)((now_ms + ms_until_next_boundary) / 1000ULL);
        
        struct tm timeinfo_now;
        struct tm timeinfo_next;
        localtime_r(&tv_now.tv_sec, &timeinfo_now);
        localtime_r(&next_sec, &timeinfo_next);

        char str_now[32];
        char str_next[32];
        strftime(str_now, sizeof(str_now), "%Y-%m-%d %H:%M:%S", &timeinfo_now);
        strftime(str_next, sizeof(str_next), "%Y-%m-%d %H:%M:%S", &timeinfo_next);

        printf("--------------------------------------------------------------------------------\n");
        ESP_LOGI(TAG, "Current time: %s | Next trigger at: %s (in %lu ms)", 
                 str_now, str_next, ms_until_next_boundary);
        printf("--------------------------------------------------------------------------------\n");

        if (ticks_to_delay > 0)
        {
            vTaskDelayUntil(&last_read_time, ticks_to_delay);
        }

        _read_sensor(); 
    }
}

static void _read_sensor(void)
{
    i2c_master_dev_handle_t ps_lidar_v4_i2c_dev_handle = board_get_lidar_v4_i2c_dev_handle();

    time_t now;
    time(&now);
    struct tm timeinfo_now;
    localtime_r(&now, &timeinfo_now);

    char str_now[32];
    strftime(str_now, sizeof(str_now), "%Y:%m:%d-%H:%M:%S", &timeinfo_now);

    uint32_t start_time = xTaskGetTickCount();
    lidarlite_v4led_take_range(ps_lidar_v4_i2c_dev_handle);
    lidarlite_v4led_wait_for_busy(ps_lidar_v4_i2c_dev_handle, 1000);
    uint32_t period_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start_time);

    uint16_t distance_cm = 0;
    lidarlite_v4led_read_distance(ps_lidar_v4_i2c_dev_handle, &distance_cm);

    uint32_t fat32_time = util_tm_to_fat32_time(&timeinfo_now);
    float distance_m = distance_cm / 100.0f;

    printf("== LIDAR V4 ====================================================================\n");
    ESP_LOGI(TAG, "%s, %lu, %.2f", str_now, period_ms, distance_m);
    printf("================================================================================\n");

    static int cnt = 0;

#if TSS_EN
    if (cnt++ == 60)
    {
        cnt = 0;
        storage_log_lidar(str_now, period_ms, distance_m);
        if (wscada_v1_post_lidar(fat32_time, period_ms, distance_m) != ESP_OK)
        {
            ESP_LOGW(TAG, "Post failed! Storing variables in flash queue.");
            storage_push_wscada_queue(fat32_time, period_ms, distance_m);
        }
        else
        {
            _flush_wscada_queue();
        }
    }
#endif

    // i2c_master_dev_handle_t ps_lidar_v3_i2c_dev_handle = board_get_lidar_v3_i2c_dev_handle();

    // start_time = xTaskGetTickCount();
    // ESP_ERROR_CHECK(lidar_lite_read_distance(ps_lidar_v3_i2c_dev_handle, true, &distance_cm));
        // period_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start_time);

    // distance_m = distance_cm / 100.0f;
    // printf("==  LIDAR V3 ===================================================================\n");
    // ESP_LOGI(TAG, "%s, %lu, %.2f", str_now, period_ms, distance_m);
    // printf("================================================================================\n");
}

static void _flush_wscada_queue(void)
{
    size_t pending_count = storage_get_wscada_queue_count();
    if (pending_count == 0) return;

    ESP_LOGI(TAG, "Flushing %u queued records to WSCADA...", pending_count);

    uint32_t fat32_time = 0;
    uint32_t interval = 0;
    float distance = 0.0f;

    while (storage_pop_wscada_queue(&fat32_time, &interval, &distance) == ESP_OK)
    {
        esp_err_t err = wscada_v1_post_lidar(fat32_time, interval, distance); //[cite: 3]
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Connection lost during flush. Re-queueing record.");
            storage_push_wscada_queue(fat32_time, interval, distance);
            break;
        }

        ESP_LOGI(TAG, "Successfully synced 1 queued record.");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}