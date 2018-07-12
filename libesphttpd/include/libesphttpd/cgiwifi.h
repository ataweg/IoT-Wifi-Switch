#ifndef CGIWIFI_H
#define CGIWIFI_H

#include "httpd.h"

int ICACHE_FLASH_ATTR rssi2perc( int rssi );
const char* ICACHE_FLASH_ATTR auth2str( int auth );
const char* ICACHE_FLASH_ATTR opmode2str( int opmode );

CgiStatus ICACHE_FLASH_ATTR cgiWiFiScan( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR tplWlan( HttpdConnData *connData, char *token, void **arg );
CgiStatus ICACHE_FLASH_ATTR cgiWiFi( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnect( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetMode( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetChannel( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnStatus( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetSSID( HttpdConnData *connData );

#endif
