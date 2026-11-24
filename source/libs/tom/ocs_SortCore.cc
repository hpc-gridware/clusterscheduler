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

#include "tom/detail/ocs_SortElems.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/detail/ocs_SortP.h"

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
      int compare_field(const ocs::tom::ElementData *a, const ocs::tom::ElementData *b,
                        const ocs::tom::SortOrderData &key) {
         const ocs::tom::FieldValue &x = ocs::tom::detail::value(a, key.pos);
         const ocs::tom::FieldValue &y = ocs::tom::detail::value(b, key.pos);
         switch (mt_get_type(key.mt)) {
            case MT_INT32:
               return three_way(x.i, y.i);
            case MT_STRING:
               return sge_strnullcmp(x.str, y.str);
            case MT_HOST:
               return sge_strnullcmp(x.host, y.host);
            case MT_UINT32:
               return three_way(x.ul, y.ul);
            case MT_UINT64:
               return three_way(x.ul64, y.ul64);
            case MT_DOUBLE:
               return three_way(x.db, y.db);
            case MT_INT64:
               return three_way(x.l, y.l);
            case MT_BOOL:
               return three_way(x.b, y.b);
            case MT_REF:
               return three_way(x.ref, y.ref);
            default:
               return 0;
         }
      }

   } // namespace

   ocs::tom::SortOrderData *make_sort_order(const ocs::tom::FieldDescr *descr, const std::span<const SortKey> keys) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }
      auto *so = static_cast<ocs::tom::SortOrderData *>(malloc(sizeof(ocs::tom::SortOrderData) * (keys.size() + 1)));
      if (so == nullptr) {
         throw ListCoreError("no memory for a sort order");
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
      so[i].mt = MT_END;
      so[i].ad = 0;
      return so;
   }

   void free_sort_order(ocs::tom::SortOrderData *&so) noexcept {
      free(so);
      so = nullptr;
   }

   int sort_compare(const ocs::tom::ElementData *a, const ocs::tom::ElementData *b, const ocs::tom::SortOrderData *so) {
      int result = 0;
      for (int i = 0; result == 0 && so[i].nm != NoName; i++) {
         result = compare_field(a, b, so[i]) * so[i].ad;
      }
      return result;
   }

   void sort_list(ocs::tom::ListData *lp, const ocs::tom::SortOrderData *so) {
      if (lp == nullptr || lp->nelem < 2) {
         return;
      }
      if (so == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp->descr);
      }
      ocs::tom::detail::sort_elems(lp, [so](const ocs::tom::ElementData *a, const ocs::tom::ElementData *b) {
         return sort_compare(a, b, so) < 0;
      });
   }

   void sort_list_with(ocs::tom::ListData *lp, const std::function<bool(const ocs::tom::ElementData *,
                                                                        const ocs::tom::ElementData *)> &less) {
      if (lp == nullptr || lp->nelem < 2) {
         return;
      }
      ocs::tom::detail::sort_elems(lp, less);
   }

   void check_sort_order(const ocs::tom::FieldDescr *descr, const ocs::tom::SortOrderData *so) {
      if (so == nullptr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, descr);
      }
      for (int i = 0; so[i].nm != NoName; i++) {
         if (present_pos(descr, so[i].nm) != so[i].pos) {
            throw_field_exception(FieldException::Reason::AbsentField, so[i].nm, descr);
         }
      }
   }

   void insert_sorted(ocs::tom::ListData *lp, ocs::tom::ElementData *ep, const ocs::tom::SortOrderData *so) {
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
      ocs::tom::ElementData *before = lp->first;
      while (before != nullptr && sort_compare(ep, before, so) > 0) {
         before = before->next;
      }
      if (before != nullptr) {
         insert_elem(lp, before->prev, ep);
      } else {
         append_elem(lp, ep);
      }
   }

   void resort_elem(ocs::tom::ListData *lp, ocs::tom::ElementData *ep, const ocs::tom::SortOrderData *so) {
      if (so == nullptr) {
         // checked before the element leaves the list, so that it is not lost
         throw_field_exception(FieldException::Reason::NullElement, NoName, lp != nullptr ? lp->descr : nullptr);
      }
      dechain_elem(lp, ep);
      insert_sorted(lp, ep, so);
   }

} // namespace ocs::tom
