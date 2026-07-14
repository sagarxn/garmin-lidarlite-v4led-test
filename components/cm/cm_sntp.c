#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include "cm_sntp.h"

#define SNTP_SERVER_1 "pool.ntp.org"
#define SNTP_SERVER_2 "time.google.com"
#define SNTP_MAX_RETRY 30

static const char *TAG = "[cm_sntp]";

static void time_sync_notification_cb(struct timeval *tv);
static void obtain_time(void);

esp_err_t cm_sntp_init(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2020 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Getting time over NTP.");
        obtain_time();

        // update 'now' variable with current time
        time(&now);
    }

    char strftime_buf[64];

    // Set timezone to Kathmandu Standard Time
    setenv("TZ", "NPT-5:45", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Kathmandu is: %s", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH)
    {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS)
        {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                     (long)outdelta.tv_sec,
                     outdelta.tv_usec / 1000,
                     outdelta.tv_usec % 1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    return ESP_OK;
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    // sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void obtain_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP via Netif wrapper");
    
    // Configure SNTP with renewal logic for the station interface
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER_1);
    config.sync_cb = time_sync_notification_cb;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    
    esp_netif_sntp_init(&config);

    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
}