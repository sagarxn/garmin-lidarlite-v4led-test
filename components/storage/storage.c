#include "storage.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"

static const char *TAG = "[storage]";

#define WSCADA_QUEUE_FILE_PATH      (LITTLEFS_MOUNT_POINT "/wscada_queue.bin")
#define WSCADA_QUEUE_ENTRY_SIZE     12

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

    printf("--------------------------------------------------------------------------------\n");
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    ESP_LOGI(TAG, "Mount LITTLEFS filesystem on %s", LITTLEFS_MOUNT_POINT);
    printf("--------------------------------------------------------------------------------\n");

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

esp_err_t storage_log_lidar(const char *datetime, uint32_t period_ms, float distance_m)
{
    esp_err_t status = ESP_OK;

    FILE *f = fopen(LITTLEFS_MOUNT_POINT "/lidar_data.csv", "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        status = ESP_FAIL;
        goto exit;
    }

    // Current time, measurement period in ms, lidar distance in m
    fprintf(f, "%s,%lu,%.2f\n", datetime, period_ms, distance_m);
    fclose(f);

exit:
    return status;
}

esp_err_t storage_push_wscada_queue(uint32_t fat32_time, uint32_t interval, float distance)
{
    FILE *f = fopen(WSCADA_QUEUE_FILE_PATH, "ab");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open queue file for writing");
        return ESP_FAIL;
    }

    // Write fields sequentially into flash
    fwrite(&fat32_time, sizeof(uint32_t), 1, f);
    fwrite(&interval, sizeof(uint32_t), 1, f);
    fwrite(&distance, sizeof(float), 1, f);

    fclose(f);
    ESP_LOGI(TAG, "Pushed 12-byte record to unsent queue file");
    return ESP_OK;
}

esp_err_t storage_pop_wscada_queue(uint32_t *out_fat32_time, uint32_t *out_interval, float *out_distance)
{
    if (!out_fat32_time || !out_interval || !out_distance) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(WSCADA_QUEUE_FILE_PATH, "rb");
    if (f == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    // Read the oldest values at the head of the file
    size_t r1 = fread(out_fat32_time, sizeof(uint32_t), 1, f);
    size_t r2 = fread(out_interval, sizeof(uint32_t), 1, f);
    size_t r3 = fread(out_distance, sizeof(float), 1, f);

    if (r1 != 1 || r2 != 1 || r3 != 1)
    {
        fclose(f);
        remove(WSCADA_QUEUE_FILE_PATH);
        return ESP_ERR_NOT_FOUND;
    }

    // Calculate remaining size in file
    fseek(f, 0, SEEK_END);
    long total_size = ftell(f);
    long remaining_bytes = total_size - WSCADA_QUEUE_ENTRY_SIZE;

    if (remaining_bytes <= 0)
    {
        fclose(f);
        remove(WSCADA_QUEUE_FILE_PATH);
    }
    else
    {
        // Shift file content forward by 12 bytes
        uint8_t *buffer = malloc(remaining_bytes);
        if (!buffer)
        {
            fclose(f);
            return ESP_ERR_NO_MEM;
        }

        fseek(f, WSCADA_QUEUE_ENTRY_SIZE, SEEK_SET);
        fread(buffer, 1, remaining_bytes, f);
        fclose(f);

        f = fopen(WSCADA_QUEUE_FILE_PATH, "wb");
        if (f)
        {
            fwrite(buffer, 1, remaining_bytes, f);
            fclose(f);
        }
        free(buffer);
    }

    return ESP_OK;
}

int storage_get_wscada_queue_count(void)
{
    FILE *f = fopen(WSCADA_QUEUE_FILE_PATH, "rb");
    if (f == NULL) return 0;

    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fclose(f);

    if (size <= 0) return 0;
    return size / WSCADA_QUEUE_ENTRY_SIZE;
}