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

i2c_master_dev_handle_t board_get_lidar_i2c_dev_handle (void);

