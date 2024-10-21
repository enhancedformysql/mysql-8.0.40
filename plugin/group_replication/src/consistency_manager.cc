/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include <algorithm>

#include <mysql/components/services/log_builtins.h>
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/sync_before_execution_message.h"
#include "plugin/group_replication/include/plugin_psi.h"

Transaction_consistency_manager::Transaction_consistency_manager()
    : m_plugin_stopping(true),
      m_primary_election_active(false) {
}

Transaction_consistency_manager::~Transaction_consistency_manager() {
}

void Transaction_consistency_manager::clear() {
}

int Transaction_consistency_manager::after_commit(my_thread_id thread_id, rpl_sidno sidno,
    rpl_gno gno) {
  return 0;
}

int Transaction_consistency_manager::before_transaction_begin(
    my_thread_id thread_id, ulong gr_consistency_level, ulong timeout,
    enum_rpl_channel_type rpl_channel_type) {
  DBUG_TRACE;
  int error = 0;

  if (GR_RECOVERY_CHANNEL == rpl_channel_type ||
      GR_APPLIER_CHANNEL == rpl_channel_type) {
    return 0;
  }

  const enum_group_replication_consistency_level consistency_level =
      static_cast<enum_group_replication_consistency_level>(
          gr_consistency_level);

  // Transaction consistency can only be used on a ONLINE member.
  if (consistency_level >= GROUP_REPLICATION_CONSISTENCY_BEFORE &&
      local_member_info->get_recovery_status() !=
          Group_member_info::MEMBER_ONLINE) {
    return ER_GRP_TRX_CONSISTENCY_NOT_ALLOWED;
  }

  DBUG_PRINT("info", ("thread_id: %d; consistency_level: %d", thread_id,
                      consistency_level));

  if (GROUP_REPLICATION_CONSISTENCY_BEFORE == consistency_level) {
    error = transaction_begin_sync_before_execution(thread_id,
                                                    consistency_level, timeout);
    if (error) {
      /* purecov: begin inspected */
      return error;
      /* purecov: end */
    }
  }

  if (m_primary_election_active) {
    return m_hold_transactions.wait_until_primary_failover_complete(timeout);
  }

  return 0;
}

int Transaction_consistency_manager::transaction_begin_sync_before_execution(
    my_thread_id thread_id,
    enum_group_replication_consistency_level consistency_level [[maybe_unused]],
    ulong timeout) const {
  DBUG_TRACE;
  assert(GROUP_REPLICATION_CONSISTENCY_BEFORE == consistency_level);
  DBUG_PRINT("info", ("thread_id: %d; consistency_level: %d", thread_id,
                      consistency_level));

  if (m_plugin_stopping) {
    return ER_GRP_TRX_CONSISTENCY_BEGIN_NOT_ALLOWED;
  }

  if (transactions_latch->registerTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_REGISTER_TRX_TO_WAIT_FOR_SYNC_BEFORE_EXECUTION_FAILED,
        thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  // send message
  Sync_before_execution_message message(thread_id);
  if (gcs_module->send_message(message)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SEND_TRX_SYNC_BEFORE_EXECUTION_FAILED,
        thread_id);
    if (transactions_latch->unregisterTicket(thread_id)) {
      LogPluginErrMsg(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
          "Failed to unregister ticket for 'before' operation, thread ID:%d",
          (int)thread_id);
    }
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  DBUG_PRINT("info", ("waiting for Sync_before_execution_message"));

  if (transactions_latch->waitTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_TRX_WAIT_FOR_SYNC_BEFORE_EXECUTION_FAILED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  std::string applier_retrieved_gtids;
  Replication_thread_api applier_channel("group_replication_applier");
  if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GTID_SET_EXTRACT_ERROR);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
  }

  DBUG_PRINT("info", ("waiting for wait_for_gtid_set_committed()"));

  /*
    We want to keep the current thd stage info of
    "Executing hook on transaction begin.", thence we disable
    `update_thd_status`.
  */
  if (wait_for_gtid_set_committed(applier_retrieved_gtids.c_str(), timeout,
                                  false /* update_thd_status */)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRX_WAIT_FOR_GROUP_GTID_EXECUTED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  return 0;
}

int Transaction_consistency_manager::handle_sync_before_execution_message(
    my_thread_id thread_id, const Gcs_member_identifier &gcs_member_id) const {
  DBUG_TRACE;
  DBUG_PRINT("info", ("thread_id: %d; gcs_member_id: %s", thread_id,
                      gcs_member_id.get_member_id().c_str()));
  if (local_member_info->get_gcs_member_id() == gcs_member_id &&
      transactions_latch->releaseTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_RELEASE_BEGIN_TRX_AFTER_WAIT_FOR_SYNC_BEFORE_EXEC,
                 thread_id);
    return 1;
    /* purecov: end */
  }

  return 0;
}

void Transaction_consistency_manager::plugin_started() {
  m_plugin_stopping = false;
}

void Transaction_consistency_manager::plugin_is_stopping() {
  m_plugin_stopping = true;
}

void Transaction_consistency_manager::register_transaction_observer() {
  group_transaction_observation_manager->register_transaction_observer(this);
}

void Transaction_consistency_manager::unregister_transaction_observer() {
  group_transaction_observation_manager->unregister_transaction_observer(this);
}

void Transaction_consistency_manager::enable_primary_election_checks() {
  m_hold_transactions.enable();
  m_primary_election_active = true;
}

void Transaction_consistency_manager::disable_primary_election_checks() {
  m_primary_election_active = false;
  m_hold_transactions.disable();
}
/*
  These methods are necessary to fulfil the Group_transaction_listener
  interface.
*/
/* purecov: begin inspected */
int Transaction_consistency_manager::before_commit(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}

int Transaction_consistency_manager::before_rollback(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}

int Transaction_consistency_manager::after_rollback(my_thread_id) { return 0; }
/* purecov: end */
