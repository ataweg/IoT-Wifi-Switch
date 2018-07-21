// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          mqtt_config.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-09  AWe   replace httpd_printf() with ESP_LOG*()
//    2017-11-27  AWe   derived ftom cgiRelayStatus.c
//    2017-09-18  AWe   replace streq() with strcmp()
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#ifndef ESP_PLATFORM
   #define _PRINT_CHATTY
   #define V_HEAP_INFO
#else
   #define HEAP_INFO( x )
#endif

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "mqtt_config";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>

#ifdef ESP_PLATFORM
   #include "tcpip_adapter.h"       // IP2STR, IPSTR
#else
   #include <osapi.h>
   #include "user_interface.h"
#endif

#include "configs.h"         // enums, config_save_str()
#include "mqtt_settings.h"
#include "mqtt_config.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct rst_info *rst_info_t;
extern rst_info_t sys_rst_info;

MQTT_client_cfg_t MQTT_Client_Cfg;
MQTT_wifi_cfg_t MQTT_Wifi_Cfg[ num_devices ];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// from user_mqtt.c
//
// char *mqtt_client_name = "/switch/";
// char *mqtt_wifi_names[] =
// {
// // same order as enum mqtt_devices
// // device name   | io type | value type |
//    "relay",     // output     1 bit      gpio_out
//    "infoled",   // output     1 bit      gpio_out
//    "sysled",    // output     1 bit      gpio_out
//
//    "button",    // input      1 bit      gpio_in
//    "pwrsense",  // input      1 bit      gpio_in
//    "adc",       // analog    10 bit      adc
//    "time"       // time       string     time
// }


// template parameter & GET/POST parameter

