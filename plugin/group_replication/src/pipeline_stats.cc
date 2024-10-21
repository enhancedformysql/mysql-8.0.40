/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/pipeline_stats.h"

#include <time.h>

#include <mysql/components/services/log_builtins.h>
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_systime.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_server_include.h"

Pipeline_stats_member_message::Pipeline_stats_member_message(
    int32 transactions_waiting_apply, int64 transactions_applied,
    int64 transactions_local, bool transaction_gtids,
    const std::string &transactions_all_committed)
    : Plugin_gcs_message(CT_PIPELINE_STATS_MEMBER_MESSAGE),
      m_transactions_waiting_apply(transactions_waiting_apply),
      m_transactions_applied(transactions_applied),
      m_transactions_local(transactions_local),
      m_transaction_gtids_present(transaction_gtids),
      m_transactions_committed_all_members(transactions_all_committed) {}

Pipeline_stats_member_message::Pipeline_stats_member_message(
    const unsigned char *buf, size_t len)
    : Plugin_gcs_message(CT_PIPELINE_STATS_MEMBER_MESSAGE),
      m_transactions_waiting_apply(0),
      m_transactions_applied(0),
      m_transactions_local(0),
      m_transaction_gtids_present(false),
      m_transactions_committed_all_members("") {
  decode(buf, len);
}

Pipeline_stats_member_message::~Pipeline_stats_member_message() = default;

int32 Pipeline_stats_member_message::get_transactions_waiting_apply() {
  DBUG_TRACE;
  return m_transactions_waiting_apply;
}

int64 Pipeline_stats_member_message::get_transactions_applied() {
  DBUG_TRACE;
  return m_transactions_applied;
}

int64 Pipeline_stats_member_message::get_transactions_local() {
  DBUG_TRACE;
  return m_transactions_local;
}

bool Pipeline_stats_member_message::get_transation_gtids_present() const {
  return m_transaction_gtids_present;
}

const std::string &
Pipeline_stats_member_message::get_transaction_committed_all_members() {
  DBUG_TRACE;
  return m_transactions_committed_all_members;
}

void Pipeline_stats_member_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  uint32 transactions_waiting_apply_aux = (uint32)m_transactions_waiting_apply;
  encode_payload_item_int4(buffer, PIT_TRANSACTIONS_WAITING_APPLY,
                           transactions_waiting_apply_aux);

  uint64 transactions_applied_aux = (uint64)m_transactions_applied;
  encode_payload_item_int8(buffer, PIT_TRANSACTIONS_APPLIED,
                           transactions_applied_aux);

  uint64 transactions_local_aux = (uint64)m_transactions_local;
  encode_payload_item_int8(buffer, PIT_TRANSACTIONS_LOCAL,
                           transactions_local_aux);

  encode_payload_item_string(buffer, PIT_TRANSACTIONS_COMMITTED_ALL_MEMBERS,
                             m_transactions_committed_all_members.c_str(),
                             m_transactions_committed_all_members.length());

  char aux_transaction_gtids_present = m_transaction_gtids_present ? '1' : '0';
  encode_payload_item_char(buffer, PIT_TRANSACTION_GTIDS_PRESENT,
                           aux_transaction_gtids_present);
}

