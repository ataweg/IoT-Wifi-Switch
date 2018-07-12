// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_http.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-08-19  AWe   change debug message printing
//    2017-08-10  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>   // system_get_free_heap_size()

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR dump_httpd_data( const char* data, unsigned short len )
{
   char buf[1024];
   int i;

   if( len >= sizeof( buf ) - 1 )
      len = sizeof( buf ) - 1; // 0..62
   for( i = 0; i < len; i++ )
   {
      char c = data[i];
      if( ( c < 0x20 || c > 0x7e ) && c != '\r' && c != '\n' )
         c = '.';
      buf[i] = c;
   }
   buf[i] = 0;

   os_printf( "%s\r\n", buf );
   return i;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR dump_httpd_data_hex( const char* data, unsigned short len )
{
   char buf[1024];
   int i;

   if( len >= sizeof( buf ) / 3 - 1 )
      len = sizeof( buf ) / 3 - 1; // 0..62
   for( i = 0; i < len; i++ )
   {
      char c = data[i];
      char hi = ( c >> 4 ) & 0x0F;
      char lo = ( c >> 0 ) & 0x0F;

      buf[3 * i + 0] = hi + 0x30 + ( hi >= 10 ? 7 : 0 );
      buf[3 * i + 1] = lo + 0x30 + ( lo >= 10 ? 7 : 0 );
      buf[3 * i + 2] = ' ';
   }
   buf[3 * i] = 0;

   os_printf( "%s\r\n", buf );
   return i;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR dump_heap( void )
{
   static unsigned long last_heap = 0;

   unsigned long heap = ( unsigned long )system_get_free_heap_size();

   if( heap != last_heap )
   {
      os_printf( "**  Heap: %ld\r\n", heap );
      last_heap = heap;
   }
   return last_heap;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
