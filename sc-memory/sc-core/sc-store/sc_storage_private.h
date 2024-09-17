/*
 * This source file is part of an OSTIS project. For the latest info, see http://ostis.net
 * Distributed under the MIT License
 * (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#ifndef _sc_storage_private_h_
#define _sc_storage_private_h_

struct _sc_storage
{
  sc_segment ** segments;
  sc_addr_seg segments_count;
  sc_addr_seg max_segments_count;
  sc_addr_seg last_not_engaged_segment_num;
  sc_addr_seg last_released_segment_num;
  sc_monitor segments_monitor;
  sc_monitor_table addr_monitors_table;
  sc_hash_table * processes_segments_table;
  sc_monitor processes_monitor;
  sc_event_emission_manager * events_emission_manager;
  sc_event_registration_manager * events_registration_manager;
};

#endif
