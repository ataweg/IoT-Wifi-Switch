// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          mqtt_config.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __MQTT_CONFIG_H__
#define __MQTT_CONFIG_H__

#include "libesphttpd/httpd.h"      // CgiStatus
#include "configs.h"                // Configuration_Item_t

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// same values as defiend enum in _devices
// see include\user_config.h

typedef enum mqtt_devices
{
   // listen devices
   dev_Relay,
   dev_InfoLed,
   dev_SysLed,

   // device status
   dev_Button,
   dev_PowerSense,
   dev_Time,
   dev_Adc,

   num_devices
} mqtt_devices_t;

typedef struct
{
   char  Name[32];
   char  Server[64];
   char  Username[32];
   char  Password[64];
   char  Client_ID[32];
   int   Port;
   bool  Enable_SSL;
   bool  Self_Signed;
   int   Keep_Alive;
} MQTT_client_cfg_t;

typedef struct
{
   char Name[32];
   char Topic_sub[32];
   char Topic_pub[32];
   bool  Enable_publish;
   bool  Retained;
   uint8_t  QoS;
   uint8_t  dmy;
} MQTT_wifi_cfg_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern MQTT_wifi_cfg_t MQTT_Wifi_Cfg[ num_devices ];
extern MQTT_client_cfg_t MQTT_Client_Cfg;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Configuration_Item_t *init_mqtt_config( void );

CgiStatus ICACHE_FLASH_ATTR tplMqttConfig( HttpdConnData *connData, char *token, void **arg );

void ICACHE_FLASH_ATTR getMqttConfig( void );
void ICACHE_FLASH_ATTR getMqttClientConfig( void );

#endif // __MQTT_CONFIG_H__
