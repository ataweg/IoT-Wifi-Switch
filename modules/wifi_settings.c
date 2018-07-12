// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          wifi_settings.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-11-29  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "wifi_settings";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

#ifdef ESP_PLATFORM
   #ifndef ASSERT
     #define ASSERT( msg, expr, ...)  \
       do { if( !( expr  )) \
         ESP_LOGE( TAG, "ASSERT line: %d: "msg, __LINE__,  ##__VA_ARGS__  ); } while(0)
   #endif
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>

#include "user_config.h"
#include "configs.h"
#include "wifi_settings.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// see WifiConfig.tpl.html

#define Default_0x81    "MQTT-Switch"            // Wifi_Name
#define Default_0x82    "domain"              // Wifi_Server         USER_DOMAIN
#define Default_0x83    "username"              // Wifi_Username
#define Default_0x84    "password"              // Wifi_Password
#define Default_0x85    "80"                    // Wifi_Port

#define Default_0x91    "esp-wlan"              // Wlan_Ssid_0         USER_WIFI_SSID
#define Default_0x92    "1234567890"            // Wlan_Password_0     USER_WIFI_PASSWORD
#if 0  // no default settings for the known network list
   #define Default_0x95    ""                   // Wlan_Ssid_1
   #define Default_0x96    ""                   // Wlan_Password_1
   #define Default_0x95    ""                   // Wlan_Ssid_2
   #define Default_0x96    ""                   // Wlan_Password_2
   #define Default_0x97    ""                   // Wlan_Ssid_3
   #define Default_0x98    ""                   // Wlan_Password_3
#endif
#define Default_0x9F    "1"                     // Wlan_Last_ap

#define Default_0xA1    "esp32-ap"              // ap_ssid           WIFI_AP_SSID
#define Default_0xA2    "1234567890"            // ap_password       WIFI_AP_PASSWORD
#define Default_0xA3    "4"                     // ap_authmode       0: open
                                                //                   1: WEP
                                                //                   2: WPA_PSK
                                                //                   3: WPA2_PSK
                                                //                   4: WPA_WPA2_PSK
                                                //                   5: WPA2_ENTERPRISE
#define Default_0xA4    "8"                     // ap_channel
#define Default_0xA5    "0"                     // ap_ssid_hidden
#define Default_0xA6    "4"                     // ap_max_connection
#define Default_0xA7    "1"                     // ap_bandwidth      1: Bandwidth is HT20
                                                //                   2: Bandwidth is HT40
#define Default_0xA8    "100"                   // ap_beacon_interval
#define Default_0xA9    "192.168.4.1"           // ap_address
#define Default_0xAA    "192.168.4.1"           // ap_gateway
#define Default_0xAB    "255.255.255.0"         // ap_netmask

#define Default_0xB1    "0"                     // sta_only
#define Default_0xB2    "0"                     // sta_power_save
#define Default_0xB3    "0"                     // sta_static_ip
#define Default_0xB4    "192.168.0.10"          // sta_ip_addr
#define Default_0xB5    "192.168.0.1"           // sta_gw_addr
#define Default_0xB6    "255.255.255.0"         // sta_netmask
#define Default_0xB7    "Wifi 192.168.0.10"     // sta_client_name

#define Default_0xC1    "0"                     // dhcp_enable
#define Default_0xC2    "192.168.178.1"         // dhcp_server       USER_TCP_SERVER_IP
#define Default_0xC3    "192.168.0.10"          // dhcp_address
#define Default_0xC4    "192.168.0.1"           // dhcp_gateway
#define Default_0xC5    "255.255.255.0"         // dhcp_netmask
#define Default_0xC6    "Wifi-Relay-1F035A"     // dhcp_client_name

#define Default_0xFF    ""                      // end of list

// --------------------------------------------------------------------------

const char String_0x81[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x81;  // Wifi_Name
const char String_0x82[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x82;  // Wifi_Server
const char String_0x83[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x83;  // Wifi_Username
const char String_0x84[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x84;  // Wifi_Password
const char String_0x85[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x85;  // Wifi_Port

const char String_0x91[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x91;  // Wlan_Ssid_0
const char String_0x92[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x92;  // Wlan_Password_0
#if 0  // no default settings for the known network list
   const char String_0x93[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x93;  // Wlan_Ssid_1
   const char String_0x94[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x94;  // Wlan_Password_1
   const char String_0x95[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x95;  // Wlan_Ssid_2
   const char String_0x96[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x96;  // Wlan_Password_2
   const char String_0x97[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x97;  // Wlan_Ssid_3
   const char String_0x98[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x98;  // Wlan_Password_3
#endif
const char String_0x9F[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x9F;  // Wlan_Last_ap

const char String_0xA1[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA1;  // ap_ssid
const char String_0xA2[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA2;  // ap_password
const char String_0xA3[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA3;  // ap_authmode
const char String_0xA4[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA4;  // ap_channel
const char String_0xA5[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA5;  // ap_ssid_hidden
const char String_0xA6[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA6;  // ap_max_connection
const char String_0xA7[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA7;  // ap_bandwidth
const char String_0xA8[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA8;  // ap_beacon_interval
const char String_0xA9[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xA9;  // ap_address
const char String_0xAA[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xAA;  // ap_gateway
const char String_0xAB[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xAB;  // ap_netmask

const char String_0xB1[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB1;  // sta_only
const char String_0xB2[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB2;  // sta_power_save
const char String_0xB3[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB3;  // sta_static_ip
const char String_0xB4[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB4;  // sta_ip_addr
const char String_0xB5[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB5;  // sta_gw_addr
const char String_0xB6[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB6;  // sta_netmask
const char String_0xB7[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xB7;  // sta_client_name

const char String_0xC1[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xC1;  // dhcp_enable
const char String_0xC2[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xC2;  // dhcp_server
const char String_0xC3[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xC3;  // dhcp_address
const char String_0xC4[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xC4;  // dhcp_gateway
const char String_0xC5[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xC5;  // dhcp_netmask

// --------------------------------------------------------------------------

// 0xFF is the value after eraseing the flash, so when the value field is not 0xFF
// the whole record is invalid

const_settings_t wifi_settings[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { { 0x81, Text,     sizeof( Default_0x81 ) - 1, 0xFF } }, String_0x81 }, // Wifi_Name
   { { { 0x82, Text,     sizeof( Default_0x82 ) - 1, 0xFF } }, String_0x82 }, // Wifi_Server
   { { { 0x83, Text,     sizeof( Default_0x83 ) - 1, 0xFF } }, String_0x83 }, // Wifi_Username
   { { { 0x84, Text,     sizeof( Default_0x84 ) - 1, 0xFF } }, String_0x84 }, // Wifi_Password
   { { { 0x85, Number,   sizeof( Default_0x85 ) - 1, 0xFF } }, String_0x85 }, // Wifi_Port

   { { { 0x91, Text,     sizeof( Default_0x91 ) - 1, 0xFF } }, String_0x91 }, // Wlan_Ssid_0
   { { { 0x92, Text,     sizeof( Default_0x92 ) - 1, 0xFF } }, String_0x92 }, // Wlan_Password_0
#if 0  // no default settings for the known network list
   { { { 0x93, Text,     sizeof( Default_0x93 ) - 1, 0xFF } }, String_0x93 }, // Wlan_Ssid_1
   { { { 0x94, Text,     sizeof( Default_0x94 ) - 1, 0xFF } }, String_0x94 }, // Wlan_Password_1
   { { { 0x95, Text,     sizeof( Default_0x95 ) - 1, 0xFF } }, String_0x95 }, // Wlan_Ssid_2
   { { { 0x96, Text,     sizeof( Default_0x96 ) - 1, 0xFF } }, String_0x96 }, // Wlan_Password_2
   { { { 0x97, Text,     sizeof( Default_0x97 ) - 1, 0xFF } }, String_0x97 }, // Wlan_Ssid_3
   { { { 0x98, Text,     sizeof( Default_0x98 ) - 1, 0xFF } }, String_0x98 }, // Wlan_Password_3
#endif
   { { { 0x9F, Number,   sizeof( Default_0x9F ) - 1, 0xFF } }, String_0x9F }, // Wlan_Last_ap

   { { { 0xA1, Text,     sizeof( Default_0xA1 ) - 1, 0xFF } }, String_0xA1 }, // ap_ssid
   { { { 0xA2, Text,     sizeof( Default_0xA2 ) - 1, 0xFF } }, String_0xA2 }, // ap_password
   { { { 0xA3, Number,   sizeof( Default_0xA3 ) - 1, 0xFF } }, String_0xA3 }, // ap_authmode
   { { { 0xA4, Number,   sizeof( Default_0xA4 ) - 1, 0xFF } }, String_0xA4 }, // ap_channel
   { { { 0xA5, Flag,     sizeof( Default_0xA5 ) - 1, 0xFF } }, String_0xA5 }, // ap_ssid_hidden
   { { { 0xA6, Number,   sizeof( Default_0xA6 ) - 1, 0xFF } }, String_0xA6 }, // ap_max_connection
   { { { 0xA7, Number,   sizeof( Default_0xA7 ) - 1, 0xFF } }, String_0xA7 }, // ap_bandwidth
   { { { 0xA8, Number,   sizeof( Default_0xA8 ) - 1, 0xFF } }, String_0xA8 }, // ap_beacon_interval
   { { { 0xA9, Ip_Addr,  sizeof( Default_0xA9 ) - 1, 0xFF } }, String_0xA9 }, // ap_address
   { { { 0xAA, Ip_Addr,  sizeof( Default_0xAA ) - 1, 0xFF } }, String_0xAA }, // ap_gateway
   { { { 0xAB, Ip_Addr,  sizeof( Default_0xAB ) - 1, 0xFF } }, String_0xAB }, // ap_netmask

   { { { 0xB1, Flag,     sizeof( Default_0xB1 ) - 1, 0xFF } }, String_0xB1 }, // sta_only
   { { { 0xB2, Flag,     sizeof( Default_0xB2 ) - 1, 0xFF } }, String_0xB2 }, // sta_power_save
   { { { 0xB3, Flag,     sizeof( Default_0xB3 ) - 1, 0xFF } }, String_0xB3 }, // sta_static_ip
   { { { 0xB4, Ip_Addr,  sizeof( Default_0xB4 ) - 1, 0xFF } }, String_0xB4 }, // sta_ip_addr
   { { { 0xB5, Ip_Addr,  sizeof( Default_0xB5 ) - 1, 0xFF } }, String_0xB5 }, // sta_gw_addr
   { { { 0xB6, Ip_Addr,  sizeof( Default_0xB6 ) - 1, 0xFF } }, String_0xB6 }, // sta_netmask
   { { { 0xB7, Text,     sizeof( Default_0xB7 ) - 1, 0xFF } }, String_0xB7 }, // sta_client_name

   { { { 0xC1, Flag,     sizeof( Default_0xC1 ) - 1, 0xFF } }, String_0xC1 }, // dhcp_enable
   { { { 0xC2, Ip_Addr,  sizeof( Default_0xC2 ) - 1, 0xFF } }, String_0xC2 }, // dhcp_server
   { { { 0xC3, Ip_Addr,  sizeof( Default_0xC3 ) - 1, 0xFF } }, String_0xC3 }, // dhcp_address
   { { { 0xC4, Ip_Addr,  sizeof( Default_0xC4 ) - 1, 0xFF } }, String_0xC4 }, // dhcp_gateway
   { { { 0xC5, Ip_Addr,  sizeof( Default_0xC5 ) - 1, 0xFF } }, String_0xC5 }, // dhcp_netmask

   { { { 0xFF, 0xFF, 0xFF, 0xFF } },  NULL }   // end of list
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR config_print_wifi_defaults( void )
{
   ESP_LOGD( TAG, "config_print_defaults" );

   int i = 0;
   char default_str[64 + 1] __attribute__( ( aligned( 4 ) ) );  // on extra byte for terminating null character
   cfg_mode_t mode __attribute__( ( aligned( 4 ) ) );

   while( true )
   {
      ASSERT( "Src %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )( &wifi_settings[ i ].mode ) & 3 ) == 0, "wifi_settings[ i ].mode", ( uint32_t )( &wifi_settings[ i ].mode ) );
      ASSERT( "Dest %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )( &mode ) & 3 ) == 0, "mode", ( uint32_t )( &mode ) );
      os_memcpy( &mode, &wifi_settings[ i ].id, sizeof( mode ) );
      if( mode.id == 0xff )  // end of list
         break;

      if( mode.len > sizeof( default_str ) - 1 )
         mode.len = sizeof( default_str ) - 1;

      int len4 = ( mode.len + 3 ) & ~3;
      if( len4 > sizeof( default_str ) )
      {
         len4 = sizeof( default_str ) & ~3;
         mode.len = len4 - 1;
      }

      ASSERT( "Src %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )wifi_settings[ i ].text & 3 ) == 0, "wifi_settings[ i ].text", ( uint32_t )wifi_settings[ i ].text );
      ASSERT( "Dest %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )default_str & 3 ) == 0, "default_str", ( uint32_t )default_str );
      os_memcpy( default_str, wifi_settings[ i ].text, len4 );
      default_str[ mode.len ] = 0; // terminate string
      os_printf( "id 0x%02x: %1d %3d \"%s\" %d\r\n", mode.id, mode.type, mode.len, default_str, len4 );
      i++;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
