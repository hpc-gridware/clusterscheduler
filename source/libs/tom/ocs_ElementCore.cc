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
#include <string>

#include "comm/cl_communication.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "cull/cull_hash.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitype.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_observe.h"
#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"

#include "tom/ocs_ListCore.h"

namespace ocs::tom {

   namespace {
      /** @brief The nesting depth of UniqueCheckSuspension guards in the calling thread */
      thread_local int unique_check_suspended = 0;

      /**
       * @brief The hash key of a new value, normalised like cull_hash_key() normalises the stored one
       * @param mt the field's multitype
       * @param value the new value, as for check_unique_on_set()
       * @param host_key buffer for a normalised host name
       * @return the key, or nullptr for an unset string or host
       */
      const void *key_of_value(const int mt, const void *value, char *host_key) {
         if (mt_get_type(mt) == lHostT) {
            if (value == nullptr) {
               return nullptr;
            }
            sge_hostcpy(host_key, static_cast<const char *>(value));
            sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
            return host_key;
         }
         return value;
      }

      /**
       * @brief Whether two hash keys of a field are equal
       * @param mt the field's multitype
       * @param a a key, not nullptr
       * @param b a key, not nullptr
       * @return true if equal
       */
      bool keys_equal(const int mt, const void *a, const void *b) {
         switch (mt_get_type(mt)) {
            case lUlongT:
               return *static_cast<const lUlong *>(a) == *static_cast<const lUlong *>(b);
            case lUlong64T:
               return *static_cast<const lUlong64 *>(a) == *static_cast<const lUlong64 *>(b);
            default:
               return strcmp(static_cast<const char *>(a), static_cast<const char *>(b)) == 0;
         }
      }

