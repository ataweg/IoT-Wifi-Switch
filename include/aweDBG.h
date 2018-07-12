// --------------------------------------------------------------------------
//
// Project       - generic -
//
// File          Axel Werner
//
// Author        aweDBG.h
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-25  AWe   dump HEAP info and ALLOC info only when global define is set
//    2017-08-19  AWe   add message levels from Apache project
//    2017-08-07  AWe   adapt for ESP8266 projects
//    28.11.2016  AWe   VERBOISTY is undefined after including this header file
//    09.08.2013  AWe   define some useful simple debug macros
//
// References
//     http://c.learncodethehardway.org/book/ex20.html
//
// --------------------------------------------------------------------------


// from http://logging.apache.org/log4j/1.2/apidocs/org/apache/log4j/Level.html
//
//  +------------+-----------------------------------------------------------+
//  |  ALL       | The ALL has the lowest possible rank and is intended to   |
//  |            | turn on all logging.                                      |
//  +------------+-----------------------------------------------------------+
//  |  DEBUG     | The DEBUG Level designates fine-grained informational     |
//  |            | events that are most useful to debug an application.      |
//  +------------+-----------------------------------------------------------+
//  |  ERROR     | The ERROR level designates error events that might still  |
//  |            | allow the application to continue running.                |
//  +------------+-----------------------------------------------------------+
//  |  FATAL     | The FATAL level designates very severe error events that  |
//  |            | will presumably lead the application to abort.            |
//  +------------+-----------------------------------------------------------+
//  |  INFO      | The INFO level designates informational messages that     |
//  |            | highlight the progress of the application at coarse-      |
//  |            | grained level.                                            |
//  +------------+-----------------------------------------------------------+
//  |  OFF       | The OFF has the highest possible rank and is intended to  |
//  |            | turn off logging.                                         |
//  +------------+-----------------------------------------------------------+
//  |  TRACE     | The TRACE Level designates finer-grained informational    |
//  |            | events than the DEBUG                                     |
//  +------------+-----------------------------------------------------------+
//  |  TRACE_INT | TRACE level integer value.                                |
//  +------------+-----------------------------------------------------------+
//  |  WARN      | The WARN level designates potentially harmful situations. |
//  +------------+-----------------------------------------------------------+

#ifndef __AWEDBG_H__
#define __AWEDBG_H__

#define V_OFF           (    0 )
#define V_FATAL         ( 1<<1 )
#define V_ERROR         ( 1<<2 )
#define V_WARNING       ( 1<<3 )
#define V_INFO          ( 1<<4 )
#define V_DEBUG         ( 1<<5 )
#define V_TRACE         ( 1<<6 )
#define V_ALL           (   -1 )

#define DBG_ENTRY       ( 1<<10 )         // on function entry
#define DBG_EXIT        ( 1<<11 )
#define DBG_LEVEL_1     ( 1<<12 )
#define DBG_LEVEL_2     ( 1<<13 )
#define DBG_LEVEL_3     ( 1<<14 )
#define DBG_LEVEL_4     ( 1<<15 )

#define V_TRACE_1       ( 1<<16 )
#define V_TRACE_2       ( 1<<17 )
#define V_TRACE_3       ( 1<<18 )
#define V_TRACE_4       ( 1<<19 )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifndef VERBOSITY
   #define VERBOSITY 0
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if defined( V_HEAP_INFO ) && defined( DBG_HEAP_INFO )
   #ifdef  __ESP_LOG_H__
         #define HEAP_INFO( msg )      os_printf( "[HEAP]\t(%s)\t%s(%d):\t%d\t%ld\t%s\n", \
                   sys_time2str( esp_log_timestamp() ), __FILE__, __LINE__, __LINE__, \
                   ( unsigned long )system_get_free_heap_size(), msg );

   #else
      #ifdef _PRINT_CHATTY
         #define HEAP_INFO( msg )      os_printf( "[HEAP]\t%s(%d):\t%d\t%ld\t%s\n", \
                   __FILE__, __LINE__, __LINE__, ( unsigned long )system_get_free_heap_size(), msg );
      #else
         #define HEAP_INFO( msg )      os_printf( "[HEAP]\t%ld\t%s\n", \
                   ( unsigned long )system_get_free_heap_size(), msg );
      #endif
   #endif
#else
   #define HEAP_INFO( msg )
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if defined( INFO ) || defined( WARNING ) || defined( ERROR ) || defined( FATAL )
   #error  "INFO, WARNING, ERROR or FATAL not used from aweDBG"
