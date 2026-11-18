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
 * @brief The flatfile spooling field tables as compile-time typed tables
 *
 * A field table says which fields of an object type the flatfile writer and reader handle, under which
 * name, with which width, how a sublist is laid out and which callbacks render or parse a field. Every
 * table is a tuple of #OcsSpoolField entries; an entry carries its field and its sub-table as template
 * arguments, so its typed callbacks take the element as `ocs::tom::Element<T>` and the code touching the
 * field's value is compiled per field from ocs::tom::FieldTraits.
 *
 * The writer and the reader in sge_flatfile.cc walk a #spooling_field array, because their callers select,
 * copy, align and patch the rows at run time (qconf_copy_fields(), spool_flatfile_align_list(), the
 * builders in sge_flatfile_obj.cc, the views of libs/procedure). #ocs_spool_view turns a typed table into
 * that array at compile time - a pack expansion over the tuple - and gives every row a pointer to the
 * #ocs_spool_field_ops of its field, which is where the walkers find the typed code. A row built at run
 * time from a descriptor (spool_get_fields_to_spool()) or from a field number (the share tree's recursion
 * row) takes the same constant from ocs_spool_field_ops_of(), so every row the walkers see has its ops.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basis_types.h"

#include "uti/sge.h"
#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_userset.h"

#include "spool/sge_spooling_utilities.h"

struct spool_flatfile_instr;

/** @brief The type of #ocs_spool_no_table, the sub-table of an entry without one */
struct ocs_spool_no_table_t {};

/** @brief The sub-table of an entry without one */
inline constexpr ocs_spool_no_table_t ocs_spool_no_table{};

/**
 * @brief One row of a typed field table
 *
 * @tparam F the field, of the object type of the table
 * @tparam Sub the table its sublist is written and read with, #ocs_spool_no_table for none; a table may name a
 *             sub-table for a field that is not a list, as the old arrays did - the walkers ignore it there
 */
template <auto F, const auto &Sub = ocs_spool_no_table>
struct OcsSpoolField {
   /// the object type of the field
   using type = typename ocs::tom::FieldTraits<F>::Type;
   /// a callback parsing the field's value, returns false if the value is rejected
   using read_fn = bool (*)(ocs::tom::Element<type>, const char *, ocs::tom::ListPtr<ocs::tom::AN> *);
   /// a callback rendering the field's value, returns false if nothing was rendered
   using write_fn = bool (*)(ocs::tom::ConstElement<type>, dstring *, ocs::tom::ListPtr<ocs::tom::AN> *);

   /// the field
   static constexpr auto field = F;
   /// the sub-table
   static constexpr const auto &sub_table = Sub;
   /// whether the entry names a sub-table
   static constexpr bool has_sub_table = !std::is_same_v<std::remove_cvref_t<decltype(Sub)>, ocs_spool_no_table_t>;

   const char *name;                                ///< the name in the text, nullptr for a nameless field
   int width;                                       ///< the width of the name column, 0 for unaligned
   const spool_flatfile_instr *sub_instr = nullptr; ///< the layout of the sublist, nullptr for the default
   read_fn read = nullptr;                          ///< parses the value instead of the default conversion
   write_fn write = nullptr;                        ///< renders the value instead of the default conversion
   unsigned need = 0;                               ///< the variant bits a builder must select for the row
};

/**
 * @brief Read the value of a field that is neither a list nor an object nor a reference
 * @tparam F the field
 * @param ep the element, holding the field
 * @return the value, in the member the field's type names
 */
