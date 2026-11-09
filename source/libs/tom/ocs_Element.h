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
 * @brief Typed handles on elements of the typed object model: Element, ConstElement, ElementPtr
 *
 * A handle is a view on an `lListElem *` that knows the object type of the
 * element at compile time. It is the size of a pointer, owns nothing and
 * converts implicitly back to the pointer, so every function of the old
 * interface takes it unchanged. `get<F>()`, `set<F>()` and `has<F>()` take the
 * field number as template argument: a field of another type does not compile,
 * the position is a constant, the value type comes from the type's field table
 * and no type check runs. What is left at run time is the presence check on a
 * reduced element.
 *
 * Including this header also includes ocs_List.h, the handles on lists, which
 * the accessors of list fields return.
 */

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "cull/cull_lerrno.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldTraits.h"
#include "uti/ocs_Exception.h"

namespace ocs::tom {

   template <ObjectType T> class ConstElement;
   template <ObjectType T> class Element;
   template <ObjectType T> class ElementPtr;
   template <ObjectType T> class ConstList;
   template <ObjectType T> class List;
   template <ObjectType T> class ListPtr;

   /**
    * @brief A field number that belongs to an object type
    * @tparam F the field number
    * @tparam T the struct of the object type
    */
   template <auto F, typename T>
   concept FieldOf = TypedField<decltype(F)> && std::is_same_v<typename TypeOf<decltype(F)>::type, T>;

   namespace detail {

      /**
       * @brief The value slot of a field that the element holds
       * @param ep the element, not nullptr
       * @param pos the field's position
       * @return the slot
       */
      inline lMultiType &slot(const lListElem *ep, const int pos) {
         return ep->cont[pos + ep->descr[pos].off];
      }

      /**
       * @brief Throw the error of a type mismatch between a pointer and the type a handle expects
       * @tparam T the struct of the expected type
       * @param descr the descriptor of the element or list
       */
      template <ObjectType T>
      [[noreturn, gnu::cold]] void throw_wrong_type(const lDescr *descr) {
         throw_field_exception(FieldException::Reason::WrongType, NoName, descr, T::descriptor.e);
      }

