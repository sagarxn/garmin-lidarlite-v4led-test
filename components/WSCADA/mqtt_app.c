/*
 * mqtt_app.c
 *
 *  Created on: Aug 21, 2023
 *  By: Ravi Tamrakar
 */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_check.h"
#include "esp_app_desc.h"

#include "setting.h"
#include "cm.h"
#include "udi.h"
#include "rts_mqtt.h"
#include "update_client.h"
#include "debug.h"
#include "sensor_rtk.h"
#include "webserver.h"

/*---------------------------------------------------------------------------------------------------------------
* MACROS AND DEFINES
*--------------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------------
* EXTERN PUBLIC VARIABLES
*--------------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------------
* EXTERN PUBLIC FUNCTION DECLARATIONS
*--------------------------------------------------------------------------------------------------------------*/
const char * app_get_device_type (void);
const char * app_get_device_mac_info (void);

/*---------------------------------------------------------------------------------------------------------------
* PRIVATE STRUCTURES
*--------------------------------------------------------------------------------------------------------------*/
typedef struct MQTT_APP
{
	TaskHandle_t p_mqtt_process;
	QueueHandle_t p_pub_queue;
	uint32_t stop_flag;
} mqtt_app_t;

/*---------------------------------------------------------------------------------------------------------------
* PRIVATE FUNCTION DECLARATIONS
*--------------------------------------------------------------------------------------------------------------*/
static void _mqtt_process (void * vp_arg);

/*---------------------------------------------------------------------------------------------------------------
* PRIVATE VARIABLES
*--------------------------------------------------------------------------------------------------------------*/
static const char * LOG_TAG = "[mqtt-app]";
static const char * gp_c_sub_topic_hdr = "RTSR&D/baanvak/sub";
static const char * gp_c_pub_topic_hdr = "RTSR&D/baanvak/pub";

// static const char * gp_c_payload_fmt =
// "{\"essential\":{\"publisherudi\":\"%s\",\"payloadType\":\"%s\",\"payload\":{\"alias\":\"%s\",\"thingCode\":%d,\"details\":{\"%s\":%.2f}}}}";

static const char * gp_c_rtk_payload_frmt =
"{"
	"\"essential\":{"
		"\"publisherudi\":\"%s\","
		"\"payloadType\":\"%s\","
			"\"payload\":{"
				"\"alias\":\"%s\","
				"\"deviceCode\":%d,"
				"\"utc\":\"%s\","
				"\"lat\":\"%s\","
				"\"long\":\"%s\","
				"\"alt\":\"%s\","
				"\"hdop\":\"%s\","
				"\"geoSep\":\"%s\","
				"\"mode\":%d,"
				"\"battery\":%.3f,"
				"\"solar\":%.2f"
			"}"
	"}"
"}";

static const char * gp_c_pub_info_frmt =
"{" \
"\"essential\":{" \
"\"publisherudi\":\"%s\"," \
"\"payloadType\":\"info\"" \
"}," \
"\"extra\":{" \
"\"firmwareVersion\":\"%s %s %s\"," \
"\"hardwareVersion\":\"%s\"" \
"}," \
"\"vNum\":%d" \
"}";

static const char * gc_p_base_pub_info_frmt =
"{" \
"\"essential\":{" \
"\"publisherudi\":\"%s\"," \
"\"payloadType\":\"info\"" \
"}," \
"\"extra\":{" \
"\"firmwareVersion\":\"%s %s %s\"," \
"\"hardwareVersion\":\"%s\"," \
"\"location\":{" \
"\"latitude\":\"%s\"," \
"\"longitude\":\"%s\"," \
"\"altitude\":\"%s\"" \
"}" \
"}," \
"\"vNum\":%d" \
"}";

static mqtt_app_t g_s_self = {0};

