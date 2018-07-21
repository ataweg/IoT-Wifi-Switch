// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_config.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-07-11  AWe   reorder pining for NodeMCU and ESP-201
//    2017-11-20  AWe   remove unused OnOFF-Button, GPIO5
//    2017-11-12  AWe   adapt for use with current HW: only one relay, different pinning
//    2017-08-19  AWe   change debug message print
//    2017-08-07  AWe   initial implementation
//
// --------------------------------------------------------------------------

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

// --------------------------------------------------------------------------
// general defines
// --------------------------------------------------------------------------

#define HIGH( x )   ( (x )>>8 )
#define LOW( x )    ( (x ))

/* Macros to access bytes within words and words within longs */
#define LOW_BYTE( x )     ( (unsigned char )( (x )&0xFF ))
#define HIGH_BYTE( x )    ( (unsigned char )( (( x )>>8 )&0xFF ))
#define LOW_WORD( x )     ( (unsigned short )( (x )&0xFFFF ))
#define HIGH_WORD( x )    ( (unsigned short )( (( x )>>16 )&0xFFFF ))

// --------------------------------------------------------------------------
// used in this project but not defined in SDK
// --------------------------------------------------------------------------

// see C:\Espressif\ESP8266_NONOS_SDK\include\user_interface.h( 570 ):

#define   WPS_CB_ST_SCAN_ERR   ( WPS_CB_ST_WEP+1 )

#define int64_t  sint64_t

// --------------------------------------------------------------------------
// more configuration
// --------------------------------------------------------------------------

#include "user_mqtt_config.h"

// --------------------------------------------------------------------------
// pin definitions
// --------------------------------------------------------------------------

// Hardware
#if defined( ESP201 )
   #define TARGET_DEVICE         "ESP-201"

   #define pin_Button             0       // in   FUNC_GPIO0  - PERIPHS_IO_MUX_GPIO0_U
   #define pin_SysLed             2       // out  FUNC_GPIO2  - PERIPHS_IO_MUX_GPIO2_U
   #define pin_InfoLed            4       // out  FUNC_GPIO4  - PERIPHS_IO_MUX_GPIO4_U
   #define pin_RelayCoil_1       12       // out  FUNC_GPIO12 - PERIPHS_IO_MUX_MTDI_U
   #define pin_RelayCoil_2       13       // out  FUNC_GPIO13 - PERIPHS_IO_MUX_MTCK_U
   #define pin_PowerSense        14       // in   FUNC_GPIO14 - PERIPHS_IO_MUX_MTMS_U

   #define BUTTON_IO_NUM         pin_Button         // WPS Button
   #define BUTTON_IO_MUX         PERIPHS_IO_MUX_GPIO0_U
   #define BUTTON_IO_FUNC        FUNC_GPIO0

#elif defined( NODEMCU )
   #define TARGET_DEVICE               "NodeMCU"

   #define pin_Button             0       // in   FUNC_GPIO0  - PERIPHS_IO_MUX_GPIO0_U
   #define pin_SysLed             2       // out  FUNC_GPIO2  - PERIPHS_IO_MUX_GPIO2_U
   #define pin_InfoLed            4       // out  FUNC_GPIO4  - PERIPHS_IO_MUX_GPIO4_U
   #define pin_RelayCoil_1       12       // out  FUNC_GPIO12 - PERIPHS_IO_MUX_MTDI_U
   #define pin_RelayCoil_2       13       // out  FUNC_GPIO13 - PERIPHS_IO_MUX_MTCK_U
   #define pin_PowerSense        14       // in   FUNC_GPIO14 - PERIPHS_IO_MUX_MTMS_U

   #define pin_Switch             5       // in   FUNC_GPIO5  - PERIPHS_IO_MUX_GPIO5_U

   #define BUTTON_IO_NUM         pin_Button         // WPS Button
   #define BUTTON_IO_MUX         PERIPHS_IO_MUX_GPIO0_U
   #define BUTTON_IO_FUNC        FUNC_GPIO0

#elif defined( NODEMCU_MODUL )
   #define TARGET_DEVICE         "NodeMCU-Modul"

   // for NodeMCU module without external components, use the on board led as relay on/off indicator
   // here use the on board led as InfoLed for relay on/off indicator

   #define pin_Button             0       // in   FUNC_GPIO0  - PERIPHS_IO_MUX_GPIO0_U
   #define pin_InfoLed            2       // out  FUNC_GPIO2  - PERIPHS_IO_MUX_GPIO2_U
   #define pin_SysLed            -1
   #define pin_RelayCoil_1       -1
   #define pin_RelayCoil_2       -1
   #define pin_PowerSense        -1

   #define BUTTON_IO_NUM         pin_Button         // WPS Button
   #define BUTTON_IO_MUX         PERIPHS_IO_MUX_GPIO0_U
   #define BUTTON_IO_FUNC        FUNC_GPIO0

#else
   #define TARGET_DEVICE         "ESP8266"

   #define pin_Button             0       // in   FUNC_GPIO0  - PERIPHS_IO_MUX_GPIO0_U
   #define pin_SysLed             2       // out  FUNC_GPIO2  - PERIPHS_IO_MUX_GPIO2_U
   #define pin_InfoLed           -1
   #define pin_RelayCoil_1       -1
   #define pin_RelayCoil_2       -1
   #define pin_PowerSense        -1

   #define BUTTON_IO_NUM         pin_Button         // WPS Button
   #define BUTTON_IO_MUX         PERIPHS_IO_MUX_GPIO0_U
   #define BUTTON_IO_FUNC        FUNC_GPIO0
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// same values as defiend enum in mqtt_devices
// see include\user_mqtt.h

enum _devices
{
   // listen devices
   Relay,
   InfoLed,
   SysLed,

   // device status
   Button,
   PowerSense,
   Adc,
   Time
} ;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif  // __USER_CONFIG_H__

