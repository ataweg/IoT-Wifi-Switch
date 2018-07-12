// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_http.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-08-10  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __USER_HTTPD_H__
#define __USER_HTTPD_H__

void ICACHE_FLASH_ATTR httpdInit( void );
int  ICACHE_FLASH_ATTR httpdBroadcastStart( void );
int  ICACHE_FLASH_ATTR httpdBroadcastStop( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __USER_HTTPD_H__

