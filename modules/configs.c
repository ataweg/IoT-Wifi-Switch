// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          configs.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-24  AWe   add FillData near the end of a block, when a new write has no place there
//    2017-12-13  AWe   implement new ringbuffer concept
//    2017-11-29  AWe   initial implementation
//                      see C:\Espressif\ESP8266_NONOS_SDK\examples\esp_mqtt_proj\modules\config.c
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "modules/configs.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

#ifdef ESP_PLATFORM
   #ifndef ASSERT
     #define ASSERT( msg, expr, ...)  \
       do { if( !( expr  )) \
         ESP_LOGE( TAG, "ASSERT line: %d: "msg, __LINE__,  ##__VA_ARGS__  ); } while(0)
   #endif
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef ESP_PLATFORM
   #include <stdio.h>               // printf()
   #include <string.h>              // memcpy()

   #include "esp_types.h"
   #include "esp_spi_flash.h"
   #include "lwip/ip_addr.h"        // IP4_ADDR()
   #include "tcpip_adapter.h"       // IPSTR, IP2STR()
#else
   #include <stdlib.h>              // strtol()

   #include <osapi.h>
   #include <user_interface.h>
   #include "os_platform.h"         // malloc(), ...

   #include "ip_addr.h"             // IP2STR, IPSTR, IP4_ADDR()
   #include "mem.h"                 // os_zalloc()
#endif


#include "configs.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// INITDATAPOS points to the spi flash address with the systems initialization data.
// Before this address we can place our configuration data. For ESP-8266 there is
// an extra sector with data used by the system. We have to take this into account.

#define EXTRA_BLOCKS_AT_END          1

#define CFG_DATA_NUM_BLOCKS          3
#define CFG_DATA_START_SEC           ( INITDATAPOS/SPI_FLASH_SEC_SIZE - ( CFG_DATA_NUM_BLOCKS + EXTRA_BLOCKS_AT_END ) )

#define CFG_DATA_START_ADDR          ( INITDATAPOS - ( CFG_DATA_NUM_BLOCKS + EXTRA_BLOCKS_AT_END ) * SPI_FLASH_SEC_SIZE )
#define CFG_START_MARKER             0xE5676643

#define NO_DATA_AVAILABLE           -1
#define NO_WRITE_BLOCK_AVAILABLE    -2
#define NO_MEMORY_AVAILABLE         -3

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR align_len( int buf_size, int *len );
static int ICACHE_FLASH_ATTR config_get_defaults( const_settings_t *defaults );
static int ICACHE_FLASH_ATTR config_get_user( void );
static int ICACHE_FLASH_ATTR user_config_get_start( void );
static int ICACHE_FLASH_ATTR user_config_check_integrity( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
   uint32_t *buf;
   uint32_t start;      // 8 .. 0x2FFF; start address of record in flash, offset to CFG_DATA_START_ADDR
   uint32_t write;      // 8 .. 0x2FFF; address for the next record to write
} user_settings_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

user_settings_t user_settings = { NULL, 0, 0 };

static settings_t *config_list;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR str2int( const char *str )
{
   int value = 0;

   value = strtol( str, NULL, 0 );

   return value;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR str2ip( const char* str, void *ip )
{
   /* The count of the number of bytes processed. */
   int i;
   /* A pointer to the next digit to process. */
   const char *start;

   start = str;
   for( i = 0; i < 4; i++ )
   {
      /* The digit being processed. */
      char c;
      /* The value of this byte. */
      int n = 0;
      while( 1 )
      {
         c = *start;
         start++;
         if( c >= '0' && c <= '9' )
         {
            n *= 10;
            n += c - '0';
         }
         /* We insist on stopping at "." if we are still parsing
            the first, second, or third numbers. If we have reached
            the end of the numbers, we will allow any character. */
         else if( ( i < 3 && c == '.' ) || i == 3 )
            break;
         else
            return 0;
      }
      if( n >= 256 )
         return 0;

      ( ( uint8_t* )ip )[i] = n;
   }
   return 1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR str2int_array( char* str, int* array, int num_values )
{
   // const char str[] = "comma separated,input,,,some fields,,empty";
   const char delims[] = ",";
   int cnt = 0;
   int value = 0;

   do
   {
      if( array != NULL && cnt >= num_values ) break;
      size_t len = strcspn( str, delims );
      // ESP_LOGD( TAG, "\"%.*s\" - %d", ( int )len, str, ( int )len );

      if( len > 0 )
      {
         value = strtol( str, NULL, 0 );
         // ESP_LOGD( TAG, "%3d %8d", cnt, value );

         if( array != NULL )
            array[ cnt ] = value;
         cnt++;
      }
      str += len;
   }
   while( *str++ );

   return cnt;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// if len is equal or greater than bufsize,
//    len will be one less for the terminating zero

static int ICACHE_FLASH_ATTR align_len( int buf_size, int *len )
{
   if( *len >= buf_size )     // space for the terminating zero?
      *len = buf_size - 1 ;   // no: make it

   int len4 = ( *len + 3 ) & ~3;
   return len4;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Configuration_List_t *Configuration_List = NULL;
int Num_Configurations = 0;

void ICACHE_FLASH_ATTR config_build_list( Configuration_List_t *cfg_list, int num_cfgs )
{
   ESP_LOGD( TAG, "config_build_list" );

   // first parse default settings, then go to the user settings in the
   // configuration section of the flash

   config_list = ( settings_t * )zalloc( sizeof( settings_t ) * ID_MAX );

   Configuration_List = cfg_list;
   Num_Configurations = num_cfgs;

   // first do the default values
   for( int i = 0; i < num_cfgs; i++ )
   {
      const_settings_t *defaults = cfg_list[ i ]->defaults;
      if( defaults != NULL )
         config_get_defaults( defaults );
   }

   // parse the list stored in the configuration section of the flash
   config_get_user();
}

Configuration_List_t * ICACHE_FLASH_ATTR get_configuration_list( void )
{
   return Configuration_List;
}

int ICACHE_FLASH_ATTR get_num_configurations( void )
{
   return Num_Configurations;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// read the default configurateion database from the flash
// convert the value string to the corresponding value type and store
// them in the config_list
// the text value points to the string in the default_settings
// value array need their own space in the memory

static int ICACHE_FLASH_ATTR config_get_defaults( const_settings_t *defaults )
{
   ESP_LOGD( TAG, "config_get_default" );

   int i = 0;
   while( true )
   {
      cfg_mode_t cfg_mode __attribute__( ( aligned( 4 ) ) );

      ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )( &defaults[ i ].mode ) & 3 ) == 0 );
      ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )( &cfg_mode ) & 3 ) == 0 );
      memcpy( &cfg_mode, &defaults[ i ].mode, sizeof( cfg_mode ) );

      if( cfg_mode.mode == 0xFFFFFFFF ) // end of list
      {
         return true;
      }

      if( cfg_mode.valid >= RECORD_VALID && cfg_mode.id < ID_MAX )
      {
         if( cfg_mode.type == Text )
         {
            // copy mode and pointer to the text
            ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )( &defaults[ i ] ) & 3 ) == 0 );
            ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )( &config_list[ cfg_mode.id ] ) & 3 ) == 0 );
            memcpy( &config_list[ cfg_mode.id ], &defaults[ i ], sizeof( settings_t ) );
         }
         else if( cfg_mode.type == NumArray )
         {
            // alloate memory for the text/string from the defaults
            // len is the number of bytes of the string with the values
            int len = cfg_mode.len;
            int len4  = ( len + 3 ) & ~3;

            char *buf = malloc( len4 );
            if( buf == NULL )
            {
               // ESP_LOGE( TAG, "cannot allocate memory for \"buf\"" );
            }
            else
            {
               // copy the value string from the defaults to buf
               ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )defaults[ i ].text & 3 ) == 0 );
               ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )buf & 3 ) == 0 );
               memcpy( buf, defaults[ i ].text, len4 );
               buf[ len ] = 0;  // terminate the string

               // determine the number of integers in the text string
               int cnt = str2int_array( buf, NULL, 0 );
               // ESP_LOGD( TAG, "NumArray has %d elements", cnt );

               if( cnt > 0 )
               {
                  // allocate memory to store the array of integers
                  // check if memory is available for the integer and it's big enough
                  int *val_array = ( int* ) config_list[ cfg_mode.id ].text;
                  if( val_array != NULL )
                  {
                     int size_array = config_list[ cfg_mode.id ].len;
                     if( cnt != size_array )
                     {
                        // not enough space to store the values into array
                        free( val_array );
                        val_array = NULL;
                     }
                  }

                  if( val_array == NULL )
                     val_array = ( int * ) malloc( sizeof( int ) * cnt );

                  if( val_array != NULL )
                     str2int_array( buf, val_array, cnt );
                  else
                     cnt = 0;

                  cfg_mode.len = sizeof( uint32_t ) * cnt;
                  config_list[ cfg_mode.id ].mode = cfg_mode.mode;
                  config_list[ cfg_mode.id ].text = ( char * )val_array;

                  free( buf );
               }
               else
               {
                  // no values in array
                  cfg_mode.len = 0;
                  config_list[ cfg_mode.id ].mode = cfg_mode.mode;
                  config_list[ cfg_mode.id ].text = NULL;
               }
            }
         }
         else
         {
            char buf[64 + 1] __attribute__( ( aligned( 4 ) ) );  // one more for terminating zero

            uint32_t val = 0;
            int len = cfg_mode.len;
            int len4 = align_len( sizeof( buf ), &len );

            ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )defaults[ i ].text & 3 ) == 0 );
            ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )buf & 3 ) == 0 );
            memcpy( buf, defaults[ i ].text, len4 );
            buf[ len ] = 0;  // zero terminate string

            if( cfg_mode.type == Number )
            {
               val = str2int( buf );
            }
            else if( cfg_mode.type == Ip_Addr )
            {
               str2ip( buf, &val );
            }
            else if( cfg_mode.type == Flag )
            {
               val = str2int( buf );
               if( val != 0 )
                  val = 1;
            }
            else
            {
               val = 0;
            }

            cfg_mode.len = sizeof( uint32_t );  // because value is not a string
            config_list[ cfg_mode.id ].mode = cfg_mode.mode;
            config_list[ cfg_mode.id ].val = val;
         }
      }
      else
      {
         // something goes wrong.
         // what to do?
      }
      i++;
   }
   // should not reached
   return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// copy the configuration string into the callers buffer
