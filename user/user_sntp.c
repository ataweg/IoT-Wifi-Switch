// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_sntp.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   update sntpFirstSyncTask()
//                      add sntpStart(), sntpStop()
//    2017-09-07  AWe   initial implementation
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
static const char *TAG = "user/user_sntp.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>
#include <sntp.h>                // sntp_get_real_time()

#include "sntp_client.h"
#include "user_sntp.h"
#include "rtc.h"
#include "user_mqtt.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR sntpFirstSyncTask( void );
static void ICACHE_FLASH_ATTR sntpTimerTask( void );


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------


static os_timer_t user_sntp_timer;

char *time_servers[] =
{
   "ptbtime2.ptb.de",
   "2.de.pool.ntp.org",
   "2.europe.pool.ntp.org",
   NULL
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR sntpTimerTask( void )
{
   // ESP_LOGD( TAG, "sntpTimerTask" );

   unsigned char msg[40];

   // create tm struct
   struct tm *dt;
   time_t timestamp = sntp_gettime();
   dt = gmtime( &timestamp );

   os_sprintf( msg, "%d:%02d:%02d", dt->tm_hour, dt->tm_min, dt->tm_sec );
   mqttPublish( &mqttClient, dev_Time, msg );

   os_sprintf( msg, "%d", system_adc_read() );
   mqttPublish( &mqttClient, dev_Adc, msg );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define GETSYNC_MAX_RETRY        8
static int retry_getsync_count = 0;

static void ICACHE_FLASH_ATTR sntpFirstSyncTask( void )
{
   ESP_LOGD( TAG, "sntpFirstSyncTask" );

   if( wifi_station_get_connect_status() == STATION_GOT_IP )
   {
      time_t first_sync_time = sntp_getFirstSync();
      if( first_sync_time != 0 )
      {
         ESP_LOGI( TAG, "first sntp sync: %s", _sntp_get_real_time( first_sync_time ) );
         retry_getsync_count = 0;

         // Set up a timer task to do a job every second
         os_timer_setfn( &user_sntp_timer, ( os_timer_func_t * )sntpTimerTask, NULL );
         os_timer_arm( &user_sntp_timer, 1000, 1 );
         return;
      }
   }

   // we got no time from the timer server, so retry it again every five seconds
   if( retry_getsync_count < GETSYNC_MAX_RETRY )
   {
      retry_getsync_count++;
      os_timer_disarm( &user_sntp_timer );

      if( retry_getsync_count == GETSYNC_MAX_RETRY )
      {
         ESP_LOGW( TAG, "Give up to get a first sync" );
      }
      else
      {
         int factor = 1 << ( retry_getsync_count / 2 );  // 1, 2, 2, 4, 4, 8, 8, 16, 16, 32
         ESP_LOGI( TAG, "%3d: wait %ds to get snpt time", retry_getsync_count, factor );
         os_timer_setfn( &user_sntp_timer, ( os_timer_func_t * )sntpFirstSyncTask, NULL );
         os_timer_arm( &user_sntp_timer, factor * 1000, 0 );  // try it again
      }
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static time_t new_timestamp;
static uint64_t last_clock;

void cb_timeSyncd( uint32_t event, void *arg, void *arg2 )
{
   ESP_LOGD( TAG, "cb_timeSyncd" );
   // Time successfully got from NTP server

   time_t timestamp = ( time_t )arg;
   new_timestamp = timestamp;          // timestamp is the time from the internet
   last_clock = rtc_time.clock;
   ESP_LOGD( TAG, "timeSyncd %u %u %u %lld %lld", system_get_time(), new_timestamp, timestamp, last_clock/1000, rtc_time.clock/1000 );
}

void cb_timeUpdated( uint32_t event, void *arg, void *arg2 )
{
   ESP_LOGD( TAG, "cb_timeUpdated" );
   // local rtc successfully updated
   time_t timestamp = ( time_t )arg;
   ESP_LOGD( TAG, "timeUpdated %u %u %u %lld %lld", system_get_time(), new_timestamp, timestamp, last_clock/1000, rtc_time.clock/1000 );
}

void cb_noResponse( uint32_t event, void *arg, void *arg2 )
{
   ESP_LOGD( TAG, "cb_noResponse" );
   // No response from server
   time_t timestamp = ( time_t )arg;
   ESP_LOGD( TAG, "noResponse %u %u %u %lld %lld", system_get_time(), new_timestamp, timestamp, last_clock/1000, rtc_time.clock/1000 );
}

void cb_notConnected( uint32_t event, void *arg, void *arg2 )
{
   ESP_LOGD( TAG, "notConnected" );
   time_t timestamp = ( time_t )arg;
   ESP_LOGD( TAG, "notConnected %u %u %u %lld %lld", system_get_time(), new_timestamp, timestamp, last_clock/1000, rtc_time.clock/1000 );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
// setup the sntp client
// start a timer. After its timeout check if we can get a timestamp

void ICACHE_FLASH_ATTR sntpInit( void )
{
   HEAP_INFO( "" );

   sntp_client_init( time_servers, 1, true ); // german timezone, daylight saving
   sntp_setEventHandler( appl_defaultEventHandler );

//   appl_event_item_t* h_timeSyncd    = appl_addEventCb( sntp_timeSyncd, cb_timeSyncd, NULL);
//   appl_event_item_t* h_timeUpdated  = appl_addEventCb( sntp_timeUpdated, cb_timeUpdated, NULL);
//   appl_event_item_t* h_noResponse   = appl_addEventCb( sntp_noResponse, cb_noResponse, NULL);
//   appl_event_item_t* h_notConnected = appl_addEventCb( sntp_notConnected, cb_notConnected, NULL);
//
//   appl_removeEventCb( sntp_notConnected );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR sntpStart( void )
{
   sntp_client_start();

   // Set up a timer to get the first snpt timestamp
   os_timer_disarm( &user_sntp_timer );
   os_timer_setfn( &user_sntp_timer, ( os_timer_func_t * )sntpFirstSyncTask, NULL );
   os_timer_arm( &user_sntp_timer, 500, 0 );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR sntpStop( void )
{
   sntp_client_stop();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

