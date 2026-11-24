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

/** @file
 * @brief Sorting in the core of the typed object model
 *
 * Building a sort order, comparing two elements by it, sorting a list,
 * inserting into a sorted list and moving a changed element to its place. The
 * functions are untyped and throw on every error; the typed sorting of
 * ocs_List.h calls them after checking at compile time, and the raw interface
 * (ocs_Raw.h) calls them for an object type known only at run time.
 *
 * Sorting is stable: elements with equal keys keep their order.
 */

#include <functional>
#include <span>

#include "tom/ocs_Types.h"

namespace ocs::tom {

   /**
    * @brief One criterion of a sort order to be built
    */
   struct SortKey {
      int nm;          ///< the field
      bool ascending;  ///< true for ascending, false for descending
   };

   /**
    * @brief Build a sort order
    * @param descr the descriptor of the type to be sorted
    * @param keys the criteria, the first deciding first
    * @return the order, owned by the caller, freed with free_sort_order()
    * @throw FieldException with reason NullDescriptor if @p descr is null, ForeignField for a field the type does
    *        not hold, AbsentField for a field a reduced descriptor does not hold
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::SortOrderData *make_sort_order(const ocs::tom::FieldDescr *descr, std::span<const SortKey> keys);

   /**
    * @brief Free a sort order, as lFreeSortOrder() does
    * @param so the order, or nullptr; nullptr afterwards
    */
   void free_sort_order(ocs::tom::SortOrderData *&so) noexcept;

   /**
    * @brief Compare two elements by a sort order, as lSortCompare() did
    *
    * Strings and host names compare as sge_strnullcmp() does, a missing one
    * before every other. A field of a type the order cannot compare counts as
    * equal.
    *
    * @param a the first element
    * @param b the second element
    * @param so the order
    * @return negative if @p a comes first, positive if @p b comes first, 0 if they are equal
    */
   int sort_compare(const ocs::tom::ElementData *a, const ocs::tom::ElementData *b, const ocs::tom::SortOrderData *so);

   /**
    * @brief Sort a list by a sort order, keeping equal elements in their order
    *
    * The hash tables of the list are rebuilt afterwards, as lSortList() did.
    *
    * @param lp the list, or nullptr, which is sorted already
    * @param so the order
    * @throw FieldException with reason NullElement if @p so is null and the list has two elements or more
    * @throw ListCoreError if there is no memory; the list is unchanged then
    */
   void sort_list(ocs::tom::ListData *lp, const ocs::tom::SortOrderData *so);

   /**
    * @brief Sort a list by a comparison, keeping equal elements in their order
    *
    * For orders the fields of a sort order do not express. The hash tables of
    * the list are rebuilt afterwards.
    *
    * @param lp the list, or nullptr, which is sorted already
    * @param less true if the first element comes before the second; a strict weak ordering
    * @throw ListCoreError if there is no memory; the list is unchanged then
    * @throw whatever @p less throws; the list is unchanged then
    */
   void sort_list_with(ocs::tom::ListData *lp, const std::function<bool(const ocs::tom::ElementData *,
                                                                        const ocs::tom::ElementData *)> &less);

   /**
    * @brief Check that the elements of a list can be compared by a sort order
    * @param descr the descriptor of the list
    * @param so the order
    * @throw FieldException with reason NullElement if @p so is null, AbsentField for a field of the order that
    *        @p descr does not hold
    */
   void check_sort_order(const ocs::tom::FieldDescr *descr, const ocs::tom::SortOrderData *so);

   /**
    * @brief Insert a free element at the place a sort order gives it, as lInsertSorted() did
    *
    * The element goes in front of the first element that does not come before
    * it, or to the end.
    *
    * @param lp the list, sorted by @p so, which takes the element over
    * @param ep the element
    * @param so the order
    * @throw FieldException with reason NullList, NullElement, WrongLayout if @p ep was created with a
    *        descriptor of another layout than the list's, BoundElement, or DuplicateKey in a debug build;
    *        nothing is changed then
    */
   void insert_sorted(ocs::tom::ListData *lp, ocs::tom::ElementData *ep, const ocs::tom::SortOrderData *so);

   /**
    * @brief Move an element of a sorted list to the place its changed key gives it, as lResortElem() did
    * @param lp the list, sorted by @p so apart from @p ep
    * @param ep the element, in @p lp
    * @param so the order
    * @throw FieldException with reason NullList, NullElement or ForeignList; nothing is changed then
    */
   void resort_elem(ocs::tom::ListData *lp, ocs::tom::ElementData *ep, const ocs::tom::SortOrderData *so);

} // namespace ocs::tom
