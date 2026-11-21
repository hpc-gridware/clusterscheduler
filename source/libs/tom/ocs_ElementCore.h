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
#include <type_traits>

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_FieldTraits.h"

namespace ocs::tom {

   /**
    * @brief The position of a field number in a descriptor, computed at run time
    *
    * The descriptor of every element of a type, full or reduced, has the full
    * length, so the position is the field number minus the first field number.
    *
    * Inline, because every access with a field known only at run time goes
    * through it: a subtraction, one range check against the descriptor header
    * and one compare.
    *
    * @param descr the descriptor, or nullptr
    * @param nm the field number
    * @return the position, or -1 if the descriptor is null or the field does not belong to the type
    */
   [[nodiscard]] inline int field_pos(const lDescr *descr, const int nm) noexcept {
      if (descr == nullptr) {
         return -1;
      }
      const int pos = nm - descr[0].nm;
      if (pos < 0 || static_cast<std::uint32_t>(pos) >= ocs::tom::descr_header_of(descr)->n || descr[pos].nm != nm) {
         return -1;
      }
      return pos;
   }

   /**
    * @brief The position of a field a descriptor holds: a field of its type that a reduction did not leave out
    *
    * The lookup lGetPosInDescr() is a thin wrapper over.
    *
    * @param descr the descriptor, or nullptr
    * @param nm the field number
    * @return the position, or -1 if the descriptor is null, the field belongs to another type or is absent
    */
   int present_pos(const lDescr *descr, int nm) noexcept;

   /**
    * @brief Record that a list or an element does not hold a field, as a soft path
    *
    * The reason is MissingField for a reduced descriptor that has the field
    * but not its value, and ForeignField for a descriptor of another type.
    * The old lookups and adds call it next to their message, the free forms
    * of ocs_FreeAccess.h through missing_key_field(); it is defined there.
    *
    * @param function the calling function, for the record
    * @param nm the field
    * @param descr the descriptor of the list or element, or nullptr
    */
   [[gnu::cold]] void report_missing_field(const char *function, int nm, const lDescr *descr) noexcept;

   /**
    * @brief The error of a field access the inline check refused: a null element, another type or an absent field
    *
    * The accessors with a field known at compile time test all three cases in
    * one inline condition and call this function only when it fails, so each
    * access site holds one call instead of a block per case. It determines
    * which case it was and throws.
    *
    * @param ep the element, or nullptr
    * @param nm the field
    * @param first_nm the first field number of the field's type, which an element of that type starts with
    * @throw FieldException with reason NullElement, ForeignField or AbsentField
    */
   [[noreturn, gnu::cold]] void throw_access_error(const lListElem *ep, int nm, int first_nm);


   /**
    * @brief The length of the type of a descriptor: the entries before the terminating one
    *
    * Counts the entries, so it also works for a descriptor that is not
    * registered; lCountDescr() is a thin wrapper over it.
    *
    * @param descr the descriptor, or nullptr
    * @return the length, or -1 for nullptr
    */
   int descr_length(const lDescr *descr) noexcept;

   /**
    * @brief The name of a field type of the list library
    * @param mt the field type, e.g. lUlongT
    * @return the name, e.g. "lUlongT", or "unknown multitype"
    */
   const char *type_name(int mt) noexcept;

   /**
    * @brief The type of a field a descriptor holds, as lGetType() gives it
    * @param descr the descriptor, or nullptr
    * @param nm the field number
    * @return the field type, or lEndT if the descriptor is null or does not hold the field
    */
   int field_type(const lDescr *descr, int nm) noexcept;

   /**
    * @brief How two descriptors differ: the same fields with the same types, holding the same of them
    *
    * Attributes other than the type are not compared, as lCompListDescr(),
    * a thin wrapper over it, did not compare them.
    *
    * @param a the first descriptor
    * @param b the second descriptor
    * @return 0 if equivalent, otherwise 1 for a null descriptor, 2 for an empty one, 3 for different ones
    */
   int layout_difference(const lDescr *a, const lDescr *b) noexcept;

