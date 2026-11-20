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
#include <cstdio>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "tom/cull_lerrno.h"
#include "tom/cull_listP.h"
#include "tom/cull_multitypeP.h"
#include "tom/ocs_cull_descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_DumpCore.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SoftPath.h"
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
       * @brief What operator-> of an owning handle returns: holds the non-owning handle for the full expression
       *
       * operator-> must end in a pointer; the owning handles have no
       * non-owning handle stored that it could point to, so they return this
       * object, whose own operator-> points to the handle it holds. The
       * object lives until the end of the full expression, which covers the
       * call made through it.
       *
       * @tparam H the non-owning handle, Element<T> or List<T>
       */
      template <typename H>
      class Arrow {
      public:
         /**
          * @brief Hold a handle
          * @param handle the non-owning handle
          */
         explicit Arrow(const H handle) noexcept : handle_(handle) {}

         /**
          * @brief The handle the call goes to
          * @return a pointer to the held handle, valid until the end of the full expression
          */
         const H *operator->() const noexcept {
            return &handle_;
         }

      private:
         H handle_;   ///< the non-owning handle
      };

      /**
       * @brief A field an offset can be added to, as lAddUlong(), lAddUlong64() and lAddDouble() add: 32 bit, 64 bit, double
       * @tparam F the field number
       */
      template <auto F>
      concept Addable = FieldTraits<F>::type == FieldType::Uint32 || FieldTraits<F>::type == FieldType::Uint64 ||
                        FieldTraits<F>::type == FieldType::Double;

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
       * @brief Check that an element holds a field known at compile time
       *
       * One condition for every error and one out-of-line call for all of
       * them, so the error path at each call site stays a single jump: a null
       * element, an element of another type, a reduced element without the field.
       *
       * For an element of a handle the type is known at compile time, so the
       * comparison with the field's type is left out (@p Typed).
       *
       * @tparam F the field number
       * @tparam Typed true if the element comes from a handle of the field's type
       * @param ep the element, or nullptr
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed = false>
      inline void check_holds(const lListElem *ep) {
         using Tr = FieldTraits<F>;
         const bool foreign = !Typed && ep != nullptr && ep->descr[0].nm != Tr::Type::fields[0].nm;
         if (ep == nullptr || foreign || ep->descr[Tr::pos].off == OCS_CULL_ABSENT) [[unlikely]] {
            throw_access_error(ep, F, Tr::Type::fields[0].nm);
         }
      }

      /**
       * @brief Stop the compilation if a field does not belong to the type of a handle
       *
       * The free forms taking a handle have no run-time check of the type:
       * a field of another type is a compile error instead (L2).
       *
       * @tparam F the field number
       * @tparam T the object type of the handle
       */
      template <auto F, typename T>
      constexpr void assert_field_of() {
         static_assert(std::is_same_v<T, typename FieldTraits<F>::Type>,
                       "the field does not belong to the object type of the handle");
      }

      /**
       * @brief The position-plus-offset slot index of a field known at compile time, checked by check_holds()
       *
       * @tparam F the field number
       * @tparam Typed true if the element comes from a handle of the field's type, see check_holds()
       * @param ep the element, or nullptr
       * @return the index into the element's values
       * @throw FieldException with reason NullElement, ForeignField or AbsentField
       */
      template <auto F, bool Typed = false>
      [[nodiscard]] inline int checked_slot(const lListElem *ep) {
         check_holds<F, Typed>(ep);
         constexpr int pos = FieldTraits<F>::pos;
         return pos + ep->descr[pos].off;
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
            if constexpr (Tr::hashed && (Tr::type == FieldType::Uint32 || Tr::type == FieldType::Uint64)) {
               if (needs_hashed_store(ep, pos)) {
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
       * @brief The value of a slot as a plain value type, for a field whose type was checked against it
       * @tparam V the value type, one of the nine plain types of #field_type_for
       * @param value the slot of a field of the field type #field_type_for gives for @p V
       * @return the value
       */
      template <typename V>
      [[nodiscard]] inline V plain_value(const lMultiType &value) noexcept {
         constexpr FieldType type = field_type_for<V>();
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
       * @brief Read a plain field given at run time, checked as the old getters by name (lGetUlong(), ...) check it
       *
       * The one implementation behind the run-time member ConstElement<T>::get<V>(int) and the free accessor
       * ocs::tom::get<V>(const lListElem *, int) of ocs_Raw.h, so both give the same value and the same reason.
       * The checks are those of #checked_pos, in its order.
       *
       * @tparam V the value type, one of the nine plain types of #field_type_for
       * @param ep the element, or nullptr
       * @param nm the field number
       * @return the value
       * @throw FieldException with reason NullElement, ForeignField, AbsentField or WrongType
       */
      template <typename V>
      [[nodiscard]] inline V runtime_get(const lListElem *ep, const int nm) {
         return plain_value<V>(slot(ep, checked_pos(ep, nm, field_type_for<V>())));
      }

      /**
       * @brief Write a plain field given at run time, checked as the old setters by name (lSetUlong(), ...) check it
       *
       * The one implementation behind the run-time member Element<T>::set(int, V) and the free accessor
       * ocs::tom::set(lListElem *, int, V) of ocs_Raw.h. Strings and host names are copied and the old value
       * freed; the hash table of the owning list is maintained.
       *
       * @tparam V the value type, one of the nine plain types of #field_type_for
       * @param ep the element, or nullptr
       * @param nm the field number
       * @param value the value
       * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType, OutOfMemory if a
       *        string cannot be copied, or in a debug build DuplicateKey when another element of the list holds
       *        the value of a unique field
       */
      template <typename V>
      inline void runtime_set(lListElem *ep, const int nm, const V value) {
         constexpr FieldType type = field_type_for<V>();
         const int pos = checked_pos(ep, nm, type);
         lMultiType &slot_value = slot(ep, pos);
         if constexpr (type == FieldType::String || type == FieldType::Host) {
            if (!store_string(ep, pos, static_cast<const char *>(value))) [[unlikely]] {
               throw_field_exception(FieldException::Reason::OutOfMemory, nm, ep->descr);
            }
         } else if constexpr (type == FieldType::Uint32 || type == FieldType::Uint64) {
            if (needs_hashed_store(ep, pos)) {
               store_hashed_number(ep, pos, value);
            } else if constexpr (type == FieldType::Uint32) {
               slot_value.ul = value;
            } else {
               slot_value.ul64 = value;
            }
         } else if constexpr (type == FieldType::Int32) {
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

   } // namespace detail

   /** @brief Whether a new list keeps hash tables for its hashed fields */
   enum class Hashing : bool {
      Off,   ///< no hash tables, as lCreateListHash() with false; lookups search linearly
      On     ///< hash tables, built when the list grows beyond the threshold, as lCreateList()
   };

   /**
    * @brief The layout of the elements of a type reduced to some of its fields: the typed form of a reduced
    *        descriptor
    *
    * Made by layout<F...>() and taken by ElementPtr<T>::create() and
    * ListPtr<T>::create(). The descriptor behind it lives as long as the
    * process, as the descriptors of the types do, so the handle owns nothing
    * and is copied freely.
    *
    * @tparam T the struct of the object type, e.g. ocs::tom::JAT
    */
   template <ObjectType T>
   class Layout {
   public:
      using Type = T;   ///< the object type

      /**
       * @brief Take a descriptor of the type that lives as long as the process, without a check
       * @param descr the descriptor, the full one of the type or a reduction of it
       * @return the handle
       */
      [[nodiscard]] static constexpr Layout from_typed(const lDescr *descr) noexcept {
         Layout handle;
         handle.descr_ = descr;
         return handle;
      }

      /**
       * @brief The descriptor, as an opaque token for the functions of the old interface
       * @return the descriptor, or nullptr for a default-constructed handle
       */
      [[nodiscard]] const lDescr *get() const noexcept {
         return descr_;
      }

   private:
      const lDescr *descr_ = nullptr;   ///< the descriptor, not owned
   };

   /**
    * @brief The layout of a type reduced to the fields given as template arguments, as
    *        ocs_cull_descr_reduce() builds it
    *
    * Built on the first call for each set of fields and kept for the life of
    * the process, so a call on a hot path costs no allocation: the typed form
    * of `static const lDescr *const d = ocs_cull_descr_reduce(T_Type, {...})`.
    * The fields may be given in any order; the layout keeps the full length of
    * the type with the fields not given marked absent, so the checked
    * constructors of the handles accept elements and lists of it.
    *
    * The reduction is built once, by the initialisation of a function-local
    * static. A first build that fails for lack of memory is not retried: the
    * static stays empty for the life of the process, and every later call for
    * the same set of fields throws as well. A caller that must survive this
    * catches the exception on every call, not only on the first.
    *
    * @tparam F the first field
    * @tparam Fs the further fields, of the same type
    * @return the layout
    * @throw ListCoreError with LEMALLOC if there is no memory to build it, on the first call and on every later
    *        call once the first build failed
    * @see Old form: ocs_cull_descr_reduce(), lReduceDescr()
    */
   template <auto F, auto... Fs>
      requires TypedField<decltype(F)> && (detail::SelectFieldOf<Fs, typename FieldTraits<F>::Type> && ...)
   [[nodiscard]] Layout<typename FieldTraits<F>::Type> layout() {
      using T = typename FieldTraits<F>::Type;
      // built once per set of fields and never released, like the descriptor blocks of the types
      static const lDescr *const descr =
              ocs_cull_descr_reduce(T::descriptor.e, {static_cast<int>(F), static_cast<int>(Fs)...});
      if (descr == nullptr) [[unlikely]] {
         throw ListCoreError(LEMALLOC, "out of memory reducing the descriptor of a layout");
      }
      return Layout<T>::from_typed(descr);
   }

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
       * @brief The layout of the element, as lGetElemDescr() gives it
       *
       * An opaque token: the full descriptor of the type or a reduction of it.
       * To make a list or element of the same layout, ListPtr<T>::create_like()
       * and ElementPtr<T>::create_like() take the handle itself.
       *
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
       * @brief Read a field, as lGetUlong(), lGetString(), lGetList() and the other getters by name do
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
       * @brief Read a field given only at run time, checked as the old getters by name (lGetUlong(), ...) check it
       *
       * Runs on the core of the free accessor ocs::tom::get<V>(const lListElem *, int) (detail::runtime_get()),
       * so both give the same value and the same reason; only an empty handle is reported as EmptyHandle here.
       *
       * @tparam V the value type, which must match the field's type
       * @param nm the field number
       * @return the value
       * @throw FieldException with reason EmptyHandle if the handle is empty, otherwise ForeignField,
       *        AbsentField or WrongType, in this order
       */
      template <typename V>
      [[nodiscard]] V get(const int nm) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, nm, T::descriptor.e);
         }
         return detail::runtime_get<V>(ep_, nm);
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

      /**
       * @brief The next element of the list, as lNext() gives it; std::next() does this for an iterator
       * @return the element, or an empty handle at the end of the list or for an empty handle
       */
      [[nodiscard]] ConstElement next() const noexcept {
         return from_typed(ep_ != nullptr ? ep_->next : nullptr);
      }

      /**
       * @brief The previous element of the list, as lPrev() gives it; std::prev() does this for an iterator
       * @return the element, or an empty handle at the front of the list or for an empty handle
       */
      [[nodiscard]] ConstElement prev() const noexcept {
         return from_typed(ep_ != nullptr ? ep_->prev : nullptr);
      }

      /**
       * @brief Whether the element fulfils a condition, as lCompare() decides it
       * @param where the condition; an empty one is fulfilled by every element
       * @return true if it is fulfilled
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @see Old form: lCompare(); core: ocs::tom::compare()
       */
      [[nodiscard]] bool matches(const Where<T> &where) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         return compare(ep_, where.get());
      }

      /**
       * @brief The next element of the list that fulfils a condition, as lFindNextRW() finds it
       * @param where the condition; an empty one takes the next element
       * @return the element, or an empty handle if none follows, or for an empty handle
       * @see Old form: lFindNextRW(); core: ocs::tom::find_next()
       */
      [[nodiscard]] ConstElement next_if(const Where<T> &where) const {
         return from_typed(ep_ != nullptr ? find_next(ep_, where.get()) : nullptr);
      }

      /**
       * @brief The previous element of the list that fulfils a condition, as lFindPrevRW() finds it
       * @param where the condition; an empty one takes the previous element
       * @return the element, or an empty handle if none precedes, or for an empty handle
       * @see Old form: lFindPrevRW(); core: ocs::tom::find_prev()
       */
      [[nodiscard]] ConstElement prev_if(const Where<T> &where) const {
         return from_typed(ep_ != nullptr ? find_prev(ep_, where.get()) : nullptr);
      }

      /**
       * @brief A deep copy of the element, as lCopyElem() makes it
       *
       * Sub-lists and sub-objects are copied as well, and a reduced element
       * stays reduced. The owning handles are move-only like std::unique_ptr,
       * so a copy is always this explicit call.
       *
       * @return the owning handle on the copy, empty if this handle is empty, as lCopyElem(nullptr) gives nullptr
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] ElementPtr<T> clone() const {
         if (ep_ == nullptr) {
            return {};
         }
         return ElementPtr<T>::from_typed(copy_elem(ep_, true, false));
      }

      /**
       * @brief A copy of the element reduced to a selection of fields, as lSelectElemPack() and
       *        lSelectElemDPack() without a pack buffer make it
       *
       * The copy has the layout the selection reduces the layout of this
       * element to; the selection caches that reduction, so selecting from many
       * elements of one layout builds it once, as the descriptor passed to
       * lSelectElemDPack() did. The packing variants of the old forms (a pack
       * buffer instead of a copy) are pack(pb, what).
       *
       * @param what the selection of fields
       * @param hashing whether copied sub-lists keep hash tables, the isHash argument of the old forms
       * @return the owning handle on the copy, a free element; empty if this handle is empty, as the old forms
       *         give nullptr for a null element
       * @throw FieldException with reason NullElement if @p what is empty
       * @throw ListCoreError if this element does not hold a selected field or a copy fails, where the old forms
       *        gave nullptr
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lSelectElemPack(), lSelectElemDPack() with lReduceDescr(); core: ocs::tom::select_elem()
       */
      [[nodiscard]] ElementPtr<T> select(const What<T> &what, const Hashing hashing = Hashing::On) const {
         if (ep_ == nullptr) {
            return {};
         }
         return ElementPtr<T>::from_typed(select_elem(ep_, what.get(), hashing == Hashing::On, false));
      }

      /**
       * @brief A copy of the element reduced to a selection of fields if it fulfils a condition, as
       *        lSelectElemPack() with a condition makes it
       * @param where the condition; an empty one is fulfilled by every element
       * @param what the selection of fields
       * @param hashing whether copied sub-lists keep hash tables, the isHash argument of the old forms
       * @return the owning handle on the copy; empty if this handle is empty or the element does not fulfil
       *         the condition
       * @throw FieldException with reason NullElement if @p what is empty
       * @throw ListCoreError if this element does not hold a selected field or a copy fails
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lSelectElemPack(); core: ocs::tom::compare(), ocs::tom::select_elem()
       */
      [[nodiscard]] ElementPtr<T> select(const Where<T> &where, const What<T> &what,
                                         const Hashing hashing = Hashing::On) const {
         if (ep_ == nullptr || !compare(ep_, where.get())) {
            return {};
         }
         return ElementPtr<T>::from_typed(select_elem(ep_, what.get(), hashing == Hashing::On, false));
      }


      /**
       * @brief Pack the element, as cull_pack_elem() does
       *
       * Does not compile for a type with an Int32 or Int64 field, which the pack format cannot carry (see
       * PackableType); such a type packs with pack(pb, what) and a selection without it.
       *
       * @param pb the buffer to append to
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @throw PackException if the buffer cannot grow
       * @see Old form: cull_pack_elem(); core: ocs::tom::pack_elem()
       */
      void pack(sge_pack_buffer &pb) const {
         static_assert(PackableType<T>, "the type holds an Int32 or Int64 field, which cannot be packed");
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         pack_elem(&pb, ep_, nullptr, 0);
      }

      /**
       * @brief Pack the fields of the element a selection names, as cull_pack_elem_partial() does
       * @param pb the buffer to append to
       * @param what the selection
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @throw PackException with PACK_BADARG if the element does not hold a selected field, or if the buffer
       *        cannot grow
       * @see Old form: cull_pack_elem_partial(); core: ocs::tom::pack_elem()
       */
      void pack(sge_pack_buffer &pb, const What<T> &what) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         pack_elem(&pb, ep_, what.get(), 0);
      }


      /**
       * @brief Write the element as text, as lDumpElemFp() does
       * @param fp the stream
       * @param indent the nesting level, three blanks each
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @throw ListCoreError if the stream is null or the last write failed
       * @see Old form: lDumpElemFp(); core: ocs::tom::dump_elem()
       */
      void dump(FILE *fp, const int indent = 0) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         dump_elem(fp, ep_, indent);
      }

      /**
       * @brief Write the element as text for debugging, as lWriteElemTo() does
       *
       * The text is not read back; dump() writes the form that is.
       *
       * @param fp the stream
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @see Old form: lWriteElemTo(); core: ocs::tom::print_elem()
       */
      void print(FILE *fp) const {
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         print_to(fp, [this](dstring *buffer) { print_elem(buffer, ep_, 0); });
      }

      /**
       * @brief The number of nodes of the tree under this element, the element itself included, as
       *        lGetNumberOfNodes() counts them
       * @tparam F a list field of this type holding elements of this type, the children
       * @return the number, 0 for an empty handle
       * @see Old form: lGetNumberOfNodes(); core: ocs::tom::count_nodes()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) &&
                  std::is_same_v<typename FieldTraits<F>::Sub, T>
      [[nodiscard]] int count_nodes() const noexcept {
         return ep_ != nullptr ? ocs::tom::count_nodes(ep_, nullptr, static_cast<int>(F)) : 0;
      }

      /**
       * @brief The number of leaves of the tree under this element, the elements without children, as
       *        lGetNumberOfLeafs() counts them
       * @tparam F a list field of this type holding elements of this type, the children
       * @return the number, 0 for an empty handle
       * @see Old form: lGetNumberOfLeafs(); core: ocs::tom::count_leaves()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) &&
                  std::is_same_v<typename FieldTraits<F>::Sub, T>
      [[nodiscard]] int count_leaves() const noexcept {
         return ep_ != nullptr ? ocs::tom::count_leaves(ep_, nullptr, static_cast<int>(F)) : 0;
      }

      /**
       * @brief Pack the element into a file, as lWriteElemToDisk() does
       * @param prefix the directory, or nullptr
       * @param name the file name, or nullptr
       * @param obj_name the kind of object, for the messages logged on a failure
       * @throw FieldException with reason EmptyHandle if the handle is empty
       * @throw ocs::RuntimeException with the message logged if no file is named, the element cannot be packed
       *        or the file cannot be written
       * @see Old form: lWriteElemToDisk(); core: ocs::tom::write_elem_to_disk()
       */
      void write_to_disk(const char *prefix, const char *name, const char *obj_name) const {
         static_assert(PackableType<T>, "the type holds an Int32 or Int64 field, which cannot be packed");
         if (ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, NoName, T::descriptor.e);
         }
         write_elem_to_disk(ep_, prefix, name, obj_name);
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
       * @brief Read a field given only at run time, checked as the old getters by name (lGetUlong(), ...) check it
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
       * @brief Read a field; list and object fields give mutable handles, as lGetListRW() and lGetObject() do
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
       * @brief Set a field, as the old setter of its field type does: lSetUlong(), lSetString(), lSetList(), ...
       *
       * Strings are copied and the old value freed, the hash table of the
       * owning list is maintained, a list or an element is taken over and the
       * one held before freed.
       *
       * @tparam F the field number, of this object type
       * @param value the value; an owning handle is moved in
       * @throw FieldException with reason EmptyHandle if the handle is empty, AbsentField if the reduced element
       *        does not hold the field, OutOfMemory if a string cannot be copied, BoundElement if an element to
       *        store is bound in a list or another object already (it stays there, owned by what it is bound
       *        to; the moved-in handle is empty afterwards); in a debug build also with reason DuplicateKey
       *        when another element of the list holds the value of a unique field
       */
      template <auto F>
         requires FieldOf<F, T>
      void set(typename detail::SetterArg<F>::type value) const {
         if (!detail::write<F>(this->ep_, this->template present_slot<F>(), std::move(value))) [[unlikely]] {
            if constexpr (FieldTraits<F>::type == FieldType::Object && FieldTraits<F>::has_sub) {
               // the element is owned by the list or the object it is bound to: the parameter must not free it
               static_cast<void>(value.release());
            }
            throw_field_exception(FieldTraits<F>::type == FieldType::Object ? FieldException::Reason::BoundElement
                                                                       : FieldException::Reason::OutOfMemory,
                              F, this->ep_->descr);
         }
      }

      /**
       * @brief Set a field given only at run time, checked as the old setters by name (lSetUlong(), ...) check it
       *
       * For plain values only: numbers, bool, strings, host names and references. Runs on the core of the
       * free accessor ocs::tom::set(lListElem *, int, V) (detail::runtime_set()).
       *
       * @tparam V the value type, which must match the field's type
       * @param nm the field number
       * @param value the value
       * @throw FieldException with reason EmptyHandle if the handle is empty, otherwise ForeignField,
       *        AbsentField or WrongType in this order, OutOfMemory if a string cannot be copied; in a debug
       *        build also with reason DuplicateKey when another element of the list holds the value of a
       *        unique field
       */
      template <typename V>
      void set(const int nm, const V value) const {
         if (this->ep_ == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, nm, T::descriptor.e);
         }
         detail::runtime_set<V>(this->ep_, nm, value);
      }

      /**
       * @brief Add an offset to a number field and return the new value, as lAddUlong(), lAddUlong64() and lAddDouble() add
       *
       * An offset of 0 changes nothing. A 32 bit sum wraps around. The hash
       * table of the owning list is maintained. Unlike the old functions,
       * which return a status, it returns the new value, as operator+= does.
       *
       * @tparam F the field number, of this object type, a 32 bit, 64 bit or double field
       * @param offset the amount to add
       * @return the new value
       * @throw FieldException if the handle is empty or the reduced element does not hold the field; in a
       *        debug build also with reason DuplicateKey when another element of the list holds the new
       *        value of a unique field, and the old value is kept
       * @see Old form: lAddUlong(), lAddUlong64(), lAddDouble(); core: ocs::tom::add_number()
       */
      template <auto F>
         requires FieldOf<F, T> && detail::Addable<F>
      typename ValueOf<FieldTraits<F>::type>::type add(const typename ValueOf<FieldTraits<F>::type>::type offset) const {
         const int s = this->template present_slot<F>();
         return add_number(this->ep_, FieldTraits<F>::pos, this->ep_->cont[s], offset);
      }

      /**
       * @brief The next element of the list, as lNextRW() gives it
       * @return the element, or an empty handle at the end of the list or for an empty handle
       */
      [[nodiscard]] Element next() const noexcept {
         return from_typed(this->ep_ != nullptr ? this->ep_->next : nullptr);
      }

      /**
       * @brief The previous element of the list, as lPrevRW() gives it
       * @return the element, or an empty handle at the front of the list or for an empty handle
       */
      [[nodiscard]] Element prev() const noexcept {
         return from_typed(this->ep_ != nullptr ? this->ep_->prev : nullptr);
      }

      /**
       * @brief The next element of the list that fulfils a condition, as lFindNextRW() finds it
       * @param where the condition; an empty one takes the next element
       * @return the element, or an empty handle if none follows, or for an empty handle
       * @see Old form: lFindNextRW(); core: ocs::tom::find_next()
       */
      [[nodiscard]] Element next_if(const Where<T> &where) const {
         return from_typed(this->ep_ != nullptr ? find_next(this->ep_, where.get()) : nullptr);
      }

      /**
       * @brief The previous element of the list that fulfils a condition, as lFindPrevRW() finds it
       * @param where the condition; an empty one takes the previous element
       * @return the element, or an empty handle if none precedes, or for an empty handle
       * @see Old form: lFindPrevRW(); core: ocs::tom::find_prev()
       */
      [[nodiscard]] Element prev_if(const Where<T> &where) const {
         return from_typed(this->ep_ != nullptr ? find_prev(this->ep_, where.get()) : nullptr);
      }

      /**
       * @brief Put a list into a list field and take out the one held, as std::exchange() and lXchgList() do
       * @tparam F a list field of this object type whose sub-type the model names
       * @param list the new list, which the element takes over; may be empty
       * @return the list held before, now owned by the caller; empty if the field held none
       * @throw FieldException if the handle is empty or the reduced element does not hold the field; nothing
       *        is changed then
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) && FieldTraits<F>::has_sub
      ListPtr<typename FieldTraits<F>::Sub> exchange(ListPtr<typename FieldTraits<F>::Sub> &&list) const {
         static_cast<void>(this->template present_slot<F>());
         lList *lp = list.release();
         exchange_list(this->ep_, FieldTraits<F>::pos, lp);
         return ListPtr<typename FieldTraits<F>::Sub>::from_typed(lp);
      }

      /**
       * @brief Take the element out of an object field without copying it, as lDechainObject() does
       *
       * The typed counterpart of lDechainObject(): where `clone()` and `set<F>(nullptr)` copy the sub-object
       * deeply and free the original, this moves it out. The field holds nullptr afterwards, and the element
       * taken is free, so it can be stored in another object or appended to a list.
       *
       * @tparam F an object field of this object type whose sub-type the model names
       * @return the element the field held, now owned by the caller; empty if the field held none
       * @throw FieldException with reason EmptyHandle if the handle is empty, AbsentField if the reduced element
       *        does not hold the field; nothing is changed then
       * @see Old form: lDechainObject(); core: ocs::tom::take_object()
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::Object) && FieldTraits<F>::has_sub
      [[nodiscard]] ElementPtr<typename FieldTraits<F>::Sub> take() const {
         static_cast<void>(this->template present_slot<F>());
         return ElementPtr<typename FieldTraits<F>::Sub>::from_typed(take_object(this->ep_, FieldTraits<F>::pos));
      }

      /**
       * @brief The list of a list field, created if the field holds none, as lGetOrCreateList() does
       *
       * With it, adding to and deleting from a list field compose:
       * `job.get_or_create<JB_env_list>().emplace_back<VA_variable>("X")` in
       * place of lAddSubStr(), `job.get<JB_env_list>().erase<VA_variable>("X")`
       * in place of lDelSubStr().
       *
       * @tparam F a list field of this object type whose sub-type the model names
       * @param name the name of a list created here; nullptr names it after the field
       * @return the list held in the field
       * @throw FieldException if the handle is empty or the reduced element does not hold the field
       * @throw ocs::RuntimeException if there is no memory
       */
      template <auto F>
         requires FieldOf<F, T> && (FieldTraits<F>::type == FieldType::List) && FieldTraits<F>::has_sub
      List<typename FieldTraits<F>::Sub> get_or_create(const char *name = nullptr) const {
         using Sub = typename FieldTraits<F>::Sub;
         static_cast<void>(this->template present_slot<F>());
         return List<Sub>::from_typed(get_or_create_list(this->ep_, FieldTraits<F>::pos, name, Sub::descriptor.e));
      }

      /**
       * @brief Swap the lists of a list field of this element and a list field of another, as lSwapList() does
       * @tparam F a list field of this object type
       * @tparam G a list field of the other element's type, holding the same sub-type as F
       * @tparam U the struct of the other element's type
       * @param other the other element
       * @throw FieldException if a handle is empty or a reduced element does not hold its field; nothing is
       *        changed then
       */
      template <auto F, auto G, ObjectType U>
         requires FieldOf<F, T> && FieldOf<G, U> && (FieldTraits<F>::type == FieldType::List) &&
                  (FieldTraits<G>::type == FieldType::List) && FieldTraits<F>::has_sub &&
                  std::is_same_v<typename FieldTraits<F>::Sub, typename FieldTraits<G>::Sub>
      void swap(const Element<U> other) const {
         static_cast<void>(this->template present_slot<F>());
         if (!other) [[unlikely]] {
            throw_field_exception(FieldException::Reason::EmptyHandle, G, U::descriptor.e);
         }
         static_cast<void>(detail::present_slot<G>(other.raw()));
         lList *lp = nullptr;
         exchange_list(other.raw(), FieldTraits<G>::pos, lp);
         exchange_list(this->ep_, FieldTraits<F>::pos, lp);
         exchange_list(other.raw(), FieldTraits<G>::pos, lp);
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
       * @brief Take over an element from a source whose type is known, without a check
       * @param ep the element, or nullptr; freed by the handle
       * @return the owning handle
       */
      [[nodiscard]] static ElementPtr from_typed(lListElem *ep) noexcept {
         ElementPtr handle;
         handle.ep_ = ep;
         return handle;
      }

      /**
       * @brief Create an element of the type, with every field at its default, as lCreateElem() does
       * @return the owning handle
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create() {
         ElementPtr handle;
         handle.ep_ = create_elem(T::descriptor.e);
         return handle;
      }

      /**
       * @brief Create an element of the type with a given descriptor, with every field at its default, as
       *        lCreateElem() does
       *
       * For an element that must have the descriptor of another list or
       * element, typically a reduced one. A reduced descriptor keeps the first
       * field and the full length of the type, so it is checked like the
       * element the checked constructor takes.
       *
       * @param descr the descriptor, the full one of the type or a reduction of it
       * @return the owning handle
       * @throw FieldException with reason WrongType if @p descr is of another type or empty, NullDescriptor
       *        if it is null
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create(const lDescr *descr) {
         if (descr != nullptr && descr[0].nm != T::fields[0].nm) [[unlikely]] {
            detail::throw_wrong_type<T>(descr);
         }
         ElementPtr handle;
         handle.ep_ = create_elem(descr);
         return handle;
      }

      /**
       * @brief Create an element of a reduced layout, with every field at its default, as lCreateElem() with a
       *        descriptor of ocs_cull_descr_reduce() does
       * @param layout the layout, from layout<F...>()
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p layout is default-constructed
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create(const Layout<T> layout) {
         ElementPtr handle;
         handle.ep_ = create_elem(layout.get());
         return handle;
      }

      /**
       * @brief Create an element of the layout a selection reduces the type to, with every field at its default,
       *        as lCreateElem() with the descriptor of lReduceDescr() does
       *
       * For a selection known only at run time; a fixed set of fields is
       * layout<F...>(). The selection caches the reduction, so creating many
       * elements with one selection builds it once.
       *
       * @param what the selection of fields
       * @return the owning handle
       * @throw FieldException with reason NullElement if @p what is empty (see ocs_Select.h), NullDescriptor if
       *        it selects no field of the type, where lReduceDescr() gave no descriptor
       * @throw ocs::RuntimeException if there is no memory
       * @see Old form: lReduceDescr() and lCreateElem(); core: ocs::tom::reduced_descr()
       */
      [[nodiscard]] static ElementPtr create(const What<T> &what) {
         if (!what) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullElement, NoName, T::descriptor.e);
         }
         const lDescr *descr = reduced_descr(T::descriptor.e, what.get());
         if (descr == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullDescriptor, NoName, T::descriptor.e);
         }
         ElementPtr handle;
         try {
            // the element takes a reference of its own on the reduction
            handle.ep_ = create_elem(descr);
         } catch (...) {
            ocs_cull_descr_release(&descr);
            throw;
         }
         ocs_cull_descr_release(&descr);
         return handle;
      }

      /**
       * @brief Create an element with the layout of another element, with every field at its default, as
       *        lCreateElem(lGetElemDescr(ep)) does
       *
       * For an element that must go into a list of a reduced element, or must
       * take the fields of one.
       *
       * @param like the element whose layout the new one gets
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p like is empty, as lCreateElem() of the null
       *        descriptor lGetElemDescr() gave for it
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create_like(const ConstElement<T> like) {
         ElementPtr handle;
         handle.ep_ = create_elem(like.descr());
         return handle;
      }

      /**
       * @brief Create an element with the layout of a list, with every field at its default, as
       *        lCreateElem(lGetListDescr(lp)) does
       *
       * An element linked into a list must have the layout of the list
       * (CS-3112); this makes one for a list that may be reduced.
       *
       * @param like the list whose layout the new element gets
       * @return the owning handle
       * @throw FieldException with reason NullDescriptor if @p like is empty, as lCreateElem() of the null
       *        descriptor lGetListDescr() gave for it
       * @throw ocs::RuntimeException if there is no memory
       */
      [[nodiscard]] static ElementPtr create_like(const ConstList<T> like) {
         ElementPtr handle;
         handle.ep_ = create_elem(like.descr());
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
            free_elem(ep_);
            ep_ = std::exchange(other.ep_, nullptr);
         }
         return *this;
      }

      /** @brief Free the element held, as lFreeElem() does */
      ~ElementPtr() {
         free_elem(ep_);
      }

      /**
       * @brief A non-owning handle on the element
       * @return the handle, empty if this one is
       */
      [[nodiscard]] Element<T> get() const noexcept {
         return Element<T>::from_typed(ep_);
      }

      /**
       * @brief Call a member of the non-owning handle, as std::unique_ptr::operator-> does: `job->set<JB_owner>("alice")`
       * @return an object that leads the call on to get()
       */
      detail::Arrow<Element<T>> operator->() const noexcept {
         return detail::Arrow<Element<T>>(get());
      }

      /**
       * @brief The non-owning handle, as std::unique_ptr::operator* gives the object: `Element<JB> job = *owned`
       * @return the handle, empty if this one is; the same as get()
       */
      [[nodiscard]] Element<T> operator*() const noexcept {
         return get();
      }

      /**
       * @brief Free the element held now, as std::unique_ptr::reset() and lFreeElem() do
       */
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

      /**
       * @brief Whether the handle holds an element
       * @return true if not empty
       */
      explicit operator bool() const noexcept {
         return ep_ != nullptr;
      }

      /**
       * @brief Whether the handle is empty, as a raw pointer compared with nullptr
       * @param a the handle
       * @return true if it holds no element
       */
      friend bool operator==(const ElementPtr &a, std::nullptr_t) noexcept {
         return a.ep_ == nullptr;
      }


      /**
       * @brief Unpack an element packed by pack() or cull_pack_elem(), as cull_unpack_elem() does
       * @param pb the buffer to read from
       * @param flags read only the fields whose flags match, 0 for all
       * @return the owning handle on the element
       * @throw FieldException with reason WrongType if the buffer holds an element of another type; it is freed
       * @throw PackException with PACK_FORMAT for bytes that are cut off, PACK_ENOMEM
       * @see Old form: cull_unpack_elem(), cull_unpack_elem_partial(); core: ocs::tom::unpack_elem()
       */
      [[nodiscard]] static ElementPtr unpack(sge_pack_buffer &pb, const int flags = 0) {
         lListElem *ep = unpack_elem(&pb, T::descriptor.e, flags);
         if (ep->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            try {
               detail::throw_wrong_type<T>(ep->descr);
            } catch (...) {
               free_elem(ep);
               throw;
            }
         }
         return from_typed(ep);
      }


      /**
       * @brief Read an element written as text by dump(), as lUndumpElemFp() does
       *
       * The text of an element does not name its type; it is read with the
       * fields of @p T in their order.
       *
       * @param fp the stream
       * @return the owning handle on the element
       * @throw ListCoreError with the error number of before if the text cannot be read
       * @see Old form: lUndumpElemFp(); core: ocs::tom::undump_elem()
       */
      [[nodiscard]] static ElementPtr undump(FILE *fp) {
         return from_typed(undump_elem(fp, T::descriptor.e));
      }

      /**
       * @brief Unpack an element from a file written by write_to_disk(), as lReadElemFromDisk() does
       * @param prefix the directory, or nullptr
       * @param name the file name, or nullptr
       * @param obj_name the kind of object, for the messages logged on a failure
       * @return the owning handle on the element
       * @throw FieldException with reason WrongType if the file holds an element of another type; it is freed
       * @throw ocs::RuntimeException with the message logged if no file is named, it cannot be read or does not
       *        hold an element
       * @see Old form: lReadElemFromDisk(); core: ocs::tom::read_elem_from_disk()
       */
      [[nodiscard]] static ElementPtr read_from_disk(const char *prefix, const char *name, const char *obj_name) {
         lListElem *ep = read_elem_from_disk(prefix, name, T::descriptor.e, obj_name);
         if (ep->descr[0].nm != T::fields[0].nm) [[unlikely]] {
            try {
               detail::throw_wrong_type<T>(ep->descr);
            } catch (...) {
               free_elem(ep);
               throw;
            }
         }
         return from_typed(ep);
      }

   private:
      lListElem *ep_ = nullptr;   ///< the element owned, or nullptr
   };

   /**
    * @brief Read a field of an element whose type is known only at run time; replaces lGetUlong(), lGetString(), ... in the rewrite of M3
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
      return detail::read<F, false>(ep, detail::checked_slot<F>(ep));
   }

   /**
    * @brief Read a field of a mutable element whose type is known only at run time; replaces lGetListRW(), lGetObject(), ...
    * @tparam F the field number
    * @param ep the element
    * @return the value; list and object fields give mutable handles or pointers
    * @throw FieldException if the element is nullptr, of another type or does not hold the field
    */
   template <auto F>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(lListElem *ep) {
      return detail::read<F, true>(ep, detail::checked_slot<F>(ep));
   }

   /**
    * @brief Read a field of an element handle (L2); the free form of a variable that became a handle
    *
    * As get() on a raw pointer, but the type of the element is known: a field
    * of another type does not compile, and only an empty handle and a reduced
    * element without the field are checked at run time.
    *
    * @tparam F the field number, of the handle's type
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @return the value, as the field table declares it
    * @throw FieldException with reason NullElement for an empty handle, AbsentField for a reduced element
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(const ConstElement<T> &ep) {
      detail::assert_field_of<F, T>();
      return detail::read<F, false>(ep.raw(), detail::checked_slot<F, true>(ep.raw()));
   }

   /**
    * @brief Read a field of a mutable element handle (L2); list and object fields give mutable handles
    * @tparam F the field number, of the handle's type
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @return the value, as the field table declares it
    * @throw FieldException with reason NullElement for an empty handle, AbsentField for a reduced element
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(const Element<T> &ep) {
      detail::assert_field_of<F, T>();
      return detail::read<F, true>(ep.raw(), detail::checked_slot<F, true>(ep.raw()));
   }

   namespace detail {

      /** @brief Whether a type is an owning element handle, which a setter of an object field takes over */
      template <typename V>
      inline constexpr bool is_element_ptr = false;

      /** @brief An owning element handle */
      template <ObjectType T>
      inline constexpr bool is_element_ptr<ElementPtr<T>> = true;

      /** @brief Whether a type is an owning list handle, which a setter of a list field takes over */
      template <typename V>
      inline constexpr bool is_list_ptr = false;

      /** @brief An owning list handle */
      template <ObjectType T>
      inline constexpr bool is_list_ptr<ListPtr<T>> = true;

      /**
       * @brief The free setter on an element that may be nullptr, for the raw pointer and the handle (L2)
       * @tparam F the field number
       * @tparam Typed true if the element comes from a handle of the field's type, see check_holds()
       * @tparam V the value's type
       * @param ep the element, or nullptr
       * @param value the value; a list or an element is taken over, as by the old setter; an owning handle
       *        (std::move) gives up its list or element once it is stored, and also when the element is bound
       *        elsewhere (it stays where it is bound; freeing it through the handle would free a bound element)
       * @return 0 on success, -1 for a null element, a string that cannot be copied or an element bound elsewhere
       * @throw FieldException with reason ForeignField or AbsentField if the element does not hold the field
       */
      template <auto F, bool Typed, typename V>
      int free_set(lListElem *ep, V &&value) {
         using Tr = FieldTraits<F>;
         using Value = std::remove_cvref_t<V>;
         if constexpr (is_element_ptr<Value> || is_list_ptr<Value>) {
            static_assert(!std::is_lvalue_reference_v<V>, "an owning handle is handed over with std::move");
            if constexpr (!std::is_void_v<typename Tr::Sub>) {
               static_assert(std::is_same_v<Value, ElementPtr<typename Tr::Sub>> ||
                                std::is_same_v<Value, ListPtr<typename Tr::Sub>>,
                             "the owning handle holds another type than the field's sub-type");
            }
         }
         if (ep == nullptr) [[unlikely]] {
            report_soft_path("ocs::tom::set", static_cast<int>(F), SoftPathReason::NullElement);
            set_lerrno(LEELEMNULL);
            return -1;
         }
         check_holds<F, Typed>(ep);
         const int off = ep->descr[Tr::pos].off;
         // a typed list or object field takes the raw pointer here, as the old setter did
         if constexpr (Tr::type == FieldType::List) {
            if constexpr (is_list_ptr<Value>) {
               store_list(ep, Tr::pos, value.release());
            } else {
               store_list(ep, Tr::pos, static_cast<lList *>(value));
            }
            return 0;
         } else if constexpr (Tr::type == FieldType::Object) {
            lListElem *object = nullptr;
            if constexpr (is_element_ptr<Value>) {
               object = value.get().raw();
            } else {
               object = static_cast<lListElem *>(value);
            }
            if (!store_object(ep, Tr::pos, object)) {
               report_soft_path("ocs::tom::set", static_cast<int>(F), SoftPathReason::BoundElement);
               set_lerrno(LEBOUNDELEM);
               if constexpr (is_element_ptr<Value>) {
                  // bound elsewhere: it belongs to where it is bound, as Element::set() leaves it
                  static_cast<void>(value.release());
               }
               return -1;
            }
            if constexpr (is_element_ptr<Value>) {
               static_cast<void>(value.release());   // the element now belongs to the field
            }
            return 0;
         } else {
            if (!write<F>(ep, Tr::pos + off, static_cast<typename ValueOf<Tr::type>::type>(value))) {
               set_lerrno(LESTRDUP);
               return -1;
            }
            return 0;
         }
      }

      /**
       * @brief The free add on an element that may be nullptr, for the raw pointer and the handle (L2)
       * @tparam F the field number, a 32 bit, 64 bit or double field
       * @tparam Typed true if the element comes from a handle of the field's type, see check_holds()
       * @param ep the element, or nullptr
       * @param offset the amount to add
       * @return 0 on success, -1 with LEELEMNULL for a null element
       * @throw FieldException with reason ForeignField or AbsentField if the element does not hold the field
       */
      template <auto F, bool Typed>
      int free_add(lListElem *ep, const typename ValueOf<FieldTraits<F>::type>::type offset) {
         using Tr = FieldTraits<F>;
         if (ep == nullptr) [[unlikely]] {
            report_soft_path("ocs::tom::add", static_cast<int>(F), SoftPathReason::NullElement);
            set_lerrno(LEELEMNULL);
            return -1;
         }
         check_holds<F, Typed>(ep);
         const int off = ep->descr[Tr::pos].off;
         add_number(ep, Tr::pos, ep->cont[Tr::pos + off], offset);
         return 0;
      }

   } // namespace detail

   /**
    * @brief Set a field of an element whose type is known only at run time, with the error behaviour of the old setter it replaces (lSetUlong(), lSetString(), ...)
    *
    * The free form that replaces the old setter in the mechanical rewrite,
    * with the contract the old setters follow since CS-3062 (N-L-4): -1 with
    * LEELEMNULL for a null element, a logic error for a field the element
    * does not hold. The object or list a field holds already is set again
    * without an error.
    *
    * @tparam F the field number
    * @param ep the element
    * @param value the value; a list or an element is taken over, as by the old setter
    * @return 0 on success, -1 for a null element, a string that cannot be copied or an element bound elsewhere
    * @throw FieldException with reason ForeignField or AbsentField if the element does not hold the field
    */
   template <auto F, typename V>
      requires TypedField<decltype(F)>
   int set(lListElem *ep, V &&value) {
      return detail::free_set<F, false>(ep, std::forward<V>(value));
   }

   /**
    * @brief Set a field of a mutable element handle (L2), with the error behaviour of the free setter
    *
    * A field of another type than the handle's does not compile; an empty
    * handle gives -1 with LEELEMNULL, a reduced element without the field
    * throws, as for the raw pointer.
    *
    * @tparam F the field number, of the handle's type
    * @tparam T the object type, deduced from the handle
    * @tparam V the value's type
    * @param ep the element
    * @param value the value; a list or an element is taken over, as by the old setter
    * @return 0 on success, -1 for an empty handle, a string that cannot be copied or an element bound elsewhere
    * @throw FieldException with reason AbsentField if the element does not hold the field
    */
   template <auto F, ObjectType T, typename V>
      requires TypedField<decltype(F)>
   int set(const Element<T> &ep, V &&value) {
      detail::assert_field_of<F, T>();
      return detail::free_set<F, true>(ep.raw(), std::forward<V>(value));
   }

   /**
    * @brief Add an offset to a number field of an element whose type is known only at run time, with the error behaviour of the old function it replaces (lAddUlong(), lAddUlong64(), lAddDouble())
    *
    * The free form for the mechanical rewrite, next to the free setter, with
    * the contract of the old setters since CS-3062 (N-L-4): -1 with
    * LEELEMNULL for a null element, a logic error for a field the element
    * does not hold. A field of another type does not compile.
    *
    * @tparam F the field number, a 32 bit, 64 bit or double field
    * @param ep the element
    * @param offset the amount to add
    * @return 0 on success, -1 on an error the old function returned
    * @throw FieldException with reason ForeignField or AbsentField if the element does not hold the field, and
    *        DuplicateKey in a debug build, where the old function aborted
    * @see Typed form: Element<T>::add<F>(); core: ocs::tom::add_number()
    */
   template <auto F>
      requires TypedField<decltype(F)> && detail::Addable<F>
   int add(lListElem *ep, const typename ValueOf<FieldTraits<F>::type>::type offset) {
      return detail::free_add<F, false>(ep, offset);
   }

   /**
    * @brief Add an offset to a number field of a mutable element handle (L2)
    *
    * A field of another type than the handle's does not compile; otherwise
    * as add() on the raw pointer.
    *
    * @tparam F the field number, of the handle's type, a 32 bit, 64 bit or double field
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @param offset the amount to add
    * @return 0 on success, -1 with LEELEMNULL for an empty handle
    * @throw FieldException with reason AbsentField if the element does not hold the field
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::Addable<F>
   int add(const Element<T> &ep, const typename ValueOf<FieldTraits<F>::type>::type offset) {
      detail::assert_field_of<F, T>();
      return detail::free_add<F, true>(ep.raw(), offset);
   }

   namespace detail {

      /** @brief A 64 bit unsigned field, which the atomic accessors read */
      template <auto F>
      concept AtomicReadable = FieldTraits<F>::type == FieldType::Uint64;

      /**
       * @brief A 64 bit unsigned field that is declared neither hashed nor unique, which the atomic accessors write
       *
       * The atomic store bypasses the hash table and the uniqueness check of the setter alike.
       */
      template <auto F>
      concept AtomicWritable = AtomicReadable<F> && !FieldTraits<F>::hashed && !FieldTraits<F>::unique;

   } // namespace detail

   /**
    * @brief Read a 64 bit unsigned field without being able to observe it torn; replaces lGetUlong64Atomic()
    *
    * For a field that several threads write while others read it, and that carries no invariant with any other
    * field of the element (a time of last use). The guarantee is about the single load, not about ordering with
    * anything else, so the access is relaxed; whoever needs ordering needs a lock.
    *
    * An Element<T> binds here too: Element derives from ConstElement, which deduction accepts.
    *
    * @tparam F the field number, of the handle's type and of type Uint64
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @return the value
    * @throw FieldException with reason NullElement for an empty handle, AbsentField for a reduced element
    * @see core: ocs::tom::load_uint64_atomic()
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::AtomicReadable<F>
   [[nodiscard]] std::uint64_t get_atomic(const ConstElement<T> &ep) {
      detail::assert_field_of<F, T>();
      const lListElem *raw = ep.raw();
      // the slot first: it throws NullElement for an empty handle before raw is dereferenced
      const int pos = detail::checked_slot<F, true>(raw);
      return load_uint64_atomic(raw->cont[pos]);
   }

   /**
    * @brief Write a 64 bit unsigned field without a reader being able to see it torn; replaces lSetUlong64Atomic()
    *
    * The store bypasses the hash table, so a field declared hashed does not compile, where lSetUlong64Atomic()
    * refused it at run time. The declaration decides, not whether a table is built: the tables are built when an
    * element enters a list. A field declared unique does not compile either: the store bypasses the uniqueness
    * check of set() as well.
    *
    * @tparam F the field number, of the handle's type, of type Uint64, neither hashed nor unique
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @param value the value to store
    * @return 0 on success, -1 with LEELEMNULL for an empty handle, as lSetUlong64Atomic() on nullptr
    * @throw FieldException with reason AbsentField if the element does not hold the field
    * @see core: ocs::tom::store_uint64_atomic()
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::AtomicWritable<F>
   int set_atomic(const Element<T> &ep, const std::uint64_t value) {
      detail::assert_field_of<F, T>();
      lListElem *raw = ep.raw();
      if (raw == nullptr) [[unlikely]] {
         report_soft_path("ocs::tom::set_atomic", static_cast<int>(F), SoftPathReason::NullElement);
         set_lerrno(LEELEMNULL);
         return -1;
      }
      const int pos = detail::checked_slot<F, true>(raw);
      store_uint64_atomic(raw->cont[pos], value);
      return 0;
   }

   // The owning element handle takes the free forms of the handle it owns: a local variable that owns its element
   // (rule L3) keeps its field access lines. Template deduction does not see conversions, hence one overload each.

   /**
    * @brief Read a field of an owning element handle (L3), as get() on the handle it owns
    * @tparam F the field number, of the handle's type
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @return the value, as the field table declares it; list and object fields give mutable handles
    * @throw FieldException with reason NullElement for an empty handle, AbsentField for a reduced element
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)>
   [[nodiscard]] auto get(const ElementPtr<T> &ep) {
      return get<F>(ep.get());
   }

   /**
    * @brief Set a field of an owning element handle (L3), as set() on the handle it owns
    * @tparam F the field number, of the handle's type
    * @tparam T the object type, deduced from the handle
    * @tparam V the value's type
    * @param ep the element
    * @param value the value; a list or an element is taken over, as by the old setter
    * @return 0 on success, -1 for an empty handle, a string that cannot be copied or an element bound elsewhere
    * @throw FieldException with reason AbsentField if the element does not hold the field
    */
   template <auto F, ObjectType T, typename V>
      requires TypedField<decltype(F)>
   int set(const ElementPtr<T> &ep, V &&value) {
      return set<F>(ep.get(), std::forward<V>(value));
   }

   /**
    * @brief Add an offset to a number field of an owning element handle (L3), as add() on the handle it owns
    * @tparam F the field number, of the handle's type, a 32 bit, 64 bit or double field
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @param offset the amount to add
    * @return 0 on success, -1 with LEELEMNULL for an empty handle
    * @throw FieldException with reason AbsentField if the element does not hold the field
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::Addable<F>
   int add(const ElementPtr<T> &ep, const typename ValueOf<FieldTraits<F>::type>::type offset) {
      return add<F>(ep.get(), offset);
   }

   /**
    * @brief Read a 64 bit unsigned field of an owning element handle (L3) atomically, as get_atomic() on the handle
    *        it owns
    * @tparam F the field number, of the handle's type and of type Uint64
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @return the value
    * @throw FieldException with reason NullElement for an empty handle, AbsentField for a reduced element
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::AtomicReadable<F>
   [[nodiscard]] std::uint64_t get_atomic(const ElementPtr<T> &ep) {
      return get_atomic<F>(ep.get());
   }

   /**
    * @brief Write a 64 bit unsigned field of an owning element handle (L3) atomically, as set_atomic() on the
    *        handle it owns
    * @tparam F the field number, of the handle's type, of type Uint64, neither hashed nor unique
    * @tparam T the object type, deduced from the handle
    * @param ep the element
    * @param value the value to store
    * @return 0 on success, -1 with LEELEMNULL for an empty handle
    * @throw FieldException with reason AbsentField if the element does not hold the field
    */
   template <auto F, ObjectType T>
      requires TypedField<decltype(F)> && detail::AtomicWritable<F>
   int set_atomic(const ElementPtr<T> &ep, const std::uint64_t value) {
      return set_atomic<F>(ep.get(), value);
   }

} // namespace ocs::tom

#include "tom/ocs_List.h"
