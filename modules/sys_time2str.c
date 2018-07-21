// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          sys_time2str.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-26  AWe   remove us, to be compatible with esp-idf
//    2018-06-26  AWe   initial implementation, used in esp_log.h
//
// --------------------------------------------------------------------------

#ifdef ESP_PLATFORM
   #include <stdio.h>
   #include "esp_types.h"

   #define ICACHE_FLASH_ATTR
#else
   #include <osapi.h>
   #define sprintf   os_sprintf
#endif

#include <stdarg.h>

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// sys_time is System time in millisecond
// 32 bit, max value 4294967295  (10 digita)
// 4294967295 => 4294.967.295
// hh:mm:ss.ms

static ICACHE_FLASH_ATTR void div_mod( uint32_t divident, uint32_t divisor, uint32_t *result, uint32_t *remainder )
{
   *result = divident / divisor;
   *remainder = divident % divisor;
}

char* ICACHE_FLASH_ATTR sys_time2str( uint32_t sys_time )
{
   static char buf[ 16 + 1 ];
   uint32_t hh, mm, ss, ms, rest;

   div_mod( sys_time, ( 60UL * 60 * 1000 ), &hh, &rest );
   div_mod( rest,     ( 60 * 1000 ),        &mm, &rest );
   div_mod( rest,     ( 1000 ),             &ss, &ms );

   sprintf( buf, "%d:%02d:%02d.%03d", hh, mm, ss, ms );

   return buf;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
