#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#define TRIGGER_PIN GPIO_NUM_5
#define MONITOR_PIN GPIO_NUM_33

#include "lidarlite_v4led.h"

static const char *TAG = "[main]";

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

static void take_range(void);
static void take_range_gpio(void);

static void loop(void *arg)
{
    while (1)
    {
        take_range();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = GPIO_NUM_22,
        .sda_io_num = GPIO_NUM_21,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_ERROR_CHECK(lidarlite_v4led_init(bus_handle, &dev_handle, 0x62));

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

static void take_range(void)
{
    lidarlite_v4led_take_range(dev_handle);
    lidarlite_v4led_wait_for_busy(dev_handle);
    uint16_t distance_cm = lidarlite_v4led_read_distance(dev_handle);
    ESP_LOGI(TAG, "Distance: %d cm", distance_cm);
}

static void take_range_gpio(void)
{
    lidarlite_v4led_take_range_gpio(TRIGGER_PIN, MONITOR_PIN);
    lidarlite_v4led_wait_for_busy(dev_handle);
    uint16_t distance_cm = lidarlite_v4led_read_distance(dev_handle);
    ESP_LOGI(TAG, "Distance: %d cm", distance_cm);
}