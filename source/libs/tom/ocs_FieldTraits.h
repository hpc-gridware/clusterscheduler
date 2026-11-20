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
 * @brief The object types of the typed object model (TOM) and what follows from them per field
 *
 * The generator writes one struct per object type into `libs/sgeobj/tom`,
 * `ocs::tom::JB` for jobs. It holds the type's field table, the one source of
 * truth about the type, and the descriptor and the field names of the list
 * library, which are derived from that table at compile time by
 * make_descriptor() and field_names(). The generator also writes
 * which object type a field enumeration belongs to (TypeOf) and the sub-type of
 * every list and object field the model types (SubType).
 *
 * Field types, flags and value types are the model's own, with the C++
 * standard types as values. make_descriptor() is the one place that translates
 * them into the tags of the list library.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

#include "tom/cull_types.h"
#include "tom/ocs_Host.h"

namespace ocs::tom {

   /** @brief The type of a field: what it holds in C++ */
   enum class FieldType : std::uint8_t {
      Uint32,  ///< an unsigned 32 bit number, std::uint32_t
      Uint64,  ///< an unsigned 64 bit number, std::uint64_t; also the model's timestamps
      Int32,   ///< a signed 32 bit number, std::int32_t
      Int64,   ///< a signed 64 bit number, std::int64_t
      Double,  ///< a double precision floating point number
      Bool,    ///< a bool
      String,  ///< a C string owned by the element; nullptr means not set
      Host,    ///< a host name owned by the element, compared like host names (ocs::tom::Host)
      List,    ///< a list of elements, of the sub-type if the model names one
      Object,  ///< a single element, of the sub-type if the model names one
      Ref      ///< an opaque pointer the element does not own
   };

   /** @brief The flags of a field */
   enum class Flag : std::uint16_t {
      None = 0,               ///< no flag
      PrimaryKey = 1 << 0,    ///< the field is part of the primary key; implies neither uniqueness nor hashing
      Hash = 1 << 1,          ///< lookups by this field use a hash table
      Unique = 1 << 2,        ///< no two elements of a list hold the same value in this field
      NoTransfer = 1 << 3,    ///< the value stays inside the process; it is neither packed nor sent in events
      Configure = 1 << 4,     ///< the field may be changed by configuration functions
      Spool = 1 << 5,         ///< the field is written when the object is spooled
      SpoolSublist = 1 << 6,  ///< the field is spooled when the type is the sub-type of another one
      SpoolProject = 1 << 7   ///< the field is spooled with a project
   };

   /**
    * @brief Combine two sets of flags
    * @param a the first set
    * @param b the second set
    * @return the union
    */
   constexpr Flag operator|(const Flag a, const Flag b) {
      return static_cast<Flag>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
   }

   /**
    * @brief Whether a set of flags holds a flag
    * @param flags the set
    * @param flag the flag
    * @return true if the flag is in the set
    */
   constexpr bool has_flag(const Flag flags, const Flag flag) {
      return (static_cast<std::uint16_t>(flags) & static_cast<std::uint16_t>(flag)) != 0;
   }

   /** @brief One field of an object type as the object model declares it */
   struct FieldInfo {
      int nm;              ///< the field number
      const char *name;    ///< the field's name, e.g. "JB_job_number"
      FieldType type;      ///< the field type
      Flag flags;          ///< the field's flags
   };

   /**
    * @brief The tag of the list library for a field type
    * @param type the field type
    * @return the tag, an @ref _enum_lMultiType value
    */
   constexpr int cull_type_of(const FieldType type) {
      switch (type) {
         case FieldType::Uint32:
            return lUlongT;
         case FieldType::Uint64:
            return lUlong64T;
         case FieldType::Int32:
            return lIntT;
         case FieldType::Int64:
            return lLongT;
         case FieldType::Double:
            return lDoubleT;
         case FieldType::Bool:
            return lBoolT;
         case FieldType::String:
            return lStringT;
         case FieldType::Host:
            return lHostT;
         case FieldType::List:
            return lListT;
         case FieldType::Object:
            return lObjectT;
         case FieldType::Ref:
            return lRefT;
      }
      return lEndT;
   }

