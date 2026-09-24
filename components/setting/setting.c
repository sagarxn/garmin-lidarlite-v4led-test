#include "setting.h"

#include "esp_err.h"
#include "esp_log.h"
#include "cbor.h"

#define SETTINGS_FILE_PATH         (LITTLEFS_MOUNT_POINT "/settings.cbor")
#define CBOR_BUFFER_SIZE            1024


static const char *TAG = "[setting]";

static const setting_sensor_t DEFAULT_SENSOR_SETTING = {
    .config_mode = 0,
    .high_accuracy_mode = 0x14
};

static const setting_wifi_sta_t DEFAULT_WIFI_STA_SETTING = {
    .ssid = "ASUS_Intern",
    .password = "iknowyouknow",
    .dhcp_enabled = true,
    .static_ip = "0.0.0.0"
};

static const setting_wifi_ap_t DEFAULT_WIFI_AP_SETTING = {
    .ssid = "LidarLite_AP",
    .password = "1234567890",
    .gateway = "192.168.10.1",
    .netmask = "255.255.255.0",
    .dhcp_enabled = true
};

static const setting_scheduler_t DEFAULT_SCHEDULER_SETTING = {
    .interval_sec = 60,
    .start_year = 2024,
    .start_month = 1,
    .start_day = 1,
    .start_hour = 0,
    .start_min = 0
};

static setting_t gs_setting = {
    .sensor = DEFAULT_SENSOR_SETTING,
    .wifi_ap = DEFAULT_WIFI_AP_SETTING,
    .wifi_sta = DEFAULT_WIFI_STA_SETTING
};

esp_err_t setting_init(void)
{
    esp_err_t e_err = setting_read(&gs_setting);
    if (e_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read settings, using default values.");
    }

    return e_err;
}

esp_err_t setting_read(setting_t *settings)
{
    esp_err_t e_err = ESP_OK;

    CborParser parser;
    CborValue it;

    if (NULL == settings)
    {
        e_err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    FILE *f = fopen(SETTINGS_FILE_PATH, "rb");
    if (NULL == f)
    {
        ESP_LOGW(TAG, "Settings file not found, using default settings.");
        e_err = ESP_ERR_NOT_FOUND;
        goto exit;
    }

    uint8_t buffer[CBOR_BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    if (0 == bytes_read)
    {
        ESP_LOGE(TAG, "Settings file is empty");
        e_err = ESP_FAIL;
        goto exit;
    }

    cbor_err_t e_cbor_err = cbor_parser_init(buffer, bytes_read, 0, &parser, &it);
    if ((e_cbor_err != CborNoError) || !cbor_value_is_map(&it))
    {
        ESP_LOGE(TAG, "Invalid CBOR stream or top level element is not a map");
        e_err = ESP_FAIL;
        goto exit;
    }

    e_cbor_err = cbor_value_enter_container(&it, &map_it);
    if (CborNoError != e_cbor_err)
    {
        ESP_LOGE(TAG, "Failed to enter CBOR map container");
        e_err = ESP_FAIL;
        goto exit;
    }

    while (!cbor_value_at_end(&map_it))
    {
        if (!cbor_value_is_text_string(&map_it))
        {
            ESP_LOGE(TAG, "Expected text string in CBOR map");
            e_err = ESP_FAIL;
            goto exit;
        }

        char key[32];
        size_t key_len = sizeof(key) - 1;

        cbor_value_copy_text_string(&map_it, key, &key_len, &map_it);
        key[key_len] = '\0';

        if ((0 == strcmp(key, SETTING_SENSOR_CONFIG_MODE)) && cbor_value_is_integer(&map_it))
        {
            uint64_t val;
            cbor_value_get_uint32(&map_it, &val);
            settings->sensor.config_mode = (uint32_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SENSOR_HIGH_ACCURACY_MODE)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->sensor.high_accuracy_mode = (uint8_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_STA_SSID)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_sta.ssid) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_sta.ssid, &len, &map_it);
            settings->wifi_sta.ssid[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_STA_PASSWORD)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_sta.password) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_sta.password, &len, &map_it);
            settings->wifi_sta.password[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_STA_DHCP_ENABLED)) && cbor_value_is_boolean(&map_it)) {
            bool val;
            cbor_value_get_boolean(&map_it, &val);
            settings->wifi_sta.dhcp_enabled = val;
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_STA_STATIC_IP)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_sta.static_ip) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_sta.static_ip, &len, &map_it);
            settings->wifi_sta.static_ip[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_AP_SSID)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_ap.ssid) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_ap.ssid, &len, &map_it);
            settings->wifi_ap.ssid[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_AP_PASSWORD)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_ap.password) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_ap.password, &len, &map_it);
            settings->wifi_ap.password[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_AP_GATEWAY)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_ap.gateway) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_ap.gateway, &len, &map_it);
            settings->wifi_ap.gateway[len] = '\0';
        } 
        else if ((0 == strcmp(key, SETTING_WIFI_AP_NETMASK)) && cbor_value_is_text_string(&map_it)) {
            size_t len = sizeof(settings->wifi_ap.netmask) - 1;
            cbor_value_copy_text_string(&map_it, settings->wifi_ap.netmask, &len, &map_it);
            settings->wifi_ap.netmask[len] = '\0';
        }
        else if ((0 == strcmp(key, SETTING_WIFI_AP_DHCP_ENABLED)) && cbor_value_is_boolean(&map_it)) {
            bool val;
            cbor_value_get_boolean(&map_it, &val);
            settings->wifi_ap.dhcp_enabled = val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_INTERVAL_SEC)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.interval_sec = (uint32_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_START_YEAR)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.start_year = (uint16_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_START_MONTH)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.start_month = (uint8_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_START_DAY)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.start_day = (uint8_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_START_HOUR)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.start_hour = (uint8_t)val;
        } 
        else if ((0 == strcmp(key, SETTING_SCHEDULER_START_MIN)) && cbor_value_is_integer(&map_it)) {
            uint64_t val;
            cbor_value_get_uint64(&map_it, &val);
            settings->scheduler.start_min = (uint8_t)val;
        }

        cbor_value_advance(&map_it);
    }

