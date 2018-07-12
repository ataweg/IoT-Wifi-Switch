// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiConfig.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __CGICONFIG_H__
#define __CGICONFIG_H__

#include "libesphttpd/httpd.h"      // CgiStatus

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

CgiStatus ICACHE_FLASH_ATTR cgiConfig( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR tplConfig( HttpdConnData *connData, char *token, void **arg );

#endif // __CGICONFIG_H__
