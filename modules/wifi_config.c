// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          wifi_config.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-04-20  AWe   takeover from WebServer project and adept it
//    2018-01-19  AWe   update to chmorgan/libesphttpd
//                         https://github.com/chmorgan/libesphttpd/commits/cmo_minify
//                         Latest commit d15cc2e  from 5. Januar 2018
//                         here: replace ( connData->conn == NULL ) with ( connData->isConnectionClosed )
//    2017-09-18  AWe   replace streq() with strcmp()
//    2017-09-13  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "wifi_config";
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
#include "wifi_settings.h"
#include "wifi_config.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// template parameter & GET/POST parameter

const Config_Keyword_t ICACHE_RODATA_ATTR WifiConfig_Keywords[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { Wifi_Name,          Text,     0, 0 }, "wifi_client"        },  //
   { { Wifi_Server,        Text,     0, 0 }, "wifi_server"        },  //
   { { Wifi_Username,      Text,     0, 0 }, "wifi_user"          },  //
   { { Wifi_Password,      Text,     0, 0 }, "wifi_password"      },  //
   { { Wifi_Port,          Number,   0, 0 }, "wifi_port"          },  //

   { { Ap_Ssid,            Text,     0, 0 }, "ap_ssid"            },  // 0xA1
   { { Ap_Password,        Text,     0, 0 }, "ap_password"        },  // 0xA2
   { { Ap_Authmode,        Number,   0, 0 }, "ap_authmode"        },  // 0xA3
   { { Ap_Channel,         Number,   0, 0 }, "ap_channel"         },  // 0xA4
   { { Ap_SsidHidden,      Flag,     0, 0 }, "ap_ssid_hidden"     },  // 0xA5
   { { Ap_MaxConnection,   Number,   0, 0 }, "ap_max_connection"  },  // 0xA6
   { { Ap_Bandwidth,       Number,   0, 0 }, "ap_bandwidth"       },  // 0xA7
   { { Ap_BeaconInterval,  Number,   0, 0 }, "ap_beacon_interval" },  // 0xA8
   { { Ap_Address,         Ip_Addr,  0, 0 }, "ap_address"         },  // 0xA9
   { { Ap_Gateway,         Ip_Addr,  0, 0 }, "ap_gateway"         },  // 0xAA
   { { Ap_Netmask,         Ip_Addr,  0, 0 }, "ap_netmask"         },  // 0xAB

   { { Sta_Only,           Flag,     0, 0 }, "sta_only"           },  // 0xB1
   { { Sta_PowerSave,      Flag,     0, 0 }, "sta_power_save"     },  // 0xB2
   { { Sta_StaticIp,       Flag,     0, 0 }, "sta_static_ip"      },  // 0xB3
   { { Sta_Address,        Ip_Addr,  0, 0 }, "sta_ip_addr"        },  // 0xB4
   { { Sta_Gateway,        Ip_Addr,  0, 0 }, "sta_gw_addr"        },  // 0xB5
   { { Sta_Netmask,        Ip_Addr,  0, 0 }, "sta_netmask"        },  // 0xB6
   { { Sta_Client_Name,    Text,     0, 0 }, "sta_client_name"    },  // 0xB7

   { { Dhcp_Enable,        Flag,     0, 0 }, "dhcp_enable"        },  // 0xC1
   { { Dhcp_Server,        Ip_Addr,  0, 0 }, "dhcp_server"        },  // 0xC2
   { { Dhcp_Address,       Ip_Addr,  0, 0 }, "dhcp_address"       },  // 0xC3
   { { Dhcp_Gateway,       Ip_Addr,  0, 0 }, "dhcp_gateway"       },  // 0xC4
   { { Dhcp_Netmask,       Ip_Addr,  0, 0 }, "dhcp_netmask"       },  // 0xC5
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

wifi_client_cfg_t Wifi_Client_Cfg;
wifi_wlan_cfg_t   Wifi_Wlan_Cfg;
wifi_ap_cfg_t     Wifi_Ap_Cfg;
wifi_sta_cfg_t    Wifi_Sta_Cfg;
wifi_dhcp_cfg_t   Wifi_Dhcp_Cfg;

