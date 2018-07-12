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
//    2018-01-18  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//    2017-08-24  AWe   replace
//                      resetTimer      --> connectTimer
//                      reassTimer      --> reconnectTimer
//                      resetTimerCb    --> staCheckConnStatusCb
//                      reassTimerCb    --> cgiWiFiDoConnectCb
//
//                      reformat some parts
//                      shrink some buffers for sprintf()
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//                      original sources at https://github.com/Spritetm/libesphttpd
//
// --------------------------------------------------------------------------

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Cgi/template routines for the /wifi url.

GET/POST parameter
   "essid"     cgiWiFiConnect
   "password"  cgiWiFiConnect
   "mode"      cgiWiFiSetMode
   "ch"        cgiWiFiSetChannel
   "name"      cgiWiFiSetSSID
*/

#ifndef ESP_PLATFORM

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "cgiwifi";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <libesphttpd/esp.h>
#include "libesphttpd/cgiwifi.h"

// Enable this to disallow any changes in AP settings
// #define DEMO_MODE

// WiFi access point data
typedef struct
{
   char ssid[32];
   char bssid[8];
   int channel;
   char rssi;
   char enc;
} ApData;

// Scan result
typedef struct
{
   char scanInProgress; // if 1, don't access the underlying stuff from the webpage.
   ApData **apData;
   int noAps;
} ScanResultData;

// Static scan status storage.
static ScanResultData cgiWifiAps;

#define CONNTRY_IDLE       0
#define CONNTRY_WORKING    1
#define CONNTRY_SUCCESS    2
#define CONNTRY_FAIL       3

// Connection result var
static int connTryStatus = CONNTRY_IDLE;
static os_timer_t connectTimer;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Callback the code calls when a wlan ap scan is done. Basically stores the result in
// the cgiWifiAps struct.

void ICACHE_FLASH_ATTR wifiScanDoneCb( void *arg, STATUS status )
{
   ESP_LOGD( TAG, "wifiScanDoneCb %d", status );

   int n;
   struct bss_info *bss_link = ( struct bss_info * )arg;

   if( status != OK )
   {
      cgiWifiAps.scanInProgress = 0;
      return;
   }

   // Clear prev ap data if needed.
   if( cgiWifiAps.apData != NULL )
   {
      for( n = 0; n < cgiWifiAps.noAps; n++ )
         free( cgiWifiAps.apData[n] );
      free( cgiWifiAps.apData );
   }

   // Count amount of access points found.
   n = 0;
   while( bss_link != NULL )
   {
      bss_link = bss_link->next.stqe_next;
      n++;
   }
   // Allocate memory for access point data
   cgiWifiAps.apData = ( ApData ** )malloc( sizeof( ApData * )*n );
   if( cgiWifiAps.apData == NULL )
   {
      ESP_LOGE( TAG, "Out of memory allocating apData" );
      return;
   }
   cgiWifiAps.noAps = n;
   ESP_LOGD( TAG, "Scan done: found %d APs", n );

   // Copy access point data to the static struct
   n = 0;
   bss_link = ( struct bss_info * )arg;
   while( bss_link != NULL )
   {
      if( n >= cgiWifiAps.noAps )
      {
         // This means the bss_link changed under our nose. Shouldn't happen!
         // Break because otherwise we will write in unallocated memory.
         ESP_LOGE( TAG, "Huh? I have more than the allocated %d aps!", cgiWifiAps.noAps );
         break;
      }
      // Save the ap data.
      cgiWifiAps.apData[n] = ( ApData * )malloc( sizeof( ApData ) );
      if( cgiWifiAps.apData[n] == NULL )
      {
         ESP_LOGE( TAG, "Can't allocate mem for ap buf." );
         cgiWifiAps.scanInProgress = 0;
         return;
      }
      cgiWifiAps.apData[n]->rssi = bss_link->rssi;
      cgiWifiAps.apData[n]->channel = bss_link->channel;
      cgiWifiAps.apData[n]->enc = bss_link->authmode;
      strncpy( cgiWifiAps.apData[n]->ssid, ( char* )bss_link->ssid, 32 );
      strncpy( cgiWifiAps.apData[n]->bssid, ( char* )bss_link->bssid, 6 );

      bss_link = bss_link->next.stqe_next;
      n++;
   }
   // We're done.
   cgiWifiAps.scanInProgress = 0;

   ESP_LOGD( TAG, "wifiScanDoneCb finished. We had %d APs", n );
}


