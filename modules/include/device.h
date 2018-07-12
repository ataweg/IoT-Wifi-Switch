#ifndef __DEVICE_H__
#define __DEVICE_H__


int ICACHE_FLASH_ATTR devSet( enum _devices dev, int val );
int ICACHE_FLASH_ATTR devGet( enum _devices dev );

int ICACHE_FLASH_ATTR switchRelay( int val );
int ICACHE_FLASH_ATTR toggleRelay( void );

#endif // __DEVICE_H__