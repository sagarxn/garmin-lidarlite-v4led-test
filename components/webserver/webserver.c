#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "cJSON.h"

#include "wifi.h"
#include "board.h"
#include "storage.h"
#include "settings.h"


static const char* TAG = "[server]";

extern const unsigned char index_html_start[]       asm("_binary_index_html_start");
extern const unsigned char index_html_end[]         asm("_binary_index_html_end");
extern const unsigned char style_css_start[]        asm("_binary_style_css_start");
extern const unsigned char style_css_end[]          asm("_binary_style_css_end");
extern const unsigned char script_js_start[]        asm("_binary_script_js_start");
extern const unsigned char script_js_end[]          asm("_binary_script_js_end");
extern const unsigned char favicon_start[]          asm("_binary_favicon_ico_start");
extern const unsigned char favicon_end[]            asm("_binary_favicon_ico_end");
extern const unsigned char logo_start[]             asm("_binary_logo_png_start");
extern const unsigned char logo_end[]               asm("_binary_logo_png_end");
extern const unsigned char system_icon_start[]      asm("_binary_microchip_svg_start");
extern const unsigned char system_icon_end[]        asm("_binary_microchip_svg_end");
extern const unsigned char schedule_icon_start[]    asm("_binary_schedule_svg_start");
extern const unsigned char schedule_icon_end[]      asm("_binary_schedule_svg_end");
extern const unsigned char console_icon_start[]     asm("_binary_terminal_svg_start");
extern const unsigned char console_icon_end[]       asm("_binary_terminal_svg_end");
extern const unsigned char sensor_icon_start[]      asm("_binary_laser_svg_start");
extern const unsigned char sensor_icon_end[]        asm("_binary_laser_svg_end");
extern const unsigned char wifi_icon_start[]        asm("_binary_wifi_svg_start");
extern const unsigned char wifi_icon_end[]          asm("_binary_wifi_svg_end");
extern const unsigned char router_icon_start[]      asm("_binary_router_svg_start");
extern const unsigned char router_icon_end[]        asm("_binary_router_svg_end");
extern const unsigned char reboot_icon_start[]      asm("_binary_reboot_svg_start");
extern const unsigned char reboot_icon_end[]        asm("_binary_reboot_svg_end");
extern const unsigned char eye_icon_start[]         asm("_binary_eye_svg_start");
extern const unsigned char eye_icon_end[]           asm("_binary_eye_svg_end");
extern const unsigned char eye_slash_icon_start[]   asm("_binary_eye_slash_svg_start");
extern const unsigned char eye_slash_icon_end[]     asm("_binary_eye_slash_svg_end");

static void _start_web_server(void *pv_param);
static void _url_decode(char *dst, const char *src);
static esp_err_t _index_html_handler(httpd_req_t *req);
static esp_err_t _schedule_html_handler(httpd_req_t *req);
static esp_err_t _style_css_handler(httpd_req_t *req);
static esp_err_t _script_js_handler(httpd_req_t *req);
static esp_err_t _favicon_handler(httpd_req_t *req);
static esp_err_t _logo_handler(httpd_req_t *req);
static esp_err_t _system_icon_handler(httpd_req_t *req);
static esp_err_t _schedule_icon_handler(httpd_req_t *req);
static esp_err_t _console_icon_handler(httpd_req_t *req);
static esp_err_t _sensor_icon_handler(httpd_req_t *req);
static esp_err_t _wifi_icon_handler(httpd_req_t *req);
static esp_err_t _router_icon_handler(httpd_req_t *req);
static esp_err_t _reboot_icon_handler(httpd_req_t *req);
static esp_err_t _eye_icon_handler(httpd_req_t *req);
static esp_err_t _eye_slash_icon_handler(httpd_req_t *req);
static esp_err_t _get_settings_json_handler(httpd_req_t *req);
static esp_err_t _post_settings_handler(httpd_req_t *req);
static esp_err_t _restart_post_handler(httpd_req_t *req);

