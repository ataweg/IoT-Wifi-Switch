// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          events.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-23  AWe   make event handling for sntp more general
//
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "modules/events.c";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <osapi.h>               // printf()
#include <user_interface.h>      // system_get_time()
#include <mem.h>                 // malloc

#include "os_platform.h"         // malloc(), ...
#include "events.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static  appl_event_item_t* appl_event_list = NULL;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR appl_defaultEventHandler( uint32_t event, void *arg )
{
   appl_event_item_t* item = appl_event_list;

   while( item != NULL)
   {
      if( item->event == event )
      {
         ESP_LOGD( TAG, "call function for %d at %d", event, system_get_time() );
         item->cb( event, arg, item->arg );
      }

      item = item->next;
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

appl_event_item_t* ICACHE_FLASH_ATTR appl_addEventCb( uint32_t event, appl_event_cb_t cb, void *arg)
{
   // ESP_LOGD( TAG, "appl_addEventCb for %d func: 0x%08x", event,cb );

   appl_event_item_t * new_item = ( appl_event_item_t * )malloc( sizeof( appl_event_item_t ) );

   if( new_item != NULL )
   {
      new_item->event = event;
      new_item->cb = cb;
      new_item->arg = arg;
      new_item->prev = NULL;
      new_item->next = appl_event_list;

      if( appl_event_list != NULL )
      {
         appl_event_list->prev = new_item;
      }

      appl_event_list = new_item;
   }

   // ESP_LOGD( TAG, "appl_addEventCb item: 0x%08x prev: 0x%08x next: 0x%08x cb: 0x%08x", new_item, new_item->prev, new_item->next, new_item->cb );
   return new_item;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR appl_removeEventCb( appl_event_item_t *item)
{
   // ESP_LOGD( TAG, "appl_removeEventCb item: 0x%08x prev: 0x%08x next: 0x%08x cb: 0x%08x", item, item->prev, item->next, item->cb );

   appl_event_item_t * prev = item->prev;
   appl_event_item_t * next = item->next;

   if( prev != NULL )
      prev->next = next;
   if( next != NULL )
      next->prev = prev;

   if( item == appl_event_list )
      appl_event_list = next;

   free( item );
}

