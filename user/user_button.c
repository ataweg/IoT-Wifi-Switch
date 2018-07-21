// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_button.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-25  AWe   remove resetButtonTimer, function is handle by the
//                        new buttonVeryLongPress function
//    2017-11-25  AWe   move button related functions to  user_button.c
//    2017-11-15  AWe   do WPS on long press, switch on/off on short press
//                      add WPS related functions resetBtnTimerCb(),
//                        ioBtnTimerInit() from io.c
//    2017-08-19  AWe   change debug message printing
//    2017-08-07  AWe   initial implementation
//                      take over some wps stuff from
//                        ESP8266_Arduino\libraries\ESP8266WiFi\src\ESP8266WiFiSTA.cpp
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

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char* TAG = "user/user_button.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>

#include "user_button.h"
#include "user_wifi.h"        // wifiWpsStart(), wifiEnableReconnect()
#include "device.h"           // toggleRelay();
#include "keys.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define BUTTONS_NUM        1     // number of buttons
#define WPS_BUTTON         0

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static keys_param_t keys;
static single_key_param_t* single_key_list[ BUTTONS_NUM ];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR buttonShortPress( void )
{
   ESP_LOGD( TAG, "buttonShortPress" );

   int state = toggleRelay();
   ESP_LOGI( TAG, "Relay switched to %s", state ? "ON" : "OFF" );
   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR buttonLongPress( void )
{
   ESP_LOGD( TAG, "buttonLongPress" );
   wifiWpsStart();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR buttonVeryLongPress( void )
{
   ESP_LOGD( TAG, "buttonVeryLongPress" );
   // restart device
   wifiEnableReconnect( false );  // disable reconnect
   wifi_station_disconnect();
   wifi_set_opmode( STATIONAP_MODE ); // reset to AP+STA mode
   ESP_LOGI( TAG, "Reset to AP mode. Restarting system...\r\n\r\n" );
   system_restart();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR buttonInit( void )
{
   ESP_LOGD( TAG, "buttonInit" );

   single_key_list[ WPS_BUTTON ] = single_key_init( BUTTON_IO_NUM, BUTTON_IO_MUX, BUTTON_IO_FUNC,
                                    buttonVeryLongPress, 10000,
                                    buttonLongPress, 2000,
                                    buttonShortPress );

   keys_init( &keys, BUTTONS_NUM, single_key_list );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------