template <auto F>
ocs_spool_value ocs_spool_get_value(const lListElem *ep) {
   using Tr = ocs::tom::FieldTraits<F>;
   using ocs::tom::FieldType;

   // the walker calls this only for an element that holds the field; the handle checks the element's type
   const auto object = ocs::tom::ConstElement<typename Tr::Type>(ep);
   ocs_spool_value value{};
   if constexpr (Tr::type == FieldType::Uint32) {
      value.u32 = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::Uint64) {
      value.u64 = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::Int32) {
      value.i32 = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::Int64) {
      value.i64 = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::Double) {
      value.dbl = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::Bool) {
      value.b = object.template get<F>();
   } else if constexpr (Tr::type == FieldType::String) {
      value.str = object.template get<F>();
   } else {
      static_assert(Tr::type == FieldType::Host, "a plain value is a number, a bool, a string or a host name");
      const ocs::tom::Host host = object.template get<F>();
      value.str = host.c_str();
   }
   return value;
}

/**
 * @brief Store the value of a field that is neither a list nor an object nor a reference
 * @tparam F the field
 * @param ep the element, holding the field
 * @param value the value, in the member the field's type names; a string is copied
 */
template <auto F>
void ocs_spool_set_value(lListElem *ep, const ocs_spool_value value) {
   using Tr = ocs::tom::FieldTraits<F>;
   using ocs::tom::FieldType;

   // the walker calls this only for an element that holds the field; the handle checks the element's type
   const auto object = ocs::tom::Element<typename Tr::Type>(ep);
   if constexpr (Tr::type == FieldType::Uint32) {
      object.template set<F>(value.u32);
   } else if constexpr (Tr::type == FieldType::Uint64) {
      object.template set<F>(value.u64);
   } else if constexpr (Tr::type == FieldType::Int32) {
      object.template set<F>(value.i32);
   } else if constexpr (Tr::type == FieldType::Int64) {
      object.template set<F>(value.i64);
   } else if constexpr (Tr::type == FieldType::Double) {
      object.template set<F>(value.dbl);
   } else if constexpr (Tr::type == FieldType::Bool) {
      object.template set<F>(value.b);
   } else if constexpr (Tr::type == FieldType::String) {
      object.template set<F>(value.str);
   } else {
      static_assert(Tr::type == FieldType::Host, "a plain value is a number, a bool, a string or a host name");
      object.template set<F>(ocs::tom::Host{value.str});
   }
}

/**
 * @brief The list of a list field
 * @tparam F the list field
 * @param ep the element, holding the field
 * @return the list, nullptr if unset
 */
template <auto F>
const lList *ocs_spool_get_list(const lListElem *ep) {
   // the walker calls this only for an element that holds the field; the handle checks the element's type
   return ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type>(ep).template get<F>();
}

/**
 * @brief Store a list for a list field
 * @tparam F the list field
 * @param ep the element, holding the field
 * @param list the list, of the field's sub-type as the reader created it, or nullptr; taken over
 */
template <auto F>
void ocs_spool_set_list(lListElem *ep, lList *list) {
   using Tr = ocs::tom::FieldTraits<F>;
   // the walker calls this only for an element that holds the field; the handle checks the element's type
   const auto object = ocs::tom::Element<typename Tr::Type>(ep);
   if constexpr (Tr::has_sub) {
      // the reader created the list with the sub-type's descriptor
      object.template set<F>(ocs::tom::ListPtr<typename Tr::Sub>::from_typed(list));
   } else {
      object.template set<F>(list);
   }
}

/**
 * @brief The element of an object field
 * @tparam F the object field
 * @param ep the element, holding the field
 * @return the element, nullptr if unset
 */
template <auto F>
const lListElem *ocs_spool_get_object(const lListElem *ep) {
   // the walker calls this only for an element that holds the field; the handle checks the element's type
   return ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type>(ep).template get<F>();
}

/**
 * @brief Store an element for an object field
 * @tparam F the object field
 * @param ep the element, holding the field
 * @param object the element, of the field's sub-type as the reader created it and bound nowhere; taken over
 */
template <auto F>
void ocs_spool_set_object(lListElem *ep, lListElem *object) {
   using Tr = ocs::tom::FieldTraits<F>;
   // the walker calls this only for an element that holds the field; the handle checks the element's type
   const auto holder = ocs::tom::Element<typename Tr::Type>(ep);
   if constexpr (Tr::has_sub) {
      // the reader created the element with the sub-type's descriptor
      holder.template set<F>(ocs::tom::ElementPtr<typename Tr::Sub>::from_typed(object));
   } else {
      holder.template set<F>(object);
   }
}

