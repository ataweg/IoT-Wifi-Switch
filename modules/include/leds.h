#ifndef __LEDS_H__
#define __LEDS_H__

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define NUM_LEDS        2    // we have two leds
#define SYS_LED         0
#define INFO_LED        1

#define LED_OFF         0
#define LED_ON          1
#define LED_FLASH       2
#define LED_BLINK       3
#define LED_NONE        0xFF

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum led_mode
{
   led_off,
   led_flash,
   led_blink,
   led_on,
};

typedef struct
{
   uint8_t mode;
   uint8_t repeat;
   uint8_t on_time;
   uint8_t off_time;
} led_config_t;

typedef struct
{
   uint8_t mode;
   uint8_t counter;    // counts down number of repeats
   uint8_t on_time;
   uint8_t off_time;

   int8_t  pin;        // -1 means there is no pin for this LED
   uint8_t timer;
   union
   {
      struct
      {
         bool no_update: 1;
         bool led_on: 1;
         bool flash_phase: 1;
         bool busy: 1;
      };
      uint8_t val;
   } flags;
   uint8_t dmy;
} led_state_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define SysLed_On()                 led_set( SYS_LED,  LED_ON,  0, 0, 0 )
#define SysLed_Off()                led_set( SYS_LED,  LED_OFF, 0, 0, 0 )
#define InfoLed_On()                led_set( INFO_LED, LED_ON,  0, 0, 0 )
#define InfoLed_Off()               led_set( INFO_LED, LED_OFF, 0, 0, 0 )

#define setSysLed( onoff )          led_set( SYS_LED,  onoff, 0, 0, 0 )
#define setInfoLed( onoff )         led_set( INFO_LED, onoff, 0, 0, 0 )

#define Led_On( id )                led_set( id, LED_ON,      0,  0, 0 )
#define Led_Off( id )               led_set( id, LED_OFF,     0,  0, 0 )
#define Led_Flash_Fast( id )        led_set( id, LED_BLINK,   1,  5, 0 )     // 100ms on, 500ms off
#define Led_Flash_Slow( id )        led_set( id, LED_BLINK,   3, 15, 0 )     // 300ms on,  1,6s off
#define Led_Blink_Fast( id )        led_set( id, LED_BLINK,   3,  3, 0 )     // 300ms on, 300ms off
#define Led_Blink_Slow( id )        led_set( id, LED_BLINK,  15, 15, 0 )     // 1,6s  on,  1,6s off
#define Led_Short_Oneshot( id )     led_set( id, LED_FLASH,   1,  0, 0 )     // 100ms on, then alway off
#define Led_Long_Oneshot( id )      led_set( id, LED_FLASH,  15,  0, 0 )     // 1,6s  on, then alway off

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR leds_init( void );
void ICACHE_FLASH_ATTR leds_update( void );
uint8_t ICACHE_FLASH_ATTR led_get( uint8_t id );
void ICACHE_FLASH_ATTR led_set( uint8_t id, uint8_t mode, uint8_t on_time, uint8_t off_time, uint8_t repeat );
bool ICACHE_FLASH_ATTR led_busy( uint8_t id );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __LEDS_H__