/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/xcom_interface.h"

#include <assert.h>
#include <stdlib.h>

#include "xcom/app_data.h"
#include "xcom/bitset.h"
#include "xcom/node_list.h"
#include "xcom/node_no.h"
#include "xcom/node_set.h"
#include "xcom/pax_msg.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/site_struct.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_cache.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xcom/xcom_vp_str.h"
#include "xdr_gen/xcom_vp.h"

static xcom_data_receiver xcom_receive_data;
static xcom_local_view_receiver xcom_receive_local_view;
static xcom_global_view_receiver xcom_receive_global_view;

void set_xcom_data_receiver(xcom_data_receiver x) { xcom_receive_data = x; }

void set_xcom_local_view_receiver(xcom_local_view_receiver x) {
  xcom_receive_local_view = x;
}

void set_xcom_global_view_receiver(xcom_global_view_receiver x) {
  xcom_receive_global_view = x;
}

xcom_logger xcom_log = nullptr;
xcom_debugger xcom_debug = nullptr;
xcom_debugger_check xcom_debug_check = nullptr;

void set_xcom_logger(xcom_logger x) { xcom_log = x; }

void set_xcom_debugger(xcom_debugger x) { xcom_debug = x; }

void set_xcom_debugger_check(xcom_debugger_check x) { xcom_debug_check = x; }

/* Deliver message to application */

void deliver_to_app(pax_machine *pma, app_data_ptr app,
                    delivery_status app_status) {
  site_def const *site = nullptr;
  int doit = (xcom_receive_data != nullptr && app_status == delivery_ok);

  if (app_status == delivery_ok && !pma) {
    g_critical(
        "A message is to be delivered but it does not have an associated "
        "PAXOS State Machine. XCom cannot proceed delivering this message "
        "without compromising correctness. This message will be skipped. No "
        "need to take any further action. If this behaviour persists, "
        "consider restarting the group at the next convenient time");

    return;
  }

  if (!doit) return;

  if (pma)
    site = find_site_def(pma->synode);
  else
    site = get_site_def();

  while (app) {
    if (app->body.c_t == app_type) { /* Decode application data */
      if (doit) {
        u_int copy_len = 0;
        char *copy = (char *)xcom_malloc(app->body.app_u_u.data.data_len);
        if (copy == nullptr) {
          /* purecov: begin inspected */
          G_ERROR("Unable to allocate memory for the received message.");
          /* purecov: end */
        } else {
          memcpy(copy, app->body.app_u_u.data.data_val,
              app->body.app_u_u.data.data_len);
          copy_len = app->body.app_u_u.data.data_len;
        }
        xcom_receive_data(pma->synode, detector_node_set(site), copy_len,
            cache_get_last_removed(), copy);
      } else {
        /* purecov: begin deadcode */
        G_TRACE("Data message was not delivered.");
        /* purecov: end */
      }
    }
    app = app->next;
  }
}

/**
   Deliver a view message
*/

void deliver_view_msg(site_def const *site) {
  if (site) {
    if (xcom_receive_local_view) {
      xcom_receive_local_view(site->start, detector_node_set(site));
    }
  }
}

void deliver_global_view_msg(site_def const *site, synode_no message_id) {
  if (site) {
    if (xcom_receive_global_view) {
      xcom_receive_global_view(site->start, message_id,
          clone_node_set(site->global_node_set),
          site->event_horizon);
    }
  }
}