/**
 * @brief The descriptor of the sub-type of a list or an object field
 * @tparam F the field
 * @return the descriptor, nullptr for a field that is neither or has no sub-type
 */
template <auto F>
consteval const lDescr *ocs_spool_sub_descr() {
   using Tr = ocs::tom::FieldTraits<F>;
   if constexpr ((Tr::type == ocs::tom::FieldType::List || Tr::type == ocs::tom::FieldType::Object) && Tr::has_sub) {
      return Tr::Sub::descriptor.e;
   } else {
      return nullptr;
   }
}

/**
 * @brief The names of the sharetree node types (`STN_type`), indexed by the value
 *
 * The order must match the `STT_*` constants of sgeobj/sge_sharetree.h (STT_USER = 0, STT_PROJECT = 1).
 */
inline constexpr const char *const ocs_sharetree_node_types[] = {"USER", "PROJECT", nullptr};

/**
 * @brief The names of the queue sort methods of the scheduler (`SC_queue_sort_method`), indexed by the value
 *
 * The order must match QSM_LOAD = 0, QSM_SEQNUM = 1; the names are the tokens of write_SC_queue_sort_method().
 */
inline constexpr const char *const ocs_sched_queue_sort_methods[] = {"load", "seqno", nullptr};

/**
 * @brief Whether object_append_field_to_dstring() renders a field by its name rather than by its type
 *
 * The special cases of that function and of object_parse_field_from_string() for the fields that are
 * neither lists nor objects; the list valued special cases there are never rendered as a value by the
 * walkers, which write a list through its sub-table. Evaluated at compile time only, for #ocs_spool_ops.
 *
 * @param nm the field
 * @return true if the field has a rendering of its own
 */
consteval bool ocs_spool_is_by_name(const int nm) {
   switch (nm) {
      case CE_valtype:
      case CE_relop:
      case CE_requestable:
      case CE_consumable:
      case QU_qtype:
      case US_type:
      case AQTLIST_value:
         return true;
      default:
         return false;
   }
}

/**
 * @brief Whether a field is an absolute time stamp (gmt64), rendered in JSON as a local ISO date-time
 * @param nm the field
 * @return true for a date-time field
 */
consteval bool ocs_spool_is_datetime(const int nm) {
   switch (nm) {
      case UU_delete_time:
         return true;
      default:
         return false;
   }
}

/**
 * @brief The bit names of a bit field, rendered in JSON as an array of the names of the set bits
 *
 * Bit i has the name names[i], e.g. qtype -> ["BATCH","INTERACTIVE"], userset type -> ["ACL","DEPT"].
 * Only the address of the sgeobj arrays is taken, so they need not be constexpr.
 *
 * @param nm the field
 * @return the nullptr terminated names, nullptr if the field is not a bit field
 */
consteval const char *const *ocs_spool_bitfield_names(const int nm) {
   switch (nm) {
      case QU_qtype:
      case AQTLIST_value:
         return queue_types;
      case US_type:
         return userset_types;
      default:
         return nullptr;
   }
}

/**
 * @brief The value names of an enumeration, rendered in JSON as the name of its value
 *
 * The value is the index into the names (e.g. STN_type 0 -> "USER"). The flatfile writer renders these
 * fields through per-field write callbacks that JSON does not use, so the mapping is kept here.
 *
 * @param nm the field
 * @return the nullptr terminated names, nullptr if the field is not such an enumeration
 */
consteval const char *const *ocs_spool_enum_names(const int nm) {
   switch (nm) {
      case STN_type:
         return ocs_sharetree_node_types;
      case SC_queue_sort_method:
         return ocs_sched_queue_sort_methods;
      default:
         return nullptr;
   }
}