/*---------------------------------------------------------------------------------------------------------------
* PRIVATE INLINE FUNCTION DEFINITIONS
*--------------------------------------------------------------------------------------------------------------*/
static inline esp_err_t _register_udi (const mqtt_server_info_t * p_cs_mqtt_server_info)
{
	void * vp_cm_handle = NULL;
	uint32_t cm_bit_val = 0;
	
	do
	{
		vp_cm_handle = cm_prepare_dynamic(0, FEATURE_DATA, xTaskGetCurrentTaskHandle(), NULL, &cm_bit_val);
		vTaskDelay(1000);
	} while (NULL == vp_cm_handle);

	udi_server_t s_udi_server =
	{
		.p_host = (char *)p_cs_mqtt_server_info->s_server_info.host_name,
		.port = 88
	};
	
	char udi[17] = {0};
	
	esp_err_t e_err = udi_get_from_server(&s_udi_server, app_get_device_mac_info(), UNIQUE_IS_MAC, 
					  (device_property_t)p_cs_mqtt_server_info->dev_property, app_get_device_type(), udi);
	ESP_LOGI(LOG_TAG, "UDI get status: %s", esp_err_to_name(e_err));

	if (!e_err)
	{
		udi_register_t s_udi_register =
		{
			.s_server = s_udi_server,
			.p_username = p_cs_mqtt_server_info->s_server_info.username,
			.p_password = p_cs_mqtt_server_info->s_server_info.password
		};

		if (0 == strlen(s_udi_register.p_username))
		{
			s_udi_register.p_username = "rtsfirmware";
		}

		if (0 == strlen(s_udi_register.p_password))
		{
			s_udi_register.p_password = "X9pm?F=tTvr=9$yS";
		}

		e_err = udi_register_to_server(&s_udi_register, udi, NULL, udi);
		ESP_LOGI(LOG_TAG, "UDI register status: %s", esp_err_to_name(e_err));

		if (!e_err)
		{
			strlcpy((char *)setting_get_device_info()->udi, udi, 17);
			setting_save_device_info();
		}
	}
	
	cm_trigger_done(vp_cm_handle, cm_bit_val, FEATURE_DATA, COMM_ERROR_NONE);
	
	return e_err;
}

static inline esp_err_t _publish_firmware_version (const char * p_c_pub_topic, const char * cp_device_udi)
{
	char * p_buffer = NULL;
	const esp_app_desc_t * p_cs_app_desc = esp_app_get_description();
	uint32_t len = 0;
	rtk_info_t * ps_rtk_info = setting_get_rtk_info();
	const char * cp_hw_version = "unknown";
	#if defined(BSP_3_0)
	cp_hw_version = "PCB_RTK_v3.0";
	#elif defined(BSP_3_1)
	cp_hw_version = "PCB_RTK_v3.1";
	#elif defined(BSP_3_3)
	cp_hw_version = "PCB_RTK_v3.3";
	#endif	

	if ( RTK_MODE_ROVER == ps_rtk_info->e_rtk_mode )
	{
		len = asprintf(&p_buffer, gp_c_pub_info_frmt, cp_device_udi, p_cs_app_desc->version, p_cs_app_desc->date, p_cs_app_desc->time, cp_hw_version, 0);
	}
	else
	{
		len = asprintf(&p_buffer, gc_p_base_pub_info_frmt, cp_device_udi, p_cs_app_desc->version, p_cs_app_desc->date, p_cs_app_desc->time, cp_hw_version,
						ps_rtk_info->latitude, ps_rtk_info->longitude, ps_rtk_info->altitude, 0);
	}
		
	esp_err_t e_err = rts_mqtt_publish(p_c_pub_topic, (const char *)p_buffer, len, 0);

	free(p_buffer);
	return e_err;
}

/*---------------------------------------------------------------------------------------------------------------
* PUBLIC FUNCTION DEFINITIONS
*--------------------------------------------------------------------------------------------------------------*/
esp_err_t mqtt_app_init (void)
{
	esp_err_t e_err = ESP_ERR_INVALID_ARG;
	const mqtt_server_info_t * p_cs_mqtt_server_info = setting_get_mqtt_server_info();

	if (0 == strlen(p_cs_mqtt_server_info->s_server_info.host_name) || 0 == p_cs_mqtt_server_info->s_server_info.port) goto exit;

	g_s_self.p_pub_queue = xQueueCreate(5, sizeof(const char *));
	if (NULL == g_s_self.p_pub_queue)
	{
		ESP_LOGE(LOG_TAG, "Failed to create pub queue");
		goto exit;
	}

	if (xTaskCreatePinnedToCore(_mqtt_process, "mqttPro", configMINIMAL_STACK_SIZE * 4, (void *)p_cs_mqtt_server_info, 3, &g_s_self.p_mqtt_process, 0) != pdPASS)
	{
		ESP_LOGE(LOG_TAG, "Failed to create mqtt process");
		vQueueDelete(g_s_self.p_pub_queue);
		g_s_self.p_pub_queue = NULL;
		goto exit;
	}

	e_err = ESP_OK;

exit:
	return e_err;
}

