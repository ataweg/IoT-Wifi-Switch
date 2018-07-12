// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - IoT-Wifi-Switch
//
// File          key.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-11-25  AWe   redesigned for proper working
//
// --------------------------------------------------------------------------

/*
 * ESPRESSIF MIT License
 *
 * Copyright ( c ) 2016 <ESPRESSIF SYSTEMS ( SHANGHAI ) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files ( the "Software" ), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
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

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char* TAG = "keys";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"                // GPIO_PIN_INTR_ANYEDGE
#include "user_interface.h"
#include "esp_missing.h"         // ets_isr_mask()

#include "keys.h"
#include "io.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void key_intr_handler( void *arg );

/******************************************************************************
 * FunctionName : single_key_init
 * Description  : init single key's gpio and register function
 * Parameters   : uint8_t  gpio_id - which gpio to use
 *                uint32_t gpio_name - gpio mux name
 *                uint32_t gpio_func - gpio function
 *                key_function long_press - long press function, needed to install
 *                key_function short_press - short press function, needed to install
 * Returns      : single_key_param - single key parameter, needed by key init
 *******************************************************************************/

single_key_param* ICACHE_FLASH_ATTR single_key_init( uint8_t gpio_id, uint32_t gpio_name, uint8_t gpio_func,
                                          key_function very_long_press, uint32_t press_very_long_time,
                                          key_function long_press, uint32_t press_long_time,
                                          key_function short_press )
{
   single_key_param* single_key = ( single_key_param* )os_malloc( sizeof( single_key_param ) );

   single_key->key_state   = KEY_IDLE;
   single_key->dmy         = 0;
   single_key->gpio_id     = gpio_id;
   single_key->gpio_func   = gpio_func;
   single_key->gpio_name   = gpio_name;
   single_key->key_counter = 0;

   single_key->short_press_cb       = short_press;
   single_key->long_press_cb        = long_press;
   single_key->press_long_time      = press_long_time;
   single_key->very_long_press_cb   = very_long_press;
   single_key->press_very_long_time = press_very_long_time;

   return single_key;
}

/******************************************************************************
 * FunctionName : key_init
 * Description  : init keys
 * Parameters   : key_param *keys - keys parameter, which inited by single_key_init
 * Returns      : none
 *******************************************************************************/

void ICACHE_FLASH_ATTR keys_init( keys_param *keys, int key_num, single_key_param* *single_key_list )
{
   uint8_t i;

   keys->key_num = key_num;
   keys->single_key_list = single_key_list;

   ETS_GPIO_INTR_ATTACH( key_intr_handler, keys );
   ETS_GPIO_INTR_DISABLE();

   for( i = 0; i < keys->key_num; i++ )
   {
      single_key_param* single_key = keys->single_key_list[i];

      single_key->key_state = KEY_IDLE;
      single_key->key_counter = 0;

      PIN_FUNC_SELECT( single_key->gpio_name, single_key->gpio_func );

      gpio_output_set( 0, 0, 0, GPIO_ID_PIN( single_key->gpio_id ) );

      gpio_register_set( GPIO_PIN_ADDR( single_key->gpio_id ), GPIO_PIN_INT_TYPE_SET( GPIO_PIN_INTR_DISABLE )
                         | GPIO_PIN_PAD_DRIVER_SET( GPIO_PAD_DRIVER_DISABLE )
                         | GPIO_PIN_SOURCE_SET( GPIO_AS_PIN_SOURCE ) );

      // clear gpio14 status
      GPIO_REG_WRITE( GPIO_STATUS_W1TC_ADDRESS, BIT( single_key->gpio_id ) );

      // enable interrupt
      gpio_pin_intr_state_set( GPIO_ID_PIN( single_key->gpio_id ), GPIO_PIN_INTR_NEGEDGE );
   }

   ETS_GPIO_INTR_ENABLE();
}

