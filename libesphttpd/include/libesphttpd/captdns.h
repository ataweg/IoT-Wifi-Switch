#ifndef CAPTDNS_H
#define CAPTDNS_H

// NOTE: hostnames that end in '.local' appear to conflict with mDNS resolution,
//       at least in MacOS. These hostnames will not be redirected to the captive
//       dns server.
void ICACHE_FLASH_ATTR captdnsInit( void );

#endif