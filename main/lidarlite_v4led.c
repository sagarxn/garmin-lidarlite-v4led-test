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

#define I2C_TIMEOUT_MS    1000
#define I2C_DEV_SCL_HZ    400000
#define I2C_MAX_WRITE_LEN 16   /* reg addr byte + up to 15 data bytes */

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
  new_address: I2C address to rebind this handle to
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_update_address(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t new_address)
{
    esp_err_t ret;
    i2c_master_dev_handle_t new_dev_handle;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = new_address,
        .scl_speed_hz    = I2C_DEV_SCL_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &new_dev_handle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = i2c_master_bus_rm_device(*dev_handle);
    if (ret != ESP_OK)
    {
        i2c_master_bus_rm_device(new_dev_handle);
        return ret;
    }

    *dev_handle = new_dev_handle;

    return ESP_OK;
} /* lidarlite_v4led_update_address */

/*------------------------------------------------------------------------------
  Configure

  Selects one of several preset configurations.

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
  configuration:
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
void lidarlite_v4led_configure(i2c_master_dev_handle_t dev, uint8_t configuration)
{
    uint8_t sig_count_max;
    uint8_t acq_config_reg;

    switch (configuration)
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

    lidarlite_v4led_write(dev, 0x05, &sig_count_max , 1);
    lidarlite_v4led_write(dev, 0xE5, &acq_config_reg, 1);
} /* lidarlite_v4led_configure */

