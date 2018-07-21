// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiTimer.c
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
static const char* TAG = "modules/cgiTimer.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <stdlib.h>  // strtol()

#include <osapi.h>
#include <user_interface.h>

#include "libesphttpd/httpd.h"
#include "configs.h"
#include "device.h"                 // devGet(), devSet();
#include "sntp_client.h"            // struct tm, sntp_settime
#include "cgiHistory.h"
#include "cgiTimer.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR timeToClock( time_t time );
char* ICACHE_FLASH_ATTR timeToDate( time_t time );
time_t ICACHE_FLASH_ATTR clockToTime( char * str );
time_t ICACHE_FLASH_ATTR dateToTime( char * str );

static int  ICACHE_FLASH_ATTR switchingTimeInsert( switching_time_ext_t *switching_time );
static int  ICACHE_FLASH_ATTR switchingTimeAdd( switching_time_ext_t *switching_time );
static int  ICACHE_FLASH_ATTR switchingTimeUpdate( int index );
static void ICACHE_FLASH_ATTR switchingTimeRemove( int index );
static void ICACHE_FLASH_ATTR switchingTimeDelete( int index );
static int  ICACHE_FLASH_ATTR switchingTimeGet( uint32_t *cfg_data, int len, uint32_t rd_addr, switching_time_ext_t *switching_time );
static void ICACHE_FLASH_ATTR switchingTimerCb( void *arg );
static void ICACHE_FLASH_ATTR switchingTimeUpdateCb( uint32_t event, void *arg, void *arg2 );
static int  ICACHE_FLASH_ATTR switchingTimeAdjust( switching_time_ext_t *switching_time, time_t time );
static void ICACHE_FLASH_ATTR switchingTimeSortList( switching_time_ext_t *switchingTime, int size ) ;

