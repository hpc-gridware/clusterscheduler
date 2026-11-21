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
 * @brief The run-time typed interface of the typed object model: checked access, handles and owners on raw pointers
 *
 * Generic code - spool walkers, the event master, GDI transport, qconf - works
 * on elements and lists whose object type is known only at run time: the
 * descriptor inside the element is the type. Such code keeps the raw pointer
 * as the generic form and gets here what the old interface lacks:
 *
 * - **Checked access.** `ocs::tom::get<V>(ep, nm)`, `set(ep, nm, v)`, `has(ep, nm)`,
 *   `get_or<V>(ep, nm, fallback)`, `exchange(ep, nm, list)` and `take_raw_object(ep, nm)`
 *   take the field as an `int` argument and the value type `V` from the caller.
 *   `V` names the field type: one of the nine plain types (`std::uint32_t`,
 *   `std::uint64_t`, `std::int32_t`, `std::int64_t`, `double`, `bool`,
 *   `const char *`, `Host`, `void *`), or for a list or object field the raw
 *   pointer (`const lList *`, `lList *`, `const lListElem *`, `lListElem *`) or
 *   the raw handle (`ConstRawList`, `RawList`, `ConstRawElement`,
 *   `RawElement`); the mutable forms only from a mutable element. Every
 *   mismatch throws FieldException instead of aborting the process.
 * - **Ownership.** `RawElementPtr` and `RawListPtr` own an element or a list of
 *   any type, as `ElementPtr<T>` and `ListPtr<T>` do for one type;
 *   `RawWherePtr` and `RawWhatPtr` own a condition and a selection.
 *   `adopt<T>()` hands an owner over to the typed owner after a check, with
 *   the strong guarantee.
 * - **Generic list and selection ops.** `create_raw_elem`, `create_raw_list`,
 *   `clone`, `append`, `insert_after`, `take`, `erase`, `splice`, `size`,
 *   `find_first<V>`, `select`, `matches`, `unpack_raw_list`, `unpack_raw_elem`,
 *   `write_to`: thin fronts of the core in ocs_ListCore.h, ocs_SelectCore.h,
 *   ocs_PackCore.h and ocs_PrintCore.h.
 * - **Run-time builders.** `where_compare<V>`, `what_of`, `what_all`: a condition
 *   or a field selection from fields and values known only at run time,
 *   without a format string; the results of lWhere(), lIntVector2What() and
 *   lWhatAll().
 * - **Selections with the old contract.** `select_as`, `select_elem_as`,
 *   `select_into_descr`, `select_elem_into_descr`, `select_pack`: the results
 *   of lSelect() and its kin (result name, no list for no match, a reduced
 *   descriptor of the caller, straight into a pack buffer), for the GDI and
 *   event paths whose result goes onto the wire; they throw nothing.
 * - **Infrastructure.** `free_raw_list`, `free_raw_elem` (process-lifetime raw
 *   holders, freed with the abort of the old functions), `remaining`,
 *   `is_free`, `make_hashable`, `release_descr`, `descr_registry_shared`:
 *   what code outside the list library needs of the storage layer and the
 *   descriptor registry, without their private headers.
 * - **Member form.** `ConstRawElement`/`RawElement` and `ConstRawList`/`RawList`
 *   carry the same operations as methods, after the model of the typed
 *   handles, with the field as a method argument instead of a template
 *   argument: `ep.get<JB_job_number>()` becomes `ep.get<std::uint32_t>(nm)`.
 *   The methods only forward to the free functions, so both forms give the same
 *   result and throw the same reason; an empty handle is a null pointer to the
 *   free function and so reports NullElement (or NullList), not EmptyHandle.
 *
 * Error contract (FieldException::Reason):
 *
 * | situation                                        | get, set, exchange, take_raw_object | has   | get_or    |
 * |--------------------------------------------------|-------------------------------------|-------|-----------|
 * | element nullptr                                  | NullElement                         | false | fallback  |
 * | field not of the element's type                  | ForeignField                        | false | fallback  |
 * | field of the type, absent in the reduced layout  | AbsentField                         | false | fallback  |
 * | V is not the field's type                        | WrongType                           | -     | WrongType |
 * | list/object of another type than the model names | WrongSubType                        | -     | -         |
 * | object bound in a list or another object already | BoundElement                        | -     | -         |
 * | string copy failed                               | OutOfMemory                         | -     | -         |
 * | unique value held twice (debug build)            | DuplicateKey                        | -     | -         |
 *
 * The checks of get, set, exchange and take_raw_object run in the order of
 * #checked_pos: NullElement, ForeignField, AbsentField, WrongType. get_or
 * checks the type before the presence, so that a wrong `V` throws also for
 * a reduced element that lacks the field. The sub-type check needs the
 * installed object model (ocs_Model.h); without it, or for a field the model
 * names no sub-type for, any list or object is accepted.
 *
 * List ops: a mutating op on a null list throws NullList (append, insert_after,
 * take, erase, splice); read-only ops treat nullptr as the empty list (size 0,
 * find_first nullptr, select and clone an empty owner). Further reasons:
 * WrongType (an element or list of another type), WrongLayout (CS-3112),
 * BoundElement, ForeignList, NullKey (find_first with a null string key),
 * NullDescriptor (create with nullptr), DuplicateKey (debug build).
 *
 * Three names of the design differ, because the core of ocs_ListCore.h and
 * ocs_PackCore.h has functions of the same signature that return raw
 * pointers: create_raw_elem() (core: create_elem()), take_raw_object() (core:
 * take_object(ep, pos), which takes a position, not a field number) and
 * unpack_raw_list() (core: unpack_list()); create_raw_list() and
 * unpack_raw_elem() follow for symmetry. The owners carry `create()` as well.
 *
 * Typed to raw: the owners take a typed owner by move, `RawListPtr raw(std::move(typed))`, which cannot fail
 * and needs no check. The constructor is explicit: an implicit one would make a temporary raw owner at every
 * parameter of type `RawElementPtr &&` or `RawListPtr &&`, which frees the element when the op throws and
 * leaves the caller's typed owner empty. The list ops and adopt<T>() therefore have overloads for the typed
 * owners (`ElementPtr<U> &&`, `ListPtr<U> &&`) with the same strong guarantee: on a throw the element or list
 * is back in the caller's typed owner. The by-value setters `set(ep, nm, RawElementPtr)` and
 * `set(ep, nm, RawListPtr)` have no typed overload; a typed owner is converted explicitly,
 * `set(ep, nm, RawElementPtr(std::move(typed)))`, which states at the call that the ownership passes there
 * and that a throw frees the element, as for any by-value owner. Raw to typed: `adopt<T>(std::move(raw))`,
 * checked.
 *
 * A bound element (one in a list or in an object field) in an owner is a misuse that the ops report with
 * BoundElement. They differ in what happens to the owner: append(), insert_after() and push_back() change
 * nothing, so the owner still holds the bound element and the caller has to release() it; set() with an
 * object releases the owner on every path, also when another check throws first, so that the parameter
 * never frees a bound element - the element stays where it is bound, as the typed Element<T>::set() and
 * the free typed set() leave it on BoundElement.
 *
 * Walking a list as a range: erase() and take() of the current element invalidate the range iterator, which
 * steps through the element's successor pointer. Fetch the successor first and walk by hand, as with the
 * typed handles: `for (RawElement ep = list.front(); ep;) { RawElement next = ep.next(); ...; ep = next; }`.
 * The handles walk as the typed ones do: front() and back() of a list, next() and prev() of an element.
 *
 * What needs the field as a template argument stays typed: sorting, typed
 * conditions and selections, layouts, keyed merges.
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_Types.h"
#include "tom/detail/ocs_WhatP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_SelectCore.h"

namespace ocs::tom {

   class ConstRawElement;
   class RawElement;
   class RawElementPtr;
   class ConstRawList;
   class RawList;
   class RawListPtr;

   namespace detail {

      /**
       * @brief A plain value type of the run-time accessors: a number, bool, a string, a host name or a reference
       * @tparam V the value type
       */
      template <typename V>
      concept RawPlainValue = std::is_same_v<V, std::uint32_t> || std::is_same_v<V, std::uint64_t> ||
                              std::is_same_v<V, std::int32_t> || std::is_same_v<V, std::int64_t> ||
                              std::is_same_v<V, double> || std::is_same_v<V, bool> ||
                              std::is_same_v<V, const char *> || std::is_same_v<V, Host> ||
                              std::is_same_v<V, void *>;

      /**
       * @brief A value type that reads a list field without the right to change the list
       * @tparam V the value type
       */
      template <typename V>
      concept RawConstListValue = std::is_same_v<V, const lList *> || std::is_same_v<V, ConstRawList>;

      /**
       * @brief A value type that reads an object field without the right to change the object
       * @tparam V the value type
       */
      template <typename V>
      concept RawConstObjectValue = std::is_same_v<V, const lListElem *> || std::is_same_v<V, ConstRawElement>;

      /**
       * @brief A value type that reads a list field for writing
       * @tparam V the value type
       */
      template <typename V>
      concept RawMutableListValue = std::is_same_v<V, lList *> || std::is_same_v<V, RawList>;

      /**
       * @brief A value type that reads an object field for writing
       * @tparam V the value type
       */
      template <typename V>
      concept RawMutableObjectValue = std::is_same_v<V, lListElem *> || std::is_same_v<V, RawElement>;

      /**
       * @brief A value type that can be read from a read-only element
       * @tparam V the value type
       */
      template <typename V>
      concept RawConstValue = RawPlainValue<V> || RawConstListValue<V> || RawConstObjectValue<V>;

      /**
       * @brief A value type that can be read from a mutable element: every one of a read-only element and the
       *        mutable list and object forms
       * @tparam V the value type
       */
      template <typename V>
      concept RawMutableValue = RawConstValue<V> || RawMutableListValue<V> || RawMutableObjectValue<V>;

      /**
       * @brief A value type of a key that find_first() looks up: a 32 or 64 bit number, a string or a host name
       * @tparam V the value type
       */
      template <typename V>
      concept RawKeyValue = std::is_same_v<V, std::uint32_t> || std::is_same_v<V, std::uint64_t> ||
                            std::is_same_v<V, const char *> || std::is_same_v<V, Host>;

      /**
       * @brief The field type a value type of the run-time accessors reads or writes, including lists and objects
       * @tparam V the value type, a #RawMutableValue
       * @return the field type
       */
      template <typename V>
         requires RawMutableValue<V>
      consteval FieldType raw_field_type_for() {
         if constexpr (RawConstListValue<V> || RawMutableListValue<V>) {
            return FieldType::List;
         } else if constexpr (RawConstObjectValue<V> || RawMutableObjectValue<V>) {
            return FieldType::Object;
         } else {
            return field_type_for<V>();
         }
      }

      /**
       * @brief The value of a slot as a value type of the run-time accessors, for a field checked against it
       *
       * Plain values come from plain_value(), the core of the typed run-time
       * members; a list or object field gives the borrowed pointer or a
       * handle on it.
       *
       * @tparam V the value type, a #RawMutableValue
       * @param value the slot of a field of the field type #raw_field_type_for gives for @p V
       * @return the value
       */
      template <typename V>
         requires RawMutableValue<V>
      [[nodiscard]] inline V raw_value(const lMultiType &value) noexcept {
         if constexpr (RawPlainValue<V>) {
            return plain_value<V>(value);
         } else if constexpr (raw_field_type_for<V>() == FieldType::List) {
            return V(value.glp);
         } else {
            return V(value.obj);
         }
      }

      /**
       * @brief Read a field given at run time, for every value type of the run-time accessors
       *
       * A plain value goes through runtime_get(), the core the typed
       * run-time member ConstElement<T>::get<V>(int) runs on.
       *
       * @tparam V the value type, a #RawMutableValue
       * @param ep the element, or nullptr
       * @param nm the field number
       * @return the value
       * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType
       */
      template <typename V>
         requires RawMutableValue<V>
      [[nodiscard]] inline V raw_get(const lListElem *ep, const int nm) {
         if constexpr (RawPlainValue<V>) {
            return runtime_get<V>(ep, nm);
         } else {
            return raw_value<V>(slot(ep, ocs::tom::checked_pos(ep, nm, raw_field_type_for<V>())));
         }
      }

