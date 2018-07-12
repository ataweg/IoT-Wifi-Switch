// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - WebServer
//
// File          httpd-nonos.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-19  AWe   for priv buffer and sendData buffer allocate memory from
//                        heap when needed. Give up to have buffers in the memory space.
//    2018-04-19  awe   httpdConnectCb changed, can now fail with out of memory
//    2018-04-12  AWe   httpdPlatSendData() returns the length on success, otherwise zero
//    2018-04-06  AWe   fix prototype for httpdPlatLock(), httpdPlatUnLock()
//    2018-04-06  AWe      adept for use for ESP8266 with libesphttpd from chmorgan
//    2018-02-02  AWe   take over changes from MightyPork/libesphttpd
//                         https://github.com/MightyPork/libesphttpd/commit/7fce9474395e208c83325ff6150fdd21ba16c9a4
//                         add const to some places
//    2018-01-19  AWe   take over changes from MightyPork/libesphttpd
//                         https://github.com/MightyPork/libesphttpd/commit/d70903bd48a1eed8d7d24dc47990ac77baea83dc
//                         make timeout configurable
// --------------------------------------------------------------------------

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
ESP8266 web server - platform-dependent routines, nonos version
*/

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#ifndef ESP_PLATFORM
   #define _PRINT_CHATTY
   #define V_HEAP_INFO
#else
   #define HEAP_INFO( x )
#endif

#define LOG_LOCAL_LEVEL    ESP_LOG_WARN
static const char *TAG = "httpd-nonos";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// #define NO_DBG_MEMLEAKS

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <libesphttpd/esp.h>
#include "libesphttpd/httpd.h"
#include "httpd-platform.h"
#include "libesphttpd/httpd-nonos.h"

#ifndef FREERTOS

#ifndef HTTPD_CONN_TIMEOUT
   #define HTTPD_CONN_TIMEOUT 2
#endif

static HttpdNonosInstance *pHttpdNonosInstance = NULL;
static HttpdInstance     *pHttpdInstance = NULL;

// --------------------------------------------------------------------------
// prototypes
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR platReconCb( void *arg, sint8 err );
static void ICACHE_FLASH_ATTR platDisconCb( void *arg );
static void ICACHE_FLASH_ATTR platRecvCb( void *arg, char *data, unsigned short len );
static void ICACHE_FLASH_ATTR platSentCb( void *arg );
static void ICACHE_FLASH_ATTR platConnCb( void *arg );

// see also libesphttpd\include\libesphttpd\httpd-nonos.h

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInitEx( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port, uint32_t listenAddress,
      int maxConnections, HttpdFlags flags );

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInit( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port,
      int maxConnections, HttpdFlags flags );

// see also libesphttpd\core\httpd-platform.h

int  ICACHE_FLASH_ATTR httpdPlatSendData( HttpdInstance *pInstance, HttpdConnData *connData, char *buf, int len );

void ICACHE_FLASH_ATTR httpdPlatDisconnect( HttpdConnData *connData );
void ICACHE_FLASH_ATTR httpdPlatDisableTimeout( HttpdConnData *connData );

void ICACHE_FLASH_ATTR httpdPlatLock( HttpdInstance *pInstance );
void ICACHE_FLASH_ATTR httpdPlatUnlock( HttpdInstance *pInstance );

HttpdPlatTimerHandle ICACHE_FLASH_ATTR httpdPlatTimerCreate( const char *name, int periodMs, int autoreload, void ( *callback )( void *arg ), void *ctx );
void ICACHE_FLASH_ATTR httpdPlatTimerStart( HttpdPlatTimerHandle timer );
void ICACHE_FLASH_ATTR httpdPlatTimerStop( HttpdPlatTimerHandle timer );
void ICACHE_FLASH_ATTR httpdPlatTimerDelete( HttpdPlatTimerHandle timer );

