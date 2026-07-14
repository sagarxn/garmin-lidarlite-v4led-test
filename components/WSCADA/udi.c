#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "mqtt_client.h"
#include "esp_http_client.h"

#include "udi.h"

/* PRIVATE VARIABLES */
#define HTTP_VALID_RESPONSE_BIT       BIT0
#define HTTP_INVALID_RESPONSE_BIT     BIT1
#define MQTT_CONNECT_BIT              BIT2
#define MQTT_SUBSCRIBE_BIT            BIT3
#define MQTT_PUBLISH_BIT              BIT4
#define MQTT_VALID_RESPONSE_BIT       BIT5
#define MQTT_INVALID_RESPONSE_BIT     BIT6

static const char * g_p_label     = "udi";
EventGroupHandle_t g_p_grp_handle = NULL;

/* PRIVATE FUNCTION DECLARATIONS */
static esp_err_t _http_event_handler (esp_http_client_event_t * p_event);
static void _mqtt_event_handler  (void * p_user_arg, esp_event_base_t p_event_base, int32_t event_id, void * p_event_data);

#undef  TRACE_I
#undef  TRACE_E
#define TRACE_I(X, ...)                         ESP_LOGI(g_p_label, X, ##__VA_ARGS__)
#define TRACE_E(X, ...)                         ESP_LOGE(g_p_label, X, ##__VA_ARGS__)

/********************************************** PUBLIC FUNCTION DEFINITION **********************************************/
esp_err_t udi_get_from_server(udi_server_t * p_server_info, const char * p_unique_info, device_unique_info_t e_unique_info, device_property_t e_device_property, const char * p_device_type, char * p_udi)
{
	esp_err_t e_err = ESP_ERR_INVALID_ARG;

	if (p_server_info && p_unique_info && p_device_type && p_udi)
	{
		if ( (NULL == p_server_info->p_host) || (0 == p_server_info->p_host[0]) )
		{
			p_server_info->p_host = "hardware.wscada.net";
		}

		if (p_server_info->p_cert_pem)
		{
			if (0 == p_server_info->port)
			{
				p_server_info->port = 483;
			}
		}
		else
		{
			if (0 == p_server_info->port)
			{
				p_server_info->port = 88;
			}
		}

		TRACE_I("fetch connect: %s:%d", p_server_info->p_host, p_server_info->port);

		g_p_grp_handle = xEventGroupCreate();
	    esp_http_client_config_t s_config;
	    memset(&s_config, 0, sizeof(esp_http_client_config_t));

	    s_config.host = p_server_info->p_host;
	    s_config.path = "/api/device/register";
		s_config.port = p_server_info->port;
		s_config.transport_type = HTTP_TRANSPORT_OVER_TCP;
		s_config.event_handler = _http_event_handler;
		s_config.cert_pem = p_server_info->p_cert_pem;
		s_config.user_data = (void *)p_udi;

	    esp_http_client_handle_t p_client = esp_http_client_init(&s_config);

	    if (p_client)
	    {
			esp_http_client_set_method(p_client, HTTP_METHOD_POST);
			esp_http_client_set_header(p_client, "Cache-Control", "no-cache");
			esp_http_client_set_header(p_client, "Connection", "close");
			esp_http_client_set_header(p_client, "Content-Type", "application/json");

			const char * p_data = NULL;
			const char * p_mac[] = {"processorID", "macAddress"};
			const char * p_device_property[] = {"publisher", "subscriber", "subscriber+"};
			
			uint32_t data_len = asprintf((char **)&p_data, "{\"messagingPattern\":\"%s\",\"type\":\"%s\",\"%s\":\"%s\"}", p_device_property[e_device_property], p_device_type, p_mac[e_unique_info], p_unique_info);
			esp_http_client_set_post_field(p_client, p_data, data_len);
//			TRACE("pubData: %.*s", data_len, p_data);
			esp_http_client_perform(p_client);
			int32_t status_code = esp_http_client_get_status_code(p_client);
			TRACE_I("HTTP POST status: %d, content length: %d", status_code, esp_http_client_get_content_length(p_client));

			if (200 == status_code)
			{
				EventBits_t event_bit = xEventGroupWaitBits(g_p_grp_handle, HTTP_VALID_RESPONSE_BIT | HTTP_INVALID_RESPONSE_BIT, pdTRUE, pdFALSE, 5000);
				if (event_bit & HTTP_VALID_RESPONSE_BIT)
				{
					TRACE_I("validHttpResponse");
					e_err = ESP_OK;
				}
				else if (event_bit & HTTP_INVALID_RESPONSE_BIT)
				{
					TRACE_E("invalidHttpResponse");
					e_err = ESP_ERR_INVALID_RESPONSE;
				}
				else
				{
					TRACE_E("httpResponseTimeout");
					e_err = ESP_ERR_TIMEOUT;
				}
			}
			else
			{
				e_err = ESP_FAIL;
			}

			free((void *)p_data);
			esp_http_client_cleanup(p_client);
	    }
	}

	return e_err;
}

