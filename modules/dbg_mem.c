
//    2018-04-16  AWe   create my own debug functions for memory leaks
// -----------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>      // system_get_time()

#include "mem.h"
#include "os_platform.h"         // malloc(), ...

extern char* ICACHE_FLASH_ATTR sys_time2str( uint32_t sys_time );
extern uint32_t esp_log_timestamp( void );

#define esp_log_timestamp()         ( system_get_time() / 1000 )     // System time in microsecond

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

void dbg_free( void *p, const char *file, unsigned line )
{
   printf( "[MEM_LEAK]\t(%s)\t%s(%d):\tfree\t\tat\t0x%08x\n",
      sys_time2str( esp_log_timestamp() ), file, line, ( uint32_t ) p );
   vPortFree( p, "", line );
}

void *dbg_malloc( size_t s, const char *file, unsigned line, bool use_iram )
{
   void *p = pvPortMalloc( s, "", line, use_iram );
   printf( "[MEM_LEAK]\t(%s)\t%s(%d):\tmallocate\t%d\tbytes at\t0x%08x\n",
      sys_time2str( esp_log_timestamp() ), file, line, s, p );
   return p;
}

void *dbg_calloc( size_t l, size_t s, const char *file, unsigned line )
{
   void *p = pvPortCallocIram( l, s, "", line );
   printf( "[MEM_LEAK]\t(%s)\t%s(%d):\tcallocate\t%d\tbytes at\t0x%08x\n",
      sys_time2str( esp_log_timestamp() ), file, line, l * s, p );
   return p;
}

void *dbg_realloc( void *p, size_t s, const char *file, unsigned line )
{
   void *q = pvPortRealloc( p, s, "", line );
   printf( "[MEM_LEAK]\t(%s)\t%s(%d):\treallocate\t%d\tbytes from\t0x%08x to 0x%08x \n",
      sys_time2str( esp_log_timestamp() ), file, line, s, p, q );
   return q;
}

void *dbg_zalloc( size_t s, const char *file, unsigned line )
{
   void *p = pvPortZallocIram( s, "", line );
   printf( "[MEM_LEAK]\t(%s)\t%s(%d):\tzallocate\t%d\tbytes at\t0x%08x\n",
      sys_time2str( esp_log_timestamp() ), file, line, s, p );
   return p;
}
