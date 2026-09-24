/*------------------------------------------------------------------------------
  lidar_lite.h

  ESP-IDF port of the Garmin LIDAR-Lite Arduino Library.

  Ported from:
    https://github.com/garmin/LIDARLite_Arduino_Library/blob/master/src/LIDARLite.h
    https://github.com/garmin/LIDARLite_Arduino_Library/blob/master/src/LIDARLite.cpp

  Original copyright (c) 2016 Garmin Ltd. or its subsidiaries.
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This port targets the new ESP-IDF `i2c_master` driver (driver/i2c_master.h,
  ESP-IDF >= 5.2) and uses snake_case naming throughout.
------------------------------------------------------------------------------*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#define LIDAR_LITE_ADDR_DEFAULT 0x62
#define LIDAR_LITE_I2C_CLK_STANDARD 100000
#define LIDAR_LITE_I2C_CLK_FAST     400000

typedef enum {
    LIDAR_LITE_CONFIG_DEFAULT           = 0,
    LIDAR_LITE_CONFIG_SHORT_FAST        = 1,
    LIDAR_LITE_CONFIG_DEFAULT_FAST      = 2,
    LIDAR_LITE_CONFIG_MAX_RANGE         = 3,
    LIDAR_LITE_CONFIG_HIGH_SENSITIVITY  = 4,
    LIDAR_LITE_CONFIG_LOW_SENSITIVITY   = 5
} lidar_lite_config_t;


esp_err_t lidar_lite_init(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t i2c_addr, uint32_t i2c_clk_hz);
esp_err_t lidar_lite_deinit(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev);

esp_err_t lidar_lite_configure(i2c_master_dev_handle_t dev, lidar_lite_config_t config);
esp_err_t lidar_lite_set_i2c_addr(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t new_address, uint8_t disable_default);
esp_err_t lidar_lite_reset(i2c_master_dev_handle_t dev);

esp_err_t lidar_lite_read_distance(i2c_master_dev_handle_t dev, bool bias_correction, uint16_t *distance_cm);

esp_err_t lidar_lite_write_reg(i2c_master_dev_handle_t dev, uint8_t reg_addr, uint8_t value);
esp_err_t lidar_lite_read_reg(i2c_master_dev_handle_t dev, uint8_t reg_addr, size_t num_bytes, uint8_t *data_out, bool monitor_busy_flag);

esp_err_t lidar_lite_log_correlation_record(i2c_master_dev_handle_t dev, size_t num_readings);