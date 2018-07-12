#ifndef __IO_H__
#define __IO_H__

int io_input_get( int8_t pin );
void io_output_set( int8_t pin, uint8_t val );
void ICACHE_FLASH_ATTR io_pins_init( void );

#endif  // __IO_H__