void Pipeline_stats_member_message::decode_payload(const unsigned char *buffer,
                                                   const unsigned char *end) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  uint32 transactions_waiting_apply_aux = 0;
  decode_payload_item_int4(&slider, &payload_item_type,
                           &transactions_waiting_apply_aux);
  m_transactions_waiting_apply = (int32)transactions_waiting_apply_aux;

  uint64 transactions_applied_aux = 0;
  decode_payload_item_int8(&slider, &payload_item_type,
                           &transactions_applied_aux);
  m_transactions_applied = (int64)transactions_applied_aux;

  uint64 transactions_local_aux = 0;
  decode_payload_item_int8(&slider, &payload_item_type,
                           &transactions_local_aux);
  m_transactions_local = (int64)transactions_local_aux;

  while (slider + Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE <= end) {
    // Read payload item header to find payload item length.
    decode_payload_item_type_and_length(&slider, &payload_item_type,
                                        &payload_item_length);

    switch (payload_item_type) {
      case PIT_TRANSACTIONS_COMMITTED_ALL_MEMBERS:
        if (slider + payload_item_length <= end) {
          m_transactions_committed_all_members.assign(
              slider, slider + payload_item_length);
          slider += payload_item_length;
        }
        break;

      case PIT_TRANSACTION_GTIDS_PRESENT:
        if (slider + payload_item_length <= end) {
          unsigned char aux_transaction_gtids_present = *slider;
          slider += payload_item_length;
          m_transaction_gtids_present =
              (aux_transaction_gtids_present == '1') ? true : false;
        }
        break;
    }
  }
}

Pipeline_stats_member_collector::Pipeline_stats_member_collector()
    : m_transactions_waiting_apply(0),
      m_transactions_applied(0),
      m_transactions_local(0),
      m_transactions_applied_during_recovery(0),
      m_previous_transactions_applied_during_recovery(0),
      m_delta_transactions_applied_during_recovery(0),
      m_transactions_delivered_during_recovery(0),
      send_transaction_identifiers(false) {
  mysql_mutex_init(key_GR_LOCK_pipeline_stats_transactions_waiting_apply,
                   &m_transactions_waiting_apply_lock, MY_MUTEX_INIT_FAST);
}

Pipeline_stats_member_collector::~Pipeline_stats_member_collector() {
  mysql_mutex_destroy(&m_transactions_waiting_apply_lock);
}

void Pipeline_stats_member_collector::clear_transactions_waiting_apply() {
  mysql_mutex_lock(&m_transactions_waiting_apply_lock);
  m_transactions_waiting_apply = 0;
  mysql_mutex_unlock(&m_transactions_waiting_apply_lock);
}

void Pipeline_stats_member_collector::increment_transactions_waiting_apply() {
  mysql_mutex_lock(&m_transactions_waiting_apply_lock);
  assert(m_transactions_waiting_apply.load() >= 0);
  ++m_transactions_waiting_apply;
  mysql_mutex_unlock(&m_transactions_waiting_apply_lock);
}

void Pipeline_stats_member_collector::decrement_transactions_waiting_apply() {
  mysql_mutex_lock(&m_transactions_waiting_apply_lock);
  if (m_transactions_waiting_apply.load() > 0) --m_transactions_waiting_apply;
  mysql_mutex_unlock(&m_transactions_waiting_apply_lock);
}

void Pipeline_stats_member_collector::increment_transactions_applied() {
  ++m_transactions_applied;
}

void Pipeline_stats_member_collector::increment_transactions_local() {
  ++m_transactions_local;
}

int32 Pipeline_stats_member_collector::get_transactions_waiting_apply() {
  return m_transactions_waiting_apply.load();
}

int64 Pipeline_stats_member_collector::get_transactions_applied() {
  return m_transactions_applied.load();
}

int64 Pipeline_stats_member_collector::get_transactions_local() {
  return m_transactions_local.load();
}

void Pipeline_stats_member_collector::set_send_transaction_identifiers() {
  send_transaction_identifiers = true;
}

void Pipeline_stats_member_collector::
    increment_transactions_applied_during_recovery() {
  ++m_transactions_applied_during_recovery;
}

void Pipeline_stats_member_collector::
    compute_transactions_deltas_during_recovery() {
  m_delta_transactions_applied_during_recovery.store(
      m_transactions_applied_during_recovery.load() -
      m_previous_transactions_applied_during_recovery);
  m_previous_transactions_applied_during_recovery =
      m_transactions_applied_during_recovery.load();
}

uint64 Pipeline_stats_member_collector::
    get_delta_transactions_applied_during_recovery() {
  return m_delta_transactions_applied_during_recovery.load();
}

