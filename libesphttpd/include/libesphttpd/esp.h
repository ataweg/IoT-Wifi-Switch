// Combined include file for esp8266 and esp32

#ifdef ESP_PLATFORM // only set in esp-idf
   #define FREERTOS 1
   #define ESP32 1

   #include "sdkconfig.h"
   #define HTTPD_MAX_CONNECTIONS      CONFIG_ESPHTTPD_MAX_CONNECTIONS
   #define HTTPD_STACKSIZE            CONFIG_ESPHTTPD_STACK_SIZE
   #include "stdint.h"
   typedef uint8_t  uint8;
   typedef uint16_t uint16;
   typedef uint32_t uint32;
   typedef int8_t   int8;
   typedef int16_t  int16;
   typedef int32_t  int32;

   #define ICACHE_FLASH_ATTR
   #define ICACHE_RODATA_ATTR
   #define STORE_ATTR               __attribute__( ( aligned( 4 ) ) )
#endif


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef linux
   #ifdef FREERTOS
      #include <stdint.h>
      #ifdef ESP_PLATFORM
         #include "esp_types.h"
         #include "esp_attr.h"
         #include "esp_spi_flash.h"
      #else
         #include <espressif/esp_common.h>
      #endif
   #else
      #include <c_types.h>
      #include <ip_addr.h>    // IP2STR, IPSTR, IP4_ADDR()
      #include <espconn.h>
      #include <ets_sys.h>
      #include <gpio.h>
      #include <mem.h>
      #include <osapi.h>
      #include <user_interface.h>
      #include <upgrade.h>
   #endif
#endif // #ifndef linux

#include "platform.h"

#ifndef linux
   int  os_snprintf( char *str, size_t size, const char *format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#endif
