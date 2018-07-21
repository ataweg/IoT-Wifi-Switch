// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          sntp_client.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   add sntp_client_start(), sntp_client_stop()
//                      update sntp_client_init(), sntp_getFirstSync()
//    2018-04-23  AWe   revert changes from 2018-04-13
//    2018-04-13  AWe   limit the number to get a first time sync
//    2017-09-05  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "sntp_client";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>                // strlen()
#include <user_interface.h>       // system_get_time()
#include <mem.h>
#include <sntp.h>

#include "rtc.h"     // sntp_client uses the rtc as its local clock source
                     // rtc_getTime(), rtc_setTime(), rtc_getUptime(), rtc_getStartTime()
#include "sntp_client.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

/*
   3.13. SNTP APIs......................90
   3.13.1. sntp_setserver...............90
   3.13.2. sntp_getserver...............90
   3.13.3. sntp_setservername...........90
   3.13.4. sntp_getservername...........90
   3.13.5. sntp_init....................91
   3.13.6. sntp_stop....................91
   3.13.7. sntp_get_current_timestamp...91
   3.13.8. sntp_get_real_time ..........91
   3.13.9. sntp_set_timezone ...........91
   3.13.10.sntp_get_timezone............92
   3.13.11.SNTP Example.................92

void      sntp_setserver( unsigned char idx, ip_addr_t *addr );
ip_addr_t sntp_getserver( unsigned char idx );
void      sntp_setservername( unsigned char idx, char *server );
char     *sntp_getservername( unsigned char idx );
void      sntp_init( void );
void      sntp_stop( void );
uint32_t  sntp_get_current_timestamp();
char*     sntp_get_real_time( long t );
bool      sntp_set_timezone( sint8 timezone );
sint8     sntp_get_timezone( void );
*/

// --------------------------------------------------------------------------
// variables
// --------------------------------------------------------------------------

static sntp_client_t sntp_client;

// --------------------------------------------------------------------------
// functions
// --------------------------------------------------------------------------

// see https://electronicfreakblog.wordpress.com/2014/03/06/die-zeit-im-sommer-und-im-winter/