   /**
    * @brief Whether two descriptors have the same layout, the entry by entry part of #same_layout
    * @param a the first descriptor, or nullptr
    * @param b the second descriptor, or nullptr
    * @return true if both are the same pointer, or have the same length and, at every position, the same
    *         field number, field type and slot offset; false if one or both are nullptr
    */
   bool same_layout_entries(const lDescr *a, const lDescr *b) noexcept;

   /**
    * @brief Whether elements of two descriptors keep their values at the same slots (CS-3112)
    *
    * An element's values are laid out by the descriptor it was created with:
    * a reduced descriptor has the full length of its type, marks the fields
    * it leaves out #OCS_CULL_ABSENT and packs the others into fewer slots. An
    * element linked into a list borrows the list's descriptor, so both must
    * have the same layout: the same length and, at every position, the same
    * field number, field type and slot offset. Attributes such as the hash
    * flags a list adds to a private copy of its descriptor are not compared.
    *
    * Equal reductions share one interned descriptor and a full one is the
    * static block of its type, so the common case is the same pointer, which
    * is decided inline without reading an entry.
    *
    * @param a the first descriptor, or nullptr
    * @param b the second descriptor, or nullptr
    * @return true if both are the same pointer or have the same layout; false if one or both are nullptr
    */
   [[nodiscard]] inline bool same_layout(const lDescr *a, const lDescr *b) noexcept {
      return a == b ? a != nullptr : same_layout_entries(a, b);
   }

   /**
    * @brief Whether two descriptors describe the same object type, either of them possibly reduced (CS-3125)
    *
    * A reduced descriptor has the full length of its type and marks the
    * fields it leaves out #OCS_CULL_ABSENT, so two descriptors of one type
    * have the same length and, at every position, the same field number and
    * field type - whichever fields each of them holds. Data received from a
    * client is checked with this before qmaster works on it: it may be
    * reduced, but it must be of the type the request is about.
    *
    * @param a the first descriptor, or nullptr
    * @param b the second descriptor, or nullptr
    * @return true if both describe the same type; false if they differ or one or both are nullptr
    */
   bool same_type(const lDescr *a, const lDescr *b) noexcept;

