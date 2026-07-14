#ifndef __UDI_H__
#define __UDI_H__

typedef enum
{
	DEVICE_PUBLISHER,
	DEVICE_SUBSCRIBER,
	DEVICE_SUBSCRIBER_PUBLISHER,
} device_property_t;

typedef enum
{
	UNIQUE_IS_NOT_MAC,
	UNIQUE_IS_MAC,
} device_unique_info_t;

typedef struct
{
	const char * p_cert_pem;
	uint32_t     cert_len;
	char *       p_host;
	uint32_t     port;
} udi_server_t;

typedef struct
{
	udi_server_t s_server;
	const char * p_username;
	const char * p_password;
} udi_register_t;

/**
	@fn	    udi_get_from_server
	@brief	will communicate with the assigned server and get an UDI on the basis of the unique info of the hardware
	@param	p_server_info     : pointer to the structure udi_server_t. if p_host is null, p_host is set to hardware.wscada.net and port is set to 483
	@param	p_unique_info     :	pointer to the unique info of the hardware. it must be in ASCII format, null terminated
	@param	e_unique_info     : enum flag which will define whether the unique_info is mac or not
	@param	e_device_property :	enum which defines the property of the device
	@param	p_device_type     :	pointer to string which informs the server of the type of device, eg. artu, motionSensor, spillSensor etc.
	@param	p_udi             : pointer to array into which the udi will be copied if everything is done corretly.
								the array needs to be atleast 16 bytes, better 17 bytes. the null is not added by the function
	@return	returns 16 if the UDI is successfully received, 16 being the length of the UDI excluding null

	@see		udi_register_to_server
	@note		If the passed unique info is already used, Invalid will be returned
				in the data part of the server communication, which is not passed to the calling function. after getting the udi,
				it needs to be registered by calling udi_get_from_server. if that fails, the udi is not registered, so if this
				function is called again, a new UDI will be provided. once the udi is registered, future calls to this function
				with the same unique info will generate invalid string from the server.
	@eg			udi_get_from_server(&s_server, "126b459a556b", UNIQUE_IS_MAC, DEVICE_SUBSCRIBER_PUBLISHER, "DEVICE_PUBLISHER", &udi);
*/
esp_err_t udi_get_from_server(udi_server_t * p_server_info, const char * p_unique_info, device_unique_info_t e_unique_info, device_property_t e_device_property, const char * p_device_type, char * p_udi);

/**
	@fn		udi_register_to_server
	@brief	after receiving the udi from the function udi_get_from_server, it needs to passed to this function which will
			complete the registration of the udi and the device into the infrastructure software. the communication is mqtt
	@param	p_register_info	: pointer to the structure 'udi_register_t' which holds the required server information. if null
							  is passed for server_name, it is set to hardware.wscada.net and port is set to 8883
	@param	p_client_id     : used to connect to mqtt broker, needs to be unique, so better to choose a random number
	@param	p_pub_topic	    : the topic to which the udi needs to be published. if null is passed, default is chosen by the function
	@param	p_udi		    : the UDI as received from the function udi_get_from_server
	@return	ESP_OK if the registration is successful

	@see		udi_get_from_server
	@note		this needs to be called after getting the udi from the function udi_get_from_server. If this fails, the udi is
				not registered, so if the function udi_get_from_server is called, a udi will be provided. but once successful,
				the UDI will not be provided again, SO SAVE IT.
	@eg
*/
esp_err_t udi_register_to_server(udi_register_t * p_register_info, const char * p_client_id, const char * p_pub_topic, char * p_udi);

#endif /* __UDI_H__ */