// terminate the string with zero

int ICACHE_FLASH_ATTR config_get( int id, char *buf, int buf_size )
{
   if( id == config_list[ id ].id && config_list[ id ].valid >= RECORD_VALID )
   {
      int len = config_list[ id ].len;
      int len4 = align_len( buf_size, &len );

      ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )buf & 3 ) == 0 );
      ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )config_list[ id ].text & 3 ) == 0 );
      // ESP_LOGD( TAG, "get config id 0x%02x  mode 0x%08x len %d addr 0x%08x ", id, config_list[ id ].mode, len, config_list[ id ].text );
      if( config_list[ id ].valid == DEFAULT_RECORD )
         memcpy( buf, config_list[ id ].text, len4 );
      else if( config_list[ id ].valid == SPI_FLASH_RECORD )
         user_config_read( ( uint32_t )config_list[ id ].text, buf, len4 );
      else
         len = 0;

      buf[ len ] = 0;  // zero terminate string

      // ESP_LOGD( TAG, "got config id 0x%02x len %d 0x%08x \"%s\"", id, len, config_list[ id ].text, buf );
      return len; // returns the length of the string without the termination null character
   }

   // ESP_LOGD( TAG, "config_get id 0x%02x FAILED", id );
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR config_get_int( int id )
{
   if( id == config_list[ id ].id && config_list[ id ].valid >= RECORD_VALID )
   {
      int type = config_list[ id ].type;

      switch( type )
      {
         case Number:
         case Ip_Addr:
         case Flag:
            // ESP_LOGD( TAG, "config_get_int id 0x%02x value %d", id, config_list[ id ].val );
            return config_list[ id ].val;
      }
   }
   // ESP_LOGD( TAG, "config_get_int id 0x%02x FAILED", id );
   return 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// print the values stored in the config_list
// for text read sring from default_settings or from spi_flash

void ICACHE_FLASH_ATTR config_print_settings( void )
{
   ESP_LOGD( TAG, "config_print_settings" );

   int i;
   for( i = 0; i < ID_MAX; i++ )
   {
      cfg_mode_t cfg_mode;
      uint32_t val;

      cfg_mode.mode = config_list[ i ].mode;

      if( cfg_mode.id != 0 && cfg_mode.valid >= RECORD_VALID )
      {
         if( cfg_mode.type == Text || cfg_mode.type == NumArray )
         {
            char buf[64 + 1] __attribute__( ( aligned( 4 ) ) );
            int buf_size = sizeof( buf );
            int len = cfg_mode.len;
            int len4 = align_len( buf_size, &len );

            ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )config_list[ i ].text & 3 ) == 0 );
            ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )buf & 3 ) == 0 );
            if( config_list[ i ].valid == DEFAULT_RECORD )
            {
               memcpy( buf, config_list[ i ].text, len4 );
            }
            else
            {
               uint32_t addr = ( uint32_t )config_list[ i ].text;
               user_config_read( addr, buf, len4 );
            }
            if( cfg_mode.type == Text )
            {
               buf[ len ] = 0;  // zero terminate string
               printf( "%3d: id 0x%02x Text     len %3d \"%s\"\r\n", i, cfg_mode.id, len, buf );
            }
            else
            {
               int num_vals = cfg_mode.len / sizeof( int );
               printf( "%3d: id 0x%02x NumArray len %3d ", i, cfg_mode.id, num_vals );
               int *val_array = ( int * )buf;
               for( int j = 0; j < num_vals; j++ )
               {
                  printf( "%d, ",  val_array[ j ] );
               }
               printf( "\r\n" );
            }
         }
         else if( cfg_mode.type == Number )
         {
            val = config_list[ i ].val;
            printf( "%3d: id 0x%02x Number         %d\r\n", i, cfg_mode.id, val );
         }
         else if( cfg_mode.type == Ip_Addr )
         {
            val = config_list[ i ].val;
            printf( "%3d: id 0x%02x Ip_Addr        "IPSTR"\r\n", i, cfg_mode.id, IP2STR( &val ) );
         }
         else if( cfg_mode.type == Flag )
         {
            val = config_list[ i ].val;
            printf( "%3d: id 0x%02x Flag           %s\r\n", i, cfg_mode.id, val == 0 ? "false" : "true" );
         }
         else
         {
            val = 0;
         }
      }

      system_soft_wdt_feed();
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// read user configuration from flash

