// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiSwitchStatus.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------


#ifndef __CGISTATUS_H__
#define __CGISTATUS_H__

CgiStatus ICACHE_FLASH_ATTR tplSwitchStatus( HttpdConnData *connData, char *token, void **arg );


#endif // __CGISTATUS_H__
