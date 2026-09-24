#include "wifi.h"

#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_sntp.h"
#include "ping/ping_sock.h"
#include "lwip/dns.h"

#include "setting.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define WIFI_AP_CHANNEL     1
#define WIFI_MAX_STA_CONN   4
#define WIFI_MAX_RETRY      5
#define DHCPS_OFFER_DNS     0x02
#define PING_TARGET         "pool.ntp.org"

static const char *TAG = "[wifi]";

static EventGroupHandle_t _gp_s_wifi_event_group;

static esp_netif_t *_init_softap(void);
static esp_netif_t *_init_sta(void);
static void _softap_set_dns_addr(esp_netif_t *ps_esp_netif_ap,esp_netif_t *ps_esp_netif_sta);
static void _event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data);
static void _on_ping_success(esp_ping_handle_t hdl, void *args);
static void _on_ping_timeout(esp_ping_handle_t hdl, void *args);
static void _on_ping_end(esp_ping_handle_t hdl, void *args);

esp_err_t wifi_init(void)
{
    esp_err_t status = ESP_OK;

    status = esp_netif_init();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize esp_netif: %s", esp_err_to_name(status));
        goto exit;
    }

    status = esp_event_loop_create_default();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(status));
        goto exit;
    }

    /* Initialize event group */
    _gp_s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    status = esp_event_handler_instance_register(WIFI_EVENT,
                                                 ESP_EVENT_ANY_ID,
                                                 &_event_handler,
                                                 NULL,
                                                 NULL);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(status));
        goto exit;
    }

    status = esp_event_handler_instance_register(IP_EVENT,
                                                 IP_EVENT_STA_GOT_IP,
                                                 &_event_handler,
                                                 NULL,
                                                 NULL);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(status));
        goto exit;
    }

    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    status = esp_wifi_init(&cfg);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(status));
        goto exit;
    }

    status = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(status));
        goto exit;
    }

    /* Initialize AP */
    esp_netif_t *ps_esp_netif_ap = _init_softap();
    if (!ps_esp_netif_ap)
    {
        ESP_LOGE(TAG, "Failed to initialize soft AP netif");
        status = ESP_FAIL;
        goto exit;
    }

    /* Initialize STA */
    esp_netif_t *ps_esp_netif_sta = _init_sta();
    if (!ps_esp_netif_sta)
    {
        ESP_LOGE(TAG, "Failed to initialize station netif");
        status = ESP_FAIL;
        goto exit;
    }

    /* Start WiFi */
    status = esp_wifi_start();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(status));
        status = ESP_FAIL;
        goto exit;
    }

    /*
     * Wait until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(_gp_s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    setting_wifi_t *ps_wifi_sta_settings = setting_get_wifi_sta();

    /* xEventGroupWaitBits() returns the bits before the call returned,
     * hence we can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 ps_wifi_sta_settings->ssid);
        _softap_set_dns_addr(ps_esp_netif_ap, ps_esp_netif_sta);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ps_wifi_sta_settings->ssid, ps_wifi_sta_settings->password);
        status = ESP_FAIL;
        goto exit;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = ESP_FAIL;
        goto exit;
    }

    /* Set sta as the default interface */
    esp_netif_set_default_netif(ps_esp_netif_sta);

    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(ps_esp_netif_ap) != ESP_OK)
    {
        ESP_LOGE(TAG, "NAPT not enabled on the netif: %p", ps_esp_netif_ap);
        status = ESP_FAIL;
    }

    // wifi_ping();

exit:
    return status;
}

