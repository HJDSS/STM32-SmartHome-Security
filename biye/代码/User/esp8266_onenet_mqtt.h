#ifndef __ESP8266_ONENET_MQTT_H
#define __ESP8266_ONENET_MQTT_H

#include "sys.h"

/*
 * OneNET MQTT（物模型）经 ESP8266 AT+MQTT 指令（ESP-AT 2.x / 安信可 MQTT AT 固件）。
 *
 * 重要约束：
 * - MQTT clientId / 物模型 topic 中的 deviceId 必须是可传输的 UTF-8 文本；强烈建议使用纯 ASCII，
 *   避免中文/空格等导致 AT+MQTTSUB / AT+MQTTPUBRAW 失败。
 * - ESP8266 AT 2.x 下单条 AT 长度有限（常用上限约 256 字节）；超长 token 可走 `AT+MQTTLONGPASSWORD`（代码保留该路径）。
 *
 * 若模块固件不含 MQTT AT，请将 ONENET_MQTT_ENABLE 置 0。
 */

#define ONENET_MQTT_ENABLE       1

#if ONENET_MQTT_ENABLE

/* OneNET 核心参数（按控制台配置填写） */
#define ONENET_PRODUCT_ID        "1TVZ0qTJOE"
/* 必须与 OneNET 控制台设备名一致（见你在资料里给出的设备 ID/名称） */
#define ONENET_DEVICE_NAME       "STM32zhinengjiaju"

#define ONENET_MQTT_USERNAME     ONENET_PRODUCT_ID

/*
 * OneNET MQTT 登录密码：使用 token（与资料中的 AT+MQTTUSERCFG 一致）。
 * 注意：token 含过期时间 et，过期后需在平台重新生成并替换此处字符串。
 */
#define ONENET_MQTT_PASSWORD \
	"version=2018-10-31&res=products%2F1TVZ0qTJOE%2Fdevices%2FSTM32zhinengjiaju&et=1805693871&method=md5&sign=w4rFG7b8I74rta35jHh6SA%3D%3D"

#define ONENET_MQTT_CLIENT_ID    ONENET_DEVICE_NAME

/*
 * OneNET MQTT 接入点（与资料一致）：
 * - 域名使用 mqtts.heclouds.com，端口 1883，scheme=1（MQTT over TCP，非 TLS 端口）
 */
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

/* 非阻塞 MQTT 建链（与 Connect 等价，按主循环轮询推进） */
void ESP8266_OneNET_MqttFsm_Reset(void);
u8 ESP8266_OneNET_MqttFsm_Poll(void);

/* 供 OneNET_Publish_Data：AT+MQTTPUBRAW 发送 UTF-8 JSON */
u8 OneNET_AT_Mqtt_PublishRaw(const char *topic, const char *payload, u16 len);

#endif /* __ESP8266_ONENET_MQTT_H */
