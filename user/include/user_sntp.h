// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_sntp.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   add prototype for sntpStart(), sntpStop()
//    2017-08-09  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __USER_SNTP_H__
#define __USER_SNTP_H__

#include "sntp_client.h"    // sntp_event_t

void ICACHE_FLASH_ATTR sntpInit( void );
void ICACHE_FLASH_ATTR sntpStart( void );
void ICACHE_FLASH_ATTR sntpStop( void );

#endif // __USER_SNTP_H__