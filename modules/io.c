// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          io.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-07-11  AWe   reorder pining for NodeMCU
//    2018-04-12  AWe   add functions which check if an io pin is not used
//    2017-11-20  AWe   remove unused OnOFF-Button, GPIO5
//    2017-11-15  AWe   move WPS related functions resetBtnTimerCb(),
//                        ioBtnTimerInit() to user_wps.c
//    2017-11-13  AWe   adjust pin configuration to HW
//
// --------------------------------------------------------------------------

// for pin definitions see
//   C:\Espressif\ESP8266_NONOS_SDK\include\eagle_soc.h

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
static const char* TAG = "io";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>

#include "user_config.h"
#include "io.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR io_input_get( int8_t pin )
{
   if( pin != -1 )
      return GPIO_INPUT_GET( pin );
   else
      return 0;
}

void ICACHE_FLASH_ATTR io_output_set( int8_t pin, uint8_t val )
{
   if( pin != -1 )
   {
      ESP_LOGD( TAG, "set pin %d to %d", pin, val );

      GPIO_OUTPUT_SET( pin, val );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR io_pins_init( void )
{
   // Initialize the GPIO subsystem.
   gpio_init();

#if defined( ESP201 )
   // pin_WPS_Button               4 -> 0
   // pin_SysLed                   2
   // pin_InfoLed                  0 -> 4
   // pin_RelayCoil_1             12
   // pin_RelayCoil_2             13
   // pin_Power_sense             14

   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 );   // in  - WPS_Button
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 );   // out - SysLed
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4 );   // out - InfoLed
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12 );   // out - RelayCoil_1
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13 );   // out - RelayCoil_2
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14 );   // in  - Power_sense

   // Led are connected to VCC, so all Leds are off
   GPIO_DIS_OUTPUT( pin_Button );       // define pin as input
   GPIO_OUTPUT_SET( pin_SysLed, 0 );
   GPIO_OUTPUT_SET( pin_InfoLed, 1 );
   GPIO_OUTPUT_SET( pin_RelayCoil_1, 0 );
   GPIO_OUTPUT_SET( pin_RelayCoil_2, 0 );
   GPIO_DIS_OUTPUT( pin_PowerSense );   // define pin as input

#elif defined( NODEMCU )
   // pin_Button                   0
   // pin_SysLed                   2
   // pin_InfoLed                  4
   // pin_RelayCoil_1             12 ( rev A-1:  5 )
   // pin_RelayCoil_2             13 ( rev A-1: 14 )
   // pin_PowerSense              14 ( rev A-1:  - )
   // pin_Switch                  5  ( rev A-1: 12 )

   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 );   // in   - WPS_Button
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 );   // out  - SysLed
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4 );   // out  - InfoLed
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12 );   // out  - RelayCoil_1
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13 );   // out  - RelayCoil_2
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14 );   // in  - Power_sense

   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5 );   // in   - Switch

   // Led are connected to VCC, so all Leds are off
   GPIO_DIS_OUTPUT( pin_Button );                           // WPS_Button - define pin as input
   GPIO_OUTPUT_SET( pin_SysLed, 1 );                        // SYS_LED
   GPIO_OUTPUT_SET( pin_InfoLed, 1 );                       // INFO_LED
   GPIO_OUTPUT_SET( pin_RelayCoil_1, 0 );                   // Relay_1
   GPIO_OUTPUT_SET( pin_RelayCoil_2, 0 );                   // Relay_2
   GPIO_DIS_OUTPUT( pin_PowerSense );                       // define pin as input

   GPIO_DIS_OUTPUT( pin_Switch );                           // not used

#elif defined( NODEMCU_MODUL )
   // for NodeMCU module without external components
   // here use the on board led as InfoLed for relay on/off indicator

   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 );   // in   - WPS_Button
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 );   // out  - SysLed

   // Led is connected to VCC, so led is on
   GPIO_DIS_OUTPUT( pin_Button );                           // WPS_Button - define pin as input
   GPIO_OUTPUT_SET( pin_InfoLed, 0 );                       // Relay_1

#else
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0 );   // in   - WPS_Button
   PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 );   // out  - SysLed

   // Led is connected to VCC, so led is on
   GPIO_DIS_OUTPUT( pin_Button );                           // WPS_Button - define pin as input
   GPIO_OUTPUT_SET( pin_SysLed, 0 );                        // SYS_LED
#endif
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
