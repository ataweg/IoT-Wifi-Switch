// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          device_control.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-08  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "device_control";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null> ": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>

#include <osapi.h>
#include "user_interface.h"

#include "device_control.h"
#include "device.h"     // devGet(), devSet();

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// template parameter & GET/POST parameter

const Config_Keyword_t ICACHE_RODATA_ATTR DeviceControl_Keywords[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { 0,          Number,     0, 0 }, "sys_led     "        },  // SysLed
   { { 1,          Number,     0, 0 }, "info_led"            },  // InfoLed
   { { 2,          Number,     0, 0 }, "switch_relay"        },  // Relay
   { { 2,          Number,     0, 0 }, "power_sense"         },  // PowerSense
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR device_get_control( int id, char **str, int *value );
static int ICACHE_FLASH_ATTR device_compare_control( int id, char *str, int value );
static int ICACHE_FLASH_ATTR device_apply_control( int id, char *str, int value );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR device_get_control( int id, char **str, int *value )
{
   ESP_LOGD( TAG, "device_get_control 0x%02x", id );

   *value = 0;
   *str = NULL;

   if( id >= 0 && id <= 3 )
   {
      switch( id )
      {
         case 0:  *value = devGet( SysLed );     return true;
         case 1:  *value = devGet( InfoLed );    return true;
         case 2:  *value = devGet( Relay );      return true;
         case 3:  *value = devGet( PowerSense ); return true;
      }
   }

   // id not handled here, nothing to compare
   return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// return
//    1       value doesn't change configuration
//    0       value  changed configuration
//   -1       id not handled here, nothing to compare

int ICACHE_FLASH_ATTR device_compare_control( int id, char *str, int value )
{
   ESP_LOGD( TAG, "device_compare_control 0x%02x %s %d", id, str, value );

   if( id >= 0 && id <= 3 )
   {
      switch( id )
      {
         case 0:  return devGet( SysLed )     == value ? 1 : 0;
         case 1:  return devGet( InfoLed )    == value ? 1 : 0;
         case 2:  return devGet( Relay )      == value ? 1 : 0;
         case 3:  return devGet( PowerSense ) == value ? 1 : 0;
      }
   }

   // id not handled here, nothing to update
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR device_apply_control( int id, char *str, int value )
{
   ESP_LOGD( TAG, "device_apply_control 0x%02x %s %d", id, S( str ), value );

   if( id >= 0 && id <= 3 )
   {
      switch( id )
      {
         case 0:  devSet( SysLed, value );     break;
         case 1:  devSet( InfoLed, value );    break;
         case 2:  devSet( Relay, value );      break;
         case 3:  devSet( PowerSense, value ); break;
      }

      return 1;
   }

   // id not handled here, nothing to update
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static Configuration_Item_t cfgItem;

Configuration_Item_t* ICACHE_FLASH_ATTR init_device_control( void )
{
   cfgItem.config_keywords = DeviceControl_Keywords;
   cfgItem.num_keywords    = sizeof( DeviceControl_Keywords ) / sizeof( Config_Keyword_t );
   cfgItem.defaults        = NULL;
   cfgItem.get_config      = device_get_control;
   cfgItem.compare_config  = device_compare_control;
   cfgItem.update_config   = NULL;
   cfgItem.apply_config    = device_apply_control;

   return &cfgItem;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
