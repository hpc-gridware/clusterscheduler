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
 * @brief The run-time core of the typed object model: positions, errors and the stores that need more than a write
 *
 * The typed accessors decide everything they can at compile time and are
 * inline. What is left for run time lives here, out of line: computing a
 * position from a field number known only at run time, the error path, and
 * the stores that copy a string, maintain a hash table or take over a list or
 * an element. The exception the core throws is in ocs_FieldException.h. The old interface of the list library uses the same functions,
 * so there is one implementation with two interfaces.
 */

#include <cstdint>

#include "cull/cull_list.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_FieldTraits.h"

namespace ocs::tom {

   /**
    * @brief The position of a field number in a descriptor, computed at run time
    *
    * The descriptor of every element of a type, full or reduced, has the full
    * length, so the position is the field number minus the first field number.
    *
    * @param descr the descriptor
    * @param nm the field number
    * @return the position, or -1 if the field does not belong to the type
    */
   int field_pos(const lDescr *descr, int nm) noexcept;

   /**
    * @brief The position of a field an element must hold, with the field type it must have
    *
    * The checks of an accessor whose field number is known only at run time,
    * in the order of the old interface: the element, the field's type, its
    * presence in a reduced element, and the field type.
    *
    * @param ep the element
    * @param nm the field number
    * @param type the field type the caller reads or writes
    * @return the field's position
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType
    */
   int checked_pos(const lListElem *ep, int nm, FieldType type);

   /**
    * @brief Store a string or host value, copying it and maintaining the hash table
    *
    * Does what the old setter does after its checks: compares with the old
    * value, removes the element from the field's hash table, copies the new
    * value, frees the old one and inserts the element again; the observe build
    * is told about the change.
    *
    * @param ep the element, which holds the field
    * @param pos the field's position, of type String or Host
    * @param value the new value, or nullptr; copied
    * @return true if stored, false if the copy failed (the old value is kept)
    */
   bool store_string(lListElem *ep, int pos, const char *value);

   /**
    * @brief Store a number into a hashed field, maintaining the hash table
    *
    * Unhashed number fields are stored inline by the typed setters; a hashed
    * one goes through here only when the value changes and the element's list
    * has built its table.
    *
    * @param ep the element, which holds the field
    * @param pos the field's position, of type Uint32 or Uint64
    * @param value the new value
    */
   void store_hashed_number(lListElem *ep, int pos, std::uint64_t value);

   /**
    * @brief Tell the observe build about a changed value; does nothing in other builds
    * @param ep the element
    * @param pos the field's position
    */
   void observe_value(const lListElem *ep, int pos);

   /**
    * @brief Store a list into a list field, freeing the list it held before
    * @param ep the element, which holds the field
    * @param pos the field's position, of type List
    * @param value the new list, or nullptr; the element takes it over
    */
   void store_list(lListElem *ep, int pos, lList *value);

   /**
    * @brief Store an element into an object field, freeing the element it held before
    * @param ep the element, which holds the field
    * @param pos the field's position, of type Object
    * @param value the new element, or nullptr; the element takes it over and marks it as a sub-object
    * @return true if stored, false if @p value is bound to a list or another element
    */
   bool store_object(lListElem *ep, int pos, lListElem *value);

   /**
    * @brief Set the error number of the list library for the calling thread
    *
    * The free setter of the typed interface keeps the error behaviour of the
    * old setter it replaces, including the error number it leaves behind.
    *
    * @param code an LE* code of cull_lerrno.h
    */
   void set_lerrno(int code);

} // namespace ocs::tom