CgiStatus ICACHE_FLASH_ATTR tplTimer( HttpdConnData *connData, char *token, void **arg );
CgiStatus ICACHE_FLASH_ATTR cgiSetTimer( HttpdConnData *connData );
int  ICACHE_FLASH_ATTR switchingTimeInit( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define MAX_SWITCHING_TIMERS 32

static switching_time_ext_t switchingTime[ MAX_SWITCHING_TIMERS ];
static int num_switchingTimes = 0;
static os_timer_t switchingTimer;
static time_t last_hourglass = 0;
static int hourglass_index = 0;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR timeToClock( time_t time )
{
   static char buf[ 9 ];  // "hh:mm:ss"
   int bufsize = sizeof( buf );

   // create tm struct
   struct tm *dt = gmtime( &time );

   snprintf( buf, bufsize, "%02d:%02d:%02d",
      dt->tm_hour, dt->tm_min, dt->tm_sec );

   return buf;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR timeToDate( time_t time )
{
   static char buf[ 11 ];   // "jjjj:mm:dd"
   int bufsize = sizeof( buf );

   // create tm struct
   struct tm *dt = gmtime( &time );

   snprintf( buf, bufsize, "%02d.%02d.%04d",
      dt->tm_mday, dt->tm_mon + 1, dt->tm_year + 1900 );

   return buf;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// convert a string representing a clock value to an integer
// 15:30:00    normal form
// 15:30       short form
// 150000      integer form
// 1500        integer short form
// 15          integer hour form

// get number until we reached a delimiter. If the delimiter is ':' we have the
// normal form. When there is no delimiter we the integer form.
//   if there are at least 5 digits we have h:mm:ss
//   if there are at least 3 digits we have h:mm
//   if there are at least 1 digits we have h

time_t ICACHE_FLASH_ATTR clockToTime( char * str )
{
   // ESP_LOGD( TAG, "clockToTime: %s", S( str ) );

   time_t clock = 0;
   if( *str )
   {
      int hhmmss[ 3 ];
      size_t len = 0;
      int cnt = 0;
      struct tm t =
      {
         .tm_sec   =  0, // seconds after the minute   0-61*
         .tm_min   =  0, // minutes after the hour     0-59
         .tm_hour  =  0, // hours since midnight       0-23
         .tm_mday  =  1, // day of the month           1-31
         .tm_mon   =  0, // months since January       0-11
         .tm_year  = 70, // years since 1900
         .tm_wday  =  0, // days since Sunday          0-6
         .tm_yday  =  0, // days since January 1       0-365
         .tm_isdst =  0, // Daylight Saving Time flag
      };

      do
      {
         const char delims[] = ":";
         len = strcspn( str, delims );
         if( len > 0 )
         {
            // remove blanks
            while( *str == ' ' )
            {
               str++;
               len--;
            }
            // remove leading zeros, otherwise strtol treats string as octal number
            while( *str == '0' )
            {
               str++;
               len--;
            }

            hhmmss[ cnt ] = strtol( str, NULL, 0 );
            cnt++;
         }
         str += len;
      }
      while( *str++ );
      // ESP_LOGD( TAG, "hhmmss[ %d ] %d %d %d", cnt, hhmmss[ 0 ], hhmmss[ 1 ], hhmmss[ 2 ] );

      if( cnt == 1 )
      {
         int val = hhmmss[ 0 ];
         if( len >= 5 )
         {
            t.tm_hour = val / 10000;
            val %= 10000;
            t.tm_min  = val / 100;
            val %= 100;
            t.tm_sec = val;
         }
         else if( len >= 3 )
         {
            t.tm_hour = val / 100;
            val %= 100;
            t.tm_min  = val;
         }
         else if( len >= 1 )
         {
            t.tm_hour = val;
         }
      }
      else if( cnt == 2 )
      {
         t.tm_hour = hhmmss[ 0 ];
         t.tm_min  = hhmmss[ 1 ];
      }
      else if( cnt == 3 )
      {
         t.tm_hour = hhmmss[ 0 ];
         t.tm_min  = hhmmss[ 1 ];
         t.tm_sec  = hhmmss[ 2 ];
      }

      clock = mktime( &t );  // represents the local time
      // ESP_LOGD( TAG, "clockToTime %d:%d:%d -> %ld", t.tm_hour, t.tm_min, t.tm_sec, clock );
   }

   return clock;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// convert a string representing a date value to an integer
// 12.06.2018    normal form
// 1.1.18        normal short form
// 2018-01-01    alternative form
// 18-01-10      alternavie short form
// 20180101      integer form
// 180101        integer short form


// get number until we reached a delimiter. If the delimiter is a '-' we have the
// alternative form, if it is '.' we have the normal form. When there is no delimiter
// we the integer form.
// When the year is lower than 100, it is the short form and we have to 2000 to the year


time_t ICACHE_FLASH_ATTR dateToTime( char * str )
{
   // ESP_LOGD( TAG, "dateToTime: %s", S( str ) );

   time_t date = 0;
   if( *str )
   {
      int yymmdd[ 3 ];
      int form = 0;
      int cnt = 0;
      struct tm d =
      {
         .tm_sec   =  0, // seconds after the minute   0-61*
         .tm_min   =  0, // minutes after the hour     0-59
         .tm_hour  =  0, // hours since midnight       0-23
         .tm_mday  =  1, // day of the month           1-31
         .tm_mon   =  0, // months since January       0-11
         .tm_year  = 70, // years since 1900
         .tm_wday  =  0, // days since Sunday          0-6
         .tm_yday  =  0, // days since January 1       0-365
         .tm_isdst =  0, // Daylight Saving Time flag
      };

      do
      {
         const char delims[] = ".-";
         size_t len = strcspn( str, delims );
         if( len > 0 )
         {
            if( str[ len ] == '.' )
               form |= 1;
            else if( str[ len ] == '-' )
               form |= 2;

            // remove blanks
            while( *str == ' ' )
            {
               str++;
               len--;
            }
            // remove leading zeros, otherwise strtol treats string as octal number
            while( *str == '0' )
            {
               str++;
               len--;
            }

            yymmdd[ cnt ] = strtol( str, NULL, 0 );
            cnt++;
         }
         str += len;
      }
      while( *str++ );
      // ESP_LOGD( TAG, "yymmdd[ %d ]  %d %d %d form %d", cnt, yymmdd[ 0 ], yymmdd[ 1 ], yymmdd[ 2 ], form );

      if( form == 0 )
      {
         // integer form
         if( cnt == 1 )
         {
            int val = yymmdd[ 0 ];
            d.tm_year = val / 10000;
            val %= 10000;
            d.tm_mon  = val / 100 - 1;
            val %= 100;
            d.tm_mday = val;
         }
      }
      else if( form == 1 )
      {
         // normal form
         if( cnt == 3 )
         {
            d.tm_mday = yymmdd[ 0 ];
            d.tm_mon  = yymmdd[ 1 ] - 1;
            d.tm_year = yymmdd[ 2 ];
         }
      }
      else if( form == 2 )
      {
         // alternative form
         if( cnt == 3 )
         {
            d.tm_year = yymmdd[ 0 ];
            d.tm_mon  = yymmdd[ 1 ] - 1;
            d.tm_mday = yymmdd[ 2 ];
         }
      }

      if( d.tm_year < 100 )
         d.tm_year += 2000;
      d.tm_year -= 1900;

      date = mktime( &d );
      // ESP_LOGD( TAG, "dateToTime %d.%d.%d -> %ld", d.tm_mday, d.tm_mon + 1, d.tm_year + 1900, date );
   }

   return date;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR switchingTimeInsert( switching_time_ext_t *switching_time )
{
   // ESP_LOGD( TAG, "switchingTimeInsert 0x%08x", switching_time );

   if( num_switchingTimes >= MAX_SWITCHING_TIMERS )
   {
      // switching timer list is full
      ESP_LOGE( TAG, "switching timer list is full" );
      return -1;
   }

   int index = 0;
   for( ; index < num_switchingTimes; index++ )
   {
      if( switching_time->time < switchingTime[ index ].time )
      {
         // move list up
         // ESP_LOGD( TAG, "move up %d num %d, %d - %d", index, num_switchingTimes - index, switching_time->time, switchingTime[ index ].time );
         memmove( &switchingTime[ index + 1 ], &switchingTime[ index ], sizeof( switching_time_ext_t ) * ( num_switchingTimes - index ) );
         break;
      }
      else if( switching_time->time == switchingTime[ index ].time )
      {
         // overwrite exisiting time
         num_switchingTimes--;
         break;
      }
   }

   // ESP_LOGD( TAG, "store to index  %d", index );
   memcpy( &switchingTime[ index ], switching_time, sizeof( switching_time_ext_t ) );
   num_switchingTimes++;

   return index;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR switchingTimeAdd( switching_time_ext_t *switching_time )
{
   // ESP_LOGD( TAG, "switchingTimeAdd 0x%08x", switching_time );

   switching_time->id = ID_SWITCHTIME;
   // write new switchingTime to the user configuration section in the flash
   uint32_t wr_addr = (uint32_t )config_save_str( ID_EXTRA_DATA, (char *)switching_time, sizeof( switching_time_t ), Structure );

   // ESP_LOGD( TAG, "write to 0x%08x", wr_addr );
   switching_time->addr = wr_addr;

   return switchingTimeInsert( switching_time );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR switchingTimeUpdate( int index )
{
   // ESP_LOGD( TAG, "switchingTimeUpdate %d", index );

   // copy current to temp
   switching_time_ext_t switching_time;
   memcpy( &switching_time, &switchingTime[ index ], sizeof( switching_time_ext_t ) );

   // remove from list
   switchingTimeRemove( index );

   // insert to sorted list
   return switchingTimeInsert( &switching_time );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR switchingTimeRemove( int index )
{
   // ESP_LOGD( TAG, "switchingTimeRemove %d", index );

   // move list down
   num_switchingTimes--;
   if( index < num_switchingTimes )
   {
      // ESP_LOGD( TAG, "move down %d num %d", index, num_switchingTimes - index );
      memmove( &switchingTime[ index ], &switchingTime[ index + 1], sizeof( switching_time_ext_t ) * ( num_switchingTimes - index ) );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR switchingTimeDelete( int index )
{
   // ESP_LOGD( TAG, "switchingTimeDelete %d", index );

   switchingTime[ index ].type = 0;
   if( switchingTime[ index ].addr )
      user_config_invalidate( switchingTime[ index ].addr );

   // remove from list
   switchingTimeRemove( index );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// callback function for user_config_scan()

// rd_addr points to the value, or the begin of the data array or string
// cfg_data point to the begin of a two uint32_t array, the first hold the cfg_mode
// the second the value or the first for bytes of the data array or string

static int ICACHE_FLASH_ATTR switchingTimeGet( uint32_t *cfg_data, int len, uint32_t rd_addr, switching_time_ext_t *switching_time )
{
   ESP_LOGD( TAG, "switchingTimeGet from 0x%08x saved to 0x%08x", rd_addr, switching_time );

   cfg_mode_t *cfg_mode = ( cfg_mode_t * )cfg_data;  // not used here
   cfg_data++;

   memcpy( switching_time, cfg_data, sizeof( switching_time_ext_t ) );

   if( switching_time->id != ID_SWITCHTIME )
      return false;

   ESP_LOGD( TAG, "read switching time %d switch %s %s %s", switching_time->type,
                  timeToDate( switching_time->time ), timeToClock( switching_time->time ),
                  switching_time->val ? "ON" : "OFF" );
   switching_time->addr = rd_addr;

   // adjust time for the future
   time_t current_time = sntp_gettime();
   if( switching_time->time < current_time )
   {
      // time stored in the flash is in the past

      if( switching_time->type == ONCE )
      {
         // don't add to list, invalidate it in the flash
         user_config_invalidate( rd_addr );
         return true;
      }

      // adjust to current date
      switchingTimeAdjust( switching_time, current_time );
   }

   switchingTimeInsert( switching_time );
   return true;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR switchingTimerCb( void *arg )
{
   time_t current_time = sntp_gettime();

   for( int i = 0; i < num_switchingTimes; i++ )
   {
      if( switchingTime[ i ].type )
      {
         if( current_time >= switchingTime[ i ].time )
         {
            // switch
            devSet( Relay, switchingTime[ i ].val );
            if( switchingTime[ i ].id == ID_HOURGLASS )
            {
               history( "Hourglass\tSwitch %s", switchingTime[ i ].val ? "ON" : "OFF" );
               ESP_LOGI( TAG, "Hourglass: %s %s: switch to %s",
                               timeToDate( current_time ), timeToClock( current_time ),
                               switchingTime[ i ].val ? "ON" : "OFF" );
            }
            else
            {
               history( "Timer\tSwitch %s", switchingTime[ i ].val ? "ON" : "OFF" );
               ESP_LOGI( TAG, "Timer: %s %s: switch to %s",
                               timeToDate( current_time ), timeToClock( current_time ),
                               switchingTime[ i ].val ? "ON" : "OFF" );
            }
            ESP_LOGD( TAG, "(%d: %d.%d %s %s)",
                            i, switchingTime[ i ].type, switchingTime[ i ].id,
                            timeToDate( switchingTime[ i ].time ), timeToClock( switchingTime[ i ].time ) );

            if( switchingTime[ i ].type == ONCE )
            {
               // remove from list
               if( switchingTime[ i ].id == ID_SWITCHTIME )
               {
                  switchingTimeDelete( i );
               }
               else if( switchingTime[ i ].id == ID_HOURGLASS )
               {
                  switchingTimeRemove( i );
                  hourglass_index = 0;
               }
            }
            else
            {
               // advance to the next day
               // switchingTime[ i ].time += SECONDS_PER_DAY;
               // adjust to next switching day
               switchingTimeAdjust( &switchingTime[ i ], current_time += SECONDS_PER_DAY );
               // here we can sort in to a new position in the list
               switchingTimeUpdate( i );
            }

            // do only one switch
            break;
         }
      }
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Template code for the "Switching Timer" page.

// TimerList
// clock_date
// clock_time

CgiStatus ICACHE_FLASH_ATTR tplTimer( HttpdConnData *connData, char *token, void **arg )
{
   char buf[256];                // normally we need 233 bytes
   int bufsize = sizeof( buf );
   int buflen;

   if( token == NULL )
   {
      *arg = NULL;
      return HTTPD_CGI_DONE;
   }

   strcpy( buf, "Unknown [" );
   strcat( buf, token );
   strcat( buf, "]" );
   buflen = strlen( buf );

   int remaining = httpdSend( connData, NULL, 0 ); // get free spece in send bufffer

   if( strcmp( token, "TimerList" ) == 0 )
   {
      int alt_row = 0;

      // build row of available switching timers
      for( int i = ( int )(*arg); i < num_switchingTimes; i++ )
      {
         if( switchingTime[ i ].type )
         {
            alt_row++;
            time_t current_time = sntp_gettime();

            // only show future switching times
            if( switchingTime[ i ].time > current_time )
            {
               buflen = snprintf( buf, bufsize,
                           "<tr%s>\r\n"   // " class=\"alt\" "
                              "<td>"
                                 "%s"     // switchingTime[ i ].type
                              "</td>\r\n"
                              "<td>"
                                "%s"      // switchingTime[ i ].time (date)
                              "</td>\r\n"
                              "<td>"
                                "%s"      // switchingTime[ i ].tine (time)
                              "</td>\r\n"
                              "<td  align=center>"
                                "%s"      // switchingTime[ i ].val
                              "</td>\r\n"
                              "<td align=center>"
                                 "<input type=\"button\" onclick=\"self.location.href='settimer.cgi?delete=%d'\" value=\"Löschen\">"
                              "</td>\r\n"
                           "</tr>\r\n",
                           alt_row % 2 == 0 ? " class=\"alt\"" : "",
                           switchingTime[ i ].type == 11 ? "Einmalig"    : // "Once"
                           switchingTime[ i ].type ==  8 ? "Täglich"     : // "Daily"
                           switchingTime[ i ].type ==  9 ? "Arbeitstag"  : // "Workday"
                           switchingTime[ i ].type == 10 ? "Wochenende"  : // "Weekend"
                           switchingTime[ i ].type ==  1 ? "Montags"     : // "Mondays"
                           switchingTime[ i ].type ==  2 ? "Dienstags"   : // "Tuesdays"
                           switchingTime[ i ].type ==  3 ? "Mittwochs"   : // "Wednesdays"
                           switchingTime[ i ].type ==  4 ? "Donnerstags" : // "Thursdays
                           switchingTime[ i ].type ==  5 ? "Freitags"    : // "Friday"
                           switchingTime[ i ].type ==  6 ? "Samstags"    : // "Saturdays"
                           switchingTime[ i ].type ==  7 ? "Sonntags"    : // "Sundays"
                                              "unknown",
                           switchingTime[ i ].time > SECONDS_PER_DAY ? timeToDate( switchingTime[ i ].time ) : "",
                           timeToClock( switchingTime[ i ].time ),
                           switchingTime[ i ].val ? "ON" : "OFF",
                           i );

               // ESP_LOGD( TAG, "tplTimer %d: buflen %d, remain %d, %s\r\n%s", i, buflen, remaining, buflen < ( remaining - 1024 ) ? "continue" : "MORE", buf );
               if( buflen < ( remaining - 1024 ) )
               {
                  remaining = httpdSend( connData, buf, buflen );
                  *arg = (void *) ( i + 1 ); // next time process next line
               }
               else
               {
                  // discard current row and try it in the next round
                  if( i < num_switchingTimes )
                     return HTTPD_CGI_MORE;
               }
            }
         }
      }
      *arg = NULL;
      return HTTPD_CGI_DONE;
   }
   else if( strcmp( token, "clock_date" ) == 0 )
   {
      struct tm *dt;
      time_t current_time = sntp_gettime();
      dt = gmtime( &current_time );

      buflen = snprintf( buf, bufsize,
                            "%02d.%02d.%04d",       // dd.mm.yyyy
                            dt->tm_mday, dt->tm_mon + 1, dt->tm_year + 1900 );
   }
   else if( strcmp( token, "clock_time" ) == 0 )
   {
      struct tm *dt;
      time_t current_time = sntp_gettime();
      dt = gmtime( &current_time );

      buflen = snprintf( buf, bufsize,
                            "%02d:%02d:%02d",      // hh:mm:ss
                            dt->tm_hour, dt->tm_min, dt->tm_sec );
   }
   else if( strcmp( token, "hourglass" ) == 0 )
   {
      buflen = snprintf( buf, bufsize, "%s", timeToClock( last_hourglass ) );
   }

   // ESP_LOGD( TAG, "tplTimer : len %d, remain %d, %s\r\n%s", buflen, remaining, buflen < ( remaining - 1024 ) ? "DONE" : "MORE", buf );
   if( buflen < ( remaining - 1024 ) )
   {
      *arg = NULL;
      httpdSend( connData, buf, buflen );
      return HTTPD_CGI_DONE;
   }
   else
   {
      // discard current row and try it in the next round
      return HTTPD_CGI_MORE;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// settimer.cgi?type=4&date=14.06.2018&time=16:30&val=1
// settimer.cgi?delete=0
// settimer.cgi?clock_date=14.06.2018&clock_time=9:30

enum
{
  TOKEN_DELETE,         // 0: delete
  TOKEN_TPYE,           // 1: type
  TOKEN_DATE,           // 2: date
  TOKEN_TIME,           // 3: time
  TOKEN_VAL,            // 4: val
  TOKEN_CLOCK_DATE,     // 5: clock_date
  TOKEN_CLOCK_TIME,     // 6: clock_time
  TOKEN_HOURGLASS,      // 7: hourglass
  NUM_TOKEN             // 8
};

typedef char* STORE_ATTR Token_t;

const Token_t ICACHE_RODATA_ATTR token[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   "delete",
   "type",
   "date",
   "time",
   "val",
   "clock_date",
   "clock_time",
   "hourglass"
};


CgiStatus ICACHE_FLASH_ATTR cgiSetTimer( HttpdConnData *connData )
{
   // ESP_LOGD( TAG, "cgiSetTimer" );

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   int len;
   char buf[64];

   time_t clock_date = 0;
   time_t clock_time = 0;
   time_t hourglass = 0;

   switching_time_ext_t newSwitchingTime;
   memset( &newSwitchingTime, 0, sizeof( switching_time_ext_t ) );

   for( int i = 0; i < NUM_TOKEN; i++ )
   {
      len = httpdFindArg( connData->getArgs, token[ i ], buf, sizeof( buf ) );
      if( len > 0 )
      {
         if( i == TOKEN_DELETE )
         {
            ESP_LOGI( TAG, "delete: %s: %d, len %d", buf, atoi( buf ), len );

            // delete a switching time
            int index = atoi( buf );
            switchingTimeDelete( index );
         }
         else if( i == TOKEN_TPYE )
         {
            uint8_t type = atoi( buf );
            ESP_LOGI( TAG, "type: %s: %d, len %d", buf, type, len );
            newSwitchingTime.type = type;
         }
         else if( i == TOKEN_DATE )
         {
            time_t date = dateToTime( buf );
            ESP_LOGI( TAG, "date: %s: %ld, len %d", buf, date, len );
            newSwitchingTime.time += date;
         }
         else if( i == TOKEN_TIME )
         {
            time_t time = clockToTime( buf );
            ESP_LOGI( TAG, "time: %s: %ld, len %d", buf, time, len );
            newSwitchingTime.time += time;
         }
         else if( i == TOKEN_VAL )
         {
            uint8_t val = atoi( buf );
            ESP_LOGI( TAG, "val: %s: %d, len %d", buf, val, len );
            newSwitchingTime.val = val;
         }
         else if( i == TOKEN_CLOCK_DATE )
         {
            clock_date = dateToTime( buf );
            ESP_LOGI( TAG, "clock_date: %s: %ld, len %d", buf, clock_date, len );
         }
         else if( i == TOKEN_CLOCK_TIME )
         {
            clock_time = clockToTime( buf );
            ESP_LOGI( TAG, "clock_time: %s: %ld, len %d", buf, clock_time, len );
         }
         else if( i == TOKEN_HOURGLASS )
         {
            hourglass = clockToTime( buf );
            ESP_LOGI( TAG, "hourglass: %s: %ld, len %d", buf, clock_time, len );
         }
      }
   }

   // check parameter
   if( newSwitchingTime.type > 0 )
   {
      time_t current_time = sntp_gettime();
      // ESP_LOGD( TAG, "add a new switching time at %d at %d", newSwitchingTime.time, current_time );
      switchingTimeAdjust( &newSwitchingTime, current_time );

      if( newSwitchingTime.time > current_time )
      {
         int index = switchingTimeAdd( &newSwitchingTime );
         // ESP_LOGD( TAG, "added a new switching time at %d", index );
      }
   }

   if( clock_date != 0 || clock_time != 0 )
   {
      // set new system time
      sntp_settime( clock_date + clock_time );
#ifndef NDEBUG
      struct tm *dt;
      time_t current_time = sntp_gettime();
      dt = gmtime( &current_time );

      ESP_LOGI( TAG, "have set new time %02d.%02d.%04d %02d:%02d:%02d",           // dd.mm.yyyy hh:mm:ss
                            dt->tm_mday, dt->tm_mon + 1, dt->tm_year + 1900,
                            dt->tm_hour, dt->tm_min, dt->tm_sec );
#endif
   }

   if( hourglass > 0 )
   {
      if( hourglass_index > 0 )
      {
         // a hourglass job exists, so remove it from the list
         switchingTimeRemove( hourglass_index - 1);
      }

      time_t off_time = sntp_gettime() + hourglass;
      newSwitchingTime.id   = ID_HOURGLASS;
      newSwitchingTime.type = ONCE;
      newSwitchingTime.val  = 0;          // switch off
      newSwitchingTime.time = off_time;
      newSwitchingTime.addr = ( uint32_t )NULL;
      hourglass_index = switchingTimeInsert( &newSwitchingTime ) + 1;
      last_hourglass = hourglass;

      devSet( Relay, 1 );        // switch on
      history( "Hourglass\tSwitch ON" );
      ESP_LOGI( TAG, "switch ON" );
      ESP_LOGI( TAG, "switch off at %s %s", timeToDate( off_time ), timeToClock( off_time ) );
   }
   else if( hourglass < 0 )
   {
      devSet( Relay, 0 );        // switch off
      history( "Hourglass\tSwitch OFF" );
      ESP_LOGI( TAG, "switch OFF now" );

      if( hourglass_index > 0 )
      {
         // remove from list
         switchingTimeRemove( hourglass_index - 1);
      }
      hourglass_index = 0;
   }

   httpdRedirect( connData, "/Timer.tpl.html" );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// first map switching time to the current day
// then adjust to the day of type references
// ONCE switching time in the future should not pass this function,
//   they will map back to the current day.

static int ICACHE_FLASH_ATTR switchingTimeAdjust( switching_time_ext_t *switching_time, time_t timestamp )
{
   time_t c_day  = ( timestamp / SECONDS_PER_DAY ) * SECONDS_PER_DAY;   // current day
   time_t c_time = timestamp % SECONDS_PER_DAY;                         // current time
   time_t s_time = switching_time->time % SECONDS_PER_DAY;              // next switching time
   if( c_time > s_time )
   {
      c_day += SECONDS_PER_DAY; // do the job next day
   }

   switching_time->time = c_day + s_time;
   ESP_LOGD( TAG, "update switching time to %s %s ", timeToDate( switching_time->time ),
   timeToClock( switching_time->time ) );
   ESP_LOGD( TAG, "                         %d %d ", c_day, s_time );

   struct tm *dt = gmtime( &switching_time->time );
   int wday = dt->tm_wday;             // Sunday (0) .. Saturday (6)
   int next_wday;
   int n = 0;

   if( switching_time->type < DAILY )  // Monday (1) ... Sunday (7)
   {
      // adjust to switching day
      next_wday = switching_time->type % 7 ;  // map Sundays to 0
      n = next_wday - wday;
      if( n < 0 ) n += 7;
   }
   else if( switching_time->type == WORKDAY )   // Monday .. Friday
   {
      // adjust to workday
      n = wday == 6 ? 2 :    // Saturday     --> Monday
          wday == 0 ? 1 :    // Sunday       --> Monday
                      0;     // other days   --> current day
      next_wday = wday + n;
   }
   else if( switching_time->type == WEEKEND )   // Saturday .. Sunday
   {
      // adjust to weekend
      next_wday = wday == 6 ? 0 :     // Saturday   --> Saturday
                          0 ? 0 :     // Sunday     --> Sunday
                              6;      // other days --> Saturday
      n = next_wday - wday;
      if( n < 0 ) n += 7;
   }

   if( n != 0 )
   {
      switching_time->time += n * SECONDS_PER_DAY;
      ESP_LOGD( TAG, "adjust switching time to %s %s: c.wday: %d -> n.wday: %d by %d days",
                       timeToDate( switching_time->time ), timeToClock( switching_time->time ),
                       wday, next_wday, n );
   }
   return true;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// extented bubble sort algorithm

static void ICACHE_FLASH_ATTR switchingTimeSortList( switching_time_ext_t *switchingTime, int size )
{
   ESP_LOGD( TAG, "sort list with %d element", size );

   int n = size;
   int count = 0;

    do{
      int new_n = 1;
      for( int i = 0; i < n - 1; ++i )
      {
         if( switchingTime[ i ].time > switchingTime[ i + 1].time  )
         {
            // swap the elements
            ESP_LOGD( TAG, "swap the element %d with %d element; swap %d (%d) <-> (%d)", i , i + 1, count, switchingTime[ i ].time, switchingTime[ i + 1].time );
            switching_time_ext_t switchingTime_tmp;
            memcpy( &switchingTime_tmp, &switchingTime[ i ], sizeof( switching_time_ext_t ) );
            memcpy( &switchingTime[ i ], &switchingTime[ i + 1], sizeof( switching_time_ext_t ) );
            memcpy( &switchingTime[ i + 1 ], &switchingTime_tmp, sizeof( switching_time_ext_t ) );

            new_n = i + 1;
            count++;
         } // end if
       } // end for
       n = new_n;
   } while( n > 1 );

   ESP_LOGD( TAG, "list sorted after %d swaps",  count );
   for( int i = 0; i < num_switchingTimes; i++ )
   {
      ESP_LOGD( TAG, "                 %d: time %d  %s %s", i ,  switchingTime[ i ].time, timeToDate( switchingTime[ i ].time ), timeToClock( switchingTime[ i ].time ) );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void  ICACHE_FLASH_ATTR switchingTimeUpdateCb( uint32_t event, void *arg, void *arg2 )
{
   time_t timestamp = ( time_t )arg;
   ESP_LOGD( TAG, "switchingTimeUpdateCb %s %s num: %d", timeToDate( timestamp ), timeToClock( timestamp), num_switchingTimes );

   for( int i = 0; i < num_switchingTimes; i++ )
   {
      if( switchingTime[ i ].type )
      {
         // adjust time for the future
         ESP_LOGD( TAG, "                      %s %s idx: %d", timeToDate( switchingTime[ i ].time ), timeToClock( switchingTime[ i ].time), i );
         if( switchingTime[ i ].time < timestamp )
         {
            // time stored in the list is in the past
            ESP_LOGD( TAG, "                      need correction" );

            if( switchingTime->type == ONCE )
            {
               // remove from lst
            }
            else
            {
               // adjust to current date
               ESP_LOGD( TAG, "                      adjust" );
               switchingTimeAdjust( &switchingTime[ i ], timestamp );
//               // sort in to a new position in the list
//               switchingTimeUpdate( i );
            }
         }
      }
   }

   // sort the updated list
   // what to do with changes at the same time
   switchingTimeSortList( switchingTime, num_switchingTimes );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR switchingTimeInit( void )
{
   // ESP_LOGD( TAG, "switchingTimeInit" );

   // on startup first clear the switchingTimer array
   memset( switchingTime, 0, sizeof( switching_time_ext_t ) * MAX_SWITCHING_TIMERS );

   switching_time_ext_t switching_time;
   int rc = user_config_scan( ID_EXTRA_DATA, switchingTimeGet, &switching_time );

   appl_event_item_t* h_timeUpdated  = appl_addEventCb( sntp_timeUpdated, switchingTimeUpdateCb, NULL);

   os_timer_disarm( &switchingTimer );
   os_timer_setfn( &switchingTimer, switchingTimerCb, NULL );
   os_timer_arm( &switchingTimer, 1000, 1 );

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

