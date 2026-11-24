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
 * @brief Selections in the core of the typed object model
 *
 * Building field selections (ocs::tom::WhatData) and conditions (ocs::tom::WhereData) from
 * their parts, evaluating a condition on an element, finding the elements of a
 * list that fulfil one, and selecting a reduced copy of a list. The functions
 * are untyped and throw on every error; the typed selections of ocs_Select.h
 * call them after checking at compile time, and the raw interface (ocs_Raw.h)
 * calls them for an object type known only at run time.
 *
 * Errors:
 * - a programming error throws FieldException: no descriptor (NullDescriptor),
 *   a field the descriptor does not hold (ForeignField), a value of another
 *   type than the field or a nested condition on a field that is not a list
 *   (WrongType), a null list or element (NullList, NullElement), a null
 *   operand of a combination (NullArgument is not a reason, so NullElement is
 *   used for it);
 * - a failure at run time throws ListCoreError, whose message says what failed.
 */

#include <cstdint>
#include <span>

#include "tom/ocs_Types.h"

namespace ocs::tom {

   /**
    * @brief The comparison of a field with a value; the operators of lWhere() in brackets
    */
   enum class CompareOp : std::uint8_t {
      Equal,          ///< `==`
      NotEqual,       ///< `!=`
      LowerEqual,     ///< `<=`
      Lower,          ///< `<`
      GreaterEqual,   ///< `>=`
      Greater,        ///< `>`
      Bitmask,        ///< `m=`: every bit of the value is set in a 32 bit field
      NoCase,         ///< `c=`: a string equals without regard to case
      Pattern,        ///< `p=`: a string matches a shell pattern
      SameHost        ///< `h=`: a string names the same host
   };

   /**
    * @brief The combination of conditions; the operators of lWhere() in brackets
    */
   enum class LogicOp : std::uint8_t {
      And,   ///< `&&`
      Or,    ///< `||`
      Not    ///< `!`
   };

   /**
    * @brief One field of a selection to be built, with the selection inside it
    */
   struct WhatEntry {
      int nm;              ///< the field
      ocs::tom::WhatData *sub;   ///< the selection inside a list or object field, or nullptr; owned by the entry
   };

   /**
    * @brief Build a selection of fields of one type
    *
    * The fields are brought into the order of the descriptor and a field given
    * twice is kept once, with the first sub-selection given for it, as lWhat()
    * did. A sub-selection is taken for any field, as lWhat() took it; the typed
    * selections allow one only on a list or object field.
    *
    * @param descr the descriptor of the type
    * @param entries the fields; the function takes the sub-selections over, also when it throws
    * @return the selection, owned by the caller, freed with lFreeWhat()
    * @throw FieldException with reason NullDescriptor if @p descr is null, ForeignField for a field it does not
    *        hold or for no field at all, AbsentField for a field a reduced descriptor does not hold
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhatData *make_what(const ocs::tom::FieldDescr *descr, std::span<const WhatEntry> entries);

   /**
    * @brief Build the selection of every field or of none
    * @param pos WHAT_ALL or WHAT_NONE
    * @return the selection, owned by the caller, freed with lFreeWhat()
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhatData *make_what_special(int pos);

   /**
    * @brief Bring the fields of a selection into the order of the descriptor and keep each once
    * @param ep the selection, terminated by NoName; may be nullptr
    */
   void sort_what_by_pos(ocs::tom::WhatData *ep) noexcept;

   /**
    * @brief Build the comparison of a field with a value
    *
    * The value of a string or host field is copied. The operator is not
    * checked against the field type, as lWhere() did not check it; lCompare()
    * reports an operator that does not apply to the field's type when it is
    * evaluated.
    *
    * @param descr the descriptor of the type
    * @param nm the field
    * @param op the operator
    * @param value_type the list library type of @p value (MT_UINT32, MT_STRING, ...); MT_STRING is taken
    *        for a host field
    * @param value the value; for a string or host only the pointer, which may be nullptr
    * @return the condition, owned by the caller, freed with lFreeWhere()
    * @throw FieldException with reason NullDescriptor, ForeignField, AbsentField, or WrongType if @p value_type
    *        is not the field's type
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhereData *make_compare(const ocs::tom::FieldDescr *descr, int nm, CompareOp op, int value_type,
                                     const ocs::tom::FieldValue &value);

   /**
    * @brief Build the condition that an element of a list field fulfils a condition
    * @param descr the descriptor of the type
    * @param nm the list field
    * @param sub the condition on the elements of the list; taken over, also when the function throws
    * @return the condition, owned by the caller, freed with lFreeWhere()
    * @throw FieldException with reason NullDescriptor, ForeignField, AbsentField, or WrongType if the field is
    *        not a list
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhereData *make_subscope(const ocs::tom::FieldDescr *descr, int nm, ocs::tom::WhereData *sub);

   /**
    * @brief Combine conditions with AND, OR or NOT
    * @param op the combination
    * @param first the first operand; taken over, also when the function throws
    * @param second the second operand, nullptr for Not; taken over, also when the function throws
    * @return the condition, owned by the caller, freed with lFreeWhere()
    * @throw FieldException with reason NullElement if an operand the operator needs is null
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhereData *make_logic(LogicOp op, ocs::tom::WhereData *first, ocs::tom::WhereData *second);

   /**
    * @brief Whether an element fulfils a condition, as lCompare() decided it
    *
    * A comparison that cannot be made, a null string or an operator that does
    * not apply to the field's type, is false, as lCompare() gave it.
    *
    * @param ep the element
    * @param cp the condition; nullptr is fulfilled by every element
    * @return true if @p ep fulfils @p cp
    * @throw FieldException with reason NullElement if @p ep is null
    */
   bool compare(const ocs::tom::ElementData *ep, const ocs::tom::WhereData *cp);

