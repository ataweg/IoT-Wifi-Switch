// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_mqtt_config.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-08-10  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __USER_MQTT_CONFIG_H__
#define __USER_MQTT_CONFIG_H__

#define CFG_HOLDER               0x00FF55A4  /* Change this value to load default configurations */
#define CFG_LOCATION             0x7C        /* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE                    // not used
#define MQTT_SSL_ENABLE
// define MQTT_SSL_SIZE            2048        // set to default value

/*DEFAULT CONFIGURATIONS*/
#define MQTT_BUF_SIZE            1024
#define QUEUE_BUFFER_SIZE        2048

#define PROTOCOL_NAMEv31         /*MQTT version 3.1 compatible with Mosquitto v0.15*/
// PROTOCOL_NAMEv311              /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

#define MQTT_RECONNECT_TIMEOUT   5  /*second*/

#endif // __USER_MQTT_CONFIG_H__