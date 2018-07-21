// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          mqtt_settings.c
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

#ifndef ESP_PLATFORM
   #define _PRINT_CHATTY
   #define V_HEAP_INFO
#else
   #define HEAP_INFO( x )
#endif

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char* TAG = "mqtt_settings";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>

#include "os_platform.h"         // malloc(), ...
#include "configs.h"
#include "mqtt_settings.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define Default_0x11    "Switch"                // Name
#define Default_0x12    "/switch/relay"         // Topic_sub
#define Default_0x13    "/switch/stat/relay"    // Topic_pub
#define Default_0x14    "1"                     // Enable_publish
#define Default_0x15    "0"                     // Retained
#define Default_0x16    "0"                     // QoS

#define Default_0x21    "InfoLed"               // Name
#define Default_0x22    "/switch/infoled"       // Topic_sub
#define Default_0x23    "/switch/stat/infoled"  // Topic_pub
#define Default_0x24    "1"                     // Enable_publish
#define Default_0x25    "0"                     // Retained
#define Default_0x26    "0"                     // QoS

#define Default_0x31    "SysLed"                // Name
#define Default_0x32    "/switch/sysled"        // Topic_sub
#define Default_0x33    "/switch/stat/sysled"   // Topic_pub
#define Default_0x34    "1"                     // Enable_publish
#define Default_0x35    "0"                     // Retained
#define Default_0x36    "0"                     // QoS

#define Default_0x41    "Button"                // Name
#define Default_0x42    "/switch/button"        // Topic_sub
#define Default_0x43    "/switch/stat/button"   // Topic_pub
#define Default_0x44    "1"                     // Enable_publish
#define Default_0x45    "0"                     // Retained
#define Default_0x46    "0"                     // QoS

#define Default_0x51    "PowerSense"            // Name
#define Default_0x52    "/switch/powersense"    // Topic_sub
#define Default_0x53    "/switch/stat/powersense"// Topic_pub
#define Default_0x54    "1"                     // Enable_publish
#define Default_0x55    "0"                     // Retained
#define Default_0x56    "0"                     // QoS

#define Default_0x61    "Time"                  // Name
#define Default_0x62    "/switch/time"          // Topic_sub
#define Default_0x63    "/switch/stat/time"     // Topic_pub
#define Default_0x64    "1"                     // Enable_publish
#define Default_0x65    "0"                     // Retained
#define Default_0x66    "2"                     // QoS - exactly once

#define Default_0x71    "Adc"                   // Name
#define Default_0x72    "/switch/adc"           // Topic_sub
#define Default_0x73    "/switch/stat/adc"      // Topic_pub
#define Default_0x74    "1"                     // Enable_publish
#define Default_0x75    "0"                     // Retained
#define Default_0x76    "0"                     // QoS

#define Default_0xD1    "MQTT-Switch"          // MQTT_Name
#define Default_0xD2    "MQTT_Server"          // MQTT_Server            USER_DOMAIN
#define Default_0xD3    "MQTT_Username"        // MQTT_Username          MQTT_LOGINNAME
#define Default_0xD4    "MQTT_Password"         // MQTT_Password          MQTT_PASSWORD
#define Default_0xD5    "8883"                  // MQTT_Port              MQTT_PORT
#define Default_0xD6    "MQTT_CLIENT_ID"        // MQTT_Client_ID         MQTT_CLIENT_ID
#define Default_0xD7    "1"                     // MQTT_Enable_SSL
#define Default_0xD8    "1"                     // MQTT_Self_Signed
#define Default_0xD9    "120"                   // MQTT_Keep_Alive        MQTT_KEEPALIVE

#define Default_0xFF    ""                      // end of list

// --------------------------------------------------------------------------

