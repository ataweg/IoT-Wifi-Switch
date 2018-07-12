// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_http.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-08  AWe   remove support for 2nd websocket
//    2017-11-12  AWe   adapt for use with current HW: only one relay
//    2017-08-19  AWe   change debug message printing
//    2017-08-10  AWe   initial implementation
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
static const char* TAG = "user/user_http.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" ( Revision 42 ):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

// The example can print out the heap use every 3 seconds. You can use this to catch memory leaks.
//#define SHOW_HEAP_USE

/*
This is example code for the esphttpd library. It's a small-ish demo showing off
the server, including WiFi connection management capabilities, some IO and
some pictures of cats.
*/

#include <osapi.h>
#include <user_interface.h>

#include "io.h"
#include "leds.h"
#include "libesphttpd/httpd.h"
#include "libesphttpd/httpdespfs.h"
#include "libesphttpd/cgiwifi.h"
#include "libesphttpd/cgiflash.h"
#include "libesphttpd/auth.h"
#include "libesphttpd/espfs.h"
#include "libesphttpd/captdns.h"
#include "libesphttpd/webpages-espfs.h"
#include "libesphttpd/cgiwebsocket.h"
#include "libesphttpd/httpd-nonos.h"
#include "libesphttpd/cgiredirect.h"
#include "libesphttpd/route.h"

#include "sntp_client.h"
#include "cgiRelayStatus.h"
#include "cgiTimer.h"
#include "cgiHistory.h"
#include "cgiConfig.h"
#include "mqtt_config.h"
#include "wifi_config.h"
#include "device.h"     // devGet()

// --------------------------------------------------------------------------
//  Prototypes
// --------------------------------------------------------------------------

static int  ICACHE_FLASH_ATTR prepareSystemStatusMsg( char *buf, int bufsize );
static int  ICACHE_FLASH_ATTR httpdPassFn( HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen );
static void ICACHE_FLASH_ATTR httpdWebsockTimerCb( void *arg );
static void ICACHE_FLASH_ATTR httpdWebsocketRecv( Websock *ws, char *data, int len, int flags );
static void ICACHE_FLASH_ATTR httpdWebsocketConnect( Websock *ws );

