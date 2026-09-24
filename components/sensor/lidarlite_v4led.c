/*------------------------------------------------------------------------------

  Garmin Lidar-Lite V4 LED ESP-IDF Driver
  lidarlite_v4led.c

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

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lidarlite_v4led.h"

#define I2C_TIMEOUT_MS          1000
#define I2C_DEV_SCL_HZ          400000

static const char *TAG = "[lidarlite_v4led]";

/*------------------------------------------------------------------------------
  Init

  Add the LIDAR-Lite as a device on an already-created I2C master bus.

  Parameters
  ------------------------------------------------------------------------------
  bus_handle: an already-initialized i2c_master_bus_handle_t
  addr:       I2C device address, e.g. LIDARLITE_ADDR_DEFAULT
  dev_handle: pointer to store the resulting device handle
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_init(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = I2C_DEV_SCL_HZ,
    };

    return i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle);
}

/*------------------------------------------------------------------------------
  Deinit

  Remove the LIDAR-Lite device from its I2C master bus.

  Parameters
  ------------------------------------------------------------------------------
  dev_handle: device handle returned by lidarlite_v4led_init
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_deinit(i2c_master_dev_handle_t dev_handle)
{
    return i2c_master_bus_rm_device(dev_handle);
}

/*------------------------------------------------------------------------------
  Update Address

  Swap dev_handle for one bound to new_address. Call this after the
  device's I2C address has actually been changed on the bus (see
  lidarlite_v4led_set_i2c_addr).

  Parameters
  ------------------------------------------------------------------------------
  bus_handle:  the I2C master bus the device lives on
  dev_handle:  pointer to the device handle to rebind
  new_addr:    I2C address to rebind this handle to
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_update_address(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev, uint8_t new_addr)
{
    esp_err_t ret;
    i2c_master_dev_handle_t new_dev;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = new_addr,
        .scl_speed_hz    = I2C_DEV_SCL_HZ,
    };

    ret = i2c_master_bus_add_device(bus, &dev_cfg, &new_dev);
    if (ret != ESP_OK)
    {
        goto exit;
    }

    ret = i2c_master_bus_rm_device(*dev);
    if (ret != ESP_OK)
    {
        i2c_master_bus_rm_device(new_dev);
        goto exit;
    }

    *dev = new_dev;

exit:
    return ret;
} /* lidarlite_v4led_update_address */

/**
 * Reset the LIDAR-Lite device to factory defaults. This will reset the I2C address to the default value of 0x62.
 *
 * Parameters
 * ------------------------------------------------------------------------------
 * dev: device handle
 */
esp_err_t lidarlite_v4led_reset(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *dev)
{
    uint8_t reset_cmd = 0x00;
    esp_err_t ret = lidarlite_v4led_write(*dev, 0x00, &reset_cmd, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset LIDAR-Lite device: %s", esp_err_to_name(ret));
        goto exit;
    }

    ret = lidarlite_v4led_update_address(bus, dev, LIDARLITE_ADDR_DEFAULT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update LIDAR-Lite device address: %s", esp_err_to_name(ret));
    }

exit:
    return ret;
} /* lidarlite_v4led_reset */

/*------------------------------------------------------------------------------
  Configure

  Selects one of several preset configurations.

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
  config:
    0: Maximum range. Uses maximum acquisition count.
    1: Balanced performance.
    2: Short range, high speed. Reduces maximum acquisition count.
    3: Mid range, higher speed. Turns on quick termination
         detection for faster measurements at short range (with decreased
         accuracy)
    4: Maximum range, higher speed on short range targets. Turns on quick
         termination detection for faster measurements at short range (with
         decreased accuracy)
    5: Very short range, higher speed, high error. Reduces maximum
         acquisition count to a minimum for faster rep rates on very
         close targets with high error.
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_configure(i2c_master_dev_handle_t dev, uint8_t config)
{
    uint8_t sig_count_max;
    uint8_t acq_config_reg;

    switch (config)
    {
        case 0: /* Default mode - Maximum range */
            sig_count_max  = 0xff;
            acq_config_reg = 0x08;
            break;

        case 1: /* Balanced performance */
            sig_count_max  = 0x80;
            acq_config_reg = 0x08;
            break;

        case 2: /* Short range, high speed */
            sig_count_max  = 0x18;
            acq_config_reg = 0x00;
            break;

        case 3: /* Mid range, higher speed on short range targets */
            sig_count_max  = 0x80;
            acq_config_reg = 0x00;
            break;

        case 4: /* Maximum range, higher speed on short range targets */
            sig_count_max  = 0xff;
            acq_config_reg = 0x00;
            break;

        case 5: /* Very short range, higher speed, high error */
            sig_count_max  = 0x04;
            acq_config_reg = 0x00;
            break;

        default:
            sig_count_max  = 0xff;
            acq_config_reg = 0x08;
            break;
    }

    esp_err_t ret = lidarlite_v4led_write(dev, 0x05, &sig_count_max , 1);
    if (ret != ESP_OK)
    {
        goto exit;
    }

    ret = lidarlite_v4led_write(dev, 0xE5, &acq_config_reg, 1);

