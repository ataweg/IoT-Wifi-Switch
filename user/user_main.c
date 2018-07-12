// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_main.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-26  AWe   on startup copy esp_init_data_default to spi flash
//    2018-04-23  AWe   add function systemReadyCb() which calls the init routiones
//                         for some wifi depend modules
//    2018-04-18  AWe   move the wifi stuff to user_wifi.c. Add an event handler there
//    2017-11-14  AWe   configure and enable WPS button in user_init()
//    2017-08-19  AWe   change debug message printing
//    2017-08-10  AWe   get stuff from
//                         DevKit-Examples\ESP8266\EspLightNode\user\user_main.c
//    2017-08-07  AWe   initial implementation
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#ifndef ESP_PLATFORM
   #define _PRINT_CHATTY
   #define V_HEAP_INFO
#endif

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char *TAG = "user/user_main.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>
#include <user_interface.h>
#include <driver/uart.h>
#include <espconn.h>          // espconn_tcp_set_max_con()
#include "mem.h"

#include "user_config.h"
#include "esp_missing.h"      // os_snprintf()

#include "user_button.h"      // buttonInit()
#include "user_sntp.h"        // user_snpt_init()
#include "user_mqtt.h"        // mqttInit()
#include "user_httpd.h"       // httpdInit()
#include "user_wifi.h"
#include "cgiTimer.h"
#include "cgiHistory.h"

#include "leds.h"
#include "configs.h"
#include "wifi_config.h"
#include "mqtt_config.h"
#include "device.h"           // getDev()
#include "device_control.h"
#include "rtc.h"              // rtc_init()
#include "io.h"               // io_pins_init()

// --------------------------------------------------------------------------
//

extern char __BUILD_UTIME;
extern char __BUILD_NUMBER;

const uint32_t build_number = ( uint32_t ) &__BUILD_NUMBER;
const time_t   build_utime  = ( time_t ) &__BUILD_UTIME;
time_t build_time;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct rst_info *rst_info_t;
rst_info_t sys_rst_info;
extern int rtc_need_restart;

os_timer_t timer_100ms;
uint32_t timer_100ms_counter = 0; // overflow after 4971h or 13,6a

union
{
   struct
   {
      bool power_state: 1;
      bool sys_led_state: 1;
      bool info_led_state: 1;
      bool system_ready: 1;
   };
   unsigned int val;
} system_flag;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR systemReadyCb( void );

