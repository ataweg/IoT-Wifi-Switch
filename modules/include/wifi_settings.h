// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          wifi_settings.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-11-29  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __WIFI_SETTINGS_H__
#define __WIFI_SETTINGS_H__

#ifdef ESP_PLATFORM
   #define ICACHE_RODATA_ATTR
#endif

#ifndef STORE_ATTR
   #define STORE_ATTR       __attribute__( ( aligned( 4 ) ) )
#endif

#include "configs.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum
{
   Wifi_Name         = 0x81,                //
   Wifi_Server       = 0x82,                // SERVER
   Wifi_Username     = 0x83,                // LOGINNAME
   Wifi_Password     = 0x84,                // PASSWORD
   Wifi_Port         = 0x85,                // PORT

   Wifi_First        = Wifi_Name,
   Wifi_Last         = Wifi_Port,

   Wlan_Ssid         = 0x91,                // USER_WIFI_SSID
   Wlan_Password     = 0x92,                // USER_WIFI_PASSWORD

   Wlan_List         = 0x91,
   Wlan_ListEnd      = Wlan_List + 8,
   Wlan_Known_AP     = 0x9F,

   Wlan_First        = Wlan_List,
   Wlan_Last         = Wlan_Known_AP,

   Ap_Ssid           = 0xA1,                // WIFI_AP_SSID
   Ap_Password       = 0xA2,                // WIFI_AP_PASSWORD
   Ap_Authmode       = 0xA3,
   Ap_Channel        = 0xA4,
   Ap_SsidHidden     = 0xA5,
   Ap_MaxConnection  = 0xA6,
   Ap_Bandwidth      = 0xA7,
   Ap_BeaconInterval = 0xA8,
   Ap_Address        = 0xA9,
   Ap_Gateway        = 0xAA,
   Ap_Netmask        = 0xAB,

   Ap_First          = Ap_Ssid,
   Ap_Last           = Ap_Netmask,

   Sta_Only          = 0xB1,
   Sta_PowerSave     = 0xB2,
   Sta_StaticIp      = 0xB3,
   Sta_Address       = 0xB4,
   Sta_Gateway       = 0xB5,
   Sta_Netmask       = 0xB6,
   Sta_Client_Name   = 0xB7,

   Sta_First         = Sta_Only,
   Sta_Last          = Sta_Client_Name,

   Dhcp_Enable       = 0xC1,
   Dhcp_Server       = 0xC2,                // USER_TCP_SERVER_IP
   Dhcp_Address      = 0xC3,
   Dhcp_Gateway      = 0xC4,
   Dhcp_Netmask      = 0xC5,

   Dhcp_First        = Dhcp_Enable,
   Dhcp_Last         = Dhcp_Netmask,

   End_Of_List       = 0xFF
} ;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern const_settings_t wifi_settings[] ICACHE_RODATA_ATTR STORE_ATTR;

void ICACHE_FLASH_ATTR config_print_wifi_defaults( void );

#endif //  __WIFI_SETTINGS_H__