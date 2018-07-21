// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiHistory.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
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
static const char* TAG = "modules/cgiHistory.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include "esp_missing.h"            // ets_vsnprintf()
#include "libesphttpd/httpd.h"      // CgiStatus, malloc(), ...
#include "configs.h"                // struct cfg_mode_t
#include "cgiTimer.h"               // ID_HISTORY
#include "cgiHistory.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define SIZE_RINGBUF       32

typedef struct
{
    uint8_t start;
    uint8_t last;
    uint8_t overflow;
    uint16_t count;
    uint8_t index;
    uint8_t alt_row;
    uint32_t rd_addr_buf[ SIZE_RINGBUF ];
}
ringbuf_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static ringbuf_t* ringbuf = NULL;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR historyGet( uint32_t *cfg_data, int len, uint32_t rd_addr, history_t *history );
static uint32_t ICACHE_FLASH_ATTR saveHistory( char *str, int len );

CgiStatus ICACHE_FLASH_ATTR tplHistory( HttpdConnData *connData, char *token, void **arg );
int ICACHE_FLASH_ATTR history( const char *format, ... );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// callback function for user_config_scan()

// rd_addr points to the value, or the begin of the data array or string
// cfg_data point to the begin of a two uint32_t array, the first hold the cfg_mode
// the second the value or the first for bytes of the data array or string

