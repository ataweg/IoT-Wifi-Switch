#ifndef __OS_PLATFORM_H__
#define __OS_PLATFORM_H__

   #include <c_types.h>             // size_t

   #define malloc( x )           os_malloc( x )
   #define zalloc( x )           os_zalloc( x )
   #define free( x )             os_free( x )

   #define memcmp( a, b, c )     os_memcmp( a, b, c )
   #define memcpy( x, a, b )     os_memcpy( x, a, b )
   #define memset( x, a, b )     os_memset( x, a, b )

   #define strcat( a, b )        os_strcat( a, b )
   #define strcmp( a, b )        os_strcmp( a, b )
   #define strcpy( a, b )        os_strcpy( a, b )
   #define strlen( a )           os_strlen( a )
   #define strncmp( a, b, c )    os_strncmp( a, b, c )
   #define strncpy( a, b, c )    os_strncpy( a, b, c )
   #define strstr( a, b )        os_strstr( a, b )

   #define printf( ... )         os_printf( __VA_ARGS__ )
   #define snprintf( ... )       os_snprintf( __VA_ARGS__ )
   #define sprintf( str, ... )   os_sprintf( str, __VA_ARGS__ )

#endif // __OS_PLATFORM_H__