#include "board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "bsp.h"

static char *TAG = "[board]";

static i2c_master_dev_handle_t _gp_s_lidar_i2c_dev_handle = NULL;

static esp_err_t _gpio_init(void);
static esp_err_t _i2c_init(void);

esp_err_t board_init (void)
{
    esp_err_t status = ESP_OK;

    status = _gpio_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize GPIO: %s", esp_err_to_name(status));
        goto exit;
    }

    status = _i2c_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(status));
        goto exit;
    }

exit:
    return status;
}

i2c_master_dev_handle_t board_get_lidar_i2c_dev_handle (void)
{
    return _gp_s_lidar_i2c_dev_handle;
}

static esp_err_t _gpio_init(void)
{
    esp_err_t status = ESP_OK;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_HEART_LED_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    status = gpio_config(&io_conf);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPIO for heart LED: %s", esp_err_to_name(status));
        goto exit;
    }

    gpio_set_level(BOARD_HEART_LED_PIN, 0);

exit:
    return status;
}

static esp_err_t _i2c_init(void)
{
    esp_err_t status = ESP_OK;

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BOARD_LIDAR_I2C_SCL_PIN,
        .sda_io_num = BOARD_LIDAR_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    i2c_master_bus_handle_t bus_handle;
    status = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(status));
        goto exit;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BOARD_LIDAR_I2C_ADDR,
        .scl_speed_hz    = BOARD_LIDAR_I2C_SCL_SPEED_HZ,
    };

    status = i2c_master_bus_add_device(bus_handle, &dev_cfg, &_gp_s_lidar_i2c_dev_handle);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add LIDAR device to I2C bus: %s", esp_err_to_name(status));
    }

exit:
    return status;
}
