// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          rtc.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2017-09-10  AWe   RTC register as timebase is too inaccurate, but it
//                      runs during sleep.
//    2017-09-05  AWe   initial implementation
//
// --------------------------------------------------------------------------

/*
3.1. Software Timer.................6
3.1.1. os_timer_arm.................6
3.1.2. os_timer_disarm .............6
3.1.3. os_timer_setfn...............7
3.1.4. system_timer_reinit .........7
3.1.5. os_timer_arm_us .............7
3.2. Hardware Timer.................8
3.2.1. hw_timer_init ...............8
3.2.2. hw_timer_arm.................8
3.2.3. hw_timer_set_func ...........9
3.2.4. Hardware Timer Example.......9

3.3.20. system_get_time ............18
3.3.21. system_get_rtc_time ........18
3.3.22. system_rtc_clock_cali_proc .19
3.3.23. system_rtc_mem_write .......19
3.3.24. system_rtc_mem_read ........19
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
static const char* TAG = "rtc";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <user_interface.h>      // system_rtc_mem_read
#include <mem.h>

#include "rtc.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int rtc_need_restart;
rtc_timer_t rtc_time;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
// clock setup
// -----------
// during system setup the clock starts at zero when the last value stored in the
// rtc_mem is invalid. When no restart is required the clock is only updated  with
// a new value from the rtc register

time_t ICACHE_FLASH_ATTR rtc_init( void )
{
   // the true SYS_Time is RTC_Time * ( (cal_factor * 1000 ) >> 12 ) / 1000;

   // check if we need to setup the rtc after a reset
   // ( 1 ) Power-on:             RTC memory contains a random value; RTC timer starts from zero.
   // ( 2 ) Reset by pin CHIP_EN: RTC memory contains a random value; RTC timer starts from zero.
   // ( 3 ) Watchdog reset:       RTC memory won’t change; RTC timer won’t change.
   // ( 4 ) system_restart:       RTC memory won’t change; RTC timer won’t change.
   // ( 5 ) Reset by pin EXT_RST: RTC memory won’t change; RTC timer starts to zero.
   // --> init rtc_time on EXT_RST, CHIP_EN and power on
   // ( 1 ), ( 2 ) REASON_DEFAULT_RST
   // ( 5 )        REASON_EXT_SYS_RST

   if( rtc_need_restart != 0 )
   {
      // read data from the beginning of user data area
      system_rtc_mem_read( 64, &rtc_time, sizeof( rtc_time ) );

      if( rtc_time.magic != RTC_MAGIC || rtc_need_restart == REASON_DEFAULT_RST || rtc_need_restart == REASON_EXT_SYS_RST )
      {
         // rtc_mem is invalid
         rtc_time.magic = RTC_MAGIC;
         rtc_time.clock = 0;
         rtc_time.start_time = 0;
         rtc_time.last_time = system_get_time();
         rtc_time.last_rtc = system_get_rtc_time();
         rtc_time.last_cal = system_rtc_clock_cali_proc();
      }
      else
      {
         // rtc_mem is still valid, but clock not
         // clock has the last value which was written to the rtc_mem
      }

      uint32_t curr_rtc  = system_get_rtc_time();
      uint32_t curr_cal  = system_rtc_clock_cali_proc();
      uint32_t curr_time = system_get_time();

      uint32_t cal = ( ( ( curr_cal * 1000 ) >> 12 ) + ( ( rtc_time.last_cal * 1000 ) >> 12 ) ) / 2;

      rtc_time.clock += ( ( ( uint64_t )( curr_rtc - rtc_time.last_rtc ) ) * cal ) / 1000;

      rtc_time.last_time = curr_time;
      rtc_time.last_rtc  = curr_rtc;
      rtc_time.last_cal  = curr_cal;

      rtc_need_restart = 0;
   }

   uint32_t curr_time = rtc_getTime();

   system_rtc_mem_write( 64, &rtc_time, sizeof( rtc_time ) );

   return curr_time;  // return seconds
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// set the system clock with a time gotten from a time server
// ----------------------------------------------------------
// set the clock with the time from the time server
// get the current value from the rtc register and save it
// recalculate the system startup time

uint64_t ICACHE_FLASH_ATTR rtc_setTime_ns( uint64_t time )
{
   uint64_t curr_time = rtc_time.clock;

   rtc_time.clock = time;

   // update the start time in respect to the new time
   int64_t delta = time - curr_time;
   int64_t diff = ( delta / ( int64_t )1000000000 );

   rtc_time.start_time += diff;

   rtc_init();             // save the new time in the rtc_mem

   return rtc_time.clock;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
// get the current time in ns
// --------------------------
// read the rtc register with system_get_rtc_time()
// get the correction value with system_rtc_clock_cali_proc();
// determine the difference of the rtc value with the pevious one.
// do the correction of the difference with the correction value
// add the corrected difference value to the current time from the clock
// save the read rtc value for the next call

uint64_t ICACHE_FLASH_ATTR rtc_getTime_ns( void )
{
   uint32_t curr_time = system_get_time();  // us

   rtc_time.clock += ( ( uint64_t )( curr_time - rtc_time.last_time ) * ( uint64_t )1000 );
   rtc_time.last_time = curr_time;
   return rtc_time.clock;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
// get the system uptime
// ---------------------
// get the current time
// build the differnce between the current time and the time when the system starts

// returns seconds since last power on or hard reset
time_t ICACHE_FLASH_ATTR rtc_getUptime( void )
{
   time_t curr_time = rtc_getTime();

   return curr_time - rtc_time.start_time;  // return seconds
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

time_t ICACHE_FLASH_ATTR rtc_getStartTime( void )
{
   return rtc_time.start_time;  // return seconds
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if 0 // no rtc test

/*
 * add next line to user_main.c shortly after function user_init() entry
 *
#if 1
   rtc_test();
   os_timer_disarm( &rtc_test_timer );
   os_timer_setfn( &rtc_test_timer, ( os_timer_func_t * )rtc_test, NULL );
   os_timer_arm( &rtc_test_timer, 5000, 1 );
#else
   rtc_init();
#endif
*/

