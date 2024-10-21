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

#ifndef PIPELINE_STATS_INCLUDED
#define PIPELINE_STATS_INCLUDED

#include <map>
#include <string>
#include <vector>

#include <mysql/group_replication_priv.h>
#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/plugin_psi.h"

/**
  @class Pipeline_stats_member_message

  Describes all statistics sent by members.
*/
class Pipeline_stats_member_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 4 bytes
    PIT_TRANSACTIONS_WAITING_CERTIFICATION = 1,

    // Length of the payload item: 4 bytes
    PIT_TRANSACTIONS_WAITING_APPLY = 2,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_APPLIED = 3,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_LOCAL = 4,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_ROWS_VALIDATING = 5,

    // Length of the payload item: variable
    PIT_TRANSACTIONS_COMMITTED_ALL_MEMBERS = 6,

    // Length of the payload item: 1 byte
    PIT_TRANSACTION_GTIDS_PRESENT = 7,

    // No valid type codes can appear after this one.
    PIT_MAX = 8 
  };

  /**
    Message constructor

    @param[in] transactions_waiting_apply
               Number of remote transactions waiting apply
    @param[in] transactions_applied
               Number of remote transactions applied
    @param[in] transactions_local
               Number of local transactions
    against
    @param[in] transaction_gtids
               Flag to indicate whether or not the transaction ids have been
    updated
    @param[in] transactions_committed_all_members
               Set of transactions committed on all members
  */
  Pipeline_stats_member_message(
      int32 transactions_waiting_apply,
      int64 transactions_applied, int64 transactions_local,
      bool transaction_gtids,
      const std::string &transactions_committed_all_members);

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Pipeline_stats_member_message(const unsigned char *buf, size_t len);

  /**
    Message destructor
   */
  ~Pipeline_stats_member_message() override;

  /**
    Get transactions waiting apply counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_apply();

  /**
    Get transactions applied.

    @return the counter value
  */
  int64 get_transactions_applied();

  /**
    Get local transactions that member tried to commit.

    @return the counter value
  */
  int64 get_transactions_local();

  /**
    Returns a flag indicating whether or not the GTIDs on this stats message
    are updated/present.

    @return the flag indicating the presence of valid GTIDs on this message.
   */
  bool get_transation_gtids_present() const;

  /**
    Get set of stable group transactions.

    @return the transaction identifier.
  */
  const std::string &get_transaction_committed_all_members();

 protected:
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;

  /**
    Message decoding method

    @param[in] buffer the received data
    @param[in] end    the end of the buffer
  */
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  int32 m_transactions_waiting_apply;
  int64 m_transactions_applied;
  int64 m_transactions_local;
  bool m_transaction_gtids_present;
  std::string m_transactions_committed_all_members;
};

/**
  @class Pipeline_stats_member_collector

  The pipeline collector for the local member stats.
*/
class Pipeline_stats_member_collector {
 public:
  /**
    Default constructor.
  */
  Pipeline_stats_member_collector();

  /**
    Destructor.
  */
  virtual ~Pipeline_stats_member_collector();

  /**
    Increment transactions waiting apply counter value.
  */
  void increment_transactions_waiting_apply();

  /**
    Decrement transactions waiting apply counter value.
  */
  void decrement_transactions_waiting_apply();

  /**
    Set transactions waiting apply counter to 0.
  */
  void clear_transactions_waiting_apply();

  /**
    Increment transactions applied counter value.
  */
  void increment_transactions_applied();

  /**
    Increment local transactions counter value.
  */
  void increment_transactions_local();

  /**
    Send member statistics to group.
  */
  void send_stats_member_message();

  /**
    Increment local recovery transactions counter value.
  */
  void increment_transactions_applied_during_recovery();

  /**
    @returns transactions waiting to be applied during recovery.
  */
  uint64 get_transactions_waiting_apply_during_recovery();

  /**
    Increment delivered transactions during recovery counter value.
  */
  void increment_transactions_delivered_during_recovery();

  /**
    Compute the transactions applied during last flow-stat tick
    while the member is in recovery.
  */
  void compute_transactions_deltas_during_recovery();

  /**
    @returns transactions applied during last flow-stat tick
             while the member is in recovery.
  */
  uint64 get_delta_transactions_applied_during_recovery();

  /**
    @returns transactions waiting to be applied.
  */
  int32 get_transactions_waiting_apply();

  /**
    @returns transactions applied of local member.
  */
  int64 get_transactions_applied();