void ICACHE_FLASH_ATTR httpdInit( void );
int  ICACHE_FLASH_ATTR httpdBroadcastStart( void );
int  ICACHE_FLASH_ATTR httpdBroadcastStop( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static HttpdNonosInstance httpdNonosInstance;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things ( like authBasic ) act as a 'barrier' and
should be placed above the URLs they protect.
*/
const HttpdBuiltInUrl builtInUrls[] ICACHE_RODATA_ATTR STORE_ATTR =
{
// url                         | SendCallback                   | Arguments
// const char *url;            | cgiSendCallback cgiCb;         | const void *cgiArg, cgiArg2;
// ----------------------------+--------------------------------+----------------------
   {"*",                        cgiRedirectApClientToHostname,   "esp8266.nonet", NULL },
   {"/",                        cgiRedirect,                     "/index.tpl.html", NULL },
   {"/index.tpl.html",          cgiEspFsTemplate,                tplRelayStatus, NULL },
   {"/Timer.tpl.html",          cgiEspFsTemplate,                tplTimer, NULL },
   {"/settimer.cgi",            cgiSetTimer,                     NULL, NULL },
   {"/WifiConfig.tpl.html",     cgiEspFsTemplate,                tplConfig, NULL },
   {"/MqttConfig.tpl.html",     cgiEspFsTemplate,                tplConfig, NULL },
   {"/Config.cgi",              cgiConfig,                       NULL, NULL },
   {"/History.tpl.html",        cgiEspFsTemplate,                tplHistory, NULL },

   {"/status",                  cgiWebsocket,                    httpdWebsocketConnect, NULL },

   // Routines to make the /wifi URL and everything beneath it work.
   // Enable the line below to protect the WiFi configuration with an username/password combo.
   {"/wifi/*",                  authBasic,                       httpdPassFn, NULL },

   {"/wifi",                    cgiRedirect,                     "/WifiSetup.tpl.html", NULL },
   {"/wifi/",                   cgiRedirect,                     "/WifiSetup.tpl.html", NULL },
   {"/WifiSetup.tpl.html",      cgiEspFsTemplate,                tplWlan, NULL },
   {"/wifi/wifiscan.cgi",       cgiWiFiScan,                     NULL, NULL },     // called from WifiSetup.tpl.html
   {"/wifi/connect.cgi",        cgiWiFiConnect,                  NULL, NULL },     // called from WifiSetup.tpl.html
   {"/wifi/connstatus.cgi",     cgiWiFiConnStatus,               NULL, NULL },     // called from wifi/connecting.html
   {"/wifi/setmode.cgi",        cgiWiFiSetMode,                  NULL, NULL },     // not used

#ifdef INCLUDE_FLASH_FNS
   {"/flash/next",              cgiGetFirmwareNext,              &uploadParams, NULL },
   {"/flash/upload",            cgiUploadFirmware,               &uploadParams, NULL },
#endif
   {"/flash/reboot",            cgiRebootFirmware,               NULL, NULL },

   {"*",                        cgiEspFsHook,                    NULL, NULL },     // Catch-all cgi function for the filesystem
   {NULL, NULL, NULL, NULL}
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Function that tells the authentication system what users/passwords live on the system.
// This is disabled in the default build; if you want to try it, enable the authBasic line in
// the builtInUrls below.

static int ICACHE_FLASH_ATTR httpdPassFn( HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen )
{
   if( no == 0 )
   {
      os_strcpy( user, "admin" );
      os_strcpy( pass, "welc0me" );
      return 1;
// Add more users this way. Check against incrementing no for each user added.
// } else if ( no==1 ) {
//    os_strcpy( user, "user1" );
//    os_strcpy( pass, "something" );
//    return 1;
   }
   return 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static os_timer_t websockTimer;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR prepareSystemStatusMsg( char *buf, int bufsize )
{
   // create tm struct
   struct tm *dt;
   time_t timestamp = sntp_gettime();
   dt = gmtime( &timestamp );

   time_t uptime = sntp_getUptime();
   int heap = ( int ) system_get_free_heap_size();
   int sysled    = devGet( SysLed );
   int info_led  = devGet( InfoLed );
   int relay     = devGet( Relay );
   int pwr_sense = devGet( PowerSense );

   /* Generate response in JSON format */
   int buflen = os_snprintf( buf, bufsize,
                             "{\"date_time\" : \"%d-%02d-%02d %02d:%02d:%02d\",",
                             dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday,
                             dt->tm_hour, dt->tm_min, dt->tm_sec );

   buf += buflen; // move to end of msg
   bufsize -= buflen;

   buflen += os_snprintf( buf, bufsize,
                          " \"uptime\" : \"%ld\","
                          " \"heap\" : \"%d\","
                          " \"sysled\" : \"%d\","
                          " \"relay\" : \"%d\","
                          " \"info_led\" : \"%d\","
                          " \"power\" : \"%d\"}",
                          uptime, heap, sysled,
                          relay, info_led, pwr_sense );

   // ESP_LOGD( TAG, "prepareSystemStatusMsg: %s", buf );
   return buflen;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Broadcast the uptime in seconds every second over connected websockets
static void ICACHE_FLASH_ATTR httpdWebsockTimerCb( void *arg )
{
   static int msg_count = 10;
   if( msg_count > 0 )
   {
      ESP_LOGD( TAG, "httpdWebsockTimerCb" );
      msg_count--;
   }

   char buf[256];

   int buflen = prepareSystemStatusMsg( buf, sizeof( buf ) );

   if( buflen < sizeof( buf ) )
      cgiWebsockBroadcast( &httpdNonosInstance.httpdInstance, "/status", buf, os_strlen( buf ), WEBSOCK_FLAG_NONE );
}

// --------------------------------------------------------------------------
// Websocket
// --------------------------------------------------------------------------

// On reception of a message, send the requested data
static void ICACHE_FLASH_ATTR httpdWebsocketRecv( Websock *ws, char *data, int len, int flags )
{
   ESP_LOGI( TAG, "httpdWebsocketRecv: url=%s len=%d %c", ws->connData->url, len, data[0] );
   char buf[256];

   int buflen = prepareSystemStatusMsg( buf, sizeof( buf ) );

   cgiWebsocketSend( &httpdNonosInstance.httpdInstance, ws, buf, buflen, flags /* WEBSOCK_FLAG_NONE*/ );
}

static void ICACHE_FLASH_ATTR httpdWebsocketConnect( Websock *ws )
{
   ESP_LOGD( TAG, "httpdWebsocketConnect" );

   ws->recvCb = httpdWebsocketRecv;
// ws->sendCb = ???
// ws-closeCb = ???
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef ESPFS_POS
#define INCLUDE_FLASH_FNS

const CgiUploadFlashDef uploadParams =
{
   .type   = CGIFLASH_TYPE_ESPFS,
   .fw1Pos = ESPFS_POS,
   .fw2Pos = 0,
   .fwSize = ESPFS_SIZE,
};
#endif

#ifdef OTA_FLASH_SIZE_K
#define INCLUDE_FLASH_FNS

const CgiUploadFlashDef uploadParams =
{
   .type    = CGIFLASH_TYPE_FW,
   .fw1Pos  = 0x1000,
   .fw2Pos  = ( ( OTA_FLASH_SIZE_K * 1024 ) / 2 ) + 0x1000,
   .fwSize  = ( ( OTA_FLASH_SIZE_K * 1024 ) / 2 ) - 0x1000,
   .tagName = OTA_TAGNAME
};
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR httpdInit( void )
{
   ESP_LOGD( TAG, "httpdInit" );
   HEAP_INFO( "" );

   captdnsInit();

   // 0x40200000 is the base address for spi flash memory mapping, ESPFS_POS is the position
   // where image is written in flash that is defined in Makefile.
#ifdef ESPFS_POS
   espFsInit( ( void* )( 0x40200000 + ESPFS_POS ) );
#else
   espFsInit( ( void* )( webpages_espfs_start ) );
#endif

   // start the http daemon
#ifndef CONFIG_ESPHTTPD_SSL_SUPPORT
   int listenPort = 80;
   httpdNonosInit( &httpdNonosInstance, builtInUrls, listenPort,
                   HTTPD_MAX_CONNECTIONS, HTTPD_FLAG_NONE );
#else
   int listenPort = 443;
   httpdNonosInit( &httpdNonosInstance, builtInUrls, listenPort,
                    HTTPD_MAX_CONNECTIONS, HTTPD_FLAG_SSL );
#endif

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR httpdBroadcastStart( void )
{
   ESP_LOGD( TAG, "httpdBroadcastStart" );
   HEAP_INFO( "" );

   os_timer_disarm( &websockTimer );
   os_timer_setfn( &websockTimer, httpdWebsockTimerCb, NULL );
   os_timer_arm( &websockTimer, 1000, 1 );

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR httpdBroadcastStop( void )
{
   ESP_LOGD( TAG, "httpdBroadcastStop" );
   HEAP_INFO( "" );

   os_timer_disarm( &websockTimer );

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
