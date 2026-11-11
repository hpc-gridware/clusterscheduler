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
 * @brief Typed selections of the typed object model: What, Where, what<F...>(), field<F>()
 *
 * A selection of fields (`What<T>`) and a condition (`Where<T>`) are owning
 * handles on the `lEnumeration` and `lCondition` of the list library, built
 * from field numbers given as template arguments: a field of another type, a
 * value of another type than the field, or a nested selection or condition on
 * a field that is not a list does not compile, where lWhat() and lWhere()
 * parse a format string at run time and take their values through varargs
 * unchecked (N-H-17).
 *
 * @code
 * auto what = what<US_name>().with<US_entries>(what<UE_name>());
 * auto where = field<US_type>().has_bits(US_ACL) && !(field<US_name>() == "defaultdepartment");
 * ListPtr<US> acls = usersets.select(where, what);
 * @endcode
 *
 * The handles give the raw pointers with get(), so they serve every function
 * of the old interface as well. The list and element handles take them in
 * select(), find_if(), remove_if() and matches().
 */

#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "cull/cull_multitypeP.h"
#include "cull/cull_what.h"
#include "cull/cull_whatP.h"
#include "cull/cull_where.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_SelectCore.h"

namespace ocs::tom {

   template <ObjectType T> class What;
   template <ObjectType T> class Where;

   namespace detail {

      /**
       * @brief A field of a given object type, as FieldOf in ocs_Element.h, which this header precedes
       * @tparam F the field number
       * @tparam T the struct of the object type
       */
      template <auto F, typename T>
      concept SelectFieldOf = TypedField<decltype(F)> && std::is_same_v<typename TypeOf<decltype(F)>::type, T>;

      /**
       * @brief A field that can be compared with ==, !=, <, <=, > and >=: a number, bool, string or host field
       * @tparam F the field number
       */
      template <auto F>
      concept Comparable = FieldTraits<F>::type == FieldType::Uint32 || FieldTraits<F>::type == FieldType::Uint64 ||
                           FieldTraits<F>::type == FieldType::Int32 || FieldTraits<F>::type == FieldType::Int64 ||
                           FieldTraits<F>::type == FieldType::Double || FieldTraits<F>::type == FieldType::Bool ||
                           FieldTraits<F>::type == FieldType::String || FieldTraits<F>::type == FieldType::Host;

      /**
       * @brief A string or host field, which the named string comparisons take
       * @tparam F the field number
       */
      template <auto F>
      concept Textual = FieldTraits<F>::type == FieldType::String || FieldTraits<F>::type == FieldType::Host;

      /**
       * @brief A list or object field whose sub-type the model names, which a nested selection takes
       * @tparam F the field number
       */
      template <auto F>
      concept Nestable = (FieldTraits<F>::type == FieldType::List || FieldTraits<F>::type == FieldType::Object) &&
                         FieldTraits<F>::has_sub;

      /**
       * @brief The value a field is compared with: a C string for a string or host field, the field's value type otherwise
       * @tparam F the field number
       */
      template <auto F>
      struct CompareArg {
         /// no value: the field cannot be compared, so the comparison operators are not viable
         struct type {};
      };

      /** @brief A number or bool field is compared with a value of its type */
      template <auto F>
         requires(Comparable<F> && !Textual<F>)
      struct CompareArg<F> {
         using type = typename ValueOf<FieldTraits<F>::type>::type;   ///< the value type of the field
      };

      /** @brief A string or host field is compared with a C string */
      template <auto F>
         requires Textual<F>
      struct CompareArg<F> {
         using type = const char *;   ///< the value, copied into the condition
      };

      /**
       * @brief Build the comparison of a field with a value through the core
       * @tparam F the field number
       * @param op the comparison
       * @param value the value
       * @return the condition, owned by the caller
       */
      template <auto F>
      lCondition *make_field_compare(const CompareOp op, const typename CompareArg<F>::type value) {
         using Tr = FieldTraits<F>;
         lMultiType v{};
         if constexpr (Tr::type == FieldType::Uint32) {
            v.ul = value;
         } else if constexpr (Tr::type == FieldType::Uint64) {
            v.ul64 = value;
         } else if constexpr (Tr::type == FieldType::Int32) {
            v.i = value;
         } else if constexpr (Tr::type == FieldType::Int64) {
            v.l = value;
         } else if constexpr (Tr::type == FieldType::Double) {
            v.db = value;
         } else if constexpr (Tr::type == FieldType::Bool) {
            v.b = value;
         } else {
            // the core copies the string; it does not change it
            v.str = const_cast<char *>(value);
         }
         return make_compare(Tr::Type::descriptor.e, static_cast<int>(F), op, ocs::tom::cull_type_of(Tr::type), v);
      }

   } // namespace detail

