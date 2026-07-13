#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#define SDA_PIN GPIO_NUM_16
#define SCL_PIN GPIO_NUM_17
#define TRIGGER_PIN GPIO_NUM_5
#define MONITOR_PIN GPIO_NUM_33

#include "lidarlite_v4led.h"

static const char *TAG = "[main]";

static i2c_master_bus_handle_t gp_s_bus_handle;
static i2c_master_dev_handle_t gp_s_dev_handle;

static void loop(void *arg);
static void take_range(void);
static void take_range_gpio(void);

void app_main(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &gp_s_bus_handle));
    ESP_ERROR_CHECK(lidarlite_v4led_init(gp_s_bus_handle, &gp_s_dev_handle, 0x62));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TRIGGER_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.pin_bit_mask = (1ULL << MONITOR_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    vTaskDelay(pdMS_TO_TICKS(100));
    xTaskCreate(loop, "loop", 4096, NULL, 5, NULL);
}

static void loop(void *arg)
{
    while (1)
    {
        take_range();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void take_range(void)
{
    lidarlite_v4led_take_range(gp_s_dev_handle);
    lidarlite_v4led_wait_for_busy(gp_s_dev_handle, 1000);
    uint16_t distance_cm = lidarlite_v4led_read_distance(gp_s_dev_handle);
    ESP_LOGI(TAG, "Distance: %d cm", distance_cm);
}

static void take_range_gpio(void)
{
    lidarlite_v4led_take_range_gpio(TRIGGER_PIN, MONITOR_PIN, 1000);
    lidarlite_v4led_wait_for_busy(gp_s_dev_handle, 1000);
    uint16_t distance_cm = lidarlite_v4led_read_distance(gp_s_dev_handle);
    ESP_LOGI(TAG, "Distance: %d cm", distance_cm);
}