void ICACHE_FLASH_ATTR systemPrintInfo();
const char* ICACHE_FLASH_ATTR reason2Str( int reason );
void ICACHE_FLASH_ATTR systemPrintResetInfo( rst_info_t sys_rst_info );
void ICACHE_FLASH_ATTR systemTimer100msTask( void *arg );
uint32_t ICACHE_FLASH_ATTR user_rf_cal_sector_set( void );
void ICACHE_FLASH_ATTR user_rf_pre_init( void );
void ICACHE_FLASH_ATTR user_init( void );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#ifdef MEMLEAK_DEBUG
bool ICACHE_FLASH_ATTR check_memleak_debug_enable(void)
{
   return MEMLEAK_DEBUG_ENABLE;
}
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR systemPrintInfo()
{
   os_printf( "\r\n\r\n" );
   os_printf( "-------------------------------------------\r\n" );
   os_printf( "BOOTUP...\r\n" );
   os_printf( "\r\n" );
   os_printf( "ESP8266 platform starting...\r\n" );
   os_printf( "==== System info: ====\r\n" );
   os_printf( "SDK version:    %s  rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check() );
   os_printf( "Project:        IoT-Wifi-Switch ( ESP8266 )\r\n" );
   os_printf( "Device:         " TARGET_DEVICE "\r\n" );
   os_printf( "Build time:     %s\r\n", _sntp_get_real_time( build_time ) );
   os_printf( "Build number:   %u\r\n", build_number );
   os_printf( "Time:           %d ms\r\n", system_get_time()/1000 );
   os_printf( "Chip id:        0x%x\r\n", system_get_chip_id() );
   os_printf( "CPU freq:       %d MHz\r\n", system_get_cpu_freq() );
   os_printf( "Flash size map: %d\r\n", system_get_flash_size_map() );
   os_printf( "Free heap size: %d\r\n", system_get_free_heap_size() );
   os_printf( "Memory info:\r\n" );
   system_print_meminfo();
#ifdef MEMLEAK_DEBUG
   os_printf( "Using MEMLEAK_DEBUG ...\r\n" );
#endif
   os_printf( " ==== End System info ====\r\n" );
   os_printf( " -------------------------------------------\r\n\r\n" );
}

// --------------------------------------------------------------------------
// print reset cause
// --------------------------------------------------------------------------

const char * ICACHE_FLASH_ATTR reason2Str( int reason )
{
   const char * rst_msg;

   switch( reason )
   {
      case REASON_DEFAULT_RST:        rst_msg = "DEFAULT_RST"     ; break;   // 0: normal startup by power on
      case REASON_WDT_RST:            rst_msg = "WDT_RST"         ; break;   // 1: hardware watch dog reset  exception reset, GPIO status won’t change
      case REASON_EXCEPTION_RST:      rst_msg = "EXCEPTION_RST"   ; break;   // 2: software watch dog reset, GPIO status won’t change
      case REASON_SOFT_WDT_RST:       rst_msg = "SOFT_WDT_RST"    ; break;   // 3: software restart,system_restart, GPIO status won’t change
      case REASON_SOFT_RESTART:       rst_msg = "SOFT_RESTART"    ; break;   // 4:
      case REASON_DEEP_SLEEP_AWAKE:   rst_msg = "DEEP_SLEEP_AWAKE"; break;   // 5: wake up from deep-sleep
      case REASON_EXT_SYS_RST:        rst_msg = "EXT_SYS_RST"     ; break;   // 6: external system reset
      default:                        rst_msg = "unknown";
   }

   return rst_msg;
}

void ICACHE_FLASH_ATTR systemPrintResetInfo( rst_info_t sys_rst_info )
{
   ESP_LOGI( TAG, "reset reason: %x: %s", sys_rst_info->reason, reason2Str( sys_rst_info->reason ) );

   if( sys_rst_info->reason == REASON_WDT_RST ||
         sys_rst_info->reason == REASON_EXCEPTION_RST ||
         sys_rst_info->reason == REASON_SOFT_WDT_RST )
   {
      if( sys_rst_info->reason == REASON_EXCEPTION_RST )
      {
         ESP_LOGI( TAG, "Fatal exception ( %d ):", sys_rst_info->exccause );
      }

      ESP_LOGI( TAG, "epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x",
            sys_rst_info->epc1, sys_rst_info->epc2, sys_rst_info->epc3,
            sys_rst_info->excvaddr, sys_rst_info->depc );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR systemTimer100msTask( void *arg )
{
   timer_100ms_counter++;

   leds_update();    // every 100ms

   bool cur_power_state = devGet( PowerSense );
   if( ( system_flag.power_state != cur_power_state ) || ( system_flag.system_ready == false ) )
   {
      if( system_flag.system_ready == false )
      {
         ESP_LOG( TAG, "System is ready" );
         system_flag.system_ready = true;
      }

      if( cur_power_state )
      {
         ESP_LOG( TAG, "Switch is on" );
         mqttPublish( &mqttClient, Relay, "1" );
         InfoLed_On();
      }
      else
      {
         ESP_LOG( TAG, "Switch is off" );
         mqttPublish( &mqttClient, Relay, "0" );
         led_set( INFO_LED, LED_BLINK, 1, 50, 0 );   // very slow flash
      }

      system_flag.power_state = cur_power_state;
   }
}

// --------------------------------------------------------------------------
// callback when the ESP8266 has initialized
// --------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR systemReadyCb( void )
{
   ESP_LOGD( TAG, "systemReadyCb" );
   HEAP_INFO( "" );

//   wifiInit();

   // setup to get time from ntp server
   sntpInit();

   // setup tcp/ip connection( s )
   httpdInit();

   // setup mqtt device
   mqttInit();

   // start wifi, after setup all tcp/ip tasks: sntp, httpd, mqtt
   wifiInit();

   HEAP_INFO( "" );
   os_printf( "\nsystem ready ..." );
   os_printf( "\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n" );
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/

uint32_t ICACHE_FLASH_ATTR user_rf_cal_sector_set( void )
{
   enum flash_size_map size_map = system_get_flash_size_map();
   uint32_t rf_cal_sec = 0;

   switch( size_map )
   {
      case FLASH_SIZE_4M_MAP_256_256:
         rf_cal_sec = 128 - 5;
         break;

      case FLASH_SIZE_8M_MAP_512_512:
         rf_cal_sec = 256 - 5;
         break;

      case FLASH_SIZE_16M_MAP_512_512:
      case FLASH_SIZE_16M_MAP_1024_1024:
         rf_cal_sec = 512 - 5;
         break;

      case FLASH_SIZE_32M_MAP_512_512:
      case FLASH_SIZE_32M_MAP_1024_1024:
         rf_cal_sec = 1024 - 5;
         break;

      case FLASH_SIZE_64M_MAP_1024_1024:
         rf_cal_sec = 2048 - 5;
         break;
      case FLASH_SIZE_128M_MAP_1024_1024:
         rf_cal_sec = 4096 - 5;
         break;
      default:
         rf_cal_sec = 0;
         break;
   }

   return rf_cal_sec;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include "esp_init_data_default_v08.h"

void ICACHE_FLASH_ATTR user_rf_pre_init( void )
{
   os_printf( "\r\n\r\n" );
   ESP_LOGI( TAG, "user_rf_pre_init ..." );

   uint8_t esp_init_data_current[ sizeof( esp_init_data_default ) ];
   int rf_cal_sec = user_rf_cal_sector_set();
   int addr = ( ( rf_cal_sec ) * SPI_FLASH_SEC_SIZE ) + SPI_FLASH_SEC_SIZE;

   spi_flash_read( addr, ( uint32_t * )esp_init_data_current, sizeof( esp_init_data_current ) );

   for( int i = 0; i < sizeof( esp_init_data_default ); i++ )
   {
      if( esp_init_data_current[i] != esp_init_data_default[i] )
      {
         spi_flash_erase_sector( rf_cal_sec );
         spi_flash_erase_sector( rf_cal_sec + 1 );
         spi_flash_erase_sector( rf_cal_sec + 2 );
         addr = ( ( rf_cal_sec ) * SPI_FLASH_SEC_SIZE ) + SPI_FLASH_SEC_SIZE;
         ESP_LOGI( TAG, "Storing rfcal init data @ address=0x%08X", addr );
         spi_flash_write( addr, ( uint32 * )esp_init_data_default, sizeof( esp_init_data_default ) );

         break;
      }
   }

   // Warning: IF YOU DON'T KNOW WHAT YOU ARE DOING, DON'T TOUCH THESE CODE

   // Control RF_CAL by esp_init_data_default.bin( 0~127byte ) 108 byte when wakeup
   // Will low current
   // system_phy_set_rfoption( 0 );

   // Process RF_CAL when wakeup.
   // Will high current
   // system_phy_set_rfoption( 1 );

   // RF initialization will do the whole RF calibration which will take about 200 ms;
   // this increases the current consumption.
   system_phy_set_powerup_option( 3 );

   // Set Wi-Fi Tx Power, Unit: 0.25dBm, Range: [0, 82]
   system_phy_set_max_tpw( 82 );
}

// --------------------------------------------------------------------------
// FunctionName : user_init
// Description  : entry of user application, init user function here
// Parameters   : none
// Returns      : none
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR user_init( void )
{
   // ----------------------------------
   // get and save reset reasons
   // ----------------------------------

   sys_rst_info = system_get_rst_info();
   rtc_need_restart = sys_rst_info->reason;
   rtc_init();

   system_update_cpu_freq( SYS_CPU_160MHZ );

   // ----------------------------------
   // set callback for system to be done.
   // ----------------------------------

   system_flag.val = 0;
   system_init_done_cb( systemReadyCb );

   // ----------------------------------
   // Setup serial port
   // ----------------------------------

   // Turn log printing on or off.
   system_set_os_print( 1 );

   uart_init( BIT_RATE_115200, BIT_RATE_115200 );  // configures UART0 and UART1, use GPIO2,
   os_delay_us( 1000 );

   // ----------------------------------
   // setup leds and button
   // ----------------------------------

   // init pins, must be after uart_init(), otherwise it don't work
   io_pins_init();            // setup pins for relays, leds and buttons
   leds_init();

   // ----------------------------------
   // print system info before wifi starts
   // ----------------------------------

   build_time = build_utime;
   // timezone correction, we get UTC time, but we are at UTC+1
   build_time += TIMEZONE_CORRECTION * 1;
   // summertime correction
   if( isSummer( build_time ) )
       build_time += TIMEZONE_CORRECTION;

   systemPrintInfo();
   systemPrintResetInfo( sys_rst_info );

   HEAP_INFO( "" );

   // ----------------------------------
   // setup configuration
   // ----------------------------------

   // config_print_defaults();
   user_config_print();
   static Configuration_List_t cfg_list[ 3 ];

   cfg_list[ 0 ] = init_wifi_config();
   cfg_list[ 1 ] = init_mqtt_config();
   cfg_list[ 2 ] = init_device_control();

   config_build_list( cfg_list, 3);
   // config_print_settings();
   HEAP_INFO( "" );

   // ----------------------------------
   // setup a repetive 100ms timer
   // ----------------------------------

   os_timer_setfn( &timer_100ms, ( os_timer_func_t * )systemTimer100msTask, NULL );
   os_timer_arm( &timer_100ms, 100, 1 );  // 100 ms

   led_set( SYS_LED, LED_BLINK,  2, 2, 3 );     // red led; 200ms on, 200ms off
   led_set( INFO_LED, LED_BLINK, 2, 2, 3 );     // green led, 200ms on, 200ms off
   ESP_LOG( TAG, "setup a repetive 100ms timer" );

   // ----------------------------------
   // setup device for wps
   // ----------------------------------

   buttonInit();  // activate button for on/off and wps

   // ----------------------------------
   // setup a switching timer
   // ----------------------------------

   switchingTimeInit();

   // ----------------------------------
   // system and application setup done
   // ----------------------------------

   history( "Reset\treason: %x: %s", sys_rst_info->reason, reason2Str( sys_rst_info->reason ) );
   HEAP_INFO( "" );
   os_printf( "\nready ..." );
   os_printf( "\n--------------------------------------------------------------------------\n" );
}