#ifdef ESPHTTPD_SHUTDOWN_SUPPORT
void ICACHE_FLASH_ATTR httpdPlatShutdown( HttpdInstance *pInstance );
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// get the base address of structure "instance" with the type "HttpdNonosInstance"
#define fr_of_instance( instance ) esp_container_of( instance, HttpdNonosInstance, httpdInstance )
#define frconn_of_conn( conn ) esp_container_of( conn, NonosConnType, connData )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

ConnTypePtr ICACHE_FLASH_ATTR get_pConn( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "get_pConn ..." );

   return frconn_of_conn( connData );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Looks up the connectionBuffer info for a specific connection
// have to go thru pInstance->pConnList = connectionBuffer;
// search for connection with remPort and remIp

static ConnTypePtr ICACHE_FLASH_ATTR httpdFindConn( espConnTypePtr conn, char *remIp, int remPort )
{
   // ESP_LOGD( TAG, "httpdFindConn for " IPSTR ":%d", IP2STR( remIp ), remPort );

   int maxConnections = pHttpdNonosInstance->httpdInstance.maxConnections;
   ConnTypePtr *pConnList = pHttpdNonosInstance->pConnList;

   for( int i = 0; i < maxConnections; i++ )
   {
      // ESP_LOGD( TAG, "*** %d connection " IPSTR ":%d", i, IP2STR( ( uint8_t* )pConnList[ i ]->remote_ip ), pConnList[i]->remote_port );
      if( pConnList != NULL && pConnList[ i ] != NULL && pConnList[ i ]->remote_port == remPort &&
            memcmp( pConnList[ i ]->remote_ip, remIp, 4 ) == 0 )
      {
         pConnList[ i ]->conn = conn;
         return pConnList[ i ];
      }
   }
   // Shouldn't happen.
   ESP_LOGE( TAG, "*** Unknown connection %d.%d.%d.%d:%d\n", remIp[0] & 0xff, remIp[1] & 0xff, remIp[2] & 0xff, remIp[3] & 0xff, remPort );
   espconn_disconnect( conn );
   return NULL;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This callback is entered when an error occurs, TCP connection broken.

// CallbackStatus httpdDisconCb( HttpdInstance *pInstance, HttpdConnData *connData );
// see C:\Espressif\ESP8266_NONOS_SDK\include\espconn.h( 104 ):

static void ICACHE_FLASH_ATTR platReconCb( void *arg, sint8 err )
{
   ESP_LOGD( TAG, "platReconCb ..." );
   HEAP_INFO( "" );

   // From ESP8266 SDK
   // If still no response, considers it as TCP connection broke, goes into espconn_reconnect_callback.

   espConnTypePtr conn = arg;
   ESP_LOGI( TAG, "Reconn from "IPSTR":%d,", IP2STR( ( uint8_t* )conn->proto.tcp->remote_ip ),
                                             conn->proto.tcp->remote_port );
   ConnTypePtr pConn = httpdFindConn( conn, ( char* )conn->proto.tcp->remote_ip,
                                                     conn->proto.tcp->remote_port );
   // Just call disconnect to clean up pool and close connection.
   if( pConn != NULL )
   {
      httpdDisconCb( pHttpdInstance, &pConn->connData );

      // mark slot as free/unused
      ESP_LOGD( TAG, "mark slot %d as free/unused", pConn->slot );
      pConn->slot = -1;
      pConn->conn = NULL;
   }
   HEAP_INFO( "" );
}

static void ICACHE_FLASH_ATTR platDisconCb( void *arg )
{
   ESP_LOGD( TAG, "platDisconCb ..." );
   HEAP_INFO( "" );

   espConnTypePtr conn = arg;
   ESP_LOGI( TAG, "Disconn from "IPSTR":%d,", IP2STR( ( uint8_t* )conn->proto.tcp->remote_ip ),
                                              conn->proto.tcp->remote_port );
   ConnTypePtr pConn = httpdFindConn( conn, ( char* )conn->proto.tcp->remote_ip,
                                                     conn->proto.tcp->remote_port );
   if( pConn != NULL )
   {
      httpdDisconCb( pHttpdInstance, &pConn->connData );

      // mark slot as free/unused
      ESP_LOGD( TAG, "mark slot %d as free/unused", pConn->slot );
      pConn->slot = -1;
      pConn->conn = NULL;
   }
   HEAP_INFO( "" );
}

static void ICACHE_FLASH_ATTR platRecvCb( void *arg, char *data, unsigned short len )
{
   ESP_LOGD( TAG, "platRecvCb ..." );
   HEAP_INFO( "" );

   espConnTypePtr conn = arg;
   ESP_LOGI( TAG, "Recv from "IPSTR":%d,", IP2STR( ( uint8_t* )conn->proto.tcp->remote_ip ),
                                           conn->proto.tcp->remote_port );
   ConnTypePtr pConn = httpdFindConn( conn, ( char* )conn->proto.tcp->remote_ip,
                                                     conn->proto.tcp->remote_port );
   if( pConn != NULL )
   {
      if( CallbackSuccess != httpdRecvCb( pHttpdInstance, &pConn->connData, data, len ) )
      {
         // disconnect the case of a memory allocation error
         ESP_LOGW( TAG, "close connection because out of memory" );
         espconn_disconnect( conn );
      }
   }
   HEAP_INFO( "" );
}

static void ICACHE_FLASH_ATTR platSentCb( void *arg )
{
   ESP_LOGD( TAG, "platSentCb ..." );
   HEAP_INFO( "" );

   espConnTypePtr conn = arg;
   // ESP_LOGI( TAG, "Sent to "IPSTR":%d,", IP2STR( ( uint8_t* )conn->proto.tcp->remote_ip ),
   //                                       conn->proto.tcp->remote_port );
   ConnTypePtr pConn = httpdFindConn( conn, ( char* )conn->proto.tcp->remote_ip,
                                                     conn->proto.tcp->remote_port );
   if( pConn != NULL )
   {
      if( CallbackSuccess != httpdSentCb( pHttpdInstance, &pConn->connData ) )
      {
         // close connection in the case of a memory allocation error
         ESP_LOGW( TAG, "close connection because out of memory" );
         espconn_disconnect( pConn->conn );
      }
   }
   HEAP_INFO( "" );
}

// callback which will be called on successful TCP connection.
// see ...\source\libesphttpd\core\httpd.c( 1137 ):

static void ICACHE_FLASH_ATTR platConnCb( void *arg )
{
   ESP_LOGD( TAG, "platConnCb ..." );
   HEAP_INFO( "" );

   espConnTypePtr conn = arg;
   ESP_LOGI( TAG, "Conn req from "IPSTR":%d,", IP2STR( ( uint8_t* )conn->proto.tcp->remote_ip ),
                                               conn->proto.tcp->remote_port );
   int maxConnections = pHttpdNonosInstance->httpdInstance.maxConnections;
   ConnTypePtr *pConnList = pHttpdNonosInstance->pConnList;

   httpdPlatLock( pHttpdInstance );
   // Find empty conndata in pConnList
   int i = 0;
   int rc = 0;
   for( ; i < maxConnections; i++ )
   {
      if( pConnList[i] == NULL )
      {
         ESP_LOGD( TAG, "pConnList[%d] = NULL", i );
      }
      else
      {
         ESP_LOGD( TAG, "pConnList[%d].slot = %d", i, pConnList[i]->slot );
      }

      if( pConnList[i] == NULL || pConnList[i]->slot == -1 )
         break;
   }

   if( i < maxConnections )
   {
      if( pConnList[i] == NULL )
      {
         ESP_LOGD( TAG, "allocate memory for slot = %d", i );
         pConnList[i] = ( ConnTypePtr ) malloc( sizeof( NonosConnType ) );
         HEAP_INFO( "" );

         if( pConnList[i] == NULL )
         {
            ESP_LOGD( TAG, "Out of memory allocating connData for slot %d!", i );
            rc = 1; // --> disconnect
         }
         else
         {
            pConnList[i]->slot = -1;
         }
      }
      else
      {
         ESP_LOGD( TAG, "reuse memory for slot = %d", i );
      }

      if( pConnList[i] != NULL )
      {
         ConnTypePtr pConn = pConnList[i];
         HttpdConnData *connData = &pConn->connData;

         // NOTE: httpdConnectCb can fail with out of memory
         httpdConnectCb( pHttpdInstance, connData );  // clears the connData structure

         memcpy( pConn->remote_ip,  conn->proto.tcp->remote_ip, 4 );
         pConn->remote_port = conn->proto.tcp->remote_port;
         pConn->conn = conn;
         pConn->slot = i;

         espconn_regist_recvcb( conn, platRecvCb );
         espconn_regist_reconcb( conn, platReconCb );
         espconn_regist_disconcb( conn, platDisconCb );
         espconn_regist_sentcb( conn, platSentCb );
      }
   }
   else
   {
      // rc = 2;
      ESP_LOGE( TAG, "slot out of range %d > %d", i, maxConnections );
   }

   if( rc > 0 )
   {
      ESP_LOGW( TAG, "Aiee, connData pool overflow!" );
      espconn_disconnect( conn );
   }

   httpdPlatUnlock( pHttpdInstance );
   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Set/clear global httpd lock.
// Not needed on nonoos.
void ICACHE_FLASH_ATTR httpdPlatLock( HttpdInstance *pInstance )
{
}

void ICACHE_FLASH_ATTR httpdPlatUnlock( HttpdInstance *pInstance )
{
}

int ICACHE_FLASH_ATTR httpdPlatSendData( HttpdInstance *pInstance, HttpdConnData *connData, char *buf, int len )
{
   ESP_LOGD( TAG, "httpdPlatSendData ..." );

   int r;

   ConnTypePtr pConn = frconn_of_conn( connData );

   r = espconn_send( pConn->conn, ( uint8_t* )buf, len );
   return ( r >= 0 ? len : 0  );
}

void ICACHE_FLASH_ATTR httpdPlatDisconnect( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "httpdPlatDisconnect ..." );

   ConnTypePtr pConn = frconn_of_conn( connData );
   espconn_disconnect( pConn->conn );
}

void ICACHE_FLASH_ATTR httpdPlatDisableTimeout( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "httpdPlatDisableTimeout ..." );

   ConnTypePtr pConn = frconn_of_conn( connData );
   // Can't disable timeout; set to 2 hours instead.
   espconn_regist_time( pConn->conn, 7199, 1 );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

HttpdPlatTimerHandle ICACHE_FLASH_ATTR httpdPlatTimerCreate( const char *name, int periodMs, int autoreload, void ( *callback )( void *arg ), void *ctx )
{
   ESP_LOGD( TAG, "httpdPlatTimerCreate ..." );

   HttpdPlatTimerHandle newTimer = malloc( sizeof( HttpdPlatTimerHandle ) );
   os_timer_setfn( &newTimer->timer, callback, ctx );

   // store the timer settings into the structure as we want to capture them here but
   // can't apply them until the timer is armed
   newTimer->autoReload = autoreload;
   newTimer->timerPeriodMS = periodMs;

   return newTimer;
}

void ICACHE_FLASH_ATTR httpdPlatTimerStart( HttpdPlatTimerHandle timer )
{
   ESP_LOGD( TAG, "httpdPlatTimerStart ..." );

   os_timer_arm( &timer->timer, timer->timerPeriodMS, timer->autoReload );
}

void ICACHE_FLASH_ATTR httpdPlatTimerStop( HttpdPlatTimerHandle timer )
{
   ESP_LOGD( TAG, "httpdPlatTimerStop ..." );

   os_timer_disarm( &timer->timer );
}

void ICACHE_FLASH_ATTR httpdPlatTimerDelete( HttpdPlatTimerHandle timer )
{
   ESP_LOGD( TAG, "httpdPlatTimerDelete ..." );

   os_timer_disarm( &timer->timer );
   free( timer );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define INADDR_ANY 0

// Listening connection data
static struct espconn espHttpdConn;
static esp_tcp espHttpdTcp;

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInitEx( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port, uint32_t listenAddress,
      int maxConnections, HttpdFlags flags )
{
   // ESP_LOGI( TAG, "httpdNonosInit for port %d", port );

   int i;
   HttpdInitStatus status;

   pHttpdNonosInstance = pInstance;

   pInstance->httpdInstance.builtInUrls = fixedUrls;
   pInstance->httpdInstance.maxConnections = maxConnections;
   pInstance->pConnList = NULL;
   pInstance->httpPort = port;
   pInstance->httpdFlags = flags;
   pInstance->isShutdown = false;

   uint32_t alloc_size = sizeof( ConnTypePtr ) * maxConnections;
   pInstance->pConnList = ( ConnTypePtr * ) malloc( alloc_size );

   if( NULL == pInstance->pConnList )
   {
      ESP_LOGE( TAG, "Cannot allocate %d bytes for connection memory", alloc_size );
      return OutOfMemory;
   }

   for( i = 0; i < maxConnections; i++ )
   {
      pInstance->pConnList[i]= NULL;
   }

   pHttpdInstance = &pInstance->httpdInstance;
   status = InitializationSuccess;

   // Initialize listening socket, do general initialization
   // TODO: check flags
   // TODO: handle listenAddress

   espHttpdConn.type = ESPCONN_TCP;
   espHttpdConn.state = ESPCONN_NONE;
   espHttpdTcp.local_port = port;
   espHttpdConn.proto.tcp = &espHttpdTcp;   // struct espconl global var

   espconn_regist_connectcb( &espHttpdConn, platConnCb );
   espconn_accept( &espHttpdConn );
   espconn_regist_time( &espHttpdConn, HTTPD_CONN_TIMEOUT, 0 ); // Configure timeout

   // increase number of http connections
   int max_tcp_con = espconn_tcp_get_max_con();
   ESP_LOGD( TAG, "Max number of TCP clients: %d", max_tcp_con );
   if( max_tcp_con < maxConnections )
   {
      espconn_tcp_set_max_con( maxConnections );
      ESP_LOG( TAG, "Increase max number of TCP clients from %d to %d", max_tcp_con, espconn_tcp_get_max_con() );
   }

   int max_http_con = espconn_tcp_get_max_con_allow( &espHttpdConn );
   ESP_LOGD( TAG, "Max HTTP connections %d", max_http_con );
   if( max_http_con < maxConnections )
   {
      espconn_tcp_set_max_con_allow( &espHttpdConn, ( uint8_t ) maxConnections );
      ESP_LOG( TAG, "Increase max number of HTTP connections from %d to %d", max_http_con, espconn_tcp_get_max_con_allow( &espHttpdConn ) );
   }

   // ESP_LOGI( TAG, "init done" );
   return status;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

HttpdInitStatus ICACHE_FLASH_ATTR httpdNonosInit( HttpdNonosInstance *pInstance,
      const HttpdBuiltInUrl *fixedUrls, int port,
      int maxConnections, HttpdFlags flags )
{
   // ESP_LOGI( TAG, "httpdNonosInit for port %d", port );
   HttpdInitStatus status;

   status = httpdNonosInitEx( pInstance, fixedUrls, port, INADDR_ANY,
                             maxConnections, flags );
   return status;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif  // ifndef FREERTOS