static int ICACHE_FLASH_ATTR wifi_get_config( int id, char **str, int *value );
static int ICACHE_FLASH_ATTR wifi_compare_config( int id, char *str, int value );
static int ICACHE_FLASH_ATTR wifi_update_config( int id, char *str, int value );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR wifi_get_config( int id, char **str, int *value )
{
//   ESP_LOGD( TAG, "wifi_get_config 0x%02x", id );

   if( id >= Wifi_First && id <= Wifi_Last )
   {
      switch( id )
      {
         // these values are also stored in the flash
         case Wifi_Name:       *str   = Wifi_Client_Cfg.Name;     return true;
         case Wifi_Server:     *str   = Wifi_Client_Cfg.Server;   return true;
         case Wifi_Username:   *str   = Wifi_Client_Cfg.Username; return true;
         case Wifi_Password:   *str   = Wifi_Client_Cfg.Password; return true;
         case Wifi_Port:       *value = Wifi_Client_Cfg.Port;     return true;
      }
   }
   else if( id >= Wlan_First && id <= Wlan_Last )
   {
      // process a list
   }
   else if( id >= Ap_First && id <= Ap_Last )
   {
      switch( id )
      {
         case Ap_Ssid:           *str   = Wifi_Ap_Cfg.Ssid;           return true;
         case Ap_Password:       *str   = Wifi_Ap_Cfg.Password;       return true;
         case Ap_Authmode:       *value = Wifi_Ap_Cfg.Authmode;       return true;
         case Ap_Channel:        *value = Wifi_Ap_Cfg.Channel;        return true;
         case Ap_SsidHidden:     *value = Wifi_Ap_Cfg.SsidHidden;     return true;
         case Ap_MaxConnection:  *value = Wifi_Ap_Cfg.MaxConnection;  return true;
         case Ap_Bandwidth:      *value = Wifi_Ap_Cfg.Bandwidth;      return true;
         case Ap_BeaconInterval: *value = Wifi_Ap_Cfg.BeaconInterval; return true;
         case Ap_Address:        *value = Wifi_Ap_Cfg.Address;        return true;
         case Ap_Gateway:        *value = Wifi_Ap_Cfg.Gateway;        return true;
         case Ap_Netmask:        *value = Wifi_Ap_Cfg.Netmask;        return true;
      }
   }
   else if( id >= Sta_First && id <= Sta_Last )
   {
      switch( id )
      {
         case Sta_Only:          *value = Wifi_Sta_Cfg.Only;         return true;
         case Sta_PowerSave:     *value = Wifi_Sta_Cfg.PowerSave;    return true;
         case Sta_StaticIp:      *value = Wifi_Sta_Cfg.StaticIp;     return true;
         case Sta_Address:       *value = Wifi_Sta_Cfg.Address;      return true;
         case Sta_Gateway:       *value = Wifi_Sta_Cfg.Gateway;      return true;
         case Sta_Netmask:       *value = Wifi_Sta_Cfg.Netmask;      return true;
         case Sta_Client_Name:   *str   = Wifi_Sta_Cfg.Client_Name;  return true;
      }
   }
   else if( id >= Dhcp_First && id <= Dhcp_Last )
   {
      switch( id )
      {
         case Dhcp_Enable:       *value = Wifi_Dhcp_Cfg.Enable;  return true;
         case Dhcp_Server:       *value = Wifi_Dhcp_Cfg.Server;  return true;
         case Dhcp_Address:      *value = Wifi_Dhcp_Cfg.Address; return true;
         case Dhcp_Gateway:      *value = Wifi_Dhcp_Cfg.Gateway; return true;
         case Dhcp_Netmask:      *value = Wifi_Dhcp_Cfg.Netmask; return true;
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

static int ICACHE_FLASH_ATTR wifi_compare_config( int id, char *str, int value )
{
   ESP_LOGD( TAG, "wifi_compare_config 0x%02x %s %d", id, S( str ), value );

   if( id >= Wifi_First && id <= Wifi_Last )
   {
      switch( id )
      {
         // these values are also stored in the flash
         case Wifi_Name:     return strncmp( Wifi_Client_Cfg.Name,     str, sizeof( Wifi_Client_Cfg.Name     ) ) == 0 ? 1 : 0;
         case Wifi_Server:   return strncmp( Wifi_Client_Cfg.Server,   str, sizeof( Wifi_Client_Cfg.Server   ) ) == 0 ? 1 : 0;
         case Wifi_Username: return strncmp( Wifi_Client_Cfg.Username, str, sizeof( Wifi_Client_Cfg.Username ) ) == 0 ? 1 : 0;
         case Wifi_Password: return strncmp( Wifi_Client_Cfg.Password, str, sizeof( Wifi_Client_Cfg.Password ) ) == 0 ? 1 : 0;
         case Wifi_Port:     return Wifi_Client_Cfg.Port  == value ? 1 : 0;
      }
   }
   else if( id >= Wlan_First && id <= Wlan_Last )
   {
      // process a list
   }
   else if( id >= Ap_First && id <= Ap_Last )
   {
      switch( id )
      {
         case Ap_Ssid:           return strncmp( Wifi_Ap_Cfg.Ssid,     str, sizeof( Wifi_Ap_Cfg.Ssid   ) ) == 0 ? 1 : 0;
         case Ap_Password:       return strncmp( Wifi_Ap_Cfg.Password, str, sizeof( Wifi_Ap_Cfg.Password ) ) == 0 ? 1 : 0;
         case Ap_Authmode:       return Wifi_Ap_Cfg.Authmode       == value ? 1 : 0;
         case Ap_Channel:        return Wifi_Ap_Cfg.Channel        == value ? 1 : 0;
         case Ap_SsidHidden:     return Wifi_Ap_Cfg.SsidHidden     == value ? 1 : 0;
         case Ap_MaxConnection:  return Wifi_Ap_Cfg.MaxConnection  == value ? 1 : 0;
         case Ap_Bandwidth:      return Wifi_Ap_Cfg.Bandwidth      == value ? 1 : 0;
         case Ap_BeaconInterval: return Wifi_Ap_Cfg.BeaconInterval == value ? 1 : 0;
         case Ap_Address:        return Wifi_Ap_Cfg.Address        == value ? 1 : 0;
         case Ap_Gateway:        return Wifi_Ap_Cfg.Gateway        == value ? 1 : 0;
         case Ap_Netmask:        return Wifi_Ap_Cfg.Netmask        == value ? 1 : 0;
      }
   }
   else if( id >= Sta_First && id <= Sta_Last )
   {
      switch( id )
      {
         case Sta_Only:        return Wifi_Sta_Cfg.Only      == value ? 1 : 0;
         case Sta_PowerSave:   return Wifi_Sta_Cfg.PowerSave == value ? 1 : 0;
         case Sta_StaticIp:    return Wifi_Sta_Cfg.StaticIp  == value ? 1 : 0;
         case Sta_Address:     return Wifi_Sta_Cfg.Address   == value ? 1 : 0;
         case Sta_Gateway:     return Wifi_Sta_Cfg.Gateway   == value ? 1 : 0;
         case Sta_Netmask:     return Wifi_Sta_Cfg.Netmask   == value ? 1 : 0;
         case Sta_Client_Name: return strncmp( Wifi_Sta_Cfg.Client_Name, str, sizeof( Wifi_Sta_Cfg.Client_Name ) ) == 0 ? 1 : 0;
      }
   }
   else if( id >= Dhcp_First && id <= Dhcp_Last )
   {
      switch( id )
      {
         case Dhcp_Enable:    return Wifi_Dhcp_Cfg.Enable  == value ? 1 : 0;
         case Dhcp_Server:    return Wifi_Dhcp_Cfg.Server  == value ? 1 : 0;
         case Dhcp_Address:   return Wifi_Dhcp_Cfg.Address == value ? 1 : 0;
         case Dhcp_Gateway:   return Wifi_Dhcp_Cfg.Gateway == value ? 1 : 0;
         case Dhcp_Netmask:   return Wifi_Dhcp_Cfg.Netmask == value ? 1 : 0;
      }
   }

   // id not handled here, nothing to compare
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR wifi_update_config( int id, char *str, int value )
{
   ESP_LOGD( TAG, "wifi_update_config 0x%02x %s %d", id, S( str ), value );

   if( id >= Wifi_First && id <= Wifi_Last )
   {
      switch( id )
      {
         // changing these values requires a restart of the system
         // or at least of the  mqtt service,. This means we have to
         // save the new values in the flash before do a restart.
         // Storing them in the memory makes no sense.
         case Wifi_Name:        strncpy( Wifi_Client_Cfg.Name,     str, sizeof( Wifi_Client_Cfg.Name     ) ); break;
         case Wifi_Server:      strncpy( Wifi_Client_Cfg.Server,   str, sizeof( Wifi_Client_Cfg.Server   ) ); break;
         case Wifi_Username:    strncpy( Wifi_Client_Cfg.Username, str, sizeof( Wifi_Client_Cfg.Username ) ); break;
         case Wifi_Password:    strncpy( Wifi_Client_Cfg.Password, str, sizeof( Wifi_Client_Cfg.Password ) ); break;
         case Wifi_Port:        Wifi_Client_Cfg.Port  = value; break;
      }
      return 1;
   }
   else if( id >= Wlan_First && id <= Wlan_Last )
   {
      // process a list
      return 1;
   }
   else if( id >= Ap_First && id <= Ap_Last )
   {
      switch( id )
      {
         case Ap_Ssid:           strncpy( Wifi_Ap_Cfg.Ssid,     str, sizeof( Wifi_Ap_Cfg.Ssid   ) ); break;
         case Ap_Password:       strncpy( Wifi_Ap_Cfg.Password, str, sizeof( Wifi_Ap_Cfg.Password ) ); break;
         case Ap_Authmode:       Wifi_Ap_Cfg.Authmode       = value; break;
         case Ap_Channel:        Wifi_Ap_Cfg.Channel        = value; break;
         case Ap_SsidHidden:     Wifi_Ap_Cfg.SsidHidden     = value; break;
         case Ap_MaxConnection:  Wifi_Ap_Cfg.MaxConnection  = value; break;
         case Ap_Bandwidth:      Wifi_Ap_Cfg.Bandwidth      = value; break;
         case Ap_BeaconInterval: Wifi_Ap_Cfg.BeaconInterval = value; break;
         case Ap_Address:        Wifi_Ap_Cfg.Address        = value; break;
         case Ap_Gateway:        Wifi_Ap_Cfg.Gateway        = value; break;
         case Ap_Netmask:        Wifi_Ap_Cfg.Netmask        = value; break;
      }
      return 1;
   }
   else if( id >= Sta_First && id <= Sta_Last )
   {
      switch( id )
      {
         case Sta_Only:         Wifi_Sta_Cfg.Only       = value; break;
         case Sta_PowerSave:    Wifi_Sta_Cfg.PowerSave  = value; break;
         case Sta_StaticIp:     Wifi_Sta_Cfg.StaticIp   = value; break;
         case Sta_Address:      Wifi_Sta_Cfg.Address    = value; break;
         case Sta_Gateway:      Wifi_Sta_Cfg.Gateway    = value; break;
         case Sta_Netmask:      Wifi_Sta_Cfg.Netmask    = value; break;
         case Sta_Client_Name:  strncpy( Wifi_Sta_Cfg.Client_Name, str, sizeof( Wifi_Sta_Cfg.Client_Name   ) ); break;
      }
      return 1;
   }
   else if( id >= Dhcp_First && id <= Dhcp_Last )
   {
      switch( id )
      {
         case Dhcp_Enable:     Wifi_Dhcp_Cfg.Enable  = value; break;
         case Dhcp_Server:     Wifi_Dhcp_Cfg.Server  = value; break;
         case Dhcp_Address:    Wifi_Dhcp_Cfg.Address = value; break;
         case Dhcp_Gateway:    Wifi_Dhcp_Cfg.Gateway = value; break;
         case Dhcp_Netmask:    Wifi_Dhcp_Cfg.Netmask = value; break;
      }
      return 1;
   }

   // id not handled here, nothing to update
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int ICACHE_FLASH_ATTR wifi_apply_config( int id, char *str, int value )
{
   ESP_LOGD( TAG, "wifi_update_config 0x%02x %s %d", id, S( str ), value );

   if( id >= Wifi_First && id <= Wifi_Last )
   {
      switch( id )
      {
         case Wifi_Name:        break;
         case Wifi_Server:      break;
         case Wifi_Username:    break;
         case Wifi_Password:    break;
         case Wifi_Port:        break;
      }
      return 1;
   }
   else if( id >= Wlan_First && id <= Wlan_Last )
   {
      // process a list
      return 1;
   }
   else if( id >= Ap_First && id <= Ap_Last )
   {
      switch( id )
      {
         case Ap_Ssid:           break;
         case Ap_Password:       break;
         case Ap_Authmode:       break;
         case Ap_Channel:        break;
         case Ap_SsidHidden:     break;
         case Ap_MaxConnection:  break;
         case Ap_Bandwidth:      break;
         case Ap_BeaconInterval: break;
         case Ap_Address:        break;
         case Ap_Gateway:        break;
         case Ap_Netmask:        break;
      }
      return 1;
   }
   else if( id >= Sta_First && id <= Sta_Last )
   {
      switch( id )
      {
         case Sta_Only:         break;
         case Sta_PowerSave:    break;
         case Sta_StaticIp:     break;
         case Sta_Address:      break;
         case Sta_Gateway:      break;
         case Sta_Netmask:      break;
         case Sta_Client_Name:  break;
      }
      return 1;
   }
   else if( id >= Dhcp_First && id <= Dhcp_Last )
   {
      switch( id )
      {
         case Dhcp_Enable:     break;
         case Dhcp_Server:     break;
         case Dhcp_Address:    break;
         case Dhcp_Gateway:    break;
         case Dhcp_Netmask:    break;
      }
      return 1;
   }

   // id not handled here, nothing to update
   return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// setup the configuration items from the default settings or the spi flash

void ICACHE_FLASH_ATTR getClientConfig( void )
{
   config_get( Wifi_Name,     Wifi_Client_Cfg.Name,     sizeof( Wifi_Client_Cfg.Name     ) );
   config_get( Wifi_Server,   Wifi_Client_Cfg.Server,   sizeof( Wifi_Client_Cfg.Server   ) );
   config_get( Wifi_Username, Wifi_Client_Cfg.Username, sizeof( Wifi_Client_Cfg.Username ) );
   config_get( Wifi_Password, Wifi_Client_Cfg.Password, sizeof( Wifi_Client_Cfg.Password ) );

   Wifi_Client_Cfg.Port = config_get_int( Wifi_Port );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR getSoftAPConfig( void )
{
   config_get( Ap_Ssid,     Wifi_Ap_Cfg.Ssid,     sizeof( Wifi_Ap_Cfg.Ssid   ) );
   config_get( Ap_Password, Wifi_Ap_Cfg.Password, sizeof( Wifi_Ap_Cfg.Password ) );

   Wifi_Ap_Cfg.Authmode       = config_get_int( Ap_Authmode       ); // WIFI_AUTH_WPA_WPA2_PSK,
   Wifi_Ap_Cfg.Channel        = config_get_int( Ap_Channel        );
   Wifi_Ap_Cfg.SsidHidden     = config_get_int( Ap_SsidHidden     );
   Wifi_Ap_Cfg.MaxConnection  = config_get_int( Ap_MaxConnection  );
   Wifi_Ap_Cfg.Bandwidth      = config_get_int( Ap_Bandwidth      );
   Wifi_Ap_Cfg.BeaconInterval = config_get_int( Ap_BeaconInterval );
   Wifi_Ap_Cfg.Address        = config_get_int( Ap_Address        );
   Wifi_Ap_Cfg.Gateway        = config_get_int( Ap_Gateway        );
   Wifi_Ap_Cfg.Netmask        = config_get_int( Ap_Netmask        );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void getWlanConfig( void )
{
   Wifi_Wlan_Cfg.Wlan_Known_AP = config_get_int( Wlan_Known_AP );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR getStationConfig( void )
{
   Wifi_Sta_Cfg.Only      = config_get_int( Sta_Only      );
   Wifi_Sta_Cfg.PowerSave = config_get_int( Sta_PowerSave );
   Wifi_Sta_Cfg.StaticIp  = config_get_int( Sta_StaticIp  );
   Wifi_Sta_Cfg.Address   = config_get_int( Sta_Address   );
   Wifi_Sta_Cfg.Gateway   = config_get_int( Sta_Gateway   );
   Wifi_Sta_Cfg.Netmask   = config_get_int( Sta_Netmask   );
   config_get( Sta_Client_Name, Wifi_Sta_Cfg.Client_Name, sizeof( Wifi_Sta_Cfg.Client_Name ) );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR getDhcpConfig( void )
{
   Wifi_Dhcp_Cfg.Enable  = config_get_int( Dhcp_Enable  );
   Wifi_Dhcp_Cfg.Server  = config_get_int( Dhcp_Server  );
   Wifi_Dhcp_Cfg.Address = config_get_int( Dhcp_Address );
   Wifi_Dhcp_Cfg.Gateway = config_get_int( Dhcp_Gateway );
   Wifi_Dhcp_Cfg.Netmask = config_get_int( Dhcp_Netmask );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static Configuration_Item_t cfgItem;

Configuration_Item_t* ICACHE_FLASH_ATTR init_wifi_config( void )
{
   cfgItem.config_keywords = WifiConfig_Keywords;
   cfgItem.num_keywords    = sizeof( WifiConfig_Keywords ) / sizeof( Config_Keyword_t );
   cfgItem.defaults        = wifi_settings;
   cfgItem.get_config      = wifi_get_config;
   cfgItem.compare_config  = wifi_compare_config;
   cfgItem.update_config   = wifi_update_config;
   cfgItem.apply_config    = wifi_apply_config;

   return &cfgItem;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