esp_err_t udi_register_to_server(udi_register_t * p_register_info, const char * p_client_id, const char * p_pub_topic, char * p_udi)
{
	esp_err_t e_err = ESP_ERR_INVALID_ARG;

	if (p_register_info && p_client_id && p_udi)
	{
		if (NULL == p_register_info->s_server.p_host)
		{
			p_register_info->s_server.p_host = "hardware.wscada.net";
		}

		if (p_register_info->s_server.p_cert_pem)
		{
			p_register_info->s_server.port = 8883;
		}
		else
		{
			p_register_info->s_server.port = 1883;
		}

		if (NULL == p_register_info->p_username)
		{
			p_register_info->p_username = "rtsfirmware";
		}

		if (NULL == p_register_info->p_password)
		{
			p_register_info->p_password = "X9pm?F=tTvr=9$yS";
		}

		TRACE_I("register connect: %s:%d", p_register_info->s_server.p_host, p_register_info->s_server.port);

		esp_mqtt_client_config_t s_config;
		memset(&s_config, 0, sizeof(s_config));

		s_config.credentials.authentication.certificate = p_register_info->s_server.p_cert_pem;
		s_config.credentials.authentication.certificate_len = 0;//p_register_info->s_server.cert_len;
		s_config.credentials.client_id = p_client_id;
		s_config.credentials.username = p_register_info->p_username;
		s_config.credentials.authentication.password = p_register_info->p_password;
		s_config.broker.address.port = p_register_info->s_server.port;
		s_config.broker.address.hostname = p_register_info->s_server.p_host;

		if (8883 == s_config.broker.address.port)
		{
			s_config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
		}
		else
		{
			s_config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
		}

		esp_mqtt_client_handle_t p_mqtt_client = esp_mqtt_client_init(&s_config);

		do
		{
			if (!p_mqtt_client)
			{
				e_err = ESP_FAIL;
				break;
			}

			esp_mqtt_client_register_event(p_mqtt_client, ESP_EVENT_ANY_ID, _mqtt_event_handler, NULL);
			esp_mqtt_client_start(p_mqtt_client);

			EventBits_t event_bit = xEventGroupWaitBits(g_p_grp_handle, MQTT_CONNECT_BIT, pdTRUE, pdFALSE, 10000);
			if (!(event_bit & MQTT_CONNECT_BIT))
			{
				e_err = ESP_ERR_TIMEOUT;
				break;
			}

			if (NULL == p_pub_topic)
			{
				p_pub_topic = "RTSR&D/baanvak/ureq";
			}

			char * p_sub_topic = NULL;
			asprintf(&p_sub_topic, "%s/%.*s", p_pub_topic, 16, p_udi);
			TRACE_I("subTopic: %s", p_sub_topic);

			int32_t ret = esp_mqtt_client_subscribe(p_mqtt_client, p_sub_topic, 2);
			TRACE_I("mqttSubcribe status: %d", ret);

			event_bit = xEventGroupWaitBits(g_p_grp_handle, MQTT_SUBSCRIBE_BIT, pdTRUE, pdFALSE, 10000);
			if (!(event_bit & MQTT_SUBSCRIBE_BIT))
			{
				e_err = ESP_ERR_TIMEOUT;
				break;
			}

			char * p_pub_data = NULL;
			uint32_t data_len = asprintf(&p_pub_data, "{\"udi\":\"%.*s\"}", 16, p_udi);
			if (!data_len)
			{
				e_err = ESP_ERR_NO_MEM;
				break;
			}

			ret = esp_mqtt_client_publish(p_mqtt_client, p_sub_topic, p_pub_data, data_len, 2, 0);
			TRACE_I("pubData: %s, mqttPublish status: %d", p_pub_data, ret);
			free(p_pub_data);
			free(p_sub_topic);

			event_bit = xEventGroupWaitBits(g_p_grp_handle, MQTT_PUBLISH_BIT, pdTRUE, pdFALSE, 10000);
			if (!(event_bit & MQTT_PUBLISH_BIT))
			{
				e_err = ESP_ERR_TIMEOUT;
				break;
			}

			event_bit = xEventGroupWaitBits(g_p_grp_handle, MQTT_VALID_RESPONSE_BIT | MQTT_INVALID_RESPONSE_BIT, pdTRUE, pdFALSE, 10000);
			if (event_bit & MQTT_VALID_RESPONSE_BIT)
			{
				TRACE_I("validMqttResponse");
				e_err = ESP_OK;
			}
			else if (event_bit & MQTT_INVALID_RESPONSE_BIT)
			{
				TRACE_E("invalidMqttResponse");
				e_err = ESP_ERR_INVALID_RESPONSE;
			}
			else
			{
				TRACE_E("mqttResponseTimeout");
				e_err = ESP_ERR_TIMEOUT;
			}

		} while (0);

		if (p_mqtt_client)
		{
			esp_mqtt_client_stop(p_mqtt_client);
			esp_mqtt_client_destroy(p_mqtt_client);
		}

		vEventGroupDelete(g_p_grp_handle);
		g_p_grp_handle = NULL;
	}

	return e_err;
}

