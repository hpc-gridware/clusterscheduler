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

#include "tom/ocs_SelectCore.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fnmatch.h>
#include <utility>
#include <vector>

#include "uti/sge_hostname.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ErrnoP.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/detail/ocs_State.h"
#include "tom/detail/ocs_WhatP.h"
#include "tom/detail/ocs_WhereP.h"
#include "tom/ocs_Descr.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Raw.h"

namespace ocs::tom {

   namespace {

      /**
       * @brief Free the sub-selections of entries that were not taken into a selection
       * @param entries the entries
       */
      void free_subs(const std::span<const WhatEntry> entries) noexcept {
         for (const WhatEntry &entry : entries) {
            lEnumeration *sub = entry.sub;
            free_what(sub);
         }
      }

      /**
       * @brief Allocate a condition node
       * @return the node, zeroed
       * @throw ListCoreError with LEMALLOC if there is no memory
       */
      lCondition *new_node() {
         auto *cp = static_cast<lCondition *>(calloc(1, sizeof(lCondition)));
         if (cp == nullptr) {
            throw ListCoreError(LEMALLOC, "out of memory building a condition");
         }
         return cp;
      }

      /**
       * @brief The position of a field in a descriptor, for building a selection or a condition
       * @param descr the descriptor
       * @param nm the field
       * @return the position
       * @throw FieldException with reason NullDescriptor or ForeignField
       */
      int descr_pos(const lDescr *descr, const int nm) {
         if (descr == nullptr) {
            throw_field_exception(FieldException::Reason::NullDescriptor, nm, nullptr);
         }
         // as the parsers of lWhat() and lWhere() looked it up: a field a reduced descriptor does not hold is absent
         const int pos = present_pos(descr, nm);
         if (pos < 0) {
            throw_field_exception(field_pos(descr, nm) < 0 ? FieldException::Reason::ForeignField
                                                           : FieldException::Reason::AbsentField, nm, descr);
         }
         return pos;
      }

      /**
       * @brief Compare two values as the old compare helpers of the list library did
       * @param a the field's value
       * @param b the condition's value
       * @return -1, 0 or 1
       */
      template <typename V>
      int three_way(const V a, const V b) {
         return a == b ? 0 : (a < b ? -1 : 1);
      }

      /**
       * @brief Copy one field for a reduced copy, mapping every failure to the error of the old partial copy
       * @param src the element copied from
       * @param dst the element copied to
       * @param pos the field's position in both
       * @param hash true to build the hash tables of copied sub-lists
       * @param sub_what the selection for a sub-list or sub-object, or nullptr
       * @param skip_no_transfer true if the copy is handed out
       * @throw ListCoreError with LECOPYSWITCH if the field cannot be copied
       */
      void copy_field_or_throw(const lListElem *src, lListElem *dst, const int pos, const bool hash,
                               const lEnumeration *sub_what, const bool skip_no_transfer) {
         try {
            copy_field(src, dst, pos, pos, hash, sub_what, skip_no_transfer);
         } catch (const FieldException &) {
            throw ListCoreError(LECOPYSWITCH, "copying a field of an element failed");
         } catch (const ListCoreError &) {
            throw ListCoreError(LECOPYSWITCH, "copying a field of an element failed");
         }
      }

      /**
       * @brief The value of the field a comparison names, read from the element's slot
       *
       * The condition was built on the descriptor of its type, so the field
       * has the condition's type unless the element is of another type; then
       * the process aborts, as the old getters by position did.
       *
       * @param ep the element
       * @param cp the comparison
       * @return the value
       */
      const lMultiType &field_value(const lListElem *ep, const lCondition *cp) {
         const int pos = cp->operand.cmp.pos;
         if (mt_get_type(ep->descr[pos].mt) != mt_get_type(cp->operand.cmp.mt)) [[unlikely]] {
            incompatibleType("lCompare");
         }
         return ocs::tom::detail::value(ep, pos);
      }

      /**
       * @brief The value of a string or host field of an element
       * @param ep the element
       * @param cp the comparison
       * @return the value, or nullptr
       */
      const char *string_value(const lListElem *ep, const lCondition *cp) {
         // the union holds a string and a host at the same place
         return field_value(ep, cp).str;
      }

