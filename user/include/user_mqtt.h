// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_mqtt.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   add mqttWifiIsConnected(), mqttWifiDisconnect()
//                      change mqttWifiIsConnect() 
//    2017-08-10  AWe   get stuff from
//                         DevKit-Examples\ESP8266\esp_mqtt\user\user_main.c
//
// --------------------------------------------------------------------------


#ifndef __USER_MQTT_H__
#define __USER_MQTT_H__

#include "mqtt.h"
#include "mqtt_config.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern MQTT_Client mqttClient;

extern MQTT_client_cfg_t MQTT_Client_Cfg;
extern MQTT_wifi_cfg_t MQTT_Wifi_Cfg[7];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR mqttInit( void );
void ICACHE_FLASH_ATTR mqttSubscribe( MQTT_Client* client, enum mqtt_devices device );
void ICACHE_FLASH_ATTR mqttPublish( MQTT_Client* client, enum mqtt_devices device, char *value );
int  ICACHE_FLASH_ATTR mqttWifiIsConnected( void );
void ICACHE_FLASH_ATTR mqttWifiConnect( void );
void ICACHE_FLASH_ATTR mqttWifiDisconnect( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __USER_MQTT_H__