const char String_0x11[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x11;  // Name
const char String_0x12[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x12;  // Topic_sub
const char String_0x13[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x13;  // Topic_pub
const char String_0x14[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x14;  // Enable_publish
const char String_0x15[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x15;  // Retained
const char String_0x16[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x16;  // QoS

const char String_0x21[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x21;  // Name
const char String_0x22[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x22;  // Topic_sub
const char String_0x23[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x23;  // Topic_pub
const char String_0x24[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x24;  // Enable_publish
const char String_0x25[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x25;  // Retained
const char String_0x26[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x26;  // QoS

const char String_0x31[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x31;  // Name
const char String_0x32[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x32;  // Topic_sub
const char String_0x33[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x33;  // Topic_pub
const char String_0x34[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x34;  // Enable_publish
const char String_0x35[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x35;  // Retained
const char String_0x36[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x36;  // QoS

const char String_0x41[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x41;  // Name
const char String_0x42[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x42;  // Topic_sub
const char String_0x43[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x43;  // Topic_pub
const char String_0x44[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x44;  // Enable_publish
const char String_0x45[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x45;  // Retained
const char String_0x46[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x46;  // QoS

const char String_0x51[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x51;  // Name
const char String_0x52[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x52;  // Topic_sub
const char String_0x53[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x53;  // Topic_pub
const char String_0x54[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x54;  // Enable_publish
const char String_0x55[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x55;  // Retained
const char String_0x56[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x56;  // QoS

const char String_0x61[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x61;  // Name
const char String_0x62[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x62;  // Topic_sub
const char String_0x63[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x63;  // Topic_pub
const char String_0x64[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x64;  // Enable_publish
const char String_0x65[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x65;  // Retained
const char String_0x66[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x66;  // QoS

const char String_0x71[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x71;  // Name
const char String_0x72[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x72;  // Topic_sub
const char String_0x73[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x73;  // Topic_pub
const char String_0x74[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x74;  // Enable_publish
const char String_0x75[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x75;  // Retained
const char String_0x76[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x76;  // QoS

const char String_0xD1[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD1;  // MQTT_Name
const char String_0xD2[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD2;  // MQTT_Server
const char String_0xD3[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD3;  // MQTT_Username
const char String_0xD4[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD4;  // MQTT_Password
const char String_0xD5[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD5;  // MQTT_Port
const char String_0xD6[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD6;  // MQTT_Client_ID
const char String_0xD7[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD7;  // MQTT_Enable_SSL
const char String_0xD8[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD8;  // MQTT_Self_Signed
const char String_0xD9[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0xD9;  // MQTT_Keep_Alive

// --------------------------------------------------------------------------

// 0xFF is the value after eraseing the flash, so when the value field is not 0xFF
// the whole record is invalid

const_settings_t mqtt_settings[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { { 0x11, Text,     sizeof( Default_0x11 ) - 1, 0xFF } }, String_0x11 }, // Name
   { { { 0x12, Text,     sizeof( Default_0x12 ) - 1, 0xFF } }, String_0x12 }, // Topic_sub
   { { { 0x13, Text,     sizeof( Default_0x13 ) - 1, 0xFF } }, String_0x13 }, // Topic_pub
   { { { 0x14, Flag,     sizeof( Default_0x14 ) - 1, 0xFF } }, String_0x14 }, // Enable_publish
   { { { 0x15, Flag,     sizeof( Default_0x15 ) - 1, 0xFF } }, String_0x15 }, // Retained
   { { { 0x16, Number,   sizeof( Default_0x16 ) - 1, 0xFF } }, String_0x16 }, // QoS

   { { { 0x21, Text,     sizeof( Default_0x21 ) - 1, 0xFF } }, String_0x21 }, // Name
   { { { 0x22, Text,     sizeof( Default_0x22 ) - 1, 0xFF } }, String_0x22 }, // Topic_sub
   { { { 0x23, Text,     sizeof( Default_0x23 ) - 1, 0xFF } }, String_0x23 }, // Topic_pub
   { { { 0x24, Flag,     sizeof( Default_0x24 ) - 1, 0xFF } }, String_0x24 }, // Enable_publish
   { { { 0x25, Flag,     sizeof( Default_0x25 ) - 1, 0xFF } }, String_0x25 }, // Retained
   { { { 0x26, Number,   sizeof( Default_0x26 ) - 1, 0xFF } }, String_0x26 }, // QoS

   { { { 0x31, Text,     sizeof( Default_0x31 ) - 1, 0xFF } }, String_0x31 }, // Name
   { { { 0x32, Text,     sizeof( Default_0x32 ) - 1, 0xFF } }, String_0x32 }, // Topic_sub
   { { { 0x33, Text,     sizeof( Default_0x33 ) - 1, 0xFF } }, String_0x33 }, // Topic_pub
   { { { 0x34, Flag,     sizeof( Default_0x34 ) - 1, 0xFF } }, String_0x34 }, // Enable_publish
   { { { 0x35, Flag,     sizeof( Default_0x35 ) - 1, 0xFF } }, String_0x35 }, // Retained
   { { { 0x36, Number,   sizeof( Default_0x36 ) - 1, 0xFF } }, String_0x36 }, // QoS

   { { { 0x41, Text,     sizeof( Default_0x41 ) - 1, 0xFF } }, String_0x41 }, // Name
   { { { 0x42, Text,     sizeof( Default_0x42 ) - 1, 0xFF } }, String_0x42 }, // Topic_sub
   { { { 0x43, Text,     sizeof( Default_0x43 ) - 1, 0xFF } }, String_0x43 }, // Topic_pub
   { { { 0x44, Flag,     sizeof( Default_0x44 ) - 1, 0xFF } }, String_0x44 }, // Enable_publish
   { { { 0x45, Flag,     sizeof( Default_0x45 ) - 1, 0xFF } }, String_0x45 }, // Retained
   { { { 0x46, Number,   sizeof( Default_0x46 ) - 1, 0xFF } }, String_0x46 }, // QoS

   { { { 0x51, Text,     sizeof( Default_0x51 ) - 1, 0xFF } }, String_0x51 }, // Name
   { { { 0x52, Text,     sizeof( Default_0x52 ) - 1, 0xFF } }, String_0x52 }, // Topic_sub
   { { { 0x53, Text,     sizeof( Default_0x53 ) - 1, 0xFF } }, String_0x53 }, // Topic_pub
   { { { 0x54, Flag,     sizeof( Default_0x54 ) - 1, 0xFF } }, String_0x54 }, // Enable_publish
   { { { 0x55, Flag,     sizeof( Default_0x55 ) - 1, 0xFF } }, String_0x55 }, // Retained
   { { { 0x56, Number,   sizeof( Default_0x56 ) - 1, 0xFF } }, String_0x56 }, // QoS

   { { { 0x61, Text,     sizeof( Default_0x61 ) - 1, 0xFF } }, String_0x61 }, // Name
   { { { 0x62, Text,     sizeof( Default_0x62 ) - 1, 0xFF } }, String_0x62 }, // Topic_sub
   { { { 0x63, Text,     sizeof( Default_0x63 ) - 1, 0xFF } }, String_0x63 }, // Topic_pub
   { { { 0x64, Flag,     sizeof( Default_0x64 ) - 1, 0xFF } }, String_0x64 }, // Enable_publish
   { { { 0x65, Flag,     sizeof( Default_0x65 ) - 1, 0xFF } }, String_0x65 }, // Retained
   { { { 0x66, Number,   sizeof( Default_0x66 ) - 1, 0xFF } }, String_0x66 }, // QoS

   { { { 0x71, Text,     sizeof( Default_0x71 ) - 1, 0xFF } }, String_0x71 }, // Name
   { { { 0x72, Text,     sizeof( Default_0x72 ) - 1, 0xFF } }, String_0x72 }, // Topic_sub
   { { { 0x73, Text,     sizeof( Default_0x73 ) - 1, 0xFF } }, String_0x73 }, // Topic_pub
   { { { 0x74, Flag,     sizeof( Default_0x74 ) - 1, 0xFF } }, String_0x74 }, // Enable_publish
   { { { 0x75, Flag,     sizeof( Default_0x75 ) - 1, 0xFF } }, String_0x75 }, // Retained
   { { { 0x76, Number,   sizeof( Default_0x76 ) - 1, 0xFF } }, String_0x76 }, // QoS

   { { { 0xD1, Text,     sizeof( Default_0xD1 ) - 1, 0xFF } }, String_0xD1 }, // MQTT_Name
   { { { 0xD2, Text,     sizeof( Default_0xD2 ) - 1, 0xFF } }, String_0xD2 }, // MQTT_Server
   { { { 0xD3, Text,     sizeof( Default_0xD3 ) - 1, 0xFF } }, String_0xD3 }, // MQTT_Username
   { { { 0xD4, Text,     sizeof( Default_0xD4 ) - 1, 0xFF } }, String_0xD4 }, // MQTT_Password
   { { { 0xD5, Number,   sizeof( Default_0xD5 ) - 1, 0xFF } }, String_0xD5 }, // MQTT_Port
   { { { 0xD6, Text,     sizeof( Default_0xD6 ) - 1, 0xFF } }, String_0xD6 }, // MQTT_Client_ID
   { { { 0xD7, Flag,     sizeof( Default_0xD7 ) - 1, 0xFF } }, String_0xD7 }, // MQTT_Enable_SSL
   { { { 0xD8, Flag,     sizeof( Default_0xD8 ) - 1, 0xFF } }, String_0xD8 }, // MQTT_Self_Signed
   { { { 0xD9, Number,   sizeof( Default_0xD9 ) - 1, 0xFF } }, String_0xD9 }, // MQTT_Keep_Alive

   { { { 0xFF, 0xFF, 0xFF, 0xFF } },  NULL }   // end of list
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR config_print_mqtt_defaults( void )
{
   ESP_LOGD( TAG, "config_print_defaults" );

   int i = 0;
   char default_str[64 + 1] __attribute__( ( aligned( 4 ) ) );  // on extra byte for terminating null character
   cfg_mode_t mode __attribute__( ( aligned( 4 ) ) );

   while( true )
   {
      ASSERT( "Src %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )( &mqtt_settings[ i ].mode ) & 3 ) == 0, "mqtt_settings[ i ].mode", ( uint32_t )( &mqtt_settings[ i ].mode ) );
      ASSERT( "Dest %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )( &mode ) & 3 ) == 0, "mode", ( uint32_t )( &mode ) );
      memcpy( &mode, &mqtt_settings[ i ].id, sizeof( mode ) );
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

      ASSERT( "Src %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )mqtt_settings[ i ].text & 3 ) == 0, "mqtt_settings[ i ].text", ( uint32_t )mqtt_settings[ i ].text );
      ASSERT( "Dest %s [0x%x08] isn't 32bit aligned", ( ( uint32_t )default_str & 3 ) == 0, "default_str", ( uint32_t )default_str );
      memcpy( default_str, mqtt_settings[ i ].text, len4 );
      default_str[ mode.len ] = 0; // terminate string
      printf( "id 0x%02x: %1d %3d \"%s\" %d\r\n", mode.id, mode.type, mode.len, default_str, len4 );
      i++;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
