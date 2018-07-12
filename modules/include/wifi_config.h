// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          wifi_config.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __WIFI_CONFIG_H__
#define __WIFI_CONFIG_H__

#include "libesphttpd/httpd.h"      // CgiStatus
#include "configs.h"                // Configuration_Item_t

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
   char     Name[32];            // 0x81
   char     Server[64];          // 0x82
   char     Username[32];        // 0x83
   char     Password[64];        // 0x84
   uint16_t Port;                // 0x85
} wifi_client_cfg_t;

typedef struct
{
   // Wlan_List                  // 0x91,
   uint32_t Wlan_Known_AP;       // 0x9F,
} wifi_wlan_cfg_t;

typedef struct
{
   char     Ssid[32];            // 0xA1     // WIFI_AP_SSID
   char     Password[64];        // 0xA2     // WIFI_AP_PASSWORD
   uint8_t  Authmode;            // 0xA3
   uint8_t  Channel;             // 0xA4
   uint8_t  SsidHidden;          // 0xA5     // Flag
   uint8_t  MaxConnection;       // 0xA6
   uint8_t  Bandwidth;           // 0xA7
   uint16_t BeaconInterval;      // 0xA8
   uint32_t Address;             // 0xA9
   uint32_t Gateway;             // 0xAB
   uint32_t Netmask;             // 0xAC
} wifi_ap_cfg_t;

typedef struct
{
   uint8_t  Only;                // 0xB1    // Flag
   uint8_t  PowerSave;           // 0xB2    // Flag
   uint8_t  StaticIp;            // 0xB3    // Flag
   uint32_t Address;             // 0xB4
   uint32_t Gateway;             // 0xB5
   uint32_t Netmask;             // 0xB6
   char     Client_Name[32];     // 0xB7
} wifi_sta_cfg_t;

typedef struct
{
   uint8_t  Enable;              // 0xC1    // Flag
   uint32_t Server;              // 0xC2    // USER_TCP_SERVER_IP
   uint32_t Address;             // 0xC3
   uint32_t Gateway;             // 0xC4
   uint32_t Netmask;             // 0xC5
} wifi_dhcp_cfg_t;

#define NUMARRAY_SIZE  16

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern wifi_client_cfg_t Wifi_Client_Cfg;
extern wifi_wlan_cfg_t   Wifi_Wlan_Cfg;
extern wifi_ap_cfg_t     Wifi_Ap_Cfg;
extern wifi_sta_cfg_t    Wifi_Sta_Cfg;
extern wifi_dhcp_cfg_t   Wifi_Dhcp_Cfg;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Configuration_Item_t * init_wifi_config( void );

CgiStatus ICACHE_FLASH_ATTR tplWifiConfig( HttpdConnData *connData, char *token, void **arg );

void ICACHE_FLASH_ATTR getClientConfig( void );
void ICACHE_FLASH_ATTR getSoftAPConfig( void );
void ICACHE_FLASH_ATTR getWlanConfig( void );
void ICACHE_FLASH_ATTR getStationConfig( void );
void ICACHE_FLASH_ATTR getDhcpConfig( void );

#endif // __WIFI_CONFIG_H__
