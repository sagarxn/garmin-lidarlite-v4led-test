/*------------------------------------------------------------------------------

  Garmin Lidar-Lite V4 LED ESP-IDF Driver
  lidarlite_v4led.h

  This library is port of the Garmin LIDARLite_v4LED Arduino library to ESP-IDF,
  using the new i2c_master.h bus/device driver API.

  Original Copyright (c) 2019 Garmin Ltd. or its subsidiaries.
  Modified on July 13, 2026 by Sagar Chaudhary for ESP-IDF

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

------------------------------------------------------------------------------*/

#ifndef LIDARLITE_V4LED_H
#define LIDARLITE_V4LED_H

#include <stdint.h>

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define LIDARLITE_ADDR_DEFAULT 0x62

esp_err_t lidarlite_v4led_init(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t addr);
esp_err_t lidarlite_v4led_deinit(i2c_master_dev_handle_t dev);
esp_err_t lidarlite_v4led_update_address(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t new_addr);

esp_err_t lidarlite_v4led_reset(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t* dev);
esp_err_t lidarlite_v4led_configure(i2c_master_dev_handle_t dev, uint8_t config);
esp_err_t lidarlite_v4led_set_i2c_addr(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t new_addr, uint8_t disable_default);
esp_err_t lidarlite_v4led_read_distance(i2c_master_dev_handle_t dev, uint16_t *distance);
esp_err_t lidarlite_v4led_wait_for_busy(i2c_master_dev_handle_t dev, uint32_t timeout_ms);
uint8_t lidarlite_v4led_get_busy_flag(i2c_master_dev_handle_t dev);
esp_err_t lidarlite_v4led_take_range(i2c_master_dev_handle_t dev);
esp_err_t lidarlite_v4led_read_temperature(i2c_master_dev_handle_t dev, int8_t *temperature);
esp_err_t lidarlite_v4led_set_high_accuracy_mode(i2c_master_dev_handle_t dev, uint8_t value);
esp_err_t lidarlite_v4led_read_high_accuracy_mode(i2c_master_dev_handle_t dev, uint8_t *value);
esp_err_t lidarlite_v4led_set_power_mode(i2c_master_dev_handle_t dev, uint8_t mode);
esp_err_t lidarlite_v4led_read_power_mode(i2c_master_dev_handle_t dev, uint8_t * mode);

esp_err_t lidarlite_v4led_wait_for_busy_gpio(gpio_num_t monitor_pin, uint32_t timeout_ms);
uint8_t lidarlite_v4led_get_busy_flag_gpio(gpio_num_t monitor_pin);
esp_err_t lidarlite_v4led_take_range_gpio(gpio_num_t trigger_pin, gpio_num_t monitor_pin, uint32_t timeout_ms);

esp_err_t lidarlite_v4led_write(i2c_master_dev_handle_t dev, uint8_t reg_addr, const uint8_t *dataBytes, uint8_t num_bytes);
esp_err_t lidarlite_v4led_read(i2c_master_dev_handle_t dev, uint8_t reg_addr, uint8_t *data_bytes, uint8_t num_bytes);

#endif // LIDARLITE_V4LED_H