/**
 * @brief The complex type of a string field holding a value of that type (a cluster queue override)
 *
 * ATIME/AINTER values are times (seconds), AMEM values are memory (bytes); JSON types them accordingly.
 *
 * @param nm the field
 * @return ocs::CEntry::Type::TIME or ocs::CEntry::Type::MEM, ocs::CEntry::Type::NONE for other fields
 */
consteval ocs::CEntry::Type ocs_spool_avalue_type(const int nm) {
   switch (nm) {
      case ATIME_value:
      case AINTER_value:
         return ocs::CEntry::Type::TIME;
      case AMEM_value:
         return ocs::CEntry::Type::MEM;
      default:
         return ocs::CEntry::Type::NONE;
   }
}

/**
 * @brief Whether a string field always holds a number, written in JSON as a native number
 *
 * The JSON reader converts the number back to its decimal string through the default conversion.
 *
 * @param nm the field
 * @return true for such a field
 */
consteval bool ocs_spool_is_number(const int nm) {
   switch (nm) {
      case CE_urgency_weight:    // always a finite double
      case ASTR_value:           // cqueue string override (e.g. priority), a number if numeric
      case PE_allocation_rule:   // "$pe_slots"/... or a fixed slot count
      case PE_urgency_slots:     // "min"/"max"/"avg" or a slot count
         return true;
      default:
         return false;
   }
}

/**
 * @brief How the JSON walkers render a field
 *
 * The kinds are tested in the precedence the JSON writer has always had: date-time, bit field,
 * enumeration, by name, complex typed, number. A field rendered by name by the flatfile walkers that is
 * a bit field (QU_qtype, US_type, AQTLIST_value) therefore is a bit field in JSON, and the JSON kind
 * #ocs_spool_render::by_name is left with the four CE fields.
 *
 * @param nm the field
 * @return the kind
 */
consteval ocs_spool_render ocs_spool_json_render(const int nm) {
   if (ocs_spool_is_datetime(nm)) {
      return ocs_spool_render::datetime;
   }
   if (ocs_spool_bitfield_names(nm) != nullptr) {
      return ocs_spool_render::bitfield;
   }
   if (ocs_spool_enum_names(nm) != nullptr) {
      return ocs_spool_render::enumeration;
   }
   if (ocs_spool_is_by_name(nm)) {
      return ocs_spool_render::by_name;
   }
   if (ocs_spool_avalue_type(nm) != ocs::CEntry::Type::NONE) {
      return ocs_spool_render::avalue;
   }
   if (ocs_spool_is_number(nm)) {
      return ocs_spool_render::number;
   }
   return ocs_spool_render::plain;
}

/**
 * @brief How the flatfile walkers render a field
 * @param nm the field
 * @return #ocs_spool_render::by_name for the special cases of object_append_field_to_dstring(),
 *         #ocs_spool_render::plain otherwise
 */
consteval ocs_spool_render ocs_spool_flat_render(const int nm) {
   return ocs_spool_is_by_name(nm) ? ocs_spool_render::by_name : ocs_spool_render::plain;
}

/**
 * @brief The names a JSON bit field or enumeration is rendered with
 * @param nm the field
 * @return the nullptr terminated names, nullptr for other fields
 */
consteval const char *const *ocs_spool_json_names(const int nm) {
   const char *const *names = ocs_spool_bitfield_names(nm);
   return names != nullptr ? names : ocs_spool_enum_names(nm);
}

/**
 * @brief The typed code of a field, instantiated only for the operations its type has
 *
 * The rendering kinds, the names and the complex type are classified here once, from the field alone.
 *
 * @tparam F the field
 * @return the operations
 */
