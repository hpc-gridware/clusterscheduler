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
 * @brief Helper operations on whole lists, in the core of the typed object model
 *
 * Parsing a string of names into a list, splitting a list by a condition,
 * removing the names two lists share, removing repeated keys, and counting the
 * nodes and leaves of a tree built of a list field. The functions are untyped
 * and throw; lString2List(), lString2ListNone(), lSplit(), lDiffListStr(),
 * lUniqStr(), lUniqHost(), lGetNumberOfNodes() and lGetNumberOfLeafs() of the
 * list library are thin wrappers that give back what they did before (N-L-1,
 * N-L-2). LEGACY-16 pins the results.
 */

#include "tom/ocs_Types.h"

namespace ocs::tom {

   /**
    * @brief Add the names of a string to a list, as lString2List() did
    *
    * The string is split at the delimiters; a name the list holds already is
    * skipped. A field that is neither a string nor a host field, or one the
    * descriptor does not hold, adds nothing.
    *
    * @param s the string; nullptr adds nothing
    * @param lp the list, created on the first name if nullptr
    * @param descr the descriptor of the elements
    * @param nm the string or host field the names go into
    * @param delimiters the delimiters, or nullptr for white space
    * @throw FieldException or ListCoreError if a name cannot be added; the list is freed and nullptr then
    */
   void string_to_list(const char *s, ocs::tom::ListData *&lp, const ocs::tom::FieldDescr *descr, int nm,
                       const char *delimiters);

   /**
    * @brief Add the names of a string to a list, with "none" for no name, as lString2ListNone() did
    *
    * As string_to_list(); a list holding only "none" (without regard to case
    * for a string field, as a host name for a host field) is freed.
    *
    * @param s the string; nullptr adds nothing
    * @param lp the list, created on the first name if nullptr
    * @param descr the descriptor of the elements
    * @param nm the string or host field the names go into
    * @param delimiters the delimiters, or nullptr for white space
    * @throw ListCoreError if "none" stands beside other names; the list is freed and
    *        nullptr then
    * @throw FieldException or ListCoreError as string_to_list()
    */
   void string_to_list_none(const char *s, ocs::tom::ListData *&lp, const ocs::tom::FieldDescr *descr, int nm,
                            const char *delimiters);

   /**
    * @brief Take the elements that do not fulfil a condition out of a list
    *
    * The elements keep their order in both lists.
    *
    * @param lp the list, not nullptr
    * @param cp the condition; nullptr is fulfilled by every element
    * @param name the name of the new list
    * @return the elements taken out, in a new list owned by the caller, or nullptr if there are none
    * @throw ListCoreError if there is no memory; no element is taken out then
    */
   ocs::tom::ListData *split_off(ocs::tom::ListData *lp, const ocs::tom::WhereData *cp, const char *name);

   /**
    * @brief Take an element and every element after it out of a list, as lDechainList() into a new list did
    *
    * The elements keep their order. Unlike lDechainList(), the hash tables of
    * the list stay and are kept up to date.
    *
    * @param lp the list, not nullptr
    * @param ep the first element to take out; an element of @p lp
    * @param name the name of the new list
    * @return the elements taken out, in a new list of the layout of @p lp owned by the caller
    * @throw FieldException with reason NullElement if @p ep is null, ForeignList if it is not in @p lp
    * @throw ListCoreError if there is no memory; no element is taken out then
    */
   ocs::tom::ListData *split_from(ocs::tom::ListData *lp, ocs::tom::ElementData *ep, const char *name);

   /**
    * @brief Split a list by a condition, as lSplit() did
    *
    * The elements that do not fulfil the condition go to @p ulp, created
    * with the name @p ulp_name (or "ulp") if it is nullptr, or are freed if
    * @p ulp is nullptr. A list that is left empty is freed: @p slp always,
    * @p ulp if it was created here.
    *
    * @param slp the list to split, or nullptr
    * @param ulp the list for the others, or nullptr to free them
    * @param ulp_name the name of a list created for @p ulp, or nullptr
    * @param cp the condition; nullptr is fulfilled by every element
    * @throw ListCoreError if there is no memory; nothing is changed then
    */
   void split_list(ocs::tom::ListData *&slp, ocs::tom::ListData **ulp, const char *ulp_name,
                   const ocs::tom::WhereData *cp);

   /**
    * @brief Remove the elements two lists share by a string key from both, as lDiffListStr() did
    *
    * For each element of @p lp1 whose key @p lp2 holds, the first element
    * with that key is removed from each list.
    *
    * @param lp1 the first list, or nullptr
    * @param lp2 the second list, or nullptr
    * @param nm the string field
    * @param free_empty whether a list left empty is freed and set to nullptr, as lDiffListStr() did
    * @throw FieldException if the elements do not hold the string field @p nm
    */
   void remove_common_str(ocs::tom::ListData *&lp1, ocs::tom::ListData *&lp2, int nm, bool free_empty);

   /**
    * @brief Sort a list by a string or host field and keep one element of each key, as lUniqStr() and lUniqHost() did
    *
    * The keys compare exactly, as strcmp() does; two null keys are equal.
    *
    * @param lp the list, not nullptr
    * @param nm the string or host field
    * @throw FieldException with reason ForeignField or AbsentField if the elements do not hold @p nm, WrongType if
    *        it is no string or host field
    * @throw ListCoreError if there is no memory
    */
   void unique_by_key(ocs::tom::ListData *lp, int nm);

   /**
    * @brief Count the nodes of a tree built of a list field, as lGetNumberOfNodes() did
    *
    * An element counts as one node plus the nodes of the elements in its list
    * field @p nm; an element without that field counts as one.
    *
    * @param ep the element, or nullptr to count the elements of @p lp
    * @param lp the list, used if @p ep is nullptr; nullptr counts nothing
    * @param nm the list field holding the children
    * @return the number of nodes
    */
   int count_nodes(const ocs::tom::ElementData *ep, const ocs::tom::ListData *lp, int nm) noexcept;

   /**
    * @brief Count the leaves of a tree built of a list field, as lGetNumberOfLeafs() did
    *
    * An element whose list field @p nm is empty is a leaf; an element without
    * that list field counts as nothing.
    *
    * @param ep the element, or nullptr to count the leaves under the elements of @p lp
    * @param lp the list, used if @p ep is nullptr; nullptr counts nothing
    * @param nm the list field holding the children
    * @return the number of leaves
    */
   int count_leaves(const ocs::tom::ElementData *ep, const ocs::tom::ListData *lp, int nm) noexcept;

} // namespace ocs::tom
