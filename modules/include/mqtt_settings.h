// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          mqtt_settings.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-11-29  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __MQTT_SETTINGS_H__
#define __MQTT_SETTINGS_H__

#include "configs.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum
{
   Dev_Name          = 0x01,
   Topic_Sub         = 0x02,
   Topic_Pub         = 0x03,
   Enable_Publish    = 0x04,
   Retained          = 0x05,
   QoS               = 0x06,
   Item_First        = Dev_Name,
   Item_Last         = QoS,

   Grp_Relay         = 0x10,
   Grp_InfoLed       = 0x20,
   Grp_SysLed        = 0x30,
   Grp_Button        = 0x40,
   Grp_PowerSense    = 0x50,
   Grp_Time          = 0x60,
   Grp_Adc           = 0x70,
   Grp_First         = Grp_Relay,
   Grp_Last          = Grp_Adc,

   Mqtt_Name         = 0xD1,                //
   Mqtt_Server       = 0xD2,                // SERVER
   Mqtt_Username     = 0xD3,                // MQTT_LOGINNAME
   Mqtt_Password     = 0xD4,                // MQTT_PASSWORD
   Mqtt_Port         = 0xD5,                // MQTT_PORT
   Mqtt_Client_Id    = 0xD6,                // MQTT_CLIENT_ID
   Mqtt_Enable_SSL   = 0xD7,                // DEFAULT_SECURITY
   Mqtt_Self_Signed  = 0xD8,  // not used
   Mqtt_Keep_Alive   = 0xD9,

   Mqtt_First        = Mqtt_Name,
   Mqtt_Last         = Mqtt_Keep_Alive,

   End_Of_List       = 0xFF
} ;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern const_settings_t mqtt_settings[] ICACHE_RODATA_ATTR STORE_ATTR;

void ICACHE_FLASH_ATTR config_print_mqtt_defaults( void );

#endif //  __MQTT_SETTINGS_H__