#ifndef __ESP_MISSING_H__
#define __ESP_MISSING_H__

//    2018-04-09  AWe   add ets_snprintf(), ets_vsprintf( 9, ets_vsnprintf() prototypes
//    2017-12-15  AWe   add defines to map snprintf to ets_snprintf

#include <stdint.h>
#include <stdarg.h>

#ifndef ESP_PLATFORM
   #include <c_types.h>
#endif

int strcasecmp( const char *a, const char *b );

#ifndef FREERTOS
   #include <eagle_soc.h>
   #include <ets_sys.h>

   // Missing function prototypes in include folders. Gcc will warn on these if we don't define 'em anywhere.
   // MOST OF THESE ARE GUESSED! but they seem to swork and shut up the compiler.
   typedef struct espconn espconn;

   int  atoi( const char *nptr );
//-   void ets_isr_attach( int intr, void ( *handler )( void * ), void *arg );
   void ets_isr_mask( unsigned intr );
   void ets_isr_unmask( unsigned intr );
   int  ets_str2macaddr( void *, void * );
   void ets_update_cpu_frequency( int freqmhz );

   // see https://mongoose-os.com/blog/esp8266-watchdog-timer/
   void ets_wdt_init(void);
   // void ets_wdt_enable(uint32_t mode, uint32_t arg1, uint32_t arg2);
   void ets_wdt_enable(void);
   void ets_wdt_disable(void);
   void ets_wdt_restore(uint32_t mode);
   uint32_t ets_wdt_get_mode(void);

   int  ets_vsprintf( char *str, const char *format, va_list arg );
   int  ets_vsnprintf( char *buffer, size_t sizeOfBuffer, const char *format, va_list arg );

//-   // Standard PIN_FUNC_SELECT gives a warning. Replace by a non-warning one.
//-   #ifdef PIN_FUNC_SELECT
//-      #undef PIN_FUNC_SELECT
//-      #define PIN_FUNC_SELECT( PIN_NAME, FUNC )  do { \
//-          WRITE_PERI_REG( PIN_NAME,   \
//-                                      ( READ_PERI_REG( PIN_NAME ) \
//-                                           &  ( ~( PERIPHS_IO_MUX_FUNC<<PERIPHS_IO_MUX_FUNC_S )) )  \
//-                                           |( ( (( FUNC&BIT2 )<<2 )|( FUNC&0x3 ))<<PERIPHS_IO_MUX_FUNC_S ) );  \
//-          } while ( 0 )
//-   #endif

#endif   // FREERTOS

#endif // __ESP_MISSING_H__