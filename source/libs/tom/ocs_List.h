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
 * @brief Typed handles on lists of the typed object model: List, ConstList, ListPtr
 *
 * A list handle is a view on an `lList *` whose elements are of one known
 * object type. It is the size of a pointer and converts implicitly back to the
 * pointer. Iterating it yields element handles of the type, and the lookups by
 * key use the lazy hash tables of the list library exactly as the old lookups
 * do; a key that is not found gives an empty handle or an empty range.
 */

#include <cstddef>
#include <iterator>
#include <utility>

#include "tom/cull_listP.h"
#include "tom/ocs_Element.h"

namespace ocs::tom {

   namespace detail {

      /**
       * @brief The key type of a lookup by a field
       *
       * Lookups exist for the field types the list library can search by:
       * unsigned numbers, strings and host names.
       *
       * @tparam F the field number
       */
      template <auto F>
      using KeyOf = typename ValueOf<FieldTraits<F>::type>::type;

      /**
       * @brief Whether a field can be looked up by
       * @tparam F the field number
       */
      template <auto F>
      concept LookupField = FieldTraits<F>::type == FieldType::Uint32 || FieldTraits<F>::type == FieldType::Uint64 ||
                            FieldTraits<F>::type == FieldType::String || FieldTraits<F>::type == FieldType::Host;