esp_err_t server_init(void)
{
    esp_err_t e_err = ESP_ERR_NO_MEM;

    BaseType_t task_ret = xTaskCreatePinnedToCore(_start_web_server, "webserver", 4096, NULL, 5, NULL, 1);

    if (task_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Server initialization aborted: Failed to create webserver task (Out of memory)");
        e_err =  ESP_ERR_NO_MEM;
    }

    return e_err;
}

static void _start_web_server(void *pv_param)
{
    httpd_handle_t server = NULL;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 24;
    server_config.stack_size = 8192;
    server_config.lru_purge_enable = true;

    static const httpd_uri_t route_root =       { .uri = "/", .method = HTTP_GET, .handler = _index_html_handler };
    static const httpd_uri_t route_index =      { .uri = "/index.html", .method = HTTP_GET, .handler = _index_html_handler };
    static const httpd_uri_t route_css =        { .uri = "/style.css", .method = HTTP_GET, .handler = _style_css_handler };
    static const httpd_uri_t route_js =         { .uri = "/script.js", .method = HTTP_GET, .handler = _script_js_handler };
    static const httpd_uri_t favicon_index =    { .uri = "/favicon.ico", .method = HTTP_GET, .handler = _favicon_handler };
    static const httpd_uri_t logo_index =       { .uri = "/logo.png", .method = HTTP_GET, .handler = _logo_handler };
    static const httpd_uri_t system_icon =      { .uri = "/icons/microchip.svg", .method = HTTP_GET, .handler = _system_icon_handler };
    static const httpd_uri_t schedule_icon =    { .uri = "/icons/schedule.svg", .method = HTTP_GET, .handler = _schedule_icon_handler };
    static const httpd_uri_t console_icon =     { .uri = "/icons/terminal.svg", .method = HTTP_GET, .handler = _console_icon_handler };
    static const httpd_uri_t sensor_icon =      { .uri = "/icons/laser.svg", .method = HTTP_GET, .handler = _sensor_icon_handler };
    static const httpd_uri_t wifi_icon =        { .uri = "/icons/wifi.svg", .method = HTTP_GET, .handler = _wifi_icon_handler };
    static const httpd_uri_t router_icon =      { .uri = "/icons/router.svg", .method = HTTP_GET, .handler = _router_icon_handler };
    static const httpd_uri_t reboot_icon =      { .uri = "/icons/reboot.svg", .method = HTTP_GET, .handler = _reboot_icon_handler };
    static const httpd_uri_t eye_icon =         { .uri = "/icons/eye.svg", .method = HTTP_GET, .handler = _eye_icon_handler };
    static const httpd_uri_t eye_slash_icon =   { .uri = "/icons/eye_slash.svg", .method = HTTP_GET, .handler = _eye_slash_icon_handler };

    static const httpd_uri_t api_get_settings =     { .uri = "/api/settings", .method = HTTP_GET, .handler = _get_settings_json_handler };
    static const httpd_uri_t api_post_settings =    { .uri = "/api/post-settings", .method = HTTP_POST, .handler = _post_settings_handler };
    static const httpd_uri_t api_restart_device =   { .uri = "/api/restart", .method = HTTP_POST, .handler = _restart_post_handler };

    ESP_LOGI(TAG, "Spawning HTTP Web Server instances on interface target port: %d", server_config.server_port);
    
    if (httpd_start(&server, &server_config) == ESP_OK)
    {
        /* Mounting Application Page Scopes */
        httpd_register_uri_handler(server, &route_root);
        httpd_register_uri_handler(server, &route_index);
        httpd_register_uri_handler(server, &route_css);
        httpd_register_uri_handler(server, &route_js);

        httpd_register_uri_handler(server, &favicon_index);
        httpd_register_uri_handler(server, &logo_index);
        httpd_register_uri_handler(server, &system_icon);
        httpd_register_uri_handler(server, &schedule_icon);
        httpd_register_uri_handler(server, &console_icon);
        httpd_register_uri_handler(server, &sensor_icon);
        httpd_register_uri_handler(server, &wifi_icon);
        httpd_register_uri_handler(server, &router_icon);
        httpd_register_uri_handler(server, &reboot_icon);
        httpd_register_uri_handler(server, &eye_icon);
        httpd_register_uri_handler(server, &eye_slash_icon);

        httpd_register_uri_handler(server, &api_get_settings);
        httpd_register_uri_handler(server, &api_post_settings);
        httpd_register_uri_handler(server, &api_restart_device);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to spin up HTTP engine daemon instance!");
    }

    vTaskDelete(NULL);
}

