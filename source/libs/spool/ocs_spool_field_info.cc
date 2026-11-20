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
 * @brief The shared value conversion of the flatfile walkers
 *
 * ocs_spool_append_value() and ocs_spool_parse_value() convert the value of a field that is neither a list nor an
 * object to and from the text of a flatfile, with the record's field type and its symbolic kind.
 *
 * The field is read and written with the checked run-time accessors of tom/ocs_Raw.h, the value type chosen from
 * the record's field type, so the code here exists once for all fields of all object types. Type fidelity is
 * guaranteed when a row is built: a record is made by ocs_spool_make_ops() from the model's field table for exactly
 * its field, so the value type chosen from it is the field's. Both functions test with ocs::tom::has() that the
 * element holds the field before they touch it and report a field it does not hold, as before; the accessors
 * therefore never throw for a missing field here. The walkers run in the worker threads of qmaster and at its
 * start.
 *
 * Called per field by the walkers, so the functions have no DENTER, as the walkers' per field helpers have none.
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "uti/sge.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"

#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/ocs_spool_field_info.h"

#include "msg_common.h"

/**
 * @brief Append the value of a field as the flatfile walkers write it
 *
 * A number, a bool, a string or a host name is read with the checked accessor of the record's field type and
 * written in the default form of its type, a string or a host name that is not set as `NONE`; a field of a symbolic
 * kind (#ocs_spool_field_ops::symbolic) is written as a word by ocs_object_append_symbolic(). Nothing is quoted. A
 * list, an object or a reference field has no such value. Shared by the walkers, which take the record from the
 * row, and by every other caller that knows a field only by its number (ocs_spool_field_ops_of()). No DENTER, as
 * the walkers' per field helpers have none.
 *
 * @param ops the record of the field
 * @param ep the element, maybe empty
 * @param[out] answer_list receives the message for an empty element or an element that does not hold the field
 * @param[out] buffer receives the text, appended
 * @return the buffer's text; nullptr for a list, an object or a reference field, and on an error
 */
const char *
ocs_spool_append_value(const ocs_spool_field_ops &ops, const ocs::tom::ConstRawElement ep,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *buffer) {
   if (!ep) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_NULLELEMENTPASSEDTO_S,
                              __func__);
      return nullptr;
   }
   const int nm = ops.nm;
   if (!ep.has(nm)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_NMNOTINELEMENT_S,
                              ocs::tom::field_name(nm));
      return nullptr;
   }
   if (!ops.has_value()) {
      return nullptr;
   }

   // the element holds the field, and the value type is the record's field type: the reads cannot throw
   if (ops.symbolic != ocs_object_symbolic::none) {
      return ocs_object_append_symbolic(buffer, ops.symbolic, ep.get<std::uint32_t>(nm));
   }
   switch (ops.type) {
      case ocs::tom::FieldType::Uint32:
         return sge_dstring_sprintf_append(buffer, sge_u32, ep.get<std::uint32_t>(nm));
      case ocs::tom::FieldType::Uint64:
         return sge_dstring_sprintf_append(buffer, sge_u64, ep.get<std::uint64_t>(nm));
      case ocs::tom::FieldType::Int32:
         return sge_dstring_sprintf_append(buffer, "%d", ep.get<std::int32_t>(nm));
      case ocs::tom::FieldType::Int64:
         return sge_dstring_sprintf_append(buffer, "%ld", static_cast<long>(ep.get<std::int64_t>(nm)));
      case ocs::tom::FieldType::Double:
         return sge_dstring_sprintf_append(buffer, "%lf", ep.get<double>(nm));
      case ocs::tom::FieldType::Bool:
         return sge_dstring_append(buffer, ep.get<bool>(nm) ? TRUE_STR : FALSE_STR);
      case ocs::tom::FieldType::Host: {
         const char *host = ep.get<ocs::tom::Host>(nm).c_str();
         return sge_dstring_append(buffer, host != nullptr ? host : NONE_STR);
      }
      default: {
         // a string, the only other field with a plain value
         const char *text = ep.get<const char *>(nm);
         return sge_dstring_append(buffer, text != nullptr ? text : NONE_STR);
      }
   }
}

/**
 * @brief Set the value of a field from the text the flatfile walkers read
 *
 * The counterpart of ocs_spool_append_value(): a field of a symbolic kind is parsed by
 * ocs_object_parse_symbolic(), which also decides whether a value is stored after a failure; a string holding a
 * value of a complex type (the time, interval and memory attributes of a cluster queue,
 * ocs_spool_avalue_type_of()) is validated and stored verbatim; anything else is parsed by the parser of its type,
 * a string or a host name is stored as it is. A list, an object or a reference field is not set from a text, which
 * is no error. The value is stored with the checked setter of the record's field type. No DENTER, as the walkers'
 * per field helpers have none.
 *
 * @param ops the record of the field
 * @param ep the element to change, maybe empty
 * @param[out] answer_list receives the messages
 * @param value the text
 * @return true on success, false on an error, described in the answer list for most fields
 * @throw ocs::tom::FieldException with reason OutOfMemory if a string cannot be copied, or in a debug build
 *        DuplicateKey for the key of a unique field another element of the element's list holds
 */
bool
ocs_spool_parse_value(const ocs_spool_field_ops &ops, const ocs::tom::RawElement ep,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value) {
   if (!ep) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_NULLELEMENTPASSEDTO_S,
                              __func__);
      return false;
   }
   const int nm = ops.nm;
   if (!ep.has(nm)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_NMNOTINELEMENT_S,
                              ocs::tom::field_name(nm));
      return false;
   }
   if (!ops.has_value()) {
      return true;
   }

   // the element holds the field, and the value type is the record's field type (a symbolic kind is a Uint32,
   // a complex typed value a String, both asserted by ocs_spool_make_ops())
   if (ops.symbolic != ocs_object_symbolic::none) {
      const ocs_object_parsed_symbol symbol = ocs_object_parse_symbolic(answer_list, ops.symbolic, value);
      if (symbol.store) {
         ep.set(nm, symbol.value);
      }
      return symbol.ok;
   }
   if (const ocs::CEntry::Type value_type = ocs_spool_avalue_type_of(ops); value_type != ocs::CEntry::Type::NONE) {
      if (!ocs_object_check_value_string(answer_list, value_type, value)) {
         return false;
      }
      ep.set(nm, value);
      return true;
   }

   switch (ops.type) {
      case ocs::tom::FieldType::Uint32:
         if (const auto v = ocs_object_parse_uint32(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Uint64:
         if (const auto v = ocs_object_parse_uint64(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Int32:
         if (const auto v = ocs_object_parse_int32(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Int64:
         if (const auto v = ocs_object_parse_int64(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Double:
         if (const auto v = ocs_object_parse_double(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Bool:
         if (const auto v = ocs_object_parse_bool(answer_list, value); v) {
            ep.set(nm, *v);
            return true;
         }
         return false;
      case ocs::tom::FieldType::Host:
         ep.set(nm, ocs::tom::Host(value));
         return true;
      default:
         // a string, the only other field with a plain value
         ep.set(nm, value);
         return true;
   }
}
