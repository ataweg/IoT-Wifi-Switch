#ifndef __MEM_H__
#define __MEM_H__

#include <stddef.h>
#include <c_types.h>         // bool

// NO_DBG_MEMLEAKS can be define in a source file when memory leak check is omitted
// DEBUG_MEMLEAK is the command line option

/* Note: check_memleak_debug_enable is a weak function inside SDK.
 * please copy following codes to user_main.c.
#include "mem.h"

bool ICACHE_FLASH_ATTR check_memleak_debug_enable(void)
{
    return MEMLEAK_DEBUG_ENABLE;
}
*/

void *pvPortMalloc( size_t sz, const char *, unsigned, bool );
void vPortFree( void *p, const char *, unsigned );
void *pvPortZalloc( size_t sz, const char *, unsigned );
void *pvPortRealloc( void *p, size_t n, const char *, unsigned );
void *pvPortCalloc( size_t count, size_t size, const char *, unsigned );
void *pvPortCallocIram( size_t count,size_t size,const char *,unsigned );
void *pvPortZallocIram( size_t sz, const char *, unsigned );

#if !defined( MEMLEAK_DEBUG ) || defined( NO_DBG_MEMLEAKS )
   #define MEMLEAK_DEBUG_ENABLE   0
   #define os_free( s )          vPortFree( s, "", __LINE__ )
   #define os_malloc( s )        pvPortMalloc( s, "", __LINE__, true )
   #define os_malloc_dram( s )   pvPortMalloc( s, "", __LINE__, false )
   #define os_calloc( l, s)      pvPortCallocIram( l, s, "", __LINE__)
   #define os_realloc( p, s )    pvPortRealloc( p, s, "", __LINE__ )
   #define os_zalloc( s )        pvPortZallocIram( s, "", __LINE__ )
#else
   void *dbg_malloc( size_t sz, const char *, unsigned, bool );
   void  dbg_free( void *p, const char *, unsigned );
   void *dbg_zalloc( size_t sz, const char *, unsigned );
   void *dbg_realloc( void *p, size_t n, const char *, unsigned );
   void *dbg_calloc( size_t, size_t, const char *, unsigned );

   #define MEMLEAK_DEBUG_ENABLE   1
   #define os_free( s )         dbg_free( s, __FILE__, __LINE__ )          // vPortFree( s, "", 0 )
   #define os_malloc( s )       dbg_malloc( s, __FILE__, __LINE__, true )  // pvPortMalloc( s, "", 0 )
   #define os_malloc_dram( s )  dbg_malloc( s, __FILE__, __LINE__, false ) // pvPortMalloc( s, "", 0 )
   #define os_calloc( l, s )    dbg_calloc( l, s, __FILE__, __LINE__ )     // pvPortCalloc( l, s, "", 0 );
   #define os_realloc( p, s )   dbg_realloc( p, s, __FILE__, __LINE__ )    // pvPortRealloc( p, s, "", 0 )
   #define os_zalloc( s )       dbg_zalloc( s, __FILE__, __LINE__ )        // pvPortZalloc( s, "", 0 )
#endif

#endif // __MEM_H__