static void _url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if (('%' == *src) && ((a = src[1]) && (b = src[2])) && (isxdigit((int)a) && isxdigit((int)b)))
        {
            if (a >= 'a') a -= 'a'-'A';
            if (b >= 'a') b -= 'a'-'A';
            *dst++ = (char)(((a >= 'A' ? a - 'A' + 10 : a - '0') << 4) + (b >= 'A' ? b - 'A' + 10 : b - '0'));
            src += 3;
        }
        else if ('+' == *src)
        {
            *dst++ = ' ';
            src++;
        } 
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static esp_err_t _index_html_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

static esp_err_t _style_css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, (const char *)style_css_start, style_css_end - style_css_start);
}

static esp_err_t _script_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/javascript");
    return httpd_resp_send(req, (const char *)script_js_start, script_js_end - script_js_start);
}

static esp_err_t _favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/x-icon");
    return httpd_resp_send(req, (const char *)favicon_start, favicon_end - favicon_start);
}

static esp_err_t _logo_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/png");
    return httpd_resp_send(req, (const char *)logo_start, logo_end - logo_start);
}

static esp_err_t _system_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)system_icon_start, system_icon_end - system_icon_start);
}

static esp_err_t _console_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)console_icon_start, console_icon_end - console_icon_start);
}

static esp_err_t _sensor_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)sensor_icon_start, sensor_icon_end - sensor_icon_start);
}

static esp_err_t _wifi_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)wifi_icon_start, wifi_icon_end - wifi_icon_start);
}

static esp_err_t _router_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)router_icon_start, router_icon_end - router_icon_start);
}

static esp_err_t _reboot_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)reboot_icon_start, reboot_icon_end - reboot_icon_start);
}

static esp_err_t _eye_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)eye_icon_start, eye_icon_end - eye_icon_start);
}

static esp_err_t _eye_slash_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *)eye_slash_icon_start, eye_slash_icon_end - eye_slash_icon_start);
}