      /**
       * @brief Evaluate a comparison of the ordering operators or a nested condition
       * @param ep the element
       * @param cp the comparison
       * @return whether it holds
       */
      bool compare_ordered(const lListElem *ep, const lCondition *cp) {
         const lMultiType &operand = cp->operand.cmp.val;
         int result;
         switch (mt_get_type(cp->operand.cmp.mt)) {
            case lIntT:
               result = three_way(field_value(ep, cp).i, operand.i);
               break;
            case lStringT:
            case lHostT: {
               const char *value = string_value(ep, cp);
               if (value == nullptr || operand.str == nullptr) {
                  set_lerrno(LENULLSTRING);
                  return false;
               }
               // strcmp() gives any negative or positive number; the old lCompare() took only -1 and 1, so an
               // ordering of strings held only where the first difference was one letter apart
               const int order = strcmp(value, operand.str);
               result = (order > 0) - (order < 0);
               break;
            }
            case lUlongT:
               result = three_way(field_value(ep, cp).ul, operand.ul);
               break;
            case lUlong64T:
               result = three_way(field_value(ep, cp).ul64, operand.ul64);
               break;
            case lListT: {
               const lList *sub = field_value(ep, cp).glp;
               if (sub == nullptr) {
                  set_lerrno(LELISTNULL);
                  return false;
               }
               return find_first(sub, operand.cp) != nullptr;
            }
            case lDoubleT:
               result = three_way(field_value(ep, cp).db, operand.db);
               break;
            case lLongT:
               result = three_way(field_value(ep, cp).l, operand.l);
               break;
            case lBoolT:
               result = three_way(field_value(ep, cp).b, operand.b);
               break;
            case lRefT:
               result = three_way(field_value(ep, cp).ref, operand.ref);
               break;
            default:
               unknownType("lCompare");
               return false;
         }
         switch (cp->op) {
            case EQUAL:
               return result == 0;
            case NOT_EQUAL:
               return result != 0;
            case LOWER_EQUAL:
               return result == -1 || result == 0;
            case LOWER:
               return result == -1;
            case GREATER_EQUAL:
               return result == 1 || result == 0;
            case GREATER:
               return result == 1;
            default:
               // a nested condition on a field that is not a list
               set_lerrno(LEOPUNKNOWN);
               return false;
         }
      }

      /**
       * @brief The operator token of the list library for a comparison
       * @param op the comparison
       * @return the token, as the parser of lWhere() stores it
       */
      int token_of(const CompareOp op) {
         switch (op) {
            case CompareOp::Equal:
               return EQUAL;
            case CompareOp::NotEqual:
               return NOT_EQUAL;
            case CompareOp::LowerEqual:
               return LOWER_EQUAL;
            case CompareOp::Lower:
               return LOWER;
            case CompareOp::GreaterEqual:
               return GREATER_EQUAL;
            case CompareOp::Greater:
               return GREATER;
            case CompareOp::Bitmask:
               return BITMASK;
            case CompareOp::NoCase:
               return STRCASECMP;
            case CompareOp::Pattern:
               return PATTERNCMP;
            case CompareOp::SameHost:
               return HOSTNAMECMP;
         }
         return EQUAL;
      }

   } // namespace

   lEnumeration *make_what(const lDescr *descr, const std::span<const WhatEntry> entries) {
      if (descr == nullptr) {
         free_subs(entries);
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }
      if (entries.empty()) {
         // no field at all, as lIntVector2What() of an empty vector
         throw_field_exception(FieldException::Reason::ForeignField, NoName, descr);
      }
      for (const WhatEntry &entry : entries) {
         try {
            static_cast<void>(descr_pos(descr, entry.nm));
         } catch (...) {
            free_subs(entries);
            throw;
         }
      }
      auto *ep = static_cast<lEnumeration *>(calloc(entries.size() + 1, sizeof(lEnumeration)));
      if (ep == nullptr) {
         free_subs(entries);
         throw ListCoreError(LEMALLOC, "out of memory building a selection");
      }
      std::size_t i = 0;
      for (const WhatEntry &entry : entries) {
         ep[i].pos = present_pos(descr, entry.nm);
         ep[i].nm = entry.nm;
         ep[i].mt = descr[ep[i].pos].mt & ~CULL_DESCR_INTERNED;
         ep[i].ep = entry.sub;
         i++;
      }
      ep[i].pos = 0;
      ep[i].nm = NoName;
      ep[i].mt = lEndT;
      ep[i].ep = nullptr;
      sort_what_by_pos(ep);
      return ep;
   }

