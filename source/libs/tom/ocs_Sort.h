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
 * @brief Typed sort orders of the typed object model: asc<F>, desc<F>, SortOrder, sort_order<...>()
 *
 * A sort order names its fields as template arguments, each with its
 * direction:
 *
 * @code
 * jobs.sort<desc<JB_priority>, asc<JB_job_number>>();
 *
 * SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
 * jobs.insert_sorted(order, std::move(job));
 * @endcode
 *
 * A field of another type, a field that cannot be compared, or no field at
 * all does not compile. The list handles sort with these orders (ocs_List.h);
 * the core does the work (ocs_SortCore.h). The string form of lPSortList()
 * stays for existing code.
 */

#include <array>
#include <type_traits>
#include <utility>

#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_SortCore.h"

namespace ocs::tom {

   /**
    * @brief A sort criterion: the field @p F, ascending
    * @tparam F the field number
    */
   template <auto F>
   struct asc {};

   /**
    * @brief A sort criterion: the field @p F, descending
    * @tparam F the field number
    */
   template <auto F>
   struct desc {};

   namespace detail {

      /**
       * @brief The field and direction of a sort criterion; the primary template is no criterion
       * @tparam K the type to inspect
       */
      template <typename K>
      struct SortKeyTraits {
         static constexpr bool is_key = false;   ///< not a sort criterion
      };

      /**
       * @brief The field and direction of asc<F>
       * @tparam F the field number
       */
      template <auto F>
      struct SortKeyTraits<asc<F>> {
         static constexpr bool is_key = true;      ///< a sort criterion
         static constexpr auto field = F;          ///< the field
         static constexpr bool ascending = true;   ///< the direction
      };

      /**
       * @brief The field and direction of desc<F>
       * @tparam F the field number
       */
      template <auto F>
      struct SortKeyTraits<desc<F>> {
         static constexpr bool is_key = true;       ///< a sort criterion
         static constexpr auto field = F;           ///< the field
         static constexpr bool ascending = false;   ///< the direction
      };

      /**
       * @brief A field the core can sort by: a number, bool, string or host field
       * @tparam F the field number
       */
      template <auto F>
      concept Sortable = FieldTraits<F>::type == FieldType::Uint32 || FieldTraits<F>::type == FieldType::Uint64 ||
                         FieldTraits<F>::type == FieldType::Int32 || FieldTraits<F>::type == FieldType::Int64 ||
                         FieldTraits<F>::type == FieldType::Double || FieldTraits<F>::type == FieldType::Bool ||
                         FieldTraits<F>::type == FieldType::String || FieldTraits<F>::type == FieldType::Host;

      /**
       * @brief The field enumeration of a sort criterion, without the const of the constant holding it
       * @tparam K asc<F> or desc<F>
       */
      template <typename K>
      using SortKeyField = std::remove_cv_t<decltype(SortKeyTraits<K>::field)>;

      /**
       * @brief A sort criterion on a sortable field of the object type @p T
       * @tparam K asc<F> or desc<F>
       * @tparam T the struct of the object type
       */
      template <typename K, typename T>
      concept SortKeyOf = SortKeyTraits<K>::is_key && TypedField<SortKeyField<K>> &&
                          std::is_same_v<typename TypeOf<SortKeyField<K>>::type, T> &&
                          Sortable<SortKeyTraits<K>::field>;

      /**
       * @brief The object type of the field of a sort criterion
       * @tparam K asc<F> or desc<F>
       */
      template <typename K>
      using SortKeyType = typename TypeOf<SortKeyField<K>>::type;

   } // namespace detail

   /**
    * @brief An owning handle on a sort order of the object type @p T
    *
    * Built with sort_order<K...>(). Move-only, as the other owning handles;
    * get() hands the order to the functions of the old interface, lSortList()
    * and lInsertSorted().
    *
    * @tparam T the struct of the object type
    */
   template <ObjectType T>
   class SortOrder {
   public:
      using Type = T;   ///< the object type

      /** @brief An empty order */
      constexpr SortOrder() noexcept = default;

      /**
       * @brief Take over a sort order built for this type
       * @param so the order, or nullptr; freed by the handle
       * @return the handle
       */
      [[nodiscard]] static SortOrder from_typed(lSortOrder *so) noexcept {
         SortOrder handle;
         handle.so_ = so;
         return handle;
      }

      SortOrder(const SortOrder &) = delete;
      SortOrder &operator=(const SortOrder &) = delete;

      /**
       * @brief Take over the order of another handle
       * @param other the handle, empty afterwards
       */
      SortOrder(SortOrder &&other) noexcept : so_(std::exchange(other.so_, nullptr)) {}

      /**
       * @brief Free the order held and take over the one of another handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      SortOrder &operator=(SortOrder &&other) noexcept {
         if (this != &other) {
            free_sort_order(so_);
            so_ = std::exchange(other.so_, nullptr);
         }
         return *this;
      }

      /** @brief Free the order, as lFreeSortOrder() does */
      ~SortOrder() {
         free_sort_order(so_);
      }

      /**
       * @brief The order, for the functions of the old interface: lSortList(), lInsertSorted(), ...
       * @return the order, or nullptr
       */
      [[nodiscard]] const lSortOrder *get() const noexcept {
         return so_;
      }

      /**
       * @brief Whether the handle holds an order
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return so_ != nullptr;
      }

   private:
      lSortOrder *so_ = nullptr;   ///< the order, owned
   };

   /**
    * @brief Build a sort order from criteria known at compile time, as lParseSortOrderVarArg() builds one from a format
    *
    * `sort_order<desc<JB_priority>, asc<JB_job_number>>()` sorts by priority,
    * the highest first, and jobs of one priority by their number.
    *
    * @tparam K the first criterion, asc<F> or desc<F>; it names the object type
    * @tparam Ks the further criteria, on fields of the same type
    * @return the owning handle on the order
    * @throw ocs::RuntimeException if there is no memory
    * @see Old form: lParseSortOrderVarArg(), lCreateSortOrder(), lAddSortCriteria(); core: ocs::tom::make_sort_order()
    */
   template <typename K, typename... Ks>
      requires detail::SortKeyOf<K, detail::SortKeyType<K>> && (detail::SortKeyOf<Ks, detail::SortKeyType<K>> && ...)
   [[nodiscard]] SortOrder<detail::SortKeyType<K>> sort_order() {
      using T = detail::SortKeyType<K>;
      const std::array<SortKey, 1 + sizeof...(Ks)> keys{
         SortKey{static_cast<int>(detail::SortKeyTraits<K>::field), detail::SortKeyTraits<K>::ascending},
         SortKey{static_cast<int>(detail::SortKeyTraits<Ks>::field), detail::SortKeyTraits<Ks>::ascending}...};
      return SortOrder<T>::from_typed(make_sort_order(T::descriptor.e, keys));
   }

} // namespace ocs::tom