exit:
    return ret;
} /* lidarlite_v4led_configure */

/*------------------------------------------------------------------------------
  Set I2C Address

  Set Alternate I2C Device Address. See Operation Manual for additional info.
  Disables the default address space first if requested, ensures the internal
  flash sequence binds properly, then rebinds the master driver handle.

  Parameters
  ------------------------------------------------------------------------------
  bus_handle:      the I2C master bus the device lives on
  dev_handle:      pointer to device handle (must currently be bound to the
                   device's *current* address)
  new_addr:        desired secondary I2C device address
  disable_default: a non-zero value here means the default 0x62 I2C device
    address will be disabled.
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_set_i2c_addr(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t new_addr, uint8_t disable_default)
{
    uint8_t data_bytes[5];
    esp_err_t ret;

    /* Open the Flash writing configuration state window */
    data_bytes[0] = 0x11;
    ret = lidarlite_v4led_write(*dev_handle, 0xEA, data_bytes, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to enable flash storage: %s", esp_err_to_name(ret));
        goto exit;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* If requested, toggle off default response behavior BEFORE address swap */
    if (disable_default)
    {
        data_bytes[0] = 0x01; /* Set the bit to disable default response at 0x62 */
        ret = lidarlite_v4led_write(*dev_handle, 0x1B, data_bytes, 1);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to write 0x1B default bypass register: %s", esp_err_to_name(ret));
            goto exit;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Read the 4-byte unique device serial number hardware layout */
    ret = lidarlite_v4led_read(*dev_handle, 0x16, data_bytes, 4);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read device serial number: %s", esp_err_to_name(ret));
        goto exit;
    }

    /* Append desired new address as the 5th byte element */
    data_bytes[4] = new_addr;

    /* Issue the block configuration write to burn the secondary address target */
    ret = lidarlite_v4led_write(*dev_handle, 0x16, data_bytes, 5);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to write serial number and new address: %s", esp_err_to_name(ret));
        goto exit;
    }

    /* Close the Flash writing loop using the original handle address config */
    data_bytes[0] = 0x00;
    ret = lidarlite_v4led_write(*dev_handle, 0xEA, data_bytes, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to disable flash storage: %s", esp_err_to_name(ret));
        goto exit;
    }

    /* Give the MCU internal hardware time to fully reboot its hardware peripheral layer */
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Rebind ESP32 master peripheral runtime pointer context to the new address space */
    ret = lidarlite_v4led_update_address(bus_handle, dev_handle, new_addr);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to update driver runtime handle target address: %s", esp_err_to_name(ret));
        goto exit;
    }

exit:
    return ret;
} /* lidarlite_v4led_set_i2c_addr */

/*------------------------------------------------------------------------------
  Take Range

  Initiate a distance measurement by writing to register 0x00.

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_take_range(i2c_master_dev_handle_t dev)
{
    uint8_t data_byte = 0x04;

    return lidarlite_v4led_write(dev, 0x00, &data_byte, 1);
} /* lidarlite_v4led_take_range */

/*------------------------------------------------------------------------------
  Wait for Busy Flag

  Blocking function to wait until the Lidar Lite's internal busy flag goes low

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
  timeout_ms: timeout in milliseconds
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_wait_for_busy(i2c_master_dev_handle_t dev, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    uint8_t busy_flag;
    uint32_t timer_start = xTaskGetTickCount();

    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag(dev);
        vTaskDelay(pdMS_TO_TICKS(1));

        if ((xTaskGetTickCount() - timer_start) >= pdMS_TO_TICKS(timeout_ms))
        {
            ret = ESP_ERR_TIMEOUT;
            ESP_LOGW(TAG, "Timeout waiting for busy flag to go LOW!\n");
            break;
        }
    } while (busy_flag);

    return ret;
} /* lidarlite_v4led_wait_for_busy */

/*------------------------------------------------------------------------------
  Get Busy Flag

  Read BUSY flag from device registers. Function will return 0x00 if not busy.

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
------------------------------------------------------------------------------*/
uint8_t lidarlite_v4led_get_busy_flag(i2c_master_dev_handle_t dev)
{
    uint8_t status_byte = 0;
    uint8_t busy_flag; /* busy_flag monitors when the device is done with a measurement */

    /* Read status register to check busy flag */
    lidarlite_v4led_read(dev, 0x01, &status_byte, 1);

    /* STATUS bit 0 is busy_flag */
    busy_flag = status_byte & 0x01;

    return busy_flag;
} /* lidarlite_v4led_get_busy_flag */

