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

#include <assert.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom/app_data.h"
#include "xcom/checked_data.h"
#include "xcom/node_list.h"
#include "xcom/node_set.h"
#include "xcom/simset.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_vp_str.h"
#include "xcom/xdr_utils.h"
#include "xdr_gen/xcom_vp.h"

static app_data_list nextp(app_data_list l);

app_data_ptr clone_app_data(app_data_ptr a) {
  app_data_ptr retval = nullptr;
  app_data_list p = &retval; /* Initialize p with empty list */

  while (nullptr != a) {
    app_data_ptr clone = clone_app_data_single(a);
    follow(p, clone);
    a = a->next;
    p = nextp(p);
    if (clone == nullptr && retval != nullptr) {
      XCOM_XDR_FREE(xdr_app_data, retval);
      break;
    }
  }
  return retval;
}

/**
   Clone an app_data struct.
 */
app_data_ptr clone_app_data_single(app_data_ptr a) {
  char *str = nullptr;
  app_data_ptr p = nullptr;

  if (nullptr != a) {
    bool_t copied = FALSE;

    p = new_app_data();
    p->unique_id = a->unique_id;
    p->lsn = a->lsn;
    p->app_key = a->app_key;
    p->consensus = a->consensus;
    p->body.c_t = a->body.c_t;
    p->group_id = a->group_id;
    p->chosen = a->chosen;
    p->recover = a->recover;
    switch (a->body.c_t) {
      case xcom_set_group:
      case unified_boot_type:
      case add_node_type:
      case remove_node_type:
      case force_config_type:
      case xcom_boot_type: {
        p->body.app_u_u.nodes = clone_node_list(a->body.app_u_u.nodes);
      } break;
      case app_type:
        copied =
            copy_checked_data(&p->body.app_u_u.data, &a->body.app_u_u.data);
        if (!copied) {
          G_ERROR("Memory allocation failed.");
          free(p);
          return nullptr;
        }
        break;
#ifdef XCOM_TRANSACTIONS
      case begin_trans:
        break;
      case prepared_trans:
        p->body.app_u_u.tid = a->body.app_u_u.tid;
        break;
      case abort_trans:
        p->body.app_u_u.tid = a->body.app_u_u.tid;
        break;
#endif
      case view_msg:
        p->body.app_u_u.present = clone_node_set(a->body.app_u_u.present);
        break;
      case exit_type: /* purecov: deadcode */
      case enable_arbitrator:
      case disable_arbitrator:
      case x_terminate_and_exit:
        break;
      case get_event_horizon_type:
        break;
      case set_event_horizon_type:
        p->body.app_u_u.event_horizon = a->body.app_u_u.event_horizon;
        break;
      default: /* Should not happen */
        str = dbg_app_data(a);
        G_ERROR("%s", str);
        free(str);
        assert(("No such xcom type" && FALSE));
    }
    assert(p->next == nullptr);
  }
  return p;
}

size_t node_set_size(node_set ns) {
  return ns.node_set_len * sizeof(*ns.node_set_val);
}

size_t synode_no_array_size(synode_no_array sa) {
  return sa.synode_no_array_len * sizeof(*sa.synode_no_array_val);
}

/**
   Return size of an app_data.
 */
size_t app_data_size(app_data const *a) {
  size_t size = sizeof(*a);
  if (a == nullptr) return 0;
  switch (a->body.c_t) {
    case xcom_set_group:
    case unified_boot_type:
    case add_node_type:
    case remove_node_type:
    case force_config_type:
    case app_type:
      size += a->body.app_u_u.data.data_len;
      break;
#ifdef XCOM_TRANSACTIONS
    case begin_trans:
      break;
    case prepared_trans:
      break;
    case abort_trans:
      break;
#endif
    case view_msg:
      size += node_set_size(a->body.app_u_u.present);
      break;
    case exit_type:
    case enable_arbitrator:
    case disable_arbitrator:
    case x_terminate_and_exit:
    case get_event_horizon_type:
    case set_event_horizon_type:
      break;
    default: /* Should not happen */
      G_ERROR("No such cargo type:%d", a->body.c_t);
  }
  return size;
}

/* app_data structs may be linked. This function returns the size of the whole
 * list */
size_t app_data_list_size(app_data const *a) {
  size_t size = 0;
  while (a) {
    size += app_data_size(a);
    a = a->next;
  }
  return (size);
}

/**
   Return next element in list of app_data.
 */
static app_data_list nextp(app_data_list l) { return (*l) ? &((*l)->next) : l; }

/**
   Constructor for app_data
 */
app_data_ptr new_app_data() {
  app_data_ptr retval = (app_data_ptr)calloc((size_t)1, sizeof(app_data));
  return retval;
}

app_data_ptr init_app_data(app_data_ptr retval) {
  memset(retval, 0, sizeof(app_data));
  return retval;
}

/* Debug list of app_data */
char *dbg_app_data(app_data_ptr a) {
  if (msg_count(a) > 100) {
    G_WARNING("Abnormally long message list %lu", msg_count(a));
  }
  {
    GET_NEW_GOUT;
    STRLIT("app_data ");
    PTREXP(a);
    while (nullptr != a) {
      a = a->next;
    }
    RET_GOUT;
  }
}

/* Replace target with copy of source list */

void _replace_app_data_list(app_data_list target, app_data_ptr source) {
  XCOM_XDR_FREE(xdr_app_data, *target); /* Will remove the whole list */
  *target = clone_app_data(source);
}

/**
   Insert p after l.
 */
void follow(app_data_list l, app_data_ptr p) {
  if (p) {
    assert(p->next == nullptr);
    p->next = *l;
  }
  *l = p;
  assert(!p || p->next != p);
}

/**
   Count the number of messages in a list.
 */
unsigned long msg_count(app_data_ptr a) {
  unsigned long n = 0;
  while (a) {
    n++;
    a = a->next;
  }
  return n;
}
