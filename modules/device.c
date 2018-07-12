// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          device.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-13  AWe   send the new state of the device to the mqtt server
//    2017-11-20  AWe   add here parts from io.c
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
static const char* TAG = "device";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>

#include "user_config.h"
#include "io.h"
#include "leds.h"
#include "device.h"        // switchRelay()
#include "user_mqtt.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int relay_state = 0;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR devSet( enum _devices dev, int val )
{
   ESP_LOGD( TAG, "devSet %d : 0x%02X", dev, val );

   switch( dev )
   {
      case Relay:
         switchRelay( val );
         break;

      case InfoLed:
         setInfoLed( val );
         break;

      case SysLed:
         setSysLed( val );
         break;

      default:
         return 0;  // don't send new state to mqrr server
   }

   return val;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR devGet( enum _devices dev )
{
   switch( dev )
   {
      case Relay:
         return relay_state;

      case InfoLed:
         return !io_input_get( pin_InfoLed );

      case SysLed:
         return !io_input_get( pin_SysLed );

      // device status
      case Button:
         return !io_input_get( pin_Button );

      case PowerSense:
#if defined( ESP201 ) || defined( NODEMCU )  // only these devices support power sensing
         return !io_input_get( pin_PowerSense );
#else
         return relay_state;
#endif
      case Adc:
         return system_adc_read();

      case Time:
         return -1;

      default:
         return -1;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// NODEMCU: relay coils are connected to GND

int ICACHE_FLASH_ATTR switchRelay( int val )
{
   ESP_LOGI( TAG, "switchRelay 0x%02X", val );

   if( val == 0 )
   {
      // switch off
      io_output_set( pin_RelayCoil_1, 0 );
      io_output_set( pin_RelayCoil_2, 1 );
   }
   else
   {
      // switch on
      io_output_set( pin_RelayCoil_1, 1 );
      io_output_set( pin_RelayCoil_2, 0 );
   }
   relay_state = val;
   os_delay_us( 10000 );  // 10 ms
   io_output_set( pin_RelayCoil_1, 0 );
   io_output_set( pin_RelayCoil_2, 0 );

   return relay_state;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR toggleRelay( void )
{
   return switchRelay( !devGet( Relay ) );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
