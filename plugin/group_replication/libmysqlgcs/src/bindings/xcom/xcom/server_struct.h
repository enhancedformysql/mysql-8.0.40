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

#ifndef SERVER_STRUCT_H
#define SERVER_STRUCT_H

#include "xcom/task.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_limits.h"

#define IP_MAX_SIZE 512

struct srv_buf {
  u_int start;
  u_int n;
  char buf[0x10000];
};
typedef struct srv_buf srv_buf;

struct server_timewait_info {
  double last_removed_time;
  char ip[IP_MAX_SIZE];
  int timeout;
  xcom_port port;
  unsigned int occupied : 1;
};
typedef struct server_timewait_info server_timewait_info;

/* Server definition */
struct server {
  int garbage;
  int refcnt;
  int invalid;
  int number_of_pings_received; /* Number of pings received from this server */
  int unreachable;
  unsigned int fast_skip_allowed_for_kill : 1;
  xcom_port port;             /* Port */
  char *srv;                  /* Server name */
  connection_descriptor *con; /* Descriptor for open connection */
  double active;              /* Last activity */
  double detected;            /* Last incoming */
  double conn_rtt;
  double large_transfer_detected; /* Process large transfer */
  double last_ping_received;      /* Last received ping timestamp */
  channel outgoing;           /* Outbound messages */
  task_env *sender;           /* The sender task */
  task_env *reply_handler;    /* The reply task */
  srv_buf out_buf;

#if defined(_WIN32)
  bool reconnect; /*States if the server should be reconnected*/
#endif
};

typedef struct server server;

#endif
