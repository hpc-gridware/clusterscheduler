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

#include "cull/cull_listP.h"
#include "cull/cull_multitype.h"
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
       * @brief The next element of a list with a key in a field, through the old lookup
       * @tparam F the field number
       * @param lp the list
       * @param key the key
       * @param iterator the lookup's state
       * @return the element, or nullptr
       */
      template <auto F>
      inline lListElem *lookup_next(const lList *lp, const KeyOf<F> key, const void **iterator) {
         constexpr FieldType type = FieldTraits<F>::type;
         if constexpr (type == FieldType::Uint32) {
            return lGetElemUlongNextRW(lp, F, key, iterator);
         } else if constexpr (type == FieldType::Uint64) {
            return lGetElemUlong64NextRW(lp, F, key, iterator);
         } else if constexpr (type == FieldType::String) {
            return lGetElemStrNextRW(lp, F, key, iterator);
         } else {
            return lGetElemHostNextRW(lp, F, key.c_str(), iterator);
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
       * @brief The number of elements
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
       * @brief The first element
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
       * @brief The first element with a key in a field
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
       * @brief Every element with a key in a field
       * @tparam F the field number, of this object type, a number, string or host field
       * @param key the key; a string or host name must outlive the range
       * @return the matches, as a range
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      [[nodiscard]] detail::KeyRange<F, ConstElement<T>> find_all(const detail::KeyOf<F> key) const noexcept {
         return {lp_, key};
      }

   protected:
      lList *lp_ = nullptr;   ///< the list, or nullptr; const unless reached through List
   };

   /**
    * @brief A typed handle on a list that may be changed
    *
    * Adds the conversion to `lList *`, iteration with mutable element handles
    * and appending. Derives from ConstList, so it converts to its read-only
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
       * @brief The first element
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
       * @brief The first element with a key in a field
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
       * @brief Every element with a key in a field
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
       * @brief Append an element, which the list takes over
       * @param element the element, empty afterwards
       * @return the handle on the element, now in the list
       * @throw FieldException if the handle is empty, or in a debug build with reason DuplicateKey when
       *        the list holds a key of one of the element's unique fields; the element then stays with
       *        @p element
       */
      Element<T> append(ElementPtr<T> &&element) const {
         if (this->lp_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         check_unique_on_add(this->lp_, element.get().raw());
         lListElem *ep = element.release();
         lAppendElem(this->lp_, ep);
         return Element<T>::from_typed(ep);
      }
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
       * @brief Create an empty list of the type
       * @param name the name of the list, for dumps and messages
       * @return the owning handle
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create(const char *name) {
         ListPtr handle;
         handle.lp_ = lCreateList(name, T::descriptor.e);
         if (handle.lp_ == nullptr) [[unlikely]] {
            throw ocs::RuntimeException("out of memory creating a list");
         }
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
            lFreeList(&lp_);
            lp_ = std::exchange(other.lp_, nullptr);
         }
         return *this;
      }

      /** @brief Free the list held, with its elements */
      ~ListPtr() {
         lFreeList(&lp_);
      }

      /**
       * @brief A non-owning handle on the list
       * @return the handle, empty if this one is
       */
      [[nodiscard]] List<T> get() const noexcept {
         return List<T>::from_typed(lp_);
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