#else
   #ifdef  __ESP_LOG_H__
      #define INFO( msg, ... )       ESP_LOGI( TAG, msg, ##__VA_ARGS__ )
      #define WARNING( msg, ... )    ESP_LOGW( TAG, msg, ##__VA_ARGS__ )
      #define ERROR( msg, ... )      ESP_LOGE( TAG, msg, ##__VA_ARGS__ )
      #define FATAL( msg, ... )      ESP_LOGE( TAG, msg, ##__VA_ARGS__ )
      #define DUMP(  msg, ... )      ESP_LOGV( TAG, msg, ##__VA_ARGS__ )
   #else
      #define INFO( msg, ... )       PRINT( "INFO", V_INFO, msg, ##__VA_ARGS__ )
      #define WARNING( msg, ... )    PRINT( "WARNING", V_WARNING, msg, ##__VA_ARGS__ )
      #define ERROR( msg, ... )      PRINT( "ERROR", V_ERROR, msg, ##__VA_ARGS__ )
      #define FATAL( msg, ... )      PRINT( "FATAL", V_FATAL, msg, ##__VA_ARGS__ )
      #define DUMP(  msg, ... )      PRINT( "", V_ALL, msg, ##__VA_ARGS__ )
   #endif
#endif


#if defined( DEBUG_ON )
   #ifdef  __ESP_LOG_H__
      #define DBG( verbose_level, msg, ... )       ESP_LOGD( TAG, msg, ##__VA_ARGS__ )
   #else
      #define DBG( verbose_level, msg, ... )       PRINT( "DEBUG", verbose_level, msg, ##__VA_ARGS__ )
   #endif
#else
   #define DBG( verbose_level, msg, ... )
#endif

// ( verbose_level | 1 ) means when VERBOSE is set to 1, print the msg in all cases
// indepented from the verbose_level in the printing function

#ifdef _PRINT_CHATTY
   #define PRINT( type, verbose_level, msg, ... )  if( (( verbose_level ) | 1 ) & ( VERBOSITY )) os_printf( "[%s] %s( %d ): " msg "\r\n", type, __FILE__, __LINE__, ##__VA_ARGS__ )
#else
   #define PRINT( type, verbose_level, msg, ... )  if( (( verbose_level ) | 1 ) & ( VERBOSITY )) os_printf( "[%s]" msg  "\r\n", type, ##__VA_ARGS__ )
#endif   // _PRINT_CHATTY

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifndef ASSERT
   #ifdef __ESP_LOG_H__
      #define ASSERT( msg, expr, ...)  \
         do { if( !( expr  )) \
            ESP_LOGE( TAG, "ASSERT "msg, ##__VA_ARGS__  ); } while(0)
   #else
      #define ASSERT( msg, expr, ... )  \
          do { if( !( expr  ) ) \
             PRINT( "ASSERT", V_ALL, msg, ##__VA_ARGS__  ); } while( 0 )
   #endif
#else
   #error "ASSERT already defined"
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int dump_httpd_data( const char* data, unsigned short len );
int dump_httpd_data_hex( const char* data, unsigned short len );
int dump_heap( void );

// --------------------------------------------------------------------------
//  esp_log compatible message printing
// --------------------------------------------------------------------------
#ifdef __ESP_LOG_H__

   #ifndef ESP_PLATFORM // only set in esp-idf
      #include <c_types.h>
   #endif

   // Log level as defines

   #define ESP_LOG_NONE    ( 0 )   /*!< No log output */
   #define ESP_LOG_ERROR   ( 1 )   /*!< Critical errors, software module can not recover on its own */
   #define ESP_LOG_WARN    ( 2 )   /*!< Error conditions from which recovery measures have been taken */
   #define ESP_LOG_INFO    ( 3 )   /*!< Information messages which describe normal flow of events */
   #define ESP_LOG_DEBUG   ( 4 )   /*!< Extra information which is not necessary for normal use ( values, pointers, sizes, etc ). */
   #define ESP_LOG_VERBOSE ( 5 )   /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */

   #define CONFIG_LOG_COLORS 1

   #define esp_log_level_t uint32_t
   // esp_log_timestamp() retuns ms, but system_get_time() returns us
   #define esp_log_timestamp()         ( system_get_time() / 1000 )     // System time in microsecond

   #define esp_log_write( level, tag, format, ... )  os_printf( format, ##__VA_ARGS__ )

   esp_log_level_t level;

   #if 0 // implementation of ESP_LOG* messages not ready
      typedef int ( *vprintf_like_t )( const char *, va_list );

      void     esp_log_level_set( const char* tag, esp_log_level_t level );
      void     esp_log_set_vprintf( vprintf_like_t func );
      uint32_t esp_log_timestamp( void );
      uint32_t esp_log_early_timestamp( void );
      void esp_log_write( esp_log_level_t level, const char* tag, const char* format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );

      esp_log_buffer_hex_internal( tag, buffer, buff_len, level );
      esp_log_buffer_char_internal( tag, buffer, buff_len, level );
      esp_log_buffer_hexdump_internal( tag, buffer, buff_len, level );
   #endif
#endif // __ESP_LOG_H__

#endif  // __AWEDBG_H__

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