esp_err_t mqtt_app_queue_rtk_data (const rtk_gps_data_t * cp_s_gps_data)
{
	char * p_payload = NULL;
	esp_err_t e_err = ESP_ERR_INVALID_STATE;

	if (!g_s_self.p_pub_queue) 
		goto exit;

	asprintf(&p_payload, gp_c_rtk_payload_frmt, setting_get_device_udi(), 
												"info", 
												setting_get_device_info()->name, 
												setting_get_device_code(),
												cp_s_gps_data->utc, 
												cp_s_gps_data->latitude,
												cp_s_gps_data->longitude,
												cp_s_gps_data->altitude,
												cp_s_gps_data->hdop,
												cp_s_gps_data->geo_sep,
												cp_s_gps_data->pos_fix,
												cp_s_gps_data->batt_voltage,
												cp_s_gps_data->solar_voltage);

		
	if (!p_payload)
	{
		e_err = ESP_ERR_NO_MEM;
		goto exit;
	}
	
	e_err = xQueueSend(g_s_self.p_pub_queue, (void *)&p_payload, 0)? ESP_OK : ESP_FAIL;
	if (e_err)
	{
		xQueueReset(g_s_self.p_pub_queue);
		free(p_payload);
	}

exit :
	return e_err;
}

esp_err_t mqtt_app_publish (char * p_publish_topic, const char * cp_payload, uint32_t payload_len)
{
	char pub_topic[128] = {0};

	if (NULL == p_publish_topic)
	{
		p_publish_topic = pub_topic;		
		snprintf(p_publish_topic, sizeof(pub_topic), "%s/%.*s", gp_c_pub_topic_hdr, 16, setting_get_device_udi());
	}

	esp_err_t e_err = rts_mqtt_publish(p_publish_topic, (const char *)cp_payload, payload_len, 0);
	ESP_LOGI(LOG_TAG, "MQTT Publish status: %s", esp_err_to_name(e_err));

	return e_err;
}

esp_err_t mqtt_app_queue_to_publish (const char * p_c_data)
{
	esp_err_t e_err = ESP_ERR_INVALID_STATE;
	char * p_payload = NULL;

	if (!g_s_self.p_pub_queue) 
		goto exit;
	
	asprintf(&p_payload, "%s", p_c_data);
		
	if (!p_payload)
	{
		e_err = ESP_ERR_NO_MEM;
		goto exit;
	}
	
	e_err = xQueueSend(g_s_self.p_pub_queue, (void *)&p_payload, 0)? ESP_OK : ESP_FAIL;
	if (e_err)
	{
		free(p_payload);
	}
	
exit:
	return e_err;
}

void mqtt_app_stop ( void )
{
	ESP_LOGW(LOG_TAG, "Stopping MQTT_APP..");
	if ( g_s_self.p_mqtt_process )
	{
		g_s_self.stop_flag = 1;
		xTaskNotify(g_s_self.p_mqtt_process, 10, eSetValueWithOverwrite);
		
		do 
		{
			vTaskDelay(500);
		} while ( g_s_self.p_mqtt_process );
	}
}

/*---------------------------------------------------------------------------------------------------------------
* PRIVATE FUNCTION DEFINITIONS
*--------------------------------------------------------------------------------------------------------------*/
static esp_err_t _subscription_msg_handler (const char * p_c_payload, uint32_t payload_len, void * vp_user_arg)
{
	/*  Backdoor to restart device */
	char * p_data = strstr(p_c_payload, "restart#123@rts");

	if (p_data)
	{
		snprintf(p_data, 64, "{\"udi\":\"%s\",\"status\":\"restarting\"}", setting_get_device_info()->udi);
		esp_err_t e_err = rts_mqtt_publish(setting_get_mqtt_server_info()->pub_topic, (const char *)p_data, strlen(p_data), 0);
		LOG_STATUS(LOG_TAG, e_err, "dev mqtt publish status: %s", esp_err_to_name(e_err));

		esp_restart();
	}

	return ESP_OK;
}