/*
Q: How large is the configuration data set?
A: default configuration data set has 919 bytes.

Q: Where to save the configuration in the flash?
A: for esp-8266:
   Flash_end - 0x8000 = 0xF8000..0xFAFFFF, 3 blocks @ 4096 bytes

SPI_FLASH_SIZE  1M Byte ( ESP-201 ) 0x100000

BLANKPOS      Flash_end - 0x2000 = 0xFE000
INITDATAPOS   Flash_end - 0x4000 = 0xFC000  RF Initialization esp_init_data_default.bin 128 byte
rf_cal_sec = 256 - 5;            = 0xFB000

The user configuration is organized in a ringbuffer of flash blocks.

Initialise
----------
1 ) Search the start marker in the ringbuffer. It is at the beginning of the
   block with the first/oldest configuration data. If there is no marker
   found, the ringbuffer is empty or have no valid data. In this case check
   if all blocks are empty, and erase those who are not empty.

2 ) Read all configurtaion data and update the config_list.

3 ) After reading the last data the area for writing new data starts.

Writing new data to the ringbuffer
----------------------------------
4 ) Check if there enough space for the new data in the current block of
   the ringbuffer. If so, then write the new data to the ringbuffer.

5 ) If not, fill the remaining space with a dummy data set. When th next block
   is not empty we cannot save the new data in the ringbupper without loosing
   data in the ringbuffer.
   Write the new data to the new block in the ringbuffer.

6 ) It this block is the last free block the we have to free an older block.
   Therefore we have to copy data from the older block to the current block.
   Create a list of all valid data from the block we want to free.
   Check this list for data that are newer in the remaing blocks. Remove
   these data from the list. Copy the data in the list to the current block.
   Erase the block. This also remove the start marker. So we have to create
   a new start marker at the begining of the valid data.
*/

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// setup the config_list with the user values stored in the spi flash

enum _cfg_get_user_rc
{
   fail = false,
   done = true,
   working
};