      /**
       * @brief Another element of a list that holds a key in a unique field
       *
       * Through the field's table where the list has one or reaches the hash
       * threshold, building it if needed, so that filling a long list stays
       * linear; by a linear search below the threshold and in a list created
       * without hashing.
       *
       * @param lp the list
       * @param pos the field's position
       * @param self the element the key belongs to, which is not counted
       * @param key the key, normalised
       * @return the other element, or nullptr
       */
      const lListElem *find_other(const lList *lp, const int pos, const lListElem *self, const void *key) {
         if (const cull_htable ht = ocs_cull_hash_lookup_table(lp, pos); ht != nullptr) {
            const void *iterator = nullptr;
            const lListElem *found = cull_hash_first(ht, key, true, &iterator);
            return found != self ? found : nullptr;
         }
         const int mt = lp->descr[pos].mt;
         char host_key[CL_MAXHOSTNAMELEN + 1];
         for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
            if (ep == self) {
               continue;
            }
            const void *other = cull_hash_key(ep, pos, host_key);
            if (other != nullptr && keys_equal(mt, other, key)) {
               return ep;
            }
         }
         return nullptr;
      }
   } // namespace

   std::string key_text(const int mt, const void *key) {
      switch (mt_get_type(mt)) {
         case lUlongT:
            return std::to_string(*static_cast<const lUlong *>(key));
         case lUlong64T:
            return std::to_string(*static_cast<const lUlong64 *>(key));
         default:
            return static_cast<const char *>(key);
      }
   }

   namespace {
      /**
       * @brief The value of a number field an element holds, read from its slot, with the type check of before
       *
       * The number lookups do not check the field's type before they search;
       * the old getter by position checked it on each element it read and
       * aborted, so a lookup by a field of another type aborts on the first
       * element it reads, as before (LEG-01).
       *
       * @param ep the element
       * @param pos the field's position
       * @param type the list library type the lookup expects
       * @param getter the name of the old getter, for the abort message
       * @return the value
       */
      const lMultiType &number_value(const lListElem *ep, const int pos, const int type, const char *getter) {
         if (mt_get_type(ep->descr[pos].mt) != type) [[unlikely]] {
            incompatibleType(getter);
         }
         return ocs_cull_value(ep, pos);
      }

      /**
       * @brief The first element of a list that holds a value in a field, the lookup of the core
       *
       * Through the field's table where the list has one or reaches the hash
       * threshold, building it if needed (#ocs_cull_hash_lookup_table, which
       * throws on a repeated key of a unique field). Otherwise by a linear
       * search; for a unique field it goes on to the end of the list and throws
       * on a second match -- in every build on a list that may be hashed, in a
       * debug build also on a list created without hashing (N-H-18). A non
       * unique field stops at the first match, as before.
       *
       * @param lp the list, not nullptr
       * @param pos the field's position
       * @param hash_key the value as the table stores it (a normalised host name, a pointer to the number)
       * @param match whether an element holds the value, for the linear search
       * @param iterator the lookup's state, for the old ...Next functions
       * @return the element, or nullptr
       * @throw FieldException with reason DuplicateKey, naming type, field, key and list
       */
      template <typename Match>
      lListElem *lookup_first(const lList *lp, const int pos, const void *hash_key, Match match,
                              const void **iterator) {
         *iterator = nullptr;
         const int mt = lp->descr[pos].mt;
         if (const cull_htable ht = ocs_cull_hash_lookup_table(lp, pos); ht != nullptr) {
            return cull_hash_first(ht, hash_key, mt_is_unique(mt), iterator);
         }
#ifdef ENABLE_DEBUG_CHECKS
         const bool to_end = mt_do_hashing(mt) && mt_is_unique(mt);
#else
         const bool to_end = mt_do_hashing(mt) && mt_is_unique(mt) && lp->nslots > 0;
#endif
         lListElem *found = nullptr;
         for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
            if (!match(ep)) {
               continue;
            }
            if (found == nullptr) {
               found = ep;
               if (!to_end) {
                  break;
               }
            } else {
               char host_key[CL_MAXHOSTNAMELEN + 1];
               const void *key = cull_hash_key(ep, pos, host_key);
               throw_duplicate_key(lp->descr[pos].nm, lp->descr, key != nullptr ? key_text(mt, key) : "",
                                   lp->listname);
            }
         }
         *iterator = found;
         return found;
      }
   } // namespace

   lListElem *lookup_first_str(const lList *lp, const int pos, const char *value, const void **iterator) {
      return lookup_first(lp, pos, value, [pos, value](const lListElem *ep) {
         const char *s = ocs_cull_value(ep, pos).str;
         return s != nullptr && strcmp(s, value) == 0;
      }, iterator);
   }

   lListElem *lookup_first_ulong(const lList *lp, const int pos, const lUlong value, const void **iterator) {
      return lookup_first(lp, pos, &value, [pos, value](const lListElem *ep) {
         return number_value(ep, pos, lUlongT, "lGetPosUlong").ul == value;
      }, iterator);
   }

   lListElem *lookup_first_ulong64(const lList *lp, const int pos, const lUlong64 value, const void **iterator) {
      return lookup_first(lp, pos, &value, [pos, value](const lListElem *ep) {
         return number_value(ep, pos, lUlong64T, "lGetPosUlong64").ul64 == value;
      }, iterator);
   }

   namespace {
      /**
       * @brief The next element after the iterator's that holds a value, the linear part of the Next lookups
       *
       * Through the field's table where the list has one; the iterator is
       * nullptr after the last match, as the old ...Next functions left it.
       *
       * @param lp the list, not nullptr
       * @param pos the field's position
       * @param match whether an element holds the value
       * @param iterator the lookup's state; nullptr gives no element
       * @return the element, or nullptr
       */
      template <typename Match>
      lListElem *lookup_next(const lList *lp, const int pos, Match match, const void **iterator) {
         if (*iterator == nullptr) {
            return nullptr;
         }
         if (const cull_htable ht = list_hash_table(lp, pos); ht != nullptr) {
            return cull_hash_next(ht, iterator);
         }
         for (lListElem *ep = static_cast<const lListElem *>(*iterator)->next; ep != nullptr; ep = ep->next) {
            if (match(ep)) {
               *iterator = ep;
               return ep;
            }
         }
         *iterator = nullptr;
         return nullptr;
      }
   } // namespace

   lListElem *lookup_next_str(const lList *lp, const int pos, const char *value, const void **iterator) {
      return lookup_next(lp, pos, [pos, value](const lListElem *ep) {
         const char *s = ocs_cull_value(ep, pos).str;
         return s != nullptr && strcmp(s, value) == 0;
      }, iterator);
   }

   lListElem *lookup_next_ulong(const lList *lp, const int pos, const lUlong value, const void **iterator) {
      return lookup_next(lp, pos, [pos, value](const lListElem *ep) {
         return ocs_cull_value(ep, pos).ul == value;
      }, iterator);
   }

   lListElem *lookup_next_ulong64(const lList *lp, const int pos, const lUlong64 value, const void **iterator) {
      return lookup_next(lp, pos, [pos, value](const lListElem *ep) {
         return ocs_cull_value(ep, pos).ul64 == value;
      }, iterator);
   }

   lListElem *lookup_next_host(const lList *lp, const int pos, const char *value, const void **iterator) {
      char wanted[CL_MAXHOSTNAMELEN + 1];
      sge_hostcpy(wanted, value);
      return lookup_next(lp, pos, [pos, &wanted](const lListElem *ep) {
         const char *s = ocs_cull_value(ep, pos).host;
         if (s == nullptr) {
            return false;
         }
         char stored[CL_MAXHOSTNAMELEN + 1];
         sge_hostcpy(stored, s);
         return SGE_STRCASECMP(stored, wanted) == 0;
      }, iterator);
   }

   lListElem *lookup_first_host(const lList *lp, const int pos, const char *value, const void **iterator) {
      char host_key[CL_MAXHOSTNAMELEN + 1];
      sge_hostcpy(host_key, value);
      sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
      char wanted[CL_MAXHOSTNAMELEN + 1];
      sge_hostcpy(wanted, value);
      return lookup_first(lp, pos, host_key, [pos, &wanted](const lListElem *ep) {
         const char *s = ocs_cull_value(ep, pos).host;
         if (s == nullptr) {
            return false;
         }
         char stored[CL_MAXHOSTNAMELEN + 1];
         sge_hostcpy(stored, s);
         return SGE_STRCASECMP(stored, wanted) == 0;
      }, iterator);
   }

   UniqueCheckSuspension::UniqueCheckSuspension() noexcept {
      unique_check_suspended++;
   }

   UniqueCheckSuspension::~UniqueCheckSuspension() {
      unique_check_suspended--;
   }

   void check_unique_on_add_debug(const lList *lp, const lListElem *ep) {
      if (lp == nullptr || ep == nullptr || unique_check_suspended != 0) {
         return;
      }
      const lDescr *descr = lp->descr;
      const uint32_t n = ocs_cull_descr_header_of(descr)->n;
      char host_key[CL_MAXHOSTNAMELEN + 1];
      for (uint32_t pos = 0; pos < n; pos++) {
         const int mt = descr[pos].mt;
         if (!mt_do_hashing(mt) || !mt_is_unique(mt) || ep->descr[pos].off == OCS_CULL_ABSENT) {
            continue;
         }
         const void *key = cull_hash_key(ep, static_cast<int>(pos), host_key);
         if (key != nullptr && find_other(lp, static_cast<int>(pos), ep, key) != nullptr) {
            throw_duplicate_key(descr[pos].nm, descr, key_text(mt, key), lp->listname);
         }
      }
   }

   void check_unique_on_set_debug(const lListElem *ep, const int pos, const void *value) {
      if (unique_check_suspended != 0) {
         return;
      }
      const int mt = ep->descr[pos].mt;
      if (!mt_do_hashing(mt) || !mt_is_unique(mt)) {
         return;
      }
      const lList *lp = elem_list(ep);
      if (lp == nullptr) {
         return;
      }
      char host_key[CL_MAXHOSTNAMELEN + 1];
      const void *key = key_of_value(mt, value, host_key);
      if (key != nullptr && find_other(lp, pos, ep, key) != nullptr) {
         throw_duplicate_key(ep->descr[pos].nm, ep->descr, key_text(mt, key), lp->listname);
      }
   }

   void legacy_abort(const char *function, const FieldException &e) {
      DENTER(CULL_LAYER);
      CRITICAL("%s: %s", function, e.what());
      ocs::TerminationManager::trigger_abort();
      std::abort();
   }

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

   int present_pos(const lDescr *descr, const int nm) noexcept {
      const int pos = field_pos(descr, nm);
      if (pos < 0 || descr[pos].off == OCS_CULL_ABSENT) {
         return -1;
      }
      return pos;
   }

   int descr_length(const lDescr *descr) noexcept {
      if (descr == nullptr) {
         return -1;
      }
      int n = 0;
      while (mt_get_type(descr[n].mt) != lEndT) {
         n++;
      }
      return n;
   }

   int layout_difference(const lDescr *a, const lDescr *b) noexcept {
      if (a == nullptr || b == nullptr) {
         return LELISTNULL;
      }
      const int n = descr_length(a);
      const int m = descr_length(b);
      if (n <= 0 || m <= 0) {
         return LECOUNTDESCR;
      }
      if (n != m) {
         return LEDIFFDESCR;
      }
      for (int i = 0; i < n; i++) {
         // both full length: the same fields must be present (CS-439)
         if (a[i].nm != b[i].nm || mt_get_type(a[i].mt) != mt_get_type(b[i].mt) ||
             (a[i].off == OCS_CULL_ABSENT) != (b[i].off == OCS_CULL_ABSENT)) {
            return LEDIFFDESCR;
         }
      }
      return 0;
   }

   int checked_pos(const lListElem *ep, const int nm, const FieldType type) {
      if (ep == nullptr) [[unlikely]] {
         throw_field_exception(FieldException::Reason::NullElement, nm, nullptr);
      }
      const int pos = field_pos(ep->descr, nm);
      if (pos < 0) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignField, nm, ep->descr);
      }
      if (ep->descr[pos].off == OCS_CULL_ABSENT) [[unlikely]] {
         throw_field_exception(FieldException::Reason::AbsentField, nm, ep->descr);
      }
      if (field_type_of(ep->descr[pos].mt) != type) [[unlikely]] {
         throw_field_exception(FieldException::Reason::WrongType, nm, ep->descr, ep->descr);
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

      check_unique_on_set(ep, pos, value);

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

      ocs_cull_hash_insert_changed(ep, pos);
      return true;
   }

   void store_hashed_number(lListElem *ep, const int pos, const std::uint64_t value) {
      lMultiType &slot = ocs_cull_value(ep, pos);
      const bool is_32 = mt_get_type(ep->descr[pos].mt) == lUlongT;
      if (is_32 ? slot.ul == static_cast<lUlong>(value) : slot.ul64 == value) {
         return;
      }
      if (is_32) {
         const lUlong value_32 = static_cast<lUlong>(value);
         check_unique_on_set(ep, pos, &value_32);
      } else {
         check_unique_on_set(ep, pos, &value);
      }

      if (elem_hash_table(ep, pos) != nullptr) {
         cull_hash_remove(ep, pos);
      }
      if (is_32) {
         slot.ul = static_cast<lUlong>(value);
      } else {
         slot.ul64 = value;
      }
      ocs_cull_hash_insert_changed(ep, pos);
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
         free_list(ocs_cull_value(ep, pos).glp);
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
         free_elem(ocs_cull_value(ep, pos).obj);
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

   namespace {
      /**
       * @brief The position of a string field in a list's descriptor, for the linear string lookups
       * @param lp the list, not nullptr
       * @param nm the field number
       * @return the position
       * @throw FieldException with reason AbsentField or WrongType
       */
      int string_lookup_pos(const lList *lp, const int nm) {
         const int pos = present_pos(lp->descr, nm);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, nm, lp->descr);
         }
         if (mt_get_type(lp->descr[pos].mt) != lStringT) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongType, nm, lp->descr);
         }
         return pos;
      }
   } // namespace

   lListElem *lookup_first_str_nocase(const lList *lp, const int nm, const char *value) {
      if (lp == nullptr || value == nullptr) {
         return nullptr;
      }
      const int pos = string_lookup_pos(lp, nm);
      for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         const char *s = ocs_cull_value(ep, pos).str;
         if (s != nullptr && SGE_STRCASECMP(s, value) == 0) {
            return ep;
         }
      }
      return nullptr;
   }

   lListElem *lookup_first_str_like(const lList *lp, const int nm, const char *value) {
      if (lp == nullptr || value == nullptr) {
         return nullptr;
      }
      const int pos = string_lookup_pos(lp, nm);
      const std::size_t len = strlen(value);
      const bool prefix = len > 0 && value[len - 1] == '*';
      for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         const char *s = ocs_cull_value(ep, pos).str;
         if (s != nullptr && (strcmp(s, value) == 0 || (prefix && strncmp(s, value, len - 1) == 0))) {
            return ep;
         }
      }
      return nullptr;
   }

} // namespace ocs::tom
