// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          httpd.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-19  AWe   for priv buffer and sendData buffer allocate memory from
//                        heap when needed. Give up to have buffers in the memory space.
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//    2017-09-01  AWe   merge some changes from MigthyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//    2017-08-24  AWe   use enums for HTTP method and mode
//                      add prototype for httpdGetVersion(), httpdMethodName()
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//
// --------------------------------------------------------------------------

#ifndef HTTPD_H
#define HTTPD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp.h"

/**
 * Design notes:
 *  - The platform code owns the memory management of connections
 *  - The platform embeds a HttpdConnData into its own structure, this enables
 *    the platform code, through the use of the container_of approach, to
 *    find its connection structure when given a HttpdConnData*
 */


// we must not use this macro outside the library, as the git hash is not defined there
#define HTTPDVER "0.5"

// default servername
#ifndef HTTPD_SERVERNAME
   #define HTTPD_SERVERNAME "esp-httpd ( awe ) " HTTPDVER
#endif

// Max length of request head. This is statically allocated for each connection.
#ifndef HTTPD_MAX_HEAD_LEN
   #define HTTPD_MAX_HEAD_LEN    1024
#endif

// Max post buffer len. This is dynamically malloc'ed if needed.
#ifndef HTTPD_MAX_POST_LEN
   #define HTTPD_MAX_POST_LEN    2048
#endif

// Max send buffer len. This is allocated on the stack.
#ifndef HTTPD_MAX_SENDBUFF_LEN
   #define HTTPD_MAX_SENDBUFF_LEN   2048
#endif

// If some data can't be sent because the underlaying socket doesn't accept the data ( like the nonos
// layer is prone to do ), we put it in a backlog that is dynamically malloc'ed. This defines the max
// size of the backlog.
#ifndef HTTPD_MAX_BACKLOG_SIZE
   #define HTTPD_MAX_BACKLOG_SIZE   ( 4*1024 )
#endif

// Max length of CORS token. This amount is allocated per connection.
#define HTTPD_MAX_CORS_TOKEN_LEN 256

// CGI handler state / return value

typedef enum
{
   HTTPD_CGI_MORE,
   HTTPD_CGI_DONE,
   HTTPD_CGI_NOTFOUND,
   HTTPD_CGI_AUTHENTICATED
} CgiStatus;

// HTTP method ( verb ) used for the request

typedef enum
{
   HTTPD_METHOD_GET,
   HTTPD_METHOD_POST,
   HTTPD_METHOD_OPTIONS,
   HTTPD_METHOD_PUT,
   HTTPD_METHOD_PATCH,
   HTTPD_METHOD_DELETE,
   HTTPD_METHOD_HEAD,
} RequestTypes;

// Transfer mode

typedef enum
{
   HTTPD_TRANSFER_CLOSE,
   HTTPD_TRANSFER_CHUNKED,
   HTTPD_TRANSFER_NONE
} TransferModes;

typedef struct HttpdPriv HttpdPriv;
typedef struct HttpdConnData HttpdConnData;
typedef struct HttpdPostData HttpdPostData;
typedef struct HttpdInstance HttpdInstance;


typedef CgiStatus( * cgiSendCallback )( HttpdConnData *connData );
typedef CgiStatus( * cgiRecvHandler )( HttpdInstance *pInstance, HttpdConnData *connData, char *data, int len );

#ifdef CONFIG_ESPHTTPD_BACKLOG_SUPPORT
typedef struct HttpSendBacklogItem HttpSendBacklogItem;

struct HttpSendBacklogItem
{
   int len;
   HttpSendBacklogItem *next;
   char data[];
};
#endif

// Private data for http connection
struct HttpdPriv
{
   char  head[HTTPD_MAX_HEAD_LEN];
#ifdef CONFIG_ESPHTTPD_CORS_SUPPORT
   char  corsToken[HTTPD_MAX_CORS_TOKEN_LEN];
#endif
   int   headPos;
   char  *sendBuff;
   int   sendBuffLen;

   /** NOTE: chunkHdr, if valid, points at memory assigned to sendBuff
      so it doesn't have to be freed */
   char *chunkHdr;

#ifdef CONFIG_ESPHTTPD_BACKLOG_SUPPORT
   HttpSendBacklogItem *sendBacklog;
   int   sendBacklogSize;
#endif
   int   flags;
};

// A struct describing the POST data sent inside the http connection.  This is used by the CGI functions
struct HttpdPostData
{
   int len;             // POST Content-Length
   int buffSize;        // The maximum length of the post buffer
   int buffLen;         // The amount of bytes in the current post buffer
   int received;        // The total amount of bytes received so far
   char *buf;          // Actual POST data buffer
   char *multipartBoundary; // Pointer to the start of the multipart boundary value in priv.head
};

// A struct describing a http connection. This gets passed to cgi functions.
struct HttpdConnData
{
   RequestTypes requestType;  // One of the HTTPD_METHOD_* values
   char *url;                 // The URL requested, without hostname or GET arguments
   char *getArgs;             // The GET arguments for this request, if any.
   const void *cgiArg;        // Argument to the CGI function, as stated as the 3rd argument of
                              // the builtInUrls entry that referred to the CGI function.
   const void *cgiArg2;       // 4th argument of the builtInUrls entries, used to pass template file to the tpl handler.
   void *cgiData;             // Opaque data pointer for the CGI function
   char *hostName;            // Host name field of request
   HttpdPriv *priv;           // Opaque pointer to data for internal httpd housekeeping
   cgiSendCallback cgi;       // CGI function pointer
   cgiRecvHandler recvHdl;    // Handler for data received after headers, if any
   HttpdPostData post;        // POST data structure
   bool isConnectionClosed;
};