/*------------------------------------------------------------------------------
  Take Range using Trigger / Monitor Pins

  Initiate a distance measurement by toggling the trigger pin

  Parameters
  ------------------------------------------------------------------------------
  trigger_pin: digital output pin connected to trigger input of LIDAR-Lite
  monitor_pin: digital input pin connected to monitor output of LIDAR-Lite
  timeout_ms: timeout in milliseconds
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_take_range_gpio(gpio_num_t trigger_pin, gpio_num_t monitor_pin, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    uint8_t busy_flag;

    if (gpio_get_level(trigger_pin))
    {
        gpio_set_level(trigger_pin, 0);
    }
    else
    {
        gpio_set_level(trigger_pin, 1);
    }

    uint32_t timer_start = xTaskGetTickCount();

    // When LLv4 receives trigger command it will drive monitor pin low.
    // Wait for LLv4 to acknowledge receipt of command before moving on.
    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag_gpio(monitor_pin);
        vTaskDelay(pdMS_TO_TICKS(1));
        
        if ((xTaskGetTickCount() - timer_start) >= pdMS_TO_TICKS(timeout_ms))
        {
            ret = ESP_ERR_TIMEOUT;
            ESP_LOGW(TAG, "Timeout waiting for busy flag to go HIGH!\n");
            break;
        }
    } while (!busy_flag);

    return ret;
} /* lidarlite_v4led_take_range_gpio */

/*------------------------------------------------------------------------------
  Wait for Busy Flag using Trigger / Monitor Pins

  Blocking function to wait until the Lidar Lite's internal busy flag goes low

  Parameters
  ------------------------------------------------------------------------------
  monitor_pin: digital input pin connected to monitor output of LIDAR-Lite
  timeout_ms: timeout in milliseconds
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_wait_for_busy_gpio(gpio_num_t monitor_pin, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    uint8_t busy_flag;
    uint32_t timer_start = xTaskGetTickCount();

    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag_gpio(monitor_pin);
        vTaskDelay(pdMS_TO_TICKS(1));

        if ((xTaskGetTickCount() - timer_start) >= pdMS_TO_TICKS(timeout_ms))
        {
            ret = ESP_ERR_TIMEOUT;
            ESP_LOGW(TAG, "Timeout waiting for busy flag to go LOW!\n");
            break;
        }

    } while (busy_flag);

    return ret;
} /* lidarlite_v4led_wait_for_busy_gpio */

/*------------------------------------------------------------------------------
  Get Busy Flag using Trigger / Monitor Pins

  Check BUSY status via Monitor pin. Function will return 0x00 if not busy.

  Parameters
  ------------------------------------------------------------------------------
  monitor_pin: digital input pin connected to monitor output of LIDAR-Lite
------------------------------------------------------------------------------*/
uint8_t lidarlite_v4led_get_busy_flag_gpio(gpio_num_t monitor_pin)
{
    uint8_t busy_flag; /* busy_flag monitors when the device is done with a measurement */

    /* Check busy flag via monitor pin */
    if (gpio_get_level(monitor_pin))
        busy_flag = 1;
    else
        busy_flag = 0;

    return busy_flag;
} /* lidarlite_v4led_get_busy_flag_gpio */

/*------------------------------------------------------------------------------
  Read Distance

  Read and return the result of the most recent distance measurement.

  Parameters
  ------------------------------------------------------------------------------
  dev:      device handle
  distance: pointer to variable to store distance
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_read_distance(i2c_master_dev_handle_t dev, uint16_t *distance)
{
    esp_err_t ret = ESP_OK;

    if (distance == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
    }
    else
    {
        *distance = 0;
        /* Read two bytes from register 0x10 and 0x11 */
        ret =  lidarlite_v4led_read(dev, 0x10, (uint8_t *)distance, 2);
    }

    return ret;
} /* lidarlite_v4led_read_distance */


/*------------------------------------------------------------------------------
  Read Temperature

  Read and return the temperature of the device.

  Parameters
  ------------------------------------------------------------------------------
  dev:          device handle
  temperature:  pointer to variable to store temperature
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_read_temperature(i2c_master_dev_handle_t dev, int8_t *temperature)
{
    esp_err_t ret = ESP_OK;

    if (temperature == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
    }
    else
    {
        *temperature = 0;
        /* Read one byte from register 0xE0 */
        ret = lidarlite_v4led_read(dev, 0xE0, (uint8_t *)temperature, 1);
    }

    return ret;
} /* lidarlite_v4led_read_temperature */

esp_err_t lidarlite_v4led_set_high_accuracy_mode(i2c_master_dev_handle_t dev, uint8_t value)
{
    return lidarlite_v4led_write(dev, 0xEB, &value, 1);
}