   lEnumeration *make_what_special(const int pos) {
      auto *ep = static_cast<lEnumeration *>(calloc(2, sizeof(lEnumeration)));
      if (ep == nullptr) {
         throw ListCoreError(LEMALLOC, "out of memory building a selection");
      }
      ep[0].pos = pos;
      ep[0].nm = -99;
      ep[0].mt = -99;
      ep[0].ep = nullptr;
      ep[1].pos = 0;
      ep[1].nm = NoName;
      ep[1].mt = lEndT;
      ep[1].ep = nullptr;
      return ep;
   }

   void sort_what_by_pos(lEnumeration *ep) noexcept {
      if (ep == nullptr || ep[0].pos < 0) {
         return;
      }
      int n = 0;
      while (ep[n].nm != NoName) {
         n++;
      }
      std::stable_sort(ep, ep + n, [](const lEnumeration &a, const lEnumeration &b) { return a.pos < b.pos; });
      int m = 0;
      for (int i = 0; i < n; i++) {
         if (m > 0 && ep[i].pos == ep[m - 1].pos) {
            if (ep[m - 1].ep == nullptr) {
               ep[m - 1].ep = ep[i].ep;
            } else {
               free_what(ep[i].ep);
            }
            continue;
         }
         if (m != i) {
            ep[m].pos = ep[i].pos;
            ep[m].mt = ep[i].mt;
            ep[m].nm = ep[i].nm;
            ep[m].ep = ep[i].ep;
         }
         m++;
      }
      if (m != n) {
         ep[m].pos = ep[n].pos;
         ep[m].mt = ep[n].mt;
         ep[m].nm = ep[n].nm;
         ep[m].ep = ep[n].ep;
      }
   }

   lCondition *make_compare(const lDescr *descr, const int nm, const CompareOp op, const int value_type,
                            const lMultiType &value) {
      const int pos = descr_pos(descr, nm);
      const int field_type = mt_get_type(descr[pos].mt);
      if (value_type != field_type && !(value_type == lStringT && field_type == lHostT)) {
         throw_field_exception(FieldException::Reason::WrongType, nm, descr);
      }
      lCondition *cp = new_node();
      cp->op = token_of(op);
      cp->operand.cmp.nm = nm;
      cp->operand.cmp.pos = pos;
      cp->operand.cmp.mt = descr[pos].mt;
      switch (field_type) {
         case lStringT:
         case lHostT: {
            // the union holds a string and a host at the same place
            const char *text = value.str;
            if (text != nullptr && (cp->operand.cmp.val.str = strdup(text)) == nullptr) {
               sge_free(&cp);
               throw ListCoreError(LESTRDUP, "out of memory copying the value of a condition");
            }
            break;
         }
         default:
            cp->operand.cmp.val = value;
            break;
      }
      return cp;
   }

   lCondition *make_subscope(const lDescr *descr, const int nm, lCondition *sub) {
      int pos;
      try {
         pos = descr_pos(descr, nm);
         if (mt_get_type(descr[pos].mt) != lListT) {
            throw_field_exception(FieldException::Reason::WrongType, nm, descr);
         }
      } catch (...) {
         free_where(sub);
         throw;
      }
      lCondition *cp;
      try {
         cp = new_node();
      } catch (...) {
         free_where(sub);
         throw;
      }
      cp->op = SUBSCOPE;
      cp->operand.cmp.nm = nm;
      cp->operand.cmp.pos = pos;
      cp->operand.cmp.mt = descr[pos].mt;
      cp->operand.cmp.val.cp = sub;
      return cp;
   }