esp_err_t wifi_ping(void)
{
    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_RAW;

int err = getaddrinfo(PING_TARGET, NULL, &hint, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE("[PING]", "DNS lookup failed for " PING_TARGET ", err=%d", err);
        return ESP_FAIL;
    }

    // Safe to access now that we know res is not NULL
    struct sockaddr_in *saddr = (struct sockaddr_in *)res->ai_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &saddr->sin_addr);
    target_addr.type = IPADDR_TYPE_V4;
    
    freeaddrinfo(res);

    ESP_LOGI("[PING]", "Pinging " PING_TARGET " [%s]...", ipaddr_ntoa(&target_addr));

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;          
    ping_config.count = 5;    

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = _on_ping_success,
        .on_ping_timeout = _on_ping_timeout,
        .on_ping_end = _on_ping_end,
        .cb_args = NULL
    };

    esp_ping_handle_t ping;
    if (esp_ping_new_session(&ping_config, &cbs, &ping) == ESP_OK) {
        esp_ping_start(ping);
        return ESP_OK;
    }

    ESP_LOGE("[PING]", "Failed to create ping session");
    return ESP_FAIL;
}

/* Initialize soft AP */
static esp_netif_t *_init_softap(void)
{
    esp_err_t status = ESP_OK;

    esp_netif_t *ps_esp_netif_ap = esp_netif_create_default_wifi_ap();
    
    /* Change AP subnet */
    esp_netif_dhcps_stop(ps_esp_netif_ap);
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton("192.168.10.1");
    ip_info.gw.addr = esp_ip4addr_aton("192.168.10.1");
    ip_info.netmask.addr = esp_ip4addr_aton("255.255.255.0");

    esp_netif_set_ip_info(ps_esp_netif_ap, &ip_info);

    setting_wifi_t *ps_wifi_ap_settings = setting_get_wifi_ap();

    wifi_config_t wifi_ap_config;

    strncpy((char *)wifi_ap_config.ap.ssid, ps_wifi_ap_settings->ssid, sizeof(wifi_ap_config.ap.ssid) - 1);
    strncpy((char *)wifi_ap_config.ap.password, ps_wifi_ap_settings->password, sizeof(wifi_ap_config.ap.password) - 1);

    wifi_ap_config.ap.ssid_len = strlen(ps_wifi_ap_settings->ssid);
    wifi_ap_config.ap.channel = WIFI_AP_CHANNEL;
    wifi_ap_config.ap.max_connection = WIFI_MAX_STA_CONN;
    wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_ap_config.ap.pmf_cfg.required = false;

    if (0 == strlen(ps_wifi_ap_settings->password))
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    status = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi AP config: %s", esp_err_to_name(status));
        goto exit;
    }

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ps_wifi_ap_settings->ssid, ps_wifi_ap_settings->password, WIFI_AP_CHANNEL);

exit:
    return ps_esp_netif_ap;
}

/* Initialize wifi station */
static esp_netif_t *_init_sta(void)
{
    esp_netif_t *ps_esp_netif_sta = esp_netif_create_default_wifi_sta();

    setting_wifi_t *ps_wifi_sta_settings = setting_get_wifi_sta();

    wifi_config_t s_wifi_sta_config = {0};

    strncpy((char *)s_wifi_sta_config.sta.ssid, ps_wifi_sta_settings->ssid, sizeof(s_wifi_sta_config.sta.ssid) - 1);
    strncpy((char *)s_wifi_sta_config.sta.password, ps_wifi_sta_settings->password, sizeof(s_wifi_sta_config.sta.password) - 1);

    s_wifi_sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    s_wifi_sta_config.sta.failure_retry_cnt = WIFI_MAX_RETRY;
    s_wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    s_wifi_sta_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_wifi_sta_config) );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return ps_esp_netif_sta;
}

static void _softap_set_dns_addr(esp_netif_t *ps_esp_netif_ap, esp_netif_t *ps_esp_netif_sta)
{
    esp_netif_dns_info_t s_dns;
    esp_netif_get_dns_info(ps_esp_netif_sta,ESP_NETIF_DNS_MAIN,&s_dns);
    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
    // ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(ps_esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ps_esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ps_esp_netif_ap, ESP_NETIF_DNS_MAIN, &s_dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(ps_esp_netif_ap));
}

static void _event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Station started");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(_gp_s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void _on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%ld bytes from %s icmp_seq=%d ttl=%d time=%ld ms\n",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

static void _on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

static void _on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%ld packets transmitted, %ld received, time %ldms\n", transmitted, received, total_time_ms);
}