const Config_Keyword_t ICACHE_RODATA_ATTR MqttConfig_Keywords[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { Grp_Relay      + Topic_Sub, Text   }, "cRelay"       },
   { { Grp_Relay      + Topic_Pub, Text   }, "sRelay"       },

   { { Grp_InfoLed    + Topic_Sub, Text   }, "cInfoLed"     },
   { { Grp_InfoLed    + Topic_Pub, Text   }, "sInfoLed"     },

   { { Grp_SysLed     + Topic_Sub, Text   }, "cSysLed"      },
   { { Grp_SysLed     + Topic_Pub, Text   }, "sSysLed"      },

   { { Grp_Button     + Topic_Sub, Text   }, "cButton"      },
   { { Grp_Button     + Topic_Pub, Text   }, "sButton"      },

   { { Grp_PowerSense + Topic_Sub, Text   }, "cPowerSense"  },
   { { Grp_PowerSense + Topic_Pub, Text   }, "sPowerSense"  },

   { { Grp_Time       + Topic_Sub, Text   }, "cTime"        },
   { { Grp_Time       + Topic_Pub, Text   }, "sTime"        },

   { { Grp_Adc        + Topic_Sub, Text   }, "cAdc"         },    // not uesed yet
   { { Grp_Adc        + Topic_Pub, Text   }, "sAdc"         },    // not uesed yet

   { { Mqtt_Name,                  Text   }, "mqtt_client"  },    // Mqtt_Name   is not the save as mqtt client id
   { { Mqtt_Server,                Text   }, "mqtt_server"  },    // Mqtt_Server
   { { Mqtt_Port,                  Number }, "mqtt_port"    },    // Mqtt_Port
   { { Mqtt_Username,              Text   }, "mqtt_user"    },    // Mqtt_Username
   { { Mqtt_Password,              Text   }, "mqtt_password"   }, // Mqtt_Password
   { { Mqtt_Client_Id,             Text   }, "mqtt_client_id"  },
   { { Mqtt_Enable_SSL,            Flag   }, "mqtt_enable_ssl" },
   { { Mqtt_Self_Signed,           Flag   }, "mqtt_self_signed"},
   { { Mqtt_Keep_Alive,            Number }, "mqtt_keep_alive" },
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR mqtt_get_config( int id, char **str, int *value );
static int ICACHE_FLASH_ATTR mqtt_compare_config( int id, char *str, int value );
static int ICACHE_FLASH_ATTR mqtt_update_config( int id, char *str, int value );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR mqtt_get_config( int id, char **str, int *value )
{
   // ESP_LOGD( TAG, "mqtt_get_config 0x%02x", id );

   int grp = id & 0xF0;

   if( grp >= Grp_First && grp <= Grp_Last )
   {
      int dev = ( grp >> 4 ) - 1;
      switch( id & 0x0F )
      {
         case Dev_Name:        *str   = MQTT_Wifi_Cfg[ dev ].Name;           return true;
         case Topic_Sub:       *str   = MQTT_Wifi_Cfg[ dev ].Topic_sub;      return true;
         case Topic_Pub:       *str   = MQTT_Wifi_Cfg[ dev ].Topic_pub;      return true;
         case Enable_Publish:  *value = MQTT_Wifi_Cfg[ dev ].Enable_publish; return true;
         case Retained:        *value = MQTT_Wifi_Cfg[ dev ].Retained;       return true;
         case QoS:             *value = MQTT_Wifi_Cfg[ dev ].QoS;            return true;
      }
   }
   else if( id >= Mqtt_First && id <= Mqtt_Last )
   {
      switch( id )
      {
         // these values are also stored in the flash
         case Mqtt_Name:         *str   = MQTT_Client_Cfg.Name;        return true;
         case Mqtt_Server:       *str   = MQTT_Client_Cfg.Server;      return true;
         case Mqtt_Username:     *str   = MQTT_Client_Cfg.Username;    return true;
         case Mqtt_Password:     *str   = MQTT_Client_Cfg.Password;    return true;
         case Mqtt_Client_Id:    *str   = MQTT_Client_Cfg.Client_ID;   return true;
         case Mqtt_Port:         *value = MQTT_Client_Cfg.Port;        return true;
         case Mqtt_Enable_SSL:   *value = MQTT_Client_Cfg.Enable_SSL;  return true;
         case Mqtt_Self_Signed:  *value = MQTT_Client_Cfg.Self_Signed; return true;
         case Mqtt_Keep_Alive:   *value = MQTT_Client_Cfg.Keep_Alive;  return true;
      }
   }

   // id not handled here, nothing to compare
   return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// return
//    1       value doesn't change configuration
//    0       value  changed configuration
//   -1       id not handled here, nothing to compare

int ICACHE_FLASH_ATTR mqtt_compare_config( int id, char *str, int value )
{
   ESP_LOGD( TAG, "mqtt_compare_config 0x%02x %s %d", id, str, value );

   int grp = id & 0xF0;

   if( grp >= Grp_First && grp <= Grp_Last )
   {
      int dev = ( grp >> 4 ) - 1;
      switch( id & 0x0F )
      {
         case Dev_Name:        return strncmp( MQTT_Wifi_Cfg[ dev ].Name,      str, sizeof( MQTT_Wifi_Cfg[ dev ].Name      ) ) == 0 ? 1 : 0;
         case Topic_Sub:       return strncmp( MQTT_Wifi_Cfg[ dev ].Topic_sub, str, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_sub ) ) == 0 ? 1 : 0;
         case Topic_Pub:       return strncmp( MQTT_Wifi_Cfg[ dev ].Topic_pub, str, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_pub ) ) == 0 ? 1 : 0;
         case Enable_Publish:  return MQTT_Wifi_Cfg[ dev ].Enable_publish == value ? 1 : 0;
         case Retained:        return MQTT_Wifi_Cfg[ dev ].Retained       == value ? 1 : 0;
         case QoS:             return MQTT_Wifi_Cfg[ dev ].QoS            == value ? 1 : 0;
      }
   }
   else if( id >= Mqtt_First && id <= Mqtt_Last )
   {
      switch( id )
      {
         // these values are also stored in the flash
         case Mqtt_Name:        return strncmp( MQTT_Client_Cfg.Name,      str, sizeof( MQTT_Client_Cfg.Name      ) ) == 0 ? 1 : 0;
         case Mqtt_Server:      return strncmp( MQTT_Client_Cfg.Server,    str, sizeof( MQTT_Client_Cfg.Server    ) ) == 0 ? 1 : 0;
         case Mqtt_Username:    return strncmp( MQTT_Client_Cfg.Username,  str, sizeof( MQTT_Client_Cfg.Username  ) ) == 0 ? 1 : 0;
         case Mqtt_Password:    return strncmp( MQTT_Client_Cfg.Password,  str, sizeof( MQTT_Client_Cfg.Password  ) ) == 0 ? 1 : 0;
         case Mqtt_Client_Id:   return strncmp( MQTT_Client_Cfg.Client_ID, str, sizeof( MQTT_Client_Cfg.Client_ID ) ) == 0 ? 1 : 0;
         case Mqtt_Port:        return MQTT_Client_Cfg.Port        == value ? 1 : 0;
         case Mqtt_Enable_SSL:  return MQTT_Client_Cfg.Enable_SSL  == value ? 1 : 0;
         case Mqtt_Self_Signed: return MQTT_Client_Cfg.Self_Signed == value ? 1 : 0;
         case Mqtt_Keep_Alive:  return MQTT_Client_Cfg.Keep_Alive  == value ? 1 : 0;
      }
   }

   // id not handled here, nothing to compare
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR mqtt_update_config( int id, char *str, int value )
{
   ESP_LOGD( TAG, "mqtt_update_config 0x%02x %s %d", id, S( str ), value );

   int grp = id & 0xF0;

   if( grp >= Grp_First && grp <= Grp_Last )
   {
      int dev = ( grp >> 4 ) - 1;
      switch( id & 0x0F )
      {
         case Dev_Name:        strncpy( MQTT_Wifi_Cfg[ dev ].Name,      str, sizeof( MQTT_Wifi_Cfg[ dev ].Name      ) ); break;
         case Topic_Sub:       strncpy( MQTT_Wifi_Cfg[ dev ].Topic_sub, str, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_sub ) ); break;
         case Topic_Pub:       strncpy( MQTT_Wifi_Cfg[ dev ].Topic_pub, str, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_pub ) ); break;
         case Enable_Publish:  MQTT_Wifi_Cfg[ dev ].Enable_publish = value; break;
         case Retained:        MQTT_Wifi_Cfg[ dev ].Retained       = value; break;
         case QoS:             MQTT_Wifi_Cfg[ dev ].QoS            = value; break;
         default:
            return 0;
      }
      return 1;
   }
   else if( id >= Mqtt_First && id <= Mqtt_Last )
   {
      switch( id )
      {
         // changing these values requires a restart of the system
         // or at least of the  mqtt service,. This means we have to
         // save the new values in the flash before do a restart.
         // Storing them in the memory makes no sense.
         case Mqtt_Name:        strncpy( MQTT_Client_Cfg.Name,      str, sizeof( MQTT_Client_Cfg.Name      ) ); break;
         case Mqtt_Server:      strncpy( MQTT_Client_Cfg.Server,    str, sizeof( MQTT_Client_Cfg.Server    ) ); break;
         case Mqtt_Username:    strncpy( MQTT_Client_Cfg.Username,  str, sizeof( MQTT_Client_Cfg.Username  ) ); break;
         case Mqtt_Password:    strncpy( MQTT_Client_Cfg.Password,  str, sizeof( MQTT_Client_Cfg.Password  ) ); break;
         case Mqtt_Client_Id:   strncpy( MQTT_Client_Cfg.Client_ID, str, sizeof( MQTT_Client_Cfg.Client_ID ) ); break;
         case Mqtt_Port:        MQTT_Client_Cfg.Port        = value; break;
         case Mqtt_Enable_SSL:  MQTT_Client_Cfg.Enable_SSL  = value; break;
         case Mqtt_Self_Signed: MQTT_Client_Cfg.Self_Signed = value; break;
         case Mqtt_Keep_Alive:  MQTT_Client_Cfg.Keep_Alive  = value; break;
         default:
            return 0;
      }
      return 1;
   }

   // id not handled here, nothing to update
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR getMqttConfig( void )
{
   // ESP_LOGD( TAG, "user_mqtt_configure" );

   mqtt_devices_t dev;

   for( dev = dev_Relay; dev < num_devices ; dev++ )
   {
      int dev_id = ( ( dev + 1 ) << 4 );
      // ESP_LOGD( TAG, "user_mqtt_configure %d 0x%02x", dev, dev_id  );

      MQTT_Wifi_Cfg[ dev ].Enable_publish = config_get_int( dev_id + Enable_Publish );
      MQTT_Wifi_Cfg[ dev ].Retained       = config_get_int( dev_id + Retained );
      MQTT_Wifi_Cfg[ dev ].QoS            = config_get_int( dev_id + QoS );

      config_get( dev_id + Dev_Name,  MQTT_Wifi_Cfg[ dev ].Name,      sizeof( MQTT_Wifi_Cfg[ dev ].Name ) );
      config_get( dev_id + Topic_Sub, MQTT_Wifi_Cfg[ dev ].Topic_sub, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_sub ) );
      config_get( dev_id + Topic_Pub, MQTT_Wifi_Cfg[ dev ].Topic_pub, sizeof( MQTT_Wifi_Cfg[ dev ].Topic_pub ) );

      ESP_LOGV( TAG, "Configure MQTT: 0x%02x, dev: %s", dev, MQTT_Wifi_Cfg[ dev ].Name );
      ESP_LOGV( TAG, "                      sub: %s",        MQTT_Wifi_Cfg[ dev ].Topic_sub );
      ESP_LOGV( TAG, "                      pub: %s",        MQTT_Wifi_Cfg[ dev ].Topic_pub );
      ESP_LOGV( TAG, "                      cfg: %d %d %d",  MQTT_Wifi_Cfg[ dev ].Enable_publish, MQTT_Wifi_Cfg[ dev ].Retained, MQTT_Wifi_Cfg[ dev ].QoS );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR getMqttClientConfig( void )
{
   MQTT_Client_Cfg.Port        = config_get_int( Mqtt_Port );
   MQTT_Client_Cfg.Enable_SSL  = config_get_int( Mqtt_Enable_SSL );
   MQTT_Client_Cfg.Self_Signed = config_get_int( Mqtt_Self_Signed );
   MQTT_Client_Cfg.Keep_Alive  = config_get_int( Mqtt_Keep_Alive );

   config_get( Mqtt_Name,     MQTT_Client_Cfg.Name,      sizeof( MQTT_Client_Cfg.Name      ) );
   config_get( Mqtt_Server,   MQTT_Client_Cfg.Server,    sizeof( MQTT_Client_Cfg.Server    ) );
   config_get( Mqtt_Username, MQTT_Client_Cfg.Username,  sizeof( MQTT_Client_Cfg.Username  ) );
   config_get( Mqtt_Password, MQTT_Client_Cfg.Password,  sizeof( MQTT_Client_Cfg.Password  ) );
   config_get( Mqtt_Client_Id,MQTT_Client_Cfg.Client_ID, sizeof( MQTT_Client_Cfg.Client_ID ) );

   ESP_LOGV( TAG, "Configure MQTT_Client Name       : %s", MQTT_Client_Cfg.Name      );
   ESP_LOGV( TAG, "Configure MQTT_Client Server     : %s", MQTT_Client_Cfg.Server    );
   ESP_LOGV( TAG, "Configure MQTT_Client Username   : %s", MQTT_Client_Cfg.Username  );
   ESP_LOGV( TAG, "Configure MQTT_Client Password   : %s", MQTT_Client_Cfg.Password  );
   ESP_LOGV( TAG, "Configure MQTT_Client Client_ID  : %s", MQTT_Client_Cfg.Client_ID );

   ESP_LOGV( TAG, "Configure MQTT_Client Port       : %d", MQTT_Client_Cfg.Port        );
   ESP_LOGV( TAG, "Configure MQTT_Client Enable_SSL : %d", MQTT_Client_Cfg.Enable_SSL  );
   ESP_LOGV( TAG, "Configure MQTT_Client Self_Signed: %d", MQTT_Client_Cfg.Self_Signed );
   ESP_LOGV( TAG, "Configure MQTT_Client Keep_Alive : %d", MQTT_Client_Cfg.Keep_Alive  );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static Configuration_Item_t cfgItem;

Configuration_Item_t* ICACHE_FLASH_ATTR init_mqtt_config( void )
{
   cfgItem.config_keywords = MqttConfig_Keywords;
   cfgItem.num_keywords    = sizeof( MqttConfig_Keywords ) / sizeof( Config_Keyword_t );
   cfgItem.defaults        = mqtt_settings;
   cfgItem.get_config      = mqtt_get_config;
   cfgItem.compare_config  = mqtt_compare_config;
   cfgItem.update_config   = mqtt_update_config;
   cfgItem.apply_config    = NULL;

   return &cfgItem;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
