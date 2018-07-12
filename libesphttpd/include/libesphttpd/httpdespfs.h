// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          httpdespfs.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-01-18  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//    2017-09-14  AWe   add prototype for serveStaticFile()
//    2017-08-24  AWe   add prototype for tplSend()
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//
// --------------------------------------------------------------------------

#ifndef __HTTPDESPFS_H__
#define __HTTPDESPFS_H__

#include "httpd.h"
#include "espfs.h"

#define FILE_CHUNK_LEN 1024

/**
 * The template substitution callback.
 * Returns CGI_MORE if more should be sent within the token, CGI_DONE otherwise.
 */
typedef CgiStatus( * TplCallback )( HttpdConnData *connData, char *token, void **arg );

typedef enum
{
   ENCODE_PLAIN = 0,
   ENCODE_HTML,
   ENCODE_JS,
}
TplEncode;

typedef struct
{
   EspFsFile *file;
   void *tplArg;
   char token[64];
   int tokenPos;

   char buf[FILE_CHUNK_LEN + 1];

   bool chunk_resume;
   int buff_len;
   int buff_x;
   int buff_sp;
   char *buff_e;

   TplEncode tokEncode;
}
TplData;

CgiStatus ICACHE_FLASH_ATTR cgiEspFsHook( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiEspFsTemplate( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR serveStaticFile( HttpdConnData *connData, const char* filepath, int responseCode );

// return value < 0 something goes wrong: -1 sendbuffer will overflow, discard data
// otherwise return value gives the number of remaining free bytes in the send buffer
int ICACHE_FLASH_ATTR tplSend( HttpdConnData *connData, const char *str, int len );

#endif // __HTTPDESPFS_H__