esp_err_t lidarlite_v4led_read_high_accuracy_mode(i2c_master_dev_handle_t dev, uint8_t *value)
{
    esp_err_t ret = ESP_OK;

    if (value == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    ret = lidarlite_v4led_read(dev, 0xEB, value, 1);

exit:
    return ret;
}

esp_err_t lidarlite_v4led_set_power_mode(i2c_master_dev_handle_t dev, uint8_t mode)
{
    return lidarlite_v4led_write(dev, 0xE2, &mode, 1);
}

esp_err_t lidarlite_v4led_read_power_mode(i2c_master_dev_handle_t dev, uint8_t * mode)
{
    esp_err_t ret = ESP_OK;

    if (mode == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    ret = lidarlite_v4led_read(dev, 0xE2, mode, 1);

exit:
    return ret;
}

/*------------------------------------------------------------------------------
  Write

  Perform I2C write to device. The I2C peripheral in the LidarLite v4 LED
  will receive multiple bytes in one I2C transmission. The first byte is
  always the register address. The bytes that follow will be written into
  the specified register address first and then the internal address in the
  Lidar Lite will be auto-incremented for all following bytes.

  Parameters
  ------------------------------------------------------------------------------
  dev:        device handle
  reg_addr:   register address to write to
  data_bytes: pointer to array of bytes to write
  num_bytes:  number of bytes in 'data_bytes' array to write (max 31)
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_write(i2c_master_dev_handle_t dev, uint8_t reg_addr, const uint8_t *data_bytes, uint8_t num_bytes)
{
    esp_err_t ret = ESP_OK;

    uint8_t *write_buf = malloc(num_bytes + 1);
    if (write_buf == NULL)
    {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }

    write_buf[0] = reg_addr;
    if (num_bytes > 0 && data_bytes != NULL)
    {
        memcpy(&write_buf[1], data_bytes, num_bytes);
    }

    ret = i2c_master_transmit(dev, write_buf, num_bytes + 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    free(write_buf);

exit:
    return ret;
} /* lidarlite_v4led_write */

/*------------------------------------------------------------------------------
  Read

  Perform I2C read from device. The I2C peripheral in the LidarLite v4 LED
  will send multiple bytes in one I2C transmission. i2c_master_transmit_receive
  performs the register-address write followed by a repeated-start read in
  a single bus transaction. The bytes that follow will be read from the
  specified register address first and then the internal address pointer
  in the Lidar Lite will be auto-incremented for following bytes.

  Parameters
  ------------------------------------------------------------------------------
  dev:        device handle
  reg_addr:   register address to read from
  data_bytes: pointer to array to store the bytes read
  num_bytes:  number of bytes to read into 'data_bytes' array
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_read(i2c_master_dev_handle_t dev, uint8_t reg_addr, uint8_t *data_bytes, uint8_t num_bytes)
{
    esp_err_t ret = ESP_OK;

    if (num_bytes == 0 || data_bytes == NULL)
    {
        ret = ESP_ERR_INVALID_ARG;
    }
    else
    {
        ret = i2c_master_transmit_receive(dev, &reg_addr, 1, data_bytes, num_bytes, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    }

    return ret;
}  /* lidarlite_v4led_read */

/*------------------------------------------------------------------------------
  Correlation Record Read

  The correlation record used to calculate distance can be read from the
  device. It has a bipolar wave shape, transitioning from a positive going
  portion to a roughly symmetrical negative going pulse. The point where the
  signal crosses zero represents the effective delay for the reference and
  return signals.

  Process
  ------------------------------------------------------------------------------
  1.  Take a distance reading (there is no correlation record without at
      least one distance reading being taken)
  2.  For as many points as you want to read from the record (max is 192)
      read the two byte signed correlation data point from 0x52

  Parameters
  ------------------------------------------------------------------------------
  dev:               device handle
  correlation_array: pointer to memory location to store the correlation record
                     ** Two bytes for every correlation value must be
                        allocated by calling function
  num_readings:      max is 192 (pass 192 for the full record)
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_correlation_record_read(i2c_master_dev_handle_t dev, int16_t *correlation_array, uint8_t num_readings)
{
    esp_err_t ret = ESP_OK;
    uint8_t  i;
    int16_t  correlation_value;
    uint8_t *data_bytes = (uint8_t *) &correlation_value;

    for (i = 0; i < num_readings; i++)
    {
        ret = lidarlite_v4led_read(dev, 0x52, data_bytes, 2);
        if (ret != ESP_OK)
        {
            goto exit;
        }
        correlation_array[i] = correlation_value;
    }

exit:
    return ret;
} /* lidarlite_v4led_correlation_record_read */