   /**
    * @brief A condition on the elements of a type, owned by the handle: the typed form of lWhere()
    *
    * Built with field<F>() and combined with &&, || and !, which take their
    * operands over, so an operand held in a variable is given with std::move().
    * Move-only, like std::unique_ptr. The fields of a condition must be present
    * in the elements it is evaluated on.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class Where {
   public:
      using Type = T;   ///< the object type

      /** @brief An empty condition; selecting with it selects every element, as a null lCondition does */
      constexpr Where() noexcept = default;

      /**
       * @brief Take over a condition built for this type
       * @param cp the condition, or nullptr; freed by the handle
       * @return the handle
       */
      [[nodiscard]] static Where from_typed(lCondition *cp) noexcept {
         Where handle;
         handle.cp_ = cp;
         return handle;
      }

      Where(const Where &) = delete;
      Where &operator=(const Where &) = delete;

      /**
       * @brief Take over the condition of another handle
       * @param other the handle, empty afterwards
       */
      Where(Where &&other) noexcept : cp_(std::exchange(other.cp_, nullptr)) {}

      /**
       * @brief Free the condition held and take over the one of another handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      Where &operator=(Where &&other) noexcept {
         if (this != &other) {
            free_where(cp_);
            cp_ = std::exchange(other.cp_, nullptr);
         }
         return *this;
      }

      /** @brief Free the condition, as lFreeWhere() does */
      ~Where() {
         free_where(cp_);
      }

      /**
       * @brief The condition, for the functions of the old interface: lSelect(), lFindFirstRW(), ...
       * @return the condition, or nullptr
       */
      [[nodiscard]] const lCondition *get() const noexcept {
         return cp_;
      }

      /**
       * @brief Give up ownership
       * @return the condition, which the caller now owns, or nullptr
       */
      [[nodiscard]] lCondition *release() noexcept {
         return std::exchange(cp_, nullptr);
      }

      /**
       * @brief Whether the handle holds a condition
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return cp_ != nullptr;
      }

      /**
       * @brief Both conditions, as `&&` in lWhere() and lAndWhere() combine them
       * @param a the first condition, taken over
       * @param b the second condition, taken over
       * @return the combination
       * @throw FieldException with reason NullElement if a condition is empty
       */
      friend Where operator&&(Where &&a, Where &&b) {
         return from_typed(make_logic(LogicOp::And, a.release(), b.release()));
      }

      /**
       * @brief Either condition, as `||` in lWhere() and lOrWhere() combine them
       * @param a the first condition, taken over
       * @param b the second condition, taken over
       * @return the combination
       * @throw FieldException with reason NullElement if a condition is empty
       */
      friend Where operator||(Where &&a, Where &&b) {
         return from_typed(make_logic(LogicOp::Or, a.release(), b.release()));
      }

      /**
       * @brief The negation, as `!` in lWhere() negates
       * @param a the condition, taken over
       * @return the negation
       * @throw FieldException with reason NullElement if the condition is empty
       */
      friend Where operator!(Where &&a) {
         return from_typed(make_logic(LogicOp::Not, a.release(), nullptr));
      }

