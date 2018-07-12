// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - Wifi Switch
//
// File          user_wifi.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   remove wait_for_connection(), add its stuff to wifiIsConnected()
//                      remove the wait_for_connection timer
//                      connect to the external clients/servers in the event handler
//    2018-05-03  AWe   replace debug output from DBG_* to ESP_LOG*
//    2018-04-23  AWe   add search for known networks from WebServer project
//    2018-04-18  AWe   take over and adept from camera project
//    2018-04-18  AWe   move the wifi stuff to user_wifi.c. Add an event handler there
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

#define LOG_LOCAL_LEVEL    ESP_LOG_VERBOSE
static const char* TAG = "user/user_wifi.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>
#include <driver/uart.h>
#include <espconn.h>          // espconn_tcp_set_max_con()

#include "user_config.h"
#include "esp_missing.h"      // os_snprintf()

#include "wifi_settings.h"
#include "wifi_config.h"
#include "user_sntp.h"        // user_snpt_start(), user_snpt_stop()
#include "user_mqtt.h"        // mqttWifiConnect()
#include "user_httpd.h"       // httpdBroadcastStart(), httpdBroadcastStop()
#include "user_wifi.h"
#include "leds.h"
#include "cgiHistory.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// WiFi access point data
typedef struct
{
   char ssid[32];
   char bssid[8];
   int channel;
   char rssi;
   char enc;
} wifi_ap_record_t;

#define MAX_KNOWN_AP    ( sizeof( uint32_t ) / sizeof( uint8_t ) )         // 4

typedef union
{
   uint32_t val;
   uint8_t  list[ MAX_KNOWN_AP ];
} last_known_ap_t;

// --------------------------------------------------------------------------
// global variables
// --------------------------------------------------------------------------

// Static scan status storage.
static wifi_ap_record_t *ap_records = NULL;
static uint8_t ap_num = 0;
static uint8_t ap_index = 0;

static uint8_t known_index_checked_mask = 0;

static uint8_t wifi_enable_reconnect = 0;

#define CONNECT_MAX_RETRY        ( 30 )         // give 30 seconds time to connect and get an ip
static int retry_connect_count = 0;
#define CONNECT_TIMEOUT          ( 1000 )       // 1000 ms
static os_timer_t connect_timeout;

#define AP_TIMEOUT               ( 2*60*1000 )  // 2 min
static os_timer_t ap_timeout;

// --------------------------------------------------------------------------
// prototypes for static functions
// --------------------------------------------------------------------------

static const char* ICACHE_FLASH_ATTR auth2str( int auth );
static const char* ICACHE_FLASH_ATTR opmode2str( int opmode );
static const char* ICACHE_FLASH_ATTR reason2str( int reason );

static void ICACHE_FLASH_ATTR wifiConfigDump( void );
static void ICACHE_FLASH_ATTR wifiSoftApConfigDump( struct softap_config *ap_config );
static void ICACHE_FLASH_ATTR wifiStationConfigDump( struct station_config *sta_config );

static void ICACHE_FLASH_ATTR wifiScanDoneCb( void *arg, STATUS status );
static int  ICACHE_FLASH_ATTR wifiSaveNetworkId( char *ssid, char *password );
static int  ICACHE_FLASH_ATTR wifiIsKnownNetwork( char *ssid, char *password, int max_password_len );
static int  ICACHE_FLASH_ATTR wifiConnectToKnownNetwork( void );
static int  ICACHE_FLASH_ATTR wifiReconnect( char *ssid, int ssid_len, int reason );

static void ICACHE_FLASH_ATTR wifiConnectionTimeout( void *arg );
static int  ICACHE_FLASH_ATTR wifiIsConnected( void );
static void ICACHE_FLASH_ATTR wifiEventHandlerCb( System_Event_t *event );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// next two function also defined in libesphttpd\util\cgiwifi.c

static const char* ICACHE_FLASH_ATTR auth2str( int auth )
{
   switch( auth )
   {
      case AUTH_OPEN:         return "Open";
      case AUTH_WEP:          return "WEP";
      case AUTH_WPA_PSK:      return "WPA";
      case AUTH_WPA2_PSK:     return "WPA2";
      case AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
      default:                return "Unknown";
   }
}

