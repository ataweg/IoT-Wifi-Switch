// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiwebsocket.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-01-18  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//    2017-09-18  AWe   remove some changes from MigthyPort:
//                         WEBSOCK_FLAG_MORE
//                      see also Fix multi-part websocket sending. Note the changed flag constants!
//    2017-09-17  AWe   take over change from MightyPork/libesphttpd/commit/58c81c0dfe8e15888408886e168ed739aa8f311d
//    2017-09-01  AWe   merge some changes from MigthyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/b804b196fca6abf28a7e2b5bd482b76b49d518f7
//                      don't takeover CORS support
//    2017-08-16  AWe   use httpd module from https://github.com/Spritetm/libesphttpd
//
// --------------------------------------------------------------------------

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Websocket support for esphttpd. Inspired by https://github.com/dangrie158/ESP-8266-WebSocket
*/

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "cgiwebsocket";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// #define NO_DBG_MEMLEAKS

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef linux
   #include <libesphttpd/linux.h>
#else
   #include <libesphttpd/esp.h>
#endif

#include "libesphttpd/httpd.h"
#include "libesphttpd/sha1.h"
#include "base64.h"
#include "libesphttpd/cgiwebsocket.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define WS_KEY_IDENTIFIER "Sec-WebSocket-Key: "
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* from IEEE RFC6455 sec 5.2
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  ( 4 )  |A|     ( 7 )     |             ( 16/64 )           |
     |N|V|V|V|       |S|             |   ( if payload len==126/127 )   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key ( continued )       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
*/

#define FLAG_FIN ( 1 << 7 )

#define OPCODE_CONTINUE 0x0
#define OPCODE_TEXT 0x1
#define OPCODE_BINARY 0x2
#define OPCODE_CLOSE 0x8
#define OPCODE_PING 0x9
#define OPCODE_PONG 0xA

#define FLAGS_MASK ( ( uint8_t )0xF0 )
#define OPCODE_MASK ( ( uint8_t )0x0F )
#define IS_MASKED ( ( uint8_t )( 1<<7 ) )
#define PAYLOAD_MASK ( ( uint8_t )0x7F )

typedef struct WebsockFrame WebsockFrame;

#define ST_FLAGS 0
#define ST_LEN0 1
#define ST_LEN1 2
#define ST_LEN2 3
// ...
#define ST_LEN8 9
#define ST_MASK1 10
#define ST_MASK4 13
#define ST_PAYLOAD 14

struct WebsockFrame
{
   uint8_t flags;
   uint8_t len8;
   uint64_t len;
   uint8_t mask[4];
};

struct WebsockPriv
{
   struct WebsockFrame fr;
   uint8_t maskCtr;
   uint8_t frameCont;
   uint8_t closedHere;
   int wsStatus;
   Websock *next; // in linked list
};

static Websock *llStart = NULL;

// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer

static int ICACHE_FLASH_ATTR sendFrameHead( Websock *ws, int opcode, int len )
{
   char buf[14];
   int i = 0;
   buf[i++] = opcode;
   if( len > 65535 )
   {
      buf[i++] = 127;
      buf[i++] = 0;
      buf[i++] = 0;
      buf[i++] = 0;
      buf[i++] = 0;
      buf[i++] = len >> 24;
      buf[i++] = len >> 16;
      buf[i++] = len >> 8;
      buf[i++] = len;
   }
   else if( len > 125 )
   {
      buf[i++] = 126;
      buf[i++] = len >> 8;
      buf[i++] = len;
   }
   else
   {
      buf[i++] = len;
   }
   return httpdSend( ws->connData, buf, i );
}

// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer

int ICACHE_FLASH_ATTR cgiWebsocketSend( HttpdInstance *pInstance, Websock *ws, const char *data, int len, int flags )
{
   ESP_LOGD( TAG, "cgiWebsocketSend" );
   int r = 0;
   int fl = 0;

   // Continuation frame has opcode 0
   if( !( flags & WEBSOCK_FLAG_CONT ) )
   {
      if( flags & WEBSOCK_FLAG_BIN )
         fl = OPCODE_BINARY;
      else
         fl = OPCODE_TEXT;
   }
   // add FIN to last frame
   if( !( flags & WEBSOCK_FLAG_MORE ) ) fl |= FLAG_FIN;

   sendFrameHead( ws, fl, len );
   if( len != 0 ) r = httpdSend( ws->connData, data, len );
   httpdFlushSendBuffer( pInstance, ws->connData );
   return r;
}

// Broadcast data to all websockets at a specific url. Returns the amount of connections sent to.
int ICACHE_FLASH_ATTR cgiWebsockBroadcast( HttpdInstance *pInstance, const char *resource, char *data, int len, int flags )
{
   ESP_LOGD( TAG, "cgiWebsockBroadcast" );
   Websock *lw = llStart;
   int ret = 0;
   while( lw != NULL )
   {
      if( strcmp( lw->connData->url, resource ) == 0 )
      {
         httpdConnSendStart( pInstance, lw->connData );
         cgiWebsocketSend( pInstance, lw, data, len, flags );
         httpdConnSendFinish( pInstance, lw->connData );
         ret++;
      }
      lw = lw->priv->next;  // !!! todo ( AWe ) : here it crashed sometimes
   }
   return ret;
}


void ICACHE_FLASH_ATTR cgiWebsocketClose( HttpdInstance *pInstance, Websock *ws, int reason )
{
   ESP_LOGD( TAG, "cgiWebsocketClose" );
   char rs[2] = {reason >> 8, reason & 0xff};
   sendFrameHead( ws, FLAG_FIN | OPCODE_CLOSE, 2 );
   httpdSend( ws->connData, rs, 2 );
   ws->priv->closedHere = 1;
   httpdFlushSendBuffer( pInstance, ws->connData );
}


static void ICACHE_FLASH_ATTR websockFree( Websock *ws )
{
   ESP_LOGD( TAG, "websockFree" );
   if( ws->closeCb ) ws->closeCb( ws );
   // Clean up linked list
   if( llStart == ws )
   {
      llStart = ws->priv->next;
   }
   else if( llStart )
   {
      Websock *lws = llStart;
      // Find ws that links to this one.
      while( lws != NULL && lws->priv->next != ws ) lws = lws->priv->next;
      if( lws != NULL ) lws->priv->next = ws->priv->next;
   }
   if( ws->priv ) free( ws->priv );
}