      /**
       * @brief The first element of a list with a key in a field, through the lookup of the core
       *
       * A key held twice in a unique field throws, where the old lookup
       * aborts (N-H-18). A list without the field, a reduced one, throws as
       * every typed access to an absent field does.
       *
       * @tparam F the field number
       * @param lp the list, not nullptr
       * @param key the key
       * @param iterator the lookup's state, for lookup_next()
       * @return the element, or nullptr
       * @throw FieldException with reason DuplicateKey, naming type, field, key and list, or AbsentField
       */
      template <auto F>
      inline lListElem *lookup_first(const lList *lp, const KeyOf<F> key, const void **iterator) {
         *iterator = nullptr;
         const int pos = field_pos(lp->descr, F);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, F, lp->descr);
         }
         constexpr FieldType type = FieldTraits<F>::type;
         if constexpr (type == FieldType::Uint32) {
            return lookup_first_ulong(lp, pos, key, iterator);
         } else if constexpr (type == FieldType::Uint64) {
            return lookup_first_ulong64(lp, pos, key, iterator);
         } else if constexpr (type == FieldType::String) {
            return key != nullptr ? lookup_first_str(lp, pos, key, iterator) : nullptr;
         } else {
            return key.c_str() != nullptr ? lookup_first_host(lp, pos, key.c_str(), iterator) : nullptr;
         }
      }

      /**
       * @brief The next element of a list with a key in a field, through the lookup of the core
       *
       * Called only after lookup_first() found an element, which checked the
       * field; a null iterator gives no element.
       *
       * @tparam F the field number
       * @param lp the list
       * @param key the key
       * @param iterator the lookup's state
       * @return the element, or nullptr
       */
      template <auto F>
      inline lListElem *lookup_next(const lList *lp, const KeyOf<F> key, const void **iterator) {
         constexpr FieldType type = FieldTraits<F>::type;
         constexpr int pos = FieldTraits<F>::pos;
         if constexpr (type == FieldType::Uint32) {
            return lookup_next_ulong(lp, pos, key, iterator);
         } else if constexpr (type == FieldType::Uint64) {
            return lookup_next_ulong64(lp, pos, key, iterator);
         } else if constexpr (type == FieldType::String) {
            return lookup_next_str(lp, pos, key, iterator);
         } else {
            return lookup_next_host(lp, pos, key.c_str(), iterator);
         }
      }

      /**
       * @brief Iterator over the elements of a list, yielding element handles
       * @tparam H the element handle, Element or ConstElement of the type
       */
      template <typename H>
      class ElementIterator {
      public:
         using iterator_category = std::forward_iterator_tag;   ///< walks forward only
         using value_type = H;                                   ///< an element handle
         using difference_type = std::ptrdiff_t;                 ///< distance between two positions
         using pointer = void;                                   ///< handles are values, there is no pointer to one
         using reference = H;                                    ///< dereferencing gives a handle by value

         /** @brief The end of every list */
         ElementIterator() noexcept = default;

         /**
          * @brief An iterator at an element
          * @param ep the element, or nullptr for the end
          */
         explicit ElementIterator(lListElem *ep) noexcept : ep_(ep) {}

         /**
          * @brief The handle on the current element
          * @return the handle
          */
         H operator*() const noexcept {
            return H::from_typed(ep_);
         }

         /**
          * @brief Step to the next element
          * @return this iterator
          */
         ElementIterator &operator++() noexcept {
            ep_ = ep_->next;
            return *this;
         }

         /**
          * @brief Step to the next element
          * @return the iterator before the step
          */
         ElementIterator operator++(int) noexcept {
            ElementIterator before = *this;
            ep_ = ep_->next;
            return before;
         }

         /**
          * @brief Whether two iterators are at the same position
          * @param other the other iterator
          * @return true if at the same element or both at the end
          */
         bool operator==(const ElementIterator &other) const noexcept = default;

      private:
         lListElem *ep_ = nullptr;   ///< the current element, or nullptr at the end
      };

      /**
       * @brief The elements of a list with a key in a field, as a range
       *
       * Iterating it runs the old lookup's first and next steps, so it uses the
       * list's hash table where the list has one.
       *
       * @tparam F the field number
       * @tparam H the element handle, Element or ConstElement of the type
       */
      template <auto F, typename H>
      class KeyRange {
      public:
         /** @brief Iterator over the matches */
         class iterator {
         public:
            using iterator_category = std::forward_iterator_tag;   ///< walks forward only
            using value_type = H;                                   ///< an element handle
            using difference_type = std::ptrdiff_t;                 ///< distance between two positions
            using pointer = void;                                   ///< handles are values
            using reference = H;                                    ///< dereferencing gives a handle by value

            /** @brief The end of the matches */
            iterator() noexcept = default;

            /**
             * @brief The first match
             * @param lp the list
             * @param key the key; a string or host name must outlive the iteration
             */
            iterator(const lList *lp, const KeyOf<F> key) : lp_(lp), key_(key) {
               ep_ = lp != nullptr ? lookup_first<F>(lp, key, &state_) : nullptr;
            }

            /**
             * @brief The handle on the current match
             * @return the handle
             */
            H operator*() const noexcept {
               return H::from_typed(ep_);
            }

            /**
             * @brief Step to the next match
             * @return this iterator
             */
            iterator &operator++() {
               ep_ = lookup_next<F>(lp_, key_, &state_);
               return *this;
            }

            /**
             * @brief Whether two iterators are at the same match
             * @param other the other iterator
             * @return true if at the same element or both at the end
             */
            bool operator==(const iterator &other) const noexcept {
               return ep_ == other.ep_;
            }

         private:
            const lList *lp_ = nullptr;      ///< the list
            KeyOf<F> key_{};                 ///< the key
            const void *state_ = nullptr;    ///< the state of the old lookup
            lListElem *ep_ = nullptr;        ///< the current match, or nullptr at the end
         };

         /**
          * @brief The matches of a key in a list
          * @param lp the list, or nullptr for no matches
          * @param key the key; a string or host name must outlive the range
          */
         KeyRange(const lList *lp, const KeyOf<F> key) noexcept : lp_(lp), key_(key) {}

         /**
          * @brief The first match
          * @return the iterator
          * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18)
          */
         [[nodiscard]] iterator begin() const {
            return iterator(lp_, key_);
         }

         /**
          * @brief The end of the matches
          * @return the iterator
          */
         [[nodiscard]] iterator end() const noexcept {
            return {};
         }

         /**
          * @brief Whether there is no match
          * @return true if the key is in no element
          */
         [[nodiscard]] bool empty() const {
            return begin() == end();
         }

      private:
         const lList *lp_;   ///< the list, or nullptr
         KeyOf<F> key_;      ///< the key
      };

   } // namespace detail

   /**
    * @brief A read-only typed handle on a list
    *
    * Converts implicitly to `const lList *`, made from one only explicitly and
    * with a check of the type. Tests as false when empty; an empty handle is
    * an empty list for iteration, size and lookups, as nullptr is for the old
    * interface.
    *
    * @tparam T the struct of the object type of the elements, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class ConstList {
   public:
      using Type = T;                                                          ///< the object type of the elements
      using const_iterator = detail::ElementIterator<ConstElement<T>>;         ///< iterates with read-only handles

      /** @brief An empty handle */
      constexpr ConstList() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr ConstList(std::nullptr_t) noexcept {}

      /**
       * @brief Take a list whose type is known only at run time, and check it
       * @param lp the list, or nullptr for an empty handle
       * @throw FieldException if the list holds elements of another type
       */
      explicit ConstList(const lList *lp) : lp_(const_cast<lList *>(lp)) {
         if (lp != nullptr && lp->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            detail::throw_wrong_type<T>(lp->descr);
         }
      }

      /**
       * @brief Take a list from a source whose type is known, without a check
       * @param lp the list, or nullptr
       * @return the handle
       */
      static ConstList from_typed(const lList *lp) noexcept {
         ConstList handle;
         handle.lp_ = const_cast<lList *>(lp);
         return handle;
      }

      /**
       * @brief The list, for every function of the old interface
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
       * @brief Whether the handle holds a list
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return lp_ != nullptr;
      }

      /**
       * @brief Whether two handles hold the same list
       * @param a the first handle
       * @param b the second handle
       * @return true if both hold the same list or both are empty
       */
      friend bool operator==(const ConstList &a, const ConstList &b) noexcept {
         return a.lp_ == b.lp_;
      }

      /**
       * @brief Whether a handle is empty
       * @param a the handle
       * @return true if it is empty
       */
      friend bool operator==(const ConstList &a, std::nullptr_t) noexcept {
         return a.lp_ == nullptr;
      }

      /**
       * @brief The number of elements, as std::list::size() and lGetNumberOfElem() give it
       * @return the number, 0 for an empty handle
       */
      [[nodiscard]] std::size_t size() const noexcept {
         return lp_ != nullptr ? lp_->nelem : 0;
      }

      /**
       * @brief Whether the list has no element
       * @return true if empty or the handle is empty
       */
      [[nodiscard]] bool empty() const noexcept {
         return size() == 0;
      }

      /**
       * @brief The first element, for range-for; steps as lFirst() and lNext() do
       * @return the iterator
       */
      [[nodiscard]] const_iterator begin() const noexcept {
         return const_iterator(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The end of the list
       * @return the iterator
       */
      [[nodiscard]] const_iterator end() const noexcept {
         return {};
      }

      /**
       * @brief The first element, as std::list::front() and lFirst() give it
       * @return the element, or an empty handle for an empty list or an empty handle
       */
      [[nodiscard]] ConstElement<T> front() const noexcept {
         return ConstElement<T>::from_typed(lp_ != nullptr ? lp_->first : nullptr);
      }

      /**
       * @brief The last element, as std::list::back() and lLast() give it
       * @return the element, or an empty handle for an empty list or an empty handle
       */
      [[nodiscard]] ConstElement<T> back() const noexcept {
         return ConstElement<T>::from_typed(lp_ != nullptr ? lp_->last : nullptr);
      }

      /**
       * @brief The name of the list, as lGetListName() gives it
       * @return the name, or nullptr for an empty handle
       */
      [[nodiscard]] const char *name() const noexcept {
         return lp_ != nullptr ? list_name(lp_) : nullptr;
      }

      /**
       * @brief A deep copy of the list with its hash tables, as lCopyList() makes it
       *
       * The elements, their sub-lists and sub-objects are copied. The owning
       * handles are move-only like std::unique_ptr, so a copy is always this
       * explicit call.
       *
       * @param name the name of the copy; nullptr keeps the name of this list, as lCopyList() does
       * @return the owning handle on the copy, empty if this handle is empty, as lCopyList() gives nullptr
       *         for nullptr
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] ListPtr<T> clone(const char *name = nullptr) const {
         if (lp_ == nullptr) {
            return {};
         }
         return ListPtr<T>::from_typed(copy_list(name, lp_, true, false));
      }

      /**
       * @brief The first element with a key in a field, as lGetElemUlong(), lGetElemStr(), lGetElemHost() and their variants find it
       *
       * A lookup in a list field of an element, which lGetSubStr(), lGetSubUlong() and lGetSubHost() do, is
       * the typed getter and this lookup: `job.get<JB_ja_tasks>().find_first<JAT_task_number>(n)`. An empty
       * list field gives an empty list handle and so no match; an element without the field throws.
       * @tparam F the field number, of this object type, a number, string or host field
       * @param key the key
       * @return the element, or an empty handle if no element holds the key
       * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18)
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      [[nodiscard]] ConstElement<T> find_first(const detail::KeyOf<F> key) const {
         const void *state = nullptr;
         return ConstElement<T>::from_typed(lp_ != nullptr ? detail::lookup_first<F>(lp_, key, &state) : nullptr);
      }

      /**
       * @brief Every element with a key in a field, as lGetElemStrFirst() and lGetElemStrNext() and their variants walk them
       * @tparam F the field number, of this object type, a number, string or host field
       * @param key the key; a string or host name must outlive the range
       * @return the matches, as a range
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      [[nodiscard]] detail::KeyRange<F, ConstElement<T>> find_all(const detail::KeyOf<F> key) const noexcept {
         return {lp_, key};
      }

      /**
       * @brief The first element whose string field equals a value without regard to case, as lGetElemCaseStr() finds it
       * @tparam F a string field of this object type
       * @param value the value; nullptr finds nothing
       * @return the element, or an empty handle
       * @throw FieldException with reason AbsentField if the elements of a reduced list do not hold the field
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
      [[nodiscard]] ConstElement<T> find_first_nocase(const char *value) const {
         return ConstElement<T>::from_typed(lookup_first_str_nocase(lp_, F, value));
      }

      /**
       * @brief The first element whose string field equals a value, or starts with it if it ends in '*', as
       *        lGetElemStrLike() finds it
       * @tparam F a string field of this object type
       * @param value the value or prefix pattern; nullptr finds nothing
       * @return the element, or an empty handle
       * @throw FieldException with reason AbsentField if the elements of a reduced list do not hold the field
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
      [[nodiscard]] ConstElement<T> find_first_like(const char *value) const {
         return ConstElement<T>::from_typed(lookup_first_str_like(lp_, F, value));
      }

      /**
       * @brief A copy of the elements that fulfil a condition, reduced to a selection of fields, as lSelect() makes it
       *
       * Unlike lSelect(), no match gives an empty list rather than none, and
       * the result is named after this list unless a name is given.
       *
       * @param where the condition; an empty one selects every element
       * @param what the selection of fields
       * @param name the name of the result, or nullptr for the name of this list
       * @return the owning handle on the result, empty if this handle is empty
       * @throw FieldException with reason NullElement if @p what is empty
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lSelect(); core: ocs::tom::select_list()
       */
      [[nodiscard]] ListPtr<T> select(const Where<T> &where, const What<T> &what, const char *name = nullptr) const {
         if (lp_ == nullptr) {
            return {};
         }
         return ListPtr<T>::from_typed(select_list(name != nullptr ? name : list_name(lp_), lp_, where.get(),
                                                   what.get(), true, false));
      }

      /**
       * @brief A copy of every element, reduced to a selection of fields, as lSelect() with no condition makes it
       * @param what the selection of fields
       * @param name the name of the result, or nullptr for the name of this list
       * @return the owning handle on the result, empty if this handle is empty
       * @throw FieldException with reason NullElement if @p what is empty
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lSelect(); core: ocs::tom::select_list()
       */
      [[nodiscard]] ListPtr<T> select(const What<T> &what, const char *name = nullptr) const {
         return select(Where<T>(), what, name);
      }

      /**
       * @brief A copy of the elements that fulfil a condition, with every field
       * @param where the condition
       * @param name the name of the result, or nullptr for the name of this list
       * @return the owning handle on the result, empty if this handle is empty
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lSelect() with lWhat("%T(ALL)"); core: ocs::tom::select_list()
       */
      [[nodiscard]] ListPtr<T> select(const Where<T> &where, const char *name = nullptr) const {
         return select(where, what_all<T>(), name);
      }

      /**
       * @brief The first element that fulfils a condition, as std::find_if() and lFindFirstRW() find it
       * @param where the condition; an empty one takes the first element
       * @return the element, or an empty handle if none fulfils it, or for an empty handle
       * @see Old form: lFindFirstRW(); core: ocs::tom::find_first()
       */
      [[nodiscard]] ConstElement<T> find_if(const Where<T> &where) const {
         return ConstElement<T>::from_typed(lp_ != nullptr ? ocs::tom::find_first(lp_, where.get()) : nullptr);
      }

      /**
       * @brief The last element that fulfils a condition, as lFindLastRW() finds it
       * @param where the condition; an empty one takes the last element
       * @return the element, or an empty handle if none fulfils it, or for an empty handle
       * @see Old form: lFindLastRW(); core: ocs::tom::find_last()
       */
      [[nodiscard]] ConstElement<T> find_last_if(const Where<T> &where) const {
         return ConstElement<T>::from_typed(lp_ != nullptr ? ocs::tom::find_last(lp_, where.get()) : nullptr);
      }

   protected:
      lList *lp_ = nullptr;   ///< the list, or nullptr; const unless reached through List
   };

   /**
    * @brief A typed handle on a list that may be changed
    *
    * Adds the conversion to `lList *`, iteration with mutable element handles
    * and the operations that change the list, named after their counterparts
    * in std::list. Derives from ConstList, so it converts to its read-only
    * counterpart, never the other way round.
    *
    * @tparam T the struct of the object type of the elements, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class List : public ConstList<T> {
   public:
      using iterator = detail::ElementIterator<Element<T>>;   ///< iterates with mutable handles

      /** @brief An empty handle */
      constexpr List() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr List(std::nullptr_t) noexcept {}

      /**
       * @brief Take a list whose type is known only at run time, and check it
       * @param lp the list, or nullptr for an empty handle
       * @throw FieldException if the list holds elements of another type
       */
      explicit List(lList *lp) : ConstList<T>(lp) {}

      /**
       * @brief Take a list from a source whose type is known, without a check
       * @param lp the list, or nullptr
       * @return the handle
       */
      static List from_typed(lList *lp) noexcept {
         List handle;
         handle.lp_ = lp;
         return handle;
      }

      /**
       * @brief The list, for every function of the old interface
       * @return the list, or nullptr
       */
      operator lList *() const noexcept {
         return this->lp_;
      }

      /**
       * @brief The list, for varargs and deduced templates
       * @return the list, or nullptr
       */
      [[nodiscard]] lList *raw() const noexcept {
         return this->lp_;
      }

      /**
       * @brief The first element, for range-for; steps as lFirst() and lNext() do
       * @return the iterator
       */
      [[nodiscard]] iterator begin() const noexcept {
         return iterator(this->lp_ != nullptr ? this->lp_->first : nullptr);
      }

      /**
       * @brief The end of the list
       * @return the iterator
       */
      [[nodiscard]] iterator end() const noexcept {
         return {};
      }

      /**
       * @brief The first element, as std::list::front() and lFirstRW() give it
       * @return the element, or an empty handle for an empty list or an empty handle
       */
      [[nodiscard]] Element<T> front() const noexcept {
         return Element<T>::from_typed(this->lp_ != nullptr ? this->lp_->first : nullptr);
      }

      /**
       * @brief The last element, as std::list::back() and lLastRW() give it
       * @return the element, or an empty handle for an empty list or an empty handle
       */
      [[nodiscard]] Element<T> back() const noexcept {
         return Element<T>::from_typed(this->lp_ != nullptr ? this->lp_->last : nullptr);
      }

      /**
       * @brief The first element with a key in a field, as lGetElemUlong(), lGetElemStr(), lGetElemHost() and their variants find it
       *
       * A lookup in a list field of an element, which lGetSubStr(), lGetSubUlong() and lGetSubHost() do, is
       * the typed getter and this lookup: `job.get<JB_ja_tasks>().find_first<JAT_task_number>(n)`. An empty
       * list field gives an empty list handle and so no match; an element without the field throws.
       * @tparam F the field number, of this object type, a number, string or host field
       * @param key the key
       * @return the element, or an empty handle if no element holds the key
       * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18)
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      [[nodiscard]] Element<T> find_first(const detail::KeyOf<F> key) const {
         const void *state = nullptr;
         return Element<T>::from_typed(this->lp_ != nullptr ? detail::lookup_first<F>(this->lp_, key, &state) : nullptr);
      }

      /**
       * @brief Every element with a key in a field, as lGetElemStrFirst() and lGetElemStrNext() and their variants walk them
       * @tparam F the field number, of this object type, a number, string or host field
       * @param key the key; a string or host name must outlive the range
       * @return the matches, as a range
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      [[nodiscard]] detail::KeyRange<F, Element<T>> find_all(const detail::KeyOf<F> key) const noexcept {
         return {this->lp_, key};
      }

      /**
       * @brief The first element whose string field equals a value without regard to case, as lGetElemCaseStrRW() finds it
       * @tparam F a string field of this object type
       * @param value the value; nullptr finds nothing
       * @return the element, or an empty handle
       * @throw FieldException with reason AbsentField if the elements of a reduced list do not hold the field
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
      [[nodiscard]] Element<T> find_first_nocase(const char *value) const {
         return Element<T>::from_typed(lookup_first_str_nocase(this->lp_, F, value));
      }

      /**
       * @brief The first element whose string field equals a value, or starts with it if it ends in '*', as
       *        lGetElemStrLikeRW() finds it
       * @tparam F a string field of this object type
       * @param value the value or prefix pattern; nullptr finds nothing
       * @return the element, or an empty handle
       * @throw FieldException with reason AbsentField if the elements of a reduced list do not hold the field
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
      [[nodiscard]] Element<T> find_first_like(const char *value) const {
         return Element<T>::from_typed(lookup_first_str_like(this->lp_, F, value));
      }

      /**
       * @brief The first element that fulfils a condition, as std::find_if() and lFindFirstRW() find it
       * @param where the condition; an empty one takes the first element
       * @return the element, or an empty handle if none fulfils it, or for an empty handle
       * @see Old form: lFindFirstRW(); core: ocs::tom::find_first()
       */
      [[nodiscard]] Element<T> find_if(const Where<T> &where) const {
         return Element<T>::from_typed(this->lp_ != nullptr ? ocs::tom::find_first(this->lp_, where.get()) : nullptr);
      }

      /**
       * @brief The last element that fulfils a condition, as lFindLastRW() finds it
       * @param where the condition; an empty one takes the last element
       * @return the element, or an empty handle if none fulfils it, or for an empty handle
       * @see Old form: lFindLastRW(); core: ocs::tom::find_last()
       */
      [[nodiscard]] Element<T> find_last_if(const Where<T> &where) const {
         return Element<T>::from_typed(this->lp_ != nullptr ? ocs::tom::find_last(this->lp_, where.get()) : nullptr);
      }

      /**
       * @brief Remove and free the elements that fulfil a condition, as std::list::remove_if() does
       *
       * lSelectDestroy() keeps the elements that fulfil its condition, so it
       * is `remove_if(!std::move(where))`; unlike lSelectDestroy(), an emptied
       * list stays, since a handle does not own the list it points to.
       *
       * @param where the condition; an empty one removes every element
       * @return the number of elements removed
       * @see Old form: lSelectDestroy(), lSplit(); core: ocs::tom::compare(), ocs::tom::remove_elem()
       */
      std::size_t remove_if(const Where<T> &where) const {
         std::size_t removed = 0;
         if (this->lp_ == nullptr) {
            return removed;
         }
         lListElem *next;
         for (lListElem *ep = this->lp_->first; ep != nullptr; ep = next) {
            next = ep->next;
            if (compare(ep, where.get())) {
               remove_elem(this->lp_, ep);
               removed++;
            }
         }
         return removed;
      }

      /**
       * @brief Append an element, which the list takes over, as std::list::push_back() and lAppendElem() do
       * @param element the element, empty afterwards
       * @return the handle on the element, now in the list
       * @throw FieldException if the handle is empty, or in a debug build with reason DuplicateKey when
       *        the list holds a key of one of the element's unique fields; the element then stays with
       *        @p element
       */
      Element<T> push_back(ElementPtr<T> &&element) const {
         check_list();
         if (!element) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         // the core takes the element only if it throws nothing, so the owning handle releases it afterwards
         append_elem(this->lp_, element.get().raw());
         return Element<T>::from_typed(element.release());
      }

      /**
       * @brief Insert an element at the front, as std::list::push_front() and lInsertElem() with no predecessor do
       * @param element the element, which the list takes over; empty afterwards
       * @return the handle on the element, now in the list
       * @throw FieldException if this handle or @p element is empty, or in a debug build with reason
       *        DuplicateKey when the list holds a key of one of the element's unique fields; the
       *        element then stays with @p element
       */
      Element<T> push_front(ElementPtr<T> &&element) const {
         return insert(nullptr, std::move(element));
      }

      /**
       * @brief Insert an element after another one of this list, as std::forward_list::insert_after() and lInsertElem() do
       * @param after the element to insert after
       * @param element the element, which the list takes over; empty afterwards
       * @return the handle on the element, now in the list
       * @throw FieldException if a handle is empty, with reason ForeignList if @p after is not in this
       *        list, or in a debug build with reason DuplicateKey; the element then stays with @p element
       */
      Element<T> insert_after(const ConstElement<T> after, ElementPtr<T> &&element) const {
         check_member(after);
         return insert(const_cast<lListElem *>(after.raw()), std::move(element));
      }

      /**
       * @brief Remove an element of this list and free it, as std::list::erase() and lRemoveElem() do
       * @param element the element; the handle dangles afterwards
       * @throw FieldException if a handle is empty, or with reason ForeignList if the element is not in
       *        this list; nothing is changed then
       */
      void erase(const Element<T> element) const {
         check_member(element);
         lListElem *ep = element.raw();
         remove_elem(this->lp_, ep);
      }

      /**
       * @brief Take an element out of this list without freeing it, as std::set::extract() and lDechainElem() do
       * @param element the element
       * @return the owning handle on the element, which is free now
       * @throw FieldException if a handle is empty, or with reason ForeignList if the element is not in
       *        this list; nothing is changed then
       */
      [[nodiscard]] ElementPtr<T> extract(const Element<T> element) const {
         check_member(element);
         dechain_elem(this->lp_, element.raw());
         return ElementPtr<T>::from_typed(element.raw());
      }

      /**
       * @brief Move every element of a list to the end of this one and free the other list, as std::list::splice() and lAddList() do
       * @param other the list, empty afterwards; an empty owning handle adds nothing
       * @throw FieldException if this handle is empty, or in a debug build with reason DuplicateKey when
       *        an element of @p other holds a key of a unique field that this list holds; nothing is
       *        changed then
       */
      void splice(ListPtr<T> &&other) const {
         check_list();
         if (!other) {
            return;
         }
         check_unique_on_add_all(other.get());
         append_list(this->lp_, other.get().raw());
         other.reset();
      }

      /**
       * @brief Move every element of another list to the end of this one, as std::list::splice() and lAppendList() do
       * @param other the list, which stays and is empty afterwards; an empty handle or this list
       *        itself moves nothing
       * @throw FieldException if this handle is empty, or in a debug build with reason DuplicateKey when
       *        an element of @p other holds a key of a unique field that this list holds; nothing is
       *        changed then
       */
      void splice(const List<T> other) const {
         check_list();
         if (!other || other.raw() == this->lp_) {
            return;
         }
         check_unique_on_add_all(other);
         append_list(this->lp_, other.raw());
      }

      /**
       * @brief Create an element with a key and append it, as std::list::emplace_back() and lAddElemStr(),
       *        lAddElemUlong(), lAddElemUlong64() and lAddElemHost() do
       * @tparam F the key field, of this object type, a number, string or host field
       * @param key the key; a string or host name is copied
       * @return the handle on the new element, now in the list
       * @throw FieldException if this handle is empty, with reason NullKey for a null string or host
       *        name, or in a debug build DuplicateKey when a unique field holds the key already; nothing
       *        is changed then
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      Element<T> emplace_back(const detail::KeyOf<F> key) const {
         check_list();
         lList *lp = this->lp_;
         constexpr FieldType type = FieldTraits<F>::type;
         if constexpr (type == FieldType::String) {
            return Element<T>::from_typed(add_elem_str(lp, F, key, T::descriptor.e));
         } else if constexpr (type == FieldType::Host) {
            return Element<T>::from_typed(add_elem_host(lp, F, key.c_str(), T::descriptor.e));
         } else if constexpr (type == FieldType::Uint32) {
            return Element<T>::from_typed(add_elem_ulong(lp, F, key, T::descriptor.e));
         } else {
            return Element<T>::from_typed(add_elem_ulong64(lp, F, key, T::descriptor.e));
         }
      }

      /**
       * @brief Erase and free the first element with a key, as std::map::erase() by key and lDelElemStr(),
       *        lDelElemUlong(), lDelElemUlong64() and lDelElemHost() do
       *
       * Unlike the old functions, the list stays when it becomes empty: a handle
       * does not own the list it points to. Code that took a null list field
       * for "no entries" asks empty() instead.
       *
       * @tparam F the key field, of this object type, a number, string or host field
       * @param key the key
       * @return the number of elements erased, 0 or 1
       * @throw FieldException if this handle is empty, with reason NullKey for a null string or host name,
       *        or DuplicateKey when a unique field holds the key twice (N-H-18)
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      std::size_t erase(const detail::KeyOf<F> key) const {
         check_list();
         constexpr FieldType type = FieldTraits<F>::type;
         bool erased;
         if constexpr (type == FieldType::String) {
            erased = remove_elem_str(this->lp_, F, key);
         } else if constexpr (type == FieldType::Host) {
            erased = remove_elem_host(this->lp_, F, key.c_str());
         } else if constexpr (type == FieldType::Uint32) {
            erased = remove_elem_ulong(this->lp_, F, key);
         } else {
            erased = remove_elem_ulong64(this->lp_, F, key);
         }
         return erased ? 1 : 0;
      }

   private:
      /**
       * @brief Check that this handle holds a list
       * @throw FieldException with reason EmptyHandle if not
       */
      void check_list() const {
         if (this->lp_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
      }

      /**
       * @brief Check that an element is in this list
       * @param element the element
       * @throw FieldException with reason EmptyHandle if a handle is empty, ForeignList if the element
       *        is in another list or in none
       */
      void check_member(const ConstElement<T> element) const {
         check_list();
         if (!element) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         if (elem_list(element.raw()) != this->lp_) [[unlikely]] {
            throw_field_exception(FieldException::Reason::ForeignList, NoName, element.raw()->descr);
         }
      }

      /**
       * @brief In a debug build, check every element of another list against the unique fields of this one
       * @param other the list whose elements are to be moved here
       * @throw FieldException with reason DuplicateKey on the first key this list already holds
       */
      void check_unique_on_add_all([[maybe_unused]] const ConstList<T> other) const {
#ifdef ENABLE_DEBUG_CHECKS
         for (const ConstElement<T> element : other) {
            check_unique_on_add(this->lp_, element.raw());
         }
#endif
      }

      /**
       * @brief Insert an element after another one, or at the front
       * @param after the predecessor in this list, or nullptr for the front
       * @param element the element, which the list takes over
       * @return the handle on the element, now in the list
       */
      Element<T> insert(lListElem *after, ElementPtr<T> &&element) const {
         check_list();
         if (!element) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         // the core takes the element only if it throws nothing, so the owning handle releases it afterwards
         insert_elem(this->lp_, after, element.get().raw());
         return Element<T>::from_typed(element.release());
      }
   };

   /** @brief Whether a new list keeps hash tables for its hashed fields */
   enum class Hashing : bool {
      Off,   ///< no hash tables, as lCreateListHash() with false; lookups search linearly
      On     ///< hash tables, built when the list grows beyond the threshold, as lCreateList()
   };

   /**
    * @brief A list owned by the handle, freed with its elements when the handle goes out of scope
    *
    * Move-only, like `std::unique_ptr`. A function that takes a list over
    * takes its owning handle by rvalue reference, so the transfer shows at the
    * call.
    *
    * @tparam T the struct of the object type of the elements, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class ListPtr {
   public:
      /** @brief An empty owning handle */
      constexpr ListPtr() noexcept = default;

      /** @brief An empty owning handle, from nullptr */
      constexpr ListPtr(std::nullptr_t) noexcept {}

      /**
       * @brief Take over a list whose type is known only at run time, and check it
       * @param lp the list, or nullptr; freed by the handle
       * @throw FieldException if the list holds elements of another type; the list is not taken over then
       */
      explicit ListPtr(lList *lp) : lp_(List<T>(lp).raw()) {}

      /**
       * @brief Take over a list from a source whose type is known, without a check
       * @param lp the list, or nullptr; freed by the handle
       * @return the owning handle
       */
      [[nodiscard]] static ListPtr from_typed(lList *lp) noexcept {
         ListPtr handle;
         handle.lp_ = lp;
         return handle;
      }

      /**
       * @brief Create an empty list of the type, as lCreateList() and lCreateListHash() do
       * @param name the name of the list, for dumps and messages
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create(const char *name, const Hashing hashing = Hashing::On) {
         ListPtr handle;
         handle.lp_ = create_list(name, T::descriptor.e, hashing == Hashing::On);
         return handle;
      }

      ListPtr(const ListPtr &) = delete;
      ListPtr &operator=(const ListPtr &) = delete;

      /**
       * @brief Take over the list of another owning handle
       * @param other the handle, empty afterwards
       */
      ListPtr(ListPtr &&other) noexcept : lp_(std::exchange(other.lp_, nullptr)) {}

      /**
       * @brief Free the list held and take over the one of another owning handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      ListPtr &operator=(ListPtr &&other) noexcept {
         if (this != &other) {
            free_list(lp_);
            lp_ = std::exchange(other.lp_, nullptr);
         }
         return *this;
      }

      /** @brief Free the list held, with its elements, as lFreeList() does */
      ~ListPtr() {
         free_list(lp_);
      }

      /**
       * @brief A non-owning handle on the list
       * @return the handle, empty if this one is
       */
      [[nodiscard]] List<T> get() const noexcept {
         return List<T>::from_typed(lp_);
      }

      /**
       * @brief Call a member of the non-owning handle, as std::unique_ptr::operator-> does: `jobs->push_back(...)`
       * @return an object that leads the call on to get()
       */
      detail::Arrow<List<T>> operator->() const noexcept {
         return detail::Arrow<List<T>>(get());
      }

      /**
       * @brief The non-owning handle, as std::unique_ptr::operator* gives the object: `for (auto job : *jobs)`
       * @return the handle, empty if this one is; the same as get()
       */
      [[nodiscard]] List<T> operator*() const noexcept {
         return get();
      }

      /**
       * @brief Free the list held now with its elements, as std::unique_ptr::reset() and lFreeList() do
       */
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

      /**
       * @brief Whether the handle holds a list
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return lp_ != nullptr;
      }

   private:
      lList *lp_ = nullptr;   ///< the list owned, or nullptr
   };

} // namespace ocs::tom
