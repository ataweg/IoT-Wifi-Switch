// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          cgiConfig.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-20  AWe   takeover from WebServer project and adept it
//    2018-04-09  AWe   replace httpd_printf() with ESP_LOG*()
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//                         here: replace ( connData->conn == NULL ) with ( connData->isConnectionClosed )
//    2017-11-27  AWe   derived ftom cgiStatus.c
//    2017-09-18  AWe   replace streq() with strcmp()
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "cgiConfig";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>

#ifdef ESP_PLATFORM
   #include "tcpip_adapter.h"       // IP2STR, IPSTR
#endif
#include "configs.h"         // enums, config_save_str()
#include "cgiConfig.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR get_num_keywords( void );
static const Config_Keyword_t* ICACHE_FLASH_ATTR get_config_keyword( int index );
static int ICACHE_FLASH_ATTR get_config( int id, char **str, int *value );
static int ICACHE_FLASH_ATTR compare_config( int id, char *str, int value );
static int ICACHE_FLASH_ATTR update_config( int id, char *str, int value );
static int ICACHE_FLASH_ATTR apply_config( int id, char *str, int value );

static int ICACHE_FLASH_ATTR send_json_reponse( HttpdConnData *connData, Config_Keyword_t *keyword );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR get_num_keywords( void )
{
   // ESP_LOGD( TAG, "get_num_keywords" );

   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();
   int num_keywords = 0;

   // ESP_LOGD( TAG, "have %d configurations", num_cfgs );

   for( int i = 0; i <  num_cfgs; i++ )
   {
      num_keywords += cfg_list[i]->num_keywords;
   }

   // ESP_LOGD( TAG, "num keywords: %d", num_keywords );
   return num_keywords;
}

static const Config_Keyword_t* ICACHE_FLASH_ATTR get_config_keyword( int index )
{
   // ESP_LOGD( TAG, "get_config_keyword %d", index );

   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();
   int num_keywords = 0;
   int offset = 0;

   for( int i = 0; i <  num_cfgs; i++ )
   {
      num_keywords += cfg_list[i]->num_keywords;
      if( index < num_keywords )
      {
         const Config_Keyword_t *keyword = &cfg_list[ i ]->config_keywords[ index - offset ];
         // ESP_LOGD( TAG, "Found %d.%d.%d: %s", i, index, index - offset, S( keyword->token ) );
         return keyword;
      }
      offset = num_keywords;
   }

   return NULL;
}

static int ICACHE_FLASH_ATTR get_config( int id, char **str, int *value )
{
   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();

   for( int i = 0; i <  num_cfgs; i++ )
   {
      int ( *get_config )( int id, char **str, int *value ) = cfg_list[ i ]->get_config;
      if( get_config != NULL )
         if( get_config( id, str, value ) == true )
            return true;
   }

   return false;
}

static int ICACHE_FLASH_ATTR compare_config( int id, char *str, int value )
{
   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();

   for( int i = 0; i <  num_cfgs; i++ )
   {
      int ( *compare_config )( int id, char *str, int value ) = cfg_list[ i ]->compare_config;
      if( compare_config != NULL )
      {
         int rc = compare_config( id, str, value );
         if( rc != -1 )
            return rc;
      }
   }

   return -1;
}

static int ICACHE_FLASH_ATTR update_config( int id, char *str, int value )
{
   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();

   for( int i = 0; i <  num_cfgs; i++ )
   {
      int ( *update_config )( int id, char *str, int value ) = cfg_list[ i ]->update_config;
      if( update_config != NULL )
      {
         int rc = update_config( id, str, value );
         if( rc != -1 )
            return rc;
      }
   }

   return -1;
}

