// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          httpd.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-12  AWe   change return value of httpdSend and related function
//                      return value < 0 something goes wrong -1 out of memory
//                      otherwise return value gives the number of remaining free bytes in the send buffer
//    2018-04-19  AWe   for priv buffer and sendData buffer allocate memory from
//                        heap when needed. Give up to have buffers in the memory space.
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//                         here: replace ( connData->conn == NULL ) with ( connData->isConnectionClosed )
//    2018-01-19  AWe   replace httpd_printf() with ESP_LOG*()
//    2017-12-14  AWe   Switch from sprintf() to snprintf() to avoid a buffer overflow
//                         update to https://github.com/chmorgan/libesphttpd/commits/cmo_minify  commit e69f4b2 6 days ago
//    2017-11-23  AWe   httpdGetMimetype() - Add const to 'ext' parameter
//                      httpdProcessRequest() - Simplify routine using a temporary variable
//                      not merged: Implemented multipart template substitutions
//    2017-09-24  AWe   fix read of unalloc mem in wsockets, correct cause in cgiWebsocketClose()
//                      take over from
//                        https://github.com/MightyPork/libesphttpd/commit/58c81c0dfe8e15888408886e168ed739aa8f311d
//    2017-09-14  AWe   changed static files expiration to 2h
//                      take over from
//                        https://github.com/MightyPork/libesphttpd/commit/13fa224963081e9ff298abd74a59faafcb9bf816
//    2017-09-14  AWe   add support for 404 html page
//    2017-09-01  AWe   merge some changes from MigthyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/b804b196fca6abf28a7e2b5bd482b76b49d518f7
//                      don't takeover CORS and cgiArg2 support
//                        see Implemented multipart template substitutions
//
// Todo
//    2017-08-23        add HTTPS support
//    2017-08-23        for 404 error send content of 404.html file if it exists
// --------------------------------------------------------------------------

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Http server - core routines
*/

/* Copyright 2017 Jeroen Domburg <git@j0h.nl> */
/* Copyright 2017 Chris Morgan <chmorgan@gmail.com> */

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
static const char *TAG = "httpd";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

