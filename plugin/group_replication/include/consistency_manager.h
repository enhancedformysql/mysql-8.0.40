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

#ifndef CONSISTENCY_MANAGER_INCLUDED
#define CONSISTENCY_MANAGER_INCLUDED

#include <mysql/group_replication_priv.h>
#include <mysql/plugin_group_replication.h>
#include <atomic>
#include <list>
#include <map>
#include <utility>

#include "plugin/group_replication/include/hold_transactions.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
  @class Transaction_consistency_info

  The consistency information of a transaction, including its
  configuration and state.
*/
class Transaction_consistency_info {
 public:
  /*
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size, const std::nothrow_t &) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_consistent_transactions, size, MYF(MY_WME));
  }

  /*
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.
  */
  void operator delete(void *ptr, const std::nothrow_t &) noexcept {
    my_free(ptr);
  }

  /**
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_consistent_transactions, size, MYF(MY_WME));
  }

  /**
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
  */
  void operator delete(void *ptr) noexcept { my_free(ptr); }

  virtual ~Transaction_consistency_info();

  /**
    Get the thread id that is executing the transaction.

    @return the thread id
  */
  my_thread_id get_thread_id();

  /**
    Is the transaction from this server?

   @return  true   yes
            false  otherwise
  */
  bool is_local_transaction();

  /**
    Get the transaction sidno.

   @return the sidno
  */
  rpl_sidno get_sidno();

  /**
    Get the transaction gno.

    @return the gno
  */
  rpl_gno get_gno();

  /**
    Get the transaction consistency.

    @return the consistency
  */
  enum_group_replication_consistency_level get_consistency_level();

  /**
    Is this transaction running on a single member group?

    @return  true   yes
             false  otherwise
  */
  bool is_a_single_member_group();

};

typedef std::pair<rpl_sidno, rpl_gno> Transaction_consistency_manager_key;
typedef std::pair<Transaction_consistency_manager_key,
                  Transaction_consistency_info *>
    Transaction_consistency_manager_pair;
typedef std::pair<Pipeline_event *, Transaction_consistency_manager_key>
    Transaction_consistency_manager_pevent_pair;
typedef std::map<
    Transaction_consistency_manager_key, Transaction_consistency_info *,
    std::less<Transaction_consistency_manager_key>,
    Malloc_allocator<std::pair<const Transaction_consistency_manager_key,
                               Transaction_consistency_info *>>>
    Transaction_consistency_manager_map;

/**
  @class Transaction_consistency_manager

  The consistency information of all ongoing transactions which have
  consistency GROUP_REPLICATION_CONSISTENCY_BEFORE.
*/
class Transaction_consistency_manager : public Group_transaction_listener {
 public:
  /**
    Constructor.
  */
  Transaction_consistency_manager();

  ~Transaction_consistency_manager() override;

  /**
    Clear all information.
  */
  void clear();

  /**
    Call action after a transaction is certified.
    The transaction coordination among the members will start on
    this point.

    @param[in]  transaction_info  the transaction info

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int after_certification(Transaction_consistency_info *transaction_info);

  /**
    Call action before a transaction starts.
    It will handle transactions with
    GROUP_REPLICATION_CONSISTENCY_BEFORE consistency.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  gr_consistency_level the transaction consistency
    @param[in]  timeout           maximum time to wait
    @param[in]  rpl_channel_type  type of the channel that receives the
                                  transaction

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int before_transaction_begin(my_thread_id thread_id,
                               ulong gr_consistency_level, ulong timeout,
                               enum_rpl_channel_type rpl_channel_type) override;

  /**
    Call action once a Sync_before_execution_message is received,
    this will allow fetch the group transactions set ordered with
    the message order.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  gcs_member_id     the member id

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int handle_sync_before_execution_message(
      my_thread_id thread_id, const Gcs_member_identifier &gcs_member_id) const;

  /**
    Inform that plugin did start.
  */
  void plugin_started();

  /**
    Inform that plugin is stopping.
    New consistent transactions are not allowed to start.
  */
  void plugin_is_stopping();

  /**
    Register an observer for transactions
  */
  void register_transaction_observer();

  /**
    Unregister the observer for transactions
  */
  void unregister_transaction_observer();

  int before_commit(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int before_rollback(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int after_rollback(my_thread_id thread_id) override;

  int after_commit(my_thread_id thread_id, rpl_sidno sidno,
                   rpl_gno gno) override;


  /**
    Tells the consistency manager that a primary election is running so it
    shall enable primary election checks
  */
  void enable_primary_election_checks();

  bool is_remote_prepare_before_view_change(const rpl_sid *sid, rpl_gno gno);

  /**
    Tells the consistency manager that a primary election ended so it
    shall disable primary election checks
  */
  void disable_primary_election_checks();

 private:
  /**
    Help method called by transaction begin action that, for
    transactions with consistency GROUP_REPLICATION_CONSISTENCY_BEFORE will:
      1) send a message to all members;
      2) when that message is received and processed in-order,
         w.r.t. the message stream, will fetch the Group Replication
         applier RECEIVED_TRANSACTION_SET, the set of remote
         transactions that were allowed to commit;
      3) wait until all the transactions on Group Replication applier
         RECEIVED_TRANSACTION_SET are committed.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  consistency_level the transaction consistency
    @param[in]  timeout           maximum time to wait

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int transaction_begin_sync_before_execution(
      my_thread_id thread_id,
      enum_group_replication_consistency_level consistency_level,
      ulong timeout) const;

  std::atomic<bool> m_plugin_stopping;
  std::atomic<bool> m_primary_election_active;

  /** Hold transaction mechanism */
  Hold_transactions m_hold_transactions;
};

#endif /* CONSISTENCY_MANAGER_INCLUDED */
