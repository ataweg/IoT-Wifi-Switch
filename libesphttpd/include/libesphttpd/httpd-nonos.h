// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - WebServer
//
// File          httpd-nonos.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-07-02  AWe   shutdown in not supported for nonos
//    2018-04-06  AWe   adept for use for ESP8266 with libesphttpd from chmorgan
//
// --------------------------------------------------------------------------

#pragma once

#include "httpd.h"

#ifdef CONFIG_ESPHTTPD_SSL_SUPPORT
   #include <openssl/ssl.h>
   #ifdef linux
      #include <openssl/err.h>
   #endif
#endif

#ifdef linux
   #include <netinet/in.h>
#endif

#ifndef FREERTOS

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

struct NonosConnType
{
   espConnTypePtr conn;      // The TCP connection. Exact type depends on the platform.
   int8_t slot;              // Slot ID -1 marks free slot  ( RTOS: fd )
   int remote_port;          // Remote TCP port             ( RTOS; port )
   uint8_t remote_ip[4];     // IP address of client        ( RTOS ip[4] )

   // int needWriteDoneNotif;
   // int needsClose;
#ifdef CONFIG_ESPHTTPD_SSL_SUPPORT
   SSL *ssl;
#endif

   // server connection data structure
   HttpdConnData connData;
};

// --------------------------------------------------------------------------

#define RECV_BUF_SIZE 2048

typedef struct
{
   ConnTypePtr *pConnList;

   int httpPort;
   HttpdFlags httpdFlags;

   bool isShutdown;

#ifdef CONFIG_ESPHTTPD_SSL_SUPPORT
   SSL_CTX *ctx;
#endif

   HttpdInstance httpdInstance;
} HttpdNonosInstance;

// --------------------------------------------------------------------------
// prototypes
// --------------------------------------------------------------------------

ConnTypePtr ICACHE_FLASH_ATTR get_pConn( HttpdConnData *connData );

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInitEx( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port, uint32_t listenAddress,
      int maxConnections, HttpdFlags flags );

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInit( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port,
      int maxConnections, HttpdFlags flags );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif  // ifndef FREERTOS
