#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef enum blinky_state
{
    BLINKY_STATE_NONE = -1,
    BLINKY_STATE_NORMAL,
    BLINKY_STATE_FAST,
    BLINKY_STATE_FLASH,
    BLINKY_STATE_FADE,
} blinky_state_t;

esp_err_t board_init (void);

// void board_set_normal_blinky (void);

// void board_set_fast_blinky (void);

// void board_set_flash_blinky (void);

// void board_set_fade_blinky (void);

i2c_master_bus_handle_t board_get_lidar_i2c_bus_handle (void);

i2c_master_dev_handle_t board_get_lidar_v4_i2c_dev_handle (void);

i2c_master_dev_handle_t board_get_lidar_v3_i2c_dev_handle (void);

void board_set_lidar_i2c_bus_handle(i2c_master_bus_handle_t bus_handle);

void board_set_lidar_v4_i2c_dev_handle(i2c_master_dev_handle_t dev_handle);

void board_set_lidar_v3_i2c_dev_handle(i2c_master_dev_handle_t dev_handle);
