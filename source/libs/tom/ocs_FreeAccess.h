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
 * @brief Free forms of the keyed and field-taking operations, for the mechanical rewrite (CS-3045)
 *
 * The mechanical rewrite replaces a call of the old interface with a constant
 * field number by a template that takes the field as a template argument, on
 * the same raw pointers. For the accessors these are ocs::tom::get<F>(),
 * set<F>() and add<F>() in ocs_Element.h; this file adds the same for the
 * other functions that take a field: the lookups, adding and deleting by a
 * key, in a list and in the list of a list field, the presence check,
 * exchanging and swapping list fields, and a few small ones.
 *
 * Each form checks at compile time what the old function could only check at
 * run time, or not at all: that the key has the field's type, that the key
 * field belongs to the sub-type of a list field, and it takes the descriptor
 * from the field instead of a parameter. At run time it keeps the error
 * behaviour of the old function it replaces, the same returns, messages and
 * soft-path records, so that the rewrite changes no behaviour
 * (N-R-5). Where the old function aborts, the free form throws a
 * FieldException, which unconverted code does not catch, so it ends in an
 * abort as well.
 *
 * The names follow the typed operations of the handles (find_first,
 * emplace_back, erase, exchange, swap, has, get_or_create); a name ending in
 * _rw gives a mutable element where the plain one gives a const element, as
 * the old function pair did, and a name ending in _in works on the list of a
 * list field of an element, as the old function with "Sub" in its name did.
 */

#include <concepts>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SoftPath.h"

namespace ocs::tom {

   /**
    * @brief The old function whose message a free form gives when a list does not hold the key field
    */
   enum class KeyOp : std::uint8_t {
      FindStr,      ///< lGetElemStr() and its variants
      FindHost,     ///< lGetElemHost() and its variants
      FindUlong,    ///< lGetElemUlong() and its variants
      FindUlong64,  ///< lGetElemUlong64() and its variants
      FindNocase,   ///< lGetElemCaseStr()
      FindLike,     ///< lGetElemStrLike()
      AddStr,       ///< lAddElemStr()
      AddHost,      ///< lAddElemHost()
      AddUlong,     ///< lAddElemUlong()
      AddUlong64    ///< lAddElemUlong64()
   };

   /**
    * @brief What the old lookups and adds do when a list does not hold the key field: a message and a soft path
    *
    * Writes the message of the old function @p op stands for, as CRITICAL,
    * and records the soft path.
    *
    * @param op the old function whose message is written
    * @param function the calling function, for the record of the soft path
    * @param nm the key field
    * @param descr the descriptor of the list, or nullptr
    */
   [[gnu::cold]] void missing_key_field(KeyOp op, const char *function, int nm, const lDescr *descr);

   namespace detail {

      /**
       * @brief The read-only handle the free forms return for an element found by key field F
       *
       * The element's type is the key field's type, known at compile time, so
       * the handle is made without a check (from_typed()). It converts
       * implicitly to `const lListElem *`, so code holding the old pointer
       * keeps working.
       *
       * @tparam F the key field
       */
      template <auto F>
      using ConstElementOf = ConstElement<typename FieldTraits<F>::Type>;

      /**
       * @brief The mutable handle the free forms return for an element found or added by key field F
       *
       * Converts implicitly to `lListElem *`; see ConstElementOf.
       *
       * @tparam F the key field
       */
      template <auto F>
      using ElementOf = Element<typename FieldTraits<F>::Type>;

      /**
       * @brief The key type of a free form: a number, or a C string for a string or host field
       * @tparam F the key field
       */
      template <auto F>
      using RawKey = KeyArgOf<FieldTraits<F>::type>;

      /**
       * @brief Whether a field of the typed object model can be a key of a free lookup, add or delete
       * @tparam F the field
       */
      template <auto F>
      concept FreeKeyField = TypedField<decltype(F)> && LookupField<F>;

      /**
       * @brief Whether a key field belongs to the sub-type of a list field
       * @tparam LF the list field
       * @tparam F the key field
       */
      template <auto LF, auto F>
      concept SubKeyField = TypedField<decltype(LF)> && FreeKeyField<F> && FieldTraits<LF>::type == FieldType::List &&
                            std::is_same_v<typename FieldTraits<LF>::Sub, typename FieldTraits<F>::Type>;

      /**
       * @brief Whether a field is a list field with a known sub-type
       * @tparam F the field
       */
      template <auto F>
      concept TypedListField = TypedField<decltype(F)> && FieldTraits<F>::type == FieldType::List &&
                               !std::is_void_v<typename FieldTraits<F>::Sub>;

      /**
       * @brief Whether a key is a string or a host name, which the old functions check for nullptr
       * @tparam F the key field
       */
      template <auto F>
      inline constexpr bool StringKey =
         FieldTraits<F>::type == FieldType::String || FieldTraits<F>::type == FieldType::Host;

      /**
       * @brief The old lookup whose message a free lookup by a field gives
       * @tparam F the key field
       */
      template <auto F>
      inline constexpr KeyOp FindOp = FieldTraits<F>::type == FieldType::Uint32   ? KeyOp::FindUlong
                                      : FieldTraits<F>::type == FieldType::Uint64 ? KeyOp::FindUlong64
                                      : FieldTraits<F>::type == FieldType::String ? KeyOp::FindStr
                                                                                  : KeyOp::FindHost;

      /**
       * @brief The old add whose message a free add by a field gives
       * @tparam F the key field
       */
      template <auto F>
      inline constexpr KeyOp AddOp = FieldTraits<F>::type == FieldType::Uint32   ? KeyOp::AddUlong
                                     : FieldTraits<F>::type == FieldType::Uint64 ? KeyOp::AddUlong64
                                     : FieldTraits<F>::type == FieldType::String ? KeyOp::AddStr
                                                                                 : KeyOp::AddHost;

      /**
       * @brief The first element of a list with a key, with the error behaviour of lGetElem<Type>First()
       *
       * A null list or a null string key finds nothing. A list that does not
       * hold the field writes the old message, records the soft path and finds
       * nothing. A key held twice in a unique field throws (N-H-18), where the
       * old lookup aborts.
       *
       * @tparam F the key field
       * @param lp the list, or nullptr
       * @param key the key
       * @param iterator the lookup's state, for #free_find_next
       * @param function the calling form, for the record of a soft path
       * @return the element, or nullptr
       * @throw FieldException with reason DuplicateKey
       */
      template <auto F>
      lListElem *free_find_first(const lList *lp, const RawKey<F> key, const void **iterator, const char *function) {
         if constexpr (StringKey<F>) {
            if (key == nullptr) {
               return nullptr;
            }
         }
         if (lp == nullptr) {
            return nullptr;
         }
         const int pos = typed_pos<F>(lp->descr);
         if (pos < 0) [[unlikely]] {
            missing_key_field(FindOp<F>, function, F, lp->descr);
            return nullptr;
         }
         return ocs::tom::lookup_first<FieldTraits<F>::type>(lp, pos, key, iterator);
      }

      /**
       * @brief The next element of a list with a key, with the error behaviour of lGetElem<Type>Next()
       *
       * An iterator at its end, a null string key or a null list finds
       * nothing; for a number key a null list writes the old message, as the
       * old function did when it asked a null descriptor for the field.
       *
       * @tparam F the key field
       * @param lp the list
       * @param key the key
       * @param iterator the lookup's state from #free_find_first
       * @param function the calling form, for the record of a soft path
       * @return the element, or nullptr
       */
      template <auto F>
      lListElem *free_find_next(const lList *lp, const RawKey<F> key, const void **iterator, const char *function) {
         if (*iterator == nullptr) {
            return nullptr;
         }
         if constexpr (StringKey<F>) {
            if (key == nullptr || lp == nullptr) {
               return nullptr;
            }
         } else {
            if (lp == nullptr) [[unlikely]] {
               missing_key_field(FindOp<F>, function, F, nullptr);
               return nullptr;
            }
         }
         const int pos = typed_pos<F>(lp->descr);
         if (pos < 0) [[unlikely]] {
            missing_key_field(FindOp<F>, function, F, lp->descr);
            return nullptr;
         }
         return ocs::tom::lookup_next<FieldTraits<F>::type>(lp, pos, key, iterator);
      }

      /**
       * @brief The first element of a list whose string field equals a value without regard to case, or is like it
       *
       * With the error behaviour of lGetElemCaseStr() and lGetElemStrLike():
       * a null list or value finds nothing, a list that does not hold the
       * field writes the old message, records the soft path and finds nothing.
       *
       * @tparam F the string field
       * @tparam Like true for the pattern of lGetElemStrLike(), false for lGetElemCaseStr()
       * @param lp the list, or nullptr
       * @param value the value, or nullptr
       * @param function the calling form, for the record of a soft path
       * @return the element, or nullptr
       */
      template <auto F, bool Like>
      lListElem *free_find_string(const lList *lp, const char *value, const char *function) {
         if (lp == nullptr || value == nullptr) {
            return nullptr;
         }
         const int pos = typed_pos<F>(lp->descr);
         if (pos < 0) [[unlikely]] {
            missing_key_field(Like ? KeyOp::FindLike : KeyOp::FindNocase, function, F, lp->descr);
            return nullptr;
         }
         return Like ? lookup_first_str_like_at(lp, pos, value) : lookup_first_str_nocase_at(lp, pos, value);
      }

