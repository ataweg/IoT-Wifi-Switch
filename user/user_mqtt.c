// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_mqtt.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-03  AWe   add mqttWifiIsConnected(), mqttWifiDisconnect()
//                      change mqttWifiIsConnect()
//    2018-05-03  AWe   add timeout for reconnection
//    2018-04-23  AWe   add more robustness
//    2017-11-12  AWe   adapt for use with current HW: only one relay
//    2017-08-19  AWe   change debug message printing
//    2017-08-10  AWe   get stuff from
//                         DevKit-Examples\ESP8266\esp_mqtt\user\user_main.c
//    2017-06-07  AWe   update to latest version from github
//
// --------------------------------------------------------------------------

/* main.c -- MQTT client example
*
* Copyright ( c ) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES ( INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION ) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT ( INCLUDING NEGLIGENCE OR OTHERWISE )
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

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
static const char* TAG = "user/user_mqtt.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>
#include <mem.h>

#include "device.h"     // devGet(), devSet();
#include "configs.h"
#include "mqtt_config.h"
#include "user_mqtt.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR mqttConnectedCb( uint32_t *args );
static void ICACHE_FLASH_ATTR mqttDisconnectedCb( uint32_t *args );
static void ICACHE_FLASH_ATTR mqttPublishedCb( uint32_t *args );
static void ICACHE_FLASH_ATTR mqttDataCb( uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

MQTT_Client mqttClient;

MQTT_client_cfg_t MQTT_Client_Cfg;
MQTT_wifi_cfg_t MQTT_Wifi_Cfg[ num_devices ];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR mqttSubscribe( MQTT_Client* client, enum mqtt_devices device )
{
//   if( client->connState == MQTT_DATA )
   {
      MQTT_Subscribe( client, MQTT_Wifi_Cfg[ device ].Topic_sub, MQTT_Wifi_Cfg[ device ].QoS );
   }
}

void ICACHE_FLASH_ATTR mqttPublish( MQTT_Client* client, enum mqtt_devices device, char *value )
{
   if( client->connState == MQTT_DATA )
   {
      if( MQTT_Wifi_Cfg[ device ].Enable_publish )
      {
         MQTT_Publish( client,  MQTT_Wifi_Cfg[ device ].Topic_pub, value, strlen( value ),
                                MQTT_Wifi_Cfg[ device ].QoS, MQTT_Wifi_Cfg[ device ].Retained );
      }
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR mqttWifiIsConnected( void )
{
   int is_connected = false;

   switch( mqttClient.connState )
   {
      case WIFI_CONNECTED:                  //  3   n.u.
      case TCP_CONNECTED:                   // 13   n.u.
      case MQTT_CONNECT_SEND:               // 14
      case MQTT_CONNECT_SENDING:            // 15
      case MQTT_SUBSCIBE_SEND:              // 16   n.u.
      case MQTT_SUBSCIBE_SENDING:           // 17   n.u.
      case MQTT_DATA:                       // 18
      case MQTT_KEEPALIVE_SEND:             // 19
      case MQTT_PUBLISH_RECV:               // 20
      case MQTT_PUBLISHING:                 // 21   n.u.
         is_connected = true;
         break;
   }

   return is_connected;
}

void ICACHE_FLASH_ATTR mqttWifiConnect( void )
{
   ESP_LOGD( TAG, "Connect MQTT client" );
   MQTT_Connect( &mqttClient );
}

void ICACHE_FLASH_ATTR mqttWifiDisconnect( void )
{
   ESP_LOGD( TAG, "Disconnect MQTT client" );
   MQTT_Disconnect( &mqttClient );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// called every 5 seconds

#define MQTT_RECONNECT_MAX_RETRY    ( 3 )                // try it for 60 seconds
#define MQTT_RECONNECT_TIME_TO_WAIT ( 1 * 60 * 1000 )    // in ms

struct retry_policy
{
   uint8_t retry;
   uint8_t interval;
   uint8_t repeat;
   uint8_t dmy;
}
retry_policy_list[] =
{
   {  3,  1,  5 },   // check 3 times every minute 5 minutes long
   {  1, 10,  6 },   // check one time every 10 minutes one hour long
   {  1, 60,  0 },
};

const int max_retry_policy = sizeof( retry_policy_list ) / sizeof( struct retry_policy );
int retry_policy = 0;
int retry_policy_repeat = 0;

int mqtt_ReconnectRetry = 0;
os_timer_t mqtt_ReconnectTimer;

static void ICACHE_FLASH_ATTR mqtt_ReconnectTimeoutCb( void *args )
{
   ESP_LOGI( TAG, "MQTT: Reconnect after timeout ..." );
   MQTT_Client* client = ( MQTT_Client* )args;

   os_timer_disarm( &mqtt_ReconnectTimer );

   if( !mqttWifiIsConnected() )
      MQTT_Connect( client );
}

static void mqttTimeoutCb( uint32_t *args )
{
   ESP_LOGD( TAG, "MQTT: Connect timeout ..." );

   MQTT_Client* client = ( MQTT_Client* )args;

   if( mqtt_ReconnectRetry >= retry_policy_list[ retry_policy ].retry )
   {
      if( retry_policy_list[ retry_policy ].repeat > 0 )
      {
         if( retry_policy_repeat >= retry_policy_list[ retry_policy ].repeat )
         {
            retry_policy_repeat = 0;
               retry_policy++;
         }
         else
            retry_policy_repeat++;
      }

      ESP_LOGD( TAG, "MQTT: Cannot reconnect, try it again in %d minutes ", retry_policy_list[ retry_policy ].interval );
      ESP_LOGD( TAG, "      policy %d, repeat %d", retry_policy, retry_policy_repeat );
      mqtt_ReconnectRetry = 0;
      MQTT_Disconnect( client );

      os_timer_disarm( &mqtt_ReconnectTimer );
      os_timer_setfn( &mqtt_ReconnectTimer, ( os_timer_func_t * )mqtt_ReconnectTimeoutCb, client );
      os_timer_arm( &mqtt_ReconnectTimer, retry_policy_list[ retry_policy ].interval * ( 60 * 1000 ), 0 );   //
   }
   else
   {
      ESP_LOGD( TAG, "MQTT: retry reconnect %d ...", mqtt_ReconnectRetry  );
      mqtt_ReconnectRetry++;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR mqttConnectedCb( uint32_t *args )
{
   MQTT_Client* client = ( MQTT_Client* )args;
   ESP_LOGD( TAG, "MQTT: Connected" );

   // topic on which this mqtt client will listen
   mqttSubscribe( client, dev_Relay );
   mqttSubscribe( client, dev_InfoLed );
   mqttSubscribe( client, dev_SysLed );

   retry_policy = 0;
   retry_policy_repeat = 0;
   mqtt_ReconnectRetry = 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR mqttDisconnectedCb( uint32_t *args )
{
//   MQTT_Client* client = ( MQTT_Client* )args;  // unused
   ESP_LOGD( TAG, "MQTT: Disconnected" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR mqttPublishedCb( uint32_t *args )
{
   // MQTT_Client* client = ( MQTT_Client* )args;  // unused
   // ESP_LOGD( TAG, "MQTT: Published" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// process the received data
// here we send them to the console

static void ICACHE_FLASH_ATTR mqttDataCb( uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len )
{
   char *topicBuf = ( char* )os_zalloc( topic_len + 1 ); // we need this for build zero terminated strings
   char *dataBuf  = ( char* )os_zalloc( data_len + 1 );

//   MQTT_Client* client = ( MQTT_Client* )args;  // unused

   os_memcpy( topicBuf, topic, topic_len );
   topicBuf[topic_len] = 0;

   os_memcpy( dataBuf, data, data_len );
   dataBuf[data_len] = 0;

   ESP_LOGD( TAG, "Receive topic: %s, data: %s", topicBuf, dataBuf );

   // process received data
   mqtt_devices_t dev;
   for( dev = dev_Relay; dev < num_devices ; dev++ )
   {
      if( strcmp( MQTT_Wifi_Cfg[ dev ].Topic_sub, topicBuf ) == 0 )
      {
         int val_onoff;
         if( data_len == 1 )
         {
            val_onoff = dataBuf[0] == '0' ? 0 : 1;

            switch( dev )
            {
               case dev_InfoLed:
                  devSet( InfoLed, val_onoff );
                  break;

               case dev_SysLed:
                  devSet( SysLed, val_onoff );
                  break;

               case dev_Relay:
                  switchRelay( val_onoff );
                  break;

               default:
                  break;
            }
         }
         break;    // topic prcessed, so leave loop
      }
   }

   os_free( topicBuf );
   os_free( dataBuf );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR mqttInit( void )
{
   ESP_LOGD( TAG, "mqttInit" );
   HEAP_INFO( "" );

   getMqttConfig();
   getMqttClientConfig();

   MQTT_InitConnection( &mqttClient, MQTT_Client_Cfg.Server, MQTT_Client_Cfg.Port,
                                     MQTT_Client_Cfg.Enable_SSL ); // setup ata structure
   if( !MQTT_InitClient( &mqttClient, MQTT_Client_Cfg.Client_ID, MQTT_Client_Cfg.Username,
                                      MQTT_Client_Cfg.Password, MQTT_Client_Cfg.Keep_Alive, 1 ) )
   {
      ESP_LOGE( TAG, "Failed to initialize properly. Check MQTT version." );
      return;
   }

   MQTT_InitLWT( &mqttClient, "/lwt", "offline", 0, 0 );    // last will
   MQTT_OnConnected( &mqttClient, mqttConnectedCb );        // called in state CONNECTION_ACCEPTED
   MQTT_OnDisconnected( &mqttClient, mqttDisconnectedCb );  // TCP: Disconnected callback
   MQTT_OnPublished( &mqttClient, mqttPublishedCb );
   MQTT_OnData( &mqttClient, mqttDataCb );
   MQTT_OnTimeout( &mqttClient, mqttTimeoutCb );

   HEAP_INFO( "" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