static esp_err_t _update_client_init (const update_server_info_t * p_cs_update_server_info, const char * p_c_device_udi)
{
	esp_err_t e_err = ESP_ERR_INVALID_ARG;

	if (0 == strlen(p_cs_update_server_info->s_server_info.host_name) || 0 == p_cs_update_server_info->s_server_info.port) goto exit;

	static update_ftp_cfg_t s_update_ftp_cfg;
	s_update_ftp_cfg = (update_ftp_cfg_t) 
	{
		.p_hostname = p_cs_update_server_info->s_server_info.host_name,
		.port 		= p_cs_update_server_info->s_server_info.port,
		.p_username = p_cs_update_server_info->s_server_info.username,
		.p_password = p_cs_update_server_info->s_server_info.password
	};

	if (0 == strlen(p_cs_update_server_info->s_server_info.username))
	{
		s_update_ftp_cfg.p_username = "ota";
	}
	
	if (0 == strlen(p_cs_update_server_info->s_server_info.password))
	{
		s_update_ftp_cfg.p_password = "rtshardware";
	}

	uint32_t err = update_client_start(p_c_device_udi, &s_update_ftp_cfg);
	if (err) e_err = ESP_FAIL; 
	else e_err = ESP_OK;

exit:
	return e_err;
}

static esp_err_t _mqtt_client_init (const mqtt_server_info_t * p_cs_mqtt_server_info)
{
	esp_mqtt_client_config_t s_mqtt_client_cfg;
	memset(&s_mqtt_client_cfg, 0, sizeof(s_mqtt_client_cfg));

	char uri[128] = {0};
	sprintf(uri, "%s://%s:%ld", "mqtt", p_cs_mqtt_server_info->s_server_info.host_name, p_cs_mqtt_server_info->s_server_info.port);
	
	s_mqtt_client_cfg.broker.address.uri = uri;
	s_mqtt_client_cfg.broker.address.port= p_cs_mqtt_server_info->s_server_info.port;
	s_mqtt_client_cfg.session.protocol_ver = MQTT_PROTOCOL_V_5;
	s_mqtt_client_cfg.credentials.username  = p_cs_mqtt_server_info->s_server_info.username;
	s_mqtt_client_cfg.credentials.authentication.password  = p_cs_mqtt_server_info->s_server_info.password;
	s_mqtt_client_cfg.credentials.client_id = setting_get_device_info()->udi;

	s_mqtt_client_cfg.session.disable_clean_session = false;
	s_mqtt_client_cfg.session.keepalive = p_cs_mqtt_server_info->keepalive_sec;
	if (0 == s_mqtt_client_cfg.session.keepalive)
	{
		s_mqtt_client_cfg.session.disable_keepalive = true;
	}
	
	s_mqtt_client_cfg.network.reconnect_timeout_ms = 1000;
	s_mqtt_client_cfg.network.disable_auto_reconnect = false;

	s_mqtt_client_cfg.buffer.out_size = 4096;
	s_mqtt_client_cfg.buffer.size = 4096;

	if (0 == strlen(s_mqtt_client_cfg.credentials.username))
	{
		s_mqtt_client_cfg.credentials.username = "rtsfirmware";
	}

	if (0 == strlen(s_mqtt_client_cfg.credentials.authentication.password))
	{
		s_mqtt_client_cfg.credentials.authentication.password = "X9pm?F=tTvr=9$yS";
	}

	esp_err_t e_err = rts_mqtt_init(&s_mqtt_client_cfg);

	if (ESP_OK == e_err)
	{
		e_err = rts_mqtt_start();
	}

	return e_err;
}