#ifdef ESP_PLATFORM
   #ifndef ASSERT
     #define ASSERT( msg, expr, ...)  \
       do { if( !( expr  )) \
         ESP_LOGE( TAG, "ASSERT line: %d: "msg, __LINE__,  ##__VA_ARGS__  ); } while(0)
   #endif
#endif

// #define NO_DBG_MEMLEAKS

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <strings.h>
#include <stdlib.h>  // atoi()

#ifdef linux
   #include <libesphttpd/linux.h>
#else
   #include <libesphttpd/esp.h>
#endif

#include "libesphttpd/httpd.h"
#include "libesphttpd/httpdespfs.h"   // serveStaticFile()
#include "httpd-platform.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This gets set at init time.
static const char *serverName = HTTPD_SERVERNAME;

// Flags
#define HFL_HTTP11          ( 1<<0 )
#define HFL_CHUNKED         ( 1<<1 )
#define HFL_SENDINGBODY     ( 1<<2 )
#define HFL_DISCONAFTERSENT ( 1<<3 )
#define HFL_NOCONNECTIONSTR ( 1<<4 )
#define HFL_NOCORS          ( 1<<5 )

// Struct to keep extension->mime data in
typedef struct
{
   const char *ext;
   const char *mimetype;
} MimeMap;


// #define RSTR( a ) ( ( const char )( a ) )

// The mappings from file extensions to mime types. If you need an extra mime type,
// add it here.
static const MimeMap mimeTypes[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   {"htm",  "text/html"},
   {"html", "text/html"},
   {"css",  "text/css"},
   {"js",   "text/javascript"},
   {"txt",  "text/plain"},
   {"csv",  "text/csv"},
   {"ico",  "image/x-icon"},
   {"jpg",  "image/jpeg"},
   {"jpeg", "image/jpeg"},
   {"png",  "image/png"},
   {"gif",  "image/gif"},
   {"bmp",  "image/bmp"},
   {"svg",  "image/svg+xml"},
   {"xml",  "text/xml"},
   {"json", "application/json"},
   {NULL,   "text/html"}, // default value
};

// Returns a static char* to a mime type for a given url to a file.
const char* ICACHE_FLASH_ATTR httpdGetMimetype( const char *url )
{
   char *urlp = ( char* )url;
   int i = 0;
   // Go find the extension
   const char *ext = urlp + ( strlen( urlp ) - 1 );
   while( ext != urlp && *ext != '.' ) ext--;
   if( *ext == '.' ) ext++;

   while( mimeTypes[i].ext != NULL && strcasecmp( ext, mimeTypes[i].ext ) != 0 ) i++;
   return mimeTypes[i].mimetype;
}

// --------------------------------------------------------------------------
// taken from MightyPork/libesphttpd
// --------------------------------------------------------------------------

const char* ICACHE_FLASH_ATTR httpdMethodName( RequestTypes m )
{
   switch( m )
   {
      default:
      case HTTPD_METHOD_GET:          return "GET";
      case HTTPD_METHOD_POST:         return "POST";
      case HTTPD_METHOD_OPTIONS:      return "OPTIONS";
      case HTTPD_METHOD_PUT:          return "PUT";
      case HTTPD_METHOD_DELETE:       return "DELETE";
      case HTTPD_METHOD_PATCH:        return "PATCH";
      case HTTPD_METHOD_HEAD:         return "HEAD";
   }
}

const char* ICACHE_FLASH_ATTR code2str( int code )
{
   switch( code )
   {
      case 200:         return "OK";
      case 301:         return "Moved Permanently";
      case 302:         return "Found";
      case 403:         return "Forbidden";
      case 400:         return "Bad Request";
      case 404:         return "Not Found";
      default:
         if( code >= 500 ) return "Server Error";
         if( code >= 400 ) return "Client Error";
         return "OK";
   }
}

/**
 * Add sensible cache control headers to avoid needless asset reloading
 *
 * @param connData
 * @param mime - mime type string
 */
void httpdAddCacheHeaders( HttpdConnData *connData, const char *mime )
{
   if( strcmp( mime, "text/html" ) == 0 ) return;
   if( strcmp( mime, "text/plain" ) == 0 ) return;
   if( strcmp( mime, "text/csv" ) == 0 ) return;
   if( strcmp( mime, "application/json" ) == 0 ) return;

   httpdHeader( connData, "Cache-Control", "max-age=7200, public, must-revalidate" );
}

const char* ICACHE_FLASH_ATTR httpdGetVersion( void )
{
   return HTTPDVER;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Retires a connection for re-use
static void ICACHE_FLASH_ATTR httpdRetireConn( HttpdInstance *pInstance, HttpdConnData *connData )
{
#ifdef CONFIG_ESPHTTPD_BACKLOG_SUPPORT
   if( connData->priv->sendBacklog != NULL )
   {
      HttpSendBacklogItem *i, *j;
      i = connData->priv->sendBacklog;
      do
      {
         j = i;
         i = i->next;
         free( j );
      }
      while( i != NULL );
   }
#endif

   if( connData->post.buf )
   {
      free( connData->post.buf );
      connData->post.buf = NULL;
   }

   if( connData->priv != NULL )
   {
      free( connData->priv );
      connData->priv = NULL;
   }
}

// Stupid li'l helper function that returns the value of a hex char.
static int ICACHE_FLASH_ATTR  httpdHexVal( char c )
{
   if( c >= '0' && c <= '9' ) return c - '0';
   if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
   if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
   return 0;
}

// Decode a percent-encoded value.
// Takes the valLen bytes stored in val, and converts it into at most retLen bytes that
// are stored in the ret buffer. Returns the actual amount of bytes used in ret. Also
// zero-terminates the ret buffer.

bool ICACHE_FLASH_ATTR httpdUrlDecode( const char *val, int valLen, char *ret, int retLen, int* bytesWritten )
{
   int s = 0; // index of theread position in val
   int d = 0; // index of the write position in 'ret'
   int esced = 0, escVal = 0;
   // d loops for ( retLen - 1 ) to ensure there is space for the null terminator
   while( s < valLen && d < ( retLen - 1 ) )
   {
      if( esced == 1 )
      {
         escVal = httpdHexVal( val[s] ) << 4;
         esced = 2;
      }
      else if( esced == 2 )
      {
         escVal += httpdHexVal( val[s] );
         ret[d++] = escVal;
         esced = 0;
      }
      else if( val[s] == '%' )
      {
         esced = 1;
      }
      else if( val[s] == '+' )
      {
         ret[d++] = ' ';
      }
      else
      {
         ret[d++] = val[s];
      }
      s++;
   }

   ret[d++] = 0;
   *bytesWritten = d;

   // if s == valLen then we processed all of the input bytes
   return ( s == valLen ) ? true : false;
}

// Find a specific arg in a string of get- or post-data.
// Line is the string of post/get-data, arg is the name of the value to find. The
// zero-terminated result is written in buf, with at most buffLen bytes used. The
// function returns the length of the result, or -1 if the value wasn't found. The
// returned string will be urldecoded already.

int ICACHE_FLASH_ATTR httpdFindArg( const char *line, const char *arg, char *buf, int buffLen )
{
   const char *p, *e;
   if( line == NULL ) return -1;
   const int arglen = strlen( arg );
   p = line;
   while( p != NULL && *p != '\n' && *p != '\r' && *p != 0 )
   {
      // ESP_LOGD( TAG, "findArg: %s", p );
      // check if line starts with "arg="
      if( strncmp( p, arg, arglen ) == 0 && p[arglen] == '=' )
      {
         p += arglen + 1; // move p to start of value
         e = strstr( p, "&" );
         if( e == NULL ) e = p + strlen( p );
         // ESP_LOGD( TAG, "findArg: val %s len %d", p, ( e-p ) );
         int bytesWritten;
         if( !httpdUrlDecode( p, ( e - p ), buf, buffLen, &bytesWritten ) )
         {
            // TODO: proper error return through this code path
            ESP_LOGE( TAG, "out of space storing url" );
         }
         return bytesWritten;
      }
      // line doesn't start with "arg=", so look for '&'
      p = strstr( p, "&" );
      if( p != NULL ) p += 1;
   }
   // ESP_LOGD( TAG, "Finding %s in %s: Not found", arg, line );
   return -1; // not found
}

// Get the value of a certain header in the HTTP client head
// Returns true when found, false when not found.

bool ICACHE_FLASH_ATTR httpdGetHeader( HttpdConnData *connData, const char *header, char *ret, int retLen )
{
   bool retval = false;

   char *p = connData->priv->head;
   p = p + strlen( p ) + 1; // skip GET/POST part
   p = p + strlen( p ) + 1; // skip HTTP part
   while( p < ( connData->priv->head + connData->priv->headPos ) )
   {
      while( *p <= 32 && *p != 0 ) p++; // skip crap at start
      // See if this is the header
      if( strncasecmp( p, header, strlen( header ) ) == 0 && p[strlen( header )] == ':' )
      {
         // Skip 'key:' bit of header line
         p = p + strlen( header ) + 1;
         // Skip past spaces after the colon
         while( *p == ' ' ) p++;
         // Copy from p to end
         // retLen check preserves one byte in ret so we can null terminate
         while( *p != 0 && *p != '\r' && *p != '\n' && retLen > 1 )
         {
            *ret++ = *p++;
            retLen--;
         }
         // Zero-terminate string
         *ret = 0;
         // All done : )
         retval = true;
      }
      p += strlen( p ) + 1; // Skip past end of string and \0 terminator
   }

   return retval;
}

void ICACHE_FLASH_ATTR httpdSetTransferMode( HttpdConnData *connData, TransferModes mode )
{
   if( mode == HTTPD_TRANSFER_CLOSE )
   {
      connData->priv->flags &= ~HFL_CHUNKED;
      connData->priv->flags &= ~HFL_NOCONNECTIONSTR;
   }
   else if( mode == HTTPD_TRANSFER_CHUNKED )
   {
      connData->priv->flags |= HFL_CHUNKED;
      connData->priv->flags &= ~HFL_NOCONNECTIONSTR;
   }
   else if( mode == HTTPD_TRANSFER_NONE )
   {
      connData->priv->flags &= ~HFL_CHUNKED;
      connData->priv->flags |= HFL_NOCONNECTIONSTR;
   }
}

void ICACHE_FLASH_ATTR httdResponseOptions( HttpdConnData *connData, int cors )
{
   if( cors == 0 )
      connData->priv->flags |= HFL_NOCORS;
}

// Start the response headers.
void ICACHE_FLASH_ATTR httpdStartResponse( HttpdConnData *connData, int code )
{
   char buf[128];
   int l;
   const char *connStr = "Connection: close\r\n";

   if( connData->priv->flags & HFL_CHUNKED ) connStr = "Transfer-Encoding: chunked\r\n";
   if( connData->priv->flags & HFL_NOCONNECTIONSTR ) connStr = "";
   l = snprintf( buf, sizeof( buf ), "HTTP/1.%d %d %s\r\nServer: %s\r\n%s",
                 ( connData->priv->flags & HFL_HTTP11 ) ? 1 : 0,
                 code,
                 code2str( code ),
                 serverName,
                 connStr );
   if( l >= sizeof( buf ) )
   {
      ESP_LOGE( TAG, "buf[%zu] too small", sizeof( buf ) );
   }
   httpdSend( connData, buf, l );

#ifdef CONFIG_ESPHTTPD_CORS_SUPPORT
   // CORS headers
   if( 0 == ( connData->priv->flags & HFL_NOCORS ) )
   {
      // CORS headers
      httpdSend( connData, "Access-Control-Allow-Origin: *\r\n", -1 );
      httpdSend( connData, "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n", -1 );
   }
#endif
}

// Send a http header.
void ICACHE_FLASH_ATTR httpdHeader( HttpdConnData *connData, const char *field, const char *val )
{
   httpdSend( connData, field, -1 );
   httpdSend( connData, ": ", -1 );
   httpdSend( connData, val, -1 );
   httpdSend( connData, "\r\n", -1 );
}

// Finish the headers.
void ICACHE_FLASH_ATTR httpdEndHeaders( HttpdConnData *connData )
{
   httpdSend( connData, "\r\n", -1 );
   connData->priv->flags |= HFL_SENDINGBODY;
}

// Redirect to the given URL.
void ICACHE_FLASH_ATTR httpdRedirect( HttpdConnData *connData, const char *newUrl )
{
   ESP_LOGD( TAG, "Redirecting to %s", newUrl );
   httpdStartResponse( connData, 302 );
   httpdHeader( connData, "Location", newUrl );
   httpdEndHeaders( connData );
   httpdSend( connData, "Moved to ", -1 );
   httpdSend( connData, newUrl, -1 );
}

// Used to spit out a 404 error
static CgiStatus ICACHE_FLASH_ATTR cgiNotFound( HttpdConnData *connData )
{
//   ESP_LOGD( TAG, "404 File not found" );
#if 1
   static bool have404Page = true;

   if( have404Page )
   {
      CgiStatus cgiState = serveStaticFile( connData, "/404.html", 404 );

      if( cgiState == HTTPD_CGI_NOTFOUND )
         have404Page = false;
      else
         return cgiState;
   }
#endif
   if( connData->isConnectionClosed ) return HTTPD_CGI_DONE;
   httpdStartResponse( connData, 404 );
   httpdHeader( connData, "Content-Type", "text/plain" );
   httpdEndHeaders( connData );
   httpdSend( connData, "404 File not found.", -1 );
   return HTTPD_CGI_DONE;
}

static const char* CHUNK_SIZE_TEXT = "0000\r\n";
static const int CHUNK_SIZE_TEXT_LEN = 6; // number of characters in CHUNK_SIZE_TEXT

// Add data to the send buffer. len is the length of the data. If len is -1
// the data is seen as a C-string. If len  is 0 return the number of remaining bytes
// in the send buffer
// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer

int ICACHE_FLASH_ATTR httpdSend( HttpdConnData *connData, const char *data, int len )
{
   // if( connData->isConnectionClosed ) return -1;
   if( len < 0 )
       len = strlen( data );

   if( len > 0 )
   {
      if( connData->priv->flags & HFL_CHUNKED && connData->priv->flags & HFL_SENDINGBODY && connData->priv->chunkHdr == NULL )
      {
         if( connData->priv->sendBuffLen + len + CHUNK_SIZE_TEXT_LEN > HTTPD_MAX_SENDBUFF_LEN )
         {
            ESP_LOGE( TAG, "httpdSend ( chrunked ): sendbuffer will overflow, discard data" );
            return -1;
         }

         // Establish start of chunk
         // Use a chunk length placeholder of 4 characters
         connData->priv->chunkHdr = &connData->priv->sendBuff[connData->priv->sendBuffLen];
         strcpy( connData->priv->chunkHdr, CHUNK_SIZE_TEXT );
         connData->priv->sendBuffLen += CHUNK_SIZE_TEXT_LEN;
         ASSERT( "sendBuffLen > HTTPD_MAX_SENDBUFF_LEN", connData->priv->sendBuffLen <= HTTPD_MAX_SENDBUFF_LEN );
      }
      if( connData->priv->sendBuffLen + len > HTTPD_MAX_SENDBUFF_LEN )
      {
         ESP_LOGE( TAG, "httpdSend: sendbuffer will overflow, discard data" );
         return -1;
      }
      memcpy( connData->priv->sendBuff + connData->priv->sendBuffLen, data, len );
      connData->priv->sendBuffLen += len;
   }

   if( connData->priv->flags & HFL_CHUNKED && connData->priv->flags & HFL_SENDINGBODY && connData->priv->chunkHdr == NULL )
   {
      ASSERT( "sendBuffLen > HTTPD_MAX_SENDBUFF_LEN", connData->priv->sendBuffLen + CHUNK_SIZE_TEXT_LEN <= HTTPD_MAX_SENDBUFF_LEN );
      return HTTPD_MAX_SENDBUFF_LEN - CHUNK_SIZE_TEXT_LEN - connData->priv->sendBuffLen;
   }
   else
   {
      ASSERT( "sendBuffLen > HTTPD_MAX_SENDBUFF_LEN", connData->priv->sendBuffLen <= HTTPD_MAX_SENDBUFF_LEN );
      return HTTPD_MAX_SENDBUFF_LEN - connData->priv->sendBuffLen;
   }
}

static char ICACHE_FLASH_ATTR httpdHexNibble( int val )
{
   val &= 0xf;
   if( val < 10 ) return '0' + val;
   return 'A' + ( val - 10 );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// encode for HTML
// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer

int ICACHE_FLASH_ATTR httpdSend_html( HttpdConnData *connData, const char *data, int len )
{
   int start = 0, end = 0;
   char c;
   int rc = 0;
   if( len < 0 )
      len = ( int ) strlen( data );

   if( len > 0 )
   {
      for( end = 0; end < len; end++ )
      {
         c = data[end];
         if( c == 0 )
         {
            // we found EOS
            break;
         }

         if( c == '"' || c == '\'' || c == '<' || c == '>' )
         {
            if( start < end )
               rc = httpdSend( connData, data + start, end - start );
            start = end + 1;

            if( rc <= 0 ) ;
            else if( c == '"' )  rc = httpdSend( connData, "&#34;", 5 );
            else if( c == '\'' ) rc = httpdSend( connData, "&#39;", 5 );
            else if( c == '<' )  rc = httpdSend( connData, "&lt;", 4 );
            else if( c == '>' )  rc = httpdSend( connData, "&gt;", 4 );

            if( rc <= 0 )
               break; // no more space in send buffer
         }
      }
   }

   if( ( rc > 0 && start <= end ) || len == 0 )
   {
      // if len or end - start is zero get only the remaining bytes in the send buffer
      rc = httpdSend( connData, data + start, end - start );
   }
   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// encode for HTML.
// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer

int ICACHE_FLASH_ATTR httpdSend_js( HttpdConnData *connData, const char *data, int len )
{
   int start = 0, end = 0;
   char c;
   int rc = 0;
   if( len < 0 )
      len = ( int ) strlen( data );

   if( len > 0 )
   {
      for( end = 0; end < len; end++ )
      {
         c = data[end];
         if( c == 0 )
         {
            // we found EOS
            break;
         }

         if( c == '"' || c == '\\' || c == '\'' || c == '<' || c == '>' || c == '\n' || c == '\r' )
         {
            if( start < end )
               rc = httpdSend( connData, data + start, end - start );
            start = end + 1;

            if( rc <= 0 ) ;
            else if( c == '"' )  rc = httpdSend( connData, "\\\"", 2 );
            else if( c == '\'' ) rc = httpdSend( connData, "\\'", 2 );
            else if( c == '\\' ) rc = httpdSend( connData, "\\\\", 2 );
            else if( c == '<' )  rc = httpdSend( connData, "\\u003C", 6 );
            else if( c == '>' )  rc = httpdSend( connData, "\\u003E", 6 );
            else if( c == '\n' ) rc = httpdSend( connData, "\\n", 2 );
            else if( c == '\r' ) rc = httpdSend( connData, "\\r", 2 );

            if( rc <= 0 )
               break; // no more space in send buffer
         }
      }
   }

   if( ( rc > 0 && start <= end ) || len == 0 )
   {
      // if len or end - start is zero get only the remaining bytes in the send buffer
      rc = httpdSend( connData, data + start, end - start );
   }
   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Function to send any data in connData->priv->sendBuff. Do not use in CGIs unless you know what you
// are doing! Also, if you do set connData->cgi to NULL to indicate the connection is closed, do it BEFORE
// calling this.

bool ICACHE_FLASH_ATTR httpdFlushSendBuffer( HttpdInstance *pInstance, HttpdConnData *connData )
{
   int r, len;
   // if( connData->isConnectionClosed ) return false;
   if( connData->priv->chunkHdr != NULL )
   {
      // We're sending chunked data, and the chunk needs fixing up.
      // Finish chunk with cr/lf
      httpdSend( connData, "\r\n", 2 );
      // Calculate length of chunk
      // +2 is to remove the two characters written above via httpdSend(), those
      // bytes aren't counted in the chunk length
      len = ( ( &connData->priv->sendBuff[connData->priv->sendBuffLen] ) - connData->priv->chunkHdr ) - ( CHUNK_SIZE_TEXT_LEN + 2 );
      // Fix up chunk header to correct value
      connData->priv->chunkHdr[0] = httpdHexNibble( len >> 12 );
      connData->priv->chunkHdr[1] = httpdHexNibble( len >> 8 );
      connData->priv->chunkHdr[2] = httpdHexNibble( len >> 4 );
      connData->priv->chunkHdr[3] = httpdHexNibble( len >> 0 );
      // Reset chunk hdr for next call
      connData->priv->chunkHdr = NULL;
   }

   if( connData->priv->flags & HFL_CHUNKED && connData->priv->flags & HFL_SENDINGBODY && connData->cgi == NULL )
   {
      if( connData->priv->sendBuffLen + 5 <= HTTPD_MAX_SENDBUFF_LEN )
      {
         // Connection finished sending whatever needs to be sent. Add NULL chunk to indicate this.
         strcpy( &connData->priv->sendBuff[connData->priv->sendBuffLen], "0\r\n\r\n" );
         connData->priv->sendBuffLen += 5;
         ASSERT( "sendBuffLen > HTTPD_MAX_SENDBUFF_LEN", connData->priv->sendBuffLen <= HTTPD_MAX_SENDBUFF_LEN );
      }
      else
      {
         ESP_LOGE( TAG, "sendBuff full" );
      }
   }

   if( connData->priv->sendBuffLen != 0 )
   {
      r = httpdPlatSendData( pInstance, connData, connData->priv->sendBuff, connData->priv->sendBuffLen );
      if( r != connData->priv->sendBuffLen )
      {
#ifdef CONFIG_ESPHTTPD_BACKLOG_SUPPORT
         // Can't send this for some reason. Dump packet in backlog, we can send it later.
         if( connData->priv->sendBacklogSize + connData->priv->sendBuffLen > HTTPD_MAX_BACKLOG_SIZE )
         {
            ESP_LOGE( TAG, "Backlog: Exceeded max backlog size, dropped %d bytes", connData->priv->sendBuffLen );
            connData->priv->sendBuffLen = 0;
            return false;
         }
         HttpSendBacklogItem *i = malloc( sizeof( HttpSendBacklogItem ) + connData->priv->sendBuffLen );
         if( i == NULL )
         {
            ESP_LOGE( TAG, "Backlog: malloc failed" );
            return false;
         }
         memcpy( i->data, connData->priv->sendBuff, connData->priv->sendBuffLen );
         i->len = connData->priv->sendBuffLen;
         i->next = NULL;
         if( connData->priv->sendBacklog == NULL )
         {
            connData->priv->sendBacklog = i;
         }
         else
         {
            HttpSendBacklogItem *e = connData->priv->sendBacklog;
            while( e->next != NULL ) e = e->next;
            e->next = i;
         }
         connData->priv->sendBacklogSize += connData->priv->sendBuffLen;
#else
         ESP_LOGE( TAG, "send buf tried to write %d bytes, wrote %d", connData->priv->sendBuffLen, r );
         HEAP_INFO( "" );
#endif
      }
      connData->priv->sendBuffLen = 0;
   }
   return true;
}

void ICACHE_FLASH_ATTR httpdCgiIsDone( HttpdInstance *pInstance, HttpdConnData *connData )
{
   connData->cgi = NULL; // no need to call this anymore

   if( connData->priv->flags & HFL_CHUNKED )
   {
      ESP_LOGD( TAG, "Cleaning up for next request" );
      httpdFlushSendBuffer( pInstance, connData );
      // Note: Do not clean up sendBacklog, it may still contain data at this point.
      connData->priv->headPos = 0;
      connData->post.len = -1;
      connData->priv->flags = 0;
      if( connData->post.buf )
         free( connData->post.buf );
      connData->post.buf = NULL;
      connData->post.buffLen = 0;
      connData->post.received = 0;
      connData->hostName = NULL;
   }
   else
   {
      // Cannot re-use this connection. Mark to get it killed after all data is sent.
      connData->priv->flags |= HFL_DISCONAFTERSENT;
   }
}

// Callback called when the data on a socket has been successfully
// sent.

CallbackStatus ICACHE_FLASH_ATTR httpdSentCb( HttpdInstance *pInstance, HttpdConnData *connData )
{
   return httpdContinue( pInstance, connData );
}

// Can be called after a CGI function has returned HTTPD_CGI_MORE to
// resume handling an open connection asynchronously

CallbackStatus ICACHE_FLASH_ATTR httpdContinue( HttpdInstance *pInstance, HttpdConnData * connData )
{
   int r;
   httpdPlatLock( pInstance );
   CallbackStatus status = CallbackSuccess;

#ifdef CONFIG_ESPHTTPD_BACKLOG_SUPPORT
   if( connData->priv->sendBacklog != NULL )
   {
      // We have some backlog to send first.
      HttpSendBacklogItem *next = connData->priv->sendBacklog->next;
      int bytesWritten = httpdPlatSendData( pInstance, connData, connData->priv->sendBacklog->data, connData->priv->sendBacklog->len );
      if( bytesWritten != connData->priv->sendBacklog->len )
      {
         ESP_LOGE( TAG, "tried to write %d bytes, wrote %d", connData->priv->sendBacklog->len, bytesWritten );
      }
      connData->priv->sendBacklogSize -= connData->priv->sendBacklog->len;
      free( connData->priv->sendBacklog );
      connData->priv->sendBacklog = next;
      httpdPlatUnlock( pInstance );
      return CallbackSuccess;
   }
#endif

   if( connData->priv->flags & HFL_DISCONAFTERSENT ) // Marked for destruction?
   {
      ESP_LOGD( TAG, "closing" );
      httpdPlatDisconnect( connData );
      status = CallbackSuccess;
      // NOTE: No need to call httpdFlushSendBuffer
   }
   else
   {
      // If we don't have a CGI function, there's nothing to do but wait for something from the client.
      if( connData->cgi == NULL )
      {
         status = CallbackSuccess;
      }
      else
      {
         char *sendBuff = malloc( HTTPD_MAX_SENDBUFF_LEN );
         if( sendBuff == NULL )
         {
            ESP_LOGE( TAG, "Malloc of sendBuff failed!" );
            HEAP_INFO( "" );
            status = CallbackErrorMemory;
         }
         else
         {
            connData->priv->sendBuff = sendBuff;
            connData->priv->sendBuffLen = 0;

            ESP_LOGD( TAG, "httpdContinue: Execute cgi fn." );
            r = connData->cgi( connData ); // Execute cgi fn.

            if( r == HTTPD_CGI_DONE )
            {
               // No special action for HTTPD_CGI_DONE
            }
            else if( r == HTTPD_CGI_NOTFOUND || r == HTTPD_CGI_AUTHENTICATED )
            {
               ESP_LOGW( TAG, "ERROR! CGI fn returns code %d after sending data! Bad CGI!", r );
            }

            if( ( r == HTTPD_CGI_DONE ) || ( r == HTTPD_CGI_NOTFOUND ) ||
                  ( r == HTTPD_CGI_AUTHENTICATED ) )
            {
               httpdCgiIsDone( pInstance, connData );
            }

            httpdFlushSendBuffer( pInstance, connData );
            free( sendBuff );
            connData->priv->sendBuff = NULL;
         }
      }
   }

   httpdPlatUnlock( pInstance );
   return status;
}

// This is called when the headers have been received and the connection is ready to send
// the result headers and data.
// We need to find the CGI function to call, call it, and dependent on what it returns either
// find the next cgi function, wait till the cgi data is sent or close up the connection.

static void ICACHE_FLASH_ATTR httpdProcessRequest( HttpdInstance *pInstance, HttpdConnData *connData )
{
   int r;
   int i = 0;
   if( connData->url == NULL )
   {
      ESP_LOGE( TAG, "url = NULL" );
      return; // Shouldn't happen
   }

#ifdef CONFIG_ESPHTTPD_CORS_SUPPORT
   // CORS preflight, allow the token we received before
   if( connData->requestType == HTTPD_METHOD_OPTIONS )
   {
      httpdStartResponse( connData, 200 );
      httpdHeader( connData, "Access-Control-Allow-Headers", connData->priv->corsToken );
      httpdEndHeaders( connData );
      httpdCgiIsDone( pInstance, connData );

      ESP_LOGD( TAG, "CORS preflight resp sent" );
      return;
   }
#endif

   // See if we can find a CGI that's happy to handle the request.
   while( 1 )
   {
      // Look up URL in the built-in URL table.
      while( pInstance->builtInUrls[i].url != NULL )
      {
         const HttpdBuiltInUrl *pUrl = &( pInstance->builtInUrls[i] );
         bool match = false;
         const char* route = pUrl->url;

         // See if there's a literal match
         if( strcmp( route, connData->url ) == 0 )
         {
            match = true;
         }
         else if( route[strlen( route ) - 1] == '*' &&
                  strncmp( route, connData->url, strlen( route ) - 1 ) == 0 )
         {
            // See if there's a wildcard match, if the route entry ends in '*'
            // and everything up to the '*' is a match
            match = true;
         }

         if( match )
         {
            ESP_LOGD( TAG, "Is url index %d", i );
            connData->cgiData = NULL;
            connData->cgi = pUrl->cgiCb;
            connData->cgiArg = pUrl->cgiArg;
            connData->cgiArg2 = pUrl->cgiArg2;
            break;
         }
         i++;
      }

      if( pInstance->builtInUrls[i].url == NULL )
      {
         // Drat, we're at the end of the URL table. This usually shouldn't happen. Well, just
         // generate a built-in 404 to handle this.
         ESP_LOGW( TAG, "%s not found. 404", connData->url );
         connData->cgi = cgiNotFound;
      }

      // Okay, we have a CGI function that matches the URL. See if it wants to handle the
      // particular URL we're supposed to handle.
      ESP_LOGD( TAG, "httpdProcessRequest: Execute cgi fn." );
      r = connData->cgi( connData ); // Execute cgi fn.

      if( r == HTTPD_CGI_MORE )
      {
         // Yep, it's happy to do so and has more data to send.
         if( connData->recvHdl )
         {
            // Seems the CGI is planning to do some long-term communications with the socket.
            // Disable the timeout on it, so we won't run into that.
            httpdPlatDisableTimeout( connData );
         }
         httpdFlushSendBuffer( pInstance, connData );
         break;
      }
      else if( r == HTTPD_CGI_DONE )
      {
         // Yep, it's happy to do so and already is done sending data.
         httpdCgiIsDone( pInstance, connData );
         break;
      }
      else if( r == HTTPD_CGI_NOTFOUND || r == HTTPD_CGI_AUTHENTICATED )
      {
         // URL doesn't want to handle the request: either the data isn't found or there's no
         // need to generate a login screen.
         i++; // look at next url the next iteration of the loop.
      }
   }
}

// Parse a line of header data and modify the connection data accordingly.
static CallbackStatus ICACHE_FLASH_ATTR httpdParseHeader( char *h, HttpdConnData *connData )
{
   int i;
   char firstLine = 0;
   CallbackStatus status = CallbackSuccess;

   if( strncmp( h, "GET ", 4 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_GET;
      firstLine = 1;
   }
   else if( strncasecmp( h, "Host:", 5 ) == 0 )
   {
      i = 5;
      while( h[i] == ' ' ) i++;
      connData->hostName = &h[i];
   }
   else if( strncmp( h, "POST ", 5 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_POST;
      firstLine = 1;
   }
   else if( strncmp( h, "PUT ", 4 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_PUT;
      firstLine = 1;
   }
   else if( strncmp( h, "PATCH ", 6 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_PATCH;
      firstLine = 1;
   }
   else if( strncmp( h, "OPTIONS ", 8 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_OPTIONS;
      firstLine = 1;
   }
   else if( strncmp( h, "DELETE ", 7 ) == 0 )
   {
      connData->requestType = HTTPD_METHOD_DELETE;
      firstLine = 1;
   }

   if( firstLine )
   {
      char *e;

      // Skip past the space after POST/GET
      i = 0;
      while( h[i] != ' ' ) i++;
      connData->url = h + i + 1;

      // Figure out end of url.
      e = ( char* )strstr( connData->url, " " );
      if( e == NULL ) return CallbackError;
      *e = 0; // terminate url part
      e++; // Skip to protocol indicator
      while( *e == ' ' ) e++; // Skip spaces.
      // If HTTP/1.1, note that and set chunked encoding
      if( strcasecmp( e, "HTTP/1.1" ) == 0 )
         connData->priv->flags |= HFL_HTTP11 | HFL_CHUNKED;

      ESP_LOGD( TAG, "URL = %s", connData->url );
      // Parse out the URL part before the GET parameters.
      connData->getArgs = ( char* )strstr( connData->url, "?" );
      if( connData->getArgs != 0 )
      {
         *connData->getArgs = 0;  // place a '0' to the location of the '?'
         connData->getArgs++;     // points to the string after the '?'
         ESP_LOGD( TAG, "GET args: %s", connData->getArgs );
      }
      else
      {
         connData->getArgs = NULL;
      }
   }
   else if( strncasecmp( h, "Connection:", 11 ) == 0 )
   {
      i = 11;
      // Skip trailing spaces
      while( h[i] == ' ' ) i++;
      if( strncmp( &h[i], "close", 5 ) == 0 ) connData->priv->flags &= ~HFL_CHUNKED; // Don't use chunked connData
   }
   else if( strncasecmp( h, "Content-Length:", 15 ) == 0 )
   {
      i = 15;
      // Skip trailing spaces
      while( h[i] == ' ' ) i++;
      // Get POST data length
      connData->post.len = atoi( h + i );

      // Allocate the buffer
      if( connData->post.len > HTTPD_MAX_POST_LEN )
      {
         // we'll stream this in in chunks
         connData->post.buffSize = HTTPD_MAX_POST_LEN;
      }
      else
      {
         connData->post.buffSize = connData->post.len;
      }

      ESP_LOGD( TAG, "Mallocced buffer for %d + 1 bytes of post data", connData->post.buffSize );
      int bufferSize = connData->post.buffSize + 1;
      connData->post.buf = ( char* )malloc( bufferSize );
      if( connData->post.buf == NULL )
      {
         ESP_LOGE( TAG, "malloc failed %d bytes", bufferSize );
         status = CallbackErrorMemory;
      }
      else
      {
         connData->post.buffLen = 0;
      }
   }
   else if( strncasecmp( h, "Content-Type: ", 14 ) == 0 )
   {
      if( strstr( h, "multipart/form-data" ) )
      {
         // It's multipart form data so let's pull out the boundary for future use
         char *b;
         if( ( b = strstr( h, "boundary=" ) ) != NULL )
         {
            connData->post.multipartBoundary = b + 7; // move the pointer 2 chars before boundary then fill them with dashes
            connData->post.multipartBoundary[0] = '-';
            connData->post.multipartBoundary[1] = '-';
            ESP_LOGD( TAG, "boundary = %s", connData->post.multipartBoundary );
         }
      }
   }
#ifdef CONFIG_ESPHTTPD_CORS_SUPPORT
   else if( strncmp( h, "Access-Control-Request-Headers: ", 32 ) == 0 )
   {
      // CORS token must be repeated in the response, copy it into
      // the connection token storage
      ESP_LOGD( TAG, "CORS preflight request." );

      strncpy( connData->priv->corsToken, h + strlen( "Access-Control-Request-Headers: " ), HTTPD_MAX_CORS_TOKEN_LEN );

      // ensure null termination of the token
      connData->priv->corsToken[ HTTPD_MAX_CORS_TOKEN_LEN - 1 ] = 0;
   }
#endif

   return status;
}

// Make a connection 'live' so we can do all the things a cgi can do to it.
// ToDo: Also make httpdRecvCb/httpdContinue use these?

CallbackStatus ICACHE_FLASH_ATTR httpdConnSendStart( HttpdInstance *pInstance, HttpdConnData *connData )
{
   CallbackStatus status;
   httpdPlatLock( pInstance );

   char *sendBuff = malloc( HTTPD_MAX_SENDBUFF_LEN );
   if( sendBuff == NULL )
   {
      ESP_LOGE( TAG, "Malloc sendBuff failed!" );
      status = CallbackErrorMemory;
   }
   else
   {
      connData->priv->sendBuff = sendBuff;
      connData->priv->sendBuffLen = 0;
      status = CallbackSuccess;
   }
   return status;
}

// Finish the live-ness of a connection. Always call this after httpdConnStart
void ICACHE_FLASH_ATTR httpdConnSendFinish( HttpdInstance *pInstance, HttpdConnData *connData )
{
   httpdFlushSendBuffer( pInstance, connData );
   free( connData->priv->sendBuff );
   connData->priv->sendBuff = NULL;
   httpdPlatUnlock( pInstance );
}

// Callback called when there's data available on a socket.
CallbackStatus ICACHE_FLASH_ATTR httpdRecvCb( HttpdInstance *pInstance, HttpdConnData *connData, char *data, unsigned short len )
{
   int x, r;
   char *p, *e;
   CallbackStatus status = CallbackSuccess;
   httpdPlatLock( pInstance );

   char *sendBuff = malloc( HTTPD_MAX_SENDBUFF_LEN );
   if( sendBuff == NULL )
   {
      ESP_LOGE( TAG, "Malloc sendBuff failed!" );
      HEAP_INFO( "" );
      status = CallbackErrorMemory;
   }
   else
   {
      connData->priv->sendBuff = sendBuff;
      connData->priv->sendBuffLen = 0;
#ifdef CONFIG_ESPHTTPD_CORS_SUPPORT
      connData->priv->corsToken[0] = 0;
#endif

      // This is slightly evil/dirty: we abuse connData->post.len as a state variable for where in the http communications we are:
      // <0 ( -1 ): Post len unknown because we're still receiving headers
      // ==0: No post data
      // >0: Need to receive post data
      // ToDo: See if we can use something more elegant for this.

      for( x = 0; x < len; x++ )
      {
         if( connData->post.len < 0 ) // This byte is a header byte
         {
            // This byte is a header byte.
            if( data[x] == '\n' )
            {
               if( connData->priv->headPos < HTTPD_MAX_HEAD_LEN - 1 )
               {
                  // Compatibility with clients that send \n only: fake a \r in front of this.
                  if( connData->priv->headPos != 0 && connData->priv->head[connData->priv->headPos - 1] != '\r' )
                  {
                     connData->priv->head[connData->priv->headPos++] = '\r';
                  }
               }
               else
               {
                  ESP_LOGE( TAG, "adding newline request too long" );
               }
            }

            // ToDo: return http error code 431 ( request header too long ) if this happens
            if( connData->priv->headPos < HTTPD_MAX_HEAD_LEN - 1 )
            {
               connData->priv->head[connData->priv->headPos++] = data[x];
            }
            else
            {
               ESP_LOGE( TAG, "request too long!" );
            }

            // always null terminate
            connData->priv->head[connData->priv->headPos] = 0;

            // Scan for /r/n/r/n. Receiving this indicate the headers end.
            if( data[x] == '\n' && ( char * )strstr( connData->priv->head, "\r\n\r\n" ) != NULL )
            {
               // Indicate we're done with the headers.
               connData->post.len = 0;
               // Reset url data
               connData->url = NULL;
               // Iterate over all received headers and parse them.
               p = connData->priv->head;
               while( p < ( &connData->priv->head[connData->priv->headPos - 4] ) )
               {
                  e = ( char * )strstr( p, "\r\n" ); // Find end of header line
                  if( e == NULL ) break;             // Shouldn't happen.
                  e[0] = 0;                          // Zero-terminate header
                  httpdParseHeader( p, connData );   // and parse it.
                  p = e + 2;                         // Skip /r/n ( now /0/n )
               }

               // If we don't need to receive post data, we can send the response now.
               if( connData->post.len == 0 )
               {
                  httpdProcessRequest( pInstance, connData );
               }
            }
         }
         else if( connData->post.len != 0 )
         {
            // This byte is a POST byte.
            connData->post.buf[connData->post.buffLen++] = data[x];
            connData->post.received++;
            connData->hostName = NULL;
            if( connData->post.buffLen >= connData->post.buffSize || connData->post.received == connData->post.len )
            {
               // Received a chunk of post data
               connData->post.buf[connData->post.buffLen] = 0; // zero-terminate, in case the cgi handler knows it can use strings
               // Process the data
               if( connData->cgi )
               {
                  ESP_LOGD( TAG, "httpdRecvCb: Execute cgi fn." );
                  r = connData->cgi( connData ); // Execute cgi fn.

                  if( r == HTTPD_CGI_DONE )
                  {
                     httpdCgiIsDone( pInstance, connData );
                  }
               }
               else
               {
                  // No CGI fn set yet: probably first call. Allow httpdProcessRequest to choose CGI and
                  // call it the first time.
                  httpdProcessRequest( pInstance, connData );
               }
               connData->post.buffLen = 0;
            }
         }
         else
         {
            // Let cgi handle data if it registered a recvHdl callback. If not, ignore.
            if( connData->recvHdl )
            {
               r = connData->recvHdl( pInstance, connData, data + x, len - x );
               if( r == HTTPD_CGI_DONE )
               {
                  ESP_LOGD( TAG, "Recvhdl returned DONE" );
                  httpdCgiIsDone( pInstance, connData );
                  // We assume the recvhdlr has sent something; we'll kill the sock in the sent callback.
               }
               break; // ignore rest of data, recvhdl has parsed it.
            }
            else
            {
               ESP_LOGE( TAG, "Unexpected data from client. %s", data );
               status = CallbackError;
            }
         }
      }
      httpdFlushSendBuffer( pInstance, connData );
      free( sendBuff );
      connData->priv->sendBuff = NULL;
   }
   httpdPlatUnlock( pInstance );

   return status;
}

// The platform layer should ALWAYS call this function, regardless if the connection is closed by the server
// or by the client.
CallbackStatus ICACHE_FLASH_ATTR httpdDisconCb( HttpdInstance *pInstance, HttpdConnData *connData )
{
   httpdPlatLock( pInstance );

   ESP_LOGD( TAG, "httpdDisconCb: Connection closed: %s", S( connData->url ) );
   connData->isConnectionClosed = true;

   if( connData->cgi )
   {
      ESP_LOGD( TAG, "httpdDisconCb: Execute cgi fn." );
      connData->cgi( connData ); // Execute cgi fn if needed
   }
   httpdRetireConn( pInstance, connData );   // free post buffer and priv data buffer
   httpdPlatUnlock( pInstance );

   return CallbackSuccess;
}

void ICACHE_FLASH_ATTR httpdConnectCb( HttpdInstance *pInstance, HttpdConnData *connData )
{
   ESP_LOGD( TAG, "httpdConnectCb: create a new connection" );

   httpdPlatLock( pInstance );

   memset( connData, 0, sizeof( HttpdConnData ) );
   connData->priv = malloc( sizeof( HttpdPriv ) );
   memset( connData->priv, 0, sizeof( HttpdPriv ) );

   connData->post.len = -1;

   httpdPlatUnlock( pInstance );
}

/**
 * Set server name ( must be constant / strdup )
 * @param name
 */
void ICACHE_FLASH_ATTR httpdSetName( const char *name )
{
   serverName = name;
}

#ifdef CONFIG_ESPHTTPD_SHUTDOWN_SUPPORT
void httpdShutdown( HttpdInstance *pInstance )
{
   httpdPlatShutdown( pInstance );
}
#endif