template <auto F>
consteval ocs_spool_field_ops ocs_spool_make_ops() {
   using Tr = ocs::tom::FieldTraits<F>;
   using ocs::tom::FieldType;

   ocs_spool_field_ops ops{Tr::type,
                           ocs_spool_sub_descr<F>(),
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           ocs_spool_flat_render(F),
                           ocs_spool_json_render(F),
                           ocs_spool_json_names(F),
                           ocs_spool_avalue_type(F)};
   // the JSON walkers read the member of ocs_spool_value the kind implies
   constexpr ocs_spool_render json = ocs_spool_json_render(F);
   static_assert(json != ocs_spool_render::datetime || Tr::type == FieldType::Uint64,
                 "a date-time field is a Uint64");
   static_assert((json != ocs_spool_render::bitfield && json != ocs_spool_render::enumeration) ||
                 Tr::type == FieldType::Uint32, "a bit field or an enumeration is a Uint32");
   static_assert((json != ocs_spool_render::avalue && json != ocs_spool_render::number) ||
                 Tr::type == FieldType::String, "a complex typed or numeric value is a String");
   static_assert(static_cast<int>(F) != RL_perm_list || Tr::type == FieldType::String,
                 "the JSON walkers read the permission list as a String");
   if constexpr (Tr::type == FieldType::List) {
      ops.get_list = &ocs_spool_get_list<F>;
      ops.set_list = &ocs_spool_set_list<F>;
   } else if constexpr (Tr::type == FieldType::Object) {
      ops.get_object = &ocs_spool_get_object<F>;
      ops.set_object = &ocs_spool_set_object<F>;
   } else if constexpr (Tr::type != FieldType::Ref) {
      ops.get = &ocs_spool_get_value<F>;
      ops.set = &ocs_spool_set_value<F>;
   }
   return ops;
}

/**
 * @brief The typed code of a field, one constant per field
 * @tparam F the field
 */
template <auto F>
inline constexpr ocs_spool_field_ops ocs_spool_ops = ocs_spool_make_ops<F>();

/**
 * @brief A typed read callback in the signature of #spooling_field::read_func
 *
 * The field number is not read: the callback knows its field.
 *
 * @tparam T the object type of the callback's field
 * @tparam Cb the typed callback
 * @param ep the element, holding the callback's field
 * @param nm the field, unused
 * @param buffer the text of the value
 * @param answer_list receives the messages
 * @return 1 if the value was accepted, 0 otherwise
 */
template <typename T, auto Cb>
int ocs_spool_read_thunk(lListElem *ep, [[maybe_unused]] const int nm, const char *buffer,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   // the walkers call a row's callback only for an element holding the row's field; checked as the old callbacks were
   return Cb(ocs::tom::Element<T>(ep), buffer, answer_list) ? 1 : 0;
}

/**
 * @brief A typed write callback in the signature of #spooling_field::write_func
 *
 * The field number is not read: the callback knows its field.
 *
 * @tparam T the object type of the callback's field
 * @tparam Cb the typed callback
 * @param ep the element, holding the callback's field
 * @param nm the field, unused
 * @param buffer receives the text, appended
 * @param answer_list receives the messages
 * @return 1 if something was rendered, 0 otherwise
 */
template <typename T, auto Cb>
int ocs_spool_write_thunk(const lListElem *ep, [[maybe_unused]] const int nm, dstring *buffer,
                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   // the walkers call a row's callback only for an element holding the row's field; checked as the old callbacks were
   return Cb(ocs::tom::ConstElement<T>(ep), buffer, answer_list) ? 1 : 0;
}

/**
 * @brief The number of entries of a typed table
 * @tparam Table the table, a tuple of OcsSpoolField entries
 */
template <const auto &Table>
inline constexpr std::size_t ocs_spool_table_size = std::tuple_size_v<std::remove_cvref_t<decltype(Table)>>;

template <const auto &Table>
constexpr spooling_field *ocs_spool_view_rows();

/**
 * @brief The row of one entry of a typed table
 * @tparam Table the table
 * @tparam I the index of the entry
 * @return the row; it owns neither its name nor its sub-table
 */