      /**
       * @brief Add an element with a key, with the error behaviour of lAddElem<Type>()
       *
       * A list that does not exist yet is no error: if *lpp is nullptr, the
       * list is created with the descriptor of the key field's type, and freed
       * again if adding the element fails, so that *lpp is nullptr then. Only
       * the reference itself must not be nullptr.
       *
       * Errors, each as lAddElem<Type>() reports it:
       * - a null list reference or a null string key records the soft path and
       *   gives nullptr;
       * - a list of another type, or a reduced list without the field, writes
       *   the old message, records the soft path and gives nullptr, as the old
       *   function does since CS-3045;
       * - a ListCoreError of the core, a run-time failure such as no memory for
       *   the list, the element or a copy of the key, gives nullptr, as ocs_add_by_key()
       *   in the list library maps it for the old function. The typed
       *   List<T>::emplace_back<F>() lets the same exception through as a
       *   run-time error instead;
       * - a duplicate key in a unique field throws in a debug build, where the
       *   old function aborts.
       *
       * @tparam F the key field; its type gives the descriptor of the new element
       * @param lpp the list reference, not nullptr for an add; the list it refers to is created if it is nullptr
       * @param key the key
       * @param function the calling form, for the record of a soft path
       * @return the new element, or nullptr
       * @throw FieldException with reason DuplicateKey
       */
      template <auto F>
      lListElem *free_emplace_back(lList **lpp, const RawKey<F> key, const char *function) {
         bool null_key = false;
         if constexpr (StringKey<F>) {
            null_key = key == nullptr;
         }
         if (lpp == nullptr || null_key) {
            report_soft_path(function, F, SoftPathReason::NullArgument);
            return nullptr;
         }
         // a list of another type, or reduced without the field, gives the old message, as add_elem_*_at() refuses it
         if (*lpp != nullptr && typed_pos<F>((*lpp)->descr) < 0) [[unlikely]] {
            missing_key_field(AddOp<F>, function, F, (*lpp)->descr);
            return nullptr;
         }
         const lDescr *descr = FieldTraits<F>::Type::descriptor.e;
         constexpr int pos = FieldTraits<F>::pos;
         try {
            return add_elem_at<FieldTraits<F>::type>(*lpp, pos, key, descr);
         } catch (const ListCoreError &) {
            // no memory: no element
         }
         return nullptr;
      }

      /**
       * @brief Remove the first element with a key, with the error behaviour of lDelElem<Type>()
       *
       * A null string key or a null list reference records the soft path and
       * returns 0; a list that is nullptr already returns 1. An element found
       * is removed and freed and 1 returned, none found returns 0, as does a
       * list of another type. A list that becomes empty is freed and set to
       * nullptr. A key held twice in a unique field throws, where the old
       * function aborts.
       *
       * @tparam F the key field
       * @param lpp the list reference
       * @param key the key
       * @param function the calling form, for the record of a soft path
       * @return 1 or 0, as described
       * @throw FieldException with reason DuplicateKey
       */
      template <auto F>
      int free_erase(lList **lpp, const RawKey<F> key, const char *function) {
         if constexpr (StringKey<F>) {
            if (key == nullptr) {
               report_soft_path(function, F, SoftPathReason::NullArgument);
               return 0;
            }
         }
         if (lpp == nullptr) {
            report_soft_path(function, SoftPathNoName, SoftPathReason::NullArgument);
            return 0;
         }
         if (*lpp == nullptr) {
            return 1;
         }
         const int pos = typed_pos<F>((*lpp)->descr);
         if (pos < 0) [[unlikely]] {
            // a list of another type finds nothing, as the old lookup did
            return 0;
         }
         if (!remove_elem_at<FieldTraits<F>::type>(*lpp, pos, key)) {
            return 0;
         }
         if ((*lpp)->nelem == 0) {
            free_list(*lpp);
         }
         return 1;
      }

      /**
       * @brief The reference to the list held in a list field, for the forms working on a sub-list
       *
       * As every old function working on a sub-list: a null element records
       * the soft path and gives nullptr; an element that does not hold the list
       * field throws, where the old function aborts.
       *
       * @tparam LF the list field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element, or nullptr
       * @param function the calling form, for the record of a soft path
       * @return the reference, or nullptr for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto LF, bool Typed = false>
      lList **sub_list_ref(const lListElem *ep, const char *function) {
         if (ep == nullptr) {
            report_soft_path(function, LF, SoftPathReason::NullElement);
            return nullptr;
         }
         if constexpr (Typed) {
            check_holds<LF, true>(ep);
            return &ocs::tom::detail::value(ep, FieldTraits<LF>::pos).glp;
         }
         const int pos = typed_pos<LF>(ep->descr);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(ep->descr[0].nm == FieldTraits<LF>::Type::fields[0].nm
                                     ? FieldException::Reason::AbsentField
                                     : FieldException::Reason::ForeignField,
                                  LF, ep->descr);
         }
         return &ocs::tom::detail::value(ep, pos).glp;
      }

      /**
       * @brief The position of a field an old setter-like function writes, with the setters' contract (N-L-4)
       *
       * A null element records the soft path and gives -1. An
       * element that does not hold the field throws, where the old function
       * aborts.
       *
       * @tparam F the field
       * @tparam Typed true if the element comes from a handle of the field's type, see check_holds()
       * @param ep the element, or nullptr
       * @param function the calling form, for the record of a soft path
       * @return the field's position, or -1 for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto F, bool Typed = false>
      int setter_pos(const lListElem *ep, const char *function) {
         using Tr = FieldTraits<F>;
         if (ep == nullptr) [[unlikely]] {
            report_soft_path(function, static_cast<int>(F), SoftPathReason::NullElement);
            return -1;
         }
         check_holds<F, Typed>(ep);
         return Tr::pos;
      }

      /**
       * @brief The position of a field an old getter-like function reads, with the getters' contract
       *
       * Every error throws, where the old function aborts: a null element, an
       * element of another type, a reduced element without the field.
       *
       * @tparam F the field
       * @tparam Typed true if the element comes from a handle of the field's type, see check_holds()
       * @param ep the element
       * @return the field's position
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed = false>
      int getter_pos(const lListElem *ep) {
         check_holds<F, Typed>(ep);
         return FieldTraits<F>::pos;
      }

   } // namespace detail

   // ---------------------------------------------------------------------------------------------------------------
   // lookups in a list

   /**
    * @brief The first element of a list with a key, read only; replaces lGetElemStr(), lGetElemHost(), lGetElemUlong() and lGetElemUlong64()
    *
    * With the error behaviour of the old lookup: a null list or a null string
    * key finds nothing; a list that does not hold the field writes the old
    * message and finds nothing.
    *
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list, or nullptr
    * @param key the key
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException with reason DuplicateKey for a key held twice in a unique field, where the old lookup aborts
    * @see Typed form: ocs::tom::ConstList<T>::find_first<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ConstElementOf<F> find_first(const lList *lp, const detail::RawKey<F> key) {
      const void *iterator = nullptr;
      return detail::ConstElementOf<F>::from_typed(detail::free_find_first<F>(lp, key, &iterator, "ocs::tom::find_first"));
   }

   /**
    * @brief The first element of a list with a key; replaces lGetElemStrRW(), lGetElemHostRW(), lGetElemUlongRW() and lGetElemUlong64RW()
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list, or nullptr
    * @param key the key
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException with reason DuplicateKey, see #find_first
    * @see Typed form: ocs::tom::List<T>::find_first<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ElementOf<F> find_first_rw(const lList *lp, const detail::RawKey<F> key) {
      const void *iterator = nullptr;
      return detail::ElementOf<F>::from_typed(detail::free_find_first<F>(lp, key, &iterator, "ocs::tom::find_first_rw"));
   }

   /**
    * @brief The first element of a list with a key, starting an iteration, read only; replaces lGetElem<Type>First()
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list, or nullptr
    * @param key the key
    * @param iterator the lookup's state, for #find_next
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException with reason DuplicateKey, see #find_first
    * @see Typed form: ocs::tom::ConstList<T>::find_all<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ConstElementOf<F> find_first(const lList *lp, const detail::RawKey<F> key, const void **iterator) {
      return detail::ConstElementOf<F>::from_typed(detail::free_find_first<F>(lp, key, iterator, "ocs::tom::find_first"));
   }

   /**
    * @brief The first element of a list with a key, starting an iteration; replaces lGetElem<Type>FirstRW()
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list, or nullptr
    * @param key the key
    * @param iterator the lookup's state, for #find_next_rw
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException with reason DuplicateKey, see #find_first
    * @see Typed form: ocs::tom::List<T>::find_all<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ElementOf<F> find_first_rw(const lList *lp, const detail::RawKey<F> key, const void **iterator) {
      return detail::ElementOf<F>::from_typed(detail::free_find_first<F>(lp, key, iterator, "ocs::tom::find_first_rw"));
   }

   /**
    * @brief The next element of a list with a key, read only; replaces lGetElem<Type>Next()
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list
    * @param key the key
    * @param iterator the lookup's state from #find_first
    * @return the element as a handle of the key field's type, empty after the last; it converts to the old pointer
    * @see Typed form: ocs::tom::ConstList<T>::find_all<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ConstElementOf<F> find_next(const lList *lp, const detail::RawKey<F> key, const void **iterator) {
      return detail::ConstElementOf<F>::from_typed(detail::free_find_next<F>(lp, key, iterator, "ocs::tom::find_next"));
   }

   /**
    * @brief The next element of a list with a key; replaces lGetElem<Type>NextRW()
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lp the list
    * @param key the key
    * @param iterator the lookup's state from #find_first_rw
    * @return the element as a handle of the key field's type, empty after the last; it converts to the old pointer
    * @see Typed form: ocs::tom::List<T>::find_all<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   [[nodiscard]] detail::ElementOf<F> find_next_rw(const lList *lp, const detail::RawKey<F> key, const void **iterator) {
      return detail::ElementOf<F>::from_typed(detail::free_find_next<F>(lp, key, iterator, "ocs::tom::find_next_rw"));
   }

   /**
    * @brief Deleted: on a typed list use the member lp.find_first<F>(key) on a List<T> (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to const lList *;
   // it also catches a List<T>, which derives from ConstList<T>
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first_rw(const ConstList<T> &lp, const detail::RawKey<F> key) = delete;

   /**
    * @brief Deleted: on a typed list use a range-for over lp.find_all<F>(key) on a List<T> (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to const lList *;
   // it also catches a List<T>, which derives from ConstList<T>
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first_rw(const ConstList<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on a typed list use a range-for over lp.find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to const lList *;
   // it also catches a List<T>, which derives from ConstList<T>
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first(const ConstList<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on a typed list use a range-for over lp.find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to const lList *;
   // it also catches a List<T>, which derives from ConstList<T>
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_next(const ConstList<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on a typed list use a range-for over lp.find_all<F>(key) on a List<T> (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to const lList *;
   // it also catches a List<T>, which derives from ConstList<T>
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_next_rw(const ConstList<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on an owner use the member lp->find_first<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lList *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first_rw(const ListPtr<T> &lp, const detail::RawKey<F> key) = delete;

   /**
    * @brief Deleted: on an owner use a range-for over lp->find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lList *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first_rw(const ListPtr<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on an owner use a range-for over lp->find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lList *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_first(const ListPtr<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on an owner use a range-for over lp->find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lList *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_next(const ListPtr<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief Deleted: on an owner use a range-for over lp->find_all<F>(key) (CS-3103)
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @param lp the handle
    * @param key the key
    * @param iterator the lookup's state
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lList *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::FreeKeyField<F>
   auto find_next_rw(const ListPtr<T> &lp, const detail::RawKey<F> key, const void **iterator) = delete;

   /**
    * @brief The first element of a list whose string field equals a value without regard to case, read only; replaces lGetElemCaseStr()
    * @tparam F the string field
    * @param lp the list, or nullptr
    * @param value the value, or nullptr
    * @return the element as a handle of the field's type, empty if none; it converts to the old pointer; a list without the field writes the old message
    * @see Typed form: ocs::tom::ConstList<T>::find_first_nocase<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   [[nodiscard]] detail::ConstElementOf<F> find_first_nocase(const lList *lp, const char *value) {
      return detail::ConstElementOf<F>::from_typed(detail::free_find_string<F, false>(lp, value, "ocs::tom::find_first_nocase"));
   }

   /**
    * @brief The first element of a list whose string field equals a value without regard to case; replaces lGetElemCaseStrRW()
    * @tparam F the string field
    * @param lp the list, or nullptr
    * @param value the value, or nullptr
    * @return the element as a handle of the field's type, empty if none; it converts to the old pointer; a list without the field writes the old message
    * @see Typed form: ocs::tom::List<T>::find_first_nocase<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   [[nodiscard]] detail::ElementOf<F> find_first_nocase_rw(const lList *lp, const char *value) {
      return detail::ElementOf<F>::from_typed(detail::free_find_string<F, false>(lp, value, "ocs::tom::find_first_nocase_rw"));
   }

   /**
    * @brief The first element of a list whose string field equals a value, or starts with it if it ends in '*', read only; replaces lGetElemStrLike()
    * @tparam F the string field
    * @param lp the list, or nullptr
    * @param value the value or prefix pattern, or nullptr
    * @return the element as a handle of the field's type, empty if none; it converts to the old pointer; a list without the field writes the old message
    * @see Typed form: ocs::tom::ConstList<T>::find_first_like<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   [[nodiscard]] detail::ConstElementOf<F> find_first_like(const lList *lp, const char *value) {
      return detail::ConstElementOf<F>::from_typed(detail::free_find_string<F, true>(lp, value, "ocs::tom::find_first_like"));
   }

   /**
    * @brief The first element of a list whose string field equals a value, or starts with it if it ends in '*'; replaces lGetElemStrLikeRW()
    * @tparam F the string field
    * @param lp the list, or nullptr
    * @param value the value or prefix pattern, or nullptr
    * @return the element as a handle of the field's type, empty if none; it converts to the old pointer; a list without the field writes the old message
    * @see Typed form: ocs::tom::List<T>::find_first_like<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   [[nodiscard]] detail::ElementOf<F> find_first_like_rw(const lList *lp, const char *value) {
      return detail::ElementOf<F>::from_typed(detail::free_find_string<F, true>(lp, value, "ocs::tom::find_first_like_rw"));
   }

   // ---------------------------------------------------------------------------------------------------------------
   // lookups in the list of a list field

   // The forms of this header that take a raw pointer have no flag to skip the type check: a `list_ref<F, true>(h)`
   // would bind to the raw form through the implicit conversion of a typed handle and pass by the deleted
   // overloads further down (CS-3103). The members of the handles, which know the element's type, call the
   // detail::free_* cores with Typed = true instead.

   namespace detail {
      /**
       * @brief The core of find_first_in() and find_first_in_rw(): the first element with a key in the list of a
       *        list field
       * @tparam LF the list field
       * @tparam F the key field, of the list field's sub-type
       * @tparam Typed true if the element comes from a handle of the list field's type: only an absent field is
       *         checked
       * @param ep the element, or nullptr
       * @param key the key
       * @param function the calling form, for the record of a soft path
       * @return the element found, or nullptr for none and for a null element
       * @throw FieldException with reason ForeignField or AbsentField for an element without the list field, or
       *        DuplicateKey
       */
      template <auto LF, auto F, bool Typed>
      lListElem *free_find_first_in(const lListElem *ep, const RawKey<F> key, const char *function) {
         lList **lpp = sub_list_ref<LF, Typed>(ep, function);
         const void *iterator = nullptr;
         return free_find_first<F>(lpp != nullptr ? *lpp : nullptr, key, &iterator, function);
      }
   } // namespace detail

