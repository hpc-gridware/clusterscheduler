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
 * A list handle is a view on an `ocs::tom::ListData *` whose elements are of one known
 * object type. It is the size of a pointer and converts implicitly back to the
 * pointer. Iterating it yields element handles of the type, and the lookups by
 * key use the lazy hash tables of the list library exactly as the old lookups
 * do; a key that is not found gives an empty handle or an empty range.
 */

#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <utility>

#include "uti/sge_string.h"

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_SortElems.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Sort.h"

namespace ocs::tom::detail {

   /**
    * @brief The slot of a sort criterion's field in the elements of a list
    *
    * All elements of a list have the list's layout, so the slot is the same for every element and is looked up
    * once per sort.
    *
    * @tparam F the field number
    * @param descr the descriptor of the list
    * @return the index into the elements' values
    * @throw FieldException with reason AbsentField if the (reduced) list does not hold the field, as
    *        check_sort_order() refuses it
    */
   template <auto F>
   int sort_slot(const ocs::tom::FieldDescr *descr) {
      constexpr int pos = FieldTraits<F>::pos;
      const int off = descr[pos].off;
      if (off == TOM_ABSENT) [[unlikely]] {
         throw_field_exception(FieldException::Reason::AbsentField, F, descr);
      }
      return pos + off;
   }

