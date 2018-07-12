// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - WebServer
//
// File          platform.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//                         here: remove #define for httpd_printf()
// --------------------------------------------------------------------------

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#ifdef FREERTOS
   #include <libesphttpd/esp.h>

   #ifdef ESP_PLATFORM
      #include "freertos/FreeRTOS.h"
      #include "freertos/timers.h"
   #else
      #include "FreeRTOS.h"
      #include "timers.h"
   #endif

   // #include "esp_timer.h"
   typedef struct RtosConnType RtosConnType;
   typedef RtosConnType* ConnTypePtr;

   #ifdef ESP_PLATFORM
      // freertos v8 api
      typedef TimerHandle_t HttpdPlatTimerHandle;
   #else
      // freertos v7 api
      typedef xTimerHandle HttpdPlatTimerHandle;
   #endif

   #ifdef ESP_PLATFORM
      #define ICACHE_FLASH_ATTR
   #endif

#elif defined( linux )

   #include <unistd.h>
   #include <stdbool.h>
   typedef struct RtosConnType RtosConnType;
   typedef RtosConnType* ConnTypePtr;

   #define vTaskDelay( milliseconds ) usleep( (milliseconds ) * 1000 )
   #define portTICK_RATE_MS 1
   #define portTICK_PERIOD_MS 1

   typedef struct
   {
      timer_t timer;
      int timerPeriodMS;
      bool autoReload;
      void ( *callback )( void* arg );
      void* callbackArg;
   } HttpdPlatTimer;

   typedef HttpdPlatTimer* HttpdPlatTimerHandle;

   #define ICACHE_FLASH_ATTR
   #define ICACHE_RODATA_ATTR
   #define STORE_ATTR               __attribute__( ( aligned( 4 ) ) )

#else // no-os, map to os-specific versions that have to be defined

   typedef struct
   {
      os_timer_t timer;
      int timerPeriodMS;
      bool autoReload;
   } HttpdPlatTimer;

   #define printf( ... ) os_printf( __VA_ARGS__ )
   #define sprintf( str, ... ) os_sprintf( str, __VA_ARGS__ )
   #define strcpy( a, b ) os_strcpy( a, b )
   #define strncpy( a, b, c ) os_strncpy( a, b, c )
   #define strcmp( a, b ) os_strcmp( a, b )
   #define strncmp( a, b, c ) os_strncmp( a, b, c )
   #define malloc( x ) os_malloc( x )
   #define zalloc( x ) os_zalloc( x )
   #define free( x ) os_free( x )
   #define memset( x, a, b ) os_memset( x, a, b )
   #define memcpy( x, a, b ) os_memcpy( x, a, b )
   #define strcat( a, b ) os_strcat( a, b )
   #define strstr( a, b ) os_strstr( a, b )
   #define strlen( a ) os_strlen( a )
   #define memcmp( a, b, c ) os_memcmp( a, b, c )

   typedef struct espconn*       espConnTypePtr;

   typedef struct NonosConnType NonosConnType;
   typedef NonosConnType* ConnTypePtr;

   typedef HttpdPlatTimer* HttpdPlatTimerHandle;
#endif

#endif  // __PLATFORM_H__