      /**
       * @brief The first element of a list whose key field holds a key, for a key field given at run time
       * @tparam V the key's value type, a #RawKeyValue
       * @param lp the list, or nullptr for no match
       * @param key_nm the key field
       * @param key the key
       * @return the element, or nullptr if none holds the key or @p lp is nullptr
       * @throw FieldException with reason ForeignField, AbsentField or WrongType if the list's elements do not
       *        hold a key field of the type of @p V, NullKey for a null string or host name, DuplicateKey if a
       *        unique field holds the key twice
       */
      template <typename V>
         requires RawKeyValue<V>
      [[nodiscard]] inline lListElem *raw_find_first(const lList *lp, const int key_nm, const V key) {
         if (lp == nullptr) {
            return nullptr;
         }
         constexpr FieldType type = field_type_for<V>();
         const int pos = ocs::tom::field_pos(lp->descr, key_nm);
         if (pos < 0) [[unlikely]] {
            ocs::tom::throw_field_exception(FieldException::Reason::ForeignField, key_nm, lp->descr);
         }
         if (lp->descr[pos].off == OCS_CULL_ABSENT) [[unlikely]] {
            ocs::tom::throw_field_exception(FieldException::Reason::AbsentField, key_nm, lp->descr);
         }
         if (ocs::tom::field_type_of(lp->descr[pos].mt) != type) [[unlikely]] {
            ocs::tom::throw_field_exception(FieldException::Reason::WrongType, key_nm, lp->descr, lp->descr);
         }
         const void *iterator = nullptr;
         if constexpr (type == FieldType::String || type == FieldType::Host) {
            const char *text = static_cast<const char *>(key);
            if (text == nullptr) [[unlikely]] {
               ocs::tom::throw_field_exception(FieldException::Reason::NullKey, key_nm, lp->descr);
            }
            return ocs::tom::lookup_first<type>(lp, pos, text, &iterator);
         } else {
            return ocs::tom::lookup_first<type>(lp, pos, key, &iterator);
         }
      }

      /**
       * @brief An iterator over the elements of a list of any type, yielding raw element handles
       * @tparam H the handle, ConstRawElement or RawElement
       */
      template <typename H>
      class RawElementIterator {
      public:
         using iterator_category = std::forward_iterator_tag;   ///< walks forward only
         using value_type = H;                                   ///< an element handle
         using difference_type = std::ptrdiff_t;                 ///< distance between two positions
         using pointer = void;                                   ///< handles are values, there is no pointer to one
         using reference = H;                                    ///< dereferencing gives a handle by value

         /** @brief The end of every list */
         RawElementIterator() noexcept = default;

         /**
          * @brief An iterator at an element
          * @param ep the element, or nullptr for the end
          */
         explicit RawElementIterator(lListElem *ep) noexcept : ep_(ep) {}

         /**
          * @brief The handle on the current element
          * @return the handle
          */
         H operator*() const noexcept {
            return H(ep_);
         }

         /**
          * @brief Step to the next element
          * @return this iterator
          */
         RawElementIterator &operator++() noexcept {
            ep_ = ep_->next;
            return *this;
         }

         /**
          * @brief Step to the next element
          * @return the iterator before the step
          */
         RawElementIterator operator++(int) noexcept {
            RawElementIterator before = *this;
            ep_ = ep_->next;
            return before;
         }

         /**
          * @brief Whether two iterators are at the same position
          * @param other the other iterator
          * @return true if at the same element or both at the end
          */
         bool operator==(const RawElementIterator &other) const noexcept = default;

      private:
         lListElem *ep_ = nullptr;   ///< the current element, or nullptr at the end
      };

      /** @brief The deleter of RawWherePtr: frees a condition with the core */
      struct FreeWhere {
         /**
          * @brief Free a condition, as lFreeWhere() does
          * @param cp the condition, or nullptr
          */
         void operator()(lCondition *cp) const noexcept {
            ocs::tom::free_where(cp);
         }
      };

      /** @brief The deleter of RawWhatPtr: frees a selection with the core */
      struct FreeWhat {
         /**
          * @brief Free a selection, as lFreeWhat() does
          * @param ep the selection, or nullptr
          */
         void operator()(lEnumeration *ep) const noexcept {
            ocs::tom::free_what(ep);
         }
      };

   } // namespace detail

   /** @brief Owns a condition of any type; methods are those of std::unique_ptr */
   using RawWherePtr = std::unique_ptr<lCondition, detail::FreeWhere>;

   /** @brief Owns a field selection of any type; methods are those of std::unique_ptr */
   using RawWhatPtr = std::unique_ptr<lEnumeration, detail::FreeWhat>;

   // ---------------------------------------------------------------------------------------------------------------
   // checked accessors on raw pointers

   /**
    * @brief Read a field given at run time from a read-only element
    *
    * Plain fields give their value; a list or an object field gives the
    * borrowed list or element (`const lList *`, `const lListElem *`) or a
    * read-only handle on it (`ConstRawList`, `ConstRawElement`).
    *
    * @tparam V the value type, which names the field type
    * @param ep the element, or nullptr
    * @param nm the field number
    * @return the value
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType, in this order
    * @see Old form: lGetUlong(), lGetString(), lGetList(), lGetObject() and the other getters by name;
    *      typed form: ConstElement<T>::get<V>(int)
    */
   template <typename V>
      requires detail::RawConstValue<V>
   [[nodiscard]] V get(const lListElem *ep, const int nm) {
      return detail::raw_get<V>(ep, nm);
   }

   /**
    * @brief Read a field given at run time from a mutable element; list and object fields also for writing
    *
    * As get() on a read-only element, and additionally `lList *`,
    * `lListElem *`, `RawList` and `RawElement` for a list or an object field.
    *
    * @tparam V the value type, which names the field type
    * @param ep the element, or nullptr
    * @param nm the field number
    * @return the value
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType, in this order
    * @see Old form: lGetListRW(), lGetObject() and the other getters by name
    */
   template <typename V>
      requires detail::RawMutableValue<V>
   [[nodiscard]] V get(lListElem *ep, const int nm) {
      return detail::raw_get<V>(ep, nm);
   }

   /**
    * @brief Read a field given at run time, or a fallback if the element does not hold it
    *
    * The soft path of the generic walkers ("0 if the element does not hold
    * the field"). Not holding the field is everything has() says false for: a
    * null element, a foreign field, a field a reduced element leaves out. A
    * wrong value type still throws, since no data can cause it.
    *
    * @tparam V the value type, which names the field type
    * @param ep the element, or nullptr
    * @param nm the field number
    * @param fallback the value for a field the element does not hold
    * @return the value, or @p fallback
    * @throw FieldException with reason WrongType if the field is of the element's type and @p V is not its type
    */
   template <typename V>
      requires detail::RawConstValue<V>
   [[nodiscard]] V get_or(const lListElem *ep, const int nm, const V fallback) {
      if (ep == nullptr) {
         return fallback;
      }
      const int pos = field_pos(ep->descr, nm);
      if (pos < 0) {
         return fallback;
      }
      if (field_type_of(ep->descr[pos].mt) != detail::raw_field_type_for<V>()) [[unlikely]] {
         throw_field_exception(FieldException::Reason::WrongType, nm, ep->descr, ep->descr);
      }
      if (ep->descr[pos].off == OCS_CULL_ABSENT) {
         return fallback;
      }
      return detail::raw_value<V>(detail::slot(ep, pos));
   }

   /**
    * @brief Whether an element holds a field: of its type and, for a reduced element, not left out
    * @param ep the element, or nullptr
    * @param nm the field number
    * @return false for nullptr, a foreign field or an absent field
    * @see Old form: lGetPosViaElem() >= 0, lGetPosInDescr() >= 0
    */
   [[nodiscard]] inline bool has(const lListElem *ep, const int nm) noexcept {
      if (ep == nullptr) {
         return false;
      }
      const int pos = field_pos(ep->descr, nm);
      return pos >= 0 && ep->descr[pos].off != OCS_CULL_ABSENT;
   }

   /**
    * @brief Set a plain field given at run time
    *
    * Strings and host names are copied and the old value freed; the hash
    * table of the owning list is maintained. There is no implicit conversion:
    * a std::string, a float or a short does not compile, and a number must
    * have the type of the field (an int is an Int32, `std::uint32_t{5}` a Uint32).
    *
    * @tparam V the value type, one of the nine plain types, which names the field type
    * @param ep the element, or nullptr
    * @param nm the field number
    * @param value the value
    * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType, OutOfMemory, or in a
    *        debug build DuplicateKey when another element of the list holds the value of a unique field
    * @see Old form: lSetUlong(), lSetString(), lSetHost() and the other setters by name;
    *      typed form: Element<T>::set(int, V)
    */
   template <typename V>
      requires detail::RawPlainValue<V>
   void set(lListElem *ep, const int nm, const V value) {
      detail::runtime_set<V>(ep, nm, value);
   }

   /**
    * @brief Store a list into a list field given at run time, freeing the list it held
    * @param ep the element, or nullptr
    * @param nm the field number of a list field
    * @param list the list, or an empty owner to clear the field; the element takes it over. On a throw the
    *        parameter frees it. A typed owner is converted explicitly: `RawListPtr(std::move(typed))`.
    * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType, or WrongSubType if
    *        the installed model names another sub-type for the field
    * @see Old form: lSetList(); typed form: Element<T>::set<F>(ListPtr<Sub>)
    */
   void set(lListElem *ep, int nm, RawListPtr list);

   /**
    * @brief Store an element into an object field given at run time, freeing the element it held
    * @param ep the element, or nullptr
    * @param nm the field number of an object field
    * @param object the element to store, or an empty owner to clear the field; the element takes it over. On a
    *        throw the parameter frees a free element. A bound element is released before any check, so that
    *        no throw frees it: it stays with what it is bound to (unlike append(), which leaves it in the
    *        owner). A typed owner is converted explicitly: `RawElementPtr(std::move(typed))`.
    * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType, WrongSubType if the
    *        installed model names another sub-type for the field, BoundElement if @p object is bound in a list
    *        or another object and every other check passed
    * @see Old form: lSetObject(); typed form: Element<T>::set<F>(ElementPtr<Sub>)
    */
   void set(lListElem *ep, int nm, RawElementPtr object);

   /**
    * @brief Put a list into a list field given at run time and take out the list it held
    * @param ep the element, or nullptr
    * @param nm the field number of a list field
    * @param list the new list, or an empty owner; the element takes it over. On a throw the parameter frees it.
    * @return the owner of the list the field held, empty if it held none
    * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType or WrongSubType
    * @see Old form: lXchgList(), lSwapList() as two calls; typed form: Element<T>::exchange<F>()
    */
   [[nodiscard]] RawListPtr exchange(lListElem *ep, int nm, RawListPtr list);

   /**
    * @brief Take the element out of an object field given at run time; the field holds none afterwards
    *
    * Named apart from the core's take_object(ep, pos), which takes a position.
    *
    * @param ep the element, or nullptr
    * @param nm the field number of an object field
    * @return the owner of the element the field held, empty if it held none
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType
    * @see Old form: lDechainObject(); typed form: Element<T>::take<F>()
    */
   [[nodiscard]] RawElementPtr take_raw_object(lListElem *ep, int nm);

   // ---------------------------------------------------------------------------------------------------------------
   // generic list and selection ops

   /**
    * @brief Create an element of a full or reduced descriptor, every field at its default
    * @param descr the descriptor
    * @return the owner of the element
    * @throw FieldException with reason NullDescriptor if @p descr is null or empty
    * @throw ListCoreError if there is no memory
    * @see Old form: lCreateElem(); core: ocs::tom::create_elem()
    */
   [[nodiscard]] RawElementPtr create_raw_elem(const lDescr *descr);

   /**
    * @brief Create an empty list of a full or reduced descriptor
    * @param descr the descriptor of its elements
    * @param hashing whether the list keeps hash tables for its hashed fields
    * @param name the name of the list, nullptr for "No list name specified"
    * @return the owner of the list
    * @throw FieldException with reason NullDescriptor if @p descr is null or empty
    * @throw ListCoreError if there is no memory
    * @see Old form: lCreateList(), lCreateListHash(); core: ocs::tom::create_list()
    */
   [[nodiscard]] RawListPtr create_raw_list(const lDescr *descr, Hashing hashing = Hashing::On,
                                            const char *name = nullptr);

   /**
    * @brief A deep copy of an element, with its sub-lists and sub-objects; a reduced element stays reduced
    * @param ep the element, or nullptr
    * @return the owner of the copy, empty for nullptr
    * @throw ListCoreError if the copy fails
    * @see Old form: lCopyElem(); core: ocs::tom::copy_elem()
    */
   [[nodiscard]] RawElementPtr clone(const lListElem *ep);

