/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
#include <stdlib.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "xcom/task_debug.h"
#include "xcom/xcom_profile.h"
#include "xdr_gen/xcom_vp.h"

bit_set *new_bit_set(uint32_t bits) {
  bit_set *bs = (bit_set *)malloc(sizeof(*bs));
  bs->bits.bits_len = howmany_words(bits, MASK_BITS);
  bs->bits.bits_val =
      (bit_mask *)malloc(bs->bits.bits_len * sizeof(*bs->bits.bits_val));
  BIT_ZERO(bs);
  return bs;
}

bit_set *clone_bit_set(bit_set *orig) {
  if (!orig) return orig;
  {
    bit_set *bs = (bit_set *)malloc(sizeof(*bs));
    bs->bits.bits_len = orig->bits.bits_len;
    bs->bits.bits_val =
        (bit_mask *)malloc(bs->bits.bits_len * sizeof(*bs->bits.bits_val));
    memcpy(bs->bits.bits_val, orig->bits.bits_val,
           bs->bits.bits_len * sizeof(*bs->bits.bits_val));
    return bs;
  }
}

void free_bit_set(bit_set *bs) {
  free(bs->bits.bits_val);
  free(bs);
}

/* Debug a bit set */
char *dbg_bitset(bit_set const *p, u_int nodes) {
  u_int i = 0;
  GET_NEW_GOUT;
  if (!p) {
    STRLIT("p == 0 ");
  } else {
    STRLIT("{");
    for (i = 0; i < nodes; i++) {
      NPUT(BIT_ISSET(i, p), d);
    }
    STRLIT("} ");
  }
  RET_GOUT;
}