   /**
    * @brief The first element of a list that fulfils a condition
    * @param lp the list
    * @param cp the condition, or nullptr for the first element
    * @return the element, or nullptr if none fulfils it
    * @throw FieldException with reason NullList if @p lp is null
    */
   ocs::tom::ElementData *find_first(const ocs::tom::ListData *lp, const ocs::tom::WhereData *cp);

   /**
    * @brief The last element of a list that fulfils a condition
    * @param lp the list
    * @param cp the condition, or nullptr for the last element
    * @return the element, or nullptr if none fulfils it
    * @throw FieldException with reason NullList if @p lp is null
    */
   ocs::tom::ElementData *find_last(const ocs::tom::ListData *lp, const ocs::tom::WhereData *cp);

   /**
    * @brief The next element after an element that fulfils a condition
    * @param ep the element to start after
    * @param cp the condition, or nullptr for the next element
    * @return the element, or nullptr if none fulfils it
    * @throw FieldException with reason NullElement if @p ep is null
    */
   ocs::tom::ElementData *find_next(const ocs::tom::ElementData *ep, const ocs::tom::WhereData *cp);

   /**
    * @brief The element before an element that fulfils a condition
    * @param ep the element to start before
    * @param cp the condition, or nullptr for the previous element
    * @return the element, or nullptr if none fulfils it
    * @throw FieldException with reason NullElement if @p ep is null
    */
   ocs::tom::ElementData *find_prev(const ocs::tom::ElementData *ep, const ocs::tom::WhereData *cp);

   /**
    * @brief Copy the elements of a list that fulfil a condition, reduced to a selection of fields
    *
    * The result is empty if no element fulfils the condition; lSelect() turns
    * that into nullptr.
    *
    * @param name the name of the new list, or nullptr for the default name of a list
    * @param lp the list
    * @param cp the condition, or nullptr for every element
    * @param what the selection of fields
    * @param hash true if the new list may be hashed
    * @param skip_no_transfer true if fields marked MT_NO_TRANSFER are not copied
    * @return the new list, owned by the caller
    * @throw FieldException with reason NullList if @p lp is null, NullElement if @p what is null
    * @throw ListCoreError if the reduction or a copy fails
    */
   ocs::tom::ListData *select_list(const char *name, const ocs::tom::ListData *lp, const ocs::tom::WhereData *cp,
                                   const ocs::tom::WhatData *what, bool hash,
                      bool skip_no_transfer);

   /**
    * @brief Free a selection and its nested selections, as lFreeWhat() does
    * @param ep the selection, or nullptr; nullptr afterwards
    */
   void free_what(ocs::tom::WhatData *&ep) noexcept;

   /**
    * @brief A deep copy of a selection, as lCopyWhat() makes it; the cached reduction is not copied
    * @param ep the selection
    * @return the copy, owned by the caller
    * @throw FieldException with reason NullElement if @p ep is null
    * @throw ListCoreError if there is no memory
    */
   ocs::tom::WhatData *copy_what(const ocs::tom::WhatData *ep);

   /**
    * @brief Free a condition with its values and operands, as lFreeWhere() does
    * @param cp the condition, or nullptr; nullptr afterwards
    */
   void free_where(ocs::tom::WhereData *&cp) noexcept;

   /**
    * @brief A deep copy of a condition, as lCopyWhere() makes it; a reference value is not copied
    * @param cp the condition, or nullptr
    * @return the copy, owned by the caller, or nullptr for nullptr
    * @throw ListCoreError if there is no memory or the condition is corrupt
    */
   ocs::tom::WhereData *copy_where(const ocs::tom::WhereData *cp);