   private:
      lCondition *cp_ = nullptr;   ///< the condition owned, or nullptr
   };

   /**
    * @brief A field in a condition; field<F>() gives it, its operators and named comparisons give a Where
    * @tparam F the field number
    */
   template <auto F>
      requires TypedField<decltype(F)>
   class Field {
   public:
      using Type = typename FieldTraits<F>::Type;               ///< the object type of the field
      using Arg = typename detail::CompareArg<F>::type;         ///< the value the field is compared with

      /**
       * @brief The field equals a value, as `==` in lWhere()
       * @param field the field
       * @param value the value; a string is copied
       * @return the condition
       */
      friend Where<Type> operator==(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::Equal, value));
      }

      /**
       * @brief The field differs from a value, as `!=` in lWhere(); a null string differs from nothing
       * @param field the field
       * @param value the value; a string is copied
       * @return the condition
       */
      friend Where<Type> operator!=(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::NotEqual, value));
      }

      /**
       * @brief The field is less than a value, as `<` in lWhere(); strings compare as strcmp() does
       * @param field the field
       * @param value the value
       * @return the condition
       */
      friend Where<Type> operator<(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::Lower, value));
      }

      /**
       * @brief The field is at most a value, as `<=` in lWhere()
       * @param field the field
       * @param value the value
       * @return the condition
       */
      friend Where<Type> operator<=(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::LowerEqual, value));
      }

      /**
       * @brief The field is greater than a value, as `>` in lWhere()
       * @param field the field
       * @param value the value
       * @return the condition
       */
      friend Where<Type> operator>(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::Greater, value));
      }

      /**
       * @brief The field is at least a value, as `>=` in lWhere()
       * @param field the field
       * @param value the value
       * @return the condition
       */
      friend Where<Type> operator>=(Field field, const Arg value) requires detail::Comparable<F> {
         static_cast<void>(field);
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::GreaterEqual, value));
      }

      /**
       * @brief The string or host field equals a value without regard to case, as `c=` in lWhere()
       * @param value the value; copied
       * @return the condition
       */
      [[nodiscard]] Where<Type> equals_nocase(const char *value) const requires detail::Textual<F> {
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::NoCase, value));
      }

      /**
       * @brief The string or host field matches a shell pattern, as `p=` in lWhere(); a null string matches as ""
       * @param pattern the pattern, as fnmatch() takes it; copied
       * @return the condition
       */
      [[nodiscard]] Where<Type> like(const char *pattern) const requires detail::Textual<F> {
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::Pattern, pattern));
      }

      /**
       * @brief The string or host field names the same host as a value, as `h=` in lWhere() and sge_hostcmp() compare
       * @param host the host name; copied
       * @return the condition
       */
      [[nodiscard]] Where<Type> same_host(const char *host) const requires detail::Textual<F> {
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::SameHost, host));
      }

      /**
       * @brief Every bit of a mask is set in the 32 bit field, as `m=` in lWhere()
       * @param mask the bits
       * @return the condition
       */
      [[nodiscard]] Where<Type> has_bits(const std::uint32_t mask) const
         requires(FieldTraits<F>::type == FieldType::Uint32) {
         return Where<Type>::from_typed(detail::make_field_compare<F>(CompareOp::Bitmask, mask));
      }

      /**
       * @brief An element of the list field fulfils a condition, as `->` in lWhere()
       * @tparam Sub the sub-type of the list field, as the model names it; not to be given
       * @param sub the condition on the sub-type, taken over; an empty one is fulfilled by any element
       * @return the condition, false for an element whose list field holds no list
       */
      template <typename Sub = typename FieldTraits<F>::Sub>
         requires(FieldTraits<F>::type == FieldType::List && FieldTraits<F>::has_sub &&
                  std::is_same_v<Sub, typename FieldTraits<F>::Sub>)
      [[nodiscard]] Where<Type> any(Where<Sub> &&sub) const {
         return Where<Type>::from_typed(make_subscope(Type::descriptor.e, static_cast<int>(F), sub.release()));
      }
   };

   /**
    * @brief A field to build a condition on: `field<JB_owner>() == "alice"`
    * @tparam F the field number
    * @return the field
    */
   template <auto F>
      requires TypedField<decltype(F)>
   [[nodiscard]] constexpr Field<F> field() noexcept {
      return {};
   }

   /**
    * @brief A selection of fields of a type, owned by the handle: the typed form of lWhat()
    *
    * Built with what<F...>(), what_all<T>() or what<T>(fields). Move-only,
    * like std::unique_ptr.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::US
    */
   template <ObjectType T>
   class What {
   public:
      using Type = T;   ///< the object type

      /** @brief An empty selection */
      constexpr What() noexcept = default;

      /**
       * @brief Take over a selection built for this type
       * @param ep the selection, or nullptr; freed by the handle
       * @return the handle
       */
      [[nodiscard]] static What from_typed(lEnumeration *ep) noexcept {
         What handle;
         handle.ep_ = ep;
         return handle;
      }

      What(const What &) = delete;
      What &operator=(const What &) = delete;

      /**
       * @brief Take over the selection of another handle
       * @param other the handle, empty afterwards
       */
      What(What &&other) noexcept : ep_(std::exchange(other.ep_, nullptr)) {}

      /**
       * @brief Free the selection held and take over the one of another handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      What &operator=(What &&other) noexcept {
         if (this != &other) {
            free_what(ep_);
            ep_ = std::exchange(other.ep_, nullptr);
         }
         return *this;
      }

      /** @brief Free the selection, as lFreeWhat() does */
      ~What() {
         free_what(ep_);
      }

      /**
       * @brief The selection, for the functions of the old interface: lSelect(), lCopyElemPartialPack(), ...
       * @return the selection, or nullptr
       */
      [[nodiscard]] const lEnumeration *get() const noexcept {
         return ep_;
      }

      /**
       * @brief Give up ownership
       * @return the selection, which the caller now owns, or nullptr
       */
      [[nodiscard]] lEnumeration *release() noexcept {
         return std::exchange(ep_, nullptr);
      }

      /**
       * @brief Whether the handle holds a selection
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

      /**
       * @brief Add a list or object field with a selection of the fields of its elements, as `%I->%T(...)` in lWhat()
       *
       * `what<US_name>().with<US_entries>(what<UE_name>())` selects the name of
       * a user set and the names of its entries.
       *
       * @tparam F a list or object field of this type whose sub-type the model names
       * @param sub the selection on the sub-type, taken over
       * @return the extended selection; this handle is empty afterwards
       * @throw FieldException with reason ForeignField if this selection is ALL, NONE or empty
       */
      template <auto F>
         requires detail::SelectFieldOf<F, T> && detail::Nestable<F>
      [[nodiscard]] What with(What<typename FieldTraits<F>::Sub> &&sub) && {
         if (ep_ == nullptr || ep_[0].pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::ForeignField, static_cast<int>(F), T::descriptor.e);
         }
         std::vector<WhatEntry> entries;
         for (int i = 0; ep_[i].nm != NoName; i++) {
            entries.push_back({ep_[i].nm, std::exchange(ep_[i].ep, nullptr)});
         }
         entries.push_back({static_cast<int>(F), sub.release()});
         free_what(ep_);
         return from_typed(make_what(T::descriptor.e, entries));
      }

   private:
      lEnumeration *ep_ = nullptr;   ///< the selection owned, or nullptr
   };

   /**
    * @brief A selection of fields given as template arguments, as lWhat("%T(%I%I)", ...) builds it
    *
    * The fields must all belong to one type, which is the type of the
    * selection. A list or object field selected here is taken whole; with()
    * adds one with a selection of its own.
    *
    * @tparam F the first field
    * @tparam Fs the further fields, of the same type
    * @return the selection
    * @throw ocs::RuntimeException if there is no memory
    */
   template <auto F, auto... Fs>
      requires TypedField<decltype(F)> && (detail::SelectFieldOf<Fs, typename FieldTraits<F>::Type> && ...)
   [[nodiscard]] What<typename FieldTraits<F>::Type> what() {
      const WhatEntry entries[] = {{static_cast<int>(F), nullptr}, {static_cast<int>(Fs), nullptr}...};
      return What<typename FieldTraits<F>::Type>::from_typed(make_what(FieldTraits<F>::Type::descriptor.e, entries));
   }

   /**
    * @brief A selection of fields known only at run time, checked against the type
    * @tparam T the struct of the object type
    * @param fields the field numbers
    * @return the selection
    * @throw FieldException with reason ForeignField for a field of another type or for no field at all
    * @throw ocs::RuntimeException if there is no memory
    */
   template <ObjectType T>
   [[nodiscard]] What<T> what(const std::span<const int> fields) {
      std::vector<WhatEntry> entries;
      entries.reserve(fields.size());
      for (const int nm : fields) {
         entries.push_back({nm, nullptr});
      }
      return What<T>::from_typed(make_what(T::descriptor.e, entries));
   }

   /**
    * @brief The selection of every field, as lWhat("%T(ALL)") and lWhatAll() build it
    * @tparam T the struct of the object type
    * @return the selection
    * @throw ocs::RuntimeException if there is no memory
    */
   template <ObjectType T>
   [[nodiscard]] What<T> what_all() {
      return What<T>::from_typed(make_what_special(WHAT_ALL));
   }

} // namespace ocs::tom
