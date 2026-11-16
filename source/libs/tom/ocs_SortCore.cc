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

#include "tom/ocs_SortCore.h"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <vector>

#include "uti/sge_string.h"

#include "tom/cull_hash.h"
#include "tom/cull_lerrno.h"
#include "tom/cull_lerrnoP.h"
#include "tom/cull_listP.h"
#include "tom/cull_multitypeP.h"
#include "tom/cull_sortP.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"

namespace ocs::tom {

   namespace {

      /**
       * @brief Compare two values as the compare helpers of the list library did
       * @param a the first value
       * @param b the second value
       * @return -1, 0 or 1
       */
      template <typename V>
      int three_way(const V a, const V b) {
         return a == b ? 0 : (a < b ? -1 : 1);
      }

      /**
       * @brief Compare one field of two elements
       * @param a the first element
       * @param b the second element
       * @param key the criterion
       * @return negative, 0 or positive, before the direction is applied
       */
      int compare_field(const lListElem *a, const lListElem *b, const lSortOrder &key) {
         const lMultiType &x = ocs_cull_value(a, key.pos);
         const lMultiType &y = ocs_cull_value(b, key.pos);
         switch (mt_get_type(key.mt)) {
            case lIntT:
               return three_way(x.i, y.i);
            case lStringT:
               return sge_strnullcmp(x.str, y.str);
            case lHostT:
               return sge_strnullcmp(x.host, y.host);
            case lUlongT:
               return three_way(x.ul, y.ul);
            case lUlong64T:
               return three_way(x.ul64, y.ul64);
            case lDoubleT:
               return three_way(x.db, y.db);
            case lLongT:
               return three_way(x.l, y.l);
            case lBoolT:
               return three_way(x.b, y.b);
            case lRefT:
               return three_way(x.ref, y.ref);
            default:
               unknownType("lSortCompare");
               return 0;
         }
      }

      /**
       * @brief Sort the elements of a list with two elements or more, keeping equal ones in their order
       *
       * The elements are sorted as an array of pointers and linked again in
       * the new order; the hash tables are rebuilt afterwards. If the
       * comparison throws, the list is left as it was.
       *
       * @param lp the list
       * @param less true if the first element comes before the second
       * @throw ListCoreError with LEMALLOC if there is no memory
       */
      template <typename Less>
      void sort_elems(lList *lp, const Less &less) {
         std::vector<lListElem *> elems;
         try {
            elems.reserve(lp->nelem);
         } catch (const std::bad_alloc &) {
            throw ListCoreError(LEMALLOC, "no memory to sort a list");
         }
         for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
            elems.push_back(ep);
         }

         // the list is changed only after the sort, so a comparison that throws leaves it as it was
         std::stable_sort(elems.begin(), elems.end(), less);

         const std::size_t n = elems.size();
         lp->first = elems[0];
         lp->last = elems[n - 1];
         for (std::size_t i = 0; i < n; i++) {
            elems[i]->prev = i > 0 ? elems[i - 1] : nullptr;
            elems[i]->next = i + 1 < n ? elems[i + 1] : nullptr;
         }

         cull_hash_recreate_after_sort(lp);
      }

   } // namespace

   lSortOrder *make_sort_order(const lDescr *descr, const std::span<const SortKey> keys) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }
      auto *so = static_cast<lSortOrder *>(malloc(sizeof(lSortOrder) * (keys.size() + 1)));
      if (so == nullptr) {
         throw ListCoreError(LEMALLOC, "no memory for a sort order");
      }
      std::size_t i = 0;
      for (const SortKey &key : keys) {
         const int pos = present_pos(descr, key.nm);
         if (pos < 0) {
            free(so);
            throw_field_exception(field_pos(descr, key.nm) < 0 ? FieldException::Reason::ForeignField
                                                               : FieldException::Reason::AbsentField, key.nm, descr);
         }
         so[i].nm = key.nm;
         so[i].pos = pos;
         so[i].mt = descr[pos].mt;
         so[i].ad = key.ascending ? 1 : -1;
         i++;
      }
      so[i].nm = NoName;
      so[i].pos = 0;
      so[i].mt = lEndT;
      so[i].ad = 0;
      return so;
   }

   void free_sort_order(lSortOrder *&so) noexcept {
      free(so);
      so = nullptr;
   }

   int sort_compare(const lListElem *a, const lListElem *b, const lSortOrder *so) {
      int result = 0;
      for (int i = 0; result == 0 && so[i].nm != NoName; i++) {
         result = compare_field(a, b, so[i]) * so[i].ad;
      }
      return result;
   }

   void sort_list(lList *lp, const lSortOrder *so) {
      if (lp == nullptr || lp->nelem < 2) {
         return;
      }
      if (so == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp->descr);
      }
      sort_elems(lp, [so](const lListElem *a, const lListElem *b) {
         return sort_compare(a, b, so) < 0;
      });
   }

   void sort_list_with(lList *lp, const std::function<bool(const lListElem *, const lListElem *)> &less) {
      if (lp == nullptr || lp->nelem < 2) {
         return;
      }
      sort_elems(lp, less);
   }

   void check_sort_order(const lDescr *descr, const lSortOrder *so) {
      if (so == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, descr);
      }
      for (int i = 0; so[i].nm != NoName; i++) {
         if (present_pos(descr, so[i].nm) != so[i].pos) {
            throw_field_exception(FieldException::Reason::AbsentField, so[i].nm, descr);
         }
      }
   }

   void insert_sorted(lList *lp, lListElem *ep, const lSortOrder *so) {
      if (so == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp != nullptr ? lp->descr : nullptr);
      }
      if (lp == nullptr) {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }
      if (ep == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp->descr);
      }
      // the comparisons below read the element by the list's sort positions, so check its layout first (CS-3112)
      if (!same_layout(ep->descr, lp->descr)) [[unlikely]] {
         throw_field_exception(FieldException::Reason::WrongLayout, NoName, ep->descr, lp->descr);
      }
      lListElem *before = lp->first;
      while (before != nullptr && sort_compare(ep, before, so) > 0) {
         before = before->next;
      }
      if (before != nullptr) {
         insert_elem(lp, before->prev, ep);
      } else {
         append_elem(lp, ep);
      }
   }

   void resort_elem(lList *lp, lListElem *ep, const lSortOrder *so) {
      if (so == nullptr) {
         // checked before the element leaves the list, so that it is not lost
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp != nullptr ? lp->descr : nullptr);
      }
      dechain_elem(lp, ep);
      insert_sorted(lp, ep, so);
   }

} // namespace ocs::tom