// A struct describing an url. This is the main struct that's used to send different URL requests to
// different routines.
typedef struct
{
   const char *url;
   cgiSendCallback cgiCb;
   const void *cgiArg;
   const void *cgiArg2;
} HttpdBuiltInUrl;

const char* ICACHE_FLASH_ATTR httpdGetVersion( void );
void ICACHE_FLASH_ATTR httpdRedirect( HttpdConnData *connData, const char *newUrl );

// Decode a percent-encoded value.
// Takes the valLen bytes stored in val, and converts it into at most retLen bytes that
// are stored in the ret buffer. ret is always null terminated.
// @return True if decoding fit into the ret buffer, false if not

bool ICACHE_FLASH_ATTR httpdUrlDecode( const char *val, int valLen, char *ret, int retLen, int* bytesWritten );
int  ICACHE_FLASH_ATTR httpdFindArg( const char *line, const char *arg, char *buf, int buffLen );

typedef enum
{
   HTTPD_FLAG_NONE = ( 1 << 0 ),
   HTTPD_FLAG_SSL = ( 1 << 1 )
} HttpdFlags;

typedef enum
{
   InitializationSuccess,
   OutOfMemory
} HttpdInitStatus;

/** Common elements to the core server code */
typedef struct HttpdInstance
{
   const HttpdBuiltInUrl *builtInUrls;
   int maxConnections;
} HttpdInstance;

typedef enum
{
   CallbackSuccess,
   CallbackErrorOutOfConnections,
   CallbackErrorCannotFindConnection,
   CallbackErrorMemory,
   CallbackError
} CallbackStatus;

const char* ICACHE_FLASH_ATTR httpdGetMimetype( const char *url );
const char* ICACHE_FLASH_ATTR httpdMethodName( RequestTypes m );
void ICACHE_FLASH_ATTR httpdSetTransferMode( HttpdConnData *connData, TransferModes mode );
void ICACHE_FLASH_ATTR httpdStartResponse( HttpdConnData *connData, int code );
void ICACHE_FLASH_ATTR httpdHeader( HttpdConnData *connData, const char *field, const char *val );
void ICACHE_FLASH_ATTR httpdEndHeaders( HttpdConnData *connData );

/**
 * Get the value of a certain header in the HTTP client head
 * Returns true when found, false when not found.
 *
 * NOTE: 'ret' will be null terminated
 *
 * @param retLen is the number of bytes available in 'ret'
 */
bool ICACHE_FLASH_ATTR httpdGetHeader( HttpdConnData *connData, const char *header, char *ret, int retLen );

int  ICACHE_FLASH_ATTR httpdSend( HttpdConnData *connData, const char *data, int len );
int  ICACHE_FLASH_ATTR httpdSend_js( HttpdConnData *connData, const char *data, int len );
int  ICACHE_FLASH_ATTR httpdSend_html( HttpdConnData *connData, const char *data, int len );
bool ICACHE_FLASH_ATTR httpdFlushSendBuffer( HttpdInstance *pInstance, HttpdConnData *connData );
CallbackStatus ICACHE_FLASH_ATTR httpdContinue( HttpdInstance *pInstance, HttpdConnData *connData );
CallbackStatus ICACHE_FLASH_ATTR httpdConnSendStart( HttpdInstance *pInstance, HttpdConnData *connData );
void ICACHE_FLASH_ATTR httpdConnSendFinish( HttpdInstance *pInstance, HttpdConnData *connData );
void ICACHE_FLASH_ATTR httpdAddCacheHeaders( HttpdConnData *connData, const char *mime );
void ICACHE_FLASH_ATTR httdResponseOptions( HttpdConnData *connData, int cors );

// Platform dependent code should call these.
CallbackStatus ICACHE_FLASH_ATTR httpdSentCb( HttpdInstance *pInstance, HttpdConnData *connData );
CallbackStatus ICACHE_FLASH_ATTR httpdRecvCb( HttpdInstance *pInstance, HttpdConnData *connData, char *data, unsigned short len );
CallbackStatus ICACHE_FLASH_ATTR httpdDisconCb( HttpdInstance *pInstance, HttpdConnData *connData );

/** NOTE: httpdConnectCb() cannot fail */
void ICACHE_FLASH_ATTR httpdConnectCb( HttpdInstance *pInstance, HttpdConnData *connData );
void ICACHE_FLASH_ATTR httpdSetName( const char *name );

#define esp_container_of( ptr, type, member ) ( {                      \
        const typeof( ( (type * )0 )->member ) *__mptr = ( ptr );    \
        ( type * )( ( char * )__mptr - offsetof( type,member ) );} )

#ifdef CONFIG_ESPHTTPD_SHUTDOWN_SUPPORT
void ICACHE_FLASH_ATTR httpdShutdown( HttpdInstance *pInstance );
#endif

#endif