   /**
    * @brief The first element with a key in the list of a list field, read only; replaces lGetSubStr(), lGetSubHost(), lGetSubUlong() and lGetSubUlong64()
    *
    * As the old function: a null element finds nothing and records the soft
    * path, an element without the list field throws where the old function
    * aborts, and the lookup in the list behaves as #find_first.
    *
    * @tparam LF the list field
    * @tparam F the key field, of the list field's sub-type
    * @param ep the element, or nullptr
    * @param key the key
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException with reason ForeignField or AbsentField for an element without the list field, or
    *        DuplicateKey
    * @see Typed form: ocs::tom::ConstElement<T>::find_first_in<LF, F>()
    */
   template <auto LF, auto F>
      requires detail::SubKeyField<LF, F>
   [[nodiscard]] detail::ConstElementOf<F> find_first_in(const lListElem *ep, const detail::RawKey<F> key) {
      return detail::ConstElementOf<F>::from_typed(
         detail::free_find_first_in<LF, F, false>(ep, key, "ocs::tom::find_first_in"));
   }

   /**
    * @brief The first element with a key in the list of a list field; replaces lGetSubStrRW(), lGetSubHostRW(), lGetSubUlongRW() and lGetSubUlong64RW()
    * @tparam LF the list field
    * @tparam F the key field, of the list field's sub-type
    * @param ep the element, or nullptr
    * @param key the key
    * @return the element as a handle of the key field's type, empty if none; it converts to the old pointer
    * @throw FieldException as #find_first_in
    * @see Typed form: ocs::tom::ConstElement<T>::find_first_in_rw<LF, F>()
    */
   template <auto LF, auto F>
      requires detail::SubKeyField<LF, F>
   [[nodiscard]] detail::ElementOf<F> find_first_in_rw(const lListElem *ep, const detail::RawKey<F> key) {
      return detail::ElementOf<F>::from_typed(
         detail::free_find_first_in<LF, F, false>(ep, key, "ocs::tom::find_first_in_rw"));
   }

   // ---------------------------------------------------------------------------------------------------------------
   // adding and deleting by a key

