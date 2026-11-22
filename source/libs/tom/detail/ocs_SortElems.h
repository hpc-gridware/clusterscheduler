#pragma once
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

/**
 * @file
 * @brief The one sort of the list library: a stable sort of the elements of a list by a comparison
 *
 * A template, so that a comparison known at compile time (the typed List<T>::sort<K...>()) is inlined
 * into the sort instead of being called through a function pointer or std::function per comparison.
 */

#include <algorithm>
#include <cstddef>
#include <new>
#include <vector>

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/ocs_ListCore.h"

namespace ocs::tom::detail {

   /**
    * @brief Sort the elements of a list with two elements or more, keeping equal ones in their order
    *
    * The elements are sorted as an array of pointers and linked again in the new order; the hash tables are
    * rebuilt afterwards, as lSortList() did. If the comparison throws, the list is left as it was.
    *
    * @tparam Less the type of the comparison
    * @param lp the list, with two elements or more
    * @param less true if the first element comes before the second; a strict weak ordering
    * @throw ListCoreError if there is no memory; the list is unchanged then
    * @throw whatever @p less throws; the list is unchanged then
    */
   template <typename Less>
   void sort_elems(lList *lp, const Less &less) {
      std::vector<lListElem *> elems;
      try {
         elems.reserve(lp->nelem);
      } catch (const std::bad_alloc &) {
         throw ListCoreError("no memory to sort a list");
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

      hash_recreate_after_sort(lp);
   }

} // namespace ocs::tom::detail