   /**
    * @brief The field type of a descriptor entry, for checks of a field known only at run time
    * @param mt the entry's type and attribute bits
    * @return the field type
    */
   constexpr FieldType field_type_of(const int mt) {
      switch (mt_get_type(mt)) {
         case lUlongT:
            return FieldType::Uint32;
         case lUlong64T:
            return FieldType::Uint64;
         case lIntT:
            return FieldType::Int32;
         case lLongT:
            return FieldType::Int64;
         case lDoubleT:
            return FieldType::Double;
         case lBoolT:
            return FieldType::Bool;
         case lStringT:
            return FieldType::String;
         case lHostT:
            return FieldType::Host;
         case lListT:
            return FieldType::List;
         case lObjectT:
            return FieldType::Object;
         default:
            return FieldType::Ref;
      }
   }

   /**
    * @brief The attribute bits of the list library for a set of flags
    * @param flags the flags
    * @return the bits, @ref cull_field_attributes or-ed together
    */
   constexpr int cull_flags_of(const Flag flags) {
      constexpr std::pair<Flag, int> map[] = {
         {Flag::PrimaryKey, CULL_PRIMARY_KEY}, {Flag::Hash, CULL_HASH}, {Flag::Unique, CULL_UNIQUE},
         {Flag::NoTransfer, CULL_NO_TRANSFER}, {Flag::Configure, CULL_CONFIGURE}, {Flag::Spool, CULL_SPOOL},
         {Flag::SpoolSublist, CULL_SUBLIST}, {Flag::SpoolProject, CULL_SPOOL_PROJECT},
      };
      int bits = CULL_DEFAULT;
      for (const auto &[flag, bit] : map) {
         if (has_flag(flags, flag)) {
            bits |= bit;
         }
      }
      return bits;
   }

   /**
    * @brief Build the descriptor of the list library from a field table
    *
    * The block has the layout #LISTDEF/#LISTEND produce: header, one entry per
    * field, terminating entry.
    *
    * @tparam N number of fields
    * @tparam I indices 0 .. N-1
    * @param fields the field table
    * @return the descriptor block
    */
   template <std::size_t N, std::size_t... I>
   consteval cull_descr_block<N + 1> make_descriptor_impl(const FieldInfo (&fields)[N], std::index_sequence<I...>) {
      const lDescr entries[N + 1] = {{fields[I].nm, cull_type_of(fields[I].type) | cull_flags_of(fields[I].flags), 0}...,
                                     {NoName, lEndT, 0}};
      return ocs_cull_descr_make(entries);
   }

   /**
    * @brief Whether the hash tables of the list library can hold a field type
    *
    * cull_hash_create() builds tables for 32 and 64 bit numbers, strings and host names only; for any other
    * type it logged an error and built none, so a declared hash or uniqueness check went silently missing.
    *
    * @param type the field type
    * @return true for Uint32, Uint64, String and Host
    */
   constexpr bool hashable_type(const FieldType type) {
      return type == FieldType::Uint32 || type == FieldType::Uint64 || type == FieldType::String ||
             type == FieldType::Host;
   }

   /**
    * @brief Refuse a field table that declares Hash or Unique on a field the hash tables cannot hold
    *
    * Called at compile time only: an invalid table makes the throw expression part of the constant
    * evaluation, which the compiler reports as an error naming the message.
    *
    * @tparam N number of fields
    * @param fields the field table
    */
   template <std::size_t N>
   consteval void check_hash_flags(const FieldInfo (&fields)[N]) {
      for (const FieldInfo &field : fields) {
         if ((has_flag(field.flags, Flag::Hash) || has_flag(field.flags, Flag::Unique)) &&
             !hashable_type(field.type)) {
            throw "Hash or Unique declared on a field that is not Uint32, Uint64, String or Host";
         }
      }
   }

   /**
    * @brief Build the descriptor of the list library from a field table, at compile time
    *
    * The table is checked first: Hash and Unique only on hashable field types (see #hashable_type), so a
    * model declaring them elsewhere does not compile.
    *
    * @tparam N number of fields
    * @param fields the field table
    * @return the descriptor block
    */
   template <std::size_t N>
   consteval cull_descr_block<N + 1> make_descriptor(const FieldInfo (&fields)[N]) {
      check_hash_flags(fields);
      return make_descriptor_impl(fields, std::make_index_sequence<N>{});
   }