// Routine to start a WiFi access point scan.
static void ICACHE_FLASH_ATTR wifiStartScan()
{
   ESP_LOGD( TAG, "wifiStartScan" );

   if( cgiWifiAps.scanInProgress ) return;
   cgiWifiAps.scanInProgress = 1;
   wifi_station_scan( NULL, wifiScanDoneCb );
}

// This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
// scan for access points and if available will return the result of an earlier scan.
// The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.

/* json parameter
      "essid"
      "bssid"
      "rssi"
      "enc"
      "channel"

      "result"
      "inProgress"
      "APs"
*/

CgiStatus ICACHE_FLASH_ATTR cgiWiFiScan( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiScan" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   int pos = ( int )connData->cgiData;
   int len;
   char buf[256];

   if( !cgiWifiAps.scanInProgress && pos != 0 )
   {
      // Fill in json code for an access point
      if( pos - 1 < cgiWifiAps.noAps )
      {
         len = sprintf( buf, "{\"essid\": \"%s\", \"bssid\": \"" MACSTR "\", \"rssi\": \"%d\", \"enc\": \"%d\", \"channel\": \"%d\"}%s\n",
                        cgiWifiAps.apData[pos - 1]->ssid,
                        MAC2STR( cgiWifiAps.apData[pos - 1]->bssid ),
                        cgiWifiAps.apData[pos - 1]->rssi,
                        cgiWifiAps.apData[pos - 1]->enc,
                        cgiWifiAps.apData[pos - 1]->channel,
                        ( pos - 1 == cgiWifiAps.noAps - 1 ) ? "\r\n" : ",\r\n" ); // <-terminator

         httpdSend( connData, buf, len );
      }
      pos++;
      if( ( pos - 1 ) >= cgiWifiAps.noAps )
      {
         len = sprintf( buf, "]\r\n}\r\n}\r\n" ); // terminate the whole object
         httpdSend( connData, buf, len );
         // Also start a new scan.
         wifiStartScan();
         return HTTPD_CGI_DONE;
      }
      else
      {
         connData->cgiData = ( void* )pos;
         return HTTPD_CGI_MORE;
      }
   }

   httpdStartResponse( connData, 200 );
   httpdHeader( connData, "Content-Type", "application/json" );
   httpdEndHeaders( connData );

   if( cgiWifiAps.scanInProgress == 1 )
   {
      // We're still scanning. Tell Javascript code that.
      len = sprintf( buf, "{\r\n \"result\": { \r\n\"inProgress\": \"1\"\r\n }\r\n}\r\n" );
      httpdSend( connData, buf, len );
      return HTTPD_CGI_DONE;
   }
   else
   {
      // We have a scan result. Pass it on.
      len = sprintf( buf, "{\r\n \"result\": { \r\n\"inProgress\": \"0\", \r\n\"APs\": [\r\n" );
      httpdSend( connData, buf, len );
      if( cgiWifiAps.apData == NULL )
         cgiWifiAps.noAps = 0;
      connData->cgiData = ( void * )1;
      return HTTPD_CGI_MORE;
   }
}

// Temp store for new ap info.
static struct station_config stconf;

// This routine is ran some time after a connection attempt to an access point. If
// the connect succeeds, this gets the module in STA-only mode.

static void ICACHE_FLASH_ATTR staCheckConnStatusCb( void *arg )
{
   int status = wifi_station_get_connect_status();
   if( status == STATION_GOT_IP )
   {
      // Go to STA mode only
      ESP_LOGI( TAG, "Got IP. Going into STA mode.." );
      wifi_set_opmode( STATION_MODE );
   }
   else
   {
      connTryStatus = CONNTRY_FAIL;
      ESP_LOGW( TAG, "Connect fail. Not going into STA-only mode." );
      // Maybe also pass this through on the webpage?
   }
}