template <const auto &Table, std::size_t I>
consteval spooling_field ocs_spool_make_row() {
   constexpr const auto &entry = std::get<I>(Table);
   using E = std::remove_cvref_t<decltype(entry)>;

   spooling_field row{};
   row.nm = E::field;
   row.width = entry.width;
   row.name = entry.name;
   row.free_name = false;
   if constexpr (E::has_sub_table) {
      row.sub_fields = ocs_spool_view_rows<E::sub_table>();
   }
   row.free_sub_fields = false;
   row.clientdata = entry.sub_instr;
   if constexpr (entry.read != nullptr) {
      row.read_func = &ocs_spool_read_thunk<typename E::type, entry.read>;
   }
   if constexpr (entry.write != nullptr) {
      row.write_func = &ocs_spool_write_thunk<typename E::type, entry.write>;
   }
   row.ops = &ocs_spool_ops<E::field>;
   return row;
}

/**
 * @brief The rows of a typed table, terminated by a `NoName` row
 *
 * The terminating row has the width of the first entry, as the hand written arrays had.
 *
 * @tparam Table the table
 * @tparam I the indices of its entries
 * @return the rows
 */
template <const auto &Table, std::size_t... I>
consteval std::array<spooling_field, sizeof...(I) + 1> ocs_spool_make_view(std::index_sequence<I...>) {
   spooling_field end{};
   end.nm = NoName;
   if constexpr (sizeof...(I) > 0) {
      end.width = std::get<0>(Table).width;
   }
   return {ocs_spool_make_row<Table, I>()..., end};
}

/**
 * @brief The run-time view of a typed table: the #spooling_field array its run-time users take
 *
 * Constant initialised, so it exists before any static constructor runs. It is mutable because the callers
 * align the shared arrays in place, as they did with the hand written ones.
 *
 * @tparam Table the table
 */
template <const auto &Table>
constinit std::array<spooling_field, ocs_spool_table_size<Table> + 1> ocs_spool_view =
      ocs_spool_make_view<Table>(std::make_index_sequence<ocs_spool_table_size<Table>>{});

/**
 * @brief The rows of the run-time view of a typed table
 *
 * Declared before the view so that a row can point at the view of its sub-table.
 *
 * @tparam Table the table
 * @return the first row
 */
template <const auto &Table>
constexpr spooling_field *ocs_spool_view_rows() {
   return ocs_spool_view<Table>.data();
}

/**
 * @brief The variant bits each entry of a typed table needs
 * @tparam Table the table
 * @tparam I the indices of its entries
 * @return the bits, in the order of the entries
 */
template <const auto &Table, std::size_t... I>
consteval std::array<unsigned, sizeof...(I)> ocs_spool_make_needs(std::index_sequence<I...>) {
   return {std::get<I>(Table).need...};
}

/**
 * @brief The variant bits each entry of a typed table needs, in the order of the entries
 * @tparam Table the table, a tuple of OcsSpoolField entries
 */
template <const auto &Table>
inline constexpr std::array<unsigned, ocs_spool_table_size<Table>> ocs_spool_needs =
      ocs_spool_make_needs<Table>(std::make_index_sequence<ocs_spool_table_size<Table>>{});

/**
 * @brief The rows of a table view a variant selects, as a heap array (see the definition)
 * @param view the rows of the table, terminated by a NoName row
 * @param needs the variant bits each row needs
 * @param size the number of rows without the terminator
 * @param variant the variant bits the caller has
 * @return the selected rows and the terminator, allocated with sge_malloc()
 */
spooling_field *ocs_spool_select_rows(const spooling_field *view, const unsigned *needs, size_t size,
                                      unsigned variant);

/**
 * @brief The rows of a typed table a variant selects, as a heap array
 *
 * A row is selected when the variant holds every bit its entry needs; the order of the table is kept.
 *
 * @tparam Table the table
 * @param variant the variant bits
 * @return the rows and the terminating row, to be freed with #spool_free_spooling_fields or sge_free()
 */
template <const auto &Table>
spooling_field *ocs_spool_select(const unsigned variant) {
   return ocs_spool_select_rows(ocs_spool_view<Table>.data(), ocs_spool_needs<Table>.data(),
                                ocs_spool_table_size<Table>, variant);
}