   /**
    * @brief A deep copy of a list with the name of the original
    * @param lp the list, or nullptr
    * @param hashing whether the copy keeps hash tables
    * @return the owner of the copy, empty for nullptr
    * @throw ListCoreError if the copy fails
    * @see Old form: lCopyList(), lCopyListHash(); core: ocs::tom::copy_list()
    */
   [[nodiscard]] RawListPtr clone(const lList *lp, Hashing hashing = Hashing::On);

   /**
    * @brief A deep copy of a condition
    * @param cp the condition, or nullptr
    * @return the owner of the copy, empty for nullptr
    * @throw ListCoreError if there is no memory or the condition is corrupt
    * @see Old form: lCopyWhere(); core: ocs::tom::copy_where()
    */
   [[nodiscard]] RawWherePtr clone(const lCondition *cp);

   /**
    * @brief A deep copy of a field selection
    * @param ep the selection, or nullptr
    * @return the owner of the copy, empty for nullptr
    * @throw ListCoreError if there is no memory
    * @see Old form: lCopyWhat(); core: ocs::tom::copy_what()
    */
   [[nodiscard]] RawWhatPtr clone(const lEnumeration *ep);

   /**
    * @brief Append an element to a list
    * @param lp the list, which takes the element over
    * @param ep the owner of the element; empty afterwards. On a throw the element stays with it, also a bound
    *        element (BoundElement), which the caller then has to release(); set() releases it instead.
    * @throw FieldException with reason NullList, NullElement for an empty owner, WrongType if the element is of
    *        another type than the list, WrongLayout if of the type but another layout (CS-3112), BoundElement,
    *        or DuplicateKey in a debug build; nothing is changed then
    * @see Old form: lAppendElem(); core: ocs::tom::append_elem(); typed owner: append(lList *, ElementPtr<T> &&)
    */
   void append(lList *lp, RawElementPtr &&ep);

   /**
    * @brief Insert an element after another one of a list, or at the front
    * @param lp the list, which takes the element over
    * @param after the predecessor, an element of @p lp, or nullptr for the front
    * @param ep the owner of the element; empty afterwards. On a throw the element stays with it.
    * @throw FieldException with the reasons of append(), and ForeignList if @p after is not in @p lp
    * @see Old form: lInsertElem(); core: ocs::tom::insert_elem()
    */
   void insert_after(lList *lp, lListElem *after, RawElementPtr &&ep);

   /**
    * @brief Take an element out of its list without freeing it
    * @param lp the list
    * @param ep the element, an element of @p lp
    * @return the owner of the element, which is free now
    * @throw FieldException with reason NullList, NullElement or ForeignList; nothing is changed then
    * @see Old form: lDechainElem(); core: ocs::tom::dechain_elem()
    */
   [[nodiscard]] RawElementPtr take(lList *lp, lListElem *ep);

   /**
    * @brief Take an element out of its list and free it
    * @param lp the list
    * @param ep the element, an element of @p lp; dangles afterwards
    * @throw FieldException with reason NullList, NullElement or ForeignList; nothing is changed then
    * @see Old form: lRemoveElem(); core: ocs::tom::remove_elem()
    */
   void erase(lList *lp, lListElem *ep);

   /**
    * @brief Move every element of a list to the end of another and free the emptied list
    * @param dst the list taking the elements
    * @param src the owner of the list giving them; empty afterwards; an empty owner moves nothing. On a throw
    *        the list stays with it.
    * @throw FieldException with reason NullList if @p dst is nullptr, WrongType if the lists are of different
    *        types, WrongLayout if of one type with different layouts, or DuplicateKey in a debug build; nothing
    *        is changed then
    * @see Old form: lAddList(); core: ocs::tom::append_list()
    */
   void splice(lList *dst, RawListPtr &&src);

   /**
    * @brief The number of elements of a list
    * @param lp the list, or nullptr
    * @return the number, 0 for nullptr
    * @see Old form: lGetNumberOfElem()
    */
   [[nodiscard]] inline std::size_t size(const lList *lp) noexcept {
      return lp != nullptr ? lp->nelem : 0;
   }

   /**
    * @brief The first element of a read-only list whose key field holds a key
    *
    * Through the field's hash table where the list has one or reaches the
    * hash threshold, otherwise by a linear search; host names compare as
    * everywhere in the cluster.
    *
    * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host; names the field type
    * @param lp the list, or nullptr for no match
    * @param key_nm the key field
    * @param key the key
    * @return the element, or nullptr if none holds the key or @p lp is nullptr
    * @throw FieldException with reason ForeignField, AbsentField, WrongType, NullKey for a null string or host
    *        name, DuplicateKey if a unique field holds the key twice
    * @see Old form: lGetElemUlong(), lGetElemUlong64(), lGetElemStr(), lGetElemHost(); core: lookup_first()
    */
   template <typename V>
      requires detail::RawKeyValue<V>
   [[nodiscard]] const lListElem *find_first(const lList *lp, const int key_nm, const V key) {
      return detail::raw_find_first<V>(lp, key_nm, key);
   }

   /**
    * @brief The first element of a mutable list whose key field holds a key
    * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host; names the field type
    * @param lp the list, or nullptr for no match
    * @param key_nm the key field
    * @param key the key
    * @return the element, or nullptr if none holds the key or @p lp is nullptr
    * @throw FieldException as find_first() on a read-only list
    * @see Old form: lGetElemUlongRW(), lGetElemStrRW(), lGetElemHostRW() and their siblings
    */
   template <typename V>
      requires detail::RawKeyValue<V>
   [[nodiscard]] lListElem *find_first(lList *lp, const int key_nm, const V key) {
      return detail::raw_find_first<V>(lp, key_nm, key);
   }

   /**
    * @brief Copy the elements of a list that fulfil a condition, reduced to a selection of fields
    *
    * Unlike lSelect(), no match gives an empty list rather than none; the
    * result has the name of the source list.
    *
    * @param lp the list, or nullptr
    * @param where the condition, or nullptr for every element
    * @param what the selection of fields
    * @param hashing whether the result and its copied sub-lists keep hash tables
    * @return the owner of the result, empty if @p lp is nullptr
    * @throw FieldException with reason NullElement if @p what is nullptr
    * @throw ListCoreError if the reduction or a copy fails
    * @see Old form: lSelect(), lSelectHashPack() and lSelectDPack() without a pack buffer; core: select_list();
    *      with the old contract: select_as(), select_into_descr(), select_pack()
    */
   [[nodiscard]] RawListPtr select(const lList *lp, const lCondition *where, const lEnumeration *what,
                                   Hashing hashing = Hashing::On);

   /**
    * @brief A copy of one element reduced to a selection of fields
    * @param ep the element, or nullptr
    * @param what the selection of fields
    * @param hashing whether copied sub-lists keep hash tables
    * @return the owner of the copy, a free element; empty if @p ep is nullptr
    * @throw FieldException with reason NullElement if @p what is nullptr
    * @throw ListCoreError if the element does not hold a selected field or a copy fails
    * @see Old form: lSelectElemPack() without a pack buffer; core: select_elem(); with the old contract:
    *      select_elem_as(), select_elem_into_descr()
    */
   [[nodiscard]] RawElementPtr select(const lListElem *ep, const lEnumeration *what, Hashing hashing = Hashing::On);

   /**
    * @brief Whether an element fulfils a condition
    * @param ep the element
    * @param where the condition, or nullptr, which every element fulfils
    * @return true if @p ep fulfils @p where
    * @throw FieldException with reason NullElement if @p ep is nullptr
    * @see Old form: lCompare(); core: ocs::tom::compare()
    */
   [[nodiscard]] bool matches(const lListElem *ep, const lCondition *where);

   // ---------------------------------------------------------------------------------------------------------------
   // conditions and selections built at run time
   //
   // For code that knows the fields and the values only at run time (the Python bridge): the field is an int
   // argument and the value type the type of the argument; no format string is parsed. Each builder makes through
   // the core what the old format builder made, and where the old one returned nullptr it returns an empty owner
   // and leaves the error number the old one left.

   namespace detail {

      /**
       * @brief A value type a run-time comparison compares a field with: a number, bool or a string
       *
       * A string is compared with a string or a host field, as `%s` of lWhere() was.
       *
       * @tparam V the value type
       */
      template <typename V>
      concept RawCompareValue = std::is_same_v<V, std::uint32_t> || std::is_same_v<V, std::uint64_t> ||
                                std::is_same_v<V, std::int32_t> || std::is_same_v<V, std::int64_t> ||
                                std::is_same_v<V, double> || std::is_same_v<V, bool> ||
                                std::is_same_v<V, const char *>;

      /**
       * @brief Build the comparison of a field with a value of a list library type, the body of where_compare()
       * @param descr the descriptor of the type
       * @param nm the field
       * @param op the comparison
       * @param value_type the list library type of @p value (lUlongT, lStringT, ...)
       * @param value the value
       * @return the owner of the condition; empty for a null descriptor or a field the descriptor does not hold,
       *         with the error number LEPARSECOND
       * @throw FieldException with reason WrongType if @p value_type does not fit the field
       */
      [[nodiscard]] RawWherePtr raw_where_compare(const lDescr *descr, int nm, CompareOp op, int value_type,
                                                  const lMultiType &value);

   } // namespace detail

   /**
    * @brief The comparison of a field given at run time with a value, as lWhere("%T(%I <op> %<v>)") built it
    *
    * The operator is a CompareOp (`==` Equal, `!=` NotEqual, `<` Lower, `<=` LowerEqual, `>` Greater, `>=`
    * GreaterEqual, and the named comparisons of lWhere()); it is not checked against the field type, as lWhere()
    * did not check it. The value type selects the format placeholder: std::uint32_t `%u`, std::uint64_t `%lu`,
    * std::int32_t `%d`, std::int64_t `%o`, double `%g`, bool `%b`, const char * `%s` (a string or a host
    * field; the string is copied, nullptr is taken as no value).
    *
    * Differences from lWhere(): a value type that does not fit the field throws WrongType, where lWhere()
    * aborted the process.
    *
    * @tparam V the value type, a #detail::RawCompareValue
    * @param descr the descriptor of the type
    * @param nm the field
    * @param op the comparison
    * @param value the value
    * @return the owner of the condition; empty for a null descriptor or a field the descriptor does not hold
    *         (also one a reduced descriptor leaves out) and when the condition or a string value cannot be
    *         allocated, with the error number LEPARSECOND, as lWhere() left it
    * @throw FieldException with reason WrongType if @p V does not fit the field's type
    * @see Old form: lWhere() with one comparison, e.g. lWhere("%T(%I == %s)", descr, nm, value); core:
    *      ocs::tom::make_compare(); typed form: ocs::tom::field<F>() with its operators
    */
   template <typename V>
      requires detail::RawCompareValue<V>
   [[nodiscard]] RawWherePtr where_compare(const lDescr *descr, const int nm, const CompareOp op, const V value) {
      lMultiType v{};
      if constexpr (std::is_same_v<V, std::uint32_t>) {
         v.ul = value;
      } else if constexpr (std::is_same_v<V, std::uint64_t>) {
         v.ul64 = value;
      } else if constexpr (std::is_same_v<V, std::int32_t>) {
         v.i = value;
      } else if constexpr (std::is_same_v<V, std::int64_t>) {
         v.l = value;
      } else if constexpr (std::is_same_v<V, double>) {
         v.db = value;
      } else if constexpr (std::is_same_v<V, bool>) {
         v.b = value;
      } else {
         // the core copies the string; it does not change it
         v.str = const_cast<char *>(value);
      }
      return detail::raw_where_compare(descr, nm, op, cull_type_of(detail::field_type_for<V>()), v);
   }

   /**
    * @brief The selection of some fields of a type given at run time, as lIntVector2What() built it
    *
    * The fields are brought into the order of the descriptor and a field given twice is kept once, as before.
    * Unlike the old int array, @p fields holds no #NoName terminator; a #NoName in it is a field the type does not
    * hold.
    *
    * @param descr the descriptor of the type
    * @param fields the fields
    * @return the owner of the selection; empty for a null descriptor, no field, or a field the descriptor does not
    *         hold (also one a reduced descriptor leaves out), with the error number LENAMENOT, as
    *         lIntVector2What() left it
    * @see Old form: lIntVector2What(), lWhat("%T(%I...)") without sub-selections; core: ocs::tom::make_what();
    *      typed form: ocs::tom::what<T>(fields)
    */
   [[nodiscard]] RawWhatPtr what_of(const lDescr *descr, std::span<const int> fields);

   /**
    * @brief The selection of every field, of whatever type it is applied to
    * @return the owner of the selection; empty only if there is no memory (the error number is set then)
    * @see Old form: lWhatAll(), lWhat("%T(ALL)", descr) with a descriptor that is not null; core:
    *      ocs::tom::make_what_special(); typed form: ocs::tom::what_all<T>()
    */
   [[nodiscard]] RawWhatPtr what_all();

