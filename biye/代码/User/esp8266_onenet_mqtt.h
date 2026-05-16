#ifndef __ESP8266_ONENET_MQTT_H
#define __ESP8266_ONENET_MQTT_H

#include "sys.h"

#define ONENET_MQTT_ENABLE       1

#if ONENET_MQTT_ENABLE

#define ONENET_PRODUCT_ID        "1TVZ0qTJOE"
#define ONENET_DEVICE_NAME       "STM32zhinengjiaju"

#define ONENET_MQTT_USERNAME     ONENET_PRODUCT_ID

#define ONENET_MQTT_PASSWORD \
    "version=2018-10-31&res=products%2F1TVZ0qTJOE%2Fdevices%2FSTM32zhinengjiaju&et=1805693871&method=md5&sign=w4rFG7b8I74rta35jHh6SA%3D%3D"

#define ONENET_MQTT_CLIENT_ID    ONENET_DEVICE_NAME

#ifndef ONENET_MQTT_AT_SCHEME
#define ONENET_MQTT_AT_SCHEME      1
#endif
#ifndef ONENET_MQTT_BROKER
#define ONENET_MQTT_BROKER         "mqtts.heclouds.com"
#endif
#ifndef ONENET_MQTT_PORT
#define ONENET_MQTT_PORT           1883
#endif
#ifndef ONENET_MQTT_AUTO_RECONNECT
#define ONENET_MQTT_AUTO_RECONNECT 1
#endif

#endif /* ONENET_MQTT_ENABLE */

u8 ESP8266_OneNET_Mqtt_Connect(void);

void ESP8266_OneNET_MqttFsm_Reset(void);
u8 ESP8266_OneNET_MqttFsm_Poll(void);

extern volatile u8 g_mqtt_substage;
extern volatile u8 g_mqtt_err;

u8 OneNET_AT_Mqtt_PublishRaw(const char *topic, const char *payload, u16 len);

#endif /* __ESP8266_ONENET_MQTT_H */