   lCondition *make_logic(const LogicOp op, lCondition *first, lCondition *second) {
      if (first == nullptr || (op != LogicOp::Not && second == nullptr)) {
         free_where(first);
         free_where(second);
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      if (op == LogicOp::Not) {
         free_where(second);
      }
      lCondition *cp;
      try {
         cp = new_node();
      } catch (...) {
         free_where(first);
         free_where(second);
         throw;
      }
      cp->op = op == LogicOp::And ? AND : op == LogicOp::Or ? OR : NEG;
      cp->operand.log.first = first;
      cp->operand.log.second = second;
      return cp;
   }

   bool compare(const lListElem *ep, const lCondition *cp) {
      if (ep == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      if (cp == nullptr) {
         return true;
      }
      switch (cp->op) {
         case EQUAL:
         case NOT_EQUAL:
         case LOWER_EQUAL:
         case LOWER:
         case GREATER_EQUAL:
         case GREATER:
         case SUBSCOPE:
            return compare_ordered(ep, cp);

         case STRCASECMP:
         case HOSTNAMECMP: {
            const int type = mt_get_type(cp->operand.cmp.mt);
            if (type != lStringT && type != lHostT) {
               unknownType("lCompare");
               return false;
            }
            const char *value = string_value(ep, cp);
            const char *operand = cp->operand.cmp.val.str;
            if (value == nullptr || operand == nullptr) {
               set_lerrno(LENULLSTRING);
               return false;
            }
            return (cp->op == STRCASECMP ? SGE_STRCASECMP(value, operand) : sge_hostcmp(value, operand)) == 0;
         }

         case PATTERNCMP: {
            const int type = mt_get_type(cp->operand.cmp.mt);
            if (type != lStringT && type != lHostT) {
               unknownType("lCompare");
               return false;
            }
            const char *value = string_value(ep, cp);
            const char *pattern = cp->operand.cmp.val.str;
            if (pattern == nullptr) {
               set_lerrno(LENULLSTRING);
               return false;
            }
            // a null string matches as the empty one
            return fnmatch(pattern, value != nullptr ? value : "", 0) == 0;
         }

         case BITMASK:
            if (mt_get_type(cp->operand.cmp.mt) != lUlongT) {
               unknownType("lCompare");
               return false;
            }
            return (field_value(ep, cp).ul & cp->operand.cmp.val.ul) == cp->operand.cmp.val.ul;

         case AND:
            return compare(ep, cp->operand.log.first) && compare(ep, cp->operand.log.second);

         case OR:
            return compare(ep, cp->operand.log.first) || compare(ep, cp->operand.log.second);

         case NEG:
            return !compare(ep, cp->operand.log.first);

         default:
            // a condition that was not built by the core or the parsers is corrupt
            exit(-1);
      }
   }

   lListElem *find_first(const lList *lp, const lCondition *cp) {
      if (lp == nullptr) {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }
      lListElem *ep = lp->first;
      while (ep != nullptr && !compare(ep, cp)) {
         ep = ep->next;
      }
      return ep;
   }

   lListElem *find_last(const lList *lp, const lCondition *cp) {
      if (lp == nullptr) {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }
      lListElem *ep = lp->last;
      while (ep != nullptr && !compare(ep, cp)) {
         ep = ep->prev;
      }
      return ep;
   }

   lListElem *find_next(const lListElem *ep, const lCondition *cp) {
      if (ep == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      lListElem *next = ep->next;
      while (next != nullptr && !compare(next, cp)) {
         next = next->next;
      }
      return next;
   }

   lListElem *find_prev(const lListElem *ep, const lCondition *cp) {
      if (ep == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      lListElem *prev = ep->prev;
      while (prev != nullptr && !compare(prev, cp)) {
         prev = prev->prev;
      }
      return prev;
   }

   lList *select_list(const char *name, const lList *lp, const lCondition *cp, const lEnumeration *what,
                      const bool hash, const bool skip_no_transfer) {
      if (lp == nullptr) {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }
      if (what == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp->descr);
      }
      // the shared reduction, cached in the selection; it sets the error number when it fails
      bool owned;
      ocs::tom::detail::state_set_lerrno(0);
      const lDescr *descr = ocs::tom::detail::what_descr(what, lp->descr, &owned);
      if (descr == nullptr) {
         throw ListCoreError(ocs::tom::detail::state_get_lerrno(), "reducing the descriptor of a selection failed");
      }
      lList *dst;
      try {
         // the new list takes a reference of its own on the reduction
         dst = create_list(name, descr, false);
      } catch (const ListCoreError &) {
         if (owned) {
            ocs::tom::release_descr(descr);
         }
         throw ListCoreError(LECREATELIST, "out of memory creating the list of a selection");
      }
      if (owned) {
         ocs::tom::release_descr(descr);
      }

      // a selection from a list that was checked for duplicate keys when it was filled cannot create one
      const UniqueCheckSuspension suspension;
      try {
         for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
            if (!compare(ep, cp)) {
               continue;
            }
            lListElem *copy = create_elem(dst->descr);
            try {
               copy_elem_partial(copy, ep, what, hash, skip_no_transfer);
            } catch (const ListCoreError &) {
               // an element whose fields cannot be copied is left out, as lSelect() left it out
               free_elem(copy);
               continue;
            }
            append_elem(dst, copy);
         }
      } catch (...) {
         free_list(dst);
         throw;
      }
      if (hash) {
         // the list may be hashed from now on; its tables are built by the first lookups
         ocs::tom::make_hashable(dst);
      }
      return dst;
   }

   void free_what(lEnumeration *&ep) noexcept {
      if (ep == nullptr) {
         return;
      }
      for (int i = 0; mt_get_type(ep[i].mt) != lEndT; i++) {
         free_what(ep[i].ep);
      }
      ocs::tom::release_descr(ep[0].cache_dp);
      ocs::tom::release_descr(ep[0].cache_src);
      sge_free(&ep);
   }

   lEnumeration *copy_what(const lEnumeration *ep) {
      if (ep == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      int n = 0;
      while (mt_get_type(ep[n].mt) != lEndT) {
         n++;
      }
      // the cache of the reduced descriptor is not copied; the copy builds its own
      auto *copy = static_cast<lEnumeration *>(calloc(n + 1, sizeof(lEnumeration)));
      if (copy == nullptr) {
         throw ListCoreError(LEMALLOC, "out of memory copying a selection");
      }
      try {
         for (int i = 0; i <= n; i++) {
            copy[i].pos = ep[i].pos;
            copy[i].nm = ep[i].nm;
            copy[i].mt = ep[i].mt;
            copy[i].ep = ep[i].ep != nullptr ? copy_what(ep[i].ep) : nullptr;
         }
      } catch (...) {
         free_what(copy);
         throw;
      }
      return copy;
   }

   void free_where(lCondition *&cp) noexcept {
      if (cp == nullptr) {
         return;
      }
      switch (cp->op) {
         case EQUAL:
         case NOT_EQUAL:
         case LOWER_EQUAL:
         case LOWER:
         case GREATER_EQUAL:
         case GREATER:
         case BITMASK:
         case STRCASECMP:
         case PATTERNCMP:
         case HOSTNAMECMP:
         case SUBSCOPE:
            switch (mt_get_type(cp->operand.cmp.mt)) {
               case lStringT:
               case lHostT:
                  // the union holds a string and a host at the same place
                  sge_free(&(cp->operand.cmp.val.str));
                  break;
               case lListT:
                  free_where(cp->operand.cmp.val.cp);
                  break;
               default:
                  break;
            }
            break;
         case AND:
         case OR:
            free_where(cp->operand.log.first);
            free_where(cp->operand.log.second);
            break;
         case NEG:
            free_where(cp->operand.log.first);
            break;
         default:
            set_lerrno(LEOPUNKNOWN);
            break;
      }
      sge_free(&cp);
   }

   lCondition *copy_where(const lCondition *cp) {
      if (cp == nullptr) {
         return nullptr;
      }
      lCondition *copy = new_node();
      copy->op = cp->op;
      try {
         switch (cp->op) {
            case EQUAL:
            case NOT_EQUAL:
            case LOWER_EQUAL:
            case LOWER:
            case GREATER_EQUAL:
            case GREATER:
            case BITMASK:
            case STRCASECMP:
            case PATTERNCMP:
            case HOSTNAMECMP:
            case SUBSCOPE:
               copy->operand.cmp.pos = cp->operand.cmp.pos;
               copy->operand.cmp.mt = cp->operand.cmp.mt;
               copy->operand.cmp.nm = cp->operand.cmp.nm;
               switch (mt_get_type(cp->operand.cmp.mt)) {
                  case lStringT:
                  case lHostT:
                     if (cp->operand.cmp.val.str != nullptr &&
                         (copy->operand.cmp.val.str = strdup(cp->operand.cmp.val.str)) == nullptr) {
                        throw ListCoreError(LESTRDUP, "out of memory copying the value of a condition");
                     }
                     break;
                  case lListT:
                     copy->operand.cmp.val.cp = copy_where(cp->operand.cmp.val.cp);
                     break;
                  case lObjectT:
                  case lRefT:
                     // as lCopyWhere() did, a reference is not taken over
                     break;
                  case lIntT:
                  case lUlongT:
                  case lUlong64T:
                  case lDoubleT:
                  case lLongT:
                  case lBoolT:
                     copy->operand.cmp.val = cp->operand.cmp.val;
                     break;
                  default:
                     unknownType("lCopyWhere");
                     throw ListCoreError(LEUNKTYPE, "a condition of an unknown type");
               }
               break;
            case AND:
            case OR:
               copy->operand.log.second = copy_where(cp->operand.log.second);
               copy->operand.log.first = copy_where(cp->operand.log.first);
               break;
            case NEG:
               copy->operand.log.first = copy_where(cp->operand.log.first);
               break;
            default:
               throw ListCoreError(LEOPUNKNOWN, "a condition with an unknown operator");
         }
      } catch (...) {
         // the node was zeroed, so freeing it frees only what was copied
         free_where(copy);
         throw;
      }
      return copy;
   }

   void copy_elem_partial(lListElem *dst, const lListElem *src, const lEnumeration *what, const bool hash,
                          const bool skip_no_transfer) {
      if (dst == nullptr || src == nullptr || what == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      switch (what[0].pos) {
         case WHAT_ALL:
            for (const auto f : ocs::tom::present_fields(src->descr)) {
               copy_field_or_throw(src, dst, f.pos, hash, what[0].ep, skip_no_transfer);
            }
            break;
         case WHAT_NONE:
            break;
         default: {
            const int maxpos = std::min(descr_length(src->descr), descr_length(dst->descr));
            for (int i = 0; what[i].nm != NoName; i++) {
               const int pos = what[i].pos;
               if (pos >= maxpos || pos < 0 || src->descr[pos].off == OCS_CULL_ABSENT ||
                   dst->descr[pos].off == OCS_CULL_ABSENT) {
                  throw ListCoreError(LEENUMDESCR, "a selected field is absent in an element");
               }
               copy_field_or_throw(src, dst, pos, hash, what[i].ep, skip_no_transfer);
            }
            break;
         }
      }
   }

   lListElem *select_elem(const lListElem *ep, const lEnumeration *what, const bool hash,
                          const bool skip_no_transfer) {
      if (ep == nullptr || what == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, nullptr);
      }
      // the shared reduction, cached in the selection; it sets the error number when it fails
      bool owned;
      ocs::tom::detail::state_set_lerrno(0);
      const lDescr *descr = ocs::tom::detail::what_descr(what, ep->descr, &owned);
      if (descr == nullptr) {
         throw ListCoreError(ocs::tom::detail::state_get_lerrno(), "reducing the descriptor of a selection failed");
      }
      lListElem *copy;
      try {
         // the element takes a reference of its own on the reduction
         copy = create_elem(descr);
      } catch (...) {
         if (owned) {
            ocs::tom::release_descr(descr);
         }
         throw;
      }
      if (owned) {
         ocs::tom::release_descr(descr);
      }
      try {
         copy_elem_partial(copy, ep, what, hash, skip_no_transfer);
      } catch (...) {
         free_elem(copy);
         throw;
      }
      return copy;
   }


   int count_what(const lEnumeration *enp, const lDescr *dp) noexcept {
      if (enp == nullptr) {
         LERROR(LEENUMNULL);
         return -1;
      }
      if (dp == nullptr) {
         LERROR(LEDESCRNULL);
         return -1;
      }
      switch (enp[0].pos) {
         case WHAT_NONE:
            return 0;
         case WHAT_ALL: {
            const int n = lCountPresent(dp);
            if (n == -1) {
               LERROR(LECOUNTDESCR);
            }
            return n;
         }
         default: {
            int n = 0;
            while (enp[n].nm != NoName) {
               n++;
            }
            return n;
         }
      }
   }

   int partial_descr(const lEnumeration *enp, const lDescr *sdp, lDescr *ddp, int *indexp) noexcept {
      if (enp == nullptr) {
         LERROR(LEELEMNULL);
         return -1;
      }
      if (sdp == nullptr || ddp == nullptr) {
         LERROR(LEDESCRNULL);
         return -1;
      }
      if (indexp == nullptr) {
         LERROR(LENULLARGS);
         return -1;
      }

      const int n = descr_length(sdp);
      std::vector<bool> keep(n, false);
      switch (enp[0].pos) {
         case WHAT_NONE:
            return 0;
         case WHAT_ALL:
            for (int i = 0; i < n; i++) {
               keep[i] = sdp[i].off != OCS_CULL_ABSENT;
            }
            break;
         default:
            for (int i = 0; mt_get_type(enp[i].mt) != lEndT; i++) {
               const int pos = enp[i].pos;
               if (pos < 0 || pos >= n || enp[i].nm != sdp[pos].nm || mt_get_type(enp[i].mt) != mt_get_type(sdp[pos].mt) ||
                   sdp[pos].off == OCS_CULL_ABSENT) {
                  LERROR(LEENUMDESCR);
                  return -1;
               }
               keep[pos] = true;
            }
      }
      if (ocs::tom::descr_layout(sdp, keep, ddp) < 0) {
         LERROR(LEENUMDESCR);
         return -1;
      }
      *indexp = n;
      return 0;
   }


   bool condition_fits(const lCondition *cp, const lDescr *descr) noexcept {
      if (cp == nullptr) {
         return true;
      }
      switch (cp->op) {
         case AND:
         case OR:
            return condition_fits(cp->operand.log.first, descr) && condition_fits(cp->operand.log.second, descr);
         case NEG:
            return condition_fits(cp->operand.log.first, descr);
         default:
            // a comparison, or the list field of a condition on its elements
            return field_pos(descr, cp->operand.cmp.nm) >= 0;
      }
   }

   lEnumeration *merge_what(lEnumeration *what1, lEnumeration *what2) {
      if (what2 == nullptr) {
         return what1;
      }
      if (what1 == nullptr || what1[0].pos == WHAT_NONE || what2[0].pos == WHAT_ALL) {
         free_what(what1);
         return what2;
      }
      if (what1[0].pos == WHAT_ALL || what2[0].pos == WHAT_NONE) {
         free_what(what2);
         return what1;
      }
      std::vector<lEnumeration> entries;
      auto give_up = [&] {
         for (lEnumeration &entry : entries) {
            free_what(entry.ep);
         }
         free_what(what1);
         free_what(what2);
      };
      try {
         for (int i = 0; mt_get_type(what1[i].mt) != lEndT; i++) {
            entries.push_back({what1[i].pos, what1[i].mt, what1[i].nm, std::exchange(what1[i].ep, nullptr), nullptr,
                               nullptr});
         }
         for (int i = 0; mt_get_type(what2[i].mt) != lEndT; i++) {
            auto same = std::ranges::find_if(entries, [&](const lEnumeration &entry) {
               return entry.nm == what2[i].nm;
            });
            lEnumeration *sub = std::exchange(what2[i].ep, nullptr);
            if (same == entries.end()) {
               entries.push_back({what2[i].pos, what2[i].mt, what2[i].nm, sub, nullptr, nullptr});
            } else if (same->ep != nullptr && sub != nullptr) {
               same->ep = merge_what(std::exchange(same->ep, nullptr), sub);
            } else {
               // one of both selects the whole sub-list or sub-object, so the merge does
               free_what(same->ep);
               free_what(sub);
            }
         }
      } catch (...) {
         give_up();
         throw;
      }
      auto *ep = static_cast<lEnumeration *>(calloc(entries.size() + 1, sizeof(lEnumeration)));
      if (ep == nullptr) {
         give_up();
         throw ListCoreError(LEMALLOC, "out of memory merging two selections");
      }
      std::ranges::copy(entries, ep);
      ep[entries.size()].pos = 0;
      ep[entries.size()].nm = NoName;
      ep[entries.size()].mt = lEndT;
      ep[entries.size()].ep = nullptr;
      free_what(what1);
      free_what(what2);
      sort_what_by_pos(ep);
      return ep;
   }

   void set_sub_what(lEnumeration *what, const int nm, lEnumeration *sub) {
      for (int i = 0; mt_get_type(what[i].mt) != lEndT; i++) {
         if (what[i].nm == nm) {
            free_what(what[i].ep);
            what[i].ep = sub;
            return;
         }
      }
      free_what(sub);
      throw_field_exception(FieldException::Reason::ForeignField, nm, nullptr);
   }

   const lDescr *reduced_descr(const lDescr *descr, const lEnumeration *what) noexcept {
      if (count_what(what, descr) <= 0) {
         return nullptr;
      }
      bool owned;
      const lDescr *dp = ocs::tom::detail::what_descr(what, descr, &owned);
      if (dp == nullptr) {
         return nullptr;
      }
      return owned ? dp : ocs::tom::descr_ref(dp);
   }

} // namespace ocs::tom

/**
 * @brief The shared reduction of a descriptor that a field selection describes
 *
 * The selection caches the last reduction it built, keyed by the source
 * descriptor (N-A-20), so a selection applied again and again to lists of one
 * type, as for the sub-lists of every element of a list, builds and interns
 * its descriptor once. The cache holds a reference on the source as well, so
 * its address cannot be reused while it is cached. Only static and counted
 * sources are cached; any other source gets a fresh reduction each time. A
 * selection is never used by two threads at once, so the cache needs no
 * synchronisation.
 *
 * @param enp the selection, not NONE
 * @param src the descriptor it is applied to
 * @param[out] owned set to true if the caller has to release the result
 * @return the shared reduction borrowed from the cache, or with a reference
 *         for the caller if @p owned is set to true; nullptr on error
 */
const lDescr *ocs::tom::detail::what_descr(const lEnumeration *enp, const lDescr *src, bool *owned) {
   *owned = false;
   auto *head = const_cast<lEnumeration *>(enp);
   if (head->cache_dp != nullptr && head->cache_src == src) {
      return head->cache_dp;
   }

   if (ocs::tom::count_what(enp, src) <= 0) {
      LERROR(LECOUNTWHAT);
      return nullptr;
   }
   // the reduction has the full length of the type (N-B-1)
   auto *tmp = reinterpret_cast<lDescr *>(sge_malloc(sizeof(lDescr) * (ocs::tom::descr_length(src) + 1)));
   if (tmp == nullptr) {
      LERROR(LEMALLOC);
      return nullptr;
   }
   int index = 0;
   if (ocs::tom::partial_descr(enp, src, tmp, &index) < 0) {
      LERROR(LEPARTIALDESCR);
      sge_free(&tmp);
      return nullptr;
   }
   const lDescr *dp = ocs::tom::descr_acquire(tmp);
   sge_free(&tmp);
   if (dp == nullptr) {
      LERROR(LEMALLOC);
      return nullptr;
   }

   if (ocs::tom::descr_is_counted(src) || ocs::tom::descr_is_static(src)) {
      ocs::tom::release_descr(head->cache_dp);
      ocs::tom::release_descr(head->cache_src);
      head->cache_src = ocs::tom::descr_ref(src);
      head->cache_dp = dp;
      return dp;
   }
   *owned = true;
   return dp;
}

/**
 * @brief Whether a field selection can be applied to a descriptor (CS-439, N-B-11)
 *
 * Positions are those of the type in every descriptor, so a selection built on
 * the full type applies to any reduction of it that holds the selected fields.
 * #WHAT_ALL and #WHAT_NONE apply to every descriptor.
 *
 * @param enp the selection
 * @param dp the descriptor
 * @return true if every selected field is present in dp at its position
 */
bool ocs::tom::detail::what_fits(const lEnumeration *enp, const lDescr *dp) {
   if (enp == nullptr || dp == nullptr) {
      return false;
   }
   if (enp[0].pos == WHAT_ALL || enp[0].pos == WHAT_NONE) {
      return true;
   }
   const int n = ocs::tom::descr_length(dp);
   for (int i = 0; enp[i].nm != NoName; i++) {
      const int pos = enp[i].pos;
      if (pos < 0 || pos >= n || dp[pos].nm != enp[i].nm || dp[pos].off == OCS_CULL_ABSENT) {
         return false;
      }
   }
   return true;
}

