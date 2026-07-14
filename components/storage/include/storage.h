#pragma once

#include "esp_err.h"

#define LITTLEFS_MOUNT_POINT    "/littlefs"
#define SD_MOUNT_POINT          "/sdcard"

esp_err_t storage_init(void);