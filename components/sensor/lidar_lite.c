/*------------------------------------------------------------------------------
  lidar_lite.c

  ESP-IDF port of the Garmin LIDAR-Lite Arduino Library.

  Ported from:
    https://github.com/garmin/LIDARLite_Arduino_Library/blob/master/src/LIDARLite.cpp
    https://github.com/garmin/LIDARLite_Arduino_Library/blob/master/src/LIDARLite.h

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
------------------------------------------------------------------------------*/

#include "lidar_lite.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "[lidar_lite]";

/* Status register that carries the busy flag (bit 0) */
#define LIDAR_LITE_REG_STATUS   0x01
#define LIDAR_LITE_REG_ACQ      0x00
#define LIDAR_LITE_REG_DISTANCE 0x8f /* auto-increments through 0x0f/0x10 */

#define LIDAR_LITE_I2C_TIMEOUT_MS 1000
#define LIDAR_LITE_BUSY_POLL_MAX  9999

/*------------------------------------------------------------------------------
  lidar_lite_init

  Adds the LIDAR-Lite as a device on an already-created I2C master bus.
  Equivalent to Wire.begin() + implicit device selection in the Arduino
  library, but the ESP-IDF i2c_master driver requires an explicit per-device
  handle instead of addressing devices ad hoc on every transfer.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_init(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t i2c_addr, uint32_t i2c_clk_hz)
{
    esp_err_t err = ESP_OK;

    if (bus == NULL || dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    if (i2c_addr == 0) {
        i2c_addr = LIDAR_LITE_ADDR_DEFAULT;
    }
    if (i2c_clk_hz == 0) {
        i2c_clk_hz = LIDAR_LITE_I2C_CLK_STANDARD;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = i2c_addr,
        .scl_speed_hz    = i2c_clk_hz,
    };

    err = i2c_master_bus_add_device(bus, &dev_cfg, dev);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to add device: %s", esp_err_to_name(err));
        goto exit;
    }

exit:
    return err;
} /* lidar_lite_init */

/*------------------------------------------------------------------------------
  lidar_lite_deinit
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_deinit(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev)
{
    esp_err_t err = ESP_OK;

    if (bus == NULL || dev == NULL || *dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    err = i2c_master_bus_rm_device(*dev);
    *dev = NULL;

exit:
    return err;
} /* lidar_lite_deinit */

/*------------------------------------------------------------------------------
  lidar_lite_configure

  configuration:
    0: Default mode, balanced performance.
    1: Short range, high speed. Uses 0x1d maximum acquisition count.
    2: Default range, higher speed short range. Turns on quick termination
       detection for faster measurements at short range (with decreased
       accuracy).
    3: Maximum range. Uses 0xff maximum acquisition count.
    4: High sensitivity detection. Overrides default valid measurement
       detection algorithm, and uses a threshold value for high
       sensitivity and noise.
    5: Low sensitivity detection. Overrides default valid measurement
       detection algorithm, and uses a threshold value for low
       sensitivity and noise.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_configure(i2c_master_dev_handle_t dev, lidar_lite_config_t config)
{
    esp_err_t err = ESP_OK;

    if (dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    /* registers 0x02, 0x04, 0x1c, defaulted then overridden per preset */
    uint8_t configs[3] = {0x80, 0x08, 0x00};

    switch (config) {
        case LIDAR_LITE_CONFIG_DEFAULT: /* Default mode, balanced performance */
            break;

        case LIDAR_LITE_CONFIG_SHORT_FAST: /* Short range, high speed */
            configs[0] = 0x1d;
            break;

        case LIDAR_LITE_CONFIG_DEFAULT_FAST: /* Default range, higher speed short range */
            configs[1] = 0x00;
            break;

        case LIDAR_LITE_CONFIG_MAX_RANGE: /* Maximum range */
            configs[0] = 0xff;
            break;

        case LIDAR_LITE_CONFIG_HIGH_SENSITIVITY: /* High sensitivity, more erroneous measurements */
            configs[2] = 0x80;
            break;

        case LIDAR_LITE_CONFIG_LOW_SENSITIVITY: /* Low sensitivity, fewer erroneous measurements */
            configs[2] = 0xb0;
            break;

        default:
            err = ESP_ERR_INVALID_ARG;
            goto exit;
    }

    err = lidar_lite_write_reg(dev, 0x02, configs[0]);
    if (err != ESP_OK) {
        goto exit;
    }
    err = lidar_lite_write_reg(dev, 0x04, configs[1]);
    if (err != ESP_OK) {
        goto exit;
    }
    err = lidar_lite_write_reg(dev, 0x1c, configs[2]);

