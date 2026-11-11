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
#include <string>

#include "cull/cull_list.h"
#include "cull/cull_listP.h"
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
    * @throw FieldException with reason DuplicateKey in a debug build, when the field is unique and
    *        another element of the list holds the value; the old value is kept
    */
   bool store_string(lListElem *ep, int pos, const char *value);

   /**
    * @brief Store a number into a hashed field, maintaining the hash table
    *
    * Unhashed number fields are stored inline by the typed setters; a hashed
    * one goes through here when #needs_hashed_store says so.
    *
    * @param ep the element, which holds the field
    * @param pos the field's position, of type Uint32 or Uint64
    * @param value the new value
    * @throw FieldException with reason DuplicateKey in a debug build, when the field is unique and
    *        another element of the list holds the value; the old value is kept
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

   /**
    * @brief Suspend the debug checks for duplicate keys in unique fields of the calling thread for a scope
    *
    * For the code that has to take a list as it is: reading a spool that
    * may hold duplicates written by an earlier version, which the qmaster
    * reports or repairs after reading, and copying or selecting from a list
    * that was checked when it was filled, where checking again would only
    * cost time. Nests; the checks resume when the outermost guard ends.
    */
   class UniqueCheckSuspension {
   public:
      /** @brief Suspend the checks */
      UniqueCheckSuspension() noexcept;

      /** @brief Resume the checks unless an outer guard still suspends them */
      ~UniqueCheckSuspension();

      UniqueCheckSuspension(const UniqueCheckSuspension &) = delete;
      UniqueCheckSuspension &operator=(const UniqueCheckSuspension &) = delete;
   };

   /**
    * @brief The check of #check_unique_on_add, out of line; called in a debug build only
    * @param lp the list
    * @param ep the element to add
    * @throw FieldException with reason DuplicateKey
    */
   void check_unique_on_add_debug(const lList *lp, const lListElem *ep);

   /**
    * @brief The check of #check_unique_on_set, out of line; called in a debug build only
    * @param ep the element
    * @param pos the field's position
    * @param value the new value, as for #check_unique_on_set
    * @throw FieldException with reason DuplicateKey
    */
   void check_unique_on_set_debug(const lListElem *ep, int pos, const void *value);

   /**
    * @brief Check the keys of an element against a list before it is added to it (N-H-18)
    *
    * For every unique field it looks the element's key up in the list, through
    * the field's hash table where the list has one or reaches the hash
    * threshold, building the table if needed, and linearly otherwise. Nothing
    * is changed, so a throw leaves list and element as they were. Does nothing
    * in a release build or while the checks are suspended.
    *
    * @param lp the list
    * @param ep the element to add, not yet in the list
    * @throw FieldException with reason DuplicateKey, naming type, field, key and list
    */
   inline void check_unique_on_add([[maybe_unused]] const lList *lp, [[maybe_unused]] const lListElem *ep) {
#ifdef ENABLE_DEBUG_CHECKS
      check_unique_on_add_debug(lp, ep);
#endif
   }

   /**
    * @brief Check a new value of a unique field against the element's list before it is stored (N-H-18)
    *
    * Like #check_unique_on_add, for one field of an element that is in a
    * list. Does nothing for an element in no list, for a field that is not
    * unique, in a release build and while the checks are suspended.
    *
    * @param ep the element
    * @param pos the field's position
    * @param value the new value: a char pointer for a string or host field, a pointer to the
    *        number for a Uint32 (lUlong) or Uint64 (lUlong64) field
    * @throw FieldException with reason DuplicateKey, naming type, field, key and list
    */
   inline void check_unique_on_set([[maybe_unused]] const lListElem *ep, [[maybe_unused]] const int pos,
                                   [[maybe_unused]] const void *value) {
#ifdef ENABLE_DEBUG_CHECKS
      check_unique_on_set_debug(ep, pos, value);
#endif
   }

   /**
    * @brief Whether a number field must be stored through #store_hashed_number
    *
    * When its list has built the field's table, and in a debug build also when
    * the field is unique and the element is in a list, so that the new value
    * is checked against the list.
    *
    * @param ep the element
    * @param pos the field's position
    * @return true if the store needs the hashed path
    */
   inline bool needs_hashed_store(const lListElem *ep, const int pos) {
      if (elem_hash_table(ep, pos) != nullptr) {
         return true;
      }
#ifdef ENABLE_DEBUG_CHECKS
      return mt_do_hashing(ep->descr[pos].mt) && mt_is_unique(ep->descr[pos].mt) && elem_list(ep) != nullptr;
#else
      return false;
#endif
   }

   /**
    * @brief End the process for an exception of the core, the way the old interface ends on a programming error
    *
    * Logs the function and the message as CRITICAL and aborts with a stack
    * trace (N-L-2).
    *
    * @param function the name of the old function
    * @param e the exception
    */
   [[noreturn, gnu::cold]] void legacy_abort(const char *function, const FieldException &e);

   /**
    * @brief A key of a hashed field as text, for a message
    * @param mt the field's multitype
    * @param key the key as the table stores it, not nullptr
    * @return the text
    */
   std::string key_text(int mt, const void *key);

   /**
    * @brief The first element of a list whose string field holds a value, the lookup of the core
    *
    * The old lookup lGetElemStrFirstRW() and the typed lookups run on it. A
    * repeated key of a unique field is a logic error (N-H-18): found while the
    * field's table is built, or by the linear search, which for a unique field
    * goes on to the end of the list.
    *
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the value, not nullptr
    * @param iterator the lookup's state, for lGetElemStrNextRW()
    * @return the element, or nullptr
    * @throw FieldException with reason DuplicateKey, naming type, field, key and list
    */
   lListElem *lookup_first_str(const lList *lp, int pos, const char *value, const void **iterator);

   /**
    * @brief The first element of a list whose 32 bit field holds a value, the lookup of the core
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the value
    * @param iterator the lookup's state, for lGetElemUlongNextRW()
    * @return the element, or nullptr
    * @throw FieldException with reason DuplicateKey, see #lookup_first_str
    */
   lListElem *lookup_first_ulong(const lList *lp, int pos, lUlong value, const void **iterator);

   /**
    * @brief The first element of a list whose 64 bit field holds a value, the lookup of the core
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the value
    * @param iterator the lookup's state, for lGetElemUlong64NextRW()
    * @return the element, or nullptr
    * @throw FieldException with reason DuplicateKey, see #lookup_first_str
    */
   lListElem *lookup_first_ulong64(const lList *lp, int pos, lUlong64 value, const void **iterator);

   /**
    * @brief The first element of a list whose host field holds a host, the lookup of the core
    *
    * Host names are compared as everywhere in the cluster: the table by the
    * normalised name, the linear search without regard to case.
    *
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the host name, not nullptr
    * @param iterator the lookup's state, for lGetElemHostNextRW()
    * @return the element, or nullptr
    * @throw FieldException with reason DuplicateKey, see #lookup_first_str
    */
   lListElem *lookup_first_host(const lList *lp, int pos, const char *value, const void **iterator);

} // namespace ocs::tom
