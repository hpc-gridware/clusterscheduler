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
 * @brief The spooling record of a field, classified at compile time
 *
 * Every row a walker sees points at an #ocs_spool_field_ops record: a row of a typed field table
 * (spool/flatfile/ocs_spool_fields.h) at #ocs_spool_ops of its field, a row built at run time at the record
 * in the per-type tables of ocs_spool_type_ops.cc. Both are made by ocs_spool_make_ops() here, from the field
 * alone and at compile time: the field type and the sub-type from the model, the symbolic kind (sgeobj's
 * ocs_object_symbolic_of()), the JSON rendering, the JSON names and the complex type of a value from the
 * consteval predicates below. A record is data only. The walkers read and write a field with the checked
 * run-time accessors of tom/ocs_Raw.h (ocs::tom::get<V>(), ocs::tom::set()), the value type chosen from the
 * record's field type or rendering kind; the shared ocs_spool_append_value() / ocs_spool_parse_value() of
 * ocs_spool_field_info.cc do so for the flatfile text, so no code is instantiated per field.
 *
 * Part of the base spooling library: no header of a backend is needed here.
 */

#include <cstdint>

#include "tom/ocs_Types.h"

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_userset.h"

#include "spool/sge_spooling_utilities.h"

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
 * @brief Whether a field is written as a word rather than in the default form of its type
 *
 * The fields of a symbolic kind (ocs_object_symbolic_of()): the four enumerations of the complex entry and the
 * queue and userset type bit fields. Evaluated at compile time only, for #ocs_spool_ops.
 *
 * @param nm the field
 * @return true if the field has a rendering of its own
 */
consteval bool ocs_spool_is_by_name(const int nm) {
   return ocs_object_symbolic_of(nm) != ocs_object_symbolic::none;
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
 * Bit i has the name names[i], e.g. qtype -> ["BATCH","INTERACTIVE"], userset type -> ["ACL","DEPT"]; the names
 * are the constant arrays of sgeobj the bit parser and the flatfile rendering use as well.
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
 * @brief Whether a string field is an RBAC permission rule list, written in JSON as an array of rule objects
 *
 * The stored value is a comma-separated list of rules of six colon-separated characteristics each
 * (ocs::Role::parse_perm_list(), sge_role(5)); the JSON walkers split and join it.
 *
 * @param nm the field
 * @return true for the permission list of a role
 */
consteval bool ocs_spool_is_perm_list(const int nm) {
   return nm == RL_perm_list;
}

/**
 * @brief How the JSON walkers render a field
 *
 * The kinds are tested in the precedence the JSON writer and reader have always had: date-time, bit field,
 * enumeration, permission list, by name, complex typed, number. A field rendered by name by the flatfile walkers
 * that is a bit field (QU_qtype, US_type, AQTLIST_value) therefore is a bit field in JSON, and the JSON kind
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
   if (ocs_spool_is_perm_list(nm)) {
      return ocs_spool_render::perm_list;
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
 * @brief The names a JSON bit field or enumeration is rendered with
 * @param nm the field
 * @return the nullptr terminated names, nullptr for other fields
 */
consteval const char *const *ocs_spool_json_names(const int nm) {
   const char *const *names = ocs_spool_bitfield_names(nm);
   return names != nullptr ? names : ocs_spool_enum_names(nm);
}


/**
 * @brief The spooling record of a field
 *
 * The symbolic and the JSON rendering kinds, the names and the complex type are classified here once, from the
 * field alone. Evaluated at compile time only: the result is a constant, no code is instantiated per field.
 *
 * @tparam F the field
 * @return the record
 */
template <auto F>
consteval ocs_spool_field_ops ocs_spool_make_ops() {
   using Tr = ocs::tom::FieldTraits<F>;
   using ocs::tom::FieldType;

   // the JSON walkers read the value type the kind implies with the checked accessors
   constexpr ocs_spool_render json = ocs_spool_json_render(F);
   static_assert(json != ocs_spool_render::datetime || Tr::type == FieldType::Uint64,
                 "a date-time field is a Uint64");
   static_assert((json != ocs_spool_render::bitfield && json != ocs_spool_render::enumeration) ||
                 Tr::type == FieldType::Uint32, "a bit field or an enumeration is a Uint32");
   static_assert((json != ocs_spool_render::avalue && json != ocs_spool_render::number) ||
                 Tr::type == FieldType::String, "a complex typed or numeric value is a String");
   static_assert(json != ocs_spool_render::perm_list || Tr::type == FieldType::String,
                 "the JSON walkers read the permission list as a String");
   static_assert(static_cast<std::uint32_t>(ocs_spool_avalue_type(F)) <= UINT8_MAX,
                 "the complex type of a value fits the record");

   // the walkers read the value of a field of a symbolic kind as an unsigned 32 bit number
   constexpr ocs_object_symbolic symbolic = ocs_object_symbolic_of(static_cast<int>(F));
   static_assert(symbolic == ocs_object_symbolic::none || Tr::type == FieldType::Uint32,
                 "a field of a symbolic kind is a Uint32");

   return {static_cast<int>(F),
           Tr::type,
           symbolic,
           json,
           static_cast<std::uint8_t>(ocs_spool_avalue_type(F)),
           ocs_spool_sub_descr<F>(),
           ocs_spool_json_names(F)};
}

/**
 * @brief The spooling record of a field, one constant per field a typed field table names
 *
 * The per-type tables of ocs_spool_type_ops.cc hold records made by the same ocs_spool_make_ops(), so a row
 * built at run time and a row of a typed table behave alike.
 *
 * @tparam F the field
 */
template <auto F>
inline constexpr ocs_spool_field_ops ocs_spool_ops = ocs_spool_make_ops<F>();

/**
 * @brief The complex type of a JSON #ocs_spool_render::avalue field
 * @param ops the record of the field
 * @return the type, ocs::CEntry::Type::NONE for other fields
 */
inline ocs::CEntry::Type ocs_spool_avalue_type_of(const ocs_spool_field_ops &ops) {
   return static_cast<ocs::CEntry::Type>(ops.avalue_type);
}