   // ---------------------------------------------------------------------------------------------------------------
   // selections with the contract of the old interface
   //
   // The ops below keep exactly what the old select functions gave, for the callers whose result goes onto the
   // wire and whose condition and selection may come from a client: the result is named as the caller says (the
   // name is packed), no match is no list when hashing is on, no selection copies every field, a precomputed
   // reduced descriptor is taken as it is, and a condition or selection that does not fit the list gives no list
   // and an error number instead of an exception. They throw no exception of the object model; every error the
   // old function survived they survive the same way.

   /**
    * @brief Copy the elements of a list that fulfil a condition, reduced to a selection, as lSelect() did
    *
    * Differences from select(): the result is named @p name, not after the
    * source list; with Hashing::On no match gives no list (error number
    * LEGETNROFELEM) instead of an empty one, with Hashing::Off an empty list;
    * @p what == nullptr copies the whole list instead of throwing - then under
    * the name of the source list and without applying @p where, as
    * lSelectHashPack() did; a selection that does not fit the list's type gives
    * no list and sets the error number instead of throwing. An element whose
    * fields cannot be copied is left out.
    *
    * @param name the name of the result, nullptr for the default name of a list; ignored when @p what is nullptr
    * @param lp the list, or nullptr for no list (reported as a soft path)
    * @param where the condition, or nullptr for every element; not applied when @p what is nullptr
    * @param what the selection of fields, or nullptr for every field
    * @param hashing whether the result and its copied sub-lists keep hash tables; Hashing::On also turns an
    *        empty result into no list
    * @return the owner of the result; empty for a null list, on an error (the CULL error number is set), and
    *         with Hashing::On for no match
    * @see Old form: lSelect() (Hashing::On), lSelectHashPack() without a pack buffer; core: select_list(),
    *      copy_list()
    */
   [[nodiscard]] RawListPtr select_as(const char *name, const lList *lp, const lCondition *where,
                                      const lEnumeration *what, Hashing hashing);

   /**
    * @brief A copy of one element reduced to a selection if it fulfils a condition, as lSelectElemPack() did
    *
    * Differences from select(const lListElem *, ...): the condition is
    * applied (no match gives no copy); @p what == nullptr copies the whole
    * element - then without applying @p where - instead of throwing; a
    * selection that does not fit gives no copy and sets the error number
    * instead of throwing.
    *
    * @param ep the element, or nullptr for no copy
    * @param where the condition, or nullptr; not applied when @p what is nullptr
    * @param what the selection of fields, or nullptr for every field
    * @param hashing whether copied sub-lists keep hash tables
    * @return the owner of the copy, a free element; empty for nullptr, no match, or an error (error number set)
    * @see Old form: lSelectElemPack() without a pack buffer; core: compare(), select_elem(), copy_elem()
    */
   [[nodiscard]] RawElementPtr select_elem_as(const lListElem *ep, const lCondition *where, const lEnumeration *what,
                                              Hashing hashing);

   /**
    * @brief Copy the elements of a list that fulfil a condition into a list of a given reduced descriptor, as
    *        lSelectDPack() did
    *
    * For a caller that keeps the reduced descriptor of a selection (the event
    * master caches one per subscription), so that the reduction is not looked
    * up per call. Differences from select(): the descriptor is the caller's
    * (it must hold every field @p what selects; an element whose copy fails is
    * left out); the result is named @p name; with Hashing::On no match gives no
    * list; @p what == nullptr leaves every element out (error number
    * LEENUMNULL); nothing throws.
    *
    * @param name the name of the result, nullptr for the default name of a list
    * @param lp the list, or nullptr for no list
    * @param where the condition, or nullptr for every element
    * @param descr the descriptor of the result, the reduction @p what makes of the list's type; nullptr for no
    *        list
    * @param what the selection of fields
    * @param hashing whether the result and its copied sub-lists keep hash tables; Hashing::On also turns an
    *        empty result into no list
    * @return the owner of the result; empty for a null list or descriptor, if the list cannot be created or an
    *         element cannot be linked into it (error number LECREATELIST, LEAPPENDELEM), and with Hashing::On for
    *         no match (LEGETNROFELEM)
    * @see Old form: lSelectDPack() without a pack buffer; core: create_list(), compare(), copy_elem_partial()
    */
   [[nodiscard]] RawListPtr select_into_descr(const char *name, const lList *lp, const lCondition *where,
                                              const lDescr *descr, const lEnumeration *what,
                                              Hashing hashing);

   /**
    * @brief A copy of one element in a given reduced descriptor if it fulfils a condition, as lSelectElemDPack()
    *        did
    *
    * The element form of select_into_descr(): no copy for a null element or
    * descriptor, for no match, or for a copy that fails (@p what nullptr, a
    * selected field the descriptor or the element does not hold); nothing
    * throws.
    *
    * @param ep the element, or nullptr
    * @param where the condition, or nullptr
    * @param descr the descriptor of the copy; nullptr for no copy
    * @param what the selection of fields
    * @param hashing whether copied sub-lists keep hash tables
    * @return the owner of the copy, a free element; empty as described (error number set on a failed copy)
    * @see Old form: lSelectElemDPack() without a pack buffer; core: create_elem(), compare(), copy_elem_partial()
    */
   [[nodiscard]] RawElementPtr select_elem_into_descr(const lListElem *ep, const lCondition *where,
                                                      const lDescr *descr, const lEnumeration *what,
                                                      Hashing hashing);

   /**
    * @brief Pack the elements of a list that fulfil a condition, reduced to a selection, without a copy, as
    *        lSelectHashPack() with a pack buffer did
    *
    * Writes the bytes of a list that unpack_raw_list() reads. With a
    * selection: whether there is a list; if there is one, @p name (the name of
    * the source list for nullptr) and the selection as descriptor, each
    * matching element preceded by a 1 (a selection of every field writes the
    * whole element, the selection of no field nothing after the 1), then a 0.
    * Without a selection the whole list is packed as pack_list() packs it:
    * under its own name and without applying @p where.
    *
    * Errors are not reported, as before: a header write that fails ends the
    * write, an element write that fails is skipped, a failed final 0 is lost;
    * the buffer then holds what was written. No exception of the object
    * model leaves it.
    *
    * @param pb the buffer to write to
    * @param name the name packed for the list, nullptr for the name of @p lp; ignored when @p what is nullptr
    * @param lp the list, or nullptr, which is packed as no list
    * @param where the condition, or nullptr for every element; not applied when @p what is nullptr
    * @param what the selection of fields, or nullptr for every field
    * @see Old form: lSelectHashPack() with a pack buffer (its hash flag had no effect there); core: compare(),
    *      put_uint32(), put_str(), pack_what_as_descr(), pack_elem(), pack_list()
    */
   void select_pack(sge_pack_buffer *pb, const char *name, const lList *lp, const lCondition *where,
                    const lEnumeration *what);

   /**
    * @brief Unpack a list of the type the buffer names
    *
    * Named apart from the core's unpack_list(), which returns the raw list.
    *
    * @param pb the buffer to read from
    * @param flags the unpack flags of cull_unpack_list_partial()
    * @return the owner of the list, empty for a list that was packed as none
    * @throw PackException on a format error or for lack of memory
    * @see Old form: cull_unpack_list(), cull_unpack_list_partial(); core: ocs::tom::unpack_list()
    */
   [[nodiscard]] RawListPtr unpack_raw_list(sge_pack_buffer *pb, int flags = 0);

   /**
    * @brief Unpack an element of the type the buffer names
    *
    * Named apart from the core's unpack_elem(), which returns the raw element. Unlike a list, an element is
    * never packed as none: the owner is never empty on return.
    *
    * @param pb the buffer to read from
    * @param descr nullptr for an element that was free at the sender, the descriptor of one that was bound; for
    *        a free element of flag-filtered data the expected type, or nullptr
    * @param flags the unpack flags of cull_unpack_elem_partial()
    * @return the owner of the element
    * @throw PackException on a format error, for a bound element without @p descr (PACK_BADARG) or for lack of
    *        memory; everything read before is freed
    * @see Old form: cull_unpack_elem(), cull_unpack_elem_partial(); core: ocs::tom::unpack_elem()
    */
   [[nodiscard]] RawElementPtr unpack_raw_elem(sge_pack_buffer *pb, const lDescr *descr, int flags = 0);

   // ---------------------------------------------------------------------------------------------------------------
   // handles

   /**
    * @brief A read-only, non-owning handle on an element of any type
    *
    * The size of a pointer; converts implicitly to `const lListElem *` and is
    * made implicitly from one or from a typed read-only handle: it owns nothing
    * and checks nothing, so the conversion cannot fail or lose an element. The
    * type is the element's descriptor. Every method forwards to the free
    * function of the same name.
    */
   class ConstRawElement {
   public:
      /** @brief An empty handle */
      constexpr ConstRawElement() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr ConstRawElement(std::nullptr_t) noexcept {}

      /**
       * @brief A handle on an element
       * @param ep the element, or nullptr for an empty handle
       */
      ConstRawElement(const lListElem *ep) noexcept : ep_(const_cast<lListElem *>(ep)) {}

      /**
       * @brief A handle on the element of a typed handle, also of a mutable one
       * @tparam T the object type
       * @param element the typed handle, maybe empty
       */
      template <ObjectType T>
      ConstRawElement(const ConstElement<T> element) noexcept : ConstRawElement(element.raw()) {}

      /**
       * @brief The element, for every function that takes the raw pointer
       * @return the element, or nullptr
       */
      operator const lListElem *() const noexcept {
         return ep_;
      }

      /**
       * @brief The element, for varargs and deduced templates
       * @return the element, or nullptr
       */
      [[nodiscard]] const lListElem *raw() const noexcept {
         return ep_;
      }

      /**
       * @brief The descriptor of the element: its type and layout
       * @return the descriptor, nullptr if the handle is empty
       * @see Old form: lGetElemDescr()
       */
      [[nodiscard]] const lDescr *descr() const noexcept {
         return ep_ != nullptr ? ep_->descr : nullptr;
      }

      /**
       * @brief Whether the handle holds an element
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

      /**
       * @brief The next element of the list, read only
       * @return the handle on it, empty at the end of the list or for an empty handle
       * @see Old form: lNext()
       */
      [[nodiscard]] ConstRawElement next() const noexcept {
         return ConstRawElement(ep_ != nullptr ? ep_->next : nullptr);
      }

      /**
       * @brief The previous element of the list, read only
       * @return the handle on it, empty at the front of the list or for an empty handle
       * @see Old form: lPrev()
       */
      [[nodiscard]] ConstRawElement prev() const noexcept {
         return ConstRawElement(ep_ != nullptr ? ep_->prev : nullptr);
      }

      /**
       * @brief Read a field given at run time; forwards to ocs::tom::get<V>(const lListElem *, int)
       * @tparam V the value type, which names the field type
       * @param nm the field number
       * @return the value
       * @throw FieldException as the free form, NullElement for an empty handle
       */
      template <typename V>
         requires detail::RawConstValue<V>
      [[nodiscard]] V get(const int nm) const {
         return ocs::tom::get<V>(raw(), nm);
      }

      /**
       * @brief Read a field given at run time, or a fallback; forwards to ocs::tom::get_or()
       * @tparam V the value type, which names the field type
       * @param nm the field number
       * @param fallback the value for a field the element does not hold, or for an empty handle
       * @return the value, or @p fallback
       * @throw FieldException with reason WrongType as the free form
       */
      template <typename V>
         requires detail::RawConstValue<V>
      [[nodiscard]] V get_or(const int nm, const V fallback) const {
         return ocs::tom::get_or<V>(raw(), nm, fallback);
      }

      /**
       * @brief Whether the element holds a field; forwards to ocs::tom::has()
       * @param nm the field number
       * @return false for an empty handle, a foreign or an absent field
       */
      [[nodiscard]] bool has(const int nm) const noexcept {
         return ocs::tom::has(raw(), nm);
      }

      /**
       * @brief A deep copy of the element; forwards to ocs::tom::clone(const lListElem *)
       * @return the owner of the copy, empty if the handle is empty
       * @throw ListCoreError if the copy fails
       */
      [[nodiscard]] RawElementPtr clone() const;

      /**
       * @brief A copy of the element reduced to a selection; forwards to ocs::tom::select(const lListElem *, ...)
       * @param what the selection of fields
       * @param hashing whether copied sub-lists keep hash tables
       * @return the owner of the copy, empty if the handle is empty
       * @throw FieldException and ListCoreError as the free form
       */
      [[nodiscard]] RawElementPtr select(const lEnumeration *what, Hashing hashing = Hashing::On) const;