// Actually connect to a station. This routine is timed because I had problems
// with immediate connections earlier. It probably was something else that caused it,
// but I can't be arsed to put the code back :P

#define CONNECT_MAX_RETRY        ( 4 )
static int connect_retry_count = 0;

static void ICACHE_FLASH_ATTR cgiWiFiDoConnectCb( void *arg )
{
   ESP_LOGD( TAG, "Try to connect to AP...." );

   int opmode  = wifi_get_opmode();
   if( opmode & STATION_MODE )
   {
      wifi_station_disconnect();
      wifi_station_set_config( &stconf );
      wifi_station_connect();
   }

   connTryStatus = CONNTRY_WORKING;
   if( opmode != STATION_MODE )
   {
      // is SoftAp or Station+SoftAP mode
      if( connect_retry_count < CONNECT_MAX_RETRY )
      {
         // Schedule disconnect/connect
         os_timer_disarm( &connectTimer );
         os_timer_setfn( &connectTimer, staCheckConnStatusCb, NULL );
         os_timer_arm( &connectTimer, 30000, 0 ); // time out after 30 secs of trying to connect
         connect_retry_count++;
      }
      else
      {
         ESP_LOGW( TAG, "Give up to connect to AP...." );
      }
   }
}

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.

// called via /wifi/connect.cgi from WifiSetup.tpl.html

CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnect( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiConnect" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   char essid[32];
   char password[64];

   httpdFindArg( connData->post.buf, "essid", essid, sizeof( essid ) );
   httpdFindArg( connData->post.buf, "password", password, sizeof( password ) );

   strncpy( ( char* )stconf.ssid, essid, 32 );
   strncpy( ( char* )stconf.password, password, 64 );
   ESP_LOGI( TAG, "Try to connect to AP \"%s\" pw \"%s\"", essid, password );

// Set to 0 if you want to disable the actual reconnecting bit
#ifdef DEMO_MODE
   httpdRedirect( connData, "/wifi" );
#else
   // Schedule disconnect/connect
   connect_retry_count = 0;
   os_timer_disarm( &connectTimer );
   os_timer_setfn( &connectTimer, cgiWiFiDoConnectCb, NULL );
   os_timer_arm( &connectTimer, 500, 0 );
   httpdRedirect( connData, "connecting.html" );
#endif
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.

CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetMode( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiSetMode" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   int len;
   char buf[64];

   len = httpdFindArg( connData->getArgs, "mode", buf, sizeof( buf ) );
   if( len > 0 )
   {
      ESP_LOGI( TAG, "cgiWifiSetMode: %d", atoi( buf ) );
#ifndef DEMO_MODE
      wifi_set_opmode( atoi( buf ) );
      // we have to setup the station here properly, but it's simpler to do a system restart
      ESP_LOGI( TAG, "Restarting system...\r\n\r\n\r\n" );
      system_restart();
#endif
   }
   httpdRedirect( connData, "/wifi" );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
// taken from MightyPork/libesphttpd
// --------------------------------------------------------------------------

// Set wifi channel for AP mode
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetChannel( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiSetChannel" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   int len;
   char buf[64];

   len = httpdFindArg( connData->getArgs, "ch", buf, sizeof( buf ) );
   if( len > 0 )
   {
      ESP_LOGD( TAG, "cgiWifiSetChannel: %s", buf );
      int channel = atoi( buf );
      if( channel > 0 && channel < 15 )
      {
         ESP_LOGI( TAG, "Setting wifi channel %d", channel );

         struct softap_config wificfg;
         wifi_softap_get_config( &wificfg );
         wificfg.channel = ( uint8_t )channel;
         wifi_softap_set_config( &wificfg );
      }
   }
   httpdRedirect( connData, "/wifi" );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// json parameter
//    "status"  : "idle" | "success" | "working" | "fail"
//    "ip"

CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnStatus( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiConnStatus" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   char buf[128];
   int len;
   struct ip_info info;
   int st = wifi_station_get_connect_status();

   httpdStartResponse( connData, 200 );
   httpdHeader( connData, "Content-Type", "text/json" );
   httpdEndHeaders( connData );

   if( connTryStatus == CONNTRY_IDLE )
   {
      len = sprintf( buf, "{\n \"status\": \"idle\"\n }\n" );
   }
   else if( connTryStatus == CONNTRY_WORKING || connTryStatus == CONNTRY_SUCCESS )
   {
      if( st == STATION_GOT_IP )
      {
         wifi_get_ip_info( 0, &info );
         len = sprintf( buf, "{\n \"status\": \"success\",\n \"ip\": \"%d.%d.%d.%d\" }\n",
                        ( info.ip.addr >> 0 ) & 0xff, ( info.ip.addr >> 8 ) & 0xff,
                        ( info.ip.addr >> 16 ) & 0xff, ( info.ip.addr >> 24 ) & 0xff );
         // Reset into AP-only mode sooner.
         os_timer_disarm( &connectTimer );
         os_timer_setfn( &connectTimer, staCheckConnStatusCb, NULL );
         os_timer_arm( &connectTimer, 1000, 0 );
      }
      else
      {
         len = sprintf( buf, "{\n \"status\": \"working\"\n }\n" );
      }
   }
   else
   {
      len = sprintf( buf, "{\n \"status\": \"fail\"\n }\n" );
   }

   httpdSend( connData, buf, len );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

/* template parameter
      %WiFiCurrSsid%
      %WiFiMode%
      %WiFiApWarn%
      %WiFiPassword%
      %wlan_net%
      %wlan_password%
 */

// Template code for the WLAN page.
CgiStatus ICACHE_FLASH_ATTR tplWlan( HttpdConnData *connData, char *token, void **arg )
{
   ESP_LOGD( TAG, "tplWlan token %s", S( token ) );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   char buf[256];
   int opmode;
   static struct station_config stconf;
   if( token == NULL ) return HTTPD_CGI_DONE;
   wifi_station_get_config( &stconf );

   os_strcpy( buf, "Unknown [" );
   os_strcat( buf, token );
   os_strcat( buf, "]" );

   if( strcmp( token, "WiFiMode" ) == 0 )
   {
      opmode = wifi_get_opmode();
      if( opmode == STATION_MODE )   strcpy( buf, "Station" );
      if( opmode == SOFTAP_MODE )    strcpy( buf, "SoftAP" );
      if( opmode == STATIONAP_MODE ) strcpy( buf, "Station + SoftAP" );
   }
   else if( strcmp( token, "WiFiCurrSsid" ) == 0 )
   {
      strcpy( buf, ( char* )stconf.ssid );
   }
   else if( strcmp( token, "WiFiPassword" ) == 0 )
   {
      strcpy( buf, ( char* )stconf.password );
   }
   else if( strcmp( token, "WiFiApWarn" ) == 0 )
   {
      opmode = wifi_get_opmode();
      if( opmode == SOFTAP_MODE )
      {
         strcpy( buf, "<b>Can't scan in this mode.</b> Click <a href=\"wifi/setmode.cgi?mode=1\">here</a> to go to Station mode."
                       " or <a href=\"wifi/setmode.cgi?mode=3\">here</a> to go to Station + SoftAP mode." );
      }
      else if( opmode == STATIONAP_MODE )
      {
         strcpy( buf, "Click <a href=\"wifi/setmode.cgi?mode=1\">here</a> to go to Station mode"
                       " or <a href=\"wifi/setmode.cgi?mode=2\">here</a> to go to SoftAP mode." );
      }
      else  // STATION_MODE
      {
         strcpy( buf, "Click <a href=\"wifi/setmode.cgi?mode=3\">here</a> to go to Station + SoftAP mode"
                       " or <a href=\"wifi/setmode.cgi?mode=2\">here</a> to go to SoftAP mode." );
      }
   }
   httpdSend( connData, buf, -1 );
   return HTTPD_CGI_DONE;
}
#endif
