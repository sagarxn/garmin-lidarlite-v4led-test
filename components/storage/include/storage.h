#pragma once

#include "esp_err.h"

#define LITTLEFS_MOUNT_POINT    "/littlefs"
#define SD_MOUNT_POINT          "/sdcard"

esp_err_t storage_init(void);

esp_err_t storage_read_settings(setting_t *settings);
esp_err_t storage_save_settings(const setting_t *settings);
esp_err_t storage_log_lidar(const char *datetime, uint32_t period_ms, float distance_m);

esp_err_t storage_push_wscada_queue(uint32_t fat32_time, uint32_t interval, float distance);
esp_err_t storage_pop_wscada_queue(uint32_t *out_fat32_time, uint32_t *out_interval, float *out_distance);
int storage_get_wscada_queue_count(void);