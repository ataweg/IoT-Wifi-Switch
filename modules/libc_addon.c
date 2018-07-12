// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          libc_addon.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-08  AWe   add functions missing in the SDKs libc
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
static const char* TAG = "libc_addon";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <stdarg.h>

#include "esp_missing.h"  // os_snprintf(), ets_vsnprintf()

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// from F:\working\ESP8266_Arduino\cores\esp8266\libc_replacements.c

int ICACHE_FLASH_ATTR os_snprintf( char* buffer, size_t size, const char* format, ... )
{
   int ret;
   va_list arglist;
   va_start( arglist, format );
   ret = ets_vsnprintf( buffer, size, format, arglist );
   va_end( arglist );
   return ret;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