   /**
    * @brief The position of a field an element must hold, with the field type it must have
    *
    * The checks of an accessor whose field number is known only at run time,
    * in the order of the old interface: the element, the field's type, its
    * presence in a reduced element, and the field type.
    *
    * The one order of these checks for every run-time access: the old
    * getters and setters by name, the run-time members of the typed handles
    * (ConstElement<T>::get<V>(int), Element<T>::set(int, V)) and the free
    * accessors on raw pointers of ocs_Raw.h. Inline, so that a run-time access
    * costs no call on its good path.
    *
    * @param ep the element
    * @param nm the field number
    * @param type the field type the caller reads or writes
    * @return the field's position
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType
    */
   [[nodiscard]] inline int checked_pos(const lListElem *ep, const int nm, const FieldType type) {
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

   /**
    * @brief Store a string or host value, copying it and maintaining the hash table
    *
    * Does what the old setter does after its checks: compares with the old
    * value, removes the element from the field's hash table, copies the new
    * value, frees the old one and inserts the element again.
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
    * @brief Store a list into a list field, freeing the list it held before
    *
    * The list the field holds already is left as it is.
    *
    * @param ep the element, which holds the field
    * @param pos the field's position, of type List
    * @param value the new list, or nullptr; the element takes it over
    */
   void store_list(lListElem *ep, int pos, lList *value);

   /**
    * @brief Store an element into an object field, freeing the element it held before
    *
    * The element the field holds already is left as it is and is no error
    * (N-L-4, CS-3062); before, it was refused as bound.
    *
    * @param ep the element, which holds the field
    * @param pos the field's position, of type Object
    * @param value the new element, or nullptr; the element takes it over and marks it as a sub-object
    * @return true if stored or held already, false if @p value is bound to a list or another element
    */
   bool store_object(lListElem *ep, int pos, lListElem *value);

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

   // std::atomic_ref would be the idiomatic form of the two accessors below, but it cannot be used: FreeBSD 14
   // ships libc++ 18, which does not implement it at all (__cpp_lib_atomic_ref is not defined), so the build fails
   // on that platform. The builtins are understood by gcc and clang alike, need no library support and emit the
   // same instruction.
   static_assert(__atomic_always_lock_free(sizeof(lUlong64), nullptr),
                 "lUlong64 cannot be accessed atomically without a lock on this platform");
   static_assert(alignof(lMultiType) >= sizeof(lUlong64),
                 "a cull value is not aligned well enough to be accessed atomically");

   /**
    * @brief Read a 64 bit unsigned value without being able to observe it torn
    *
    * The core of lGetUlong64Atomic() and ocs::tom::get_atomic(). For a field that several threads write while
    * others read it, and that carries no invariant with any other field of the element (a time of last use).
    * That is a guarantee about a single load, not about ordering with anything else, so the access is relaxed.
    *
    * @param value the value slot of the field, of type Uint64
    * @return the value
    */
   inline std::uint64_t load_uint64_atomic(const lMultiType &value) noexcept {
      return __atomic_load_n(&value.ul64, __ATOMIC_RELAXED);
   }

   /**
    * @brief Write a 64 bit unsigned value without a reader being able to see it torn
    *
    * The core of lSetUlong64Atomic() and ocs::tom::set_atomic(). The store bypasses the hash table, so the field
    * must not be declared hashed; the callers check that.
    *
    * @param value the value slot of the field, of type Uint64
    * @param v the value to store
    */
   inline void store_uint64_atomic(lMultiType &value, const std::uint64_t v) noexcept {
      __atomic_store_n(&value.ul64, v, __ATOMIC_RELAXED);
   }

   /**
    * @brief Add an offset to a number field, as lAddUlong(), lAddUlong64() and lAddDouble() do
    *
    * The implementation of the old increments and of Element<T>::add<F>(). An
    * offset of 0 changes nothing, so the hash table of the list is not
    * touched. A hashed or, in a debug build, unique 32 or 64 bit field is
    * stored through #store_hashed_number, which keeps the hash table right;
    * a 32 bit sum wraps around as the old function's did.
    *
    * @tparam V the value type of the field: std::uint32_t, std::uint64_t or double
    * @param ep the element, which holds the field
    * @param pos the field's position in the element's descriptor
    * @param slot the field's value in the element
    * @param offset the amount to add
    * @return the new value
    * @throw FieldException with reason DuplicateKey in a debug build, when the field is unique and
    *        another element of the list holds the new value; the old value is kept
    * @see lAddUlong(), lAddUlong64(), lAddDouble(), Element<T>::add<F>(), ocs::tom::add<F>()
    */
   template <typename V>
      requires std::is_same_v<V, std::uint32_t> || std::is_same_v<V, std::uint64_t> || std::is_same_v<V, double>
   V add_number(lListElem *ep, const int pos, lMultiType &slot, const V offset) {
      if constexpr (std::is_same_v<V, double>) {
         slot.db += offset;
         return slot.db;
      } else if constexpr (std::is_same_v<V, std::uint32_t>) {
         if (offset == 0) {
            return slot.ul;
         }
         const V sum = slot.ul + offset;
         if (needs_hashed_store(ep, pos)) {
            store_hashed_number(ep, pos, sum);
         } else {
            slot.ul = sum;
         }
         return sum;
      } else {
         if (offset == 0) {
            return slot.ul64;
         }
         const V sum = slot.ul64 + offset;
         if (needs_hashed_store(ep, pos)) {
            store_hashed_number(ep, pos, sum);
         } else {
            slot.ul64 = sum;
         }
         return sum;
      }
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
    * @brief The first element of a list whose field holds a value, the lookup of the core
    *
    * The old lookups (lGetElemStrFirstRW() and its siblings), the typed
    * lookups and the free forms run on it. Through the field's table where the
    * list has one or reaches the hash threshold, otherwise by a linear search.
    * Host names are compared as everywhere in the cluster: the table by the
    * normalised name, the linear search without regard to case. A repeated key
    * of a unique field is a logic error (N-H-18): found while the field's table
    * is built, or by the linear search, which for a unique field goes on to
    * the end of the list.
    *
    * The caller has checked that the field at @p pos is of type @p T; the
    * search reads the slots without checking each element. Instantiated in the
    * core for the four key field types.
    *
    * @tparam T the field type: Uint32, Uint64, String or Host
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the value; a string or host name not nullptr
    * @param iterator the lookup's state, for #lookup_next
    * @return the element, or nullptr
    * @throw FieldException with reason DuplicateKey, naming type, field, key and list
    */
   template <FieldType T>
      requires KeyFieldType<T>
   lListElem *lookup_first(const lList *lp, int pos, KeyArgOf<T> value, const void **iterator);

   /**
    * @brief The next element of a list whose field holds a value, continuing #lookup_first
    *
    * Through the field's table where the list has one, otherwise by a linear
    * search from the element the iterator holds; the iterator is nullptr after
    * the last match. The old ...Next functions are thin wrappers over it.
    *
    * @tparam T the field type: Uint32, Uint64, String or Host
    * @param lp the list, not nullptr
    * @param pos the field's position in the list's descriptor
    * @param value the value; a string or host name not nullptr
    * @param iterator the lookup's state; nullptr gives no element
    * @return the element, or nullptr
    */
   template <FieldType T>
      requires KeyFieldType<T>
   lListElem *lookup_next(const lList *lp, int pos, KeyArgOf<T> value, const void **iterator);

   /**
    * @brief The first element of a list whose string field equals a value without regard to case
    *
    * A linear search, as lGetElemCaseStrRW() did; the old function is a thin
    * wrapper over it and the typed find_first_nocase().
    *
    * @param lp the list, or nullptr for no match
    * @param nm the field number
    * @param value the value, or nullptr for no match
    * @return the element, or nullptr
    * @throw FieldException with reason AbsentField if the list's elements do not hold the field, or
    *        WrongType if it is not a string field
    */
   lListElem *lookup_first_str_nocase(const lList *lp, int nm, const char *value);

   /**
    * @brief The first element of a list whose string field at a known position equals a value without regard to
    *        case, without the checks of #lookup_first_str_nocase
    * @param lp the list, or nullptr for no match
    * @param pos the position of a string field present in the list's descriptor
    * @param value the value, or nullptr for no match
    * @return the element, or nullptr
    */
   lListElem *lookup_first_str_nocase_at(const lList *lp, int pos, const char *value);

   /**
    * @brief The first element of a list whose string field equals a value, or starts with it if it ends in '*'
    *
    * A linear search, as lGetElemStrLikeRW() did; the old function is a thin
    * wrapper over it and the typed find_first_like(). Only a trailing '*' is a
    * pattern; everything else compares exactly.
    *
    * @param lp the list, or nullptr for no match
    * @param nm the field number
    * @param value the value or prefix pattern, or nullptr for no match
    * @return the element, or nullptr
    * @throw FieldException with reason AbsentField if the list's elements do not hold the field, or
    *        WrongType if it is not a string field
    */
   lListElem *lookup_first_str_like(const lList *lp, int nm, const char *value);

   /**
    * @brief The first element of a list whose string field at a known position equals a value, or starts with it
    *        if it ends in '*', without the checks of #lookup_first_str_like
    * @param lp the list, or nullptr for no match
    * @param pos the position of a string field present in the list's descriptor
    * @param value the value or prefix pattern, or nullptr for no match
    * @return the element, or nullptr
    */
   lListElem *lookup_first_str_like_at(const lList *lp, int pos, const char *value);

} // namespace ocs::tom
