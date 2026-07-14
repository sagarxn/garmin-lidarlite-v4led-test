#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t wscada_v1_post_lidar(uint32_t fat32_time, uint32_t interval, float distance);