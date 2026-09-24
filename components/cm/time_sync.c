#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "lwip/inet.h"

#include "time_sync.h"

#define SNTP_SERVER_1       "pool.ntp.org"
#define SNTP_SERVER_2       "time.google.com"
#define SNTP_SERVER_COUNT    2
#define SNTP_MAX_RETRY      20

static const char *TAG = "[time_sync]";

static void _obtain_time(void);
static void _print_servers(void);

esp_err_t time_sync_init(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2020 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Getting time over NTP.");
        _obtain_time();

        // update 'now' variable with current time
        time(&now);
    }

    char strftime_buf[64];

    // Set timezone to Kathmandu Standard Time
    setenv("TZ", "NPT-5:45", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("--------------------------------------------------------------------------------\n");
    ESP_LOGI(TAG, "The current date/time in Kathmandu is: %s", strftime_buf);
    printf("--------------------------------------------------------------------------------\n");

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

static void _obtain_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t s_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(SNTP_SERVER_COUNT,
                    ESP_SNTP_SERVER_LIST(SNTP_SERVER_1, SNTP_SERVER_2));
    s_config.start = false;                       // start SNTP service explicitly (after connecting)
    s_config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    s_config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    s_config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // configure the event on which we renew servers
    s_config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    esp_netif_sntp_init(&s_config);

    _print_servers();

    ESP_LOGI(TAG, "Starting SNTP");
    esp_netif_sntp_start();

    int retry = 0;
    while ((ESP_ERR_TIMEOUT == esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS)) && (++retry < SNTP_MAX_RETRY))
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, SNTP_MAX_RETRY);
    }
}

static void _print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_SERVER_COUNT; ++i)
    {
        if (esp_sntp_getservername(i))
        {
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        }
        else
        {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}