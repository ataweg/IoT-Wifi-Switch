// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiHistory.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-18  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __CGIHISTORY_H__
#define __CGIHISTORY_H__

#include "libesphttpd/httpd.h"      // CgiStatus

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
   uint8_t type;
   uint8_t len;
   uint8_t dmy;
   uint8_t id;
   time_t  time;
} history_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

CgiStatus ICACHE_FLASH_ATTR tplHistory( HttpdConnData *connData, char *token, void **arg );
int ICACHE_FLASH_ATTR history( const char *format, ... );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __CGIHISTORY_H__
