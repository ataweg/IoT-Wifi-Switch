#ifndef HTTPD_PLATFORM_H
#define HTTPD_PLATFORM_H

#include "libesphttpd/platform.h"
#include "libesphttpd/httpd.h"         // HttpdInitStatus

/**
 * @return number of bytes that were written
 */
int ICACHE_FLASH_ATTR httpdPlatSendData( HttpdInstance *pInstance, HttpdConnData *connData, char *buf, int len );

void ICACHE_FLASH_ATTR httpdPlatDisconnect( HttpdConnData *ponn );
void ICACHE_FLASH_ATTR httpdPlatDisableTimeout( HttpdConnData *connData );

void ICACHE_FLASH_ATTR httpdPlatLock( HttpdInstance *pInstance );
void ICACHE_FLASH_ATTR httpdPlatUnlock( HttpdInstance *pInstance );

HttpdPlatTimerHandle ICACHE_FLASH_ATTR httpdPlatTimerCreate( const char *name, int periodMs, int autoreload, void ( *callback )( void *arg ), void *ctx );
void ICACHE_FLASH_ATTR httpdPlatTimerStart( HttpdPlatTimerHandle timer );
void ICACHE_FLASH_ATTR httpdPlatTimerStop( HttpdPlatTimerHandle timer );
void ICACHE_FLASH_ATTR httpdPlatTimerDelete( HttpdPlatTimerHandle timer );

#ifdef CONFIG_ESPHTTPD_SHUTDOWN_SUPPORT
void ICACHE_FLASH_ATTR httpdPlatShutdown( HttpdInstance *pInstance );
#endif

#endif