   /**
    * @brief Whether every field of a field table can be packed
    *
    * The pack format has no encoding for Int32 and Int64 fields (pack_value() fails with PACK_FORMAT, as
    * cull_pack_switch() did), also when they are declared NoTransfer: such a field keeps its place on the
    * wire with a default value. A type holding one is packed only with a selection that leaves it out.
    *
    * @tparam N number of fields
    * @param fields the field table
    * @return true if no field is of type Int32 or Int64
    */
   template <std::size_t N>
   consteval bool packable_fields(const FieldInfo (&fields)[N]) {
      for (const FieldInfo &field : fields) {
         if (field.type == FieldType::Int32 || field.type == FieldType::Int64) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief The names of the fields of a field table, in its order
    * @tparam N number of fields
    * @tparam I indices 0 .. N-1
    * @param fields the field table
    * @return the names
    */
   template <std::size_t N, std::size_t... I>
   consteval std::array<const char *, N> field_names_impl(const FieldInfo (&fields)[N], std::index_sequence<I...>) {
      return {fields[I].name...};
   }

   /**
    * @brief The names of the fields of a field table, at compile time
    *
    * Each generated type keeps them as `names` (see #ObjectType); the name
    * lookups of the list library read the field table through the installed
    * object model instead (tom/ocs_Model.h).
    *
    * @tparam N number of fields
    * @param fields the field table
    * @return the names, in the order of the field numbers
    */
   template <std::size_t N>
   consteval std::array<const char *, N> field_names(const FieldInfo (&fields)[N]) {
      return field_names_impl(fields, std::make_index_sequence<N>{});
   }

   /**
    * @brief An object type of the typed object model, as the generator writes it
    *
    * It has a field table, which starts at the type's lower bound, and the
    * descriptor and the field names derived from it.
    *
    * @tparam T the struct of the type, e.g. ocs::tom::JB
    */
   template <typename T>
   concept ObjectType = requires {
      { T::fields[0] } -> std::convertible_to<const FieldInfo &>;
      T::descriptor.e;
      T::names.data();
   };

   /**
    * @brief An object type whose own fields can all be packed: no Int32 or Int64 field (see #packable_fields)
    *
    * The full pack of an element or a list of such a type compiles; a type with such a field packs only a
    * selection. The sub-types of list and object fields are not followed: a packed sub-list of a type with an
    * Int32 or Int64 field still fails at run time.
    *
    * @tparam T the struct of the type
    */
   template <typename T>
   concept PackableType = ObjectType<T> && packable_fields(T::fields);

   /**
    * @brief Object type of a field enumeration, generated per type
    *
    * A specialisation holds `using type`, the struct of the type. Every type has
    * an enumeration of its own, `JB_Field` for jobs, so the type of a field
    * number identifies its object type. A parameter of that type takes the field
    * numbers of that object type only.
    *
    * @tparam E the type of the field enumeration, e.g. `JB_Field`
    */
   template <typename E>
   struct TypeOf;

   /**
    * @brief Sub-type of a list or object field, generated per field that has one
    *
    * The primary template says that the field has none. The generator
    * specialises it with `using type`, the struct of the sub-type, which may be
    * declared only, so a type can name itself or a type that names it.
    *
    * @tparam F the field number
    */
   template <auto F>
   struct SubType {
      using type = void;   ///< no sub-type
   };

   /**
    * @brief A field number of an object type of the typed object model
    * @tparam E the type of the field enumeration
    */
   template <typename E>
   concept TypedField = std::is_enum_v<E> && requires { typename TypeOf<E>::type; };

   /**
    * @brief Value type of a field type that holds a plain value
    *
    * List and object fields are mapped by the handles, which know the sub-type.
    *
    * @tparam T the field type
    */
   template <FieldType T>
   struct ValueOf;

   /** @brief Uint32 holds a std::uint32_t */
   template <> struct ValueOf<FieldType::Uint32> { using type = std::uint32_t; };
   /** @brief Uint64 holds a std::uint64_t */
   template <> struct ValueOf<FieldType::Uint64> { using type = std::uint64_t; };
   /** @brief Int32 holds a std::int32_t */
   template <> struct ValueOf<FieldType::Int32> { using type = std::int32_t; };
   /** @brief Int64 holds a std::int64_t */
   template <> struct ValueOf<FieldType::Int64> { using type = std::int64_t; };
   /** @brief Double holds a double */
   template <> struct ValueOf<FieldType::Double> { using type = double; };
   /** @brief Bool holds a bool */
   template <> struct ValueOf<FieldType::Bool> { using type = bool; };
   /** @brief String is read as a C string owned by the element */
   template <> struct ValueOf<FieldType::String> { using type = const char *; };
   /** @brief Host is read as a host name owned by the element */
   template <> struct ValueOf<FieldType::Host> { using type = Host; };
   /** @brief Ref holds an opaque pointer, as the old interface does */
   template <> struct ValueOf<FieldType::Ref> { using type = void *; };

   /**
    * @brief Whether a field type can be the key of a lookup, an add or a delete by key: a number, a string or a host
    * @tparam T the field type
    */
   template <FieldType T>
   concept KeyFieldType = T == FieldType::Uint32 || T == FieldType::Uint64 || T == FieldType::String ||
                          T == FieldType::Host;

   /**
    * @brief The type the core takes a key of a field type as: the number, or a C string for a string or a host name
    * @tparam T the field type, a key field type
    */
   template <FieldType T>
      requires KeyFieldType<T>
   using KeyArgOf = std::conditional_t<T == FieldType::Uint32, std::uint32_t,
                                       std::conditional_t<T == FieldType::Uint64, std::uint64_t, const char *>>;

   // The value types are stored in the slots of the list library; they must be the same size.
   static_assert(sizeof(std::uint32_t) == sizeof(lUlong) && sizeof(std::uint64_t) == sizeof(lUlong64));
   static_assert(sizeof(std::int32_t) == sizeof(lInt), "Int32 is stored as an lInt");
   static_assert(sizeof(std::int64_t) == sizeof(lLong), "Int64 is stored as an lLong, which needs an LP64 platform");
   static_assert(sizeof(Host) == sizeof(lHost), "a Host is a host name pointer");

   /**
    * @brief Everything known at compile time about one field
    *
    * @tparam F the field number, e.g. `JB_job_number`
    */
   template <auto F>
      requires TypedField<decltype(F)>
   struct FieldTraits {
      /** @brief The struct of the field's object type */
      using Type = typename TypeOf<decltype(F)>::type;
      /** @brief The struct of the sub-type, void if the model names none */
      using Sub = typename SubType<F>::type;
      /** @brief The first field number of the type */
      static constexpr int base = Type::fields[0].nm;
      /** @brief The position of the field in every descriptor of the type, full or reduced */
      static constexpr int pos = static_cast<int>(F) - base;
      /** @brief The field's entry in the field table */
      static constexpr const FieldInfo &info = Type::fields[pos];
      /** @brief The field type */
      static constexpr FieldType type = info.type;
      /** @brief Whether the field is hashed */
      static constexpr bool hashed = has_flag(info.flags, Flag::Hash);
      /** @brief Whether the field's values are unique within a list */
      static constexpr bool unique = has_flag(info.flags, Flag::Unique);
      /** @brief Whether the model names a sub-type for the list or object field */
      static constexpr bool has_sub = !std::is_void_v<Sub>;

      static_assert(pos >= 0 && static_cast<std::size_t>(pos) < std::size(Type::fields),
                    "the field number lies outside its type");
      static_assert(info.nm == static_cast<int>(F), "the field table does not hold the field at its position");
   };

   /** @brief The field table of one object type, as listed in `ocs_tom_all.h` */
   struct TypeTable {
      const char *name;            ///< the name of the type, e.g. "JB"
      const lDescr *descr;         ///< the descriptor derived from the table
      const FieldInfo *fields;     ///< the field table
      std::size_t size;            ///< number of fields
   };

   /** @brief The sub-type of one list or object field, as listed in `ocs_tom_all.h` */
   struct SubTypeEntry {
      int nm;                      ///< the field number
      const lDescr *sub;           ///< the descriptor of the sub-type
   };

} // namespace ocs::tom