/******************************************************************************
 * FunctionName : key_50ms_cb
 * Description  : 50ms timer callback to check it's a real key push
 * Parameters   : single_key_param *single_key - single key parameter
 * Returns      : none
 *******************************************************************************/

static void ICACHE_FLASH_ATTR key_50ms_cb( single_key_param *single_key )
{
   os_timer_disarm( &single_key->key_50ms );

   // high, then key is up
   if( 1 == io_input_get( single_key->gpio_id  ) )
   {
      if( single_key->key_state == KEY_RELEASED )
      {
         ESP_LOGD( TAG, "Key released (0x%08x) %u", single_key, single_key->key_counter * 50 );
      }
      single_key->key_state = KEY_IDLE;
      gpio_pin_intr_state_set( GPIO_ID_PIN( single_key->gpio_id ), GPIO_PIN_INTR_NEGEDGE );

      uint32_t duration = single_key->key_counter * 50;
      if( single_key->very_long_press_cb && duration > single_key->press_very_long_time )        // 10 sec
      {
         ESP_LOGD( TAG, "Registered a very long press %u", duration );
         single_key->very_long_press_cb();
      }
      else if( single_key->long_press_cb && duration > single_key->press_long_time )        // 2 sec
      {
         ESP_LOGD( TAG, "Registered a long press %u", duration );
         single_key->long_press_cb();
      }
      else if( single_key->short_press_cb )
      {
         ESP_LOGD( TAG, "Registered a short press %u", duration );
         single_key->short_press_cb();
      }

      single_key->key_counter = 0;
   }
   else
   {
      // key is down
      if( single_key->key_state == KEY_PRESSED )
      {
         ESP_LOGD( TAG, "Key pressed  (0x%08x) %u", single_key, single_key->key_counter * 50 );
      }
      single_key->key_state = KEY_DOWN;
      single_key->key_counter++;

      os_timer_setfn( &single_key->key_50ms, ( os_timer_func_t * )key_50ms_cb, single_key );
      os_timer_arm( &single_key->key_50ms, 50, 0 );

      gpio_pin_intr_state_set( GPIO_ID_PIN( single_key->gpio_id ), GPIO_PIN_INTR_POSEDGE );
   }
}

/******************************************************************************
 * FunctionName : key_intr_handler
 * Description  : key interrupt handler
 * Parameters   : key_param *keys - keys parameter, which inited by single_key_init
 * Returns      : none
 *******************************************************************************/

static void key_intr_handler( void *arg )
{
   uint8_t i;
   uint32_t gpio_status = GPIO_REG_READ( GPIO_STATUS_ADDRESS );  // read Interrupt status register
   keys_param *keys = ( keys_param * )arg;

   for( i = 0; i < keys->key_num; i++ )
   {
      single_key_param* single_key = keys->single_key_list[i];

      if( gpio_status & BIT( single_key->gpio_id ) )
      {
         // disable interrupt
         gpio_pin_intr_state_set( GPIO_ID_PIN( single_key->gpio_id ), GPIO_PIN_INTR_DISABLE );

         // clear interrupt status
         GPIO_REG_WRITE( GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT( single_key->gpio_id ) );

         switch( single_key->key_state )
         {
            case KEY_IDLE:             // key is pressed.
               single_key->key_state = KEY_PRESSED;
               break;
            case KEY_DOWN:             // key is released
               single_key->key_state = KEY_RELEASED;
               break;
            case KEY_RELEASED:         // key was released ( bouncing?)
            case KEY_PRESSED:          // key was pressed( bouncing?)
               break;
         }

         // timer retrigger during button bouncing
         // 50ms, check if this is a real key up
         os_timer_disarm( &single_key->key_50ms );
         os_timer_setfn( &single_key->key_50ms, ( os_timer_func_t * )key_50ms_cb, single_key );
         os_timer_arm( &single_key->key_50ms, 50, 0 );

         gpio_pin_intr_state_set( GPIO_ID_PIN( single_key->gpio_id ), GPIO_PIN_INTR_ANYEDGE );
      }
   }
}
