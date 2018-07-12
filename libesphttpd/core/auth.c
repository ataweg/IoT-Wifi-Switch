// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - WebServer
//
// File          auth.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-04  AWe   add debug support
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//                         here: replace ( connData->conn == NULL ) with ( connData->isConnectionClosed )
//    2017-11-23  AWe   Change return value of authBasic() to CgiStatus, missed in initial CgiStatus introduction
//    2017-11-05  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/24f9a371eb5c0804dcc6657f99449ef07788140c
//                        fix 401 message
// --------------------------------------------------------------------------

/*
   HTTP auth implementation. Only does basic authentication for now.
*/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
HTTP auth implementation. Only does basic authentication for now.
*/

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_NONE
static const char *TAG = "auth";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef linux
   #include <libesphttpd/linux.h>
#else
   #include <libesphttpd/esp.h>
#endif

#include "libesphttpd/auth.h"
#include "base64.h"

const char *unauthorized = "401 Unauthorized.";

CgiStatus ICACHE_FLASH_ATTR authBasic( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "authBasic ..." );

   int no = 0;
   int r;
   char hdr[( AUTH_MAX_USER_LEN + AUTH_MAX_PASS_LEN + 2 ) * 10];
   char userpass[AUTH_MAX_USER_LEN + AUTH_MAX_PASS_LEN + 2];
   char user[AUTH_MAX_USER_LEN];
   char pass[AUTH_MAX_PASS_LEN];

   if( connData->isConnectionClosed )
   {
      // Connection closed. Clean up.
      return HTTPD_CGI_DONE;
   }

   r = httpdGetHeader( connData, "Authorization", hdr, sizeof( hdr ) );
   ESP_LOGD( TAG, "hdr: %s", S( hdr ) );

   if( r && strncmp( hdr, "Basic", 5 ) == 0 )
   {
      r = base64_decode( strlen( hdr ) - 6, hdr + 6, sizeof( userpass ), ( unsigned char * )userpass );
      if( r < 0 ) r = 0;      // just clean out string on decode error
      userpass[r] = 0;        // zero-terminate user:pass string
      ESP_LOGD( TAG, "Auth: %s", userpass );

      while( ( ( AuthGetUserPw )( connData->cgiArg ) )( connData, no,
             user, AUTH_MAX_USER_LEN, pass, AUTH_MAX_PASS_LEN ) )
      {
         // Check user/pass against auth header
         ESP_LOGD( TAG, "Check: %s:%s", S( user ), S( pass ) );

         if( strlen( userpass ) == strlen( user ) + strlen( pass ) + 1 &&
               strncmp( userpass, user, strlen( user ) ) == 0 &&
               userpass[strlen( user )] == ':' &&
               strcmp( userpass + strlen( user ) + 1, pass ) == 0 )
         {
            // Authenticated. Yay!
            ESP_LOGD( TAG, "Authenticated. Yay!" );
            return HTTPD_CGI_AUTHENTICATED;
         }
         no++; // Not authenticated with this user/pass. Check next user/pass combo.
      }
   }

   // Not authenticated. Go bug user with login screen.
   httpdStartResponse( connData, 401 );
   httpdHeader( connData, "Content-Type", "text/plain" );
   httpdHeader( connData, "WWW-Authenticate", "Basic realm=\""HTTP_AUTH_REALM"\"" );
   httpdEndHeaders( connData );
   httpdSend( connData, unauthorized, -1 );

   // Okay, all done.
   ESP_LOGD( TAG, "Okay, all done." );
   return HTTPD_CGI_DONE;
}