/********************************************** PRIVATE FUNCTION DEFINITION **********************************************/
static void _process_http_response (const char * p_data, uint32_t data_len, char * p_user_data)
{
	if ( (NULL == strstr(p_data, "invalid")) && (NULL == strstr(p_data, "Invalid")) && (NULL == strstr(p_data, "error")))
	{
		if (16 == data_len)
		{
			memcpy((void *)p_user_data, (const void *)p_data, data_len);
			xEventGroupSetBits(g_p_grp_handle, HTTP_VALID_RESPONSE_BIT);
		}
		else
		{
			xEventGroupSetBits(g_p_grp_handle, HTTP_INVALID_RESPONSE_BIT);
		}
	}
	else
	{
		xEventGroupSetBits(g_p_grp_handle, HTTP_INVALID_RESPONSE_BIT);
	}
}

static void _process_mqtt_response (const char * p_data, uint32_t data_len)
{
	char * status = "errors";
	char * ptr = strstr(p_data, status);

	if (ptr)
	{
		ptr += strlen(status) + 2;
		char * temp;

		uint32_t ret = strtol(ptr, &temp, 10);

		if ( ( 0 == ret ) && ( ptr != temp ) )
		{
			xEventGroupSetBits(g_p_grp_handle, MQTT_VALID_RESPONSE_BIT);
		}
		else
		{
			xEventGroupSetBits(g_p_grp_handle, MQTT_INVALID_RESPONSE_BIT);
		}
	}
}

static esp_err_t _http_event_handler (esp_http_client_event_t * p_event)
{
    switch(p_event->event_id)
    {
        case HTTP_EVENT_ON_DATA:
        	TRACE_I("HTTP_EVENT_ON_DATA, len=%d", p_event->data_len);
        	TRACE_I("%.*s", p_event->data_len, (const char *)p_event->data);
        	if (!esp_http_client_is_chunked_response(p_event->client))
            {
                 _process_http_response(p_event->data, p_event->data_len, p_event->user_data);
            }

            break;
        default:
            break;
    }

    return ESP_OK;
}

static void _mqtt_event_handler  (void * p_user_arg, esp_event_base_t p_event_base, int32_t event_id, void * p_event_data)
{
	esp_mqtt_event_handle_t p_event_handle = p_event_data;

	switch ( p_event_handle->event_id )
	{
		case MQTT_EVENT_CONNECTED:
			TRACE_I("MQTT Connected");
			xEventGroupSetBits(g_p_grp_handle, MQTT_CONNECT_BIT);
		break;

		case MQTT_EVENT_DISCONNECTED:
			TRACE_E("MQTT Disconnected");
		break;

		case MQTT_EVENT_SUBSCRIBED:
			TRACE_I("Subscribed Success, subId : %d, freeHeap: %d", p_event_handle->msg_id, xPortGetFreeHeapSize());
			xEventGroupSetBits(g_p_grp_handle, MQTT_SUBSCRIBE_BIT);
		break;

		case MQTT_EVENT_UNSUBSCRIBED:
			TRACE_I("Unsubscribed Success, subId : %d, freeHeap: %d", p_event_handle->msg_id, xPortGetFreeHeapSize());
		break;

		case MQTT_EVENT_PUBLISHED:
			TRACE_I("Publised success, pubId: %d, freeHeap: %d", p_event_handle->msg_id, xPortGetFreeHeapSize());
			xEventGroupSetBits(g_p_grp_handle, MQTT_PUBLISH_BIT);
		break;

		case MQTT_EVENT_DATA:
			TRACE_I("Topic: %.*s", p_event_handle->topic_len, p_event_handle->topic);
			TRACE_I("Data: %.*s", p_event_handle->data_len, p_event_handle->data);
			_process_mqtt_response(p_event_handle->data, p_event_handle->data_len);
		break;

		case MQTT_EVENT_ERROR:
			TRACE_E("MQTT event error");
		break;

		default:

		break;
	}
}

#undef  TRACE_I
#undef  TRACE_E
