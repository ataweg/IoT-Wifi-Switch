// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiswitchStatus.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-18  AWe   replace streq() with strcmp()
//    2017-09-13  AWe   initial implementation
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
static const char* TAG = "cgiswitchStatus";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>

#include "libesphttpd/httpd.h"      // CgiStatus

#include "sntp_client.h"
#include "device.h"     // devGet()

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct rst_info *rst_info_t;
extern rst_info_t sys_rst_info;

extern time_t build_time;              // user_main.c
extern const uint32_t build_number;    // user_main.c

// Template code for the "Device Status" page.

CgiStatus ICACHE_FLASH_ATTR tplSwitchStatus( HttpdConnData *connData, char *token, void **arg )
{
   char buf[256];
   int bufsize = sizeof( buf );
   int buflen;

   if( token == NULL ) return HTTPD_CGI_DONE;

   // create tm struct
   struct tm *dt;
   time_t timestamp = sntp_gettime();
   dt = gmtime( &timestamp );

   strcpy( buf, "Unknown [" );
   strcat( buf, token );
   strcat( buf, "]" );
   buflen = strlen( buf );

   if( strcmp( token, "switch_relay" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%d", devGet( Relay ) );
   }
   else if( strcmp( token, "power_state" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%d", devGet( PowerSense ) );
   }
   else if( strcmp( token, "date_time" ) == 0 )
   {
      buflen = snprintf( buf, bufsize,
                            "%d-%02d-%02d %02d:%02d:%02d",
                            dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday,
                            dt->tm_hour, dt->tm_min, dt->tm_sec );
   }
   else if( strcmp( token, "date" ) == 0 )
   {
      buflen = snprintf( buf, bufsize,
                            "%d-%02d-%02d",
                            dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday );
   }
   else if( strcmp( token, "time" ) == 0 )
   {
      buflen = snprintf( buf, bufsize,
                            "%02d:%02d:%02d",
                            dt->tm_hour, dt->tm_min, dt->tm_sec );
   }
   else if( strcmp( token, "uptime" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%d", sntp_getUptime() );
   }
   else if( strcmp( token, "heap" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%d", system_get_free_heap_size() );
   }
   else if( strcmp( token, "sys_rst_info" ) == 0 )
   {
      switch( sys_rst_info->reason )
      {
         case REASON_DEFAULT_RST:        strcpy( buf, "DEFAULT_RST" );      break;  // 0: normal startup by power on
         case REASON_WDT_RST:            strcpy( buf, "WDT_RST" );          break;  // 1: hardware watch dog reset  exception reset, GPIO status won’t change
         case REASON_EXCEPTION_RST:      strcpy( buf, "EXCEPTION_RST" );    break;  // 2: software watch dog reset, GPIO status won’t change
         case REASON_SOFT_WDT_RST:       strcpy( buf, "SOFT_WDT_RST" );     break;  // 3: software restart,system_restart, GPIO status won’t change
         case REASON_SOFT_RESTART:       strcpy( buf, "SOFT_RESTART" );     break;  // 4:
         case REASON_DEEP_SLEEP_AWAKE:   strcpy( buf, "DEEP_SLEEP_AWAKE" ); break;  // 5: wake up from deep-sleep
         case REASON_EXT_SYS_RST:        strcpy( buf, "EXT_SYS_RST" );      break;  // 6: external system reset
         default:                        strcpy( buf, "unknown" );
      }
      buflen = strlen( buf );
   }
   else if( strcmp( token, "hostname" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%s", wifi_station_get_hostname() );
   }
   else if( strcmp( token, "ip_address" ) == 0 )
   {
      if( wifi_get_opmode() & STATION_MODE )
      {
         struct ip_info ipconfig;
         wifi_get_ip_info( STATION_IF, &ipconfig ); //  0x00
         buflen = snprintf( buf, bufsize, IPSTR, IP2STR( &ipconfig.ip.addr ) );
      }
      else
         buflen = snprintf( buf, bufsize, "%s", "--" );
   }
   else if( strcmp( token, "ap_address" ) == 0 )
   {
      if( wifi_get_opmode() & SOFTAP_MODE )
      {
         struct ip_info ipconfig;
         wifi_get_ip_info( SOFTAP_IF, &ipconfig );
         buflen = snprintf( buf, bufsize, IPSTR, IP2STR( &ipconfig.ip.addr ) );
      }
      else
         buflen = snprintf( buf, bufsize, "%s", "--" );
   }
   else if( strcmp( token, "mac_address" ) == 0 )
   {
      if( wifi_get_opmode() & STATION_MODE )
      {
         uint8_t macaddr[6];
         wifi_get_macaddr( STATION_IF, macaddr );
         buflen = snprintf( buf, bufsize, MACSTR, MAC2STR( macaddr ) );
      }
      else
         buflen = snprintf( buf, bufsize, "%s", "--" );
   }
   else if( strcmp( token, "mac_address_ap" ) == 0 )
   {
      if( wifi_get_opmode() & SOFTAP_MODE )
      {
         uint8_t macaddr[6];
         wifi_get_macaddr( SOFTAP_IF, macaddr );
         buflen = snprintf( buf, bufsize, MACSTR, MAC2STR( macaddr ) );
      }
      else
         buflen = snprintf( buf, bufsize, "%s", "--" );
   }
   else if( strcmp( token, "wlan_net" ) == 0 )
   {
      struct station_config sta_config;
      if( wifi_station_get_config( &sta_config ) )
      {
         buflen = snprintf( buf, bufsize, "%s", sta_config.ssid );
      }
      else
         buflen = snprintf( buf, bufsize, "%s", "--" );
   }
   else if( strcmp( token, "version_date" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%s", _sntp_get_real_time( build_time ) );
   }
   else if( strcmp( token, "version_build" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%d", build_number );
   }

   httpdSend( connData, buf, buflen );
   return HTTPD_CGI_DONE;
}