// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_wifi.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-18  AWe   move the wifi stuff to user_wifi.c. Add an event handler there
//    2018-04-18  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __USER_WIFI_h__
#define __USER_WIFI_h__

#include "user_interface.h"         // System_Event_t

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int  ICACHE_FLASH_ATTR wifiEnableReconnect( int enable );

void ICACHE_FLASH_ATTR wifiHandleEventsCb( System_Event_t *evt );
int  ICACHE_FLASH_ATTR wifiSetupStationMode( void );
int  ICACHE_FLASH_ATTR wifiSetupSoftApMode( void );
void ICACHE_FLASH_ATTR wifiWpsStart( void );
void ICACHE_FLASH_ATTR wifiInit( void );

#endif // __USER_WIFI_h__