exit:
    return err;
} /* lidar_lite_configure */

/*------------------------------------------------------------------------------
  lidar_lite_set_i2c_addr

  Set alternate I2C device address. See operating manual for additional info.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_set_i2c_addr(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t new_address, uint8_t disable_default)
{
    esp_err_t err = ESP_OK;
    uint8_t data_bytes[2];

    if (bus == NULL || dev == NULL || *dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    /* Read UNIT_ID serial number bytes and write them into I2C_ID byte locations */
    err = lidar_lite_read_reg(*dev, (0x16 | 0x80), 2, data_bytes, false);
    if (err != ESP_OK) {
        goto exit;
    }
    err = lidar_lite_write_reg(*dev, 0x18, data_bytes[0]);
    if (err != ESP_OK) {
        goto exit;
    }
    err = lidar_lite_write_reg(*dev, 0x19, data_bytes[1]);
    if (err != ESP_OK) {
        goto exit;
    }

    /* Write the new I2C device address to registers */
    err = lidar_lite_write_reg(*dev, 0x1a, new_address);
    if (err != ESP_OK) {
        goto exit;
    }

    /* Enable the new I2C device address using the current I2C device address */
    err = lidar_lite_write_reg(*dev, 0x1e, 0x00);
    if (err != ESP_OK) {
        goto exit;
    }

    /* If desired, disable default I2C device address (using the new I2C device address) */
    if (disable_default)
    {
        /* The device now responds on new_address; re-target the handle */
        err = i2c_master_bus_rm_device(*dev);
        if (err != ESP_OK) {
            goto exit;
        }

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = new_address,
            .scl_speed_hz    = LIDAR_LITE_I2C_CLK_STANDARD,
        };
        err = i2c_master_bus_add_device(bus, &dev_cfg, dev);
        if (err != ESP_OK) {
            goto exit;
        }

        uint8_t disable_bit = (1 << 3); /* set bit to disable default address */
        err = lidar_lite_write_reg(*dev, 0x1e, disable_bit);
    }

exit:
    return err;
} /* lidar_lite_set_i2c_addr */

/*------------------------------------------------------------------------------
  lidar_lite_reset

  Reset device. The device reloads default register settings, including the
  default I2C address. Re-initialization takes approximately 22ms.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_reset(i2c_master_dev_handle_t dev)
{
    esp_err_t err = ESP_OK;

    if (dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    err = lidar_lite_write_reg(dev, 0x00, 0x00);

exit:
    return err;
} /* lidar_lite_reset */

/*------------------------------------------------------------------------------
  lidar_lite_read_distance

  Take a distance measurement and read the result.

  Process:
    1. Write 0x04 or 0x03 to register 0x00 to initiate an acquisition.
    2. Read register 0x01 (handled inside lidar_lite_read_reg)
       - if the first bit is "1" then the sensor is busy, loop until the
         first bit is "0"
       - if the first bit is "0" then the sensor is ready
    3. Read two bytes from register 0x8f and save
    4. Shift the first value from 0x8f << 8 and add to second value from
       0x8f. The result is the measured distance in centimeters.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_read_distance(i2c_master_dev_handle_t dev, bool bias_correction, uint16_t *distance_cm)
{
    esp_err_t err = ESP_OK;
    uint8_t distance_bytes[2];

    if (dev == NULL || distance_cm == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    if (bias_correction) {
        /* Take acquisition & correlation processing with receiver bias correction */
        err = lidar_lite_write_reg(dev, LIDAR_LITE_REG_ACQ, 0x04);
    } else {
        /* Take acquisition & correlation processing without receiver bias correction */
        err = lidar_lite_write_reg(dev, LIDAR_LITE_REG_ACQ, 0x03);
    }
    if (err != ESP_OK) {
        goto exit;
    }

    /* Read two bytes from register 0x8f (auto-increments through 0x0f and 0x10) */
    err = lidar_lite_read_reg(dev, LIDAR_LITE_REG_DISTANCE, 2, distance_bytes, true);
    if (err != ESP_OK) {
        goto exit;
    }

    /* Shift high byte and add to low byte */
    *distance_cm = (uint16_t)((distance_bytes[0] << 8) + distance_bytes[1]);

exit:
    return err;
} /* lidar_lite_read_distance */