      /**
       * @brief Whether the element fulfils a condition; forwards to ocs::tom::matches()
       * @param where the condition, or nullptr
       * @return true if the element fulfils @p where
       * @throw FieldException with reason NullElement for an empty handle
       */
      [[nodiscard]] bool matches(const lCondition *where) const {
         return ocs::tom::matches(raw(), where);
      }

   protected:
      lListElem *ep_ = nullptr;   ///< the element, not owned; mutable only through RawElement
   };

   /**
    * @brief A mutable, non-owning handle on an element of any type; adds the setters
    *
    * Converts implicitly to `lListElem *` and to the read-only handle; made
    * implicitly from `lListElem *` or a mutable typed handle, never from a
    * read-only one.
    */
   class RawElement : public ConstRawElement {
   public:
      /** @brief An empty handle */
      constexpr RawElement() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr RawElement(std::nullptr_t) noexcept {}

      /**
       * @brief A handle on an element
       * @param ep the element, or nullptr for an empty handle
       */
      RawElement(lListElem *ep) noexcept : ConstRawElement(ep) {}

      /**
       * @brief A handle on the element of a mutable typed handle
       * @tparam T the object type
       * @param element the typed handle, maybe empty
       */
      template <ObjectType T>
      RawElement(const Element<T> element) noexcept : ConstRawElement(element.raw()) {}

      /**
       * @brief The element, for every function that takes the raw pointer
       * @return the element, or nullptr
       */
      operator lListElem *() const noexcept {
         return ep_;
      }

      /**
       * @brief The element, for varargs and deduced templates
       * @return the element, or nullptr
       */
      [[nodiscard]] lListElem *raw() const noexcept {
         return ep_;
      }

      /**
       * @brief The next element of the list
       * @return the mutable handle on it, empty at the end of the list or for an empty handle
       * @see Old form: lNextRW()
       */
      [[nodiscard]] RawElement next() const noexcept {
         return RawElement(ep_ != nullptr ? ep_->next : nullptr);
      }

      /**
       * @brief The previous element of the list
       * @return the mutable handle on it, empty at the front of the list or for an empty handle
       * @see Old form: lPrevRW()
       */
      [[nodiscard]] RawElement prev() const noexcept {
         return RawElement(ep_ != nullptr ? ep_->prev : nullptr);
      }

      /**
       * @brief Read a field given at run time; list and object fields also for writing; forwards to
       *        ocs::tom::get<V>(lListElem *, int)
       * @tparam V the value type, which names the field type
       * @param nm the field number
       * @return the value
       * @throw FieldException as the free form, NullElement for an empty handle
       */
      template <typename V>
         requires detail::RawMutableValue<V>
      [[nodiscard]] V get(const int nm) const {
         return ocs::tom::get<V>(raw(), nm);
      }

      /**
       * @brief Set a plain field given at run time; forwards to ocs::tom::set(lListElem *, int, V)
       * @tparam V the value type, one of the nine plain types, which names the field type
       * @param nm the field number
       * @param value the value
       * @throw FieldException as the free form, NullElement for an empty handle
       */
      template <typename V>
         requires detail::RawPlainValue<V>
      void set(const int nm, const V value) const {
         ocs::tom::set(raw(), nm, value);
      }

      /**
       * @brief Store a list into a list field; forwards to ocs::tom::set(lListElem *, int, RawListPtr)
       * @param nm the field number
       * @param list the list, which the element takes over
       * @throw FieldException as the free form
       */
      void set(int nm, RawListPtr list) const;

      /**
       * @brief Store an element into an object field; forwards to ocs::tom::set(lListElem *, int, RawElementPtr)
       * @param nm the field number
       * @param object the element, which the element takes over
       * @throw FieldException as the free form
       */
      void set(int nm, RawElementPtr object) const;

      /**
       * @brief Swap a list field with a list; forwards to ocs::tom::exchange()
       * @param nm the field number
       * @param list the new list, which the element takes over
       * @return the owner of the list the field held
       * @throw FieldException as the free form
       */
      [[nodiscard]] RawListPtr exchange(int nm, RawListPtr list) const;

      /**
       * @brief Take the element out of an object field; forwards to ocs::tom::take_raw_object()
       * @param nm the field number
       * @return the owner of the element the field held
       * @throw FieldException as the free form
       */
      [[nodiscard]] RawElementPtr take(int nm) const;
   };

   /**
    * @brief A read-only, non-owning handle on a list of any type
    *
    * Walked as a range of ConstRawElement. Converts implicitly to
    * `const lList *` and is made implicitly from one or from a typed read-only
    * handle, as ConstRawElement is.
    */
   class ConstRawList {
   public:
      using const_iterator = detail::RawElementIterator<ConstRawElement>;   ///< iterates with read-only handles

      /** @brief An empty handle */
      constexpr ConstRawList() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr ConstRawList(std::nullptr_t) noexcept {}

      /**
       * @brief A handle on a list
       * @param lp the list, or nullptr for an empty handle
       */
      ConstRawList(const lList *lp) noexcept : lp_(const_cast<lList *>(lp)) {}

      /**
       * @brief A handle on the list of a typed handle, also of a mutable one
       * @tparam T the object type
       * @param list the typed handle, maybe empty
       */
      template <ObjectType T>
      ConstRawList(const ConstList<T> list) noexcept : ConstRawList(list.raw()) {}

      /**
       * @brief The list, for every function that takes the raw pointer
       * @return the list, or nullptr
       */
      operator const lList *() const noexcept {
         return lp_;
      }

      /**
       * @brief The list, for varargs and deduced templates
       * @return the list, or nullptr
       */
      [[nodiscard]] const lList *raw() const noexcept {
         return lp_;
      }

      /**
       * @brief The descriptor of the list's elements: their type and layout
       * @return the descriptor, nullptr if the handle is empty
       * @see Old form: lGetListDescr()
       */
      [[nodiscard]] const lDescr *descr() const noexcept {
         return lp_ != nullptr ? lp_->descr : nullptr;
      }

      /**
       * @brief Whether the handle holds a list
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return lp_ != nullptr;
      }

      /**
       * @brief The number of elements; forwards to ocs::tom::size()
       * @return the number, 0 for an empty handle
       */
      [[nodiscard]] std::size_t size() const noexcept {
         return ocs::tom::size(raw());
      }

      /**
       * @brief Whether the list holds no element
       * @return true for an empty list or an empty handle
       */
      [[nodiscard]] bool empty() const noexcept {
         return size() == 0;
      }

