// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          configs.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-24  AWe   add FillData near the end of a block, when a new write has no place there
//    2017-11-29  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __CONFIGS_H__
#define __CONFIGS_H__

#include <stdint.h>

#ifdef ESP_PLATFORM // only set in esp-idf
   #define STORE_ATTR               __attribute__( ( aligned( 4 ) ) )
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define SPI_FLASH_SEC_SIZE          4096
#define SPI_FLASH_SIZE              0x400000
#define SPI_FLASH_BASE_ADDR         0x0

#ifdef ESP_PLATFORM // only set in esp-idf
    #define INITDATAPOS                 ( SPI_FLASH_BASE_ADDR + SPI_FLASH_SIZE )
#endif

// .id field
#define ID_MAX             0xF0
#define ID_EXTRA_DATA      0xF1      // extra data are not stored in the config_list[]
                                     // they are stored in the config_user date section in the spi-flash
#define ID_EXTRA_DATA_TEMP 0xF3      // record can remove whem flash is cleaned up
#define ID_SKIP_DATA       0xFF

// .valid field
#define SPI_FLASH_RECORD   0xFE      // record stored in spi flash
#define DEFAULT_RECORD     0xFF      // record from program memory
#define RECORD_ERASED      0xF0
#define RECORD_VALID       0xF0

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum
{
   Text = 1,      // texts are stored in readonly 32bit aligned memory ( irom, iram )
   Number,
   Ip_Addr,
   Flag,
   NumArray,
   Structure,
   FillData
};

typedef union
{
   struct
   {
      uint8_t id;
      uint8_t type;
      uint8_t len;
      uint8_t valid;  // >= 0xF0 record is valid
   };
   uint32_t mode;
} cfg_mode_t;

typedef struct
{
   union
   {
      struct
      {
         uint8_t id;
         uint8_t type;
         uint8_t len;      // strlen, without terminating null character
         uint8_t valid;
      };
      uint32_t mode;
   };
   union
   {
      char *text;
      uint32_t val;
   };
} settings_t;

typedef struct
{
   union
   {
      struct
      {
         uint8_t id;
         uint8_t type;
         uint8_t len;
         uint8_t valid;
      };
      uint32_t mode;
   };
   const char *text;
} const const_settings_t;

typedef struct
{
   struct
   {
      uint8_t id;
      uint8_t type;
      uint8_t dmy1, dmy2;
   };
   const char STORE_ATTR *token;
} STORE_ATTR Config_Keyword_t;

typedef struct
{
   const  Config_Keyword_t *config_keywords;
   int num_keywords;
   const_settings_t *defaults;
   int ( *get_config )( int id, char **str, int *value );
   int ( *compare_config )( int id, char *str, int value );
   int ( *update_config )( int id, char *str, int value );
   int ( *apply_config )( int id, char *str, int value );

}  Configuration_Item_t;

typedef Configuration_Item_t *Configuration_List_t;

#define NUMARRAY_SIZE  16

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef ESP_PLATFORM
   #define ICACHE_FLASH_ATTR
#endif

// called in cgiConfig.c
int  ICACHE_FLASH_ATTR str2int( const char *s );
bool ICACHE_FLASH_ATTR str2ip( const char* str, void *ip );
int  ICACHE_FLASH_ATTR str2int_array( char* str, int* array, int num_values );

// called in user_main.c
void ICACHE_FLASH_ATTR config_build_list( Configuration_List_t *cfg_list, int num_cfgs );
Configuration_List_t * ICACHE_FLASH_ATTR get_configuration_list( void );
int  ICACHE_FLASH_ATTR get_num_configurations( void );

void  ICACHE_FLASH_ATTR config_print_defaults( const_settings_t *config_defaults, int num_lists );
void  ICACHE_FLASH_ATTR config_print_settings( void );

// called in user_wifi.c, wifi_config.c, mqtt_confic.c
int   ICACHE_FLASH_ATTR config_get( int id, char *buf, int buf_size );
int   ICACHE_FLASH_ATTR config_get_int( int id );

// called in cgiConfig.c, user_wifi.c, cgiTimer.c
char* ICACHE_FLASH_ATTR config_save_str( int id, char *str, int strlen, int type );
char* ICACHE_FLASH_ATTR config_save_int( int id, int value, int type );
int ICACHE_FLASH_ATTR config_save( cfg_mode_t cfg_mode, char *str, int value );

int ICACHE_FLASH_ATTR user_config_read( uint32_t addr, char *buf, int len );
int ICACHE_FLASH_ATTR user_config_write( uint32_t addr, char *buf, int len );
int ICACHE_FLASH_ATTR user_config_scan( int id, int (call_back)(), void *arg );
uint32_t ICACHE_FLASH_ATTR user_config_invalidate( uint32_t addr );
int ICACHE_FLASH_ATTR user_config_print( void );

#endif //  __CONFIGS_H__
