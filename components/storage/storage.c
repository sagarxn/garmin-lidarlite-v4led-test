#include "storage.h"

#include <stdio.h>
#include <string.h>

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "[storage]";

static esp_err_t _littlefs_init();
static esp_err_t _sdcard_init(void);

esp_err_t storage_init(void)
{
    esp_err_t status = ESP_OK;

    status = nvs_flash_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(status));
        goto exit;
    }

    status = _littlefs_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LittleFS: %s", esp_err_to_name(status));
        goto exit;
    }

    // status = _sdcard_init();
    // if (status != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(status));
    // } 

exit:
    return status;
}

static esp_err_t _littlefs_init()
{
    esp_err_t status = ESP_OK;

    ESP_LOGI(TAG, "Initializing LittleFS file system on Builtin SPI Flash Memory");

    esp_vfs_littlefs_conf_t s_conf = {
        .base_path = LITTLEFS_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    status = esp_vfs_littlefs_register(&s_conf);

    if (status != ESP_OK)
    {
        if (status == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (status == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(status));
        }
        
        goto exit;
    }

    size_t total = 0, used = 0;
    status = esp_littlefs_info(s_conf.partition_label, &total, &used);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(status));
        esp_littlefs_format(s_conf.partition_label);
        goto exit;
    }

    printf("================================================================================\n");
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    ESP_LOGI(TAG, "Mount LITTLEFS filesystem on %s", LITTLEFS_MOUNT_POINT);
    printf("================================================================================\n");

exit:
    return status;
}

static esp_err_t _sdcard_init(void)
{
    esp_err_t status = ESP_OK;

    esp_vfs_fat_sdmmc_mount_config_t s_mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *ps_card;
    ESP_LOGI(TAG, "Initializing SDMMC peripheral...");

    sdmmc_host_t s_host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t s_slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    s_slot_config.width = 4;

    status = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &s_host, &s_slot_config, &s_mount_config, &ps_card);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount filesystem (%s).", esp_err_to_name(status));
        goto exit;
    }

    sdmmc_card_print_info(stdout, ps_card);

exit:
    return status;
}
