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

#include "plugin/group_replication/include/plugin_messages/transaction_with_guarantee_message.h"
#include "my_dbug.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

const uint64_t
    Transaction_with_guarantee_message::s_consistency_level_pit_size =
        Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE + 1;

bool Transaction_with_guarantee_message::write(const unsigned char *buffer,
                                               my_off_t length) {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return true;
  }

  return m_gcs_message_data->append_to_payload(buffer, length);
}

uint64_t Transaction_with_guarantee_message::length() {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return 0;
  }

  return m_gcs_message_data->get_encode_size();
}

Gcs_message_data *
Transaction_with_guarantee_message::get_message_data_and_reset() {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return nullptr;
  }

  /*
    Add the PIT_TRANSACTION_CONSISTENCY_LEVEL to the Gcs_message_data.
  */
  std::vector<unsigned char> buffer;
  char consistency_level_aux = static_cast<char>(m_consistency_level);
  encode_payload_item_char(&buffer, PIT_TRANSACTION_CONSISTENCY_LEVEL,
                           consistency_level_aux);
  m_gcs_message_data->append_to_payload(&buffer.front(),
                                        s_consistency_level_pit_size);

  Gcs_message_data *result = m_gcs_message_data;
  m_gcs_message_data = nullptr;
  return result;
}

void Transaction_with_guarantee_message::encode_payload(
    std::vector<unsigned char> *) const {
  DBUG_TRACE;
  assert(0);
}

void Transaction_with_guarantee_message::decode_payload(const unsigned char *,
                                                        const unsigned char *) {
  DBUG_TRACE;
  assert(0);
}