/*------------------------------------------------------------------------------
  lidar_lite_write_reg

  Perform I2C write to device.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_write_reg(i2c_master_dev_handle_t dev, uint8_t reg_addr, uint8_t value)
{
    esp_err_t err = ESP_OK;

    if (dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t write_buf[2] = { reg_addr, value };

    err = i2c_master_transmit(dev, write_buf, sizeof(write_buf), LIDAR_LITE_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        /* A nack means the device is not responding, report the error over the log */
        ESP_LOGW(TAG, "> nack");
    }

    vTaskDelay(pdMS_TO_TICKS(1)); /* 1 ms delay for robustness with successive reads and writes */

exit:
    return err;
} /* lidar_lite_write_reg */

/*------------------------------------------------------------------------------
  lidar_lite_read_reg

  Perform I2C read from device. Will detect an unresponsive device and report
  the error over the log. The optional busy flag monitoring can be used to
  read registers that are updated at the end of a distance measurement to
  obtain the new data.
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg_addr,
                               size_t num_bytes,
                               uint8_t *data_out,
                               bool monitor_busy_flag)
{
    esp_err_t err = ESP_OK;
    bool busy;
    int busy_counter = 0; /* counts number of times busy flag is checked, for timeout */

    if (dev == NULL || data_out == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    busy = monitor_busy_flag; /* begin read immediately if not monitoring busy flag */

    while (busy)
    {
        /* Read status register to check busy flag */
        uint8_t status_reg = LIDAR_LITE_REG_STATUS;
        uint8_t status_val = 0;

        err = i2c_master_transmit(dev, &status_reg, 1, LIDAR_LITE_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "> nack");
        }

        err = i2c_master_receive(dev, &status_val, 1, LIDAR_LITE_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "> nack");
        }

        busy = (status_val & 0x01) != 0; /* LSB of the status register is the busy flag */
        busy_counter++;

        if (busy_counter > LIDAR_LITE_BUSY_POLL_MAX)
        {
            ESP_LOGW(TAG, "> read failed");
            err = ESP_ERR_TIMEOUT;
            goto exit;
        }
    }

    /* Device is not busy, begin read */
    err = i2c_master_transmit(dev, &reg_addr, 1, LIDAR_LITE_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "> nack");
        goto exit;
    }

    err = i2c_master_receive(dev, data_out, num_bytes, LIDAR_LITE_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "> nack");
    }

exit:
    return err;
} /* lidar_lite_read_reg */

/*------------------------------------------------------------------------------
  lidar_lite_log_correlation_record

  The correlation record used to calculate distance can be read from the
  device. It has a bipolar wave shape, transitioning from a positive going
  portion to a roughly symmetrical negative going pulse. The point where the
  signal crosses zero represents the effective delay for the reference and
  return signals.

  Process:
    1. Take a distance reading (there is no correlation record without at
       least one distance reading being taken)
    2. Select memory bank by writing 0xc0 to register 0x5d
    3. Set test mode select by writing 0x07 to register 0x40
    4. For as many readings as desired (max 1024):
       1. Read two bytes from 0xd2
       2. The low byte is the value from the record
       3. The high byte is the sign from the record
------------------------------------------------------------------------------*/
esp_err_t lidar_lite_log_correlation_record(i2c_master_dev_handle_t dev, size_t num_readings)
{
    esp_err_t err = ESP_OK;
    esp_err_t disable_err = ESP_OK;

    if (dev == NULL)
    {
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    /* Select memory bank */
    err = lidar_lite_write_reg(dev, 0x5d, 0xc0);
    if (err != ESP_OK) {
        goto exit;
    }

    /* Test mode enable */
    err = lidar_lite_write_reg(dev, 0x40, 0x07);
    if (err != ESP_OK) {
        goto exit;
    }

    for (size_t i = 0; i < num_readings; i++)
    {
        uint8_t correlation_bytes[2];
        err = lidar_lite_read_reg(dev, 0xd2, 2, correlation_bytes, false);
        if (err != ESP_OK) {
            break;
        }

        /* Low byte is the value of the correlation record */
        int16_t correlation_value = correlation_bytes[0];

        /* If upper byte lsb is set, the value is negative */
        if (correlation_bytes[1] == 1) {
            correlation_value |= 0xff00;
        }

        ESP_LOGI(TAG, "%d", correlation_value);
    }

    /* Test mode disable */
    disable_err = lidar_lite_write_reg(dev, 0x40, 0x00);
    if (err == ESP_OK) {
        err = disable_err;
    }

exit:
    return err;
} /* lidar_lite_log_correlation_record */