static esp_err_t _get_settings_json_handler(httpd_req_t *req)
{
    esp_err_t e_err = ESP_OK;

    cJSON *p_root = cJSON_CreateObject();
    if (NULL == p_root)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON core memory structural overflow.");
        e_err = ESP_ERR_NO_MEM;
        goto exit;
    }

    const setting_t *p_settings = setting_get();

    cJSON_AddNumberToObject(p_root, "configMode", p_settings->config_mode);
    cJSON_AddNumberToObject(p_root, "highAccuracyMode", p_settings->sensor.high_accuracy_mode);
    cJSON_AddStringToObject(p_root, "wifiSTASSID", p_settings->wifi_sta.ssid);
    cJSON_AddStringToObject(p_root, "wifiSTAPassword", p_settings->wifi_sta.password);
    cJSON_AddBooleanToObject(p_root, "wifiSTADHCPEnabled", p_settings->wifi_sta.dhcp_enabled);
    cJSON_AddStringToObject(p_root, "wifiSTAStaticIP", p_settings->wifi_sta.static_ip);
    cJSON_AddStringToObject(p_root, "wifiAPSSID", p_settings->wifi_ap.ssid);
    cJSON_AddStringToObject(p_root, "wifiAPPassword", p_settings->wifi_ap.password);
    cJSON_AddNumberToObject(p_root, "wifiAPGateway", p_settings->wifi_ap.gateway);
    cJSON_AddNumberToObject(p_root, "wifiAPNetmask", p_settings->wifi_ap.netmask);
    cJSON_AddBooleanToObject(p_root, "wifiAPDHCPEnabled", p_settings->wifi_ap.dhcp_enabled);
    cJSON_AddNumberToObject(p_root, "scheduleInterval", p_settings->schedule_interval);
    cJSON_AddNumberToObject(p_root, "scheduleStartYear", p_settings->start_year);
    cJSON_AddNumberToObject(p_root, "scheduleStartMonth", p_settings->start_month);
    cJSON_AddNumberToObject(p_root, "scheduleStartDay", p_settings->start_day);
    cJSON_AddNumberToObject(p_root, "scheduleStartHour", p_settings->start_hour);
    cJSON_AddNumberToObject(p_root, "scheduleStartMin", p_settings->start_min);

    char *p_json_str = cJSON_Print(p_root);
    printf("====================================================================\r\n");
    ESP_LOGI(TAG, "Settings JSON: %s\n", cJSON_Print(p_root));
    printf("====================================================================\r\n");

    cJSON_Delete(p_root);

    if (NULL == p_json_str)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "String translation block fault.");
        e_err = ESP_FAIL;
        goto exit;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, p_json_str);
    free(p_json_str);

exit:
    return e_err;
}