/*------------------------------------------------------------------------------
  Set I2C Address

  Set Alternate I2C Device Address. See Operation Manual for additional info.
  Internally rebinds *dev_handle to newAddress partway through (via
  lidarlite_v4led_update_address) once the device has actually switched.

  Parameters
  ------------------------------------------------------------------------------
  bus_handle:     the I2C master bus the device lives on
  dev_handle:     pointer to device handle (must currently be bound to the
                   device's *current* address)
  newAddress:     desired secondary I2C device address
  disableDefault: a non-zero value here means the default 0x62 I2C device
    address will be disabled.
------------------------------------------------------------------------------*/
void lidarlite_v4led_set_i2c_addr(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t newAddress, uint8_t disableDefault)
{
    uint8_t data_bytes[5];

    /* Enable flash storage */
    data_bytes[0] = 0x11;
    lidarlite_v4led_write(*dev_handle, 0xEA, data_bytes, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read 4-byte device serial number */
    lidarlite_v4led_read(*dev_handle, 0x16, data_bytes, 4);

    /* Append the desired I2C address to the end of the serial number byte array */
    data_bytes[4] = newAddress;

    /* Write the serial number and new address in one 5-byte transaction */
    lidarlite_v4led_write(*dev_handle, 0x16, data_bytes, 5);

    /* Wait for the I2C peripheral to be restarted with new device address */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Rebind dev_handle to the new address for the remaining steps */
    lidarlite_v4led_update_address(bus_handle, dev_handle, newAddress);

    /* If desired, disable default I2C device address (using the new address) */
    if (disableDefault)
    {
        data_bytes[0] = 0x01; /* set bit to disable default address */
        lidarlite_v4led_write(*dev_handle, 0x1b, data_bytes, 1);

        /* Wait for the I2C peripheral to be restarted with new device address */
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* Disable flash storage */
    data_bytes[0] = 0;
    lidarlite_v4led_write(*dev_handle, 0xEA, data_bytes, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
} /* lidarlite_v4led_set_i2c_addr */

/*------------------------------------------------------------------------------
  Take Range

  Initiate a distance measurement by writing to register 0x00.

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
------------------------------------------------------------------------------*/
void lidarlite_v4led_take_range(i2c_master_dev_handle_t dev)
{
    uint8_t data_byte = 0x04;

    lidarlite_v4led_write(dev, 0x00, &data_byte, 1);
} /* lidarlite_v4led_take_range */

/*------------------------------------------------------------------------------
  Wait for Busy Flag

  Blocking function to wait until the Lidar Lite's internal busy flag goes low

  Parameters
  ------------------------------------------------------------------------------
  dev: device handle
------------------------------------------------------------------------------*/
void lidarlite_v4led_wait_for_busy(i2c_master_dev_handle_t dev)
{
    uint8_t busy_flag;

    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag(dev);
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (busy_flag);

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
  triggerPin: digital output pin connected to trigger input of LIDAR-Lite
  monitorPin: digital input pin connected to monitor output of LIDAR-Lite
------------------------------------------------------------------------------*/
void lidarlite_v4led_take_range_gpio(gpio_num_t trigger_pin, gpio_num_t monitor_pin)
{
    uint8_t busy_flag;

    if (gpio_get_level(trigger_pin))
    {
        gpio_set_level(trigger_pin, 0);
    }
    else
    {
        gpio_set_level(trigger_pin, 1);
    }

    uint32_t timeout_counter = 0;

    // When LLv4 receives trigger command it will drive monitor pin low.
    // Wait for LLv4 to acknowledge receipt of command before moving on.
    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag_gpio(monitor_pin);
        vTaskDelay(pdMS_TO_TICKS(1));
        
        timeout_counter++;
        if (timeout_counter > 1000)
        {
            ESP_LOGW(TAG, "Timeout waiting for busy flag to go HIGH!\n");
            break;
        }
    } while (!busy_flag);
} /* lidarlite_v4led_take_range_gpio */

/*------------------------------------------------------------------------------
  Wait for Busy Flag using Trigger / Monitor Pins

  Blocking function to wait until the Lidar Lite's internal busy flag goes low

  Parameters
  ------------------------------------------------------------------------------
  monitorPin: digital input pin connected to monitor output of LIDAR-Lite
------------------------------------------------------------------------------*/
void lidarlite_v4led_wait_for_busy_gpio(gpio_num_t monitor_pin)
{
    uint8_t busy_flag;
    uint32_t timeout_counter = 0;

    do
    {
        busy_flag = lidarlite_v4led_get_busy_flag_gpio(monitor_pin);
        vTaskDelay(pdMS_TO_TICKS(1));

        timeout_counter++;
        if (timeout_counter > 1000)
        {
            ESP_LOGW(TAG, "Timeout waiting for busy flag to go LOW!\n");
            break;
        }

    } while (busy_flag);

} /* lidarlite_v4led_wait_for_busy_gpio */

/*------------------------------------------------------------------------------
  Get Busy Flag using Trigger / Monitor Pins

  Check BUSY status via Monitor pin. Function will return 0x00 if not busy.

  Parameters
  ------------------------------------------------------------------------------
  monitorPin: digital input pin connected to monitor output of LIDAR-Lite
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
  dev: device handle
------------------------------------------------------------------------------*/
uint16_t lidarlite_v4led_read_distance(i2c_master_dev_handle_t dev)
{
    uint16_t  distance = 0;
    uint8_t  *data_bytes = (uint8_t *) &distance;

    /* Read two bytes from registers 0x10 and 0x11 */
    lidarlite_v4led_read(dev, 0x10, data_bytes, 2);

    return distance;
} /* lidarlite_v4led_read_distance */

/*------------------------------------------------------------------------------
  Write

  Perform I2C write to device. The I2C peripheral in the LidarLite v4 LED
  will receive multiple bytes in one I2C transmission. The first byte is
  always the register address. The bytes that follow will be written into
  the specified register address first and then the internal address in the
  Lidar Lite will be auto-incremented for all following bytes.

  Parameters
  ------------------------------------------------------------------------------
  dev:       device handle
  regAddr:   register address to write to
  dataBytes: pointer to array of bytes to write
  numBytes:  number of bytes in 'dataBytes' array to write (max 31)
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_write(i2c_master_dev_handle_t dev, uint8_t regAddr, const uint8_t *dataBytes, uint8_t numBytes)
{
    uint8_t write_buf[I2C_MAX_WRITE_LEN];

    if ((size_t)(numBytes + 1) > sizeof(write_buf))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    write_buf[0] = regAddr;
    if (numBytes > 0)
    {
        memcpy(&write_buf[1], dataBytes, numBytes);
    }

    /* A failing return code here means the device is not responding. */
    return i2c_master_transmit(dev, write_buf, numBytes + 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
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
  dev:       device handle
  regAddr:   register address to read from
  dataBytes: pointer to array to store the bytes read
  numBytes:  number of bytes to read into 'dataBytes' array
------------------------------------------------------------------------------*/
esp_err_t lidarlite_v4led_read(i2c_master_dev_handle_t dev, uint8_t regAddr, uint8_t *dataBytes, uint8_t numBytes)
{
    if (numBytes == 0)
    {
        return ESP_OK;
    }

    return i2c_master_transmit_receive(dev, &regAddr, 1, dataBytes, numBytes, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
} /* lidarlite_v4led_read */

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
  correlationArray: pointer to memory location to store the correlation record
                     ** Two bytes for every correlation value must be
                        allocated by calling function
  numberOfReadings:  max is 192 (pass 192 for the full record)
------------------------------------------------------------------------------*/
void lidarlite_v4led_correlation_record_read(i2c_master_dev_handle_t dev, int16_t *correlationArray, uint8_t numberOfReadings)
{
    uint8_t  i;
    int16_t  correlation_value;
    uint8_t *data_bytes = (uint8_t *) &correlation_value;

    for (i = 0; i < numberOfReadings; i++)
    {
        lidarlite_v4led_read(dev, 0x52, data_bytes, 2);
        correlationArray[i] = correlation_value;
    }
} /* lidarlite_v4led_correlation_record_read */