      /**
       * @brief The first element
       * @return the handle on it, empty for an empty list or an empty handle
       */
      [[nodiscard]] ConstRawElement front() const noexcept {
         return ConstRawElement(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The last element
       * @return the handle on it, empty for an empty list or an empty handle
       * @see Old form: lLast()
       */
      [[nodiscard]] ConstRawElement back() const noexcept {
         return ConstRawElement(lp_ != nullptr ? lp_->last : nullptr);
      }

      /**
       * @brief The start of the range of elements
       * @return the iterator; equal to end() for an empty list or an empty handle
       */
      [[nodiscard]] const_iterator begin() const noexcept {
         return const_iterator(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The end of the range of elements
       * @return the iterator
       */
      [[nodiscard]] const_iterator end() const noexcept {
         return const_iterator();
      }

      /**
       * @brief The first element whose key field holds a key; forwards to ocs::tom::find_first<V>()
       * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host
       * @param key_nm the key field
       * @param key the key
       * @return the handle on the element, empty if none holds the key or the handle is empty
       * @throw FieldException as the free form
       */
      template <typename V>
         requires detail::RawKeyValue<V>
      [[nodiscard]] ConstRawElement find_first(const int key_nm, const V key) const {
         return ConstRawElement(ocs::tom::find_first<V>(raw(), key_nm, key));
      }

      /**
       * @brief Copy the elements that fulfil a condition, reduced to a selection; forwards to ocs::tom::select()
       * @param where the condition, or nullptr for every element
       * @param what the selection of fields
       * @param hashing whether the result and its copied sub-lists keep hash tables
       * @return the owner of the result, empty if the handle is empty
       * @throw FieldException and ListCoreError as the free form
       */
      [[nodiscard]] RawListPtr select(const lCondition *where, const lEnumeration *what,
                                      Hashing hashing = Hashing::On) const;

      /**
       * @brief A deep copy of the list; forwards to ocs::tom::clone(const lList *, Hashing)
       * @param hashing whether the copy keeps hash tables
       * @return the owner of the copy, empty if the handle is empty
       * @throw ListCoreError if the copy fails
       */
      [[nodiscard]] RawListPtr clone(Hashing hashing = Hashing::On) const;

   protected:
      lList *lp_ = nullptr;   ///< the list, not owned; mutable only through RawList
   };

   /**
    * @brief A mutable, non-owning handle on a list of any type; adds the operations that change the list
    *
    * Walked as a range of RawElement. Made implicitly from `lList *` or a
    * mutable typed handle, never from a read-only one.
    */
   class RawList : public ConstRawList {
   public:
      using iterator = detail::RawElementIterator<RawElement>;   ///< iterates with mutable handles

      /** @brief An empty handle */
      constexpr RawList() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr RawList(std::nullptr_t) noexcept {}

      /**
       * @brief A handle on a list
       * @param lp the list, or nullptr for an empty handle
       */
      RawList(lList *lp) noexcept : ConstRawList(lp) {}

      /**
       * @brief A handle on the list of a mutable typed handle
       * @tparam T the object type
       * @param list the typed handle, maybe empty
       */
      template <ObjectType T>
      RawList(const List<T> list) noexcept : ConstRawList(list.raw()) {}

      /**
       * @brief The list, for every function that takes the raw pointer
       * @return the list, or nullptr
       */
      operator lList *() const noexcept {
         return lp_;
      }

      /**
       * @brief The list, for varargs and deduced templates
       * @return the list, or nullptr
       */
      [[nodiscard]] lList *raw() const noexcept {
         return lp_;
      }

      /**
       * @brief The first element
       * @return the mutable handle on it, empty for an empty list or an empty handle
       */
      [[nodiscard]] RawElement front() const noexcept {
         return RawElement(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The last element
       * @return the mutable handle on it, empty for an empty list or an empty handle
       * @see Old form: lLastRW()
       */
      [[nodiscard]] RawElement back() const noexcept {
         return RawElement(lp_ != nullptr ? lp_->last : nullptr);
      }

      /**
       * @brief The start of the range of elements, with mutable handles
       * @return the iterator; equal to end() for an empty list or an empty handle
       */
      [[nodiscard]] iterator begin() const noexcept {
         return iterator(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The end of the range of elements
       * @return the iterator
       */
      [[nodiscard]] iterator end() const noexcept {
         return iterator();
      }

      /**
       * @brief The first element whose key field holds a key, for writing; forwards to ocs::tom::find_first<V>()
       * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host
       * @param key_nm the key field
       * @param key the key
       * @return the mutable handle on the element, empty if none holds the key or the handle is empty
       * @throw FieldException as the free form
       */
      template <typename V>
         requires detail::RawKeyValue<V>
      [[nodiscard]] RawElement find_first(const int key_nm, const V key) const {
         return RawElement(ocs::tom::find_first<V>(raw(), key_nm, key));
      }

      /**
       * @brief Append an element; forwards to ocs::tom::append()
       * @param element the owner of the element; empty afterwards, keeps the element on a throw
       * @return the handle on the element, now in the list
       * @throw FieldException as the free form, NullList for an empty handle
       */
      RawElement push_back(RawElementPtr &&element) const;

      /**
       * @brief Append the element of a typed owner; forwards to ocs::tom::append(lList *, ElementPtr<T> &&)
       * @tparam T the object type of the typed owner
       * @param element the typed owner; empty afterwards, keeps the element on a throw
       * @return the handle on the element, now in the list
       * @throw FieldException as the free form, NullList for an empty handle
       */
      template <ObjectType T>
      RawElement push_back(ElementPtr<T> &&element) const;

      /**
       * @brief Insert an element after another one, or at the front; forwards to ocs::tom::insert_after()
       * @param after the predecessor, an element of this list, or an empty handle for the front
       * @param element the owner of the element; empty afterwards, keeps the element on a throw
       * @return the handle on the element, now in the list
       * @throw FieldException as the free form
       */
      RawElement insert_after(ConstRawElement after, RawElementPtr &&element) const;

      /**
       * @brief Insert the element of a typed owner after another one, or at the front; forwards to
       *        ocs::tom::insert_after(lList *, lListElem *, ElementPtr<T> &&)
       * @tparam T the object type of the typed owner
       * @param after the predecessor, an element of this list, or an empty handle for the front
       * @param element the typed owner; empty afterwards, keeps the element on a throw
       * @return the handle on the element, now in the list
       * @throw FieldException as the free form
       */
      template <ObjectType T>
      RawElement insert_after(ConstRawElement after, ElementPtr<T> &&element) const;

      /**
       * @brief Take an element out of the list without freeing it; forwards to ocs::tom::take(lList *, lListElem *)
       *
       * Taking the current element of a range-for invalidates the range iterator; fetch the successor first.
       *
       * @param element the element, an element of this list
       * @return the owner of the element
       * @throw FieldException as the free form
       */
      [[nodiscard]] RawElementPtr take(RawElement element) const;

      /**
       * @brief Take an element out of the list and free it; forwards to ocs::tom::erase(lList *, lListElem *)
       *
       * Erasing the current element of a range-for invalidates the range iterator; fetch the successor first.
       *
       * @param element the element, an element of this list; dangles afterwards
       * @throw FieldException as the free form
       */
      void erase(RawElement element) const;

      /**
       * @brief Move every element of a list to the end of this one; forwards to ocs::tom::splice()
       * @param other the owner of the list; empty afterwards, keeps the list on a throw
       * @throw FieldException as the free form
       */
      void splice(RawListPtr &&other) const;

      /**
       * @brief Move every element of the list of a typed owner to the end of this one; forwards to
       *        ocs::tom::splice(lList *, ListPtr<T> &&)
       * @tparam T the object type of the typed owner
       * @param other the typed owner; empty afterwards, keeps the list on a throw
       * @throw FieldException as the free form
       */
      template <ObjectType T>
      void splice(ListPtr<T> &&other) const;
   };

   // ---------------------------------------------------------------------------------------------------------------
   // the accessors and the lookup on a handle: a handle converts to the const and (the mutable one) to the
   // mutable pointer, so it would reach the pointer overloads only through user-defined conversions that can
   // tie; these overloads take the handle as it is, so that every function that takes the pointer takes it

   /**
    * @brief Read a field given at run time through a read-only handle; forwards to get<V>(const lListElem *, int)
    * @tparam V the value type, which names the field type
    * @param element the handle, or an empty handle
    * @param nm the field number
    * @return the value
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType, in this order
    */
   template <typename V>
      requires detail::RawConstValue<V>
   [[nodiscard]] V get(const ConstRawElement element, const int nm) {
      return detail::raw_get<V>(element.raw(), nm);
   }

   /**
    * @brief Read a field given at run time through a mutable handle; forwards to get<V>(lListElem *, int)
    * @tparam V the value type, which names the field type
    * @param element the handle, or an empty handle
    * @param nm the field number
    * @return the value
    * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType, in this order
    */
   template <typename V>
      requires detail::RawMutableValue<V>
   [[nodiscard]] V get(const RawElement element, const int nm) {
      return detail::raw_get<V>(element.raw(), nm);
   }

   // A typed handle converts to the element pointer and to the raw handles, so without the deleted overloads
   // below get<V>(h, nm), set(h, nm, v) and has(h, nm) would bind silently to the raw forms. On a typed handle
   // the member is the one way (CS-3103): h.get<V>(nm), h.set(nm, v); where the code is generic over object
   // types and the field number is a run-time value, the raw form is written explicitly on h.raw(). A
   // deleted overload taking ConstElement<T> also catches an Element<T>, which derives from it.

   /**
    * @brief Deleted: on a typed handle use the member (h.get<V>(nm)) or the raw form on h.raw() (CS-3103)
    * @tparam V the value type
    * @tparam T the object type of the handle
    * @param element the handle
    * @param nm the field number
    * @return nothing, the call does not compile
    */
   template <typename V, ObjectType T>
   V get(const ConstElement<T> &element, int nm) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->get<V>(nm)) or the raw form on ep->raw() (CS-3103)
    * @tparam V the value type
    * @tparam T the object type of the handle
    * @param element the owning handle
    * @param nm the field number
    * @return nothing, the call does not compile
    */
   template <typename V, ObjectType T>
   V get(const ElementPtr<T> &element, int nm) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.set(nm, v)) or the raw form on h.raw() (CS-3103)
    * @tparam V the value type
    * @tparam T the object type of the handle
    * @param element the handle
    * @param nm the field number
    * @param value the value
    */
   template <typename V, ObjectType T>
   void set(const ConstElement<T> &element, int nm, V value) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->set(nm, v)) or the raw form on ep->raw() (CS-3103)
    * @tparam V the value type
    * @tparam T the object type of the handle
    * @param element the owning handle
    * @param nm the field number
    * @param value the value
    */
   template <typename V, ObjectType T>
   void set(const ElementPtr<T> &element, int nm, V value) = delete;

   /**
    * @brief Deleted: a typed handle has no run-time has(); use has(h.raw(), nm) (CS-3103)
    * @tparam T the object type of the handle
    * @param element the handle
    * @param nm the field number
    * @return nothing, the call does not compile
    */
   template <ObjectType T>
   bool has(const ConstElement<T> &element, int nm) = delete;

   /**
    * @brief Deleted: an owning handle has no run-time has(); use has(ep->raw(), nm) (CS-3103)
    * @tparam T the object type of the handle
    * @param element the owning handle
    * @param nm the field number
    * @return nothing, the call does not compile
    */
   template <ObjectType T>
   bool has(const ElementPtr<T> &element, int nm) = delete;

   /**
    * @brief The first element of a list whose key field holds a key, through a read-only handle; forwards to
    *        find_first<V>(const lList *, int, V)
    * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host; names the field type
    * @param list the handle, or an empty handle for no match
    * @param key_nm the key field
    * @param key the key
    * @return the element, or nullptr if none holds the key or the handle is empty
    * @throw FieldException as find_first() on a read-only list
    */
   template <typename V>
      requires detail::RawKeyValue<V>
   [[nodiscard]] const lListElem *find_first(const ConstRawList list, const int key_nm, const V key) {
      return detail::raw_find_first<V>(list.raw(), key_nm, key);
   }

   /**
    * @brief The first element of a list whose key field holds a key, through a mutable handle; forwards to
    *        find_first<V>(lList *, int, V)
    * @tparam V the key's value type: std::uint32_t, std::uint64_t, const char * or Host; names the field type
    * @param list the handle, or an empty handle for no match
    * @param key_nm the key field
    * @param key the key
    * @return the element, or nullptr if none holds the key or the handle is empty
    * @throw FieldException as find_first() on a read-only list
    */
   template <typename V>
      requires detail::RawKeyValue<V>
   [[nodiscard]] lListElem *find_first(const RawList list, const int key_nm, const V key) {
      return detail::raw_find_first<V>(list.raw(), key_nm, key);
   }

   // ---------------------------------------------------------------------------------------------------------------
   // writing as text for debugging: the text of the old lWrite*To() functions, byte for byte. Overload resolution:
   // a list pointer (`lList *`, `const lList *`), a RawList and a typed list handle reach the list form, an
   // element pointer, a RawElement and a typed element handle the element form, through the implicit handle
   // constructors; a condition and a selection pointer reach their forms directly. An owner converts to no
   // handle; pass `*owner`. The literal nullptr would tie between the condition and the selection form and is
   // rejected by a deleted overload; an empty handle or a null pointer of a named type writes nothing.

   /**
    * @brief Write a list with its name, its number of elements and its elements as text for debugging
    * @param fp the stream, or nullptr to write nothing
    * @param list the list; an empty handle writes nothing and sets the CULL error number LELISTNULL, as before
    * @see Old form: lWriteListTo(); core: ocs::tom::print_list(); typed form: ConstList<T>::print()
    */
   void write_to(FILE *fp, ConstRawList list);

   /**
    * @brief Write an element as text for debugging
    * @param fp the stream, or nullptr to write nothing
    * @param element the element; an empty handle writes nothing and sets the CULL error number LEELEMNULL, as
    *        before
    * @see Old form: lWriteElemTo(); core: ocs::tom::print_elem(); typed form: ConstElement<T>::print()
    */
   void write_to(FILE *fp, ConstRawElement element);

   /**
    * @brief Write a condition as text for debugging
    * @param fp the stream, or nullptr to write nothing
    * @param where the condition; nullptr writes nothing and sets the CULL error number LECONDNULL, as before
    * @see Old form: lWriteWhereTo(); core: ocs::tom::print_where(); typed form: Where<T>::print()
    */
   void write_to(FILE *fp, const lCondition *where);

   /**
    * @brief Write a field selection with its nested selections as text for debugging
    * @param fp the stream, or nullptr to write nothing
    * @param what the selection; nullptr writes nothing and sets the CULL error number LEENUMNULL, as before
    * @see Old form: lWriteWhatTo(); core: ocs::tom::print_what(); typed form: What<T>::print()
    */
   void write_to(FILE *fp, const lEnumeration *what);

   /**
    * @brief The literal nullptr names no object type to write; rejected at compile time
    * @param fp the stream
    * @param null the literal nullptr
    */
   void write_to(FILE *fp, std::nullptr_t null) = delete;

   // ---------------------------------------------------------------------------------------------------------------
   // raw holders, list position, hashing and descriptor references: what the infrastructure outside the list
   // library needs of the storage layer, without its private headers

   /**
    * @brief Free a list that a raw holder keeps, and clear the holder
    *
    * For the process-lifetime globals and C struct members that hold a list as a raw pointer, since such state
    * gets no static owner. Frees as the old function did: nullptr frees nothing; a list holding an element
    * without descriptor - a corrupted list - sets the CULL error number LEDESCRNULL and aborts the process.
    *
    * @param lp the holder; nullptr afterwards
    * @see Old form: lFreeList(&lp); core: ocs::tom::free_list(), which throws instead of aborting
    */
   void free_raw_list(lList *&lp) noexcept;

   /**
    * @brief Free an element that a raw holder keeps, and clear the holder
    *
    * The element counterpart of free_raw_list(): nullptr frees nothing; an element without descriptor sets
    * the CULL error number LEDESCRNULL and aborts the process, as the old function did.
    *
    * @param ep the holder of a free element; nullptr afterwards
    * @see Old form: lFreeElem(&ep); core: ocs::tom::free_elem(), which throws instead of aborting
    */
   void free_raw_elem(lListElem *&ep) noexcept;

   /**
    * @brief The number of elements after an element in its list
    * @param element the element; an empty handle or a free element has none after it
    * @return the number of successors, 0 for the last element
    * @see Old form: lGetNumberOfRemainingElem()
    */
   [[nodiscard]] std::size_t remaining(ConstRawElement element) noexcept;

   /**
    * @brief Whether an element is free: in no list and no object field
    *
    * Unlike the old function, an empty handle does not set the CULL error number.
    *
    * @param element the element
    * @return true if the element is free, false if it is bound to a list or an object field, or the handle is
    *         empty
    * @see Old form: lIsFreeElem()
    */
   [[nodiscard]] bool is_free(ConstRawElement element) noexcept;

   /**
    * @brief Let a list created without hashing keep hash tables for the hashed fields of its type
    *
    * No table is built here: the first lookup on a long enough list builds the one of its field. A list that
    * keeps hash tables already is left as it is.
    *
    * @param list the list; an empty handle is ignored
    */
   void make_hashable(RawList list) noexcept;

   /**
    * @brief Give up a reference on a shared descriptor, as reduced_descr() hands one out, and clear the holder
    *
    * The release of the last reference frees the descriptor. The static descriptor of a type is not counted;
    * releasing it only clears the holder.
    *
    * @param descr the holder of the reference, or of nullptr; nullptr afterwards
    */
   void release_descr(const lDescr *&descr) noexcept;

   /**
    * @brief Whether a loaded shared object uses the descriptor registry of this process (N-A-22)
    *
    * The loaded object answers through its probe ocs_tom_descr_registry_seen(), which every loadable object
    * links with ocs_link_registry_probe() (cmake/OcsRegistryProbe.cmake); an object without the probe is
    * refused ("none").
    *
    * @param handle the handle dlopen() returned for the object
    * @param[out] own the file name of the object holding the registry of the caller
    * @param[out] seen the file name of the object holding the registry the loaded object uses, or "none" if it
    *             has no probe
    * @return true if the loaded object uses the registry of the caller
    */
   [[nodiscard]] bool descr_registry_shared(void *handle, std::string &own, std::string &seen);

   // ---------------------------------------------------------------------------------------------------------------
   // owners

   /**
    * @brief Owns an element of any type that is in no list, as ElementPtr<T> does for one type
    *
    * Move-only; frees the element in the destructor. Taking over a raw
    * pointer is unchecked, since there is no type to check against.
    */
   class RawElementPtr {
   public:
      /** @brief An empty owner */
      constexpr RawElementPtr() noexcept = default;

      /** @brief An empty owner, from nullptr */
      constexpr RawElementPtr(std::nullptr_t) noexcept {}

      /**
       * @brief Take over an element
       * @param ep the element, free, or nullptr; freed by the owner
       */
      explicit RawElementPtr(lListElem *ep) noexcept : ep_(ep) {}

      /**
       * @brief Take over the element of a typed owner; the way from typed to raw, which cannot fail
       *
       * Explicit, so that a typed owner passed to a `RawElementPtr &&` parameter does not become a temporary
       * that frees the element when the op throws; the ops have typed overloads instead (file comment).
       *
       * @tparam T the object type of the typed owner
       * @param typed the typed owner, empty afterwards
       */
      template <ObjectType T>
      explicit RawElementPtr(ElementPtr<T> &&typed) noexcept : ep_(typed.release()) {}

      /**
       * @brief Create an element of a full or reduced descriptor; forwards to ocs::tom::create_raw_elem()
       * @param descr the descriptor
       * @return the owner
       * @throw FieldException with reason NullDescriptor if @p descr is null or empty
       * @throw ListCoreError if there is no memory
       */
      [[nodiscard]] static RawElementPtr create(const lDescr *descr) {
         return create_raw_elem(descr);
      }

      RawElementPtr(const RawElementPtr &) = delete;
      RawElementPtr &operator=(const RawElementPtr &) = delete;

      /**
       * @brief Take over the element of another owner
       * @param other the owner, empty afterwards
       */
      RawElementPtr(RawElementPtr &&other) noexcept : ep_(std::exchange(other.ep_, nullptr)) {}

      /**
       * @brief Free the element held and take over the one of another owner
       * @param other the owner, empty afterwards
       * @return this owner
       */
      RawElementPtr &operator=(RawElementPtr &&other) noexcept {
         if (this != &other) {
            free_elem(ep_);
            ep_ = std::exchange(other.ep_, nullptr);
         }
         return *this;
      }

      /** @brief Free the element held, as lFreeElem() does */
      ~RawElementPtr() {
         free_elem(ep_);
      }

      /**
       * @brief A non-owning handle on the element
       * @return the handle, empty if this owner is
       */
      [[nodiscard]] RawElement get() const noexcept {
         return RawElement(ep_);
      }

      /**
       * @brief Call a method of the non-owning handle: `obj->set(nm, std::uint32_t{42})`
       * @return an object that leads the call on to get()
       */
      detail::Arrow<RawElement> operator->() const noexcept {
         return detail::Arrow<RawElement>(get());
      }

      /**
       * @brief The non-owning handle, as std::unique_ptr::operator* gives the object
       * @return the handle, empty if this owner is; the same as get()
       */
      [[nodiscard]] RawElement operator*() const noexcept {
         return get();
      }

      /**
       * @brief Whether the owner holds an element
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

      /**
       * @brief A deep copy of the element; forwards to ocs::tom::clone(const lListElem *)
       * @return the owner of the copy, empty if this owner is
       * @throw ListCoreError if the copy fails
       */
      [[nodiscard]] RawElementPtr clone() const {
         return ocs::tom::clone(static_cast<const lListElem *>(ep_));
      }

      /** @brief Free the element held now, as std::unique_ptr::reset() and lFreeElem() do */
      void reset() noexcept {
         free_elem(ep_);
      }

      /**
       * @brief Give up ownership
       * @return the element, which the caller now owns, or nullptr
       */
      [[nodiscard]] lListElem *release() noexcept {
         return std::exchange(ep_, nullptr);
      }

   private:
      lListElem *ep_ = nullptr;   ///< the element owned, or nullptr
   };

   /**
    * @brief Owns a list of any type, as ListPtr<T> does for one type
    *
    * Move-only; frees the list with its elements in the destructor. Taking
    * over a raw pointer is unchecked, since there is no type to check against.
    */
   class RawListPtr {
   public:
      /** @brief An empty owner */
      constexpr RawListPtr() noexcept = default;

      /** @brief An empty owner, from nullptr */
      constexpr RawListPtr(std::nullptr_t) noexcept {}

      /**
       * @brief Take over a list
       * @param lp the list, or nullptr; freed by the owner
       */
      explicit RawListPtr(lList *lp) noexcept : lp_(lp) {}

      /**
       * @brief Take over the list of a typed owner; the way from typed to raw, which cannot fail
       *
       * Explicit, so that a typed owner passed to a `RawListPtr &&` parameter does not become a temporary
       * that frees the list when the op throws; the ops have typed overloads instead (file comment).
       *
       * @tparam T the object type of the typed owner
       * @param typed the typed owner, empty afterwards
       */
      template <ObjectType T>
      explicit RawListPtr(ListPtr<T> &&typed) noexcept : lp_(typed.release()) {}

      /**
       * @brief Create an empty list of a full or reduced descriptor; forwards to ocs::tom::create_raw_list()
       * @param descr the descriptor of its elements
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @param name the name of the list, nullptr for "No list name specified"
       * @return the owner
       * @throw FieldException with reason NullDescriptor if @p descr is null or empty
       * @throw ListCoreError if there is no memory
       */
      [[nodiscard]] static RawListPtr create(const lDescr *descr, const Hashing hashing = Hashing::On,
                                             const char *name = nullptr) {
         return create_raw_list(descr, hashing, name);
      }

      RawListPtr(const RawListPtr &) = delete;
      RawListPtr &operator=(const RawListPtr &) = delete;

      /**
       * @brief Take over the list of another owner
       * @param other the owner, empty afterwards
       */
      RawListPtr(RawListPtr &&other) noexcept : lp_(std::exchange(other.lp_, nullptr)) {}

      /**
       * @brief Free the list held and take over the one of another owner
       * @param other the owner, empty afterwards
       * @return this owner
       */
      RawListPtr &operator=(RawListPtr &&other) noexcept {
         if (this != &other) {
            free_list(lp_);
            lp_ = std::exchange(other.lp_, nullptr);
         }
         return *this;
      }

      /** @brief Free the list held, as lFreeList() does */
      ~RawListPtr() {
         free_list(lp_);
      }

      /**
       * @brief A non-owning handle on the list
       * @return the handle, empty if this owner is
       */
      [[nodiscard]] RawList get() const noexcept {
         return RawList(lp_);
      }

      /**
       * @brief Call a method of the non-owning handle: `list->push_back(std::move(ep))`
       * @return an object that leads the call on to get()
       */
      detail::Arrow<RawList> operator->() const noexcept {
         return detail::Arrow<RawList>(get());
      }

      /**
       * @brief The non-owning handle, as std::unique_ptr::operator* gives the object; also for range-for
       * @return the handle, empty if this owner is; the same as get()
       */
      [[nodiscard]] RawList operator*() const noexcept {
         return get();
      }

      /**
       * @brief Whether the owner holds a list
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return lp_ != nullptr;
      }

      /**
       * @brief A deep copy of the list; forwards to ocs::tom::clone(const lList *, Hashing)
       * @param hashing whether the copy keeps hash tables
       * @return the owner of the copy, empty if this owner is
       * @throw ListCoreError if the copy fails
       */
      [[nodiscard]] RawListPtr clone(const Hashing hashing = Hashing::On) const {
         return ocs::tom::clone(static_cast<const lList *>(lp_), hashing);
      }

      /** @brief Free the list held now, as std::unique_ptr::reset() and lFreeList() do */
      void reset() noexcept {
         free_list(lp_);
      }

      /**
       * @brief Give up ownership
       * @return the list, which the caller now owns, or nullptr
       */
      [[nodiscard]] lList *release() noexcept {
         return std::exchange(lp_, nullptr);
      }

   private:
      lList *lp_ = nullptr;   ///< the list owned, or nullptr
   };

   // ---------------------------------------------------------------------------------------------------------------
   // the list ops on typed owners, defined once the raw owners are complete

   namespace detail {

      /**
       * @brief Run a raw list op on the element or list of a typed owner, with the strong guarantee of the raw op
       *
       * The typed owner hands its element or list to a raw owner for the op. The raw ops give it up only
       * after success, so on a throw the raw owner still holds it and it goes back into the typed owner.
       * Simpler than checking first, which would duplicate the checks of the raw op.
       *
       * @tparam Raw the raw owner, RawElementPtr or RawListPtr
       * @tparam Typed the typed owner, ElementPtr<T> or ListPtr<T>
       * @tparam Op the type of the op
       * @param typed the typed owner; empty afterwards unless the op throws
       * @param op the op, called with the raw owner as an rvalue
       * @throw whatever @p op throws; @p typed holds its element or list again then
       */
      template <typename Raw, typename Typed, typename Op>
      void through_raw_owner(Typed &typed, Op &&op) {
         Raw raw(typed.release());
         try {
            std::forward<Op>(op)(std::move(raw));
         } catch (...) {
            typed = Typed::from_typed(raw.release());
            throw;
         }
      }

   } // namespace detail

   /**
    * @brief Append the element of a typed owner to a list of any type
    * @tparam T the object type of the typed owner
    * @param lp the list, which takes the element over
    * @param ep the typed owner; empty afterwards. On a throw the element is back in it.
    * @throw FieldException with the reasons of append(lList *, RawElementPtr &&); nothing is changed then
    */
   template <ObjectType T>
   void append(lList *lp, ElementPtr<T> &&ep) {
      detail::through_raw_owner<RawElementPtr>(ep, [lp](RawElementPtr &&raw) { ocs::tom::append(lp, std::move(raw)); });
   }

   /**
    * @brief Insert the element of a typed owner after another one of a list of any type, or at the front
    * @tparam T the object type of the typed owner
    * @param lp the list, which takes the element over
    * @param after the predecessor, an element of @p lp, or nullptr for the front
    * @param ep the typed owner; empty afterwards. On a throw the element is back in it.
    * @throw FieldException with the reasons of insert_after(lList *, lListElem *, RawElementPtr &&)
    */
   template <ObjectType T>
   void insert_after(lList *lp, lListElem *after, ElementPtr<T> &&ep) {
      detail::through_raw_owner<RawElementPtr>(
         ep, [lp, after](RawElementPtr &&raw) { ocs::tom::insert_after(lp, after, std::move(raw)); });
   }

   /**
    * @brief Move every element of the list of a typed owner to the end of a list of any type
    * @tparam T the object type of the typed owner
    * @param dst the list taking the elements
    * @param src the typed owner; empty afterwards; an empty owner moves nothing. On a throw the list is back
    *        in it.
    * @throw FieldException with the reasons of splice(lList *, RawListPtr &&); nothing is changed then
    */
   template <ObjectType T>
   void splice(lList *dst, ListPtr<T> &&src) {
      detail::through_raw_owner<RawListPtr>(src, [dst](RawListPtr &&raw) { ocs::tom::splice(dst, std::move(raw)); });
   }

   // ---------------------------------------------------------------------------------------------------------------
   // forwarding members that return or take an owner, defined once the owners are complete

   /**
    * @brief A deep copy of the element; forwards to ocs::tom::clone(const lListElem *)
    * @return the owner of the copy, empty if the handle is empty
    * @throw ListCoreError if the copy fails
    */
   inline RawElementPtr ConstRawElement::clone() const {
      return ocs::tom::clone(raw());
   }

   /**
    * @brief A copy of the element reduced to a selection; forwards to ocs::tom::select(const lListElem *, ...)
    * @param what the selection of fields
    * @param hashing whether copied sub-lists keep hash tables
    * @return the owner of the copy, empty if the handle is empty
    * @throw FieldException and ListCoreError as the free form
    */
   inline RawElementPtr ConstRawElement::select(const lEnumeration *what, const Hashing hashing) const {
      return ocs::tom::select(raw(), what, hashing);
   }

   /**
    * @brief Store a list into a list field; forwards to ocs::tom::set(lListElem *, int, RawListPtr)
    * @param nm the field number
    * @param list the list, which the element takes over
    * @throw FieldException as the free form
    */
   inline void RawElement::set(const int nm, RawListPtr list) const {
      ocs::tom::set(raw(), nm, std::move(list));
   }

   /**
    * @brief Store an element into an object field; forwards to ocs::tom::set(lListElem *, int, RawElementPtr)
    * @param nm the field number
    * @param object the element, which the element takes over
    * @throw FieldException as the free form
    */
   inline void RawElement::set(const int nm, RawElementPtr object) const {
      ocs::tom::set(raw(), nm, std::move(object));
   }

   /**
    * @brief Swap a list field with a list; forwards to ocs::tom::exchange()
    * @param nm the field number
    * @param list the new list, which the element takes over
    * @return the owner of the list the field held
    * @throw FieldException as the free form
    */
   inline RawListPtr RawElement::exchange(const int nm, RawListPtr list) const {
      return ocs::tom::exchange(raw(), nm, std::move(list));
   }

   /**
    * @brief Take the element out of an object field; forwards to ocs::tom::take_raw_object()
    * @param nm the field number
    * @return the owner of the element the field held
    * @throw FieldException as the free form
    */
   inline RawElementPtr RawElement::take(const int nm) const {
      return ocs::tom::take_raw_object(raw(), nm);
   }

   /**
    * @brief Copy the elements that fulfil a condition, reduced to a selection; forwards to ocs::tom::select()
    * @param where the condition, or nullptr for every element
    * @param what the selection of fields
    * @param hashing whether the result and its copied sub-lists keep hash tables
    * @return the owner of the result, empty if the handle is empty
    * @throw FieldException and ListCoreError as the free form
    */
   inline RawListPtr ConstRawList::select(const lCondition *where, const lEnumeration *what,
                                          const Hashing hashing) const {
      return ocs::tom::select(raw(), where, what, hashing);
   }

   /**
    * @brief A deep copy of the list; forwards to ocs::tom::clone(const lList *, Hashing)
    * @param hashing whether the copy keeps hash tables
    * @return the owner of the copy, empty if the handle is empty
    * @throw ListCoreError if the copy fails
    */
   inline RawListPtr ConstRawList::clone(const Hashing hashing) const {
      return ocs::tom::clone(raw(), hashing);
   }

   /**
    * @brief Append an element; forwards to ocs::tom::append()
    * @param element the owner of the element; empty afterwards, keeps the element on a throw
    * @return the handle on the element, now in the list
    * @throw FieldException as the free form, NullList for an empty handle
    */
   inline RawElement RawList::push_back(RawElementPtr &&element) const {
      const RawElement added = element.get();
      ocs::tom::append(raw(), std::move(element));
      return added;
   }

   /**
    * @brief Insert an element after another one, or at the front; forwards to ocs::tom::insert_after()
    * @param after the predecessor, an element of this list, or an empty handle for the front
    * @param element the owner of the element; empty afterwards, keeps the element on a throw
    * @return the handle on the element, now in the list
    * @throw FieldException as the free form
    */
   inline RawElement RawList::insert_after(const ConstRawElement after, RawElementPtr &&element) const {
      const RawElement added = element.get();
      ocs::tom::insert_after(raw(), const_cast<lListElem *>(after.raw()), std::move(element));
      return added;
   }

   /**
    * @brief Append the element of a typed owner; forwards to ocs::tom::append(lList *, ElementPtr<T> &&)
    * @tparam T the object type of the typed owner
    * @param element the typed owner; empty afterwards, keeps the element on a throw
    * @return the handle on the element, now in the list
    * @throw FieldException as the free form, NullList for an empty handle
    */
   template <ObjectType T>
   RawElement RawList::push_back(ElementPtr<T> &&element) const {
      const RawElement added(element.get().raw());
      ocs::tom::append(raw(), std::move(element));
      return added;
   }

   /**
    * @brief Insert the element of a typed owner after another one, or at the front; forwards to
    *        ocs::tom::insert_after(lList *, lListElem *, ElementPtr<T> &&)
    * @tparam T the object type of the typed owner
    * @param after the predecessor, an element of this list, or an empty handle for the front
    * @param element the typed owner; empty afterwards, keeps the element on a throw
    * @return the handle on the element, now in the list
    * @throw FieldException as the free form
    */
   template <ObjectType T>
   RawElement RawList::insert_after(const ConstRawElement after, ElementPtr<T> &&element) const {
      const RawElement added(element.get().raw());
      ocs::tom::insert_after(raw(), const_cast<lListElem *>(after.raw()), std::move(element));
      return added;
   }

   /**
    * @brief Take an element out of the list without freeing it; forwards to ocs::tom::take(lList *, lListElem *)
    * @param element the element, an element of this list
    * @return the owner of the element
    * @throw FieldException as the free form
    */
   inline RawElementPtr RawList::take(const RawElement element) const {
      return ocs::tom::take(raw(), element.raw());
   }

   /**
    * @brief Take an element out of the list and free it; forwards to ocs::tom::erase(lList *, lListElem *)
    * @param element the element, an element of this list; dangles afterwards
    * @throw FieldException as the free form
    */
   inline void RawList::erase(const RawElement element) const {
      ocs::tom::erase(raw(), element.raw());
   }

   /**
    * @brief Move every element of a list to the end of this one; forwards to ocs::tom::splice()
    * @param other the owner of the list; empty afterwards, keeps the list on a throw
    * @throw FieldException as the free form
    */
   inline void RawList::splice(RawListPtr &&other) const {
      ocs::tom::splice(raw(), std::move(other));
   }

   /**
    * @brief Move every element of the list of a typed owner to the end of this one; forwards to
    *        ocs::tom::splice(lList *, ListPtr<T> &&)
    * @tparam T the object type of the typed owner
    * @param other the typed owner; empty afterwards, keeps the list on a throw
    * @throw FieldException as the free form
    */
   template <ObjectType T>
   void RawList::splice(ListPtr<T> &&other) const {
      ocs::tom::splice(raw(), std::move(other));
   }

   // ---------------------------------------------------------------------------------------------------------------
   // raw to typed

   /**
    * @brief Hand an element over to its typed owner, checked first
    *
    * The strong guarantee: the type is checked before ownership moves, so a
    * failure leaves the element with @p raw. `ElementPtr<T>(raw.release())`
    * would leak it when the checked constructor throws.
    *
    * @tparam T the object type
    * @param raw the owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p raw was empty
    * @throw FieldException with reason WrongType if the element is of another type
    */
   template <ObjectType T>
   [[nodiscard]] ElementPtr<T> adopt(RawElementPtr &&raw) {
      static_cast<void>(Element<T>(raw.get().raw()));
      return ElementPtr<T>::from_typed(raw.release());
   }

   /**
    * @brief Hand a list over to its typed owner, checked first; the strong guarantee as for an element
    * @tparam T the object type
    * @param raw the owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p raw was empty
    * @throw FieldException with reason WrongType if the list holds elements of another type
    */
   template <ObjectType T>
   [[nodiscard]] ListPtr<T> adopt(RawListPtr &&raw) {
      static_cast<void>(List<T>(raw.get().raw()));
      return ListPtr<T>::from_typed(raw.release());
   }

   /**
    * @brief Hand the element of a typed owner over to the typed owner of a type given here, checked first
    *
    * For an element whose type the code knows better than its owner says, e.g. one taken out of an untyped
    * field. The check runs on the element before ownership moves, so a failure leaves it with @p typed;
    * there is no temporary raw owner that could free it.
    *
    * @tparam T the object type of the new owner
    * @tparam U the object type of @p typed
    * @param typed the typed owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p typed was empty
    * @throw FieldException with reason WrongType if the element is not of type @p T
    */
   template <ObjectType T, ObjectType U>
   [[nodiscard]] ElementPtr<T> adopt(ElementPtr<U> &&typed) {
      static_cast<void>(Element<T>(typed.get().raw()));
      return ElementPtr<T>::from_typed(typed.release());
   }

   /**
    * @brief Hand the list of a typed owner over to the typed owner of a type given here, checked first; the
    *        strong guarantee as for an element
    * @tparam T the object type of the new owner
    * @tparam U the object type of @p typed
    * @param typed the typed owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p typed was empty
    * @throw FieldException with reason WrongType if the list holds elements of another type than @p T
    */
   template <ObjectType T, ObjectType U>
   [[nodiscard]] ListPtr<T> adopt(ListPtr<U> &&typed) {
      static_cast<void>(List<T>(typed.get().raw()));
      return ListPtr<T>::from_typed(typed.release());
   }

   /**
    * @brief Hand a condition over to its typed owner, checked first with condition_fits()
    * @tparam T the object type
    * @param raw the owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p raw was empty
    * @throw FieldException with reason ForeignField if the condition compares a field the type does not have
    */
   template <ObjectType T>
   [[nodiscard]] Where<T> adopt(RawWherePtr &&raw) {
      if (!condition_fits(raw.get(), T::descriptor.e)) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignField, NoName, T::descriptor.e);
      }
      return Where<T>::from_typed(raw.release());
   }

   /**
    * @brief Hand a field selection over to its typed owner, checked first against the type's descriptor
    * @tparam T the object type
    * @param raw the owner; empty afterwards unless the check fails
    * @return the typed owner, empty if @p raw was empty
    * @throw FieldException with reason ForeignField if the selection names a field the type does not have
    */
   template <ObjectType T>
   [[nodiscard]] What<T> adopt(RawWhatPtr &&raw) {
      if (raw != nullptr && !ocs::tom::detail::what_fits(raw.get(), T::descriptor.e)) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignField, NoName, T::descriptor.e);
      }
      return What<T>::from_typed(raw.release());
   }

