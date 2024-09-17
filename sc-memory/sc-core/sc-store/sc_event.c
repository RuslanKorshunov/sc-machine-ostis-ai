/*
 * This source file is part of an OSTIS project. For the latest info, see http://ostis.net
 * Distributed under the MIT License
 * (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#include "sc_event.h"

#include "sc_storage.h"
#include "sc-event/sc_event_private.h"
#include "sc-event/sc_event_queue.h"

#include "../sc_memory_context_manager.h"

#include "sc-base/sc_allocator.h"
#include "sc-base/sc_mutex.h"

struct _sc_event_registration_manager
{
  sc_hash_table * events_table;
  sc_monitor events_table_monitor;
};

#define TABLE_KEY(__Addr) GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(__Addr))

// Pointer to hash table that contains events

guint events_table_hash_func(gconstpointer pointer)
{
  return GPOINTER_TO_UINT(pointer);
}

gboolean events_table_equal_func(gconstpointer a, gconstpointer b)
{
  return (a == b);
}

//! Inserts specified event into events table
sc_result _sc_event_registration_manager_add(sc_event_registration_manager * manager, sc_event * event)
{
  sc_hash_table_list * element_events_list = null_ptr;

  // the first, if table doesn't exist, then return error
  if (manager == null_ptr)
    return SC_RESULT_NO;

  sc_monitor_acquire_write(&manager->events_table_monitor);

  if (manager->events_table == null_ptr)
  {
    sc_monitor_release_write(&manager->events_table_monitor);
    return SC_RESULT_NO;
  }

  // if there are no events for specified sc-element, then create new events list
  element_events_list = (sc_hash_table_list *)sc_hash_table_get(manager->events_table, TABLE_KEY(event->element));
  element_events_list = sc_hash_table_list_append(element_events_list, (sc_pointer)event);
  sc_hash_table_insert(manager->events_table, TABLE_KEY(event->element), (sc_pointer)element_events_list);

  sc_monitor_release_write(&manager->events_table_monitor);

  return SC_RESULT_OK;
}

//! Remove specified sc-event from events table
sc_result _sc_event_registration_manager_remove(sc_event_registration_manager * manager, sc_event * event)
{
  sc_hash_table_list * element_events_list = null_ptr;

  // the first, if table doesn't exist, then return error
  if (manager == null_ptr)
    return SC_RESULT_NO;

  sc_monitor_acquire_write(&manager->events_table_monitor);
  element_events_list = (sc_hash_table_list *)sc_hash_table_get(manager->events_table, TABLE_KEY(event->element));
  if (element_events_list == null_ptr)
    goto error;

  if (manager->events_table == null_ptr)
    goto error;

  // remove event from list of events for specified sc-element
  element_events_list = sc_hash_table_list_remove(element_events_list, (sc_const_pointer)event);
  if (element_events_list == null_ptr)
    sc_hash_table_remove(manager->events_table, TABLE_KEY(event->element));
  else
    sc_hash_table_insert(manager->events_table, TABLE_KEY(event->element), (sc_pointer)element_events_list);

  sc_monitor_release_write(&manager->events_table_monitor);
  return SC_RESULT_OK;
error:
  sc_monitor_release_write(&manager->events_table_monitor);
  return SC_RESULT_ERROR_INVALID_PARAMS;
}

void sc_event_registration_manager_initialize(sc_event_registration_manager ** manager)
{
  (*manager) = sc_mem_new(sc_event_registration_manager, 1);
  (*manager)->events_table = sc_hash_table_init(events_table_hash_func, events_table_equal_func, null_ptr, null_ptr);
  sc_monitor_init(&(*manager)->events_table_monitor);
}

void sc_event_registration_manager_shutdown(sc_event_registration_manager * manager)
{
  sc_monitor_destroy(&manager->events_table_monitor);
  sc_hash_table_destroy(manager->events_table);
  sc_mem_free(manager);
}

sc_event * sc_event_new(
    sc_memory_context const * ctx,
    sc_addr el,
    sc_event_type type,
    sc_pointer data,
    fEventCallback callback,
    fDeleteCallback delete_callback)
{
  if (SC_ADDR_IS_EMPTY(el))
    return null_ptr;

  sc_event * event = null_ptr;

  event = sc_mem_new(sc_event, 1);
  event->element = el;
  event->type = type;
  event->callback = callback;
  event->delete_callback = delete_callback;
  event->data = data;
  event->ref_count = 1;
  event->access_levels = 0;
  sc_monitor_init(&event->monitor);

  // register created event
  sc_event_registration_manager * manager = sc_storage_get_event_registration_manager();
  _sc_event_registration_manager_add(manager, event);

  return event;
}

sc_event * sc_event_new_ex(
    sc_memory_context const * ctx,
    sc_addr el,
    sc_event_type type,
    sc_pointer data,
    fEventCallbackEx callback,
    fDeleteCallback delete_callback)
{
  if (SC_ADDR_IS_EMPTY(el))
    return null_ptr;

  sc_event * event = null_ptr;

  event = sc_mem_new(sc_event, 1);
  event->element = el;
  event->type = type;
  event->callback_ex = callback;
  event->delete_callback = delete_callback;
  event->data = data;
  event->ref_count = 1;
  event->access_levels = 0;
  sc_monitor_init(&event->monitor);

  // register created event
  sc_event_registration_manager * manager = sc_storage_get_event_registration_manager();
  _sc_event_registration_manager_add(manager, event);

  return event;
}

sc_result sc_event_destroy(sc_event * event)
{
  sc_event_registration_manager * registration_manager = sc_storage_get_event_registration_manager();
  sc_event_emission_manager * emission_manager = sc_storage_get_event_emission_manager();

  sc_monitor_acquire_write(&event->monitor);
  if (_sc_event_registration_manager_remove(registration_manager, event) != SC_RESULT_OK)
  {
    sc_monitor_release_write(&event->monitor);
    return SC_RESULT_ERROR;
  }

  if (event->delete_callback != null_ptr)
    event->delete_callback(event);

  event->ref_count = SC_EVENT_REQUEST_DESTROY;
  event->element = SC_ADDR_EMPTY;
  event->type = 0;
  event->callback_ex = null_ptr;
  event->delete_callback = null_ptr;
  event->data = null_ptr;
  event->access_levels = 0;

  sc_storage * storage = sc_storage_get();
  if (storage != null_ptr)
  {
    sc_monitor_acquire_write(&emission_manager->pool_monitor);
    sc_queue_push(emission_manager->deletable_events, event);
    sc_monitor_release_write(&emission_manager->pool_monitor);
  }
  sc_monitor_release_write(&event->monitor);

  return SC_RESULT_OK;
}

sc_result sc_event_notify_element_deleted(sc_addr element)
{
  sc_hash_table_list * element_events_list = null_ptr;
  sc_event * event = null_ptr;

  sc_event_registration_manager * registration_manager = sc_storage_get_event_registration_manager();
  sc_event_emission_manager * emission_manager = sc_storage_get_event_emission_manager();

  // do nothing, if there are no registered events
  if (registration_manager == null_ptr || registration_manager->events_table == null_ptr)
    goto result;

  // sc_set_lookup for all registered to specified sc-element events
  if (registration_manager != null_ptr)
  {
    sc_monitor_acquire_read(&registration_manager->events_table_monitor);
    element_events_list =
        (sc_hash_table_list *)sc_hash_table_get(registration_manager->events_table, TABLE_KEY(element));
    sc_monitor_release_read(&registration_manager->events_table_monitor);
  }

  if (element_events_list != null_ptr)
  {
    sc_monitor_acquire_write(&registration_manager->events_table_monitor);
    sc_hash_table_remove(registration_manager->events_table, TABLE_KEY(element));
    sc_monitor_release_write(&registration_manager->events_table_monitor);

    while (element_events_list != null_ptr)
    {
      event = (sc_event *)element_events_list->data;

      // mark event for deletion
      sc_monitor_acquire_write(&event->monitor);

      sc_monitor_acquire_write(&emission_manager->pool_monitor);
      sc_queue_push(emission_manager->deletable_events, event);
      sc_monitor_release_write(&emission_manager->pool_monitor);

      sc_monitor_release_write(&event->monitor);

      element_events_list = sc_hash_table_list_remove_sublist(element_events_list, element_events_list);
    }
    sc_hash_table_list_destroy(element_events_list);
  }

result:
  return SC_RESULT_OK;
}

sc_result sc_event_emit(
    sc_memory_context const * ctx,
    sc_addr el,
    sc_access_levels el_access,
    sc_event_type type,
    sc_addr edge,
    sc_addr other_el)
{
  if (_sc_memory_context_is_pending(ctx))
  {
    _sc_memory_context_pend_event(ctx, type, el, edge, other_el);
    return SC_RESULT_OK;
  }

  return sc_event_emit_impl(ctx, el, el_access, type, edge, other_el);
}

sc_result sc_event_emit_impl(
    sc_memory_context const * ctx,
    sc_addr el,
    sc_access_levels el_access,
    sc_event_type type,
    sc_addr edge,
    sc_addr other_el)
{
  sc_hash_table_list * element_events_list = null_ptr;
  sc_event * event = null_ptr;

  if (SC_ADDR_IS_EMPTY(el))
    return SC_RESULT_ERROR_ADDR_IS_NOT_VALID;

  sc_event_registration_manager * manager = sc_storage_get_event_registration_manager();
  sc_event_emission_manager * events_queue = sc_storage_get_event_emission_manager();

  // if table is empty, then do nothing
  if (manager == null_ptr || manager->events_table == null_ptr)
    goto result;

  // sc_set_lookup for all registered to specified sc-element events
  sc_monitor_acquire_read(&manager->events_table_monitor);
  if (manager != null_ptr)
    element_events_list = (sc_hash_table_list *)sc_hash_table_get(manager->events_table, TABLE_KEY(el));
  sc_monitor_release_read(&manager->events_table_monitor);

  while (element_events_list != null_ptr)
  {
    event = (sc_event *)element_events_list->data;

    if (event->type == type)
      _sc_event_emission_manager_add(events_queue, event, edge, other_el);

    element_events_list = element_events_list->next;
  }

result:
  return SC_RESULT_OK;
}

sc_bool sc_event_is_deletable(sc_event const * event)
{
  return event->ref_count == SC_EVENT_REQUEST_DESTROY;
}

sc_pointer sc_event_get_data(sc_event const * event)
{
  return event->data;
}

sc_addr sc_event_get_element(sc_event const * event)
{
  return event->element;
}