CgiStatus ICACHE_FLASH_ATTR cgiWebSocketRecv( HttpdInstance *pInstance, HttpdConnData *connData, char *data, int len )
{
   ESP_LOGD( TAG, "cgiWebSocketRecv" );
   int i, j, sl;

   CgiStatus r = HTTPD_CGI_MORE;
   int wasHeaderByte;
   Websock *ws = ( Websock* )connData->cgiData;
   for( i = 0; i < len; i++ )
   {
      wasHeaderByte = 1;
      if( ws->priv->wsStatus == ST_FLAGS )
      {
         ws->priv->maskCtr = 0;
         ws->priv->frameCont = 0;
         ws->priv->fr.flags = ( uint8_t )data[i];
         ws->priv->wsStatus = ST_LEN0;
      }
      else if( ws->priv->wsStatus == ST_LEN0 )
      {
         ws->priv->fr.len8 = ( uint8_t )data[i];
         if( ( ws->priv->fr.len8 & 127 ) >= 126 )
         {
            ws->priv->fr.len = 0;
            ws->priv->wsStatus = ST_LEN1;
         }
         else
         {
            ws->priv->fr.len = ws->priv->fr.len8 & 127;
            ws->priv->wsStatus = ( ws->priv->fr.len8 & IS_MASKED ) ? ST_MASK1 : ST_PAYLOAD;
         }
      }
      else if( ws->priv->wsStatus <= ST_LEN8 )
      {
         ws->priv->fr.len = ( ws->priv->fr.len << 8 ) | data[i];
         if( ( ( ws->priv->fr.len8 & 127 ) == 126 && ws->priv->wsStatus == ST_LEN2 ) || ws->priv->wsStatus == ST_LEN8 )
         {
            ws->priv->wsStatus = ( ws->priv->fr.len8 & IS_MASKED ) ? ST_MASK1 : ST_PAYLOAD;
         }
         else
         {
            ws->priv->wsStatus++;
         }
      }
      else if( ws->priv->wsStatus <= ST_MASK4 )
      {
         ws->priv->fr.mask[ws->priv->wsStatus - ST_MASK1] = data[i];
         ws->priv->wsStatus++;
      }
      else
      {
         // Was a payload byte.
         wasHeaderByte = 0;
      }

      if( ws->priv->wsStatus == ST_PAYLOAD && wasHeaderByte )
      {
         // We finished parsing the header, but i still is on the last header byte. Move one forward so
         // the payload code works as usual.
         i++;
      }
      // Also finish parsing frame if we haven't received any payload bytes yet, but the length of the frame
      // is zero.
      if( ws->priv->wsStatus == ST_PAYLOAD )
      {
         // Okay, header is in; this is a data byte. We're going to process all the data bytes we have
         // received here at the same time; no more byte iterations till the end of this frame.
         // First, unmask the data
         sl = len - i;
         ESP_LOGD( TAG, "Frame payload. wasHeaderByte %d fr.len %d sl %d cmd 0x%x", wasHeaderByte, ( int )ws->priv->fr.len, ( int )sl, ws->priv->fr.flags );
         if( sl > ws->priv->fr.len ) sl = ws->priv->fr.len;
         for( j = 0; j < sl; j++ ) data[i + j] ^= ( ws->priv->fr.mask[( ws->priv->maskCtr++ ) & 3] );

         // Inspect the header to see what we need to do.
         if( ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_PING )
         {
            if( ws->priv->fr.len > 125 )
            {
               if( !ws->priv->frameCont ) cgiWebsocketClose( pInstance, ws, 1002 );
               r = HTTPD_CGI_DONE;
               break;
            }
            else
            {
               if( !ws->priv->frameCont ) sendFrameHead( ws, OPCODE_PONG | FLAG_FIN, ws->priv->fr.len );
               if( sl > 0 ) httpdSend( ws->connData, data + i, sl );
            }
         }
         else if( ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_TEXT ||
                  ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_BINARY ||
                  ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_CONTINUE )
         {
            if( sl > ws->priv->fr.len ) sl = ws->priv->fr.len;
            if( !( ws->priv->fr.len8 & IS_MASKED ) )
            {
               // We're a server; client should send us masked packets.
               cgiWebsocketClose( pInstance, ws, 1002 );
               r = HTTPD_CGI_DONE;
               break;
            }
            else
            {
               int flags = 0;
               if( ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_BINARY ) flags |= WEBSOCK_FLAG_BIN;
               if( ( ws->priv->fr.flags & FLAG_FIN ) == 0 ) flags |= WEBSOCK_FLAG_MORE;
               if( ws->recvCb ) ws->recvCb( ws, data + i, sl, flags );
            }
         }
         else if( ( ws->priv->fr.flags & OPCODE_MASK ) == OPCODE_CLOSE )
         {
            ESP_LOGD( TAG, "Got close frame" );
            if( !ws->priv->closedHere )
            {
               ESP_LOGD( TAG, "Sending response close frame" );
               cgiWebsocketClose( pInstance, ws, ( ( data[i] << 8 ) & 0xff00 ) + ( data[i + 1] & 0xff ) );
            }
            r = HTTPD_CGI_DONE;
            break;
         }
         else
         {
            if( !ws->priv->frameCont )
               ESP_LOGE( TAG, "Unknown opcode 0x%X", ws->priv->fr.flags & OPCODE_MASK );
         }
         i += sl - 1;
         ws->priv->fr.len -= sl;
         if( ws->priv->fr.len == 0 )
         {
            ws->priv->wsStatus = ST_FLAGS; // go receive next frame
         }
         else
         {
            ws->priv->frameCont = 1; // next payload is continuation of this frame.
         }
      }
   }
   if( r == HTTPD_CGI_DONE )
   {
      // We're going to tell the main webserver we're done. The webserver expects us to clean up by ourselves
      // we're chosing to be done. Do so.
      websockFree( ws );
      free( connData->cgiData );
      connData->cgiData = NULL;
   }
   return r;
}