   // the members of the typed element handle for the fields without a sub-type that return a raw owner, defined
   // here, where the raw owners are complete

   /**
    * @brief Element::exchange() of a list field without a sub-type: puts in a list of any type, gives back the
    *        list held before as a raw owner
    * @tparam T the object type of the handle
    * @tparam F a list field of T for which the model names no sub-type
    * @tparam U the struct of the new list's object type
    * @param list the new list, which the element takes over; may be empty
    * @return the list held before, now owned by the caller; empty if the field held none
    * @throw FieldException with reason EmptyHandle if the handle is empty, AbsentField if the reduced element does
    *        not hold the field; nothing is changed then
    */
   template <ObjectType T>
   template <auto F, ObjectType U>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) && detail::NoSubType<F>
   RawListPtr Element<T>::exchange(ListPtr<U> &&list) const {
      static_cast<void>(this->template present_slot<F>());
      lList *lp = list.release();
      exchange_list(this->ep_, FieldTraits<F>::pos, lp);
      return RawListPtr(lp);
   }

   /**
    * @brief Element::exchange() of a list field without a sub-type with a raw owner: puts in a list of any type,
    *        gives back the list held before as a raw owner
    * @tparam T the object type of the handle
    * @tparam F a list field of T for which the model names no sub-type
    * @param list the new list, which the element takes over; may be empty
    * @return the list held before, now owned by the caller; empty if the field held none
    * @throw FieldException with reason EmptyHandle if the handle is empty, AbsentField if the reduced element does
    *        not hold the field; nothing is changed then
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) && detail::NoSubType<F>
   RawListPtr Element<T>::exchange(RawListPtr &&list) const {
      static_cast<void>(this->template present_slot<F>());
      lList *lp = list.release();
      exchange_list(this->ep_, FieldTraits<F>::pos, lp);
      return RawListPtr(lp);
   }

   /**
    * @brief Element::exchange() of an object field without a sub-type: puts in an element of any type, gives back
    *        the element held before as a raw owner
    * @tparam T the object type of the handle
    * @tparam F an object field of T for which the model names no sub-type
    * @tparam U the struct of the new element's object type
    * @param elem the new element, which the field takes over; may be empty
    * @return the element held before, now free and owned by the caller; empty if the field held none
    * @throw FieldException with reason EmptyHandle if the handle is empty, AbsentField if the reduced element does
    *        not hold the field (a free element stays with the caller), BoundElement if the element is bound in a
    *        list or another object already (@p elem is empty afterwards); the field is unchanged then
    */
   template <ObjectType T>
   template <auto F, ObjectType U>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Object) && detail::NoSubType<F>
   RawElementPtr Element<T>::exchange(ElementPtr<U> &&elem) const {
      // a bound element is given up before present_slot() can throw, so that the caller's owner never frees it
      lListElem *const object = detail::release_if_bound(elem);
      static_cast<void>(this->template present_slot<F>());
      if (object != nullptr && !elem) [[unlikely]] {
         // given up above: it is bound in a list or another object, also when it is the one the field holds
         throw_field_exception(FieldException::Reason::BoundElement, F, this->ep_->descr);
      }
      RawElementPtr old(take_object(this->ep_, FieldTraits<F>::pos));
      // the field is empty now and the element free, so the store cannot be refused
      static_cast<void>(store_object(this->ep_, FieldTraits<F>::pos, elem.release()));
      return old;
   }

} // namespace ocs::tom