exit:
    return e_err;
}

esp_err_t setting_save(const setting_t *settings)
{
    esp_err_t e_err = ESP_OK;

    if (NULL == settings)
    {
        e_err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t buffer[CBOR_BUFFER_SIZE];
    CborEncoder encoder, map_encoder;

    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);

    cbor_err_t e_cbor_err = cbor_encoder_create_map(&encoder, &map_encoder, 17);
    if (CborNoError != e_cbor_err)
    {
        ESP_LOGE(TAG, "Failed to create CBOR map: %d", e_cbor_err);
        e_err = ESP_FAIL;
        goto exit;
    }

    for (int i = 0; i < SETTING_MAX_KEYS; ++i)
    {
        const setting_meta_t *meta = &SETTING_REGISTRY[i];
        const void *value_ptr = (const uint8_t *)settings + meta->offset;

        cbor_encode_text_stringz(&map_encoder, meta->name);

        switch (meta->type)
        {
            case SETTING_TYPE_UINT8:
                cbor_encode_uint(&map_encoder, *(const uint8_t *)value_ptr);
            break;
            
            case SETTING_TYPE_UINT16:
                cbor_encode_uint(&map_encoder, *(const uint16_t *)value_ptr);
            break;
            
            case SETTING_TYPE_UINT32:
                cbor_encode_uint(&map_encoder, *(const uint32_t *)value_ptr);
            break;
            
            case SETTING_TYPE_BOOL:
                cbor_encode_boolean(&map_encoder, *(const bool *)value_ptr);
            break;

            case SETTING_TYPE_FLOAT32:
                cbor_encode_float(&map_encoder, *(const float *)value_ptr);
            break;

            case SETTING_TYPE_STRING:
                cbor_encode_text_stringz(&map_encoder, (const char *)value_ptr);
            break;

            default:
                ESP_LOGE(TAG, "Unsupported setting type for key %s", meta->name);
                e_err = ESP_ERR_NOT_SUPPORTED;
                goto exit;
            break;
        }
    }

    cbor_encode_text_stringz(&map_encoder, SETTING_SENSOR_CONFIG_MODE);
    cbor_encode_uint(&map_encoder, settings->sensor.config_mode);

    cbor_encode_text_stringz(&map_encoder, SETTING_SENSOR_HIGH_ACCURACY_MODE);
    cbor_encode_uint(&map_encoder, settings->sensor.high_accuracy_mode);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_STA_SSID);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_sta.ssid);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_STA_PASSWORD);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_sta.password);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_STA_DHCP_ENABLED);
    cbor_encode_boolean(&map_encoder, settings->wifi_sta.dhcp_enabled);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_STA_STATIC_IP);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_sta.static_ip);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_AP_SSID);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_ap.ssid);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_AP_PASSWORD);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_ap.password);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_AP_GATEWAY);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_ap.gateway);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_AP_NETMASK);
    cbor_encode_text_stringz(&map_encoder, settings->wifi_ap.netmask);

    cbor_encode_text_stringz(&map_encoder, SETTING_WIFI_AP_DHCP_ENABLED);
    cbor_encode_boolean(&map_encoder, settings->wifi_ap.dhcp_enabled);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_INTERVAL_SEC);
    cbor_encode_uint(&map_encoder, settings->scheduler.interval_sec);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_START_YEAR);
    cbor_encode_uint(&map_encoder, settings->scheduler.start_year);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_START_MONTH);
    cbor_encode_uint(&map_encoder, settings->scheduler.start_month);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_START_DAY);
    cbor_encode_uint(&map_encoder, settings->scheduler.start_day);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_START_HOUR);
    cbor_encode_uint(&map_encoder, settings->scheduler.start_hour);

    cbor_encode_text_stringz(&map_encoder, SETTING_SCHEDULER_START_MIN);
    cbor_encode_uint(&map_encoder, settings->scheduler.start_min);

    e_cbor_err = cbor_encoder_close_container(&encoder, &map_encoder);
    if (CborNoError != e_cbor_err)
    {
        ESP_LOGE(TAG, "Failed to close CBOR map: %d", e_cbor_err);
        e_err = ESP_FAIL;
        goto exit;
    }

    size_t encoded_len = cbor_encoder_get_buffer_size(&encoder, buffer);

    FILE *f = fopen(SETTINGS_FILE_PATH, "wb");
    if (NULL == f)
    {
        ESP_LOGE(TAG, "Failed to open settings file for writing");
        e_err = ESP_FAIL;
        goto exit;
    }

    size_t bytes_written = fwrite(buffer, 1, encoded_len, f);
    fclose(f);

    if (bytes_written != encoded_len) {
        ESP_LOGE(TAG, "Failed to write complete CBOR buffer");
        e_err = ESP_FAIL;
        goto exit;
    }

    ESP_LOGI(TAG, "Successfully saved %zu bytes of settings", encoded_len);

exit:
    return e_err;
}

setting_t setting_get(void)
{
    return gs_setting;
}

void setting_set(const setting_t *ps_settings)
{
    if (NULL != ps_settings)
    {
        gs_setting = *ps_settings;
    }
}

setting_t *setting_get_ptr(void)
{
    return &gs_setting;
}