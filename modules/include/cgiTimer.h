// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiTimer.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-24  AWe   add support for WORKDAY and WEEKEND
//    2018-06-08  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __CGITIMER_H__
#define __CGITIMER_H__

#include "sntp_client.h"            // struct tm

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define DAILY     8
#define WORKDAY   9
#define WEEKEND  10
#define ONCE     11

#define SECONDS_PER_DAY    ( 24 * 3600 )

#define ID_HISTORY         0
#define ID_SWITCHTIME      1
#define ID_HOURGLASS       2

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
   uint8_t type;
   uint8_t val;
   uint8_t dmy;
   uint8_t id;
   time_t  time;
} switching_time_t;

typedef struct
{
   uint8_t type;
   uint8_t val;
   uint8_t dmy;
   uint8_t id;
   time_t  time;
   uint32_t addr;    // address in flash where the timer ist stored, simplifies erase
} switching_time_ext_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR timeToClock( time_t time );
char* ICACHE_FLASH_ATTR timeToDate( time_t time );
time_t ICACHE_FLASH_ATTR clockToTime( char * str );
time_t ICACHE_FLASH_ATTR dateToTime( char * str );

CgiStatus ICACHE_FLASH_ATTR tplTimer( HttpdConnData *connData, char *token, void **arg );
CgiStatus ICACHE_FLASH_ATTR cgiSetTimer( HttpdConnData *connData );
int ICACHE_FLASH_ATTR switchingTimeInit( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __CGITIMER_H__
