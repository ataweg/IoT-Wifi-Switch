// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          rtc.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-05  AWe   initial implementation
//                      see also
//                         C:\Espressif\ESP8266_NONOS_SDK\third_party\include\lwip\app\time.h
//                         C:\Program Files ( x86 )\Arduino\hardware\tools\avr\avr\include\time.h
//
// --------------------------------------------------------------------------

#ifndef __RTC_H__
#define __RTC_H__

#include <time.h>

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define  RTC_MAGIC  0x476F6F64

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
   uint64_t clock;         // accurate time in us, this is the real local system clock; overflows in 584 years
   uint32_t last_time;     // last value read from system_get_time()
   uint32_t last_rtc;      // last value read from the rtc register
   uint32_t last_cal;
   uint32_t magic;
   time_t   start_time;    // unix time when system starts
} rtc_timer_t;

extern rtc_timer_t rtc_time;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// do some rounding
#define rtc_setTime_us( time )    ( ( uint64_t )( (rtc_setTime_ns( ( uint64_t )( time ) * ( uint64_t )1000 )  + ( uint64_t )500 ) / ( uint64_t )1000 ))
#define rtc_setTime_ms( time )    ( ( uint64_t )( (rtc_setTime_ns( ( uint64_t )( time ) * ( uint64_t )1000000 )  + ( uint64_t )500000 ) / ( uint64_t )1000000 ))
#define rtc_setTime( time )       ( ( time_t )  ( (rtc_setTime_ns( ( uint64_t )( time ) * ( uint64_t )1000000000 )  + ( uint64_t )500000000 ) / ( uint64_t )1000000000 ))

#define rtc_getTime_us()         ( ( uint64_t )( (rtc_getTime_ns() + ( uint64_t )500 ) /( uint64_t )1000 ) )
#define rtc_getTime_ms()         ( ( uint64_t )( (rtc_getTime_ns() + ( uint64_t )500000 ) /( uint64_t )1000000 ) )
#define rtc_getTime()            ( ( time_t )  ( (rtc_getTime_ns() + ( uint64_t )500000000 ) /( uint64_t )1000000000 ) )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t   ICACHE_FLASH_ATTR rtc_init( void );
uint64_t ICACHE_FLASH_ATTR rtc_setTime_ns( uint64_t time );
uint64_t ICACHE_FLASH_ATTR rtc_getTime_ns( void );
time_t   ICACHE_FLASH_ATTR rtc_getUptime( void );
time_t   ICACHE_FLASH_ATTR rtc_getStartTime( void );

time_t   ICACHE_FLASH_ATTR rtc_test( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // __RTC_H__
