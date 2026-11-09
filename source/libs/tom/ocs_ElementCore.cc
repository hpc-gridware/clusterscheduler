/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The run-time core of the typed object model
 */

#include "tom/ocs_ElementCore.h"

#include <cstring>

#include "comm/cl_communication.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_hash.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_observe.h"
#include "cull/cull_state.h"

namespace ocs::tom {

   int field_pos(const lDescr *descr, const int nm) noexcept {
      if (descr == nullptr) {
         return -1;
      }
      const int pos = nm - descr[0].nm;
      if (pos < 0 || static_cast<uint32_t>(pos) >= ocs_cull_descr_header_of(descr)->n || descr[pos].nm != nm) {
         return -1;
      }
      return pos;
   }

   bool store_string(lListElem *ep, const int pos, const char *value) {
      const bool is_host = mt_get_type(ep->descr[pos].mt) == lHostT;
      char *old_value = is_host ? ocs_cull_value(ep, pos).host : ocs_cull_value(ep, pos).str;

      observe_value(ep, pos);

      // nothing to do if both are unset or both hold the same text
      if (value == old_value || (value != nullptr && old_value != nullptr && strcmp(value, old_value) == 0)) {
         return true;
      }

      char *new_value = nullptr;
      if (value != nullptr) {
         new_value = strdup(value);
         if (new_value == nullptr) {
            return false;
         }
      }

      if (elem_hash_table(ep, pos) != nullptr) {
         cull_hash_remove(ep, pos);
      }

      if (is_host) {
         sge_free(&(ocs_cull_value(ep, pos).host));
         ocs_cull_value(ep, pos).host = new_value;
      } else {
         sge_free(&(ocs_cull_value(ep, pos).str));
         ocs_cull_value(ep, pos).str = new_value;
      }

      if (const cull_htable ht = elem_hash_table(ep, pos); ht != nullptr) {
         char host_key[CL_MAXHOSTNAMELEN + 1];
         cull_hash_insert(ep, cull_hash_key(ep, pos, host_key), ht, mt_is_unique(ep->descr[pos].mt));
      }
      return true;
   }

   void store_hashed_number(lListElem *ep, const int pos, const std::uint64_t value) {
      lMultiType &slot = ocs_cull_value(ep, pos);
      const bool is_32 = mt_get_type(ep->descr[pos].mt) == lUlongT;
      if (is_32 ? slot.ul == static_cast<lUlong>(value) : slot.ul64 == value) {
         return;
      }

      if (elem_hash_table(ep, pos) != nullptr) {
         cull_hash_remove(ep, pos);
      }
      if (is_32) {
         slot.ul = static_cast<lUlong>(value);
      } else {
         slot.ul64 = value;
      }
      if (const cull_htable ht = elem_hash_table(ep, pos); ht != nullptr) {
         cull_hash_insert(ep, is_32 ? static_cast<void *>(&slot.ul) : static_cast<void *>(&slot.ul64), ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   void observe_value([[maybe_unused]] const lListElem *ep, [[maybe_unused]] const int pos) {
#ifdef OBSERVE
      lObserveChangeValue(ep, elem_hash_table(ep, pos) != nullptr, ep->descr[pos].nm);
#endif
   }

   void store_list(lListElem *ep, const int pos, lList *value) {
#ifdef OBSERVE
      lObserveChangeOwner(ocs_cull_value(ep, pos).glp, nullptr, ep, ep->descr[pos].nm);
      lObserveChangeOwner(value, ep, nullptr, ep->descr[pos].nm);
#endif
      if (value != ocs_cull_value(ep, pos).glp) {
         lFreeList(&(ocs_cull_value(ep, pos).glp));
         ocs_cull_value(ep, pos).glp = value;
      }
   }

   bool store_object(lListElem *ep, const int pos, lListElem *value) {
      if (value != nullptr && elem_kind(value) != FREE_ELEM && elem_kind(value) != TRANS_BOUND_ELEM) {
         return false;
      }
#ifdef OBSERVE
      lObserveChangeOwner(ocs_cull_value(ep, pos).obj, nullptr, ep, ep->descr[pos].nm);
      lObserveChangeOwner(value, ep, nullptr, ep->descr[pos].nm);
#endif
      if (value != ocs_cull_value(ep, pos).obj) {
         lFreeElem(&(ocs_cull_value(ep, pos).obj));
         ocs_cull_value(ep, pos).obj = value;
         if (value != nullptr) {
            elem_set_object(value);
         }
      }
      return true;
   }

   void set_lerrno(const int code) {
      cull_state_set_lerrno(code);
   }

} // namespace ocs::tom