// Websocket 'cgi' implementation
CgiStatus ICACHE_FLASH_ATTR cgiWebsocket( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWebsocket" );
   char buf[256];
   int i;
   sha1nfo s;
   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      ESP_LOGD( TAG, "Cleanup" );
      if( connData->cgiData )
      {
         Websock *ws = ( Websock* )connData->cgiData;
         websockFree( ws );
         free( connData->cgiData );
         connData->cgiData = NULL;
      }
      return HTTPD_CGI_DONE;
   }

   if( connData->cgiData == NULL )
   {
      ESP_LOGD( TAG, "WS: First call" );
      // First call here. Check if client headers are OK, send server header.
      i = httpdGetHeader( connData, "Upgrade", buf, sizeof( buf ) - 1 );
      ESP_LOGD( TAG, "Upgrade: %s", buf );
      if( i && strcasecmp( buf, "websocket" ) == 0 )
      {
         i = httpdGetHeader( connData, "Sec-WebSocket-Key", buf, sizeof( buf ) - 1 );
         if( i )
         {
            ESP_LOGD( TAG, "Key: %s", buf );
            // Seems like a WebSocket connection.
            // Alloc structs
            connData->cgiData = malloc( sizeof( Websock ) );
            if( connData->cgiData == NULL )
            {
               ESP_LOGE( TAG, "Can't allocate mem for websocket" );
               return HTTPD_CGI_DONE;
            }
            memset( connData->cgiData, 0, sizeof( Websock ) );
            Websock *ws = ( Websock* )connData->cgiData;
            ws->priv = malloc( sizeof( WebsockPriv ) );
            if( ws->priv == NULL )
            {
               ESP_LOGE( TAG, "Can't allocate mem for websocket priv" );
               free( connData->cgiData );
               connData->cgiData = NULL;
               return HTTPD_CGI_DONE;
            }
            memset( ws->priv, 0, sizeof( WebsockPriv ) );
            ws->connData = connData;
            // Reply with the right headers.
            strcat( buf, WS_GUID );
            sha1_init( &s );
            sha1_write( &s, buf, strlen( buf ) );
            httpdSetTransferMode( connData, HTTPD_TRANSFER_NONE );
            httpdStartResponse( connData, 101 );
            httpdHeader( connData, "Upgrade", "websocket" );
            httpdHeader( connData, "Connection", "upgrade" );
            base64_encode( 20, sha1_result( &s ), sizeof( buf ), buf );
            httpdHeader( connData, "Sec-WebSocket-Accept", buf );
            httpdEndHeaders( connData );
            // Set data receive handler
            connData->recvHdl = cgiWebSocketRecv;
            // Inform CGI function we have a connection
            WsConnectedCb connCb = connData->cgiArg;
            connCb( ws );
            // Insert ws into linked list
            if( llStart == NULL )
            {
               llStart = ws;
            }
            else
            {
               Websock *lw = llStart;
               while( lw->priv->next ) lw = lw->priv->next;
               lw->priv->next = ws;
            }
            ESP_LOGD( TAG, "Start a websocket connection" );
            return HTTPD_CGI_MORE;
         }
      }
      // No valid websocket connection
      httpdStartResponse( connData, 500 );
      httpdEndHeaders( connData );
      return HTTPD_CGI_DONE;
   }

   // Sending is done. Call the sent callback if we have one.
   ESP_LOGD( TAG, "WS: Sending is done. Call the sent callback" );
   Websock *ws = ( Websock* )connData->cgiData;
   if( ws && ws->sentCb ) ws->sentCb( ws );

   return HTTPD_CGI_MORE;
}
