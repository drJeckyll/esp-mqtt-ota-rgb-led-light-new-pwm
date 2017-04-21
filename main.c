#include <c_types.h>
#include <ets_sys.h>
#include <osapi.h>
#include <gpio.h>
#include <os_type.h>
#include <user_interface.h>
#include <mem.h>

#include "main.h"
#include "config.h"
#include "uart.h"
#include "wifi.h"
#include "mqtt.h"
#include "pwm.h"
#include "ota.h"
#include "debug.h"

MQTT_Client mqttClient;

void _pwm_stop()
{
	pwm_set_duty(0, LIGHT_RED);
	pwm_set_duty(0, LIGHT_GREEN);
	pwm_set_duty(0, LIGHT_BLUE);
	pwm_set_duty(0, LIGHT_COLD_WHITE);
	pwm_set_duty(0, LIGHT_WARM_WHITE);
	pwm_start();
}

void _pwm_start()
{
	pwm_set_period(sysCfg.pwm_period);
	pwm_start();
}

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

	MQTT_Subscribe(client, MQTT_TOPIC_POWER, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_PERIOD, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_ALL, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_ALL_COLORS, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_ALL_WHITE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_RED, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_GREEN, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_BLUE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_COLD_WHITE, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_WARM_WHITE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_SETTINGS, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_UPDATE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_RESTART, 0);

	mqttSendSettings(args);
}

void mqttSendSettings(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;

	char buff[100] = "";

	os_sprintf(buff, "{\"power\":%d,\"r\":%d,\"g\":%d,\"b\":%d,\"cw\":%d,\"ww\":%d,\"p\":%d,\"pwm\":%d,\"rom\":%d,\"device\":\"%s\"}",
		sysCfg.power,
		sysCfg.pwm_duty[LIGHT_RED],
		sysCfg.pwm_duty[LIGHT_GREEN],
		sysCfg.pwm_duty[LIGHT_BLUE],
		sysCfg.pwm_duty[LIGHT_COLD_WHITE],
		sysCfg.pwm_duty[LIGHT_WARM_WHITE],
		sysCfg.pwm_period,
		sysCfg.pwm_on,
		rboot_get_current_rom(),
		DEVICE
	);
	MQTT_Publish(client, MQTT_TOPIC_SETTINGS_REPLY, buff, os_strlen(buff), 0, 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len + 1),
	     *dataBuf  = (char*)os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s\r\n", topicBuf, dataBuf);

	if (os_strcmp(topicBuf, MQTT_TOPIC_POWER) == 0)
	{
		if (atoi(dataBuf) == 0)
		{
			sysCfg.power = 0;
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
		} else {
			sysCfg.power = 1;

			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_PERIOD) == 0)
	{
		sysCfg.pwm_period = atoi(dataBuf);
		if ((sysCfg.pwm_period < 0) || (sysCfg.pwm_period > PWM_PERIOD)) sysCfg.pwm_period = PWM_PERIOD;
		_pwm_start();
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL) == 0)
	{
		sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);
		if (sysCfg.power)
		{
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL_COLORS) == 0)
	{
		sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		if (sysCfg.power)
		{
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL_WHITE) == 0)
	{
		sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);
		if (sysCfg.power)
		{
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (
		(os_strcmp(topicBuf, MQTT_TOPIC_RED) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_GREEN) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_BLUE) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_COLD_WHITE) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_WARM_WHITE) == 0)
	)
	{
		if (os_strcmp(topicBuf, MQTT_TOPIC_RED) == 0) sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_GREEN) == 0) sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_BLUE) == 0) sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_COLD_WHITE) == 0) sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_WARM_WHITE) == 0) sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);

		if (sysCfg.power)
		{
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}

		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_SETTINGS) == 0)
	{
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_UPDATE) == 0)
	{
		OtaUpdate();
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_RESTART) == 0)
	{
		system_restart();
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR
user_light_init(void)
{
	// reset everything
	if ((sysCfg.pwm_period < 0) || (sysCfg.pwm_period > PWM_PERIOD)) sysCfg.pwm_period = PWM_PERIOD;
	_pwm_stop();
	GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
	GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
	GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
	GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
	GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);

	if (sysCfg.power)
	{
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
		_pwm_start();
	} else {
		_pwm_stop();
		GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
		GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
		GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
		GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
		GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
	}

	INFO("LIGHT PARAM: R: %d\r\n", sysCfg.pwm_duty[LIGHT_RED]);
	INFO("LIGHT PARAM: G: %d\r\n", sysCfg.pwm_duty[LIGHT_GREEN]);
	INFO("LIGHT PARAM: B: %d\r\n", sysCfg.pwm_duty[LIGHT_BLUE]);
	INFO("LIGHT PARAM: CW: %d\r\n", sysCfg.pwm_duty[LIGHT_COLD_WHITE]);
	INFO("LIGHT PARAM: WW: %d\r\n", sysCfg.pwm_duty[LIGHT_WARM_WHITE]);
	INFO("LIGHT PARAM: P: %d\r\n", sysCfg.pwm_period);
}

void ICACHE_FLASH_ATTR
user_init()
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000000);

	INFO("Starting up...\r\n");

	INFO("Loading config...\r\n");
	CFG_Load();

	gpio_init();

	INFO("Initializing MQTT...\r\n");
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	INFO("Connect to WIFI...\r\n");
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("Init PWM...\r\n");
	uint32 io_info[][3] = {
		{PWM_0_OUT_IO_MUX, PWM_0_OUT_IO_FUNC ,PWM_0_OUT_IO_NUM},
		{PWM_1_OUT_IO_MUX, PWM_1_OUT_IO_FUNC, PWM_1_OUT_IO_NUM},
		{PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC, PWM_2_OUT_IO_NUM},
		{PWM_3_OUT_IO_MUX, PWM_3_OUT_IO_FUNC, PWM_3_OUT_IO_NUM},
		{PWM_4_OUT_IO_MUX, PWM_4_OUT_IO_FUNC, PWM_4_OUT_IO_NUM},
	};

	// PIN FUNCTION INIT FOR PWM OUTPUT
	pwm_init(sysCfg.pwm_period, sysCfg.pwm_duty, PWM_CHANNEL, io_info);

	set_pwm_debug_en(0); //disable debug print in pwm driver
	INFO("PWM version : %08x\r\n", get_pwm_version());

	pwm_set_period(sysCfg.pwm_period);
	pwm_start();

	INFO("Init light...\r\n");
	user_light_init();

	INFO("Startup completed. Now running from rom %d...\r\n", rboot_get_current_rom());
}