uint64 Pipeline_stats_member_collector::
    get_transactions_waiting_apply_during_recovery() {
  uint64 transactions_delivered_during_recovery =
      m_transactions_delivered_during_recovery.load();
  uint64 transactions_applied_during_recovery =
      m_transactions_applied_during_recovery.load();

  /* view change transactions were applied */
  if (transactions_applied_during_recovery >
      transactions_delivered_during_recovery) {
    return 0;
  }

  return transactions_delivered_during_recovery -
         transactions_applied_during_recovery;
}

void Pipeline_stats_member_collector::
    increment_transactions_delivered_during_recovery() {
  ++m_transactions_delivered_during_recovery;
}

void Pipeline_stats_member_collector::send_stats_member_message() {
  if (local_member_info == nullptr) return; /* purecov: inspected */
  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status != Group_member_info::MEMBER_ONLINE &&
      member_status != Group_member_info::MEMBER_IN_RECOVERY)
    return;

  std::string committed_transactions;

  Certifier_interface *cert_interface =
      (applier_module && applier_module->get_certification_handler())
          ? applier_module->get_certification_handler()->get_certifier()
          : nullptr;

  if (send_transaction_identifiers && cert_interface != nullptr) {
    char *committed_transactions_buf = nullptr;
    size_t committed_transactions_buf_length = 0;
    int get_group_stable_transactions_set_string_outcome =
        cert_interface->get_group_stable_transactions_set_string(
            &committed_transactions_buf, &committed_transactions_buf_length);
    if (!get_group_stable_transactions_set_string_outcome &&
        committed_transactions_buf_length > 0) {
      committed_transactions.assign(committed_transactions_buf);
    }
    my_free(committed_transactions_buf);
  }

  Pipeline_stats_member_message message(
      m_transactions_waiting_apply.load(), 
      m_transactions_applied.load(), m_transactions_local.load(),
      send_transaction_identifiers, committed_transactions);

  enum_gcs_error msg_error = gcs_module->send_message(message, true);
  if (msg_error != GCS_OK) {
    LogPluginErr(INFORMATION_LEVEL,
                 ER_GRP_RPL_SEND_STATS_ERROR); /* purecov: inspected */
  }
  send_transaction_identifiers = false;
}

Pipeline_member_stats::Pipeline_member_stats()
    : m_transactions_waiting_apply(0),
      m_transactions_applied(0),
      m_delta_transactions_applied(0),
      m_transactions_local(0),
      m_delta_transactions_local(0),
      m_transactions_committed_all_members(),
      m_stamp(0) {}

Pipeline_member_stats::Pipeline_member_stats(Pipeline_stats_member_message &msg)
    : m_transactions_waiting_apply(msg.get_transactions_waiting_apply()),
      m_transactions_applied(msg.get_transactions_applied()),
      m_delta_transactions_applied(0),
      m_transactions_local(msg.get_transactions_local()),
      m_delta_transactions_local(0),
      m_transactions_committed_all_members(
          msg.get_transaction_committed_all_members()),
      m_stamp(0) {}

Pipeline_member_stats::Pipeline_member_stats(
    Pipeline_stats_member_collector *pipeline_stats) {
  m_transactions_waiting_apply =
      pipeline_stats->get_transactions_waiting_apply();
  m_transactions_applied = pipeline_stats->get_transactions_applied();
  m_delta_transactions_applied = 0;
  m_transactions_local = pipeline_stats->get_transactions_local();
  m_delta_transactions_local = 0;
  m_stamp = 0;
}

void Pipeline_member_stats::update_member_stats(
    Pipeline_stats_member_message &msg, uint64 stamp) {

  m_transactions_waiting_apply = msg.get_transactions_waiting_apply();

  int64 previous_transactions_applied = m_transactions_applied;
  m_transactions_applied = msg.get_transactions_applied();
  m_delta_transactions_applied =
      m_transactions_applied - previous_transactions_applied;

  int64 previous_transactions_local = m_transactions_local;
  m_transactions_local = msg.get_transactions_local();
  m_delta_transactions_local =
      m_transactions_local - previous_transactions_local;

  /*
    Only update the transaction GTIDs if the current stats message contains
    these GTIDs, i.e. if they are "dirty" and in need of an update. Currently
    these updates are sent in 30 sec periods, while the stats message itself is
    sent at 1 sec period (for flow control purposes).
   */
  if (msg.get_transation_gtids_present()) {
    m_transactions_committed_all_members =
        msg.get_transaction_committed_all_members();
  }

  m_stamp = stamp;
}