static esp_err_t _post_settings_handler(httpd_req_t *req)
{
    esp_err_t e_err = ESP_OK;
    char content[1024];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);

    int req_ret = httpd_req_recv(req, content, recv_size);
    if (req_ret <= 0)
    {
        if (HTTPD_SOCK_ERR_TIMEOUT == req_ret)
        {
            httpd_resp_send_408(req);
        }

        e_err = ESP_FAIL;
        goto exit;
    }

    content[req_ret] = '\0';

    char page_scope[32] = {0};

    if (ESP_OK == httpd_query_key_value(content, "pageScope", page_scope, sizeof(page_scope))) {
        
        printf("===============================================\r\n");
        ESP_LOGI(TAG, "POST Config Received. Scope: [%s]", page_scope);

        if (strcmp(page_scope, "settings") == 0) {
            char val[32];
            setting_t *p_settings = setting_get();

            if (httpd_query_key_value(content, "configMode", val, sizeof(val)) == ESP_OK) p_settings->sensor.config_mode = atoi(val);
            if (httpd_query_key_value(content, "highAccuracyMode", val, sizeof(val)) == ESP_OK) p_settings->sensor.high_accuracy_mode = atoi(val);
            
            if (httpd_query_key_value(content, "wifiSTASSID", val, sizeof(val)) == ESP_OK) strncpy(p_settings->wifi_sta.ssid, val, sizeof(p_settings->wifi_sta.ssid) - 1);
            if (httpd_query_key_value(content, "wifiSTAPassword", val, sizeof(val)) == ESP_OK) strncpy(p_settings->wifi_sta.password, val, sizeof(p_settings->wifi_sta.password) - 1);
            if (httpd_query_key_value(content, "wifiSTADHCPEnabled", val, sizeof(val)) == ESP_OK) p_settings->wifi_sta.dhcp_enabled = (strcmp(val, "true") == 0) ? true : false;
            if (httpd_query_key_value(content, "wifiSTAStaticIP", val, sizeof(val)) == ESP_OK) strncpy(p_settings->wifi_sta.static_ip, val, sizeof(p_settings->wifi_sta.static_ip) - 1);
            
            if (httpd_query_key_value(content, "wifiAPSSID", val, sizeof(val)) == ESP_OK) strncpy(p_settings->wifi_ap.ssid, val, sizeof(p_settings->wifi_ap.ssid) - 1);
            if (httpd_query_key_value(content, "wifiAPPassword", val, sizeof(val)) == ESP_OK) strncpy(p_settings->wifi_ap.password, val, sizeof(p_settings->wifi_ap.password) - 1);
            if (httpd_query_key_value(content, "wifiAPGateway", val, sizeof(val)) == ESP_OK) p_settings->wifi_ap.gateway = atoi(val);
            if (httpd_query_key_value(content, "wifiAPNetmask", val, sizeof(val)) == ESP_OK) p_settings->wifi_ap.netmask = atoi(val);
            if (httpd_query_key_value(content, "wifiAPDHCPEnabled", val, sizeof(val)) == ESP_OK) p_settings->wifi_ap.dhcp_enabled = (strcmp(val, "true") == 0) ? true : false;
            
            if (httpd_query_key_value(content, "scheduleInterval", val, sizeof(val)) == ESP_OK) p_settings->schedule_interval = atoi(val);
            if (httpd_query_key_value(content, "scheduleStartYear", val, sizeof(val)) == ESP_OK) p_settings->start_year = atoi(val);
            if (httpd_query_key_value(content, "scheduleStartMonth", val, sizeof(val)) == ESP_OK) p_settings->start_month = atoi(val);
            if (httpd_query_key_value(content, "scheduleStartDay", val, sizeof(val)) == ESP_OK) p_settings->start_day = atoi(val);
            if (httpd_query_key_value(content, "scheduleStartHour", val, sizeof(val)) == ESP_OK) p_settings->start_hour = atoi(val);
            if (httpd_query_key_value(content, "scheduleStartMin", val, sizeof(val)) == ESP_OK) p_settings->start_min = atoi(val);

            printf("UPDATED SETTINGS:\r\n");
            printf("configMode: %d\r\n", p_settings->sensor.config_mode);
            printf("highAccuracyMode: %d\r\n", p_settings->sensor.high_accuracy_mode);
            printf("wifiSTASSID: %s\r\n", p_settings->wifi_sta.ssid);
            printf("wifiSTAPassword: %s\r\n", p_settings->wifi_sta.password);
            printf("wifiSTADHCPEnabled: %s\r\n", p_settings->wifi_sta.dhcp_enabled ? "true" : "false");
            printf("wifiSTAStaticIP: %s\r\n", p_settings->wifi_sta.static_ip);
            printf("wifiAPSSID: %s\r\n", p_settings->wifi_ap.ssid);
            printf("wifiAPPassword: %s\r\n", p_settings->wifi_ap.password);
            printf("wifiAPGateway: %d\r\n", p_settings->wifi_ap.gateway);
            printf("wifiAPNetmask: %d\r\n", p_settings->wifi_ap.netmask);
            printf("wifiAPDHCPEnabled: %s\r\n", p_settings->wifi_ap.dhcp_enabled ? "true" : "false");
            printf("scheduleInterval: %d\r\n", p_settings->schedule_interval);
            printf("scheduleStartYear: %d\r\n", p_settings->start_year);
            printf("scheduleStartMonth: %d\r\n", p_settings->start_month);
            printf("scheduleStartDay: %d\r\n", p_settings->start_day);
            printf("scheduleStartHour: %d\r\n", p_settings->start_hour);
            printf("scheduleStartMin: %d\r\n", p_settings->start_min);
        }
        printf("===============================================\r\n");
    }
    
    if (settings_save(p_settings) == ESP_OK)
    {
        ESP_LOGI(TAG, "Configuration committed to flash successfully.");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "SUCCESS");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Flash block sync write sequence faulted!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash block sync write sequence faulted.");
        return ESP_FAIL;
    }
}

static esp_err_t _restart_post_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Hardware reset requested via Web UI context");
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "REBOOTING", 9);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}