static int ICACHE_FLASH_ATTR config_get_user( void )
{
   ESP_LOGD( TAG, "config_get_user" );

   int rc = working;

   if( user_settings.buf == NULL )
      user_settings.buf = ( uint32_t* ) malloc( SPI_FLASH_SEC_SIZE );

   if( user_settings.buf != NULL )
   {
      if( user_settings.start == 0 )
         user_config_get_start();

      if( user_settings.start != 0 )
      {
         uint32_t rd_addr = user_settings.start;      // 8 .. 0x2FFF

         int i = 0;
         for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
         {
            // wrap address at user configuration data section end
            if( rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
               rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;

            if( ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 )
            {
               rd_addr += sizeof( uint32_t ) * 2; // spare reserved of the marker
            }

            uint32_t *buf32 = user_settings.buf;
            int num_words = ( SPI_FLASH_SEC_SIZE - ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) ) / sizeof( uint32_t );

            ESP_LOGD( TAG, "config_get_user from: 0x%04x", rd_addr );
            user_config_read( rd_addr, ( char * )buf32, num_words * sizeof( uint32_t ) );

            while( num_words > 0)
            {
               cfg_mode_t cfg_mode;
               cfg_mode.mode = *buf32++;
               num_words--;

               if( cfg_mode.mode == 0xFFFFFFFF ) // end of list
               {
                  user_settings.write = rd_addr;
                  ESP_LOGD( TAG, "user_settings.write: 0x%04x", rd_addr );
                  rc = done;
                  break;
               }

               rd_addr += sizeof( cfg_mode_t );

               if( cfg_mode.valid >= RECORD_VALID )
               {
                  if( cfg_mode.id < ID_MAX )
                  {
                     if( cfg_mode.type == Text || cfg_mode.type == NumArray )
                     {
                        // copy mode and text field
                        config_list[ cfg_mode.id ].mode = cfg_mode.mode;
                        // the text field points to a location in the spi flash
                        config_list[ cfg_mode.id ].text = ( char * )( rd_addr );
                        // ESP_LOGD( TAG, "got user config id 0x%02x, mode: 0x%08x text: 0x%08x",
                        //                cfg_mode.id, config_list[ cfg_mode.id ].mode, ( uint32_t )config_list[ cfg_mode.id ].text );

                        int len4 = ( cfg_mode.len + 3 ) & ~3;
                        rd_addr += len4;
                        buf32 += len4 / sizeof( uint32_t );
                        num_words -= len4 / sizeof( uint32_t );
                     }
                     else
                     {
                        config_list[ cfg_mode.id ].mode = cfg_mode.mode;
                        config_list[ cfg_mode.id ].val = *buf32++;
                        // ESP_LOGD( TAG, "got user config id 0x%02x, mode: 0x%08x value: %d", cfg_mode.id, config_list[ cfg_mode.id ].mode, config_list[ cfg_mode.id ].val );
                        rd_addr += sizeof( uint32_t );
                        num_words -= sizeof( uint32_t );
                     }
                  }
                  else  // skip extra data, also fill data
                  {
                     int len4 = ( cfg_mode.len + 3 ) & ~3;
                     rd_addr += len4;
                     buf32 += len4 / sizeof( uint32_t );
                     num_words -= len4 / sizeof( uint32_t );                                      }
               }
               else if( cfg_mode.mode == 0 )  // skip fill words
               {
               }
               else  // something goes wrong.
               {
                  ESP_LOGE( TAG, "config_get_user: something goes wrong at 0x%04x, 0x%08x", rd_addr, cfg_mode.mode );
                  // what to do?
                  rc = fail;
                  break;
               }
            }

            if( rc != working )
               break;      // leave the for loop

            ASSERT( "rd_addr doesn't match begin of next block\r\n", ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 );
         }
      }

      free( user_settings.buf );
      user_settings.buf = NULL;
   }

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR user_config_get_start( void )
{
//   ESP_LOGD( TAG, "user_config_get_start" );

   user_settings.start = 0;
   uint32_t marker_loc = 0;
   int i;
   for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
   {
      uint32_t marker[ 2 ];  // marker, record_start

      // ESP_LOGD( TAG, "look for marker %d, 0x%04x", i, marker_loc );
      user_config_read( marker_loc, ( char * )marker, sizeof( marker ) );
      if( marker[ 0 ] == CFG_START_MARKER )
      {
         // ESP_LOGD( TAG, "marker found 0x%08x 0x%08x", marker[ 0], marker[ 1 ] );
         user_settings.start = marker[ 1 ];  // start of first record
         return ( int )user_settings.start;
      }
      marker_loc += SPI_FLASH_SEC_SIZE;
   }
   // no marker found
   // check if there is a block with valid data

   ESP_LOGE( TAG, "no marker found" );
   return user_config_check_integrity();  // start of records;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// a ) check record integrity of every block,
// b ) if not ok, erase block
// c ) write marker to the end of the last block ( repair )

enum
{
   unknown      = 0,
   first_valid  = 1,
   second_valid = 2,
   maybe_valid  = 0x40,
   invalid      = 0x80,
   maybe_erased = 0xf0,
   erased       = 0xFF
};

static int ICACHE_FLASH_ATTR user_config_check_integrity( void )
{
   ESP_LOGW( TAG, "user_config_check_integrity" );

   uint8_t status [ CFG_DATA_NUM_BLOCKS ];

   int i;
   for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
   {
      status[ i ] = unknown;
      uint32_t *buf32 = user_settings.buf;
      uint32_t  addr = i * SPI_FLASH_SEC_SIZE;

      user_config_read( addr, ( char * )buf32, SPI_FLASH_SEC_SIZE );
      int num_words = SPI_FLASH_SEC_SIZE / sizeof( uint32_t );

      // first location of a block contains the marker or is erased.
      if( *buf32 == 0xFFFFFFFF )
      {
         // this block potenially is the second valid block
         // check if there are valid data
         status[ i ] = second_valid;
      }
      else if( *buf32 == CFG_START_MARKER )
      {
         // this block potenially is the first valid block
         // check if there are valid data
         status[ i ] = first_valid;
      }
      else
      {
         // block has an invalid marker
         // so a ) check if there are valid data or
         //    b ) erase this block <--
         status[ i ] = invalid;
      }
      buf32++;
      num_words--;

      if(  status[ i ] != invalid )
      {
         // check the current block
         // read the word after the marker ( first location ) and
         // skip a record which starts in the previous block
         if( *buf32 != 0xFFFFFFFF ) // end of list
         {
            uint32_t skip = *buf32 - addr - sizeof( uint32_t ) * 2;
            buf32++;
            num_words--;
            buf32 += skip / sizeof( uint32_t );
            num_words -= skip / sizeof( uint32_t );
         }

         while( num_words > 0)
         {
            cfg_mode_t cfg_mode;
            cfg_mode.mode = *buf32++;
            num_words--;

            if( cfg_mode.mode == 0xFFFFFFFF ) // end of list
            {
               // flash sector seems to be erased
               if( status[ i ] == second_valid )
               {
                  status[ i ] = erased;
               }
            }
            else
            {
               if( status[ i ] == erased )
               {
                   status[ i ] = invalid;
                   break;
               }

               if( cfg_mode.mode == 0 )  // skip fill words
               {
               }
               else if( cfg_mode.valid >= RECORD_VALID )    // check for a usable record valid code
               {
                  if( cfg_mode.type == Text || cfg_mode.type == NumArray )
                  {
                     uint32_t len4 = ( cfg_mode.len +3 ) & ~3;
                     buf32 += len4 / sizeof( uint32_t );
                     num_words -= len4 / sizeof( uint32_t );
                  }
                  else if( cfg_mode.type > Text && cfg_mode.type <= Flag )
                  {
                     buf32++;
                     num_words--;
                  }
                  else
                  {
                     // not a vaild type found
                     status[ i ] = invalid;
                     break;
                  }
               }
               else
               {
                  // record is not valid, but we can check its integrity
                  status[ i ] = invalid;
                  break;
               }
            }
         }
      }

      if( status[ i ] == invalid )
      {
         uint32_t page = i  + CFG_DATA_START_ADDR / SPI_FLASH_SEC_SIZE;
         ESP_LOGE( TAG, "Erase page 0x%04x", page );
         spi_flash_erase_sector( page );
         status[ i ] = erased;
      }
   }

   // are all blockes empty? Then write marker to the begin of the first block
   int first = -1;
   int second = -1;
   for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
   {
      // ESP_LOGD( TAG, "check integrity block %d: 0x%02x", i, status[ i ] );
      if( status[ i ] == first_valid )
         first = i;
      if( status[ i ] == second_valid )
         second = i;
   }

   uint32_t marker_loc = 0;
   if( first == -1 && second == -1 )
   {
      // no block with data found
      ESP_LOGE( TAG, "write marker" );
      uint32_t marker[ 2 ];
      marker[ 0 ] = CFG_START_MARKER;
      marker[ 1 ] = sizeof( uint32_t ) * 2; // start of first record
      user_config_write( marker_loc, ( char * )marker, sizeof( marker ) );  // initialize config start marker
   }
   else
   {
      marker_loc = first * SPI_FLASH_SEC_SIZE;
   }

   marker_loc += sizeof( uint32_t ) * 2;
   return marker_loc;  // start at addr 0x8 with reading or writing
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// read from the current block of the user configuration in the spi flash to the buffer
// process also wrap around at the end of the user configuration section
// all functions parameter must be 32bit aligned
// len includes also zero termination of a string

int ICACHE_FLASH_ATTR user_config_read( uint32_t addr, char *buf, int len )
{
   // ESP_LOGD( TAG, "user_config_read %d, 0x%04x", len, addr );

   ASSERT( "Src isn't 32bit aligned", ( addr & 3 ) == 0 );
   ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )( buf ) & 3 ) == 0 );

   int rd_len = 0;

   while( len > 0 )
   {
      int len4m = len & ~3;    // number of 32bit words to read
      int idx = addr & ( SPI_FLASH_SEC_SIZE - 1 ) ;

      // wrap address at user configuration data section end
      if( addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
      {
         ESP_LOGW( TAG, "wrap address at section end 0x%04x %d", addr, len );
         addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
      }

      uint32_t rd_addr = CFG_DATA_START_ADDR + addr;  // here 0x3FD000 + addr

      if( idx + len4m > SPI_FLASH_SEC_SIZE )
         len4m = SPI_FLASH_SEC_SIZE - idx;

      if( len4m > 0 )
      {
         // ESP_LOGD( TAG, "spi_flash_read %d, 0x%04x", len4m, rd_addr );
         spi_flash_read( rd_addr, ( uint32_t *)buf, len4m );
         addr += len4m;
         buf += len4m;
         len -= len4m;
         idx += len4m;
         rd_len += len4m;
      }
      else if( len < 4 )
      {
         uint32_t last;
         // ESP_LOGD( TAG, "spi_flash_read %d, 0x%04x", len, rd_addr );
         spi_flash_read( rd_addr, &last, sizeof( last ) );
         memcpy( buf, &last, len );
         addr += len;
         buf += len;
         len = 0;
         idx += len;
         rd_len += sizeof( uint32_t );
      }

      ASSERT( "addr overflow error 0x%04x 0x%04x", idx <= SPI_FLASH_SEC_SIZE, idx, SPI_FLASH_SEC_SIZE );
      if( idx == SPI_FLASH_SEC_SIZE )
      {
         addr = ( addr & ~( SPI_FLASH_SEC_SIZE - 1 ) ) + sizeof( uint32_t ) * 2;
         rd_len += sizeof( uint32_t ) * 2;
      }
   }

   return rd_len;    // number of bytes not read, normally zero
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// write bytes from the buffer to the user configuration space in the spi flash
// len doesn't include the terminating zero

int ICACHE_FLASH_ATTR user_config_write( uint32_t addr, char *str, int len )
{
   ASSERT( "Src isn't 32bit aligned", ( ( uint32_t )( str ) & 3 ) == 0 );
   ASSERT( "Dest isn't 32bit aligned", ( addr & 3 ) == 0 );

   int wr_len = ( len + 3 )& ~3;

   while( len > 0 )
   {
      int len4m = len & ~3;    // number of 32bit words to write
      int idx = addr & ( SPI_FLASH_SEC_SIZE - 1 ) ;

      // wrap address at user configuration data section end
      if( addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
         addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;

      uint32_t wr_addr = CFG_DATA_START_ADDR + addr;

      if( idx + len > SPI_FLASH_SEC_SIZE )
         len = SPI_FLASH_SEC_SIZE - idx;        // number of bytes to write to the current block

      if( len4m > 0 )
      {
         // ESP_LOGD( TAG, "spi flash write %d bytes, 0x%04x", len, wr_addr );
         spi_flash_write( wr_addr, ( uint32_t * )str, len4m );
         addr += len4m;
         str += len4m;
         len -= len4m;
         idx += len4m;
      }
      else if( len < 4 )
      {
         uint32_t last = 0;
         memcpy( &last, str, len );
         // ESP_LOGD( TAG, "spi flash write %d, 0x%04x", len, wr_addr );
         spi_flash_write( wr_addr, &last, sizeof( last ) );
         addr += len;
         str += len;
         len = 0;
         idx += len;
      }

      if( idx == SPI_FLASH_SEC_SIZE )
      {
         // set the start address of the next record in the new block
         addr &= ~( SPI_FLASH_SEC_SIZE - 1 );

         uint32_t start = addr + ( ( len + 3 ) & ~3 ) + sizeof( uint32_t ) * 2;
         uint32_t wr_addr = CFG_DATA_START_ADDR + addr + sizeof( uint32_t );
         // ESP_LOGD( TAG, "spi flash write at 0x%04x next record 0x%04x", wr_addr, start );
         spi_flash_write( wr_addr, &start, sizeof( start ) );  // write record start address
         addr += sizeof( uint32_t ) * 2;
         wr_len += sizeof( uint32_t ) * 2;
      }
      else if( idx > SPI_FLASH_SEC_SIZE )
      {
         ESP_LOGE( TAG, "addr overflow error 0x%04x 0x%04x", idx, SPI_FLASH_SEC_SIZE );
      }
   }

   return wr_len;    // number of 32bit aligned bytes written
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR config_save_str( int id, char *str, int len, int type )
{
   ESP_LOGD( TAG, "config_save_str 0x%02x '%s', len %d, type %d", id, S( str ), len, type );

   // write new value to the user configuration section in the flash
   cfg_mode_t cfg_mode;
   cfg_mode.id = id;
   cfg_mode.type = type;
   cfg_mode.len = len == 0 ? strlen( str ) : len; // don't save termination zero
   cfg_mode.valid = SPI_FLASH_RECORD;

   // write new str to the user configuration section in the flash
   uint32_t wr_addr = config_save( cfg_mode, str, 0 );
   if( wr_addr > 0 && id < ID_MAX )
   {
      // update config_list
      config_list[ id ].mode = cfg_mode.mode;
      config_list[ id ].text = ( char * )wr_addr;
   }
   return ( char * )wr_addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

char* ICACHE_FLASH_ATTR config_save_int( int id, int value, int type )
{
   ESP_LOGD( TAG, "config_save_int 0x%02x %d %d", id, value, type );

   cfg_mode_t cfg_mode;
   cfg_mode.id = id;
   cfg_mode.type = type;
   cfg_mode.len = sizeof( uint32_t );  // because value is not a string
   cfg_mode.valid = SPI_FLASH_RECORD;

   // update config_list
   config_list[ id ].mode = cfg_mode.mode;
   config_list[ id ].val = value;

   // write new value to the user configuration section in the flash
   uint32_t wr_addr = config_save( cfg_mode, NULL, value );

   return ( char * )wr_addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// write value to the spi flash
// treat NumArray as well as Text, len is a multiple of four bytes
// save strings without terminating zero, but fill 32 bit words with zeros if needed

int ICACHE_FLASH_ATTR config_save( cfg_mode_t cfg_mode, char *str, int value )
{
   ASSERT( "str isn't 32bit aligned", ( ( uint32_t ) str & 3 ) == 0 );
   ESP_LOGD( TAG, "config_save 0x%08x %s %d", cfg_mode.mode, S( str ), value );

   // get the address to save the configuration record
   uint32_t addr = user_settings.write;   // 8 .. 0x2FFF; address for the next record to write
   int wr_addr = -1;                      // at this address the string or value is stored in to the spi flash

   // check if write will overflows into next block
   // if so, check if the block following the next on is the last empty block
   // the first/start block follows the last empty block, so check for the start/first block

   int len = cfg_mode.len;
   int len4 = ( len + 3 ) & ~3;
   int current_block = addr / SPI_FLASH_SEC_SIZE;
   int next_block = ( addr + sizeof( cfg_mode_t ) + len4 - 1 ) / SPI_FLASH_SEC_SIZE;

   if( next_block != current_block )
   {
      ESP_LOGW( TAG, "will write %d bytes into next block from 0x%04x to 0x%04x", len4, addr, addr + len4 );

      int gap_size = SPI_FLASH_SEC_SIZE - ( addr & ( SPI_FLASH_SEC_SIZE - 1 ) );
      // write skip date until the end of the current block
      cfg_mode_t cfg_mode_skip =
      {
         .id    = ID_SKIP_DATA,
         .type  = FillData,
         .len   = gap_size - sizeof( cfg_mode_t ),
         .valid = SPI_FLASH_RECORD,
      };

      ESP_LOGI( TAG, "Fill gap from 0x%04x gap_size %d", addr, gap_size );
      user_config_write( addr, ( char * )&cfg_mode_skip.mode, sizeof( cfg_mode_t ) );
      addr += + gap_size;
      addr += sizeof( uint32_t ) * 2;  // reserve the first two words for a marker
      current_block++;
      ESP_LOGI( TAG, "next addr to write 0x%04x", addr );

      // check if the 2nd next block is the last empty block
      // the first/start block follows the last empty block, so check for the start/first block
      // read marker_loc one blocks ahead

      int first = ( current_block + 1 ) % CFG_DATA_NUM_BLOCKS;
      uint32_t marker_loc = first * SPI_FLASH_SEC_SIZE;
      uint32_t marker[ 2 ];

      ESP_LOGW( TAG, "look for marker at next block %d, 0x%04x", first, marker_loc );
      user_config_read( marker_loc, ( char * )marker, sizeof( marker ) );
      if( marker[ 0 ] == CFG_START_MARKER )
      {
         // if so: copy the records from the oldest block to the current block.
         // Erase the oldest block.
         ESP_LOGW( TAG, "if so: copy the records from the oldest block to the current block" );

         // go thru the config_list and look for records in the first block
         // and copy them to the current block
         int i;
         for( i = 0; i < ID_MAX; i++ )
         {
            settings_t *cfg = &config_list[ i ];

            if( cfg->id != 0 && cfg->valid == SPI_FLASH_RECORD )  // user defeined record stored in spi flash
            {
               if( cfg->type == Text || cfg->type == NumArray || cfg->type == Structure )
               {
                  uint32_t text = ( uint32_t )cfg->text ;
                  if( ( text / SPI_FLASH_SEC_SIZE ) == first )
                  {
                     int wr_len = user_config_write( addr, ( char * )&cfg->mode, sizeof( cfg_mode_t ) );
                     addr += wr_len;

                     char buf[ cfg->len + 1 ];
                     user_config_read( text, buf, cfg->len );

                     ESP_LOGW( TAG, "spi flash write %d bytes of \"%s\" to 0x%04x", len, S( buf ), addr );
                     wr_len = user_config_write( addr, buf, len );
                     addr += wr_len;
                  }
               }
               else if( cfg->type > Text && cfg->type <= Flag )
               {
                  // Save non-text records regardless of whether it
                  // has already been defined in other blocks.
                  int wr_len = user_config_write( addr, ( char * )cfg, sizeof( settings_t ) );
                  addr += wr_len;
               }
            }
         }

         // go to the first block and look for valid extra data record
         // and copy them to the current block

         int num_words = SPI_FLASH_SEC_SIZE / sizeof( uint32_t );
         uint32_t rd_addr = marker_loc + sizeof( marker );
         num_words--;
         num_words--;

         while( num_words > 0)
         {
            cfg_mode_t cfg_mode;
            spi_flash_read( rd_addr, ( uint32_t *)&cfg_mode, sizeof(cfg_mode ) );
            num_words--;

            if( cfg_mode.mode == 0xFFFFFFFF ) // end of list
            {
               break;
            }
            else if( cfg_mode.mode == 0 )   // skip fill words
            {
            }
            else if( cfg_mode.valid >= RECORD_VALID )    // check for a usable record valid code
            {
               uint32_t len4 = ( cfg_mode.len + 3 ) & ~3;

               if( cfg_mode.id == ID_EXTRA_DATA && cfg_mode.valid != RECORD_ERASED )
               {
                  uint8_t buf[ len4 ];
                  spi_flash_read( rd_addr, ( uint32_t *)buf, len4 );

                  int wr_len = user_config_write( addr, ( char * )&cfg_mode, sizeof( cfg_mode_t ) );
                  addr += wr_len;
                  wr_len = user_config_write( addr, ( char * )buf, len4 );
                  addr += wr_len;
                  num_words -= len4 / sizeof( uint32_t );
               }

               rd_addr += len4;
               num_words -= len4 / sizeof( uint32_t );
            }
            else   // record is not valid
            {
            }
         }

         // erase start block
         uint32_t page = first  + CFG_DATA_START_ADDR / SPI_FLASH_SEC_SIZE;
         ESP_LOGW( TAG, "Erase page 0x%04x", page );
         spi_flash_erase_sector( page );

         // write marker to its next block
         int next = ( first + 1 )  % CFG_DATA_NUM_BLOCKS;
         uint32_t marker_loc = next * SPI_FLASH_SEC_SIZE;
         marker[ 0 ] = CFG_START_MARKER;
         marker[ 1 ] = marker_loc + sizeof( uint32_t ) * 2; // start of first record
         ESP_LOGW( TAG, "write marker to block %d at 0x%08x", next, marker_loc );
         user_config_write( marker_loc, ( char * )&marker[0], sizeof( marker ) );  // initialize config start marker
      }
   }

   // write the record to the spi flash.
   if( cfg_mode.type == Text || cfg_mode.type == NumArray || cfg_mode.type == Structure )
   {
      int wr_len = user_config_write( addr, ( char * )&cfg_mode.mode, sizeof( cfg_mode_t ) );
      addr += wr_len;
      wr_addr = addr;
      ESP_LOGD( TAG, "spi flash write %d bytes of \"%s\" to 0x%04x", len, str, addr );
      wr_len = user_config_write( addr, str, len );
      addr += wr_len;
   }
   else if( cfg_mode.type > Text && cfg_mode.type <= Flag )
   {
      int wr_len = user_config_write( addr, ( char * )&cfg_mode.mode, sizeof( cfg_mode ) );
      addr += wr_len;
      wr_addr = addr;
      ESP_LOGD( TAG, "spi_flash_write value 0x%08x to 0x%04x", value, addr );
      wr_len = user_config_write( addr, ( char * )&value, sizeof( value ) );
      addr += wr_len;
   }

   user_settings.write = addr;
   ESP_LOGD( TAG, "user_settings.write: 0x%04x", wr_addr );
   return wr_addr;   // start address in spi flash of last written string or value
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// dump the settings stored in the spi flash

int ICACHE_FLASH_ATTR user_config_print( void )
{
   // ESP_LOGD( TAG, "user_config_print" );

   int rc = working;

   if( user_settings.buf == NULL )
      user_settings.buf = ( uint32_t* ) malloc( SPI_FLASH_SEC_SIZE );

   if( user_settings.buf != NULL )
   {
      if( user_settings.start == 0 )
         user_config_get_start();

      if( user_settings.start != 0 )
      {
         uint32_t rd_addr = user_settings.start;      // 8 .. 0x2FFF

         printf( "\r\n+--------+------+----------+-----+-----+------------------------\r\n" );
         printf(     "| Addr   | id   | Type     |valid| Len | Value\r\n" );
         printf(     "+--------+------+----------+-----+-----+------------------------\r\n" );

         int i = 0;
         for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
         {
            // wrap address at user configuration data section end
            if( rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
               rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;

            if( ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 )
            {
               rd_addr += sizeof( uint32_t ) * 2; // spare reserved of the marker
            }

            uint32_t *buf32 = user_settings.buf;
            int num_words = ( SPI_FLASH_SEC_SIZE - ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) ) / sizeof( uint32_t );

            // ESP_LOGD( TAG, "config_get_user from: 0x%04x", rd_addr );
            user_config_read( rd_addr, ( char * )buf32, num_words * sizeof( uint32_t ) );

            while( num_words > 0 )
            {
               cfg_mode_t cfg_mode;
               cfg_mode.mode = *buf32++;
               num_words--;
               uint32_t id_addr = rd_addr;

               if( cfg_mode.mode == 0xFFFFFFFF ) // end of list
               {
                  printf( "+--------+------+----------+-----+-----+------------------------\r\n\r\n" );

                  rc = done;
                  break;
               }

               rd_addr += sizeof( cfg_mode_t );

               if( cfg_mode.valid >= RECORD_VALID )
               {
                  if( cfg_mode.type == Text || cfg_mode.type == NumArray || cfg_mode.type == Structure  || cfg_mode.type == FillData )
                  {
                     char buf[64 + 1] __attribute__( ( aligned( 4 ) ) );  // one more for terminating zero

                     ASSERT( "Src isn't 32bit aligned", ( rd_addr & 3 ) == 0 );
                     ASSERT( "Dest isn't 32bit aligned", ( ( uint32_t )buf & 3 ) == 0 );

                     // the text field points to a location in the spi flash
                     // ESP_LOGD( TAG, "got user config id 0x%02x, mode: 0x%08x text: 0x%08x",
                     //                cfg_mode.id, cfg_mode.mode, ( uint32_t )cfg_mode.text );

                     int len = cfg_mode.len;
                     int len4 = align_len( sizeof( buf ), &len );

                     memcpy( buf, buf32, len4 );
                     rd_addr += len4;
                     buf32 += len4 / sizeof( uint32_t );
                     num_words -= len4 / sizeof( uint32_t );

                     // buf[ len ] = 0;  // terminate string
                     // ESP_LOGD( TAG, "print user text %s len: %d", buf, len );

                     if( cfg_mode.type == Text )
                     {
                        buf[ len ] = 0;  // zero terminate string
                        printf( "| 0x%04x | 0x%02x | Text     |  %2d | %3d | \"%s\"\r\n", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len, buf );
                     }
                     else if( cfg_mode.type == NumArray )
                     {
                        printf( "| 0x%04x | 0x%02x | NumArray |  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len );
                        int *val_array = ( int * )buf;
                        int num_vals = ( cfg_mode.len + sizeof( int ) - 1 ) / sizeof( int );
                        for( int j = 0; j < num_vals; j++ )
                        {
                           printf( "%d, ",  val_array[ j ] );
                        }
                        printf( "\r\n" );
                     }
                     else if( cfg_mode.type == Structure )
                     {
                        printf( "| 0x%04x | 0x%02x | Structure|  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len );
                        uint32_t *val_array = ( uint32_t * )buf;
                        int num_vals = ( cfg_mode.len + sizeof( int ) - 1 ) / sizeof( int );
                        for( int j = 0; j < num_vals; j++ )
                        {
                           printf( "0x%08x, ",  val_array[ j ] );
                        }
                        printf( "\r\n" );
                     }
                     else if( cfg_mode.type == FillData )
                     {
                        printf( "| 0x%04x | 0x%02x | FillData |  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len );
                        uint32_t *val_array = ( uint32_t * )buf;
                        int num_vals = ( cfg_mode.len + sizeof( int ) - 1 ) / sizeof( int );
                        for( int j = 0; j < num_vals; j++ )
                        {
                           printf( "0x%08x, ",  val_array[ j ] );
                        }
                        printf( "\r\n" );
                     }
                  }
                  else
                  {
                     uint32_t val = *buf32++;
                     // ESP_LOGD( TAG, "got user config id 0x%02x, mode: 0x%08x value: %d", cfg_mode.id, cfg_mode.mode, ccfg_mode.val );
                     rd_addr += sizeof( uint32_t );
                     num_words -= sizeof( uint32_t );

                     if( cfg_mode.type == Number )
                     {
                        printf( "| 0x%04x | 0x%02x | Number   |  %2d |     | 0x%08x : %10d\r\n", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val, val );
                     }
                     else if( cfg_mode.type == Ip_Addr )
                     {
                        printf( "| 0x%04x | 0x%02x | Ip_Addr  |  %2d |     | "IPSTR"\r\n", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, IP2STR( ( uint8_t* )&val ) );
                     }
                     else if( cfg_mode.type == Flag )
                     {
                        printf( "| 0x%04x | 0x%02x | Flag     |  %2d |     | %s\r\n", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val == 0 ? "false" : "true" );
                     }
                     else
                     {
                        printf( "| 0x%04x | 0x%02x | Unknown  |  %2d |     | 0x%08x : %10d\r\n", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val, val );
                     }
                  }
               }
               else if( cfg_mode.mode == 0 )  // skip fill words
               {
               }
               else   // something goes wrong.
               {
                  ESP_LOGE( TAG, "config_get_user: something goes wrong at 0x%04x, 0x%08x", rd_addr, cfg_mode.mode );
                  // what to do?
                  rc = fail;
                  break;
               }

               system_soft_wdt_feed();
            }

            if( rc != working )
               break;      // leave the for loop

            ASSERT( "rd_addr doesn't match begin of next block", ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 );
         }
      }

      free( user_settings.buf );
      user_settings.buf = NULL;
   }

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

uint32_t ICACHE_FLASH_ATTR user_config_invalidate( uint32_t addr )
{
   ESP_LOGD( TAG, "user_config_invalidate 0x%08x", addr );

   // addr points to the value or the begin of the string
   // the contol block is 4 bytes before

   uint32_t cfg_addr = addr - sizeof( cfg_mode_t );

   if( cfg_addr >= 4 && cfg_addr < SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
   {
      cfg_mode_t cfg_mode;
      uint32_t spi_addr = CFG_DATA_START_ADDR + cfg_addr;
      spi_flash_read(spi_addr, ( uint32_t *)&cfg_mode, sizeof(cfg_mode ) );
      if( cfg_mode.valid != RECORD_ERASED )
      {
         cfg_mode.valid = RECORD_ERASED;
         spi_flash_write( spi_addr, ( uint32_t *)&cfg_mode, sizeof(cfg_mode ) );
      }
   }
   else
   {
      ESP_LOGE( TAG, "addr 0x%08x out of user configuration data space", ( uint32_t )addr );
   }
   return addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// call_back( void *arg, uint32_t *flash_rd_addr )

int ICACHE_FLASH_ATTR user_config_scan( int id, int (call_back)(), void *arg )
{
   ESP_LOGD( TAG, "user_config_scan" );

   int rc = working;

   if( user_settings.start == 0 )
      user_config_get_start();

   if( user_settings.start != 0 )
   {
      uint32_t rd_addr = user_settings.start;      // 8 .. 0x2FFF
      int i = 0;
      for( i = 0; i < CFG_DATA_NUM_BLOCKS; i++ )
      {
         // wrap address at user configuration data section end
         if( rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS )
            rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;

         // parse the configuration in the spi-flash for valid switching timers setting
         if( ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 )
         {
            rd_addr += sizeof( uint32_t ) * 2; // spare reserved of the marker
         }

         int num_words = ( SPI_FLASH_SEC_SIZE - ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) ) / sizeof( uint32_t );
         while( num_words > 0)
         {
            uint32_t buf32[ 1 + 64 ];     // cfg_mode + 256 bytes
            int rd_words = sizeof( buf32 );
            if( rd_words > num_words )
               rd_words = num_words;
            user_config_read( rd_addr, ( char * )buf32, rd_words );
            cfg_mode_t *cfg_mode = ( cfg_mode_t * )buf32;

            if( cfg_mode->mode == 0xFFFFFFFF ) // end of list
            {
               user_settings.write = rd_addr;
               ESP_LOGD( TAG, "user_settings.write: 0x%04x", rd_addr );
               rc = done;
               break;
            }

            rd_addr += sizeof( cfg_mode_t );
            num_words--;

            if( cfg_mode->valid >= RECORD_VALID )
            {
               uint32_t len4 = ( cfg_mode->len + 3 ) & ~3;

               if( ( cfg_mode->id == id ) && cfg_mode->valid != RECORD_ERASED )
               {
                  if( call_back )
                     call_back( buf32, cfg_mode->len, rd_addr, arg );  // call_back function handles the destionation
               }

               rd_addr += len4;
               num_words -= len4 / sizeof( uint32_t );
            }
            else if( cfg_mode->mode == 0 )   // skip fill words
            {
            }
            else   // record is not valid
            {
               ESP_LOGW( TAG, "record is not valid: 0x%04x", rd_addr );
            }

            system_soft_wdt_feed();
         }

         if( rc != working )
            break;      // leave the for loop

         ASSERT( "rd_addr doesn't match begin of next block", ( rd_addr & ( SPI_FLASH_SEC_SIZE - 1 ) ) == 0 );
      }
   }

   return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