static void _mqtt_process (void * vp_arg)
{
	const mqtt_server_info_t * p_cs_mqtt_server_info = vp_arg;
	
	esp_err_t e_err = ESP_OK;
	void * vp_cm_handle = NULL;
	uint32_t cm_bit_val = 0;
	char sub_topic[128] = {0};
	char pub_topic[128] = {0};

	/* Check if the UDI is valid. Otherwise register UDI */
	uint32_t udi_len = strlen(setting_get_device_info()->udi);

	if (udi_len != 16)
	{
		e_err = _register_udi(p_cs_mqtt_server_info);
		if (e_err) goto exit;
	}

	const char * p_c_device_udi = setting_get_device_info()->udi;

	/* Check for availability of subscription topic */
	if (0 == strlen(p_cs_mqtt_server_info->sub_topic))
	{
		snprintf(sub_topic, sizeof(sub_topic), "%s/%.*s", gp_c_sub_topic_hdr, 16, p_c_device_udi);
	}
	else
	{
		strlcpy(sub_topic, p_cs_mqtt_server_info->sub_topic, sizeof(p_cs_mqtt_server_info->sub_topic));
	}

	/* Check for availability of publish topic */
	if (0 == strlen(p_cs_mqtt_server_info->pub_topic))
	{
		snprintf(pub_topic, sizeof(pub_topic), "%s/%.*s", gp_c_pub_topic_hdr, 16, p_c_device_udi);
	}
	else
	{
		strlcpy(pub_topic, p_cs_mqtt_server_info->pub_topic, sizeof(p_cs_mqtt_server_info->pub_topic));
	}

	/* Update Server */
	const update_server_info_t * p_cs_update_server_info = setting_get_update_server_info();
	uint32_t one_time = 1; // One time publish firmware version
	uint32_t stop_notification = 0;

	do
	{
		while ( webserver_is_active() )
		{
			vTaskDelay(1000);
		}

		do
		{
			vp_cm_handle = cm_prepare_dynamic(0, FEATURE_DATA, xTaskGetCurrentTaskHandle(), NULL, &cm_bit_val);
			vTaskDelay(1000);

			if ( ulTaskNotifyTake(pdTRUE, 100) || g_s_self.stop_flag )
			{
				ESP_LOGW(LOG_TAG, "Received stop notification!!! Stopping the client");
				stop_notification = 1;
				break;
			}

		} while (NULL == vp_cm_handle);

		ESP_LOGE(LOG_TAG, "MQTT process started with cm_handle: %p, cm_bit_val: %d", vp_cm_handle, cm_bit_val);

		if ( (!vp_cm_handle && stop_notification) || ( (10 == (uint32_t)vp_cm_handle) && stop_notification) )
		{
			goto notification;
		}

		e_err = _mqtt_client_init(p_cs_mqtt_server_info);
		if (e_err)
		{
			ESP_LOGE(LOG_TAG, "Failed to init MQTT client: %s", esp_err_to_name(e_err));
			goto skip;
		}

		e_err = rts_mqtt_is_client_connected(10000);
		if ( e_err )
		{
			ESP_LOGE(LOG_TAG, "MQTT client failed to connect: %s", esp_err_to_name(e_err));
			goto skip;
		}

		e_err = _update_client_init(p_cs_update_server_info, p_c_device_udi);
		if (!e_err && one_time)
		{
			char update_topic[128];
			snprintf(update_topic, sizeof(update_topic), "%s/%.*s", gp_c_pub_topic_hdr, 16, p_c_device_udi);

			if (ESP_OK == _publish_firmware_version(update_topic, p_c_device_udi))
			{
				one_time = 0;
			}
		}
		
		if (p_cs_mqtt_server_info->dev_property)
		{
			e_err = rts_mqtt_subscribe(sub_topic, _subscription_msg_handler, NULL);
			LOG_STATUS(LOG_TAG, e_err, "Subscribe topic: %s", sub_topic);
			if (e_err) goto skip;
		}

		if ( stop_notification )
		{
			goto skip;
		}

		do	/* Wait for Publish Queue and do the necessary mqtt publish stuff. Also check if mqtt is connected or not */
		{
			char * p_payload = NULL;
			if (xQueueReceive(g_s_self.p_pub_queue, (void *)&p_payload, 5000))
			{
				e_err = rts_mqtt_publish(pub_topic, p_payload, strlen(p_payload), 0);
				LOG_STATUS(LOG_TAG, e_err, "Publish status: %s", esp_err_to_name(e_err));
				free(p_payload);
				vTaskDelay(10);
			}
			
			if ( webserver_is_active() )
			{
				ESP_LOGW(LOG_TAG, "Webserver is active. Stopping MQTT process..");
				break;
			}

			stop_notification = ulTaskNotifyTake(pdTRUE, 100);

			if ( stop_notification )
			{
				ESP_LOGW(LOG_TAG, "Received notification to stop MQTT process..");
				break;
			}
		} while (ESP_OK == rts_mqtt_is_client_connected(100));

skip:
		update_client_wait_to_complete_with_timeout(5000);
		update_client_stop();
		rts_mqtt_deinit();

notification:		
		cm_err_t e_cm_err = webserver_is_active()? COMM_ERROR_NONE : COMM_ERROR_NET_DOWN;	
		if ( stop_notification )
		{
			g_s_self.stop_flag = 0;
			cm_trigger_done(vp_cm_handle, cm_bit_val, FEATURE_DATA, COMM_ERROR_NONE);
			vQueueDelete(g_s_self.p_pub_queue);
			g_s_self.p_pub_queue = NULL;
			break;
		}
		else
		{
			cm_trigger_done(vp_cm_handle, cm_bit_val, FEATURE_DATA, e_cm_err);
		}		
	} while ( 1 );

exit:
	g_s_self.p_mqtt_process = NULL;
	ESP_LOGE(LOG_TAG, "MQTT process stopped. Deleting task..");
	vTaskDelete(NULL);
}