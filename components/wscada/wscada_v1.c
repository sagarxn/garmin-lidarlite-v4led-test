#include "wscada.h"

#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "mbedtls/aes.h"

#define TSS_SERVER      "alpha.wscada.net"
#define TSS_PORT        80
#define TSS_PATH        "/devices/p2/postdata.php"
#define HTTP_TIMEOUT_MS 50000

typedef struct __attribute__((packed))
{
    uint32_t device_code;
    uint32_t security_code;
    uint32_t command;
    uint16_t table_no;
    uint8_t log_count;
    uint32_t read_pointer;
    uint32_t write_pointer;
} wscada_v1_data_header_t;

typedef struct __attribute__((packed))
{
    uint32_t category;
    float value;
    uint32_t date_time;
    uint32_t interval;
} wscada_v1_log_t;

typedef struct __attribute__((packed))
{
    uint32_t security_code;
    uint32_t command;
    uint32_t response;
    uint16_t table;
} wscada_v1_response_t;

typedef struct
{
    uint8_t buffer[64];
    size_t length;
} response_ctx_t;

static const char *TAG = "[wscada_v1]";

static const uint32_t DEVICE_ID             = 138;
static const uint32_t SECURITY_CODE         = 4253717;
static const char *AES_KEY_STR              = "9752325135323618";
static const uint32_t PARAM_CODE_DISTANCE   = 2;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static void _aes128_encrypt(const uint8_t *input, size_t length, uint8_t *output, const uint8_t *key);
static void _aes128_decrypt(const uint8_t *input, size_t length, uint8_t *output, const uint8_t *key);

esp_err_t wscada_v1_post_lidar(uint32_t fat32_time, uint32_t interval, float distance)
{
    uint8_t log_count = 1; 

    size_t raw_payload_size = sizeof(wscada_v1_data_header_t) + (log_count * sizeof(wscada_v1_log_t));
    
    // The device code prefix remains plaintext; encrypt starting from security code offset
    size_t encrypted_part_size = raw_payload_size - sizeof(uint32_t);
    size_t encrypted_size = ((encrypted_part_size + 15) / 16) * 16;
    size_t total_buffer_size = sizeof(uint32_t) + encrypted_size;

    uint8_t *unencrypted_buf = (uint8_t *)calloc(1, raw_payload_size + 16);
    uint8_t *final_packet_buf = (uint8_t *)calloc(1, total_buffer_size + 16);

    if (!unencrypted_buf || !final_packet_buf)
    {
        ESP_LOGE(TAG, "Buffer allocation failed");
        if (unencrypted_buf) free(unencrypted_buf);
        if (final_packet_buf) free(final_packet_buf);
        return ESP_ERR_NO_MEM;
    }

    // Populate data layout structure sequentially
    wscada_v1_data_header_t *header = (wscada_v1_data_header_t *)unencrypted_buf;
    header->device_code = DEVICE_ID;
    header->security_code = SECURITY_CODE;
    header->command = 0x00000095;
    header->table_no = 0x0003;
    header->log_count = log_count;
    header->read_pointer = 0;
    header->write_pointer = 0;

    wscada_v1_log_t *logs = (wscada_v1_log_t *)(unencrypted_buf + sizeof(wscada_v1_data_header_t));
    logs[0].category = PARAM_CODE_DISTANCE;
    logs[0].value = distance;
    logs[0].date_time = fat32_time;
    logs[0].interval = interval;

    // Assign plaintext transmission prefix
    memcpy(final_packet_buf, &DEVICE_ID, sizeof(uint32_t));

    uint8_t raw_key[16] = {0};
    memcpy(raw_key, AES_KEY_STR, strlen(AES_KEY_STR) > 16 ? 16 : strlen(AES_KEY_STR));

    // Encrypt exactly from security_code field offset forward
    _aes128_encrypt(unencrypted_buf + sizeof(uint32_t), encrypted_size, final_packet_buf + sizeof(uint32_t), raw_key);

    response_ctx_t response_data = {0};

    esp_http_client_config_t config = {
        .host = TSS_SERVER,
        .port = TSS_PORT,
        .path = TSS_PATH,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = &response_data,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(unencrypted_buf);
        free(final_packet_buf);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    
    // Set field parameters first to prevent runtime header overwrites
    esp_http_client_set_post_field(client, (const char *)final_packet_buf, total_buffer_size);
    esp_http_client_set_header(client, "User-Agent", "DL");
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    ESP_LOGI(TAG, "Posting data payload to WSCADA Server...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Success, Status Code = %d", status_code);

        if (status_code == 200 && response_data.length >= 16)
        {
            uint8_t decrypted_response[16] = {0};
            _aes128_decrypt(response_data.buffer, 16, decrypted_response, raw_key);

            wscada_v1_response_t *res = (wscada_v1_response_t *)decrypted_response;

            printf("--------------------------------------------------------------------------------\n");
            ESP_LOGI(TAG, "Decrypted Response Metrics:");
            // ESP_LOGI(TAG, " -> Security Code: %u (Expected: %u)", res->security_code, SECURITY_CODE);
            ESP_LOGI(TAG, " -> Command: 0x%08X (Expected: 0x80000095)", res->command);
            ESP_LOGI(TAG, " -> Response Code: 0x%08X (Expected: 0xFFFFFFFF)", res->response);
            ESP_LOGI(TAG, " -> Table: 0x%04X (Expected: 0x0003)", res->table);
            printf("--------------------------------------------------------------------------------\n");

            if (res->security_code != SECURITY_CODE)
            {
                ESP_LOGE(TAG, "Validation Failed: Security Code mismatch!");
                err = ESP_ERR_INVALID_STATE;
            }
            else if (res->command != 0x80000095)
            {
                ESP_LOGE(TAG, "Validation Failed: Response Protocol High mismatch!");
                err = ESP_ERR_INVALID_RESPONSE;
            }
            else if (res->table != 0x0003)
            {
                ESP_LOGE(TAG, "Validation Failed: Response Protocol Low mismatch!");
                err = ESP_ERR_INVALID_RESPONSE;
            }
            else if (res->response != 0xFFFFFFFF)
            {
                ESP_LOGE(TAG, "Post Unsuccessful: Response code error (0xFFFFFFFF expected)!");
                err = ESP_FAIL;
            }
            else
            {
                ESP_LOGI(TAG, "WSCADA Transaction Verified successfully!");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Response size insufficient or status code error. Received length: %d", response_data.length);
            err = ESP_ERR_INVALID_SIZE;
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST Request Failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(unencrypted_buf);
    free(final_packet_buf);

    return err;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client) && evt->user_data)
            {
                response_ctx_t *ctx = (response_ctx_t *)evt->user_data;
                if (ctx->length + evt->data_len < sizeof(ctx->buffer))
                {
                    memcpy(ctx->buffer + ctx->length, evt->data, evt->data_len);
                    ctx->length += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            esp_http_client_set_redirection(evt->client);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void _aes128_encrypt(const uint8_t *input, size_t length, uint8_t *output, const uint8_t *key)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);

    for (size_t i = 0; i < length; i += 16)
    {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input + i, output + i);
    }
    mbedtls_aes_free(&aes);
}

static void _aes128_decrypt(const uint8_t *input, size_t length, uint8_t *output, const uint8_t *key)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 128);

    for (size_t i = 0; i < length; i += 16)
    {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input + i, output + i);
    }
    mbedtls_aes_free(&aes);
}