   /**
    * @brief Create an element with a key and append it to a list, created if needed; replaces lAddElemStr(), lAddElemHost(), lAddElemUlong() and lAddElemUlong64()
    *
    * The descriptor of the new element is the key field's type; the old
    * function took it as a parameter. A list that is nullptr is created. With
    * the error behaviour of the old function: a null list reference or a null
    * string key gives nullptr and records the soft path, a list of another
    * type writes the old message, and a run-time failure of the core
    * (ListCoreError, such as no memory) gives nullptr; see detail::free_emplace_back().
    *
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lpp the list reference; the list is created if it is nullptr
    * @param key the key
    * @return the new element as a handle of the key field's type, empty on an error; it converts to the old pointer
    * @throw FieldException with reason DuplicateKey in a debug build, where the old function aborts
    * @see Typed form: ocs::tom::List<T>::emplace_back<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   detail::ElementOf<F> emplace_back(lList **lpp, const detail::RawKey<F> key) {
      return detail::ElementOf<F>::from_typed(detail::free_emplace_back<F>(lpp, key, "ocs::tom::emplace_back"));
   }

   namespace detail {
      /**
       * @brief The core of emplace_back_in(): create an element with a key and append it to the list of a list
       *        field, created if needed
       * @tparam LF the list field
       * @tparam F the key field, of the list field's sub-type
       * @tparam Typed true if the element comes from a handle of the list field's type: only an absent field is
       *         checked
       * @param ep the element, or nullptr
       * @param key the key
       * @return the new element, or nullptr for a null element and on an error
       * @throw FieldException with reason ForeignField or AbsentField, or DuplicateKey as #emplace_back
       */
      template <auto LF, auto F, bool Typed>
      lListElem *free_emplace_back_in(lListElem *ep, const RawKey<F> key) {
         lList **lpp = sub_list_ref<LF, Typed>(ep, "ocs::tom::emplace_back_in");
         return lpp != nullptr ? free_emplace_back<F>(lpp, key, "ocs::tom::emplace_back_in") : nullptr;
      }
   } // namespace detail

   /**
    * @brief Create an element with a key and append it to the list of a list field, created if needed; replaces lAddSubStr(), lAddSubHost(), lAddSubUlong() and lAddSubUlong64()
    *
    * A null element does nothing and records the soft path; an element
    * without the list field throws, where the old function aborts.
    *
    * @tparam LF the list field
    * @tparam F the key field, of the list field's sub-type
    * @param ep the element, or nullptr
    * @param key the key
    * @return the new element as a handle of the key field's type, empty on an error; it converts to the old pointer
    * @throw FieldException with reason ForeignField or AbsentField, or DuplicateKey as #emplace_back
    * @see Typed form: ocs::tom::Element<T>::emplace_back_in<LF, F>()
    */
   template <auto LF, auto F>
      requires detail::SubKeyField<LF, F>
   detail::ElementOf<F> emplace_back_in(lListElem *ep, const detail::RawKey<F> key) {
      return detail::ElementOf<F>::from_typed(detail::free_emplace_back_in<LF, F, false>(ep, key));
   }

   /**
    * @brief Remove and free the first element of a list with a key; replaces lDelElemStr(), lDelElemHost(), lDelElemUlong() and lDelElemUlong64()
    *
    * Returns as the old function: 1 if an element was removed or the list is
    * nullptr already, 0 if none holds the key or an argument is nullptr. A
    * list that becomes empty is freed and set to nullptr; the typed erase()
    * of a list handle leaves it in place and returns the number removed.
    *
    * @tparam F the key field, a string, host, 32 bit or 64 bit field
    * @param lpp the list reference
    * @param key the key
    * @return 1 or 0, as described
    * @throw FieldException with reason DuplicateKey, where the old function aborts
    * @see Typed form: ocs::tom::List<T>::erase<F>()
    */
   template <auto F>
      requires detail::FreeKeyField<F>
   int erase(lList **lpp, const detail::RawKey<F> key) {
      return detail::free_erase<F>(lpp, key, "ocs::tom::erase");
   }

   namespace detail {
      /**
       * @brief The core of erase_in(): remove and free the first element with a key from the list of a list field
       * @tparam LF the list field
       * @tparam F the key field, of the list field's sub-type
       * @tparam Typed true if the element comes from a handle of the list field's type: only an absent field is
       *         checked
       * @param ep the element, or nullptr
       * @param key the key
       * @return 1 or 0, as #erase; 0 for a null element
       * @throw FieldException with reason ForeignField or AbsentField, or DuplicateKey as #erase
       */
      template <auto LF, auto F, bool Typed>
      int free_erase_in(lListElem *ep, const RawKey<F> key) {
         lList **lpp = sub_list_ref<LF, Typed>(ep, "ocs::tom::erase_in");
         return lpp != nullptr ? free_erase<F>(lpp, key, "ocs::tom::erase_in") : 0;
      }
   } // namespace detail

   /**
    * @brief Remove and free the first element with a key from the list of a list field; replaces lDelSubStr(), lDelSubUlong() and lDelSubUlong64()
    *
    * A null element does nothing and returns 0; an element without the list
    * field throws, where the old function aborts; otherwise as #erase.
    *
    * @tparam LF the list field
    * @tparam F the key field, of the list field's sub-type
    * @param ep the element, or nullptr
    * @param key the key
    * @return 1 or 0, as #erase
    * @throw FieldException with reason ForeignField or AbsentField, or DuplicateKey as #erase
    * @see Typed form: ocs::tom::Element<T>::erase_in<LF, F>()
    */
   template <auto LF, auto F>
      requires detail::SubKeyField<LF, F>
   int erase_in(lListElem *ep, const detail::RawKey<F> key) {
      return detail::free_erase_in<LF, F, false>(ep, key);
   }

   // ---------------------------------------------------------------------------------------------------------------
   // presence, exchanging and swapping

   /**
    * @brief Whether an element holds a field; replaces lGetPosViaElem(ep, F, SGE_NO_ABORT) compared with -1 or 0
    *
    * False for a null element, an element of another type and a reduced
    * element without the field, as the old function gave -1 for each.
    *
    * There is no second template parameter: a `has<F, true>(handle)` would bind to this form through the
    * implicit conversion of the handle and pass by the deleted overloads below (CS-3103). The free forms of
    * this header that know the element's type use detail::has_unchecked().
    *
    * @tparam F the field
    * @param ep the element, or nullptr
    * @return true if the element holds the field
    * @see Typed form: ocs::tom::ConstElement<T>::has<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)>
   [[nodiscard]] bool has(const lListElem *ep) noexcept {
      return ep != nullptr && detail::typed_pos<F>(ep->descr) >= 0;
   }

   namespace detail {
      /**
       * @brief Whether an element known to be of the field's object type holds a field; the type is not checked
       *
       * For the free forms of this header that take a typed handle and may meet an empty one; outside code uses
       * the member has<F>() of the handle.
       *
       * @tparam F the field
       * @param ep the element of the field's object type, or nullptr
       * @return true if @p ep is not nullptr and holds the field (a reduced element may leave it out)
       */
      template <auto F>
         requires TypedField<decltype(F)>
      [[nodiscard]] bool has_unchecked(const lListElem *ep) noexcept {
         return ep != nullptr && ep->descr[FieldTraits<F>::pos].off != OCS_CULL_ABSENT;
      }
   } // namespace detail

   /**
    * @brief Deleted: on a typed handle use the member (h.has<F>()), there is one way per operation (CS-3103)
    * @tparam F the field number
    * @tparam T the object type of the handle
    * @param ep the handle
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to lListElem *
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)>
   bool has(const ConstElement<T> &ep) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h->has<F>()), there is one way per operation (CS-3103)
    * @tparam F the field number
    * @tparam T the object type of the handle
    * @param ep the handle
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lListElem *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)>
   bool has(const ElementPtr<T> &ep) = delete;

   /**
    * @brief Put a list into a list field and take out the one held; replaces lXchgList()
    *
    * With the setters' contract (N-L-4): a null element or a null list
    * reference records the soft path and returns -1; an
    * element without the field throws, where the old function aborts.
    *
    * @tparam F the list field
    * @param ep the element, or nullptr
    * @param lpp the list to put in; the list held before afterwards
    * @return 0, or -1 as described
    * @throw FieldException with reason ForeignField or AbsentField
    * @see Typed form: ocs::tom::Element<T>::exchange<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::List)
   int exchange(lListElem *ep, lList **lpp) {
      if (lpp == nullptr) {
         report_soft_path("ocs::tom::exchange", F, SoftPathReason::NullArgument);
         return -1;
      }
      const int pos = detail::setter_pos<F>(ep, "ocs::tom::exchange");
      if (pos < 0) {
         return -1;
      }
      exchange_list(ep, pos, *lpp);
      return 0;
   }

   /**
    * @brief Put a string into a string field and take out the one held; replaces lXchgString()
    *
    * Neither string is copied. Error behaviour as the list form.
    *
    * @tparam F the string field
    * @param ep the element, or nullptr
    * @param str the string to put in; the string held before afterwards
    * @return 0, or -1 for a null element or a null @p str
    * @throw FieldException with reason ForeignField or AbsentField
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   int exchange(lListElem *ep, char **str) {
      if (str == nullptr) {
         report_soft_path("ocs::tom::exchange", F, SoftPathReason::NullArgument);
         return -1;
      }
      const int pos = detail::setter_pos<F>(ep, "ocs::tom::exchange");
      if (pos < 0) {
         return -1;
      }
      char *&held = ocs::tom::detail::value(ep, pos).str;
      if (*str != held) {
         std::swap(*str, held);
      }
      return 0;
   }

   /**
    * @brief Swap the lists of two list fields of two elements; replaces lSwapList()
    *
    * Both fields hold lists of the same sub-type. A null element records the
    * soft path and returns -1, without changing either
    * element; the old function had taken the list out of @p from already
    * when it found @p to to be nullptr, and lost it. An element without its
    * field throws, where the old function aborts.
    *
    * @tparam F the list field of @p to
    * @tparam G the list field of @p from
    * @param to the first element
    * @param from the second element
    * @return 0, or -1 as described
    * @throw FieldException with reason ForeignField or AbsentField
    * @see Typed form: ocs::tom::Element<T>::swap<F, G>()
    */
   template <auto F, auto G>
      requires detail::TypedListField<F> && detail::TypedListField<G> &&
               std::is_same_v<typename FieldTraits<F>::Sub, typename FieldTraits<G>::Sub>
   int swap(lListElem *to, lListElem *from) {
      const int from_pos = detail::setter_pos<G>(from, "ocs::tom::swap");
      if (from_pos < 0) {
         return -1;
      }
      const int to_pos = detail::setter_pos<F>(to, "ocs::tom::swap");
      if (to_pos < 0) {
         return -1;
      }
      lList *held = nullptr;
      exchange_list(from, from_pos, held);
      exchange_list(to, to_pos, held);
      exchange_list(from, from_pos, held);
      return 0;
   }

   // ---------------------------------------------------------------------------------------------------------------
   // small ones

   /**
    * @brief The list of a list field, created if the field holds none; replaces lGetOrCreateList()
    *
    * The descriptor of a list created here is the field's sub-type; the old
    * function took it as a parameter. A null element records the soft path
    * and gives nullptr, lack of memory gives nullptr, an element without the field throws, where the old function
    * aborts.
    *
    * @tparam F the list field
    * @param ep the element, or nullptr
    * @param list_name the name of a list created here, nullptr for the old default
    * @return the list held in the field as a handle of the field's sub-type, empty on an error; it converts to the
    *         old pointer
    * @throw FieldException with reason ForeignField or AbsentField
    * @see Typed form: ocs::tom::Element<T>::get_or_create<F>()
    */
   template <auto F>
      requires detail::TypedListField<F>
   List<typename FieldTraits<F>::Sub> get_or_create(lListElem *ep, const char *list_name) {
      using Result = List<typename FieldTraits<F>::Sub>;
      if (ep == nullptr) {
         report_soft_path("ocs::tom::get_or_create", F, SoftPathReason::NullElement);
         return Result();
      }
      const int pos = detail::getter_pos<F>(ep);
      try {
         return Result::from_typed(get_or_create_list(ep, pos, list_name != nullptr ? list_name : "No list name specified",
                                                      FieldTraits<F>::Sub::descriptor.e));
      } catch (const ListCoreError &) {
         // no memory: an empty handle
      }
      return Result();
   }

   namespace detail {
      /**
       * @brief The core of list_ref(): the reference to the list held in a list field
       * @tparam F the list field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element
       * @return the reference to the field's list
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      [[nodiscard]] lList **free_list_ref(const lListElem *ep) {
         const int pos = getter_pos<F, Typed>(ep);
         return &ocs::tom::detail::value(ep, pos).glp;
      }

      /**
       * @brief The core of get_or_empty(): the string of a string field, or "" for an unset one
       * @tparam F the string field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element
       * @return the string, never nullptr
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      [[nodiscard]] const char *free_get_or_empty(const lListElem *ep) {
         const int pos = getter_pos<F, Typed>(ep);
         const char *str = ocs::tom::detail::value(ep, pos).str;
         return str != nullptr ? str : "";
      }

      /**
       * @brief The core of has_any_bits(): whether a 32 bit field has any bit of a mask set
       * @tparam F the 32 bit field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element
       * @param mask the bits
       * @return true if the field and the mask share a bit
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      [[nodiscard]] bool free_has_any_bits(const lListElem *ep, const std::uint32_t mask) {
         const int pos = getter_pos<F, Typed>(ep);
         return (ocs::tom::detail::value(ep, pos).ul & mask) > 0;
      }

      /**
       * @brief The core of or_bits(), and_bits() and clear_bits(): change the bits of a 32 bit field
       * @tparam F the 32 bit field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @tparam Op the change, called with a reference to the field's value
       * @param ep the element, or nullptr
       * @param function the calling form, for the record of a soft path
       * @param op the change
       * @return 0, or -1 for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto F, bool Typed, typename Op>
      int free_change_bits(lListElem *ep, const char *function, Op op) {
         const int pos = setter_pos<F, Typed>(ep, function);
         if (pos < 0) {
            return -1;
         }
         op(ocs::tom::detail::value(ep, pos).ul);
         return 0;
      }

      /**
       * @brief The core of or_bits(): set the bits of a mask in a 32 bit field
       * @tparam F the 32 bit field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element, or nullptr
       * @param mask the bits
       * @return 0, or -1 for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      int free_or_bits(lListElem *ep, const std::uint32_t mask) {
         return free_change_bits<F, Typed>(ep, "ocs::tom::or_bits", [mask](std::uint32_t &ul) { ul |= mask; });
      }

      /**
       * @brief The core of and_bits(): keep only the bits of a mask in a 32 bit field
       * @tparam F the 32 bit field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element, or nullptr
       * @param mask the bits to keep
       * @return 0, or -1 for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      int free_and_bits(lListElem *ep, const std::uint32_t mask) {
         return free_change_bits<F, Typed>(ep, "ocs::tom::and_bits", [mask](std::uint32_t &ul) { ul &= mask; });
      }

      /**
       * @brief The core of clear_bits(): clear the bits of a mask in a 32 bit field
       * @tparam F the 32 bit field
       * @tparam Typed true if the element comes from a handle of the field's type: only an absent field is checked
       * @param ep the element, or nullptr
       * @param mask the bits to clear
       * @return 0, or -1 for a null element
       * @throw FieldException with reason ForeignField or AbsentField
       */
      template <auto F, bool Typed>
      int free_clear_bits(lListElem *ep, const std::uint32_t mask) {
         return free_change_bits<F, Typed>(ep, "ocs::tom::clear_bits", [mask](std::uint32_t &ul) { ul &= ~mask; });
      }
   } // namespace detail

   /**
    * @brief The reference to the list held in a list field; replaces lGetListRef()
    * @tparam F the list field
    * @param ep the element
    * @return the reference to the field's list
    * @throw FieldException with reason NullElement, ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::ConstElement<T>::list_ref<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::List)
   [[nodiscard]] lList **list_ref(const lListElem *ep) {
      return detail::free_list_ref<F, false>(ep);
   }

   /**
    * @brief The string of a string field, or "" for an unset one; replaces lGetStringNotNull()
    * @tparam F the string field
    * @param ep the element
    * @return the string, never nullptr
    * @throw FieldException with reason NullElement, ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::ConstElement<T>::get_or_empty<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   [[nodiscard]] const char *get_or_empty(const lListElem *ep) {
      return detail::free_get_or_empty<F, false>(ep);
   }

   /**
    * @brief Whether a 32 bit field has any bit of a mask set; replaces lMatchUlongBitMask()
    *
    * "Any bit", where the condition field<F>().has_all_bits(mask) asks for every bit; the two coincide for a
    * single bit.
    *
    * @tparam F the 32 bit field
    * @param ep the element
    * @param mask the bits
    * @return true if the field and the mask share a bit
    * @throw FieldException with reason NullElement, ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::ConstElement<T>::has_any_bits<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   [[nodiscard]] bool has_any_bits(const lListElem *ep, const std::uint32_t mask) {
      return detail::free_has_any_bits<F, false>(ep, mask);
   }

   /**
    * @brief Set the bits of a mask in a 32 bit field; replaces lOrUlongBitMask()
    * @tparam F the 32 bit field
    * @param ep the element, or nullptr
    * @param mask the bits
    * @return 0, or -1 for a null element
    * @throw FieldException with reason ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::Element<T>::or_bits<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   int or_bits(lListElem *ep, const std::uint32_t mask) {
      return detail::free_or_bits<F, false>(ep, mask);
   }

   /**
    * @brief Keep only the bits of a mask in a 32 bit field; replaces lAndUlongBitMask()
    * @tparam F the 32 bit field
    * @param ep the element, or nullptr
    * @param mask the bits to keep
    * @return 0, or -1 for a null element
    * @throw FieldException with reason ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::Element<T>::and_bits<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   int and_bits(lListElem *ep, const std::uint32_t mask) {
      return detail::free_and_bits<F, false>(ep, mask);
   }

   /**
    * @brief Clear the bits of a mask in a 32 bit field; replaces lClearUlongBitMask()
    * @tparam F the 32 bit field
    * @param ep the element, or nullptr
    * @param mask the bits to clear
    * @return 0, or -1 for a null element
    * @throw FieldException with reason ForeignField or AbsentField, where the old function aborts
    * @see Typed form: ocs::tom::Element<T>::clear_bits<F>()
    */
   template <auto F>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   int clear_bits(lListElem *ep, const std::uint32_t mask) {
      return detail::free_clear_bits<F, false>(ep, mask);
   }


   // ---------------------------------------------------------------------------------------------------------------
   // no free forms on a typed handle (CS-3103)
   //
   // On a typed handle the member is the one way per operation: h.find_first_in<LF, F>(key), h.list_ref<F>(),
   // h.or_bits<F>(mask), ep->erase_in<LF, F>(key) and so on. A handle converts implicitly to the element pointer,
   // so without the deleted overloads below a call with a handle would bind silently to the form on the raw
   // pointer, which checks the element's type at run time instead of at compile time. A deleted overload taking
   // ConstElement<T> also catches an Element<T>, which derives from it; an owner does not convert to the
   // element pointer, its deleted overload gives the reason instead of "no matching function".

   /**
    * @brief Deleted: on a typed handle use the member (h.exchange<F>(std::move(list)) for a list field,
    *        h.exchange<F>(str) for a string field), there is one way per operation (CS-3103)
    * @tparam F the field number
    * @tparam T the object type of the handle
    * @tparam A the type of the value to put in
    * @param ep the handle
    * @param value the value to put in
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to lListElem *;
   // it also catches an Element<T>, which derives from ConstElement<T>
   template <auto F, ObjectType T, typename A>
      requires TypedField<decltype(F)>
   int exchange(const ConstElement<T> &ep, A value) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.get_or_create<F>(name)), there is one way per
    *        operation (CS-3103)
    * @tparam F the list field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @param list_name the name of a list created here
    * @return nothing, the call does not compile
    */
   // without it the call would bind silently to the raw form through the implicit conversion to lListElem *;
   // it also catches an Element<T>, which derives from ConstElement<T>
   template <auto F, ObjectType T>
      requires detail::TypedListField<F>
   auto get_or_create(const ConstElement<T> &ep, const char *list_name) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->exchange<F>(...)), there is one way per operation
    *        (CS-3103)
    * @tparam F the field number
    * @tparam T the object type of the handle
    * @tparam A the type of the value to put in
    * @param ep the owning handle
    * @param value the value to put in
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lListElem *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T, typename A>
      requires TypedField<decltype(F)>
   int exchange(const ElementPtr<T> &ep, A value) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->get_or_create<F>(name)), there is one way per
    *        operation (CS-3103)
    * @tparam F the list field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @param list_name the name of a list created here
    * @return nothing, the call does not compile
    */
   // an owner does not convert to lListElem *; the deleted overload gives the reason instead of "no matching function"
   template <auto F, ObjectType T>
      requires detail::TypedListField<F>
   auto get_or_create(const ElementPtr<T> &ep, const char *list_name) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.find_first_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto find_first_in(const ConstElement<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.find_first_in_rw<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto find_first_in_rw(const ConstElement<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.emplace_back_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto emplace_back_in(const ConstElement<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.erase_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto erase_in(const ConstElement<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.list_ref<F>()), there is one way per operation (CS-3103)
    * @tparam F the list field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::List)
   auto list_ref(const ConstElement<T> &ep) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.get_or_empty<F>()), there is one way per operation (CS-3103)
    * @tparam F the string field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   auto get_or_empty(const ConstElement<T> &ep) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.has_any_bits<F>(mask)),
    *        there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto has_any_bits(const ConstElement<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.or_bits<F>(mask)), there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto or_bits(const ConstElement<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.and_bits<F>(mask)), there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto and_bits(const ConstElement<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on a typed handle use the member (h.clear_bits<F>(mask)), there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto clear_bits(const ConstElement<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->find_first_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the owning handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto find_first_in(const ElementPtr<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->find_first_in_rw<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the owning handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto find_first_in_rw(const ElementPtr<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->emplace_back_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the owning handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto emplace_back_in(const ElementPtr<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->erase_in<LF, F>(key)),
    *        there is one way per operation (CS-3103)
    * @tparam LF the list field
    * @tparam F the key field
    * @tparam T the object type of the handle
    * @tparam K the type of the key
    * @param ep the owning handle
    * @param key the key
    * @return nothing, the call does not compile
    */
   template <auto LF, auto F, ObjectType T, typename K>
      requires detail::SubKeyField<LF, F>
   auto erase_in(const ElementPtr<T> &ep, K key) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->list_ref<F>()), there is one way per operation (CS-3103)
    * @tparam F the list field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::List)
   auto list_ref(const ElementPtr<T> &ep) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->get_or_empty<F>()),
    *        there is one way per operation (CS-3103)
    * @tparam F the string field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::String)
   auto get_or_empty(const ElementPtr<T> &ep) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->has_any_bits<F>(mask)),
    *        there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto has_any_bits(const ElementPtr<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->or_bits<F>(mask)),
    *        there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto or_bits(const ElementPtr<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->and_bits<F>(mask)),
    *        there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto and_bits(const ElementPtr<T> &ep, std::uint32_t mask) = delete;

   /**
    * @brief Deleted: on an owning handle use the member (ep->clear_bits<F>(mask)),
    *        there is one way per operation (CS-3103)
    * @tparam F the 32 bit field
    * @tparam T the object type of the handle
    * @param ep the owning handle
    * @param mask the bits
    * @return nothing, the call does not compile
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type == FieldType::Uint32)
   auto clear_bits(const ElementPtr<T> &ep, std::uint32_t mask) = delete;

   // swap() takes two elements: a typed handle on either side is refused, the other may be a raw pointer

   /**
    * @brief Deleted: swap() on typed handles; use the member (to.swap<F, G>(from)), there is one way per
    *        operation (CS-3103)
    * @tparam F the list field of @p to
    * @tparam G the list field of @p from
    * @tparam T the object type of @p to
    * @tparam U the object type of @p from
    * @param to the first element
    * @param from the second element
    * @return nothing, the call does not compile
    */
   template <auto F, auto G, ObjectType T, ObjectType U>
      requires detail::TypedListField<F> && detail::TypedListField<G>
   int swap(const ConstElement<T> &to, const ConstElement<U> &from) = delete;

   /**
    * @brief Deleted: swap() on a typed handle and a raw pointer; use the member (to.swap<F, G>(from)),
    *        there is one way per operation (CS-3103)
    * @tparam F the list field of @p to
    * @tparam G the list field of @p from
    * @tparam T the object type of @p to
    * @param to the first element
    * @param from the second element
    * @return nothing, the call does not compile
    */
   template <auto F, auto G, ObjectType T>
      requires detail::TypedListField<F> && detail::TypedListField<G>
   int swap(const ConstElement<T> &to, lListElem *from) = delete;

   /**
    * @brief Deleted: swap() on a raw pointer and a typed handle; use the member (to.swap<F, G>(from)),
    *        there is one way per operation (CS-3103)
    * @tparam F the list field of @p to
    * @tparam G the list field of @p from
    * @tparam U the object type of @p from
    * @param to the first element
    * @param from the second element
    * @return nothing, the call does not compile
    */
   template <auto F, auto G, ObjectType U>
      requires detail::TypedListField<F> && detail::TypedListField<G>
   int swap(lListElem *to, const ConstElement<U> &from) = delete;

   /**
    * @brief Deleted: swap() on owning handles; use the member (to.swap<F, G>(from)), there is one way per
    *        operation (CS-3103)
    * @tparam F the list field of @p to
    * @tparam G the list field of @p from
    * @tparam T the object type of @p to
    * @tparam U the object type of @p from
    * @param to the first element
    * @param from the second element
    * @return nothing, the call does not compile
    */
   template <auto F, auto G, ObjectType T, ObjectType U>
      requires detail::TypedListField<F> && detail::TypedListField<G>
   int swap(const ElementPtr<T> &to, const ElementPtr<U> &from) = delete;

   /**
    * @brief Copy a field from one element to another when the source holds it
    *
    * The source may be reduced (e.g. the element of a modify request, which holds only
    * the fields the client sent); a field it does not hold leaves the destination as it
    * is. The value is copied as it is, a string or host field copies nullptr as well; a
    * list or object field is copied deeply, as `lSetList(dst, nm, lCopyList(...))` does.
    *
    * A reference field does not compile: the element does not own what a reference points
    * to, so a copy would share it with nothing to say who frees it (clone() copies the
    * pointer, as lCopyElem() did).
    *
    * @tparam F the field, of the object type of both elements; not a reference field
    * @tparam T the object type, deduced from the destination
    * @param dst the element to copy into, or an empty handle (nothing is copied then)
    * @param src the element to copy from, may be reduced or an empty handle
    * @return true if the source held the field and the destination took the value; false for an empty
    *         destination or source, a source without the field, or a value the setter refused (a string that
    *         cannot be copied)
    * @throw FieldException with reason AbsentField if the destination is reduced and lacks the field, in a debug
    *        build DuplicateKey if another element of the destination's list holds the value of a unique field
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && (FieldTraits<F>::type != FieldType::Ref)
   bool copy_if_present(const Element<T> &dst, const ConstElement<T> &src) {
      detail::assert_field_of<F, T>();
      if (!dst || !detail::has_unchecked<F>(src.raw())) {
         return false;
      }
      // the core of the free setter: a value the setter refuses gives false, not an exception
      if constexpr (FieldTraits<F>::type == FieldType::List || FieldTraits<F>::type == FieldType::Object) {
         return detail::free_set<F, true>(dst.raw(), src.template get<F>().clone()) == 0;
      } else {
         return detail::free_set<F, true>(dst.raw(), src.template get<F>()) == 0;
      }
   }

   namespace detail {
      /**
       * @brief Compare a key field of two elements of one type
       *
       * Host names compare as host names (Host::operator==, as sge_hostcmp() does), as find_first() and
       * modify_sublist() compare them; strings compare with strcmp().
       *
       * @tparam Key the key field, a number, string or host field
       * @tparam T the object type, deduced from the elements
       * @param a the one element
       * @param b the other element
       * @param null_matches whether two string or host keys that are both nullptr are equal
       * @return true if the keys are equal
       * @throw FieldException with reason AbsentField if an element is reduced and lacks the key field
       */
      template <auto Key, ObjectType T>
      bool same_key(const ConstElement<T> &a, const ConstElement<T> &b, const bool null_matches) {
         if constexpr (FieldTraits<Key>::type == FieldType::Host) {
            const Host x = a.template get<Key>();
            const Host y = b.template get<Key>();
            if (x == nullptr || y == nullptr) {
               return null_matches && x == nullptr && y == nullptr;
            }
            return x == y;
         } else if constexpr (FieldTraits<Key>::type == FieldType::String) {
            const char *const x = a.template get<Key>();
            const char *const y = b.template get<Key>();
            if (x == nullptr || y == nullptr) {
               return null_matches && x == y;
            }
            return std::strcmp(x, y) == 0;
         } else {
            return a.template get<Key>() == b.template get<Key>();
         }
      }
   } // namespace detail

   /**
    * @brief Keep only the last element of each key, as a user writing an option twice expects
    *
    * Walks the list front to back and removes every earlier element with the key of the
    * current one, so the order of the elements that stay is kept. A string or host key that
    * is nullptr is never a repetition. Host names compare as host names (as find_first()
    * does), strings with strcmp(). This is what `cull_compress_definition_list()` did, which
    * compared host names with strcmp().
    *
    * @tparam Key the key field of the list's type: a number (also Int32, Int64, Double, Bool), string or host
    *         field; not a list, object or reference field
    * @tparam T the object type, deduced from the list
    * @param lp the list, or an empty handle
    * @throw FieldException with reason AbsentField if the list is reduced and lacks the key field
    */
   template <auto Key, ObjectType T>
      requires TypedField<decltype(Key)> && (FieldTraits<Key>::type != FieldType::List &&
                                             FieldTraits<Key>::type != FieldType::Object &&
                                             FieldTraits<Key>::type != FieldType::Ref)
   void keep_last(const List<T> &lp) {
      for (auto one = lp.front(); one; one = one.next()) {
         for (auto other = lp.front(); other && other.raw() != one.raw();) {
            const auto next = other.next();
            if (detail::same_key<Key>(ConstElement<T>(one), ConstElement<T>(other), false)) {
               lp.erase(other);
            }
            other = next;
         }
      }
   }

   /**
    * @brief Merge a list into another by key: a known key takes the new value, a new key is appended
    *
    * For each element of @p src the first element of @p dst with the same key gets the
    * value of @p Value; an element whose key @p dst does not hold is appended as a copy.
    * Two string or host keys that are both nullptr are equal. Host names compare as host
    * names (as find_first() does), strings with strcmp(). This is what
    * `cull_merge_definition_list()` did, which compared host names with strcmp().
    *
    * @tparam Key the key field of the lists' type: a key-capable field (Uint32, Uint64, String or Host), as
    *         find_first() takes
    * @tparam Value the value field, a number, string or host field of the lists' type
    * @tparam T the object type, deduced from the destination
    * @param dst the list merged into; not an empty handle
    * @param src the list merged from, may be an empty handle; an owning handle converts to it
    * @throw FieldException with reason EmptyHandle if @p dst is an empty handle and @p src holds an element,
    *        AbsentField if a list is reduced and lacks the key or the value field, WrongLayout if an element
    *        of @p src to append has another layout than @p dst; in a debug build DuplicateKey if a unique field
    *        would repeat in @p dst
    */
   template <auto Key, auto Value, ObjectType T>
      requires detail::FreeKeyField<Key> && FieldOf<Key, T> && TypedField<decltype(Value)> && FieldOf<Value, T> &&
               (FieldTraits<Value>::type != FieldType::List && FieldTraits<Value>::type != FieldType::Object &&
                FieldTraits<Value>::type != FieldType::Ref)
   void merge_by_key(const List<T> &dst, const std::type_identity_t<ConstList<T>> &src) {
      for (const auto src_ep : src) {
         Element<T> found;
         for (const auto dst_ep : dst) {
            if (detail::same_key<Key>(src_ep, ConstElement<T>(dst_ep), true)) {
               found = dst_ep;
               break;
            }
         }
         if (found) {
            // the core of the free setter: a string that cannot be copied is ignored, as before
            detail::free_set<Value, true>(found.raw(), src_ep.template get<Value>());
         } else {
            dst.push_back(src_ep.clone());
         }
      }
   }

   /**
    * @brief How modify_sublist() applies the sub-list of a request; no flag set replaces the whole sub-list
    *
    * The flags combine as the GDI sub-commands they come from. Where more than one is set, a held key is
    * handled by the first of change, append, remove that is set, and a key that is not held is reported
    * as not held when remove is set and added otherwise.
    */
   struct SublistMode {
      bool change = false;   ///< a held key takes the request's element if it differs, a new key is added
      bool append = false;   ///< a new key is added, a held key ends the operation as a failure
      bool remove = false;   ///< every element holding a requested key is removed
   };

   /** @brief What modify_sublist() reports to its caller, at the moment it happens */
   enum class SublistEvent {
      MissingKey,    ///< an element of either sub-list holds no key; the operation fails
      AlreadyHeld,   ///< append: the key is held already; the request's element is dropped, the operation fails
      NotHeld,       ///< remove: the key is not held; nothing is removed for it
      CreatedList,   ///< the object had no sub-list; it gets a copy of what is left of the request's sub-list
      Appended       ///< a key the object does not hold: the request's element is moved to the object's sub-list
   };

   /** @brief The result of modify_sublist() */
   struct SublistOutcome {
      bool ok;        ///< false if the request lacks the sub-list, a key is missing or an append met a held key
      bool changed;   ///< whether the object's sub-list was changed
   };

   namespace detail {
      /**
       * @brief The key of an element of a sub-list as a C string, nullptr if it holds none
       * @tparam Key the key field, a string or host field
       * @tparam S the sub-list's object type, deduced from the element
       * @param ep the element
       * @return the key, or nullptr if the element lacks the field or the key is not set
       */
      template <auto Key, ObjectType S>
      const char *sublist_key(const ConstElement<S> &ep) {
         if (!has_unchecked<Key>(ep.raw())) {
            return nullptr;
         }
         if constexpr (FieldTraits<Key>::type == FieldType::Host) {
            return ep.template get<Key>().c_str();
         } else {
            return ep.template get<Key>();
         }
      }

      /**
       * @brief Whether two keys of a sub-list are equal, host names as sge_hostcmp() compares them
       * @tparam Key the key field, a string or host field
       * @param a the one key, not nullptr
       * @param b the other key, not nullptr
       * @return true if the keys are equal
       */
      template <auto Key>
      bool same_sublist_key(const char *const a, const char *const b) {
         if constexpr (FieldTraits<Key>::type == FieldType::Host) {
            return Host(a) == Host(b);
         } else {
            return std::strcmp(a, b) == 0;
         }
      }
   } // namespace detail

   /**
    * @brief Apply the sub-list of a modify request to the sub-list of an object, key by key
    *
    * Without a flag in @p mode the object's sub-list is replaced by a copy of the request's. Otherwise each
    * element of the request's sub-list is looked up by @p SubKeyF in the object's:
    * - a held key: change replaces the held element by the request's one if @p differs says they differ
    *   (the new one goes to the end) and drops the request's one otherwise; append reports
    *   SublistEvent::AlreadyHeld and fails; remove removes every element with the key;
    * - the keys that are not held, once the held ones are done and nothing failed: remove reports
    *   SublistEvent::NotHeld; otherwise, if the object has no sub-list, it gets a copy of what is left of the
    *   request's (SublistEvent::CreatedList), else the element is moved over (SublistEvent::Appended).
    *
    * An element of either sub-list without a key is reported as SublistEvent::MissingKey for each pair it is
    * compared in, and the operation fails. After a request element is applied, the walk starts again at the
    * front of what is left of the request's sub-list, so a key the request holds twice is applied twice.
    * Unless the operation failed, an object's sub-list left without elements is removed (set to nullptr).
    *
    * The request is consumed: its elements are moved to the object or freed as they are applied, as
    * attr_mod_sub_list() always did. What the operation reports is the caller's to put into words.
    *
    * @tparam ListF the list field, of both elements' object type
    * @tparam SubKeyF the key field, a string or host field of the list field's sub-type
    * @tparam T the object type, deduced from the destination
    * @tparam Differs a predicate on (request element, held element) of the sub-type
    * @tparam Report a callable taking (SublistEvent, const char *key); the key is nullptr for MissingKey
    * @param dst the object to modify
    * @param request the modify request, may be reduced; its sub-list is consumed
    * @param mode how to apply the request's sub-list
    * @param differs whether a request element changes the held element with the same key
    * @param report called for every event, in the order they happen
    * @return whether the operation succeeded and whether the object's sub-list was changed
    * @throw FieldException with reason NullElement if @p dst is an empty handle, AbsentField if @p dst is reduced
    *        and lacks @p ListF, WrongLayout if a request element moved into the object's sub-list has another
    *        layout than that list; in a debug build DuplicateKey if a unique field would repeat there. A request
    *        without the list field, and an element of either sub-list without a key, are not exceptions: they
    *        are reported (return value, SublistEvent::MissingKey)
    */
   template <auto ListF, auto SubKeyF, ObjectType T, typename Differs, typename Report>
      requires detail::SubKeyField<ListF, SubKeyF> &&
               (FieldTraits<SubKeyF>::type == FieldType::String || FieldTraits<SubKeyF>::type == FieldType::Host) &&
               std::predicate<Differs &, ConstElement<typename FieldTraits<ListF>::Sub>,
                              ConstElement<typename FieldTraits<ListF>::Sub>> &&
               std::invocable<Report &, SublistEvent, const char *>
   SublistOutcome modify_sublist(const Element<T> &dst, const std::type_identity_t<Element<T>> &request,
                                 const SublistMode mode, Differs &&differs, Report &&report) {
      detail::assert_field_of<ListF, T>();
      using S = typename FieldTraits<ListF>::Sub;
      bool ok = true;
      bool changed = false;

      if (!detail::has_unchecked<ListF>(request.raw())) {
         return {false, false};
      }

      if (mode.change || mode.append || mode.remove) {
         const List<S> reduced = request.template get<ListF>();
         // the core of the free getter: an empty destination throws NullElement, as documented
         const List<S> full = detail::read<ListF, true>(dst.raw(), detail::checked_slot<ListF, true>(dst.raw()));

         // first the keys the object holds; whatever is applied leaves the request's sub-list
         Element<S> next_reduced = reduced.front();
         while (const Element<S> reduced_ep = next_reduced) {
            bool restart = false;
            bool remove_reduced = false;

            next_reduced = reduced_ep.next();
            Element<S> next_full = full.front();
            while (const Element<S> full_ep = next_full) {
               next_full = full_ep.next();

               const char *const rkey = detail::sublist_key<SubKeyF>(ConstElement<S>(reduced_ep));
               const char *const fkey = detail::sublist_key<SubKeyF>(ConstElement<S>(full_ep));
               if (rkey == nullptr || fkey == nullptr) {
                  report(SublistEvent::MissingKey, nullptr);
                  ok = false;
               }

               if (ok && detail::same_sublist_key<SubKeyF>(rkey, fkey)) {
                  if (mode.change) {
                     if (differs(ConstElement<S>(reduced_ep), ConstElement<S>(full_ep))) {
                        changed = true;
                        auto moved = reduced.extract(reduced_ep);
                        full.erase(full_ep);
                        full.push_back(std::move(moved));
                     } else {
                        reduced.erase(reduced_ep);
                     }
                     restart = true;
                     break;
                  } else if (mode.append) {
                     report(SublistEvent::AlreadyHeld, rkey);
                     ok = false;
                     reduced.erase(reduced_ep);
                     restart = true;
                     break;
                  } else if (mode.remove) {
                     // every element with the key goes, so that a key held twice cannot survive its removal
                     changed = true;
                     full.erase(full_ep);
                     remove_reduced = true;
                     continue;
                  }
               }
            }
            if (remove_reduced) {
               reduced.erase(reduced_ep);
               restart = true;
            }
            if (!ok) {
               break;
            }
            if (restart) {
               next_reduced = reduced.front();
            }
         }

         // then the keys the object does not hold
         if (ok) {
            next_reduced = reduced.front();
            while (const Element<S> reduced_ep = next_reduced) {
               next_reduced = reduced_ep.next();

               const char *const rkey = detail::sublist_key<SubKeyF>(ConstElement<S>(reduced_ep));
               if (rkey == nullptr) {
                  report(SublistEvent::MissingKey, nullptr);
                  ok = false;
               }

               if (ok) {
                  if (mode.remove) {
                     report(SublistEvent::NotHeld, rkey);
                  } else if (!full) {
                     report(SublistEvent::CreatedList, rkey);
                     detail::free_set<ListF, true>(dst.raw(), reduced.clone(""));
                     changed = true;
                     break;
                  } else {
                     report(SublistEvent::Appended, rkey);
                     full.push_back(reduced.extract(reduced_ep));
                     changed = true;
                  }
               }
            }
         }
      } else {
         // without a flag the request's sub-list replaces the object's
         detail::free_set<ListF, true>(dst.raw(), request.template get<ListF>().clone(""));
         changed = true;
      }

      // a sub-list left without elements is removed
      if (ok) {
         if (const auto lp = detail::read<ListF, true>(dst.raw(), detail::checked_slot<ListF, true>(dst.raw()));
             lp && lp.empty()) {
            detail::free_set<ListF, true>(dst.raw(), nullptr);
         }
      }

      return {ok, changed};
   }

   // ---------------------------------------------------------------------------------------------------------------
   // the forwarding members of the handles (ocs_Element.h, ocs_List.h)
   //
   // Declared and documented in the handle classes, defined here: a member names its free form, which needs the
   // complete handles and so comes after them. Each only forwards; the free form decides the behaviour. The
   // members on a single element call the core of their free form (detail::free_*) with Typed = true: the
   // handle's type is known at compile time, and an empty handle behaves as the free form's null element (no
   // EmptyHandle). The free forms themselves are deleted for typed handles (CS-3103).

   /**
    * @brief ConstElement::find_first_in(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam LF the list field
    * @tparam F the key field
    * @param key the key
    * @return the element, empty if none
    * @see free form ocs::tom::find_first_in()
    */
   template <ObjectType T>
   template <auto LF, auto F>
      requires detail::SubKeyOf<LF, F, T>
   ConstElement<typename FieldTraits<F>::Type>
   ConstElement<T>::find_first_in(const KeyArgOf<FieldTraits<F>::type> key) const {
      return ConstElement<typename FieldTraits<F>::Type>::from_typed(
         detail::free_find_first_in<LF, F, true>(this->ep_, key, "ocs::tom::find_first_in"));
   }

   /**
    * @brief ConstElement::find_first_in_rw(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam LF the list field
    * @tparam F the key field
    * @param key the key
    * @return the element, empty if none
    * @see free form ocs::tom::find_first_in_rw()
    */
   template <ObjectType T>
   template <auto LF, auto F>
      requires detail::SubKeyOf<LF, F, T>
   Element<typename FieldTraits<F>::Type>
   ConstElement<T>::find_first_in_rw(const KeyArgOf<FieldTraits<F>::type> key) const {
      return Element<typename FieldTraits<F>::Type>::from_typed(
         detail::free_find_first_in<LF, F, true>(this->ep_, key, "ocs::tom::find_first_in_rw"));
   }

   /**
    * @brief ConstElement::list_ref(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the list field
    * @return the reference to the field's list
    * @see free form ocs::tom::list_ref()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List)
   lList **ConstElement<T>::list_ref() const {
      return detail::free_list_ref<F, true>(this->ep_);
   }

   /**
    * @brief ConstElement::get_or_empty(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the string field
    * @return the string, never nullptr
    * @see free form ocs::tom::get_or_empty()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
   const char *ConstElement<T>::get_or_empty() const {
      return detail::free_get_or_empty<F, true>(this->ep_);
   }

   /**
    * @brief ConstElement::has_any_bits(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the 32 bit field
    * @param mask the bits
    * @return true if the field and the mask share a bit
    * @see free form ocs::tom::has_any_bits()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Uint32)
   bool ConstElement<T>::has_any_bits(const std::uint32_t mask) const {
      return detail::free_has_any_bits<F, true>(this->ep_, mask);
   }

   /**
    * @brief Element::emplace_back_in(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam LF the list field
    * @tparam F the key field
    * @param key the key
    * @return the new element, empty on an error
    * @see free form ocs::tom::emplace_back_in()
    */
   template <ObjectType T>
   template <auto LF, auto F>
      requires detail::SubKeyOf<LF, F, T>
   Element<typename FieldTraits<F>::Type> Element<T>::emplace_back_in(const KeyArgOf<FieldTraits<F>::type> key) const {
      return Element<typename FieldTraits<F>::Type>::from_typed(
         detail::free_emplace_back_in<LF, F, true>(this->ep_, key));
   }

   /**
    * @brief Element::erase_in(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam LF the list field
    * @tparam F the key field
    * @param key the key
    * @return 1 or 0, as the free form
    * @see free form ocs::tom::erase_in()
    */
   template <ObjectType T>
   template <auto LF, auto F>
      requires detail::SubKeyOf<LF, F, T>
   int Element<T>::erase_in(const KeyArgOf<FieldTraits<F>::type> key) const {
      return detail::free_erase_in<LF, F, true>(this->ep_, key);
   }

   /**
    * @brief Element::or_bits(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the 32 bit field
    * @param mask the bits
    * @return 0, or -1 for an empty handle
    * @see free form ocs::tom::or_bits()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Uint32)
   int Element<T>::or_bits(const std::uint32_t mask) const {
      return detail::free_or_bits<F, true>(this->ep_, mask);
   }

   /**
    * @brief Element::and_bits(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the 32 bit field
    * @param mask the bits to keep
    * @return 0, or -1 for an empty handle
    * @see free form ocs::tom::and_bits()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Uint32)
   int Element<T>::and_bits(const std::uint32_t mask) const {
      return detail::free_and_bits<F, true>(this->ep_, mask);
   }

   /**
    * @brief Element::clear_bits(): the core of the free form, without its run-time type check
    * @tparam T the object type of the handle
    * @tparam F the 32 bit field
    * @param mask the bits to clear
    * @return 0, or -1 for an empty handle
    * @see free form ocs::tom::clear_bits()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Uint32)
   int Element<T>::clear_bits(const std::uint32_t mask) const {
      return detail::free_clear_bits<F, true>(this->ep_, mask);
   }

   /**
    * @brief Element::copy_if_present(): forwards to the free form, this element as the destination
    * @tparam T the object type of the handle
    * @tparam F the field, not a reference field
    * @param src the element to copy from
    * @return true if the value was copied
    * @see free form ocs::tom::copy_if_present()
    */
   template <ObjectType T>
   template <auto F>
      requires FieldOf<F, T> && (FieldTraits<F>::type != FieldType::Ref)
   bool Element<T>::copy_if_present(const ConstElement<T> &src) const {
      return ocs::tom::copy_if_present<F>(*this, src);
   }

   /**
    * @brief Element::modify_sublist(): forwards to the free form, this element as the object modified
    * @tparam T the object type of the handle
    * @tparam ListF the list field
    * @tparam SubKeyF the key field of the list field's sub-type
    * @tparam Differs the predicate type
    * @tparam Report the callable type
    * @param request the modify request; its sub-list is consumed
    * @param mode how to apply the request's sub-list
    * @param differs whether a request element changes the held element with the same key
    * @param report called for every event
    * @return whether the operation succeeded and whether the sub-list was changed
    * @see free form ocs::tom::modify_sublist()
    */
   template <ObjectType T>
   template <auto ListF, auto SubKeyF, typename Differs, typename Report>
      requires detail::SubKeyOf<ListF, SubKeyF, T> &&
               (FieldTraits<SubKeyF>::type == FieldType::String || FieldTraits<SubKeyF>::type == FieldType::Host) &&
               std::predicate<Differs &, ConstElement<typename FieldTraits<ListF>::Sub>,
                              ConstElement<typename FieldTraits<ListF>::Sub>> &&
               std::invocable<Report &, SublistEvent, const char *>
   SublistOutcome Element<T>::modify_sublist(const Element<T> &request, const SublistMode mode, Differs &&differs,
                                             Report &&report) const {
      return ocs::tom::modify_sublist<ListF, SubKeyF>(*this, request, mode, std::forward<Differs>(differs),
                                                      std::forward<Report>(report));
   }

   /**
    * @brief List::keep_last(): forwards to the free form
    * @tparam T the object type of the list
    * @tparam Key the key field
    * @see free form ocs::tom::keep_last()
    */
   template <ObjectType T>
   template <auto Key>
      requires FieldOf<Key, T> && (FieldTraits<Key>::type != FieldType::List &&
                                   FieldTraits<Key>::type != FieldType::Object &&
                                   FieldTraits<Key>::type != FieldType::Ref)
   void List<T>::keep_last() const {
      ocs::tom::keep_last<Key>(*this);
   }

   /**
    * @brief List::merge_by_key(): forwards to the free form, this list as the destination
    * @tparam T the object type of the lists
    * @tparam Key the key field
    * @tparam Value the value field
    * @param src the list merged from
    * @see free form ocs::tom::merge_by_key()
    */
   template <ObjectType T>
   template <auto Key, auto Value>
      requires FieldOf<Key, T> && detail::LookupField<Key> && FieldOf<Value, T> &&
               (FieldTraits<Value>::type != FieldType::List && FieldTraits<Value>::type != FieldType::Object &&
                FieldTraits<Value>::type != FieldType::Ref)
   void List<T>::merge_by_key(const ConstList<T> &src) const {
      ocs::tom::merge_by_key<Key, Value>(*this, src);
   }

} // namespace ocs::tom

// the raw handles and owners, which the members of the typed element handle give for a list or object field without
// a sub-type (Element<T>::get<F>(), exchange<F>()); included last, so that the typed handles are complete in it
// whichever of the typed headers a source includes first
#include "tom/ocs_Raw.h"