   /**
    * @brief Copy the fields a selection names from one element to another, as lCopyElemPartialPack() without a
    *        pack buffer does
    * @param dst the element copied to; it must hold every selected field
    * @param src the element copied from; it must hold every selected field
    * @param what the selection; ALL copies every present field, NONE none
    * @param hash true to build the hash tables of copied sub-lists
    * @param skip_no_transfer true if fields marked MT_NO_TRANSFER are not copied
    * @throw FieldException with reason NullElement if an argument is null
    * @throw ListCoreError if a selected field is absent or a copy fails
    */
   void copy_elem_partial(ocs::tom::ElementData *dst, const ocs::tom::ElementData *src, const ocs::tom::WhatData *what,
                          bool hash, bool skip_no_transfer);

   /**
    * @brief A copy of one element reduced to a selection of fields, as lSelectElemPack() without a pack buffer
    * @param ep the element
    * @param what the selection of fields
    * @param hash true to build the hash tables of copied sub-lists
    * @param skip_no_transfer true if fields marked MT_NO_TRANSFER are not copied
    * @return the copy, a free element owned by the caller
    * @throw FieldException with reason NullElement if an argument is null
    * @throw ListCoreError if the reduction or a copy fails
    */
   ocs::tom::ElementData *select_elem(const ocs::tom::ElementData *ep, const ocs::tom::WhatData *what, bool hash,
                                      bool skip_no_transfer);


   /**
    * @brief The number of fields a selection takes from a descriptor, as lCountWhat() counted them
    *
    * @param enp the selection
    * @param dp the descriptor
    * @return the number of fields, every present one for WHAT_ALL, 0 for WHAT_NONE; -1 if an argument is null
    *         or the descriptor cannot be counted
    */
   int count_what(const ocs::tom::WhatData *enp, const ocs::tom::FieldDescr *dp) noexcept;

   /**
    * @brief Lay out the descriptor a selection reduces another one to, as lPartialDescr() did
    *
    * The result has the full length of the type, with the selected fields in
    * their slots and the others absent (see ocs::tom::descr_layout()). The source
    * may itself be a reduction; every selected field must be present in it.
    *
    * @param enp the selection
    * @param sdp the source descriptor
    * @param ddp receives the entries; room for descr_length(sdp) + 1 of them
    * @param indexp set to the number of entries written, without the terminating one
    * @return 0 on success, -1 on error
    */
   int partial_descr(const ocs::tom::WhatData *enp, const ocs::tom::FieldDescr *sdp, ocs::tom::FieldDescr *ddp,
                     int *indexp) noexcept;

   /**
    * @brief Whether every field a condition compares belongs to a type
    *
    * The condition on the elements of a list field (`->` in lWhere(), any() in
    * the typed form) is not looked into: its fields belong to the sub-type.
    *
    * @param cp the condition, or nullptr
    * @param descr the descriptor of the type
    * @return true if every field compared at the top belongs to @p descr, and for nullptr
    */
   bool condition_fits(const ocs::tom::WhereData *cp, const ocs::tom::FieldDescr *descr) noexcept;

   /**
    * @brief Merge two selections into one selecting the fields of both, as lMergeWhat() does
    *
    * ALL with anything is ALL, NONE adds nothing. A field both select keeps
    * the merge of both nested selections; if either selects the whole sub-list
    * or sub-object, the merge does too. A field only @p what2 selects keeps its
    * nested selection. The result is sorted by position.
    *
    * Before CS-3077, lMergeWhat() kept only the nested selection of @p what1
    * for a field both select, dropped the nested selection of a field only
    * @p what2 selects, and took the NONE of @p what2 for a field.
    *
    * @param what1 the first selection, or nullptr; taken over
    * @param what2 the second selection, or nullptr; taken over
    * @return the merged selection, owned by the caller; nullptr if both are nullptr
    * @throw ListCoreError if there is no memory; both selections are freed then
    */
   ocs::tom::WhatData *merge_what(ocs::tom::WhatData *what1, ocs::tom::WhatData *what2);

   /**
    * @brief Give a selected list or object field a nested selection, as lWhatSetSubWhat() does
    *
    * A nested selection held before is freed.
    *
    * @param what the selection, not nullptr
    * @param nm the field
    * @param sub the nested selection, or nullptr for the whole sub-list or sub-object; taken over
    * @throw FieldException with reason ForeignField if @p what does not select @p nm; @p sub is freed then
    */
   void set_sub_what(ocs::tom::WhatData *what, int nm, ocs::tom::WhatData *sub);

   /**
    * @brief The shared reduced descriptor a selection makes of a type, as lGetReducedDescr() gives it
    * @param descr the full descriptor of the type
    * @param what the selection
    * @return the descriptor, holding a reference for the caller, who gives it up with
    *         ocs::tom::release_descr(); nullptr if the selection selects nothing of the type
    */
   const ocs::tom::FieldDescr *reduced_descr(const ocs::tom::FieldDescr *descr,
                                             const ocs::tom::WhatData *what) noexcept;

} // namespace ocs::tom
