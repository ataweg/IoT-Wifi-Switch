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

#ifndef __KEY_H__
#define __KEY_H__

#include "gpio.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef void ( * key_function )( void );

//  _____           _____
//       |_________|
//  0 -> 1 -> 3 -> 2 -> 0

#define KEY_UP          0
#define KEY_PRESSED     1
#define KEY_DOWN        3
#define KEY_RELEASED    2
#define KEY_IDLE        KEY_UP

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct single_key_param
{
   uint8_t  key_state;
   uint8_t  dmy;
   uint8_t  gpio_id;
   uint8_t  gpio_func;
   uint32_t gpio_name;
   uint32_t key_counter;

   key_function short_press_cb;  // short press function, needed to install
   key_function long_press_cb;   // long press function, needed to install
   uint32_t press_long_time;
   key_function very_long_press_cb;
   uint32_t press_very_long_time;

   os_timer_t key_50ms;
} single_key_param;

typedef struct keys_param
{
   uint8_t key_num;                       // number of keys
   single_key_param* *single_key_list; // pointer to an array
} keys_param;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

single_key_param* ICACHE_FLASH_ATTR single_key_init( uint8_t gpio_id, uint32_t gpio_name, uint8_t gpio_func,
                                          key_function very_long_press, uint32_t press_very_long_time,
                                          key_function long_press,  uint32_t press_long_time,
                                          key_function short_press );
void ICACHE_FLASH_ATTR keys_init( keys_param *keys, int key_num, single_key_param* *single_key_list );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif  // __KEY_H__
