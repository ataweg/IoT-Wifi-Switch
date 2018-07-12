// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          appl_events.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-06-23  AWe   make event handling for sntp more general
//
// --------------------------------------------------------------------------

#ifndef __EVENT_H__
#define __EVENT_H__

#include <time.h>

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef void ( *appl_event_handler_t )( uint32_t event, void *arg );
typedef void ( *appl_event_cb_t )( uint32_t event, void *arg, void *arg2 );

// mamage the list of event callbacks
typedef struct appl_event_item appl_event_item_t;
typedef struct appl_event_item
{
   uint32_t event;
   appl_event_cb_t cb;
   appl_event_item_t *prev;
   appl_event_item_t *next;
   void *arg;
} appl_event_item_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void   ICACHE_FLASH_ATTR appl_setEventHandler( appl_event_handler_t handler );
void   ICACHE_FLASH_ATTR appl_defaultEventHandler( uint32_t event, void *arg );
appl_event_item_t* ICACHE_FLASH_ATTR appl_addEventCb( uint32_t event, appl_event_cb_t cb, void *arg);
void   ICACHE_FLASH_ATTR appl_removeEventCb( appl_event_item_t *item);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif // __EVENT_H__