static int ICACHE_FLASH_ATTR historyGet( uint32_t *cfg_data, int len, uint32_t rd_addr, history_t *history )
{
   // ESP_LOGD( TAG, "historyGet from 0x%08x saved to 0x%08x", rd_addr, history );

   cfg_mode_t *cfg_mode = ( cfg_mode_t * )cfg_data;  // not used here
   cfg_data++;

   memcpy( history, cfg_data, sizeof( history_t ) );

   if( history->id != ID_HISTORY )
      return false;

   time_t time = history->time;

   if( ringbuf != NULL )
   {
      ringbuf->rd_addr_buf[ ringbuf->last ] = rd_addr;
      // ESP_LOGD( TAG, "historyGet %d 0x%08x", ringbuf->last, rd_addr );
      ringbuf->count++;
      ringbuf->last++;
      if( ringbuf->last >= SIZE_RINGBUF )
      {
         ringbuf->last = 0;
         ringbuf->overflow = true;
      }
   }
   else
   {
      ESP_LOGE( TAG, "ringbuffer not initialized!" );
   }

   int history_len = history->len;
   char *history_msg = ( char * )cfg_data + sizeof( history_t );
   // ESP_LOGD( TAG, "historyGet %s %s %s len %d", timeToDate( time ), timeToClock( time ), history_msg, history_len );
   return true;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Template code for the "History" page.

// HistoryList

#define PRE_COL_SIZE  96  // 61 + 11 + 4 + 11 + 2

CgiStatus ICACHE_FLASH_ATTR tplHistory( HttpdConnData *connData, char *token, void **arg )
{
   char buf[ PRE_COL_SIZE + 256];
   int bufsize = sizeof( buf );
   int buflen;

   ringbuf = ( ringbuf_t* )*arg;

   if( token == NULL )
   {
      ESP_LOGD( TAG, "tplHistory clean up" );
      // clean up
      if( ringbuf )
         free( ringbuf );
      *arg = NULL;            // ringbuf
      return HTTPD_CGI_DONE;
   }

   strcpy( buf, "Unknown [" );
   strcat( buf, token );
   strcat( buf, "]" );
   buflen = strlen( buf );

   int remaining = httpdSend( connData, NULL, 0 ); // get free spece in send bufffer

   if( strcmp( token, "HistoryList" ) == 0 )
   {
      if( ringbuf == NULL )
      {
         // create a ring buffer and fill it
         ringbuf = ( ringbuf_t* )malloc( sizeof( ringbuf_t ) );
         if( ringbuf == NULL )
         {
            ESP_LOGE( TAG, "tplHistory cannot allocate memory" );
            return HTTPD_CGI_DONE;
         }
         *arg = ringbuf;
         memset( ringbuf, 0, sizeof( ringbuf_t ) );
         // get history messages from the flash
         history_t history;
         ESP_LOGD( TAG, "get history ..." );
         user_config_scan( ID_EXTRA_DATA_TEMP, historyGet, &history );
         ESP_LOGD( TAG, "get history done found %d entries", ringbuf->count );

         if( ringbuf-> overflow )
            ringbuf->start = ringbuf->last;

         ringbuf->index = ringbuf->last;
         ringbuf->alt_row = 0;
      }

      // ESP_LOGD( TAG, "get history count %d index %d start %d last %d", ringbuf->count, ringbuf->index, ringbuf->start, ringbuf->last );

      // send the lines of history from the ringbuf, newest first
      while( ( ( ringbuf->index != ringbuf->start ) || ringbuf-> overflow ) && ringbuf->count )
      {
         ringbuf->index--;
         // build date and time column
         ringbuf->alt_row++;
         history_t history;
         uint32_t addr = ringbuf->rd_addr_buf[ ringbuf->index ];
         user_config_read( addr, ( char * )&history, sizeof( history_t ) );
         buflen = snprintf( buf, PRE_COL_SIZE,   // 61 bytes
                     "<tr%s>\r\n"      // " class=\"alt\" "
                        "<td>"
                           "%d"        // count
                        "</td>\r\n"
                        "<td>"
                           "%s"        // date
                        "</td>\r\n"
                        "<td>"
                           "%s"        // time
                        "</td>\r\n"
                        "<td>",        // begin of next column
                     ringbuf->alt_row % 2 == 0 ? " class=\"alt\"" : "",
                     ringbuf->count,
                     timeToDate( history.time ), timeToClock( history.time ) );

         if( buflen > PRE_COL_SIZE )
         {
            ESP_LOGE( TAG, "too many bytes printed %d", buflen );
         }
         char *buf_end = buf + buflen;

         // get message and build their columns
         char *_buf = &buf[ PRE_COL_SIZE ];
         addr += sizeof( history_t );
         user_config_read( addr, _buf, history.len );

         while( *_buf )
         {
            if( *_buf == '\t' )
            {
               strcpy( buf_end,    // 11 bytes
                        "</td>\r\n"
                        "<td>" );
               while( *buf_end )    // get end of string
               {
                  buf_end++;
                  buflen++;
               }
               _buf++;
            }
            else
            {
               *buf_end++ = *_buf++;
               buflen++;
            }
         }

         // terminate column
         strcpy( buf_end,
                     "</td>\r\n"
                  "</tr>\r\n" );
         while( *buf_end )    // get length of string
         {
            buf_end++;
            buflen++;
         }

         // ESP_LOGD( TAG, "tplHistory ( %d ) %d: buflen %d, remain %d, %s\r\n%s", ringbuf->count, ringbuf->index, buflen, remaining, buflen < ( remaining - 1024 ) ? "continue" : "MORE", buf );
         if( buflen < ( remaining - 1024 ) )
         {
            remaining = httpdSend( connData, buf, buflen );

            if( ringbuf->index == 0 && ringbuf->overflow )
            {
               ringbuf->index = SIZE_RINGBUF;
               ringbuf->overflow = false;
            }
            ringbuf->count--;
         }
         else
         {
            // discard current row and try it again in the next round
            ringbuf->index++;
            ringbuf->alt_row--;
            return HTTPD_CGI_MORE;
         }
      }
      return HTTPD_CGI_DONE;
   }

   // in the case of an error here reaches
   // ESP_LOGD( TAG, "tplHistory : len %d, remain %d, %s\r\n%s", buflen, remaining, buflen < ( remaining - 1024 ) ? "DONE" : "MORE", buf );
   if( buflen < ( remaining - 1024 ) )
   {
      *arg = NULL;
      httpdSend( connData, buf, buflen );
      return HTTPD_CGI_DONE;
   }
   else
   {
      // discard current row and try it again in the next round
      return HTTPD_CGI_MORE;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static uint32_t ICACHE_FLASH_ATTR saveHistory( char *str, int len )
{
   // ESP_LOGD( TAG, "saveHistory '%s', len %d", S( str + sizeof( history_t ) ), len );

   history_t* new_msg = ( history_t* )str;

   if( len == 0 )
      len = strlen( str + sizeof( history_t ) );

   new_msg->id   = ID_HISTORY;
   new_msg->type = 0xFF;      // unused
   new_msg->dmy  = 0xFF;      // unused
   new_msg->len  = len + 1;   // save also terminating zero
   new_msg->time = sntp_gettime();

   uint32_t wr_addr = (uint32_t )config_save_str( ID_EXTRA_DATA_TEMP, (char *)new_msg, new_msg->len + sizeof( history_t ), Structure );

   return wr_addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR history( const char *format, ... )
{
   // ESP_LOGD( TAG, "history '%s'", S( format ) );

   int len;
   char buf[ 256 ];

   char *pBuf = buf + sizeof( history_t );
   size_t pSize = sizeof( buf ) - sizeof( history_t );

   va_list arglist;
   va_start( arglist, format );
   len = ets_vsnprintf( pBuf, pSize, format, arglist );
   va_end( arglist );

   // save message to flash
   // ESP_LOGD( TAG, "history '%s' %d of %d", S( pBuf ), len, pSize );
   ESP_LOG( TAG, "'%s'", S( pBuf ) );
   saveHistory( buf, len );

   return len;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