bool ICACHE_FLASH_ATTR isSummer( time_t timestamp )
{
   // create tm struct
   struct tm *dt = gmtime( &timestamp );

   uint8_t mon   = dt->tm_mon + 1;
   uint8_t mday  = dt->tm_mday;
   uint8_t wday  = dt->tm_wday;
   uint8_t hour  = dt->tm_hour;

   // no summer time from November to Februar
   if( mon > 10 || mon < 3 )
      return false;

   // summer time from April to September
   if( mon > 3 && mon < 10 )
      return true;

   // summer time starts on the last Sunday of March
   if( mon == 3 && ( mday - wday ) >= 25 && hour >= 2 )
      return true;

   // and ends on the last Sunday of October
   if( mon == 10 && ( mday - wday ) < 25 && hour < 3 )
      return true;

   return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR _sntp_get_real_time( time_t t )
{
   char *time_str = sntp_get_real_time( t );
   int len = strlen( time_str );

   if( time_str[ len - 1 ] == '\n' )
      time_str[ len  - 1 ] = 0;

   return time_str;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_client_init( char *time_servers[], int timezone, bool daylight )
{
   ESP_LOGD( TAG, "sntp_client_init ..." );
   sntp_stop();

   // memset( ( void * )&sntp_client, 0, sizeof( sntp_client ) );
   // setup the time when the rtc needs an sync update
   sntp_client.daylight = daylight;
   sntp_client.isSummer = 0;
   sntp_client.get_first_time = 0;
   sntp_client.timezone = timezone;
   sntp_client.syncInterval = DEFAULT_SYNC_INTERVAL;        // e.g. 10 sec
   sntp_client.updateInterval = DEFAULT_UPDATE_INTERVAL;    // e.g. 30 min
   sntp_client.nextSyncTime = 0;
   sntp_client.firstSyncTime = 0;
   sntp_client.lastSyncTime = 0;

   sntp_set_timezone( timezone );

   int i = 0;
   for( ; i < 3 && time_servers[i] != NULL; i++ )
   {
      sntp_setservername( i, time_servers[i] );
      ESP_LOGD( TAG, "set server %d: %s", i, time_servers[i] );
   }

   ESP_LOGD( TAG, "sntp_client_init done" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR sntp_client_start( void )
{
   ESP_LOGD( TAG, "sntp_client_start ..." );
   sntp_init();      // start sntp client
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR sntp_client_stop( void )
{
   ESP_LOGD( TAG, "sntp_client_stop ..." );
   sntp_stop();      // stop sntp client
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------


time_t ICACHE_FLASH_ATTR sntp_settime( time_t time )
{
   time_t new_time = rtc_setTime( time );

   if( sntp_client.eventHandler )
   {
      ESP_LOGD( TAG, "callback on settime" );
      sntp_client.eventHandler( sntp_timeUpdated, ( void * )new_time ); // curr_time will be the new_time
   }
   return new_time;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define SNTP_MAX_RETRY        2
static int retry_sntp_count = 0;

time_t ICACHE_FLASH_ATTR sntp_gettime( void )
{
   // ESP_LOGD( TAG, "sntp_gettime" );

   time_t rtc_time = rtc_getTime();
   time_t sntp_time = 0;

   if( rtc_time >= sntp_client.nextSyncTime || sntp_client.get_first_time )
   {
      if( wifi_station_get_connect_status() == STATION_GOT_IP )
      {
         ESP_LOGD( TAG, "Get time from sntp" );
         // get the number of seconds since 1.1.1970, timezone corrected
         sntp_time = sntp_get_current_timestamp();

         if( sntp_time != 0 )
         {
            if( sntp_client.daylight && isSummer( sntp_time ) )
            {
               sntp_time += TIMEZONE_CORRECTION;
               sntp_client.isSummer = true;
            }
            else
               sntp_client.isSummer = false;

            ESP_LOGD( TAG, "New timestamp: %s", _sntp_get_real_time( sntp_time ) );
            // sync the rtc to the internet time
            time_t new_time = sntp_settime( sntp_time );

            if( sntp_client.eventHandler )
            {
               ESP_LOGD( TAG, "callback on timeSyncd" );
               sntp_client.eventHandler( sntp_timeSyncd, ( void * )sntp_time ); // curr_time will be the new_time
            }

            if( !sntp_client.firstSyncTime )
            {
               ESP_LOGD( TAG, "Set first snc time: %s", _sntp_get_real_time( sntp_time ) );
               sntp_client.firstSyncTime = new_time;
            }
            sntp_client.get_first_time = 0;     // clear flag

            sntp_client.lastSyncTime = new_time;
            sntp_client.nextSyncTime = new_time + sntp_client.updateInterval;  // long interval (e.g. 30 min )
            ESP_LOGD( TAG, "Next sync %s", _sntp_get_real_time( sntp_client.nextSyncTime ) );

#if ( ( VERBOSITY ) & V_INFO )
            if( new_time != rtc_time )
            {
               char time_str_buf[32];
               char *time_str = _sntp_get_real_time( rtc_time );  // result includes line ending character
               strcpy( time_str_buf, time_str );
               ESP_LOGI( TAG, "Resync clock: correct time from %s to %s", time_str_buf, _sntp_get_real_time( new_time ) );
            }
#endif
            retry_sntp_count = 0;
            return new_time; // return time from the internet
         }
         else
         {
            if( retry_sntp_count < SNTP_MAX_RETRY || sntp_client.get_first_time )
               ESP_LOGD( TAG, "%3d: no valid timestamp from sntp server.", retry_sntp_count );

            if( sntp_client.eventHandler )
            {
               ESP_LOGD( TAG, "callback on noResponse" );
               sntp_client.eventHandler( sntp_noResponse, ( void * )rtc_time );
            }

            if( retry_sntp_count < SNTP_MAX_RETRY )
               retry_sntp_count++;

            if( retry_sntp_count == SNTP_MAX_RETRY )
            {
               sntp_client.nextSyncTime = rtc_time + sntp_client.updateInterval;    // long interval (e.g. 30 min )
               sntp_client.get_first_time = 0;
               retry_sntp_count = 0;
            }
            else
               sntp_client.nextSyncTime = rtc_time + sntp_client.syncInterval;      // short interval ( e.g. 10 sec )
         }
      }
      else
      {
         // ESP_LOGW( TAG,  "SNTP server not connected." );
         if( sntp_client.eventHandler )
         {
            ESP_LOGD( TAG, "callback on notConnected" );
            sntp_client.eventHandler( sntp_notConnected, ( void * )rtc_time );
         }
      }
   }

   // ESP_LOGD( TAG, "rtc time %d", rtc_time );
   return rtc_time; // return time taken from rtc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_setServerName( int idx, char *server )
{
   if( ( idx >= 0 ) && ( idx <= 2 ) )
   {
      sntp_stop();
      sntp_setservername( idx, server );
      sntp_init();      // restart sntp client
      return true;
   }
   return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR sntp_getServerName( int idx )
{
   if( ( idx >= 0 ) && ( idx <= 2 ) )
   {
      return sntp_getservername( ( unsigned char )idx );
   }
   return "";
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_setTimeZone( int timezone )
{
   time_t curr_timezone = sntp_get_timezone();
   time_t time_diff = timezone - curr_timezone;

   sntp_stop();
   bool result = sntp_set_timezone( curr_timezone );
   sntp_init();      // restart sntp client

   sntp_settime( rtc_getTime() + time_diff * 3600 );

   return result;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR sntp_getTimeZone( void )
{
   return sntp_get_timezone();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// get the current time from the rtc instead from the sntp
// check if summer time already set

void ICACHE_FLASH_ATTR sntp_setDayLight( bool daylight )
{
   // check if daylight setting has changed
   if( sntp_client.daylight != daylight )
   {
      // so we need to update the rtc
      time_t curr_time = rtc_getTime();

      sntp_client.daylight = daylight;

      if( daylight && !sntp_client.isSummer && isSummer( curr_time ) )
      {
         // need daylight update and summer time is not set
         curr_time += TIMEZONE_CORRECTION;
         sntp_client.isSummer = true;
      }
      else
      {
         // no daylight used, so also remove summer time
         // remove summer time when it is summer and the rtc is in summer time
         if( sntp_client.isSummer && isSummer( curr_time ) )
            curr_time -= TIMEZONE_CORRECTION;
         sntp_client.isSummer = false;
      }

      sntp_settime( curr_time );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_getDayLight( void )
{
   return sntp_client.daylight;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t ICACHE_FLASH_ATTR sntp_getUptime( void )
{
   return rtc_getUptime();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t ICACHE_FLASH_ATTR sntp_getLastBootTime( void )
{
   return rtc_getStartTime();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t ICACHE_FLASH_ATTR sntp_getLastSync( void )
{
   return sntp_client.lastSyncTime;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t ICACHE_FLASH_ATTR sntp_getFirstSync( void )
{
   ESP_LOGD( TAG, "sntp_getFirstSync" );

   sntp_client.get_first_time = 1;
   sntp_gettime();      // updates sntp_client.firstSyncTime

   if( !sntp_client.firstSyncTime )
   {
      ESP_LOGD( TAG, "cannot get a first time sync from time server." );
   }

   return sntp_client.firstSyncTime;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_setSyncInterval( int interval )
{
   if( interval >= MIN_SYNC_INTERVAL )
   {
      sntp_client.syncInterval = interval;
      return true;
   }
   else
      return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR sntp_getSyncInterval( void )
{
   return sntp_client.syncInterval;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR sntp_setUpdateInterval( int interval )
{
   if( interval >= MIN_UPDATE_INTERVAL )
   {
      sntp_client.updateInterval = interval;
      return true;
   }
   else
      return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR sntp_getUpdateInterval( void )
{
   return sntp_client.updateInterval;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR sntp_setEventHandler( appl_event_handler_t handler )
{
   sntp_client.eventHandler = handler;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if 0  // no sntp test
static os_timer_t sntp_timer;
void ICACHE_FLASH_ATTR sntp_check_stamp( void *arg );

void ICACHE_FLASH_ATTR sntp_test( void )
{
   // Step 1. Enable SNTP.
   sntp_stop();
   ip_addr_t *addr = ( ip_addr_t * )zalloc( sizeof( ip_addr_t ) );
   sntp_setservername( 0, "us.pool.ntp.org" ); // set server 0 by domain name
   sntp_setservername( 1, "ntp.sjtu.edu.cn" ); // set server 1 by domain name
   ipaddr_aton( "210.72.145.44", addr );
   sntp_setserver( 2, addr ); // set server 2 by IP address
   sntp_set_timezone( 0 );

   sntp_init();      // start sntp client

   free( addr );

   // Step 2. Set a timer to check SNTP timestamp.
   os_timer_disarm( &sntp_timer );
   os_timer_setfn( &sntp_timer, ( os_timer_func_t * )sntp_check_stamp, NULL );
   os_timer_arm( &sntp_timer, 2000, 0 );
}

void ICACHE_FLASH_ATTR sntp_check_stamp( void *arg )
{
   // Step 3. Timer Callback

   uint32_t sys_time = system_get_time();
   printf( "Timer Function 0 ( Time: %d us )\n", sys_time );

   if( wifi_station_get_connect_status() == STATION_GOT_IP )
   {
      uint32_t current_stamp;
      current_stamp = sntp_get_current_timestamp();
      if( current_stamp != 0 )
      {
         os_timer_disarm( &sntp_timer );
         printf( "sntp: %d, %s", current_stamp, sntp_get_real_time( current_stamp ) );

         printf( "sntp server name: %s\r\n", sntp_getservername( 0 ) );
         printf( "sntp server name: %s\r\n", sntp_getservername( 1 ) );
         printf( "sntp ip address : %d\r\n", sntp_getserver( 2 ) );
         printf( "snpt timezone   : %d\r\n", sntp_get_timezone() );

         os_timer_arm( &sntp_timer, 10000, 0 );
         return;
      }
   }

   os_timer_arm( &sntp_timer, 2000, 0 );
   printf( "wait for snpt time\r\n" );
}
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
