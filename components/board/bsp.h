#pragma once

/* LED Pins */
#define BOARD_HEART_LED_PIN         GPIO_NUM_25

/* I2C Pins */
#define BOARD_LIDAR_I2C_PORT        I2C_NUM_0
#define BOARD_LIDAR_I2C_SDA_PIN     GPIO_NUM_16
#define BOARD_LIDAR_I2C_SCL_PIN     GPIO_NUM_17

/* I2C Configuration */
#define BOARD_LIDAR_I2C_ADDR                0x62
#define BOARD_LIDAR_I2C_SCL_SPEED_HZ        400000UL
