#pragma once

#include "esp_err.h"

esp_err_t cm_sntp_init(void);
esp_err_t cm_sntp_sync_time(void);