   /**
    * @brief Compare two elements by the sort criteria K, Ks..., with the rules of the core's compare_field()
    *
    * Strings and hosts compare with sge_strnullcmp(), every other type three-way; a descending criterion
    * negates the result. The field types are known at compile time, so the comparison is inlined.
    *
    * @tparam I the index of the slot of criterion K in @p slots
    * @tparam K the first criterion, asc<F> or desc<F>
    * @tparam Ks the further criteria
    * @tparam N the number of criteria
    * @param a the first element
    * @param b the second element
    * @param slots the slot of each criterion's field, from sort_slot()
    * @return less than, equal to or greater than 0 as @p a comes before, with or after @p b
    */
   template <std::size_t I, typename K, typename... Ks, std::size_t N>
   int compare_by_keys(const ocs::tom::ElementData *a, const ocs::tom::ElementData *b,
                       const std::array<int, N> &slots) {
      constexpr auto F = SortKeyTraits<K>::field;
      constexpr FieldType type = FieldTraits<F>::type;
      const ocs::tom::FieldValue &x = a->cont[slots[I]];
      const ocs::tom::FieldValue &y = b->cont[slots[I]];
      int result;
      if constexpr (type == FieldType::String) {
         result = sge_strnullcmp(x.str, y.str);
      } else if constexpr (type == FieldType::Host) {
         result = sge_strnullcmp(x.host, y.host);
      } else if constexpr (type == FieldType::Uint32) {
         result = (x.ul > y.ul) - (x.ul < y.ul);
      } else if constexpr (type == FieldType::Uint64) {
         result = (x.ul64 > y.ul64) - (x.ul64 < y.ul64);
      } else if constexpr (type == FieldType::Int32) {
         result = (x.i > y.i) - (x.i < y.i);
      } else if constexpr (type == FieldType::Int64) {
         result = (x.l > y.l) - (x.l < y.l);
      } else if constexpr (type == FieldType::Double) {
         result = (x.db > y.db) - (x.db < y.db);
      } else {
         static_assert(type == FieldType::Bool, "a sort criterion on a field the core cannot sort by");
         result = (x.b > y.b) - (x.b < y.b);
      }
      if constexpr (!SortKeyTraits<K>::ascending) {
         result = -result;
      }
      if constexpr (sizeof...(Ks) == 0) {
         return result;
      } else {
         return result != 0 ? result : compare_by_keys<I + 1, Ks...>(a, b, slots);
      }
   }

} // namespace ocs::tom::detail

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
       * @brief The position of a field in a descriptor, known at compile time, or -1 if the descriptor does not hold it
       *
       * Inline, as the free accessors check: a descriptor of another type
       * starts with another field number, and a reduced descriptor marks a
       * field it does not hold as absent. Gives what present_pos() gives,
       * without a call.
       *
       * @tparam F the field
       * @param descr the descriptor, or nullptr
       * @return the field's position, or -1
       */
      template <auto F>
      [[nodiscard]] inline int typed_pos(const ocs::tom::FieldDescr *descr) noexcept {
         using Tr = FieldTraits<F>;
         if (descr == nullptr || descr[0].nm != Tr::Type::fields[0].nm ||
             descr[Tr::pos].off == TOM_ABSENT) [[unlikely]] {
            return -1;
         }
         return Tr::pos;
      }

      /**
       * @brief The position of a field in the descriptor of a list of the field's type, known at compile time
       * @tparam F the field
       * @param lp the list, not nullptr
       * @return the field's position
       * @throw FieldException with reason AbsentField if the list is reduced without the field
       */
      template <auto F>
      [[nodiscard]] inline int present_pos_of(const ocs::tom::ListData *lp) {
         const int pos = typed_pos<F>(lp->descr);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, F, lp->descr);
         }
         return pos;
      }

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
      inline ocs::tom::ElementData *lookup_first(const ocs::tom::ListData *lp, const KeyOf<F> key,
                                                 const void **iterator) {
         *iterator = nullptr;
         const int pos = present_pos_of<F>(lp);
         constexpr FieldType type = FieldTraits<F>::type;
         if constexpr (type == FieldType::String) {
            return key != nullptr ? ocs::tom::lookup_first<type>(lp, pos, key, iterator) : nullptr;
         } else if constexpr (type == FieldType::Host) {
            return key.c_str() != nullptr ? ocs::tom::lookup_first<type>(lp, pos, key.c_str(), iterator) : nullptr;
         } else {
            return ocs::tom::lookup_first<type>(lp, pos, key, iterator);
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
      inline ocs::tom::ElementData *lookup_next(const ocs::tom::ListData *lp, const KeyOf<F> key,
                                                const void **iterator) {
         constexpr FieldType type = FieldTraits<F>::type;
         constexpr int pos = FieldTraits<F>::pos;
         if constexpr (type == FieldType::Host) {
            return ocs::tom::lookup_next<type>(lp, pos, key.c_str(), iterator);
         } else {
            return ocs::tom::lookup_next<type>(lp, pos, key, iterator);
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
         explicit ElementIterator(ocs::tom::ElementData *ep) noexcept : ep_(ep) {}

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
         ocs::tom::ElementData *ep_ = nullptr;   ///< the current element, or nullptr at the end
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
            iterator(const ocs::tom::ListData *lp, const KeyOf<F> key) : lp_(lp), key_(key) {
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
            const ocs::tom::ListData *lp_ = nullptr;      ///< the list
            KeyOf<F> key_{};                 ///< the key
            const void *state_ = nullptr;    ///< the state of the old lookup
            ocs::tom::ElementData *ep_ = nullptr;        ///< the current match, or nullptr at the end
         };

         /**
          * @brief The matches of a key in a list
          * @param lp the list, or nullptr for no matches
          * @param key the key; a string or host name must outlive the range
          */
         KeyRange(const ocs::tom::ListData *lp, const KeyOf<F> key) noexcept : lp_(lp), key_(key) {}

         /**
          * @brief The first match
          * @return the iterator
          * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18),
          *        AbsentField if the list is reduced and its elements lack the key field
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
         const ocs::tom::ListData *lp_;   ///< the list, or nullptr
         KeyOf<F> key_;      ///< the key
      };

   } // namespace detail

   /**
    * @brief A read-only typed handle on a list
    *
    * Converts implicitly to `const ocs::tom::ListData *`, made from one only explicitly and
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
       * @brief View the list an owning handle holds, as std::string_view views a std::string
       * @param lp the owning handle; it must outlive this handle
       */
      ConstList(const ListPtr<T> &lp) noexcept;

      /** @brief No view of a temporary owning handle: it would dangle */
      ConstList(const ListPtr<T> &&) = delete;

      /**
       * @brief Take a list whose type is known only at run time, and check it
       * @param lp the list, or nullptr for an empty handle
       * @throw FieldException if the list holds elements of another type
       */
      explicit ConstList(const ocs::tom::ListData *lp) : lp_(const_cast<ocs::tom::ListData *>(lp)) {
         if (lp != nullptr && lp->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            detail::throw_wrong_type<T>(lp->descr);
         }
      }

      /**
       * @brief Take a list from a source whose type is known, without a check
       * @param lp the list, or nullptr
       * @return the handle
       */
      static ConstList from_typed(const ocs::tom::ListData *lp) noexcept {
         ConstList handle;
         handle.lp_ = const_cast<ocs::tom::ListData *>(lp);
         return handle;
      }

      /**
       * @brief The list, for every function of the old interface
       * @return the list, or nullptr
       */
      operator const ocs::tom::ListData *() const noexcept {
         return lp_;
      }

      /**
       * @brief The list, for varargs and deduced templates
       * @return the list, or nullptr
       */
      [[nodiscard]] const ocs::tom::ListData *raw() const noexcept {
         return lp_;
      }

      /**
       * @brief The layout of the elements of the list, as lGetListDescr() gives it
       *
       * An opaque token: the full descriptor of the type or a reduction of it.
       * To make a list or element of the same layout, ListPtr<T>::create_like()
       * and ElementPtr<T>::create_like() take the handle itself.
       *
       * @return the descriptor, nullptr if the handle is empty
       * @see Old form: lGetListDescr()
       */
      [[nodiscard]] const ocs::tom::FieldDescr *descr() const noexcept {
         return lp_ != nullptr ? lp_->descr : nullptr;
      }

      /**
       * @brief Whether another list of the type has the same layout, as lCompListDescr() of their descriptors
       *        gives 0
       *
       * Both lists hold the same fields of the type: both full, or both
       * reduced to the same selection. Attributes other than the field type,
       * such as the hash flags of a list, are not compared, as
       * lCompListDescr() did not compare them. Use it before moving elements
       * of one list into the other: an element linked into a list must have
       * the layout of the list (CS-3112).
       *
       * @param other the other list
       * @return true if both handles hold lists of the same layout; false if they differ or either handle is
       *         empty, where lCompListDescr() gave -1
       * @see Old form: lCompListDescr(lGetListDescr(a), lGetListDescr(b)) == 0; core:
       *      ocs::tom::layout_difference()
       */
      [[nodiscard]] bool same_layout(const ConstList<T> other) const noexcept {
         return layout_difference(descr(), other.descr()) == 0;
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
       * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18),
       *        AbsentField if the list is reduced and its elements lack the key field
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
       * @return the matches, as a range; building it throws nothing, walking it throws as KeyRange::begin()
       *         and the iterator's increment do (AbsentField, DuplicateKey)
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
         if (lp_ == nullptr || value == nullptr) {
            return ConstElement<T>();
         }
         return ConstElement<T>::from_typed(lookup_first_str_nocase_at(lp_, detail::present_pos_of<F>(lp_), value));
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
         if (lp_ == nullptr || value == nullptr) {
            return ConstElement<T>();
         }
         return ConstElement<T>::from_typed(lookup_first_str_like_at(lp_, detail::present_pos_of<F>(lp_), value));
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


      /**
       * @brief Pack the list, as tom_pack_list() does; an empty handle packs a null list
       *
       * Does not compile for a type with an Int32 or Int64 field, which the pack format cannot carry (see
       * PackableType); such a type packs with pack(pb, what) and a selection without it.
       *
       * @param pb the buffer to append to
       * @throw PackException if the buffer cannot grow
       * @see Old form: tom_pack_list(); core: ocs::tom::pack_list()
       */
      void pack(sge_pack_buffer &pb) const {
         static_assert(PackableType<T>, "the type holds an Int32 or Int64 field, which cannot be packed");
         pack_list(&pb, lp_, nullptr, 0);
      }

      /**
       * @brief Pack the fields of the elements a selection names, as tom_pack_list_partial() does
       * @param pb the buffer to append to
       * @param what the selection
       * @throw PackException with PACK_BADARG if the elements do not hold a selected field, or if the buffer
       *        cannot grow
       * @see Old form: tom_pack_list_partial(); core: ocs::tom::pack_list()
       */
      void pack(sge_pack_buffer &pb, const What<T> &what) const {
         pack_list(&pb, lp_, what.get(), 0);
      }


      /**
       * @brief Write the list with its name, its descriptor and its elements as text, as lDumpList() does
       * @param fp the stream
       * @param indent the nesting level, three blanks each
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @throw ListCoreError if the stream is null or the last write failed
       * @see Old form: lDumpList(); core: ocs::tom::dump_list()
       */
      void dump(FILE *fp, const int indent = 0) const {
         if (lp_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         dump_list(fp, lp_, indent);
      }

      /**
       * @brief The number of nodes of the trees under the elements of the list, as lGetNumberOfNodes() counts them
       * @tparam F a list field of this type holding elements of this type, the children
       * @return the number, 0 for an empty handle
       * @see Old form: lGetNumberOfNodes(); core: ocs::tom::count_nodes()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) &&
                  std::is_same_v<typename FieldTraits<F>::Sub, T>
      [[nodiscard]] int count_nodes() const noexcept {
         return ocs::tom::count_nodes(nullptr, lp_, static_cast<int>(F));
      }

      /**
       * @brief The number of leaves of the trees under the elements of the list, as lGetNumberOfLeafs() counts them
       * @tparam F a list field of this type holding elements of this type, the children
       * @return the number, 0 for an empty handle
       * @see Old form: lGetNumberOfLeafs(); core: ocs::tom::count_leaves()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) &&
                  std::is_same_v<typename FieldTraits<F>::Sub, T>
      [[nodiscard]] int count_leaves() const noexcept {
         return ocs::tom::count_leaves(nullptr, lp_, static_cast<int>(F));
      }

      /**
       * @brief Write the list with its name and its elements as text for debugging, as lWriteListTo() does
       *
       * The text is not read back; dump() writes the form that is.
       *
       * @param fp the stream
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @see Old form: lWriteListTo(); core: ocs::tom::print_list()
       */
      void print(FILE *fp) const {
         if (lp_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         print_to(fp, [this](dstring *buffer) { print_list(buffer, lp_, 0); });
      }

   protected:
      ocs::tom::ListData *lp_ = nullptr;   ///< the list, or nullptr; const unless reached through List
   };

   /**
    * @brief A typed handle on a list that may be changed
    *
    * Adds the conversion to `ocs::tom::ListData *`, iteration with mutable element handles
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
       * @brief View the list an owning handle holds, as std::string_view views a std::string
       * @param lp the owning handle; it must outlive this handle
       */
      List(const ListPtr<T> &lp) noexcept : ConstList<T>(lp) {}

      /** @brief No view of a temporary owning handle: it would dangle */
      List(const ListPtr<T> &&) = delete;

      /**
       * @brief Take a list whose type is known only at run time, and check it
       * @param lp the list, or nullptr for an empty handle
       * @throw FieldException if the list holds elements of another type
       */
      explicit List(ocs::tom::ListData *lp) : ConstList<T>(lp) {}

      /**
       * @brief Take a list from a source whose type is known, without a check
       * @param lp the list, or nullptr
       * @return the handle
       */
      static List from_typed(ocs::tom::ListData *lp) noexcept {
         List handle;
         handle.lp_ = lp;
         return handle;
      }

      /**
       * @brief The list, for every function of the old interface
       * @return the list, or nullptr
       */
      operator ocs::tom::ListData *() const noexcept {
         return this->lp_;
      }

      /**
       * @brief The list, for varargs and deduced templates
       * @return the list, or nullptr
       */
      [[nodiscard]] ocs::tom::ListData *raw() const noexcept {
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
       * @throw FieldException with reason DuplicateKey when a unique field holds the key twice (N-H-18),
       *        AbsentField if the list is reduced and its elements lack the key field
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
       * @return the matches, as a range; building it throws nothing, walking it throws as KeyRange::begin()
       *         and the iterator's increment do (AbsentField, DuplicateKey)
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
         if (this->lp_ == nullptr || value == nullptr) {
            return Element<T>();
         }
         return Element<T>::from_typed(lookup_first_str_nocase_at(this->lp_, detail::present_pos_of<F>(this->lp_), value));
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
         if (this->lp_ == nullptr || value == nullptr) {
            return Element<T>();
         }
         return Element<T>::from_typed(lookup_first_str_like_at(this->lp_, detail::present_pos_of<F>(this->lp_), value));
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
         ocs::tom::ElementData *next;
         for (ocs::tom::ElementData *ep = this->lp_->first; ep != nullptr; ep = next) {
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
       * @throw FieldException if the handle is empty, with reason WrongLayout if the element was created with
       *        a descriptor of another layout than the list's, e.g. a full element for a reduced list
       *        (CS-3112), or in a debug build with reason DuplicateKey when the list holds a key of one of
       *        the element's unique fields; the element then stays with @p element
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
       * @throw FieldException if this handle or @p element is empty, with reason WrongLayout as
       *        #push_back, or in a debug build with reason DuplicateKey when the list holds a key of one
       *        of the element's unique fields; the element then stays with @p element
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
       *        list, WrongLayout as #push_back, or in a debug build with reason DuplicateKey; the element
       *        then stays with @p element
       */
      Element<T> insert_after(const ConstElement<T> after, ElementPtr<T> &&element) const {
         check_member(after);
         return insert(const_cast<ocs::tom::ElementData *>(after.raw()), std::move(element));
      }

      /**
       * @brief Remove an element of this list and free it, as std::list::erase() and lRemoveElem() do
       * @param element the element; the handle dangles afterwards
       * @throw FieldException if a handle is empty, or with reason ForeignList if the element is not in
       *        this list; nothing is changed then
       */
      void erase(const Element<T> element) const {
         check_member(element);
         ocs::tom::ElementData *ep = element.raw();
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
       * @throw FieldException if this handle is empty, with reason WrongLayout if @p other has a descriptor
       *        of another layout (CS-3112), or in a debug build with reason DuplicateKey when an element of
       *        @p other holds a key of a unique field that this list holds; nothing is changed then
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
       * @throw FieldException if this handle is empty, with reason WrongLayout if @p other has a descriptor
       *        of another layout (CS-3112), or in a debug build with reason DuplicateKey when an element of
       *        @p other holds a key of a unique field that this list holds; nothing is changed then
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
       * @brief Sort the list by a string or host field and keep the first element of each key, as std::list::unique()
       *        after sort() and lUniqStr() or lUniqHost() do
       *
       * The keys compare exactly, as strcmp() does; two null keys are equal.
       *
       * @tparam F a string or host field of this type
       * @throw FieldException with reason AbsentField if the elements do not hold @p F
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lUniqStr(), lUniqHost(); core: ocs::tom::unique_by_key()
       */
      template <auto F>
         requires FieldOf<F, T> && detail::Textual<F>
      void unique() const {
         if (this->lp_ == nullptr) {
            return;
         }
         unique_by_key(this->lp_, static_cast<int>(F));
      }

      /**
       * @brief Keep the elements that fulfil a condition and take the others out, as lSplit() does
       *
       * Unlike lSplit(), a list left empty stays, and the elements taken out
       * are given back rather than freed; dropping the result frees them.
       *
       * @param where the condition
       * @param name the name of the list taken out, or nullptr for the name of this list
       * @return the owning handle on the elements taken out, in their order, empty if there are none or this
       *         handle is empty
       * @throw ocs::RuntimeException if there is no memory; nothing is taken out then
       * @see Old form: lSplit(); core: ocs::tom::split_off()
       */
      [[nodiscard]] ListPtr<T> split_off(const Where<T> &where, const char *name = nullptr) const {
         if (this->lp_ == nullptr) {
            return {};
         }
         return ListPtr<T>::from_typed(
                 ocs::tom::split_off(this->lp_, where.get(), name != nullptr ? name : list_name(this->lp_)));
      }

      /**
       * @brief Take an element and every element after it out of the list, as lDechainList() does
       *
       * The elements keep their order and the layout of this list, and this
       * list keeps the elements before @p element, also when none is left.
       * lDechainList() appended them to a target list, creating it if there was
       * none; with an existing target, splice() the result into it. Unlike
       * lDechainList(), the hash tables of this list are kept up to date
       * rather than dropped.
       *
       * @param element the first element to take out
       * @param name the name of the list taken out, or nullptr for the name of this list
       * @return the owning handle on the elements taken out, a list with hash tables, as lCreateList() makes one
       * @throw FieldException with reason EmptyHandle if this handle or @p element is empty, ForeignList if
       *        @p element is not in this list, where lDechainList() aborted
       * @throw ocs::RuntimeException if there is no memory; nothing is taken out then
       * @see Old form: lDechainList(); core: ocs::tom::split_from()
       */
      [[nodiscard]] ListPtr<T> split_at(const ConstElement<T> element, const char *name = nullptr) const {
         check_member(element);
         return ListPtr<T>::from_typed(split_from(this->lp_, const_cast<ocs::tom::ElementData *>(element.raw()),
                                                  name != nullptr ? name : list_name(this->lp_)));
      }

      /**
       * @brief Give the list a hash table on a field if it has none, as ocs::tom::detail::hash_new_check() does
       *
       * For a field the type does not hash, to speed up the lookups by it on
       * this list; like every other table, it is built by the first lookup.
       * A list that holds the field in a hash table already, and a list whose
       * layout does not hold the field, are left as they are.
       *
       * @tparam F a string, host or number field of the type
       * @param unique true if no two elements hold the same value in the field
       * @return true on success or if there was nothing to do, also for an empty handle; false if the table
       *         could not be added, as ocs::tom::detail::hash_new_check() gives 0
       * @see Old form: ocs::tom::detail::hash_new_check()
       */
      template <auto F>
         requires FieldOf<F, T> && (detail::Textual<F> || FieldTraits<F>::type == FieldType::Uint32 ||
                                    FieldTraits<F>::type == FieldType::Uint64)
      bool add_hash(const bool unique = false) const {
         return ocs::tom::detail::hash_new_check(this->lp_, static_cast<int>(F), unique) != 0;
      }

      /**
       * @brief Remove the elements this list and another share by a string key from both, as lDiffListStr() does
       *
       * For each element of this list whose key the other holds, the first
       * element with that key is removed from each list. Unlike lDiffListStr(),
       * a list left empty stays.
       *
       * An empty handle on either side removes nothing and throws nothing, as lDiffListStr() with a null list.
       *
       * @tparam F a string field of this type
       * @param other the other list; an empty handle removes nothing
       * @throw FieldException with reason AbsentField if the elements do not hold @p F
       * @see Old form: lDiffListStr(); core: ocs::tom::remove_common_str()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::String)
      void remove_common(const List<T> other) const {
         ocs::tom::ListData *mine = this->lp_;
         ocs::tom::ListData *theirs = other.raw();
         remove_common_str(mine, theirs, static_cast<int>(F), false);
      }

      /**
       * @brief Sort the list by fields known at compile time, as std::list::sort() sorts, and lPSortList() did
       *
       * `jobs.sort<desc<JB_priority>, asc<JB_job_number>>()` sorts by priority,
       * the highest first, and jobs of one priority by their number. Elements
       * with equal keys keep their order. The hash tables are rebuilt.
       *
       * The comparison is generated from the criteria at compile time and inlined into the sort; the slots of
       * the fields are looked up once per call.
       *
       * @tparam K the first criterion, asc<F> or desc<F> on a field of this type
       * @tparam Ks the further criteria
       * @throw FieldException with reason AbsentField if the elements do not hold a field of the order
       * @throw ocs::RuntimeException if there is no memory; the list is unchanged then
       * @see Old form: lPSortList(); core: ocs::tom::detail::sort_elems()
       */
      template <typename K, typename... Ks>
         requires detail::SortKeyOf<K, T> && (detail::SortKeyOf<Ks, T> && ...)
      void sort() const {
         if (this->lp_ == nullptr) {
            return;
         }
         // the slots are looked up once, which also refuses an absent field of a reduced list as before, even a
         // short one; each comparison is then inlined, without a sort order to interpret
         const std::array<int, 1 + sizeof...(Ks)> slots{
               detail::sort_slot<detail::SortKeyTraits<K>::field>(this->lp_->descr),
               detail::sort_slot<detail::SortKeyTraits<Ks>::field>(this->lp_->descr)...};
         if (this->lp_->nelem < 2) {
            return;
         }
         detail::sort_elems(this->lp_, [&slots](const ocs::tom::ElementData *a, const ocs::tom::ElementData *b) {
            return detail::compare_by_keys<0, K, Ks...>(a, b, slots) < 0;
         });
      }

      /**
       * @brief Sort the list by a sort order built before, as lSortList() did
       * @param order the order
       * @throw FieldException with reason NullElement if @p order is empty, AbsentField if the elements do not
       *        hold a field of the order
       * @throw ocs::RuntimeException if there is no memory; the list is unchanged then
       * @see Old form: lSortList(); core: ocs::tom::sort_list()
       */
      void sort(const SortOrder<T> &order) const {
         if (this->lp_ == nullptr) {
            return;
         }
         check_sort_order(this->lp_->descr, order.get());
         sort_list(this->lp_, order.get());
      }

      /**
       * @brief Sort the list by a comparison, as std::list::sort(comp) does
       *
       * For orders the fields do not express, e.g. a key computed from
       * several fields. Elements that compare equal keep their order.
       *
       * @param less true if the first element comes before the second; a strict weak ordering
       * @throw ocs::RuntimeException if there is no memory; whatever @p less throws; the list is unchanged then
       * @see Core: ocs::tom::sort_list_with()
       */
      template <typename Less>
         requires std::predicate<const Less &, ConstElement<T>, ConstElement<T>>
      void sort(const Less &less) const {
         if (this->lp_ == nullptr) {
            return;
         }
         sort_list_with(this->lp_, [&less](const ocs::tom::ElementData *a, const ocs::tom::ElementData *b) {
            return less(ConstElement<T>::from_typed(a), ConstElement<T>::from_typed(b));
         });
      }

      /**
       * @brief Insert an element at the place a sort order gives it, as lInsertSorted() did
       *
       * The list must be sorted by @p order. The element goes in front of the
       * first element that does not come before it, or to the end.
       *
       * @param order the order the list is sorted by
       * @param element the element, which the list takes over; empty afterwards
       * @return the handle on the element, now in the list
       * @throw FieldException if this handle, @p order or @p element is empty, with reason AbsentField if
       *        the elements do not hold a field of the order, or in a debug build DuplicateKey when the list
       *        holds a key of one of the element's unique fields; the element then stays with @p element
       * @see Old form: lInsertSorted(); core: ocs::tom::insert_sorted()
       */
      Element<T> insert_sorted(const SortOrder<T> &order, ElementPtr<T> &&element) const {
         check_list();
         if (!element) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         check_sort_order(this->lp_->descr, order.get());
         check_sort_order(element.get().raw()->descr, order.get());
         // the core takes the element only if it throws nothing, so the owning handle releases it afterwards
         ocs::tom::insert_sorted(this->lp_, element.get().raw(), order.get());
         return Element<T>::from_typed(element.release());
      }

      /**
       * @brief Move an element of this list to the place its changed key gives it, as lResortElem() did
       * @param order the order the list is sorted by, apart from @p element
       * @param element the element, in this list
       * @throw FieldException if a handle or @p order is empty, with reason ForeignList if the element is not in
       *        this list, AbsentField if the elements do not hold a field of the order; nothing is changed then
       * @see Old form: lResortElem(); core: ocs::tom::resort_elem()
       */
      void resort(const SortOrder<T> &order, const Element<T> element) const {
         check_member(element);
         check_sort_order(this->lp_->descr, order.get());
         resort_elem(this->lp_, element.raw(), order.get());
      }

      /**
       * @brief Create an element with a key and append it, as std::list::emplace_back() and lAddElemStr(),
       *        lAddElemUlong(), lAddElemUlong64() and lAddElemHost() do
       *
       * The element gets the list's descriptor, so a reduced list gets a
       * reduced element (CS-3112).
       *
       * @tparam F the key field, of this object type, a number, string or host field
       * @param key the key; a string or host name is copied
       * @return the handle on the new element, now in the list
       * @throw FieldException if this handle is empty, with reason NullKey for a null string or host
       *        name, WrongType for a list reduced without the key field, or in a debug build DuplicateKey
       *        when a unique field holds the key already; nothing is changed then
       */
      template <auto F>
         requires FieldOf<F, T> && detail::LookupField<F>
      Element<T> emplace_back(const detail::KeyOf<F> key) const {
         check_list();
         ocs::tom::ListData *lp = this->lp_;
         constexpr FieldType type = FieldTraits<F>::type;
         constexpr int pos = FieldTraits<F>::pos;
         if constexpr (type == FieldType::String) {
            if (key == nullptr) [[unlikely]] {
               throw_field_exception(FieldException::Reason::NullKey, F, T::descriptor.e);
            }
            return Element<T>::from_typed(add_elem_at<type>(lp, pos, key, T::descriptor.e));
         } else if constexpr (type == FieldType::Host) {
            if (key.c_str() == nullptr) [[unlikely]] {
               throw_field_exception(FieldException::Reason::NullKey, F, T::descriptor.e);
            }
            return Element<T>::from_typed(add_elem_at<type>(lp, pos, key.c_str(), T::descriptor.e));
         } else {
            return Element<T>::from_typed(add_elem_at<type>(lp, pos, key, T::descriptor.e));
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
            if (key == nullptr) [[unlikely]] {
               throw_field_exception(FieldException::Reason::NullKey, F, this->lp_->descr);
            }
            erased = remove_elem_at<type>(this->lp_, detail::present_pos_of<F>(this->lp_), key);
         } else if constexpr (type == FieldType::Host) {
            if (key.c_str() == nullptr) [[unlikely]] {
               throw_field_exception(FieldException::Reason::NullKey, F, this->lp_->descr);
            }
            erased = remove_elem_at<type>(this->lp_, detail::present_pos_of<F>(this->lp_), key.c_str());
         } else {
            erased = remove_elem_at<type>(this->lp_, detail::present_pos_of<F>(this->lp_), key);
         }
         return erased ? 1 : 0;
      }

      // The members below forward to a free form of ocs_FreeAccess.h and are defined there, after it; each keeps
      // the run-time behaviour of its free form, also for an empty handle.

      /**
       * @brief Keep only the last element of each key, as ocs_compress_definition_list() did
       *
       * Removes every earlier element with the key of a later one, so the order of the elements that stay is
       * kept; a string or host key that is nullptr is never a repetition, host names compare as host names.
       *
       * @tparam Key the key field, of this object type: a number, string or host field; not a list, object or
       *         reference field
       * @throw FieldException with reason AbsentField if the list is reduced and lacks the key field
       * @see free form ocs::tom::keep_last()
       */
      template <auto Key>
         requires FieldOf<Key, T> && (FieldTraits<Key>::type != FieldType::List &&
                                      FieldTraits<Key>::type != FieldType::Object &&
                                      FieldTraits<Key>::type != FieldType::Ref)
      void keep_last() const;

      /**
       * @brief Merge another list into this one by key: a known key takes the new value, a new key is appended
       *        as a copy, as ocs_merge_definition_list() did
       *
       * This list is the destination, the argument the source: `dst.merge_by_key<K, V>(src)` in the order of
       * the free form `merge_by_key<K, V>(dst, src)`. The source stays as it is. Two string or host keys that
       * are both nullptr are equal, host names compare as host names.
       *
       * @tparam Key the key field, of this object type: Uint32, Uint64, String or Host
       * @tparam Value the value field, of this object type: a number, string or host field
       * @param src the list merged from, may be an empty handle; an owning handle converts to it
       * @throw FieldException with reason EmptyHandle if this handle is empty and @p src holds an element,
       *        AbsentField if a list is reduced and lacks the key or the value field, WrongLayout if an element of
       *        @p src to append has another layout than this list; in a debug build DuplicateKey
       * @see free form ocs::tom::merge_by_key()
       */
      template <auto Key, auto Value>
         requires FieldOf<Key, T> && detail::LookupField<Key> && FieldOf<Value, T> &&
                  (FieldTraits<Value>::type != FieldType::List && FieldTraits<Value>::type != FieldType::Object &&
                   FieldTraits<Value>::type != FieldType::Ref)
      void merge_by_key(const ConstList<T> &src) const;

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
      Element<T> insert(ocs::tom::ElementData *after, ElementPtr<T> &&element) const {
         check_list();
         if (!element) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         // the core takes the element only if it throws nothing, so the owning handle releases it afterwards
         insert_elem(this->lp_, after, element.get().raw());
         return Element<T>::from_typed(element.release());
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
   /**
    * @brief How ListPtr::from_string() reads the name "none"
    */
   enum class NoneName : bool {
      Name,   ///< as a name like any other, as lString2List() does
      Empty   ///< as no name, as lString2ListNone() does; beside other names it is an error
   };

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
      explicit ListPtr(ocs::tom::ListData *lp) : lp_(List<T>(lp).raw()) {}

      /**
       * @brief Take over a list from a source whose type is known, without a check
       * @param lp the list, or nullptr; freed by the handle
       * @return the owning handle
       */
      [[nodiscard]] static ListPtr from_typed(ocs::tom::ListData *lp) noexcept {
         ListPtr handle;
         handle.lp_ = lp;
         return handle;
      }

      /**
       * @brief A list of the names a string holds, one element per name, as lString2List() and lString2ListNone()
       *        make it
       *
       * A name the list holds already is skipped.
       *
       * @tparam F the string or host field the names go into
       * @param s the string; nullptr gives an empty handle
       * @param delimiters the delimiters, or nullptr for white space
       * @param none how the name "none" is read; with NoneName::Empty a string of only "none" gives an empty handle
       * @return the owning handle on the list, empty if the string holds no name
       * @throw ListCoreError if @p none is NoneName::Empty and "none" stands beside other names
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lString2List(), lString2ListNone(); core: ocs::tom::string_to_list()
       */
      template <auto F>
         requires FieldOf<F, T> && detail::Textual<F>
      [[nodiscard]] static ListPtr from_string(const char *s, const char *delimiters = nullptr,
                                               const NoneName none = NoneName::Name) {
         ocs::tom::ListData *lp = nullptr;
         if (none == NoneName::Empty) {
            string_to_list_none(s, lp, T::descriptor.e, static_cast<int>(F), delimiters);
         } else {
            string_to_list(s, lp, T::descriptor.e, static_cast<int>(F), delimiters);
         }
         return from_typed(lp);
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

      /**
       * @brief Create an empty list of the type with a given descriptor, as lCreateList() and lCreateListHash()
       *        do
       *
       * For a list that must have the descriptor of another list or element,
       * typically a reduced one. A reduced descriptor keeps the first field and
       * the full length of the type, so it is checked like the list the checked
       * constructor takes.
       *
       * @param name the name of the list, for dumps and messages
       * @param descr the descriptor of the elements, the full one of the type or a reduction of it
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw FieldException with reason WrongType if @p descr is of another type or empty, NullDescriptor
       *        if it is null
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create(const char *name, const ocs::tom::FieldDescr *descr,
                                          const Hashing hashing = Hashing::On) {
         if (descr != nullptr && descr[0].nm != T::fields[0].nm) [[unlikely]] {
            detail::throw_wrong_type<T>(descr);
         }
         ListPtr handle;
         handle.lp_ = create_list(name, descr, hashing == Hashing::On);
         return handle;
      }

      /**
       * @brief Create an empty list of a reduced layout, as lCreateList() with a descriptor of
       *        ocs::tom::descr_reduce() does
       * @param name the name of the list, for dumps and messages
       * @param layout the layout of the elements, from layout<F...>()
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p layout is default-constructed
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create(const char *name, const Layout<T> layout,
                                          const Hashing hashing = Hashing::On) {
         ListPtr handle;
         handle.lp_ = create_list(name, layout.get(), hashing == Hashing::On);
         return handle;
      }

      /**
       * @brief Create an empty list of the layout a selection reduces the type to, as lCreateList() with the
       *        descriptor of lReduceDescr() does
       *
       * For a selection known only at run time; a fixed set of fields is
       * layout<F...>().
       *
       * @param name the name of the list, for dumps and messages
       * @param what the selection of fields
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw FieldException with reason NullElement if @p what is empty (see ocs_Select.h), NullDescriptor if
       *        it selects no field of the type, where lReduceDescr() gave no descriptor
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lReduceDescr() and lCreateList(); core: ocs::tom::reduced_descr()
       */
      [[nodiscard]] static ListPtr create(const char *name, const What<T> &what, const Hashing hashing = Hashing::On) {
         if (!what) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullElement, NoName, T::descriptor.e);
         }
         const ocs::tom::FieldDescr *descr = reduced_descr(T::descriptor.e, what.get());
         if (descr == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullDescriptor, NoName, T::descriptor.e);
         }
         ListPtr handle;
         try {
            // the list takes a reference of its own on the reduction
            handle.lp_ = create_list(name, descr, hashing == Hashing::On);
         } catch (...) {
            ocs::tom::release_descr(descr);
            throw;
         }
         ocs::tom::release_descr(descr);
         return handle;
      }

      /**
       * @brief Create an empty list with the layout of another list, as lCreateList(name, lGetListDescr(lp))
       *        does
       *
       * For a list that takes elements out of another, possibly reduced, list:
       * an element linked into a list must have the layout of the list
       * (CS-3112).
       *
       * @param name the name of the list, for dumps and messages
       * @param like the list whose layout the new one gets
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p like is empty, as lCreateList() of the null
       *        descriptor lGetListDescr() gave for it
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create_like(const char *name, const ConstList<T> like,
                                               const Hashing hashing = Hashing::On) {
         ListPtr handle;
         handle.lp_ = create_list(name, like.descr(), hashing == Hashing::On);
         return handle;
      }

      /**
       * @brief Create an empty list with the layout of an element, as lCreateList(name, lGetElemDescr(ep)) does
       *
       * For a list that takes a possibly reduced element, or copies of it.
       *
       * @param name the name of the list, for dumps and messages
       * @param like the element whose layout the new list gets
       * @param hashing whether the list keeps hash tables for its hashed fields
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p like is empty, as lCreateList() of the null
       *        descriptor lGetElemDescr() gave for it
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ListPtr create_like(const char *name, const ConstElement<T> like,
                                               const Hashing hashing = Hashing::On) {
         ListPtr handle;
         handle.lp_ = create_list(name, like.descr(), hashing == Hashing::On);
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
            if (lp_ != nullptr) {   // a moved-from owner is empty: no call
               free_list(lp_);
            }
            lp_ = std::exchange(other.lp_, nullptr);
         }
         return *this;
      }

      /** @brief Free the list held, with its elements, as lFreeList() does */
      ~ListPtr() {
         if (lp_ != nullptr) {   // a moved-from owner is empty: no call
            free_list(lp_);
         }
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
         if (lp_ != nullptr) {   // a moved-from owner is empty: no call
            free_list(lp_);
         }
      }

      /**
       * @brief Give up ownership
       * @return the list, which the caller now owns, or nullptr
       */
      [[nodiscard]] ocs::tom::ListData *release() noexcept {
         return std::exchange(lp_, nullptr);
      }

      /**
       * @brief Whether the handle holds a list
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return lp_ != nullptr;
      }

      /**
       * @brief Whether the handle is empty, as a raw pointer compared with nullptr
       * @param a the handle
       * @return true if it holds no list
       */
      friend bool operator==(const ListPtr &a, std::nullptr_t) noexcept {
         return a.lp_ == nullptr;
      }


      /**
       * @brief Unpack a list packed by pack() or tom_pack_list(), as tom_unpack_list() does
       * @param pb the buffer to read from
       * @param flags read only the fields whose flags match, 0 for all
       * @return the owning handle on the list, empty for a list that was null
       * @throw FieldException with reason WrongType if the buffer holds a list of another type; it is freed
       * @throw PackException with PACK_FORMAT for bytes that are cut off, PACK_ENOMEM
       * @see Old form: tom_unpack_list(), tom_unpack_list_partial(); core: ocs::tom::unpack_list()
       */
      [[nodiscard]] static ListPtr unpack(sge_pack_buffer &pb, const int flags = 0) {
         ocs::tom::ListData *lp = unpack_list(&pb, flags);
         if (lp != nullptr && lp->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            try {
               detail::throw_wrong_type<T>(lp->descr);
            } catch (...) {
               free_list(lp);
               throw;
            }
         }
         return from_typed(lp);
      }


      /**
       * @brief Read a list written as text by dump(), as lUndumpList() does
       * @param fp the stream
       * @param name the name of the list, or nullptr for the name in the text
       * @return the owning handle on the list
       * @throw FieldException with reason WrongType if the text holds a list of another type; it is freed
       * @throw ListCoreError if the text cannot be read
       * @see Old form: lUndumpList(); core: ocs::tom::undump_list()
       */
      [[nodiscard]] static ListPtr undump(FILE *fp, const char *name = nullptr) {
         ocs::tom::ListData *lp = undump_list(fp, name, nullptr);
         if (lp->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            try {
               detail::throw_wrong_type<T>(lp->descr);
            } catch (...) {
               free_list(lp);
               throw;
            }
         }
         return from_typed(lp);
      }

   private:
      ocs::tom::ListData *lp_ = nullptr;   ///< the list owned, or nullptr
   };

   /**
    * @brief View the list an owning handle holds
    * @param lp the owning handle; it must outlive this handle
    */
   template <ObjectType T>
   ConstList<T>::ConstList(const ListPtr<T> &lp) noexcept : lp_(lp.get().raw()) {}

} // namespace ocs::tom

#include "tom/ocs_FreeAccess.h"
