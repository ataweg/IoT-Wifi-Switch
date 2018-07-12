// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "cgiredirect";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifndef FRRERTOS
   #include "libesphttpd/httpd-nonos.h"  // get_conn();
#endif

#include <libesphttpd/esp.h>
#include <libesphttpd/cgiredirect.h>

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This CGI function redirects to a fixed url of http://[hostname]/ if hostname field of request isn't
// already that hostname. Use this in combination with a DNS server that redirects everything to the
// ESP in order to load a HTML page as soon as a phone, tablet etc connects to the ESP. Watch out:
// this will also redirect connections when the ESP is in STA mode, potentially to a hostname that is not
// in the 'official' DNS and so will fail.

// Use this as a cgi function to redirect one url to another.

CgiStatus ICACHE_FLASH_ATTR cgiRedirect( HttpdConnData *connData )
{
   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }
   httpdRedirect( connData, ( char* )connData->cgiArg );
   return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR cgiRedirectToHostname( HttpdConnData *connData )
{
   static const char hostFmt[] = "http://%s/";
   char *buf;
   int isIP = 0;
   int x;

   if( connData->isConnectionClosed )
   {
      // Connection closed.
      return HTTPD_CGI_DONE;
   }
   if( connData->hostName == NULL )
   {
      ESP_LOGW( TAG, "Huh? No hostname." );
      return HTTPD_CGI_NOTFOUND;
   }

   // Quick and dirty code to see if host is an IP
   if( strlen( connData->hostName ) > 8 )
   {
      isIP = 1;
      for( x = 0; x < strlen( connData->hostName ); x++ )
      {
         if( connData->hostName[x] != '.' && ( connData->hostName[x] < '0' || connData->hostName[x] > '9' ) ) isIP = 0;
      }
   }

   if( isIP )
   {
      return HTTPD_CGI_NOTFOUND;
   }

   // Check hostname; pass on if the same
   if( strcasecmp( connData->hostName, ( char* )connData->cgiArg ) == 0 )
   {
      ESP_LOGD( TAG, "connData->hostName:'%s', redirect hostname: '%s'", connData->hostName,
                ( char* )connData->cgiArg );
      return HTTPD_CGI_NOTFOUND;
   }

   // Not the same. Redirect to real hostname.
   buf = malloc( strlen( ( char* )connData->cgiArg ) + sizeof( hostFmt ) );
   if( buf == NULL )
   {
      ESP_LOGE( TAG, "allocating memory" );
      // Bail out
      return HTTPD_CGI_DONE;
   }
   sprintf( buf, hostFmt, ( char* )connData->cgiArg );
   ESP_LOGI( TAG, "Redirecting to hostname url %s", buf );
   httpdRedirect( connData, buf );
   free( buf );
   return HTTPD_CGI_DONE;
}

// Same as above, but will only redirect clients with an IP that is in the range of
// the SoftAP interface. This should preclude clients connected to the STA interface
// to be redirected to nowhere.

CgiStatus ICACHE_FLASH_ATTR cgiRedirectApClientToHostname( HttpdConnData *connData )
{
#ifdef linux
   return HTTPD_CGI_NOTFOUND;
#else
 #ifndef FREERTOS
   uint32_t *remadr;
   struct ip_info apip;

   int opmode = wifi_get_opmode();
   // Check if we have an softap interface; bail out if not
   if( opmode != SOFTAP_MODE && opmode != STATIONAP_MODE )
      return HTTPD_CGI_NOTFOUND;

   ConnTypePtr pConn = get_pConn( connData );
   remadr = ( uint32_t * )pConn->remote_ip;

   wifi_get_ip_info( SOFTAP_IF, &apip );
   if( ( *remadr & apip.netmask.addr ) == ( apip.ip.addr & apip.netmask.addr ) )
   {
      return cgiRedirectToHostname( connData );
   }
   else
   {
      return HTTPD_CGI_NOTFOUND;
   }
 #else
   return HTTPD_CGI_NOTFOUND;
 #endif  // FREERTOS
#endif  // linux
}