static int ICACHE_FLASH_ATTR apply_config( int id, char *str, int value )
{
   Configuration_List_t *cfg_list = get_configuration_list();
   int num_cfgs = get_num_configurations();

   for( int i = 0; i <  num_cfgs; i++ )
   {
      int ( *apply_config )( int id, char *str, int value ) = cfg_list[ i ]->apply_config;
      if( apply_config != NULL )
      {
         int rc = apply_config( id, str, value );
         if( rc != -1 )
            return rc;
      }
   }

   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR send_json_reponse( HttpdConnData *connData, Config_Keyword_t *keyword )
{
   char buf[128];
   int len = 0;
   char *str = NULL;
   int value = 0;

   int id = keyword->id;
   int type = keyword->type;

   if( get_config( id, &str, &value ) )
   {
      ESP_LOGD( TAG, "Config: 0x%02x: '%s', %d", id, S( str ), value );

      if( keyword->type == Text )
      {
         len = sprintf( buf, "{ \"param\" : \"%s\", \"value\" : \"%s\" }", keyword->token, str );
      }
      else if( keyword->type == NumArray )
      {
         // implementation here not required
      }
      else if( keyword->type == Flag )
      {
         len = sprintf( buf, "{ \"param\" : \"%s\", \"value\" : \"%d\" }", keyword->token, value == 0 ? 0 : 1 );
      }
      else if( keyword->type == Ip_Addr )
      {
#ifdef ESP_PLATFORM
         len = sprintf( buf, "{ \"param\" : \"%s\", \"value\" : \""IPSTR"\" }", keyword->token, IP2STR( ( ip4_addr_t* )&value ) );
#else
         len = sprintf( buf, "{ \"param\" : \"%s\", \"value\" : \""IPSTR"\" }", keyword->token, IP2STR( &value ) );
#endif
      }
      else
      {
         len = sprintf( buf, "{ \"param\" : \"%s\", \"value\" : \"%d\" }", keyword->token, value );
      }
   }
   else
   {
       len = sprintf( buf, "{ \"param\" : \"unknown\", \"value\" : \"undef\" }" );
   }

   // Generate response in JSON format
   // ESP_LOGD( TAG, "Json repsonse: %s", buf );

   httpdStartResponse( connData, 200 );
   httpdHeader( connData, "Content-Type", "text/json" );
   httpdEndHeaders( connData );
   httpdSend( connData, buf, len );

   return len;
}

// --------------------------------------------------------------------------
// get the configuration from the web page
// --------------------------------------------------------------------------

// search for all keywords the corresponding id
// with this id get the configuration value, check it against the token
// if not equal update the configuration and store the new value in
// the user configuration section of the flash

CgiStatus ICACHE_FLASH_ATTR cgiConfig( HttpdConnData *connData )
{
   char buf[128];

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

#if 0 // use post or get method
   // ESP_LOGD( TAG, "%s", S( connData->post.buf ) );
#else
   if( connData->requestType != HTTPD_METHOD_GET )
   {
      // Sorry, we only accept GET requests.
      httpdStartResponse( connData, 406 ); // http error code 'unacceptable'
      httpdEndHeaders( connData );
      return HTTPD_CGI_DONE;
   }
#endif

   int i;
   for( i = 0; i < get_num_keywords(); i++ )
   {
      Config_Keyword_t keyword;
      memcpy ( &keyword, get_config_keyword( i ), sizeof( Config_Keyword_t ) );

#if 0  // use post or get method
      int len = httpdFindArg( connData->post.buf, keyword.token, ( char * )buf, sizeof( buf ) );
#else
      int len = httpdFindArg( connData->getArgs,  keyword.token, ( char * )buf, sizeof( buf ) );
#endif
      if( len > 0 )
      {
         int id = keyword.id;
         int type = keyword.type;

         // ESP_LOGD( TAG, "cgiConfig id 0x%02x len %d \"%s\"", id, len, S( buf ) );

         // remove leading blanks from str, there are no trailing blanks, so at least one character
         char *buf_p = buf;
         while( *buf_p == ' ' || *buf_p == '\t' )
         {
            buf_p++;
            len--;
         }
         // move to begin of buf
         if( buf != buf_p )
         {
            strcpy( buf, buf_p );
            ESP_LOGD( TAG, "remove blanks '%s'", S( buf ) );
         }

         // buf holds the new value for configlist[ id ]
         // check if the value has changed
         // if so, save the new value to the flash and update the config_list

         if( type == Text )
         {
            // compare with string stored in flash
            if( 0 == compare_config( id, buf, 0 ) )
            {
               // ESP_LOGD( TAG, "update config %d, %s", id, S( buf ) );
               if( update_config( id, buf, 0 ) == 1 )
                  config_save_str( id, buf, 0, Text );

               // apply the changes to the device, module, ...
               apply_config( id, buf, 0 );
            }
         }
         else if( type == NumArray )
         {
            // determine the number of integers in buf
            int cnt = str2int_array( buf, NULL, 0 );
            // ESP_LOGD( TAG, "NumArray has %d elements", cnt );
            int num_array[ NUMARRAY_SIZE ] __attribute__( ( aligned( 4 ) ) );
            if( cnt > NUMARRAY_SIZE )
               cnt = NUMARRAY_SIZE;

            str2int_array( buf, num_array, cnt );

            for( int i = cnt; i < NUMARRAY_SIZE; i++ )
               num_array [ i ] = 0;

            if( 0 == compare_config( id, ( char* )num_array, 0 ) )
            {
               if( update_config( id, ( char* )num_array, 0 ) == 1 )
                  config_save_str( id, ( char* )num_array, cnt * sizeof( int ), NumArray );

               // apply the changes to the device, module, ...
               apply_config( id, ( char* )num_array, 0 );
            }
         }
         else
         {
            // compare with value stored in config_list
            // first convert value string in buf to an integer

            int val = 0;
            if( type == Number )
               val = str2int( buf );
            else if( type == Flag )
                val = strcmp( "0", buf ) == 0 ? 0 : 1;
            else if( type == Ip_Addr )
               str2ip( buf, &val );

            if( 0 == compare_config( id, NULL, val ) )
            {
               // ESP_LOGD( TAG, "update config %d, %d", id, val );
               if( update_config( id, NULL, val ) == 1 )
                  config_save_int( id, val, type );

               // apply the changes to the device, module, ...
               apply_config( id, NULL, val );
            }
         }

         send_json_reponse( connData, &keyword );
      }
   }

   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

CgiStatus ICACHE_FLASH_ATTR tplConfig( HttpdConnData *connData, char *token, void **arg )
{
   char buf[64];
   int bufsize = sizeof( buf );
   int buflen = 0;
   int i;

   if( token == NULL )
      return HTTPD_CGI_DONE;

   // ESP_LOGD( TAG, "tplConfig %s", S( token ) );

   for( i = 0; i < get_num_keywords(); i++ )
   {
      Config_Keyword_t keyword;
      memcpy ( &keyword, get_config_keyword( i ), sizeof( Config_Keyword_t ) );

      if( strcmp( token, keyword.token ) == 0 )
      {
         char *str;
         int value;

         if( get_config( keyword.id, &str, &value ) )
         {
            if( keyword.type == Text )
            {
               buflen = sprintf( buf, "%s", str );
               // ESP_LOGD( TAG, "tplConfig id 0x%02x len %d \"%s\"", keyword.id, buflen, S( buf ) );
            }
            else if( keyword.type == NumArray )
            {
               // !!! todo ( AWe ): implement function to get NumArray
            }
            else if( keyword.type == Flag )
            {
               buflen = sprintf( buf, "%d", value == 0 ? 0 : 1 );
               // ESP_LOGD( TAG, "Flag %d", value );
            }
            else if( keyword.type == Ip_Addr )
            {
#ifdef ESP_PLATFORM
               // ESP_LOGD( TAG, "Ip_Addr "IPSTR, IP2STR( ( ip4_addr_t* )&value ) );
               buflen = sprintf( buf, IPSTR, IP2STR( ( ip4_addr_t* )&value ) );
#else
               // ESP_LOGD( TAG, "Ip_Addr "IPSTR, IP2STR( &value ) );
               buflen = sprintf( buf, IPSTR, IP2STR( &value ) );
#endif
            }
            else
            {
               buflen = sprintf( buf, "%d", value );
            }
         }
         break;
      }
   }

   if( buflen == 0 )
   {
      strcpy( buf, "Unknown [" );
      strcat( buf, token );
      strcat( buf, "]" );
      buflen = strlen( buf );
   }

   // ESP_LOGD( TAG, "Send %s", S( buf ) );
   httpdSend( connData, buf, buflen );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