time_t ICACHE_FLASH_ATTR rtc_test( void )
{
   static uint8_t cnt = 0;

   printf( "RTC TIMER: %d \r\n", system_get_rtc_time() );
   printf( "SYS TIMER: %d \r\n", system_get_time() );

   if( rtc_need_restart != 0 )
   {
      // read data from the beginning of user data area
      system_rtc_mem_read( 64, &rtc_time, sizeof( rtc_time ) );

      if( rtc_time.magic != RTC_MAGIC || rtc_need_restart == REASON_DEFAULT_RST || rtc_need_restart == REASON_EXT_SYS_RST )
      {
         // rtc_mem is invalid

         printf( "clock    : %d \r\n", rtc_time.clock );
         printf( "last rtc : %d \r\n", rtc_time.last_rtc );
         printf( "magic    : 0x%08x\r\n", rtc_time.magic );
         printf( "rtc time init...\r\n" );

         rtc_time.magic = RTC_MAGIC;
         rtc_time.clock = 0;
         rtc_time.start_time = 0;
         rtc_time.last_rtc = system_get_rtc_time();

         printf( "last rtc : %d \r\n", rtc_time.last_rtc );
      }
      else
      {
         // rtc_mem is still valid

         printf( "magic correct\r\n" );
         printf( "clock    : %lld \r\n", rtc_time.clock );
         printf( "last rtc : %ld \r\n", rtc_time.last_rtc );
         printf( "magic    : 0x%08x\r\n", rtc_time.magic );
      }
      rtc_need_restart = 0;
   }
   else
   {
      printf( "magic still valid\r\n" );
      printf( "clock    : %lld \r\n", rtc_time.clock );
      printf( "last rtc : %ld \r\n", rtc_time.last_rtc );
      printf( "magic    : 0x%08x\r\n", rtc_time.magic );
   }

   printf( "==================\r\n" );
   printf( "RTC time test : \r\n" );

   uint32_t rtc_t1, rtc_t2;
   uint32_t st_t1, st_t2;
   uint32_t cal_t1, cal_t2;

   rtc_t1 = system_get_rtc_time();
   st_t1 = system_get_time();
   cal_t1 = system_rtc_clock_cali_proc();

   os_delay_us( 300 );

   st_t2 = system_get_time();
   rtc_t2 = system_get_rtc_time();
   cal_t2 = system_rtc_clock_cali_proc();

   printf( " rtc_t1 : %d \r\n", rtc_t1 );
   printf( " rtc_t2 : %d \r\n", rtc_t2 );
   printf( " rtc_t2-t1 : %d \r\n", rtc_t2 - rtc_t1 );
   printf( " st_t2-t2 :  %d  \r\n", st_t2 - st_t1 );
   printf( " cal 1  : %d.%03d  0x%08x\r\n", ( ( cal_t1 * 1000 ) >> 12 ) / 1000, ( ( cal_t1 * 1000 ) >> 12 ) % 1000, cal_t1 );
   printf( " cal 2  : %d.%03d  0x%08x\r\n", ( ( cal_t2 * 1000 ) >> 12 ) / 1000, ( ( cal_t2 * 1000 ) >> 12 ) % 1000, cal_t2 );
   printf( "==================\r\n\r\n" );

   uint32_t curr_rtc = system_get_rtc_time();
   uint32_t curr_cal = system_rtc_clock_cali_proc();

   rtc_time.clock += ( ( ( uint64_t )( curr_rtc - rtc_time.last_rtc ) ) * ( ( uint64_t )( ( curr_cal * 1000 ) >> 12 ) ) );
   rtc_time.last_rtc = curr_rtc;

   printf( " rtc clock     : %lld \r\n", rtc_time.clock );
   printf( " power on time : %lldus\r\n", rtc_time.clock / 1000 );
   printf( " power on time : %lld.%02llds\r\n", ( rtc_time.clock / 10000000 ) / 100, ( rtc_time.clock / 10000000 ) % 100 );

   rtc_time.last_rtc = rtc_t2;
   system_rtc_mem_write( 64, &rtc_time, sizeof( rtc_time ) );
   printf( "------------------------\r\n" );

#if 1
   if( 20 == ( cnt++ ) )
   {
      printf( " system restart\r\n" );
      system_restart();
   }
   else
   {
      printf( " continue ...\r\n" );
   }
#else
   cnt++;
#endif
   printf( " count:  %d\r\n", cnt );
   printf( " uptime: %lds\r\n", rtc_getUptime() );

   return ( time_t )( ( uint64_t )rtc_time.clock / 1000 / 1000 / 1000 ); // return seconds
}
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