int32 Pipeline_member_stats::get_transactions_waiting_apply() {
  return m_transactions_waiting_apply;
}

int64 Pipeline_member_stats::get_delta_transactions_applied() {
  return m_delta_transactions_applied;
}

int64 Pipeline_member_stats::get_delta_transactions_local() {
  return m_delta_transactions_local;
}

int64 Pipeline_member_stats::get_transactions_applied() {
  return m_transactions_applied;
}

int64 Pipeline_member_stats::get_transactions_local() {
  return m_transactions_local;
}

void Pipeline_member_stats::get_transaction_committed_all_members(
    std::string &value) {
  value.assign(m_transactions_committed_all_members);
}

void Pipeline_member_stats::set_transaction_committed_all_members(char *str,
                                                                  size_t len) {
  m_transactions_committed_all_members.assign(str, len);
}

uint64 Pipeline_member_stats::get_stamp() { return m_stamp; }

Flow_stat_module::Flow_stat_module()
    : m_stamp(0),
      seconds_to_skip(1) {
  m_flow_stat_module_info_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_flow_stat_module_info
#endif
  );
}

Flow_stat_module::~Flow_stat_module() {
  delete m_flow_stat_module_info_lock;
}

void Flow_stat_module::flow_stat_step(
    Pipeline_stats_member_collector *member) {
  if (--seconds_to_skip > 0) return;

  seconds_to_skip = 10;
  m_stamp++;

  /*
    Send statistics to other members
  */
  member->send_stats_member_message();

  // compute applier rate during recovery
  if (local_member_info->get_recovery_status() ==
      Group_member_info::MEMBER_IN_RECOVERY) {
    applier_module->get_pipeline_stats_member_collector()
        ->compute_transactions_deltas_during_recovery();
  }
}

int Flow_stat_module::handle_stats_data(const uchar *data, size_t len,
                                           const std::string &member_id) {
  DBUG_TRACE;
  int error = 0;
  Pipeline_stats_member_message message(data, len);

  /*
    This method is called synchronously by communication layer, so
    we do not need concurrency control.
  */
  m_flow_stat_module_info_lock->wrlock();
  Flow_stat_module_info::iterator it = m_info.find(member_id);
  if (it == m_info.end()) {
    Pipeline_member_stats stats;

    std::pair<Flow_stat_module_info::iterator, bool> ret = m_info.insert(
        std::pair<std::string, Pipeline_member_stats>(member_id, stats));
    error = !ret.second;
    it = ret.first;
  }
  it->second.update_member_stats(message, m_stamp);

  m_flow_stat_module_info_lock->unlock();
  return error;
}

Pipeline_member_stats *Flow_stat_module::get_pipeline_stats(
    const std::string &member_id) {
  Pipeline_member_stats *member_pipeline_stats = nullptr;
  m_flow_stat_module_info_lock->rdlock();
  Flow_stat_module_info::iterator it = m_info.find(member_id);
  if (it != m_info.end()) {
    try {
      DBUG_EXECUTE_IF("flow_stat_simulate_bad_alloc_exception",
                      throw std::bad_alloc(););
      member_pipeline_stats = new Pipeline_member_stats(it->second);
    } catch (const std::bad_alloc &) {
      my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0),
               "while getting replication_group_member_stats table rows",
               "get_pipeline_stats");
      m_flow_stat_module_info_lock->unlock();
      return nullptr;
    }
  }
  m_flow_stat_module_info_lock->unlock();
  return member_pipeline_stats;
}

