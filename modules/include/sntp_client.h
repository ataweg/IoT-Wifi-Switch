// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          sntp-client.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   add prototypes for sntp_client_start(), sntp_client_stop()
//                      change bools to flags
//    2017-09-05  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __SNTP_CLIENT_H__
#define __SNTP_CLIENT_H__

#include <time.h>
#include "events.h"

#define TIMEZONE_CORRECTION         3600           // in seconds

#define MIN_SYNC_INTERVAL           1           // [s] repeat time to get the first timestamp from snpt server
#define MIN_UPDATE_INTERVAL         60          // [s] renew time when we have a timestamp
#define DEFAULT_SYNC_INTERVAL       10          // [s]
#define DEFAULT_UPDATE_INTERVAL   ( 60*30*1 )   // [s] update every three hours

#define now()             sntp_gettime()
#define getTime()         sntp_gettime()

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum
{
   sntp_timeSyncd  = 0x1000,    // Time successfully got from NTP server
   sntp_timeUpdated,            // local rtc successfully updated
   sntp_noResponse,             // No response from server
   sntp_notConnected
};

typedef struct
{
   bool     daylight: 1;         // Does this time zone have daylight saving?
   bool     isSummer: 1;
   bool     get_first_time: 1;
   uint8_t  timezone;
   uint32_t syncInterval;        // shorter time, to get a sync from the time server
   uint32_t updateInterval;      // longer time, to update the internal rtc with the tiime from the time server
   time_t   nextSyncTime;
   time_t   firstSyncTime;       // time of the first timestamp from sntp server
   time_t   lastSyncTime;        // time of the last timestamp from sntp server

   appl_event_handler_t eventHandler;      // Event handler callback
} sntp_client_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool   ICACHE_FLASH_ATTR isSummer( time_t timestamp );

char*  ICACHE_FLASH_ATTR _sntp_get_real_time( time_t t );

bool   ICACHE_FLASH_ATTR sntp_client_init( char *time_server[], int timezone, bool daylight );
void   ICACHE_FLASH_ATTR sntp_client_start( void );
void   ICACHE_FLASH_ATTR sntp_client_stop( void );
time_t ICACHE_FLASH_ATTR sntp_settime( time_t time );
time_t ICACHE_FLASH_ATTR sntp_gettime( void );
bool   ICACHE_FLASH_ATTR sntp_setServerName( int idx, char *server );
char*  ICACHE_FLASH_ATTR sntp_getServerName( int idx );
bool   ICACHE_FLASH_ATTR sntp_setTimeZone( int timeZone );
int    ICACHE_FLASH_ATTR sntp_getTimeZone( void );
void   ICACHE_FLASH_ATTR sntp_setDayLight( bool daylight );
bool   ICACHE_FLASH_ATTR sntp_getDayLight( void );
time_t ICACHE_FLASH_ATTR sntp_getUptime( void );
time_t ICACHE_FLASH_ATTR sntp_getLastBootTime( void );
time_t ICACHE_FLASH_ATTR sntp_getLastSync( void );
time_t ICACHE_FLASH_ATTR sntp_getFirstSync( void );
bool   ICACHE_FLASH_ATTR sntp_setSyncInterval( int interval );
int    ICACHE_FLASH_ATTR sntp_getSyncInterval( void );
bool   ICACHE_FLASH_ATTR sntp_setUpdateInterval( int tInterval );
int    ICACHE_FLASH_ATTR sntp_getUpdateInterval( void );
void   ICACHE_FLASH_ATTR sntp_setEventHandler( appl_event_handler_t handler );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif // __SNTP_CLIENT_H__