  /**
    @returns local transactions proposed by member.
  */
  int64 get_transactions_local();

  /**
    Send Transaction Identifiers or not.
    Once Transactions identifiers are sent, variable will be reset to FALSE
    So need to set each time Transactions identifiers needs to be transmitted
  */
  void set_send_transaction_identifiers();

 private:
  std::atomic<int32> m_transactions_waiting_apply;
  std::atomic<int64> m_transactions_applied;
  std::atomic<int64> m_transactions_local;
  /* Includes both positively and negatively certified. */
  std::atomic<uint64> m_transactions_applied_during_recovery;
  uint64 m_previous_transactions_applied_during_recovery;
  std::atomic<uint64> m_delta_transactions_applied_during_recovery;
  std::atomic<uint64> m_transactions_delivered_during_recovery;

  bool send_transaction_identifiers;
  mysql_mutex_t m_transactions_waiting_apply_lock;
};

/**
  @class Pipeline_member_stats

  Computed statistics per member.
*/
class Pipeline_member_stats {
 public:
  /**
    Default constructor.
  */
  Pipeline_member_stats();

  /**
    Constructor.
  */
  Pipeline_member_stats(Pipeline_stats_member_message &msg);

  /**
    Constructor.
  */
  Pipeline_member_stats(Pipeline_stats_member_collector *pipeline_stats);

  /**
    Updates member statistics with a new message from the network
  */
  void update_member_stats(Pipeline_stats_member_message &msg, uint64 stamp);

  /**
    Get transactions waiting apply counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_apply();

  /**
    Get transactions applied counter value.

    @return the counter value
  */
  int64 get_transactions_applied();

  /**
    Get local member transactions proposed counter value.

    @return the counter value
  */
  int64 get_transactions_local();

  /**
    Get the stable group transactions.
  */
  void get_transaction_committed_all_members(std::string &value);

  /**
    Set the stable group transactions.
  */
  void set_transaction_committed_all_members(char *str, size_t len);

  /**
    Get transactions applied since last stats message.

    @return the counter value
  */
  int64 get_delta_transactions_applied();

  /**
    Get local transactions that member tried to commit
    since last stats message.

    @return the counter value
  */
  int64 get_delta_transactions_local();

  /**
    Get the last stats update stamp.

    @return the counter value
  */
  uint64 get_stamp();

 private:
  int32 m_transactions_waiting_apply;
  int64 m_transactions_applied;
  int64 m_delta_transactions_applied;
  int64 m_transactions_local;
  int64 m_delta_transactions_local;
  std::string m_transactions_committed_all_members;
  uint64 m_stamp;
};

/**
  Data type that holds all members stats.
  The key value is the GCS member_id.
*/
typedef std::map<std::string, Pipeline_member_stats> Flow_stat_module_info;

/**
  @class Flow_stat_module

  The pipeline stats aggregator of all group members stats and
  flow control module.
*/
class Flow_stat_module {
 public:
  static const int64 MAXTPS;

  /**
    Default constructor.
  */
  Flow_stat_module();

  /**
    Destructor.
  */
  virtual ~Flow_stat_module();

    /**
    Handles a Pipeline_stats_message, updating the
    Flow_stat_module_info and the delay, if needed.

    @param[in] data      the packet data
    @param[in] len       the packet length
    @param[in] member_id the GCS member_id which sent the message

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle_stats_data(const uchar *data, size_t len,
                        const std::string &member_id);

  /**
    Evaluate the information received in the last flow control period
    and adjust the system parameters accordingly
  */
  void flow_stat_step(Pipeline_stats_member_collector *);

  /**
    Returns copy of individual member stats information.
    @note      Its caller responsibility to clean up allocated memory.

    @param[in] member_id     GCS Type Member Id, i.e. format HOST:PORT
    @return the reference to class Pipeline_member_stats of memberID
    storing network(GCS Broadcasted) received information
  */
  Pipeline_member_stats *get_pipeline_stats(const std::string &member_id);

 private:
  Flow_stat_module_info m_info;
  /*
    A rw lock to protect the Flow_stat_module_info map.
  */
  Checkable_rwlock *m_flow_stat_module_info_lock;

  /*
    Counter incremented on every flow control step.
  */
  uint64 m_stamp;

  /*
    Remaining seconds to skip flow-stat steps
  */
  int seconds_to_skip;
};


#endif /* PIPELINE_STATS_INCLUDED */