static const char* ICACHE_FLASH_ATTR opmode2str( int opmode )
{
   switch( opmode )
   {
      case NULL_MODE:      return "Disabled";
      case STATION_MODE:   return "Station";
      case SOFTAP_MODE:    return "SoftAP";
      case STATIONAP_MODE: return "Station + SoftAP";
      default:             return "Unknown";
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// next two function also defined in libesphttpd\util\cgiwifi.c

static const char* ICACHE_FLASH_ATTR reason2str( int reason )
{
   switch( reason )
   {
      case REASON_UNSPECIFIED:              return "UNSPECIFIED";                  //   1
      case REASON_AUTH_EXPIRE:              return "AUTH_EXPIRE";                  //   2
      case REASON_AUTH_LEAVE:               return "AUTH_LEAVE";                   //   3
      case REASON_ASSOC_EXPIRE:             return "ASSOC_EXPIRE";                 //   4
      case REASON_ASSOC_TOOMANY:            return "ASSOC_TOOMANY";                //   5
      case REASON_NOT_AUTHED:               return "NOT_AUTHED";                   //   6
      case REASON_NOT_ASSOCED:              return "NOT_ASSOCED";                  //   7
      case REASON_ASSOC_LEAVE:              return "ASSOC_LEAVE";                  //   8
      case REASON_ASSOC_NOT_AUTHED:         return "ASSOC_NOT_AUTHED";             //   9
      case REASON_DISASSOC_PWRCAP_BAD:      return "DISASSOC_PWRCAP_BAD";          //  10   /* 11h */
      case REASON_DISASSOC_SUPCHAN_BAD:     return "DISASSOC_SUPCHAN_BAD";         //  11   /* 11h */
      case REASON_IE_INVALID:               return "IE_INVALID";                   //  13   /* 11i */
      case REASON_MIC_FAILURE:              return "MIC_FAILURE";                  //  14   /* 11i */
      case REASON_4WAY_HANDSHAKE_TIMEOUT:   return "4WAY_HANDSHAKE_TIMEOUT";       //  15   /* 11i */
      case REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";     //  16   /* 11i */
      case REASON_IE_IN_4WAY_DIFFERS:       return "IE_IN_4WAY_DIFFERS";           //  17   /* 11i */
      case REASON_GROUP_CIPHER_INVALID:     return "GROUP_CIPHER_INVALID";         //  18   /* 11i */
      case REASON_PAIRWISE_CIPHER_INVALID:  return "PAIRWISE_CIPHER_INVALID";      //  19   /* 11i */
      case REASON_AKMP_INVALID:             return "AKMP_INVALID";                 //  20   /* 11i */
      case REASON_UNSUPP_RSN_IE_VERSION:    return "UNSUPP_RSN_IE_VERSION";        //  21   /* 11i */
      case REASON_INVALID_RSN_IE_CAP:       return "INVALID_RSN_IE_CAP";           //  22   /* 11i */
      case REASON_802_1X_AUTH_FAILED:       return "REASON_802_1X_AUTH_FAILED";    //  23   /* 11i */
      case REASON_CIPHER_SUITE_REJECTED:    return "REASON_CIPHER_SUITE_REJECTED"; //  24   /* 11i */

      case REASON_BEACON_TIMEOUT:           return "REASON_BEACON_TIMEOUT";        // 200
      case REASON_NO_AP_FOUND:              return "REASON_NO_AP_FOUND";           // 201
      case REASON_AUTH_FAIL:                return "REASON_AUTH_FAIL";             // 202
      case REASON_ASSOC_FAIL:               return "REASON_ASSOC_FAIL";            // 203
      case REASON_HANDSHAKE_TIMEOUT:        return "REASON_HANDSHAKE_TIMEOUT";     // 204
      default:                              return "Unknown";
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR wifiConfigDump( void )
{
#ifndef NDEBUG
   ESP_LOGV( TAG, "wifi:Settings: wifi_client:     \"%s\"", Wifi_Client_Cfg.Name );
   ESP_LOGV( TAG, "wifi:Settings: wifi_server:     \"%s\"", Wifi_Client_Cfg.Server );
   ESP_LOGV( TAG, "wifi:Settings: wifi_user:       \"%s\"", Wifi_Client_Cfg.Username );
   ESP_LOGV( TAG, "wifi:Settings: wifi_password:   \"%s\"", Wifi_Client_Cfg.Password );
   ESP_LOGV( TAG, "wifi:Settings: wifi_port:       %d\r\n", Wifi_Client_Cfg.Port );

   ESP_LOGV( TAG, "wifi:Settings: ap_ssid:         \"%s\"", Wifi_Ap_Cfg.Ssid );
   ESP_LOGV( TAG, "wifi:Settings: ap_password:       \"%s\"", Wifi_Ap_Cfg.Password );
   ESP_LOGV( TAG, "wifi:Settings: ap_authmode:     %s",     auth2str( Wifi_Ap_Cfg.Authmode ) );
   ESP_LOGV( TAG, "wifi:Settings: ap_channel:      %d",     Wifi_Ap_Cfg.Channel );
   ESP_LOGV( TAG, "wifi:Settings: ap_ssid_hidden:  %s",     Wifi_Ap_Cfg.SsidHidden ? "yes" : "no" );
   ESP_LOGV( TAG, "wifi:Settings: ap_max_conn:     %d",     Wifi_Ap_Cfg.MaxConnection );
   ESP_LOGV( TAG, "wifi:Settings: ap_bandwidth:    %d",     Wifi_Ap_Cfg.Bandwidth );
   ESP_LOGV( TAG, "wifi:Settings: ap_beacon_ival:  %d",     Wifi_Ap_Cfg.BeaconInterval );
   ESP_LOGV( TAG, "wifi:Settings: ap_address:      "IPSTR,  IP2STR( &Wifi_Ap_Cfg.Address ) );
   ESP_LOGV( TAG, "wifi:Settings: ap_gateway:      "IPSTR,  IP2STR( &Wifi_Ap_Cfg.Gateway ) );
   ESP_LOGV( TAG, "wifi:Settings: ap_netmask:      "IPSTR"\r\n", IP2STR( &Wifi_Ap_Cfg.Netmask ) );

   ESP_LOGV( TAG, "wifi_settings: sta_only:        %s",     Wifi_Sta_Cfg.Only ? "yes" : "no" );
   ESP_LOGV( TAG, "wifi_settings: sta_power_save:  %s",     Wifi_Sta_Cfg.PowerSave ? "yes" : "no" );
   ESP_LOGV( TAG, "wifi_settings: sta_static_ipi:  %s",     Wifi_Sta_Cfg.StaticIp ? "yes" : "no" );
   ESP_LOGV( TAG, "wifi_settings: sta_ip_addr:     "IPSTR,  IP2STR( &Wifi_Sta_Cfg.Address ) );
   ESP_LOGV( TAG, "wifi_settings: sta_gw_addr:     "IPSTR,  IP2STR( &Wifi_Sta_Cfg.Gateway ) );
   ESP_LOGV( TAG, "wifi_settings: sta_netmask:     "IPSTR,  IP2STR( &Wifi_Sta_Cfg.Netmask ) );
   ESP_LOGV( TAG, "wifi_settings: sta_client_name: \"%s\"\r\n", Wifi_Sta_Cfg.Client_Name );

   ESP_LOGV( TAG, "wifi_settings: dhcp_enable:     %s",     Wifi_Dhcp_Cfg.Enable ? "yes" : "no" );
   ESP_LOGV( TAG, "wifi_settings: dhcp_server:     "IPSTR,  IP2STR( &Wifi_Dhcp_Cfg.Server  ) );
   ESP_LOGV( TAG, "wifi_settings: dhcp_ip_addr:    "IPSTR,  IP2STR( &Wifi_Dhcp_Cfg.Address ) );
   ESP_LOGV( TAG, "wifi_settings: dhcp_gw_addr:    "IPSTR,  IP2STR( &Wifi_Dhcp_Cfg.Gateway ) );
   ESP_LOGV( TAG, "wifi_settings: dhcp_netmask:    "IPSTR"\r\n", IP2STR( &Wifi_Dhcp_Cfg.Netmask ) );
#endif
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR wifiSoftApConfigDump( struct softap_config *ap_config )
{
#ifndef NDEBUG
   ESP_LOGV( TAG, "ap config - ssid:            \"%s\"", ap_config->ssid );
   ESP_LOGV( TAG, "ap config - password:        \"%s\"", ap_config->password );
   ESP_LOGV( TAG, "ap config - authmode:        %s",     auth2str( ap_config->authmode ) );
   ESP_LOGV( TAG, "ap config - channel:         %d",     ap_config->channel );
   ESP_LOGV( TAG, "ap config - hidden:          %s",     ap_config->ssid_hidden ? "yes" : "no" );
   ESP_LOGV( TAG, "ap config - max_connection:  %d",     ap_config->max_connection );
   ESP_LOGV( TAG, "ap config - beacon_interval: %d\r\n", ap_config->beacon_interval );
#endif
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR wifiStationConfigDump( struct station_config *sta_config )
{
#ifndef NDEBUG
   ESP_LOGV( TAG, "sta config - SSID of target AP:            \"%s\"",  sta_config->ssid );
   ESP_LOGV( TAG, "sta config - password of target AP:        \"%s\"",  sta_config->password );
   ESP_LOGV( TAG, "sta config - set MAC address of target AP: %s",      sta_config->bssid_set == 0 ? "no" : "yes" );
   ESP_LOGV( TAG, "sta config - MAC address of target AP:     " MACSTR, MAC2STR( sta_config->bssid ) );
   ESP_LOGV( TAG, "sta config - scan threshold rssi:          %d",      sta_config->threshold.rssi );
   ESP_LOGV( TAG, "                            security:      %s\r\n",  auth2str( sta_config->threshold.authmode )  );
#endif
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR wifiSaveNetworkId( char *ssid, char *password )
{
   ESP_LOGD( TAG, "wifiSaveNetworkId: ssid: \"%s\" pw: \"%s\"", S( ssid ), S( password ) );

   char saved_ssid[32];
   char saved_password[64];
   bool ap_saved = false;
   bool update_password = false;
   bool rc = false;   // AP is new and not saved

   last_known_ap_t lastKnown;

   lastKnown.val = config_get_int( Wlan_Known_AP );
   ESP_LOGD( TAG, "wifiSaveNetworkId: Last known networks: %d %d %d %d", lastKnown.list[ 0 ], lastKnown.list[ 1 ], lastKnown.list[ 2 ], lastKnown.list[ 3 ] );

   int i;
   for( i = 0; i < MAX_KNOWN_AP; i++ )
   {
      int index  = lastKnown.list[ i ];
      if( index != 0 )
      {
         index--;
         int ssid_id = Wlan_List + 2 * index;
         int password_id = Wlan_List + 2 * index + 1;

         // Q: what todo when ssid matches, but password not?
         // A: since we have a valid connection we can update the password for this ssid

         int len_saved_ssid = config_get( ssid_id, saved_ssid, sizeof( saved_ssid ) );
         int len_saved_password = config_get( password_id, saved_password, sizeof( saved_password ) );

         if( len_saved_ssid > 0 )
         {
            if( 0 == strncmp( ssid, saved_ssid, len_saved_ssid ) )
            {
               if( len_saved_password > 0 )
               {
                  if( 0 == strncmp( password, saved_password, len_saved_password ) )
                  {
                     // ssid, password already stored in known ap list
                     ESP_LOGD( TAG, "ssid \"%s\", pw \"%s\" already stored in known network list at pos %d",
                               S( saved_ssid ), S( saved_password ), index );
                     ap_saved = true;
                     rc = true;
                     break;
                  }
                  else
                  {
                     // ssid ok, password different
                     update_password = true;
                     rc = true;
                     break;
                  }
               }
            }
            else
            {
               // ssid doesn't match
               ESP_LOGD( TAG, "wifiSaveNetworkId: ssid doesn't match: ssid \"%s\" at pos %d.",
                         S( saved_ssid ), index );
            }
         }
         else
         {
            ESP_LOGD( TAG, "Setting %d or %d not set", ssid_id, password_id );
         }
      }
   }

   if( update_password )
   {
      // update only the password
      int index = lastKnown.list[ i ];
      if( index != 0 )
      {
         index--;
         int password_id = Wlan_List + 2 * index + 1;

         ESP_LOGD( TAG, "wifiSaveNetworkId: Save string @id: 0x%02x, str: \"%s\"", password_id, S( password ) );
         config_save_str( password_id, password, 0, Text );
      }
   }
   else if( !ap_saved )
   {
      ESP_LOGD( TAG, "wifiSaveNetworkId: ssid \"%s\", pw \"%s\" not in known network list", S( ssid ), S( password ) );

      // ssid, password are not in the list of the known APs
      // need to save current ssid, password to spi_flash

      // move lastKnown left
      lastKnown.val <<= 8;

      // search for a free entry in known AP list
      // since we moved the lastknown value left, the fist entry is zero, so we start with the second entry
      uint8_t mask = ( 1 << MAX_KNOWN_AP ) - 1;  // 0x0F
      // clear bits in mask for the ssid/pw entry at index is valid
      for( i = 1; i < MAX_KNOWN_AP; i++ )
      {
         int index  = lastKnown.list[ i ];
         if( index > 0 )
            mask &= ~( 1 << ( index - 1 ) );    // clear the bit

         ESP_LOGD( TAG, "wifiSaveNetworkId: %d Index %d mask 0x%1X", i, index, mask );
      }

      for( i = 0; i < MAX_KNOWN_AP; i++ )
      {
         if( mask & ( 1 << i ) )
         {
            ESP_LOGD( TAG, "wifiSaveNetworkId: Found free entry %d", i );

            int ssid_id = Wlan_List + 2 * i;
            int password_id = Wlan_List + 2 * i + 1;

            ESP_LOGD( TAG, "wifiSaveNetworkId: Save string @id: 0x%02x, str: \"%s\"", ssid_id,  S( ssid ) );
            ESP_LOGD( TAG, "wifiSaveNetworkId: Save string @id: 0x%02x, str: \"%s\"", password_id, S( password ) );
            config_save_str( ssid_id, ssid, 0, Text );
            config_save_str( password_id, password, 0, Text );

            // update Wlan_Known_AP
            lastKnown.val += ( ( i + 1 ) & 0xFF );
            ESP_LOGD( TAG, "wifiSaveNetworkId: Save number @id: 0x%02x, val: 0x%08x", Wlan_Known_AP, lastKnown.val );
            config_save_int( Wlan_Known_AP, lastKnown.val, Number );
            break;
         }
      }
   }

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// check if a scanned network is in the known networ list.
// return the index in the known network list and the password

static int ICACHE_FLASH_ATTR wifiIsKnownNetwork( char *ssid, char *password, int max_password_len )
{
   ESP_LOGD( TAG, "check \"%s\" is in known network list", ssid );

   char known_ssid[ 32 ];
   last_known_ap_t lastKnown;

   lastKnown.val = config_get_int( Wlan_Known_AP );
   ESP_LOGD( TAG, "Last known networks: %d %d %d %d", lastKnown.list[ 0 ], lastKnown.list[ 1 ], lastKnown.list[ 2 ], lastKnown.list[ 3 ] );

   for( int i = 0; i < MAX_KNOWN_AP; i++ )
   {
      int index  = lastKnown.list[ i ];
      if( index != 0 )
      {
         index--;
         int known_ssid_id = Wlan_List + 2 * index;
         int known_password_id = Wlan_List + 2 * index + 1;

         int known_ssid_len = config_get( known_ssid_id, known_ssid, sizeof( known_ssid ) );
         if( known_ssid_len < sizeof( known_ssid ) )
            known_ssid[ known_ssid_len ] = 0;

         if( 0 == strncmp( ssid, known_ssid, sizeof( known_ssid ) ) )
         {
            int known_password_len = config_get( known_password_id, password, max_password_len );
            if( known_password_len < max_password_len )
               password[ known_password_len ] = 0;

            ESP_LOGD( TAG, "found ssid \"%s\" in known network list at %d", known_ssid, index );
            return ++index;
         }
      }
   }

   return 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR wifiConnectToKnownNetwork( void )
{
   ESP_LOGD( TAG, "wifiConnectToKnownNetwork: try to connect to %d of %d networks", ap_index, ap_num  );

   // go thru the list of scanned network and check if there is one known, and connect to the known
   for( ; ap_index < ap_num; ap_index++ )
   {
      char password[64];

      int known_index = wifiIsKnownNetwork( ( char * ) ap_records[ ap_index ].ssid, password, sizeof( password ) );
      if( known_index > 0 && known_index_checked_mask & ( 1 << known_index ) )
      {
         // found a known net work, so try to connect to it
         ESP_LOGI( TAG, "Network \"%s\" is known", ( char * ) ap_records[ ap_index ].ssid );

         struct station_config sta_config;
         if( wifi_station_get_config( &sta_config ) )
         {
            sta_config.bssid_set = 0; // no need to check MAC address of AP
            strncpy( ( char * ) sta_config.ssid, ( char * ) ap_records[ ap_index ].ssid, sizeof( sta_config.ssid ) );
            strncpy( ( char * ) sta_config.password, password, sizeof( sta_config.password ) );

            ESP_LOGI( TAG, "Connect to network ssid \"%s\", pw \"%s\"", sta_config.ssid, sta_config.password );

            // wifi_station_disconnect();
            wifi_station_set_config( &sta_config );  // save to flash
            wifi_station_connect();

            // mark current known_index as checked for connection, by clear the bit
            known_index_checked_mask &= ~( 1 << ( known_index - 1 ) );    // clear the bit

            // Wait for connection
            os_timer_disarm( &connect_timeout );
            os_timer_setfn( &connect_timeout, ( os_timer_func_t * )wifiConnectionTimeout, NULL );
            os_timer_arm( &connect_timeout, CONNECT_TIMEOUT, 0 );

            return known_index;
         }
      }
   }

   ESP_LOGE( TAG, "No known network found" );

   // all networks from known network list checked,
   // but none of them are in the scanned network list
   // so setup an access point

   retry_connect_count = 0;
   if( ap_records != NULL )
      free( ap_records );
   ap_records = NULL;
   ap_num = 0;

   if( !( wifi_get_opmode() & SOFTAP_MODE ) )
   {
      wifiSetupSoftApMode();
   }

   return 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// get the list of APs found in the last scan
//
// Allocate memory for access point data
// Copy access point data to the ap_record struct
// print the list

static void ICACHE_FLASH_ATTR wifiScanDoneCb( void *arg, STATUS status )
{
   ESP_LOGD( TAG, "wifiScanDoneCb %d", status );
   HEAP_INFO( "" );

   struct bss_info *bss_link = ( struct bss_info * )arg;
   if( status == OK && bss_link != NULL )
   {
      // Count amount of access points found.
      ap_num = 0;
      while( bss_link != NULL )
      {
         bss_link = bss_link->next.stqe_next;
         ap_num++;
      }
      ESP_LOGD( TAG, "Scan done: found %d access points", ap_num );

      if( ap_records != NULL )
          free( ap_records );

      // Allocate memory for access point data
      ap_records = ( wifi_ap_record_t * )malloc( sizeof( wifi_ap_record_t ) * ap_num );
      if( ap_records == NULL )
      {
         ESP_LOGE( TAG, "Out of memory allocating apData" );
      }
      else
      {
         // Copy access point data to the ap_record struct
         int i = 0;
         bss_link = ( struct bss_info * )arg;
         while( bss_link != NULL )
         {
            if( i >= ap_num )
            {
               // This means the bss_link changed under our nose. Shouldn't happen!
               // Break because otherwise we will write in unallocated memory.
               ESP_LOGE( TAG, "Huh? I have more than the allocated %d aps!", ap_num );
               break;
            }
            // Save the ap data.
            ap_records[ i ].rssi = bss_link->rssi;
            ap_records[ i ].channel = bss_link->channel;
            ap_records[ i ].enc = bss_link->authmode;
            strncpy( ap_records[ i ].ssid, ( char* )bss_link->ssid, 32 );
            strncpy( ap_records[ i ].bssid, ( char* )bss_link->bssid, 6 );

            bss_link = bss_link->next.stqe_next;
            i++;
         }
         // We're done.

         // print the list
         ESP_LOGD( TAG, "Found %d access points:", ap_num );
         ESP_LOGD( TAG, "" );
         ESP_LOGD( TAG, "---------------------------------+---------+------+-------------" );
         ESP_LOGD( TAG, "               SSID              | Channel | RSSI |   Auth Mode " );
         ESP_LOGD( TAG, "---------------------------------+---------+------+-------------" );
         for( int i = 0; i < ap_num; i++ )
         {
            ESP_LOGD( TAG, "%32s | %7d | %4d | %12s",
                      ( char * )ap_records[i].ssid,
                      ap_records[i].channel,
                      ap_records[i].rssi,
                      auth2str( ap_records[i].enc ) );
         }
         ESP_LOGD( TAG, "---------------------------------+---------+------+-------------" );

         // get the first network which is in the known netwotk list and connect
         // if none found setup an access point
         ap_index = 0;  // start with first element
         known_index_checked_mask = ( 1 << MAX_KNOWN_AP ) - 1;
         wifiConnectToKnownNetwork();
      }
      HEAP_INFO( "" );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

//  see C:\Espressif\ESP8266_NONOS_SDK\include\user_interface.h( 440 ):

// +------------------------------------+--------------------------------------+--------------------+
// | Event                              | Structure                            | Field              |
// +------------------------------------+--------------------------------------+--------------------+
// | EVENT_STAMODE_CONNECTED            | Event_StaMode_Connected_t            | connected          |
// | EVENT_STAMODE_DISCONNECTED         | Event_StaMode_Disconnected_t         | disconnected       |
// | EVENT_STAMODE_AUTHMODE_CHANGE      | Event_StaMode_AuthMode_Change_t      | auth_change        |
// | EVENT_STAMODE_GOT_IP               | Event_StaMode_Got_IP_t               | got_ip             |
// | EVENT_STAMODE_DHCP_TIMEOUT         |                                      |                    |
// | EVENT_SOFTAPMODE_STACONNECTED      | Event_SoftAPMode_StaConnected_t      | sta_connected      |
// | EVENT_SOFTAPMODE_STADISCONNECTED   | Event_SoftAPMode_StaDisconnected_t   | sta_disconnected   |
// | EVENT_SOFTAPMODE_PROBEREQRECVED    | Event_SoftAPMode_ProbeReqRecved_t    | ap_probereqrecved  |
// | EVENT_OPMODE_CHANGED               | Event_OpMode_Change_t                | opmode_changed     |
// | EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP | Event_SoftAPMode_Distribute_Sta_IP_t | distribute_sta_ip  |
// +------------------------------------+--------------------------------------+--------------------+

static void ICACHE_FLASH_ATTR wifiEventHandlerCb( System_Event_t *event )
{
   if( event == NULL )
   {
      return;
   }

   switch( event->event )
   {
      case EVENT_STAMODE_CONNECTED:
      {
         Event_StaMode_Connected_t *connected = &event->event_info.connected;
         ESP_LOGD( TAG, "STAMODE_CONNECTED ssid:\"%s\", ssid_len:%d, bssid:" MACSTR ", channel:%d",
                   connected->ssid, connected->ssid_len, MAC2STR( connected->bssid ), connected->channel );

         break;
      }
      case EVENT_STAMODE_DISCONNECTED:
      {
         Event_StaMode_Disconnected_t *disconnected = &event->event_info.disconnected;
         ESP_LOGD( TAG, "STAMODE_DISCONNECTED reason %d: %s", disconnected->reason, reason2str( disconnected->reason ) );
         ESP_LOGD( TAG, "                     ssid:\"%s\", ssid_len:%d, bssid:" MACSTR,
                   disconnected->ssid, disconnected->ssid_len, MAC2STR( disconnected->bssid ) );

         httpdBroadcastStop();

         if( mqttWifiIsConnected() )
         {
            // disconnect mqtt server
            mqttWifiDisconnect();
         }

         // disconnect sntp server
         sntpStop();

         if( wifi_enable_reconnect )
         {
            // to the same ap, after some retries try to connect to one of the known aps
            // if also failed, setup an access point and/or go back to the first ap and
            // try if for a longer time

            wifiReconnect( disconnected->ssid, disconnected->ssid_len, disconnected->reason );
         }
         else
            wifi_enable_reconnect = true;
         break;
      }
      case EVENT_STAMODE_AUTHMODE_CHANGE:
      {
         Event_StaMode_AuthMode_Change_t *auth_change = &event->event_info.auth_change;
         ESP_LOGD( TAG, "STAMODE_AUTHMODE_CHANGE from %s to %s",
                   auth2str( auth_change->old_mode ), auth2str( auth_change->new_mode ) );
         break;
      }
      case EVENT_STAMODE_GOT_IP:
      {
         Event_StaMode_Got_IP_t *got_ip = &event->event_info.got_ip;
         ESP_LOGD( TAG, "STAMODE_GOT_IP ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
                   IP2STR( &got_ip->ip ), IP2STR( &got_ip->mask ), IP2STR( &got_ip->gw ) );

         if( wifiIsConnected() )
         {
            // we have a valid connection
            // get SSID, password and save it to the known AP list
            struct station_config sta_config;
            if( wifi_station_get_config( &sta_config ) )
            {
               wifiStationConfigDump( &sta_config );
               wifiSaveNetworkId( ( char * ) sta_config.ssid, ( char * ) sta_config.password );
            }

            httpdBroadcastStart();

            // connect to mqtt server
            if( !mqttWifiIsConnected() )
               mqttWifiConnect();

            // connect to sntp server
            sntpStart();
            led_set( SYS_LED, LED_BLINK,  15, 0, 0 );     // red led; long oneshot
            history( "Wifi\tconnected to \"%s\" got ip " IPSTR, sta_config.ssid, IP2STR( &got_ip->ip ) );
         }
         break;
      }
      case EVENT_STAMODE_DHCP_TIMEOUT:
      {
         ESP_LOGD( TAG, "STAMODE_DHCP_TIMEOUT" );
         break;
      }
      case EVENT_SOFTAPMODE_STACONNECTED:
      {
         Event_SoftAPMode_StaConnected_t *sta_connected = &event->event_info.sta_connected;
         ESP_LOGD( TAG, "SOFTAPMODE_STACONNECTED mac:" MACSTR ", aid:%d",
                   MAC2STR( sta_connected->mac ), sta_connected->aid );

         struct station_info *station = wifi_softap_get_station_info();
         while( station )
         {
            ESP_LOGD( TAG, "bssid: "MACSTR", ip: "IPSTR,
                      MAC2STR( station->bssid ), IP2STR( &station->ip ) );
            station = STAILQ_NEXT( station, next );
         }
         wifi_softap_free_station_info();
         break;
      }
      case EVENT_SOFTAPMODE_STADISCONNECTED:
      {
         Event_SoftAPMode_StaDisconnected_t *sta_disconnected = &event->event_info.sta_disconnected;
         ESP_LOGD( TAG, "SOFTAPMODE_STADISCONNECTED mac:" MACSTR ", aid:%d",
                   MAC2STR( sta_disconnected->mac ), sta_disconnected->aid );
         break;
      }
      case EVENT_SOFTAPMODE_PROBEREQRECVED:
      {
         static int msg_count = 10;
         Event_SoftAPMode_ProbeReqRecved_t *ap_probereqrecved = &event->event_info.ap_probereqrecved;
#if 0
         if( msg_count > 0 )
         {
            ESP_LOGD( TAG, "SOFTAPMODE_PROBEREQRECVED mac:" MACSTR ", rssi: %d",
                      MAC2STR( ap_probereqrecved->mac ), ap_probereqrecved->rssi );
            msg_count--;
         }
#endif
         break;
      }
      case EVENT_OPMODE_CHANGED:
      {
         Event_OpMode_Change_t *opmode_changed = &event->event_info.opmode_changed;
         ESP_LOGD( TAG, "OPMODE_CHANGED from %s to %s",
                   opmode2str( opmode_changed->old_opmode ), opmode2str( opmode_changed->new_opmode ) );
         break;
      }
      case EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
      {
         Event_SoftAPMode_Distribute_Sta_IP_t *distribute_sta_ip = &event->event_info.distribute_sta_ip;
         ESP_LOGD( TAG, "SOFTAPMODE_DISTRIBUTE_STA_IP ip:" IPSTR ", mac:" MACSTR ", aid:%d",
                   IP2STR( &distribute_sta_ip->ip ), MAC2STR( distribute_sta_ip->mac ), distribute_sta_ip->aid );
         break;
      }
      default:
         break;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// check if we get an ip with in 30 sec
// when we get no ip we can try to connect to an other ap from the known ip list
// or goto SoftAP mode and be an access point (fast blinking System & Info Led for 2 minutes)
// stop timeout when we get a connection ( STATION_GOT_IP or EVENT_SOFTAPMODE_STACONNECTED )

static void ICACHE_FLASH_ATTR wifiConnectionTimeout( void *arg )
{
   HEAP_INFO( "" );
   os_timer_disarm( &connect_timeout );      // stop timer

   if( wifiIsConnected() )
   {
      // we have a valid connection
      ESP_LOGI( TAG, "We got an ip address, so we have a valid connection" );
      retry_connect_count = 0;
      if( ap_records != NULL )
          free( ap_records );
      ap_records = NULL;
      ap_num = 0;
   }
   else
   {
      // retry, if failed request WPS action or set to AP mode
      if( retry_connect_count < CONNECT_MAX_RETRY )
      {
         ESP_LOGD( TAG, "Wait for getting an ip address, retry again %d", retry_connect_count );
         retry_connect_count++;
         os_timer_setfn( &connect_timeout, ( os_timer_func_t * )wifiConnectionTimeout, NULL );
         os_timer_arm( &connect_timeout, CONNECT_TIMEOUT, 0 );
      }
      else
      {
         ESP_LOGE( TAG, "Cannot connect to an access point, give it up" );
         retry_connect_count = 0;

         if( ap_records == NULL )
         {
            // scan for accessible networks and connect to a known one
            // if there isn't a known network setup an access point and start it
            // also start WPS

            ESP_LOGD( TAG, "Start wifi_scan_ap" );
            led_set( SYS_LED, LED_BLINK,  3, 3, 0 );     // red led; fast blink
            wifi_station_scan( NULL, wifiScanDoneCb );
         }
         else
         {
            // get the next network which is in the known netwotk list and connect
            // if none found setup an access point
            led_set( SYS_LED, LED_BLINK,  1, 5, 0 );     // red led; fast flash
            wifiConnectToKnownNetwork();
         }
      }
   }
   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// check after 5 minutes if the access point isn't in use
// and the client (station) has a vaild connection to an external ap

static void ICACHE_FLASH_ATTR wifiApTimeout( void *arg )
{
   ESP_LOGD( TAG, "wifiApTimeout" );

   HEAP_INFO( "" );
   os_timer_disarm( &ap_timeout );           // stop timer

   // get number of clients connected to the ap
   int ap_num_stations = wifi_softap_get_station_num();
   if( ap_num_stations == 0 )
   {
      // no stations connected to the ap
      ESP_LOGD( TAG, "no stations connected to the ap" );
      // check if the client/station has a vaild connection to an external ap
      if( wifiIsConnected() )
      {
         // if so, stop softap mode
         ESP_LOGD( TAG, "have a vaild connection to an external ap, stop softap mode" );
         int opmode = wifi_get_opmode();
         wifi_set_opmode( ( opmode & ~SOFTAP_MODE ) & STATIONAP_MODE );
      }
      else
      {
         // increment count to keep ap active
         ap_num_stations++;
      }
   }

   if( ap_num_stations > 0 )
   {
      // keep acces point alive
      // also start scan for networks
      ESP_LOGD( TAG, "Start wifi_scan_ap" );
      led_set( SYS_LED, LED_BLINK,  3, 3, 0 );     // red led; fast blink
      wifi_station_scan( NULL, wifiScanDoneCb );

      // restart timer
      ESP_LOGD( TAG, "restart timer" );
      os_timer_setfn( &ap_timeout, ( os_timer_func_t * )wifiApTimeout, NULL );
      os_timer_arm( &ap_timeout, AP_TIMEOUT, 0 ); // 5 min
   }

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// check the connection status

static int ICACHE_FLASH_ATTR wifiIsConnected( void )
{
   // ESP_LOGD( TAG, "wifiIsConnected ..." );

   uint8_t connect = false;
   uint8_t connect_status = wifi_station_get_connect_status();
   switch( connect_status )
   {
      case STATION_GOT_IP:
      {
         struct ip_info ipconfig;
         wifi_get_ip_info( STATION_IF, &ipconfig );
         if( ipconfig.ip.addr != 0 )
         {
            ESP_LOGI( TAG, "WiFi connected to "IPSTR, IP2STR( &ipconfig.ip.addr ) );
            connect = true;
         }
         break;
      }
      // handling the following event is not very usefull
      // we stupid do the same again - and wil get the same errors
      // we can do it better - later
      case STATION_WRONG_PASSWORD:
         ESP_LOGW( TAG, "WiFi connecting error, wrong password" );
         break;

      case STATION_NO_AP_FOUND:
         ESP_LOGW( TAG, "WiFi connecting error, ap not found" );
         break;

      case STATION_CONNECT_FAIL:
         ESP_LOGW( TAG, "WiFi connecting fail" );
         break;

      case STATION_IDLE:
      default:
         // ESP_LOGD( TAG, "WiFi connecting..." );
         break;
   }

   return connect;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// setup for be an access point ( AP mode )
// from F:\Projects\Evaluation\ESP8266\Firmware\DevKit-Examples\ESP8266\wifi-sta-tcp-client\user\user_main.c

int ICACHE_FLASH_ATTR wifiSetupSoftApMode( void )
{
   ESP_LOGD( TAG, "wifiSetupSoftApMode" );

   struct softap_config ap_config;
   uint8_t opmode;
   int rc = true;

   opmode = wifi_get_opmode();
   wifi_set_opmode( ( opmode | SOFTAP_MODE ) & STATIONAP_MODE );  // keep station mode if set

   // no need to get the current configuration, we setup the fields with the configuration
   os_memset( ap_config.ssid, 0, sizeof( ap_config.ssid ) );
   os_memset( ap_config.password, 0, sizeof( ap_config.password ) );

   strncpy( ( char* )ap_config.ssid, Wifi_Ap_Cfg.Ssid, sizeof( ap_config.ssid ) );
   strncpy( ( char* )ap_config.password, Wifi_Ap_Cfg.Password, sizeof( ap_config.password ) );
   ap_config.ssid_len         = 0;
   ap_config.channel          = Wifi_Ap_Cfg.Channel;
   ap_config.authmode         = Wifi_Ap_Cfg.Authmode;
   ap_config.ssid_hidden      = Wifi_Ap_Cfg.SsidHidden;
   ap_config.max_connection   = Wifi_Ap_Cfg.MaxConnection;
   ap_config.beacon_interval  = Wifi_Ap_Cfg.BeaconInterval;

   if( !wifi_softap_set_config( &ap_config ) )
   {
      ESP_LOGW( TAG, "ESP8266 cannot set AP config!" );
      rc = false;
   }

   ESP_LOGD( TAG, "ESP8266 in AP mode configured." );
   wifiSoftApConfigDump( &ap_config );

   // stop DHCP server
   wifi_softap_dhcps_stop();
   ESP_LOGD( TAG, "- DHCP server stopped" );

   // assign a static IP to the AP network interface
   struct ip_info ap_info;
   wifi_get_ip_info( SOFTAP_IF, &ap_info );

   ap_info.ip.addr      = Wifi_Ap_Cfg.Address;
   ap_info.gw.addr      = Wifi_Ap_Cfg.Gateway;
   ap_info.netmask.addr = Wifi_Ap_Cfg.Netmask;

   wifi_set_ip_info( SOFTAP_IF, &ap_info );

   // cannot use ip4addr_ntoa() function, because it return for all parameter a pointer to the same buffer
   ESP_LOGD( TAG, "- TCP adapter configured with ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
             IP2STR( &ap_info.ip ),
             IP2STR( &ap_info.netmask ),
             IP2STR( &ap_info.gw ) );

   // start dhcp server
   wifi_softap_dhcps_start();
   ESP_LOGD( TAG, "- DHCP server started" );

   // check after 5 minutes if the access point isn't in use
   // and the client (station) has a vaild connection to an external ap
   os_timer_disarm( &ap_timeout );
   os_timer_setfn( &ap_timeout, ( os_timer_func_t * )wifiApTimeout, NULL );
   os_timer_arm( &ap_timeout, AP_TIMEOUT, 0 ); // 5 min

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR wifiSetupStationMode( void )
{
   ESP_LOGD( TAG, "wifiSetupStationMode" );

   struct station_config sta_config;
   uint8_t opmode;
   int ssid_valid = false;

   // if softAP mode is set, then mode is set to station + softAP
   opmode = wifi_get_opmode();
   wifi_set_opmode( ( opmode | STATION_MODE ) & STATIONAP_MODE );

   // set hostname for the station
   ESP_LOGD( TAG, "Hostname: %s", wifi_station_get_hostname() );
   wifi_station_set_hostname( Wifi_Client_Cfg.Name );
   ESP_LOGI( TAG, "New hostname: %s", wifi_station_get_hostname() );

   if( wifi_station_get_auto_connect() == 1 )
   {
      ESP_LOGI( TAG, "Disable auto connection at power on"  );
      wifi_station_set_auto_connect( 0 );
   }

   wifi_station_set_reconnect_policy( false );  // don't reconnect after connection lost
   wifiEnableReconnect( true );

   if( wifi_station_get_config_default( &sta_config ) )  // Get Wi-Fi Station’s configuration from flash memory.
   {
      ESP_LOGD( TAG, "STA configuration from flash." );
      wifiStationConfigDump( &sta_config );

      if( sta_config.ssid != NULL && sta_config.ssid[ 0 ] != '\0' )
      {
         // we have a valid ssid
         ssid_valid = true;
      }
   }

   if( !ssid_valid )
   {
      // default station configuration is not valid
      ESP_LOGD( TAG, "Default station configuration is not valid." );

      // request network scan or setup an access point?
      // setup up an access point later.
      // don't connect to the network ==> no check for connection

      // no need to get the current configuration, we clear all fields
      // Does this make sense?
      os_memset( sta_config.ssid, 0, sizeof( sta_config.ssid ) );
      os_memset( sta_config.password, 0, sizeof( sta_config.password ) );

      sta_config.bssid_set = 0; // no need to check MAC address of AP
      sta_config.threshold.rssi = 0;
      sta_config.threshold.authmode = 0;

      if( !wifi_station_set_config_current( &sta_config ) )  //  this will connect to router automatically
      {
         ESP_LOGW( TAG, "ESP8266 cannot set station config!" );
      }
   }

   if( Wifi_Sta_Cfg.StaticIp )
   {
      // cannot use ip4addr_ntoa() function, because it return for all parameter the a pointer to the same buffer
      ESP_LOGD( TAG, "assigning static ip to STA interface. ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
                IP2STR( &Wifi_Sta_Cfg.Address ),
                IP2STR( &Wifi_Sta_Cfg.Netmask ),
                IP2STR( &Wifi_Sta_Cfg.Gateway ) );

      // stop DHCP client
      wifi_station_dhcpc_stop();
      ESP_LOGI( TAG, "- DHCP client stopped" );

      // assign a static IP to the STA network interface
      struct ip_info sta_info;
      os_memset( &sta_info, 0, sizeof( sta_info ) );
      sta_info.ip.addr      = Wifi_Sta_Cfg.Address;
      sta_info.gw.addr      = Wifi_Sta_Cfg.Gateway;
      sta_info.netmask.addr = Wifi_Sta_Cfg.Netmask;
      wifi_set_ip_info( STATION_IF, &sta_info );
   }
   else
   {
      // start DHCP client if not started
      enum dhcp_status dhcp_status;
      dhcp_status = wifi_station_dhcpc_status();
      if( dhcp_status != DHCP_STARTED )
      {
         wifi_station_dhcpc_start();
         ESP_LOGI( TAG, "- DHCP client started" );
      }
   }

   return ssid_valid;
}

// --------------------------------------------------------------------------
// WPS callback
// --------------------------------------------------------------------------

// here we need a retry mechanism. After fail change to softAP mode or
// use buildin station configuration.

enum wps_rc
{
   WPS_FAILED,
   WPS_RETRY,
   WPS_OK
};

#define WPS_MAX_RETRY        ( 4 )
static int wps_retry_count = 0;


static void ICACHE_FLASH_ATTR wifiWpsStatusCb( int status )
{
   ESP_LOGD( TAG, "wifiWpsStatusCb: %d", status );

   enum wps_rc rc = WPS_FAILED;

   switch( status )
   {
      case WPS_CB_ST_SUCCESS:
         ESP_LOGD( TAG, "wps SUCCESS" );
         struct station_config sta_config;
         wifi_station_get_config( &sta_config );
         if( strlen( sta_config.ssid ) > 0 )
         {
            // WPS config has already connected in STA mode successfully to the new station.
            ESP_LOGD( TAG, "WPS finished. Connected successfull to SSID '%s' with '%s'", sta_config.ssid, sta_config.password );

            if( !wifi_wps_disable() )
            {
               ESP_LOGE( TAG, "wps disable failed" );
            }
            wifi_station_connect();

            // Wait for connection
            os_timer_disarm( &connect_timeout );
            os_timer_setfn( &connect_timeout, ( os_timer_func_t * )wifiConnectionTimeout, NULL );
            os_timer_arm( &connect_timeout, CONNECT_TIMEOUT, 0 );

            rc = WPS_OK;
         }
         else
         {
            ESP_LOGD( TAG, "WPS finished. No valid SSID found" );
            rc = WPS_RETRY;
         }
         break;

      case WPS_CB_ST_FAILED:
         ESP_LOGE( TAG, "wps FAILED" );
         break;

      case WPS_CB_ST_TIMEOUT:
         ESP_LOGE( TAG, "wps TIMEOUT" );
         rc = WPS_RETRY;
         break;

      case WPS_CB_ST_WEP:           // WPS failed because that WEP is not supported
         ESP_LOGE( TAG, "wps WEP" );
         break;

      case WPS_CB_ST_SCAN_ERR:      // can not find the target WPS AP
         ESP_LOGE( TAG, "wps SCAN_ERR" );
         break;

      default:
         ESP_LOGE( TAG, "wps UNKNOWN: %d", status );
         break;
   }

   if( rc == WPS_RETRY )
   {
      if( wps_retry_count < WPS_MAX_RETRY )
      {
         ESP_LOGW( TAG, "wps retry %d", wps_retry_count );
         wifi_wps_start();
         wps_retry_count++;
      }
      else
      {
         ESP_LOGW( TAG, "wps give up" );
         wps_retry_count = 0;
         rc = WPS_FAILED;
      }
   }

   if( rc == WPS_FAILED )
   {
      if( !wifi_wps_disable() )
      {
         ESP_LOGE( TAG, "wps disable failed" );
      }
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR wifiWpsStart( void )
{
   ESP_LOGD( TAG, "wifiWpsStart" );

   // WPS works only in station mode, not in stationAP mode
   // WPS can only be used when ESP8266 Station is enabled
   wifi_set_opmode( STATION_MODE );

   // Disconnect from the network
   struct station_config sta_config;
   *sta_config.ssid = 0;
   *sta_config.password = 0;

   ETS_UART_INTR_DISABLE();
   wifi_station_set_config_current( &sta_config );
   wifi_station_disconnect();
   // check for disconnected from wifi
   ETS_UART_INTR_ENABLE();

   wifi_wps_disable();

   // so far only WPS_TYPE_PBC is supported ( SDK 1.2.0 )
   wifi_wps_enable( WPS_TYPE_PBC );
   wifi_set_wps_cb( wifiWpsStatusCb );
   wifi_wps_start();
   led_set( SYS_LED, LED_BLINK,  15, 15, 0 );     // red led; slow blink
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// to the same ap, after some retries try to connect to one of the known aps
// if also failed, setup an access point and/or go back to the first ap and
// try if for a longer time

static int ICACHE_FLASH_ATTR wifiReconnect( char *ssid, int ssid_len, int reason )
{
   ESP_LOGD( TAG, "wifiReconnect ..." );
   HEAP_INFO( "" );

   uint8_t opmode = wifi_get_opmode();

   if( opmode & STATION_MODE )
   {
      struct station_config sta_config;
      if( wifi_station_get_config( &sta_config ) )
      {
         // Reconnect the station to the WLAN
         ESP_LOGI( TAG, "Try to reconnect to %s ...", sta_config.ssid );
         wifi_station_connect();

         // Wait for connection
         os_timer_disarm( &connect_timeout );
         os_timer_setfn( &connect_timeout, ( os_timer_func_t * )wifiConnectionTimeout, NULL );
         os_timer_arm( &connect_timeout, CONNECT_TIMEOUT, 0 );
      }
      else
      {
         ESP_LOGE( TAG, "Cannot get station configuration" );
      }
   }
   else
   {
      ESP_LOGE( TAG, "Not in station mode" );
   }

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int  ICACHE_FLASH_ATTR wifiEnableReconnect( int enable )
{
   int rc = wifi_enable_reconnect;
   wifi_enable_reconnect = enable;
   return rc;
}

// --------------------------------------------------------------------------
// configure the wifi subsystem
// --------------------------------------------------------------------------

// setup station mode and connect to wifi with last saved credentials
// if not successful, retry it. If retry also failed then enable WPS or
// set to softAP mode to setup configuration manually

// initialize the tcp stack
// initialize the wifi event handler
// initialize wifi stack
//
// start as station and  access point
// configure the softAP and start it
// configure the station and start it
//
// try to connect to default wifi
// look for available networks, if found one setup ssid and password in wifi_config structure
// setup for be an access point ( AP mode )
// stop DHCP server
// assign a static IP to the AP network interface
// start dhcp server
//
// for static ip
//    stop DHCP client
//    assign a static IP to the STA network interface
// start DHCP client if not started
//
// connect with known networks

void ICACHE_FLASH_ATTR wifiInit( void )
{
   ESP_LOGD( TAG, "wifi_initialize ..." );
   HEAP_INFO( "" );

   getClientConfig();  // Name, Server, Username, Password, Port
   getSoftAPConfig();
   getWlanConfig();
   getStationConfig();
   getDhcpConfig();

   // wifiConfigDump();

   // set callback for event handler
   wifi_set_event_handler_cb( wifiEventHandlerCb );

   // initialize wifi stack
   uint8_t opmode = wifi_get_opmode();
   ESP_LOGI( TAG, "ESP8266 in %s mode configured at power on.", opmode2str( opmode ) );

   if( wifi_get_phy_mode() != PHY_MODE_11N )
      wifi_set_phy_mode( PHY_MODE_11N );

   // increase number of http connections
   int max_tcp_con = espconn_tcp_get_max_con();
   ESP_LOGD( TAG, "Max number of TCP clients: %d", max_tcp_con );
   if( max_tcp_con < TCP_MAX_CONNECTIONS )
   {
      espconn_tcp_set_max_con( TCP_MAX_CONNECTIONS );
      ESP_LOG( TAG, "Increase max number of TCP clients from %d to %d", max_tcp_con, espconn_tcp_get_max_con() );
   }

   // force to STATION_MODE at startup and save it to flash ( for testing)
   // wifi_set_opmode( STATION_MODE );

   //  set to station mode only when the ssid is valid
   int ssid_valid = wifiSetupStationMode();

   if( ssid_valid )
   {
      // Connect the station to the WLAN
      ESP_LOGI( TAG, "Wifi connect ..." );
      wifi_station_connect();
   }

   opmode = wifi_get_opmode();
   if( opmode & SOFTAP_MODE )
   {
      struct softap_config ap_config;
      if( wifi_softap_get_config_default( &ap_config ) )
      {
         ESP_LOGD( TAG, "AP configuration from flash." );
         wifiSoftApConfigDump( &ap_config );
      }

      // setup an access point from the configuration
      wifiSetupSoftApMode();
   }

   // Wait for connection
   os_timer_disarm( &connect_timeout );
   os_timer_setfn( &connect_timeout, ( os_timer_func_t * )wifiConnectionTimeout, NULL );
   os_timer_arm( &connect_timeout, CONNECT_TIMEOUT, 0 );

   ESP_LOGI( TAG, "Wifi started ...\r\n\r\n" );

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
