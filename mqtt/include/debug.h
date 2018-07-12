/*
 * debug.h
 *
 *  Created on: Dec 4, 2014
 *      Author: Minh
 */

#ifndef USER_DEBUG_H_
#define USER_DEBUG_H_

#if defined( MQTT_DEBUG_ON )
   #ifdef __AWEDBG_H__
      #define MQTT_ERROR      ERROR
      #define MQTT_WARNING    WARNING
      #define MQTT_INFO       INFO
      #define MQTT_DEBUG( format, ... )      DBG( V_ALL, format, ## __VA_ARGS__ )
   #else
      #define MQTT_ERROR      MQTT_DEBUG
      #define MQTT_WARNING    MQTT_DEBUG
      #define MQTT_INFO       MQTT_DEBUG
      #define MQTT_DEBUG( format, ... )      os_printf( format"\r\n", ## __VA_ARGS__ )
   #endif // __AWEDBG_H__
#else
   #define MQTT_ERROR      MQTT_DEBUG
   #define MQTT_WARNING    MQTT_DEBUG
   #define MQTT_INFO       MQTT_DEBUG
   #define MQTT_DEBUG( format, ... )
#endif

#endif /* USER_DEBUG_H_ */