      /**
       * @brief Check in a debug build that a list or object field holds elements of its declared sub-type
       *
       * A wrong declaration in the model would otherwise show only as reads
       * from wrong slots (N-H-3).
       *
       * @tparam Sub the struct of the declared sub-type
       * @param nm the field number
       * @param descr the descriptor of the sub-list or sub-object, or nullptr
       */
      template <ObjectType Sub>
      inline void check_sub_type([[maybe_unused]] const int nm, [[maybe_unused]] const lDescr *descr) {
#ifdef ENABLE_DEBUG_CHECKS
         if (descr != nullptr && descr[0].nm != Sub::fields[0].nm) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongSubType, nm, descr, Sub::descriptor.e);
         }
#endif
      }

      /**
       * @brief The position-plus-offset slot index of a field the element must hold
       *
       * Throws if the element does not hold the field: the only check left at
       * run time for a field known at compile time.
       *
       * @tparam F the field number
       * @param ep the element, not nullptr
       * @return the index into the element's values
       */
      template <auto F>
      inline int present_slot(const lListElem *ep) {
         constexpr int pos = FieldTraits<F>::pos;
         const int off = ep->descr[pos].off;
         if (off == OCS_CULL_ABSENT) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, F, ep->descr);
         }
         return pos + off;
      }

      /**
       * @brief Read a field the element holds, as the typed interface returns it
       *
       * @tparam F the field number
       * @tparam Mutable whether the value is read through a mutable handle; it
       *         decides between List and ConstList, Element and ConstElement
       * @param ep the element, not nullptr
       * @param s the slot index of the field
       * @return the value
       */
      template <auto F, bool Mutable>
      inline auto read(const lListElem *ep, const int s) {
         using Tr = FieldTraits<F>;
         const lMultiType &value = ep->cont[s];
         if constexpr (Tr::type == FieldType::Uint32) {
            return static_cast<std::uint32_t>(value.ul);
         } else if constexpr (Tr::type == FieldType::Uint64) {
            return static_cast<std::uint64_t>(value.ul64);
         } else if constexpr (Tr::type == FieldType::Int32) {
            return static_cast<std::int32_t>(value.i);
         } else if constexpr (Tr::type == FieldType::Int64) {
            return static_cast<std::int64_t>(value.l);
         } else if constexpr (Tr::type == FieldType::Double) {
            return static_cast<double>(value.db);
         } else if constexpr (Tr::type == FieldType::Bool) {
            return static_cast<bool>(value.b);
         } else if constexpr (Tr::type == FieldType::String) {
            return static_cast<const char *>(value.str);
         } else if constexpr (Tr::type == FieldType::Host) {
            return Host{value.host};
         } else if constexpr (Tr::type == FieldType::Ref) {
            return static_cast<void *>(value.ref);
         } else if constexpr (Tr::type == FieldType::List) {
            if constexpr (Tr::has_sub) {
               using Sub = typename Tr::Sub;
               check_sub_type<Sub>(F, value.glp != nullptr ? value.glp->descr : nullptr);
               if constexpr (Mutable) {
                  return List<Sub>::from_typed(value.glp);
               } else {
                  return ConstList<Sub>::from_typed(value.glp);
               }
            } else if constexpr (Mutable) {
               return value.glp;
            } else {
               return static_cast<const lList *>(value.glp);
            }
         } else {
            static_assert(Tr::type == FieldType::Object);
            if constexpr (Tr::has_sub) {
               using Sub = typename Tr::Sub;
               check_sub_type<Sub>(F, value.obj != nullptr ? value.obj->descr : nullptr);
               if constexpr (Mutable) {
                  return Element<Sub>::from_typed(value.obj);
               } else {
                  return ConstElement<Sub>::from_typed(value.obj);
               }
            } else if constexpr (Mutable) {
               return value.obj;
            } else {
               return static_cast<const lListElem *>(value.obj);
            }
         }
      }

      /**
       * @brief The type a setter of a field takes
       *
       * A plain value for numbers and strings, a Host for host names, the owning
       * handle of the sub-type for a typed list or object field, and the raw
       * pointer, as today, for a list or object field of any type.
       *
       * @tparam F the field number
       */
      template <auto F>
      struct SetterArg {
         using Tr = FieldTraits<F>;   ///< the field's traits
         /// the argument type
         using type = decltype([] {
            if constexpr (Tr::type == FieldType::List) {
               if constexpr (Tr::has_sub) {
                  return std::type_identity<ListPtr<typename Tr::Sub>>{};
               } else {
                  return std::type_identity<lList *>{};
               }
            } else if constexpr (Tr::type == FieldType::Object) {
               if constexpr (Tr::has_sub) {
                  return std::type_identity<ElementPtr<typename Tr::Sub>>{};
               } else {
                  return std::type_identity<lListElem *>{};
               }
            } else {
               return std::type_identity<typename ValueOf<Tr::type>::type>{};
            }
         }())::type;
      };

      /**
       * @brief Store a value into a field the element holds
       *
       * Numbers that are not hashed are stored inline. A hashed number goes
       * out of line only if it changes and the element's list has built the
       * table of the field. Strings, host names, lists and objects go out of
       * line.
       *
       * @tparam F the field number
       * @param ep the element, not nullptr
       * @param s the slot index of the field
       * @param value the value; a list or an element is taken over
       * @return false if a string could not be copied or an element to store is bound elsewhere
       */
      template <auto F, typename V>
      inline bool write(lListElem *ep, const int s, V &&value) {
         using Tr = FieldTraits<F>;
         constexpr int pos = Tr::pos;
         lMultiType &slot_value = ep->cont[s];
         if constexpr (Tr::type == FieldType::String || Tr::type == FieldType::Host) {
            return store_string(ep, pos, static_cast<const char *>(value));
         } else if constexpr (Tr::type == FieldType::List) {
            if constexpr (Tr::has_sub) {
               store_list(ep, pos, value.release());
            } else {
               store_list(ep, pos, value);
            }
            return true;
         } else if constexpr (Tr::type == FieldType::Object) {
            if constexpr (Tr::has_sub) {
               if (!store_object(ep, pos, value.get())) {
                  return false;
               }
               static_cast<void>(value.release());
               return true;
            } else {
               return store_object(ep, pos, value);
            }
         } else {
            observe_value(ep, pos);
            if constexpr (Tr::hashed && (Tr::type == FieldType::Uint32 || Tr::type == FieldType::Uint64)) {
               if (elem_hash_table(ep, pos) != nullptr) {
                  store_hashed_number(ep, pos, static_cast<std::uint64_t>(value));
                  return true;
               }
            }
            if constexpr (Tr::type == FieldType::Uint32) {
               slot_value.ul = value;
            } else if constexpr (Tr::type == FieldType::Uint64) {
               slot_value.ul64 = value;
            } else if constexpr (Tr::type == FieldType::Int32) {
               slot_value.i = value;
            } else if constexpr (Tr::type == FieldType::Int64) {
               slot_value.l = value;
            } else if constexpr (Tr::type == FieldType::Double) {
               slot_value.db = value;
            } else if constexpr (Tr::type == FieldType::Bool) {
               slot_value.b = value;
            } else {
               static_assert(Tr::type == FieldType::Ref);
               slot_value.ref = value;
            }
            return true;
         }
      }

      /**
       * @brief The field type that a value type of the run-time accessors reads and writes
       * @tparam V the value type
       * @return the field type, or a field type that never matches for an unsupported value type
       */
      template <typename V>
      consteval FieldType field_type_for() {
         if constexpr (std::is_same_v<V, std::uint32_t>) {
            return FieldType::Uint32;
         } else if constexpr (std::is_same_v<V, std::uint64_t>) {
            return FieldType::Uint64;
         } else if constexpr (std::is_same_v<V, std::int32_t>) {
            return FieldType::Int32;
         } else if constexpr (std::is_same_v<V, std::int64_t>) {
            return FieldType::Int64;
         } else if constexpr (std::is_same_v<V, double>) {
            return FieldType::Double;
         } else if constexpr (std::is_same_v<V, bool>) {
            return FieldType::Bool;
         } else if constexpr (std::is_same_v<V, const char *>) {
            return FieldType::String;
         } else if constexpr (std::is_same_v<V, Host>) {
            return FieldType::Host;
         } else {
            static_assert(std::is_same_v<V, void *>, "the run-time accessors read and write plain values only");
            return FieldType::Ref;
         }
      }

      /**
       * @brief The slot index of a field given at run time, checked as the old interface checks it
       * @tparam V the value type the caller reads or writes
       * @param ep the element, not nullptr
       * @param nm the field number
       * @return the index into the element's values
       */
      template <typename V>
      inline int runtime_slot(const lListElem *ep, const int nm) {
         const int pos = field_pos(ep->descr, nm);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::ForeignField, nm, ep->descr);
         }
         if (field_type_of(ep->descr[pos].mt) != field_type_for<V>()) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongType, nm, ep->descr, ep->descr);
         }
         const int off = ep->descr[pos].off;
         if (off == OCS_CULL_ABSENT) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, nm, ep->descr);
         }
         return pos + off;
      }

   } // namespace detail

   /**
    * @brief A read-only typed handle on an element
    *
    * Converts implicitly to `const lListElem *`, made from one only explicitly
    * and with a check of the type. Tests as false when empty; any field access
    * through an empty handle throws.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class ConstElement {
   public:
      using Type = T;   ///< the object type

      /** @brief An empty handle */
      constexpr ConstElement() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr ConstElement(std::nullptr_t) noexcept {}

      /**
       * @brief Take an element whose type is known only at run time, and check it
       * @param ep the element, or nullptr for an empty handle
       * @throw FieldException if the element is of another type
       */
      explicit ConstElement(const lListElem *ep) : ep_(const_cast<lListElem *>(ep)) {
         if (ep != nullptr && ep->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            detail::throw_wrong_type<T>(ep->descr);
         }
      }

      /**
       * @brief Take an element from a source whose type is known, without a check
       * @param ep the element, or nullptr
       * @return the handle
       */
      static ConstElement from_typed(const lListElem *ep) noexcept {
         ConstElement handle;
         handle.ep_ = const_cast<lListElem *>(ep);
         return handle;
      }

      /**
       * @brief The element, for every function of the old interface
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
       * @brief Whether the handle holds an element
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

      /**
       * @brief Whether two handles hold the same element
       * @param a the first handle
       * @param b the second handle
       * @return true if both hold the same element or both are empty
       */
      friend bool operator==(const ConstElement &a, const ConstElement &b) noexcept {
         return a.ep_ == b.ep_;
      }

      /**
       * @brief Whether a handle is empty
       * @param a the handle
       * @return true if it is empty
       */
      friend bool operator==(const ConstElement &a, std::nullptr_t) noexcept {
         return a.ep_ == nullptr;
      }

      /**
       * @brief Read a field
       * @tparam F the field number, of this object type
       * @return the value, as the type's field table declares it
       * @throw FieldException if the handle is empty or the reduced element does not hold the field
       */
      template <auto F>
         requires FieldOf<F, T>
      [[nodiscard]] auto get() const {
         return detail::read<F, false>(ep_, present_slot<F>());
      }

      /**
       * @brief Read a field given only at run time, checked as the old interface checks it
       * @tparam V the value type, which must match the field's type
       * @param nm the field number
       * @return the value
       * @throw FieldException if the handle is empty, or the field is foreign, of another type or absent
       */
      template <typename V>
      [[nodiscard]] V get(const int nm) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, nm, T::descriptor.e);
         }
         const lMultiType &value = ep_->cont[detail::runtime_slot<V>(ep_, nm)];
         constexpr FieldType type = detail::field_type_for<V>();
         if constexpr (type == FieldType::Uint32) {
            return value.ul;
         } else if constexpr (type == FieldType::Uint64) {
            return value.ul64;
         } else if constexpr (type == FieldType::Int32) {
            return value.i;
         } else if constexpr (type == FieldType::Int64) {
            return value.l;
         } else if constexpr (type == FieldType::Double) {
            return value.db;
         } else if constexpr (type == FieldType::Bool) {
            return value.b;
         } else if constexpr (type == FieldType::String) {
            return value.str;
         } else if constexpr (type == FieldType::Host) {
            return Host{value.host};
         } else {
            return value.ref;
         }
      }

      /**
       * @brief Whether the element holds a field; a full element holds every field of its type
       * @tparam F the field number, of this object type
       * @return true if the field is present
       * @throw FieldException if the handle is empty
       */
      template <auto F>
         requires FieldOf<F, T>
      [[nodiscard]] bool has() const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, F, T::descriptor.e);
         }
         return ep_->descr[FieldTraits<F>::pos].off != OCS_CULL_ABSENT;
      }

   protected:
      lListElem *ep_ = nullptr;   ///< the element, or nullptr; const unless reached through Element

      /**
       * @brief The slot index of a field the element holds, checking the handle
       * @tparam F the field number
       * @return the index into the element's values
       */
      template <auto F>
      [[nodiscard]] int present_slot() const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, F, T::descriptor.e);
         }
         return detail::present_slot<F>(ep_);
      }
   };

   /**
    * @brief A typed handle on an element that may be changed
    *
    * Adds the setters and the conversion to `lListElem *`. Derives from
    * ConstElement, so it converts to its read-only counterpart, never the
    * other way round.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class Element : public ConstElement<T> {
   public:
      /** @brief An empty handle */
      constexpr Element() noexcept = default;

      /** @brief An empty handle, from nullptr */
      constexpr Element(std::nullptr_t) noexcept {}

      /**
       * @brief Take an element whose type is known only at run time, and check it
       * @param ep the element, or nullptr for an empty handle
       * @throw FieldException if the element is of another type
       */
      explicit Element(lListElem *ep) : ConstElement<T>(ep) {}

      /**
       * @brief Take an element from a source whose type is known, without a check
       * @param ep the element, or nullptr
       * @return the handle
       */
      static Element from_typed(lListElem *ep) noexcept {
         Element handle;
         handle.ep_ = ep;
         return handle;
      }

      /**
       * @brief The element, for every function of the old interface
       * @return the element, or nullptr
       */
      operator lListElem *() const noexcept {
         return this->ep_;
      }

      /**
       * @brief The element, for varargs and deduced templates
       * @return the element, or nullptr
       */
      [[nodiscard]] lListElem *raw() const noexcept {
         return this->ep_;
      }

      /**
       * @brief Read a field given only at run time, checked as the old interface checks it
       *
       * Declared here, because the template accessor below hides every get() of the base.
       *
       * @tparam V the value type, which must match the field's type
       * @param nm the field number
       * @return the value
       * @throw FieldException if the handle is empty, or the field is foreign, of another type or absent
       */
      template <typename V>
      [[nodiscard]] V get(const int nm) const {
         return ConstElement<T>::template get<V>(nm);
      }

      /**
       * @brief Read a field; list and object fields give mutable handles
       * @tparam F the field number, of this object type
       * @return the value, as the type's field table declares it
       * @throw FieldException if the handle is empty or the reduced element does not hold the field
       */
      template <auto F>
         requires FieldOf<F, T>
      [[nodiscard]] auto get() const {
         return detail::read<F, true>(this->ep_, this->template present_slot<F>());
      }

      /**
       * @brief Set a field, as the old setter of its field type does
       *
       * Strings are copied and the old value freed, the hash table of the
       * owning list is maintained, a list or an element is taken over and the
       * one held before freed.
       *
       * @tparam F the field number, of this object type
       * @param value the value; an owning handle is moved in
       * @throw FieldException if the handle is empty, the reduced element does not
       *        hold the field, a string cannot be copied or an element to store
       *        is bound elsewhere
       */
      template <auto F>
         requires FieldOf<F, T>
      void set(typename detail::SetterArg<F>::type value) const {
         if (!detail::write<F>(this->ep_, this->template present_slot<F>(), std::move(value))) [[unlikely]] {
            throw_field_exception(FieldTraits<F>::type == FieldType::Object ? FieldException::Reason::WrongType
                                                                       : FieldException::Reason::OutOfMemory,
                              F, this->ep_->descr);
         }
      }

      /**
       * @brief Set a field given only at run time, checked as the old interface checks it
       *
       * For plain values only: numbers, bool, strings, host names and references.
       *
       * @tparam V the value type, which must match the field's type
       * @param nm the field number
       * @param value the value
       * @throw FieldException if the handle is empty, or the field is foreign, of another type or absent,
       *        or a string cannot be copied
       */
      template <typename V>
      void set(const int nm, const V value) const {
         if (this->ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, nm, T::descriptor.e);
         }
         const int s = detail::runtime_slot<V>(this->ep_, nm);
         const int pos = field_pos(this->ep_->descr, nm);
         constexpr FieldType type = detail::field_type_for<V>();
         lMultiType &slot_value = this->ep_->cont[s];
         if constexpr (type == FieldType::String || type == FieldType::Host) {
            if (!store_string(this->ep_, pos, static_cast<const char *>(value))) [[unlikely]] {
               throw_field_exception(FieldException::Reason::OutOfMemory, nm, this->ep_->descr);
            }
         } else if constexpr (type == FieldType::Uint32 || type == FieldType::Uint64) {
            observe_value(this->ep_, pos);
            if (elem_hash_table(this->ep_, pos) != nullptr) {
               store_hashed_number(this->ep_, pos, value);
            } else if constexpr (type == FieldType::Uint32) {
               slot_value.ul = value;
            } else {
               slot_value.ul64 = value;
            }
         } else {
            observe_value(this->ep_, pos);
            if constexpr (type == FieldType::Int32) {
               slot_value.i = value;
            } else if constexpr (type == FieldType::Int64) {
               slot_value.l = value;
            } else if constexpr (type == FieldType::Double) {
               slot_value.db = value;
            } else if constexpr (type == FieldType::Bool) {
               slot_value.b = value;
            } else {
               slot_value.ref = value;
            }
         }
      }
   };

   /**
    * @brief An element owned by the handle, freed when the handle goes out of scope
    *
    * Move-only, like `std::unique_ptr`. A function that takes an element over
    * takes its owning handle by rvalue reference, so the transfer shows at the
    * call.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::JB
    */
   template <ObjectType T>
   class ElementPtr {
   public:
      /** @brief An empty owning handle */
      constexpr ElementPtr() noexcept = default;

      /** @brief An empty owning handle, from nullptr */
      constexpr ElementPtr(std::nullptr_t) noexcept {}

      /**
       * @brief Take over an element whose type is known only at run time, and check it
       * @param ep the element, or nullptr; freed by the handle
       * @throw FieldException if the element is of another type; the element is not taken over then
       */
      explicit ElementPtr(lListElem *ep) : ep_(Element<T>(ep).raw()) {}

      /**
       * @brief Create an element of the type, with every field at its default
       * @return the owning handle
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create() {
         ElementPtr handle;
         handle.ep_ = lCreateElem(T::descriptor.e);
         if (handle.ep_ == nullptr) [[unlikely]] {
            throw ocs::RuntimeException("out of memory creating an element");
         }
         return handle;
      }

      ElementPtr(const ElementPtr &) = delete;
      ElementPtr &operator=(const ElementPtr &) = delete;

      /**
       * @brief Take over the element of another owning handle
       * @param other the handle, empty afterwards
       */
      ElementPtr(ElementPtr &&other) noexcept : ep_(std::exchange(other.ep_, nullptr)) {}

      /**
       * @brief Free the element held and take over the one of another owning handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      ElementPtr &operator=(ElementPtr &&other) noexcept {
         if (this != &other) {
            lFreeElem(&ep_);
            ep_ = std::exchange(other.ep_, nullptr);
         }
         return *this;
      }

      /** @brief Free the element held */
      ~ElementPtr() {
         lFreeElem(&ep_);
      }

      /**
       * @brief A non-owning handle on the element
       * @return the handle, empty if this one is
       */
      [[nodiscard]] Element<T> get() const noexcept {
         return Element<T>::from_typed(ep_);
      }

      /**
       * @brief Give up ownership
       * @return the element, which the caller now owns, or nullptr
       */
      [[nodiscard]] lListElem *release() noexcept {
         return std::exchange(ep_, nullptr);
      }

      /**
       * @brief Whether the handle holds an element
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

   private:
      lListElem *ep_ = nullptr;   ///< the element owned, or nullptr
   };

   /**
    * @brief Read a field of an element whose type is known only at run time
    *
    * The free form for code that still holds a raw pointer: the field's type
    * comes from the field table, and one comparison checks at run time that
    * the element is of the field's type. An error throws a logic error, which
    * unconverted code does not catch, so it ends in an abort as the old getter
    * did.
    *
    * @tparam F the field number
    * @param ep the element
    * @return the value, as the field table declares it
    * @throw FieldException if the element is nullptr, of another type or does not hold the field
    */
   template <auto F>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(const lListElem *ep) {
      using Type = typename FieldTraits<F>::Type;
      if (ep == nullptr) [[unlikely]] {
         throw_field_exception(FieldException::Reason::NullElement, F, nullptr);
      }
      if (ep->descr[0].nm != Type::fields[0].nm) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignField, F, ep->descr);
      }
      return detail::read<F, false>(ep, detail::present_slot<F>(ep));
   }

   /**
    * @brief Read a field of a mutable element whose type is known only at run time
    * @tparam F the field number
    * @param ep the element
    * @return the value; list and object fields give mutable handles or pointers
    * @throw FieldException if the element is nullptr, of another type or does not hold the field
    */
   template <auto F>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(lListElem *ep) {
      using Type = typename FieldTraits<F>::Type;
      if (ep == nullptr) [[unlikely]] {
         throw_field_exception(FieldException::Reason::NullElement, F, nullptr);
      }
      if (ep->descr[0].nm != Type::fields[0].nm) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignField, F, ep->descr);
      }
      return detail::read<F, true>(ep, detail::present_slot<F>(ep));
   }

   namespace detail {

      /**
       * @brief The result of the free setter when the element does not hold the field, as the old setter had it
       *
       * The old setter of a string or host field aborted, so this throws; the
       * old setter of a number, list or object field returned -1 without an
       * error number; the others returned -1 with LENEGPOS.
       *
       * @tparam F the field number
       * @param ep the element
       * @param reason why the element does not hold the field
       * @return -1
       */
      template <auto F>
      int missing_field_result(const lListElem *ep, const FieldException::Reason reason) {
         constexpr FieldType type = FieldTraits<F>::type;
         if constexpr (type == FieldType::String || type == FieldType::Host) {
            throw_field_exception(reason, F, ep->descr);
         } else if constexpr (type != FieldType::Uint32 && type != FieldType::Uint64 && type != FieldType::List &&
                              type != FieldType::Object) {
            set_lerrno(LENEGPOS);
         }
         return -1;
      }

   } // namespace detail

   /**
    * @brief Set a field of an element whose type is known only at run time, with the error behaviour of the old setter
    *
    * The free form that replaces the old setter in the mechanical rewrite. It
    * returns -1 where the old setter of its field type returned -1, with the
    * same error number, and throws a logic error where that aborted.
    *
    * @tparam F the field number
    * @param ep the element
    * @param value the value; a list or an element is taken over, as by the old setter
    * @return 0 on success, -1 on an error the old setter returned
    * @throw FieldException where the old setter aborted
    */
   template <auto F, typename V>
      requires TypedField<decltype(F)>
   int set(lListElem *ep, V &&value) {
      using Tr = FieldTraits<F>;
      using Type = typename Tr::Type;
      if (ep == nullptr) [[unlikely]] {
         set_lerrno(LEELEMNULL);
         return -1;
      }
      if (ep->descr[0].nm != Type::fields[0].nm) [[unlikely]] {
         return detail::missing_field_result<F>(ep, FieldException::Reason::ForeignField);
      }
      const int off = ep->descr[Tr::pos].off;
      if (off == OCS_CULL_ABSENT) [[unlikely]] {
         return detail::missing_field_result<F>(ep, FieldException::Reason::AbsentField);
      }
      // a typed list or object field takes the raw pointer here, as the old setter did
      if constexpr (Tr::type == FieldType::List) {
         store_list(ep, Tr::pos, static_cast<lList *>(value));
         return 0;
      } else if constexpr (Tr::type == FieldType::Object) {
         if (!store_object(ep, Tr::pos, static_cast<lListElem *>(value))) {
            set_lerrno(LEBOUNDELEM);
            return -1;
         }
         return 0;
      } else {
         if (!detail::write<F>(ep, Tr::pos + off, static_cast<typename ValueOf<Tr::type>::type>(value))) {
            set_lerrno(LESTRDUP);
            return -1;
         }
         return 0;
      }
   }

} // namespace ocs::tom

#include "tom/ocs_List.h"
