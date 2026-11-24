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
 * @brief Rendering objects as JSON, for `qconf -fmt json`
 */

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/sge_string.h"              /* sge_strnullcasecmp */
#include "uti/sge.h"                     /* NONE_STR */
#include "uti/sge_parse_num_par.h"       /* parse_ulong_val for typed override values */
#include <cfloat>                        /* DBL_MAX (INFINITY sentinel) */
#include "uti/ocs_JsonUtil.h"            /* rapidjson Writer + write_json helpers */
#include <rapidjson/prettywriter.h>      /* indented (pretty) JSON output */
#include <rapidjson/document.h>          /* JSON parsing (reader) */

#include "tom/ocs_Types.h"

#include "sgeobj/sge_object.h"           /* object_get_type_name, the symbolic value renderer */
#include "sgeobj/sge_attr.h"             /* HOSTREF_DEFAULT ("@/") */
#include "sgeobj/sge_centry.h"           /* CE / CE_* for complex-value typing */
#include "sgeobj/sge_ulong.h"            /* double_print_to_dstring (compact time/mem) */
#include "uti/sge_time.h"                /* sge_ctime64 / sge_gmt32_to_gmt64 (ISO date-time) */
#include <ctime>                         /* struct tm, mktime (ISO date-time parse) */
#include "sgeobj/sge_conf.h"             /* config_param_list_type (config list params) */
#include "sgeobj/ocs_CEntry.h"           /* ocs::CEntry::Type */
#include "sgeobj/sge_answer.h"           /* answer_list_add_sprintf */
#include "sgeobj/config.h"               /* add_nm_to_set */
#include "sgeobj/sge_utility.h"          /* SGE_CHECK_POINTER_FALSE */
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_CE.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/ocs_spool_fields.h"
#include "spool/flatfile/msg_spoollib_flatfile.h"

#include "msg_common.h"                  /* MSG_NULLELEMENTPASSEDTO_S (SGE_CHECK_POINTER_FALSE) */

#include "ocs_spool_json.h"

/** @def JSON_LAYER
 * @brief The rmon layer this module traces on
 */
#define JSON_LAYER BASIS_LAYER

/** @brief The value format in effect
 *
 * Defaults to the compact unit and colon strings for TIME and MEM; see
 * #ocs_json_value_format.
 */
ocs_json_value_format ocs_json_value_format_opt = OCS_JSON_VALUES_COMPACT;

/* forward declarations (mutually recursive sublist handling) */
static bool
spool_json_write_object_members(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstRawElement object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields);
static bool
spool_json_write_list_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstRawList list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields);
static bool
spool_json_write_object_braced(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstRawElement object,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const spooling_field *fields);
static bool
spool_json_write_positional_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstRawList list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static bool
spool_json_write_list_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstRawList sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static bool spool_json_value_to_u32(const rapidjson::Value &v, std::uint32_t *out);

/* the JSON token shown for the cqueue default host reference HOSTREF_DEFAULT ("@/") */
static const char *const SPOOL_JSON_DEFAULT_HREF = "default";

/**
 * @brief Lowercase object type name (e.g. "CALENDAR" -> "calendar").
 *
 * Used for the JSON $id and the list wrapper key.
 *
 * @param object  the object whose type name is resolved
 * @param out     buffer receiving the lowercased name
 * @return the lowercased type name (pointer into @p out)
 */
static const char *
spool_json_typename(const ocs::tom::ConstRawElement object, dstring *out) {
   const char *type_name = object_get_type_name(object.raw());
   sge_dstring_clear(out);
   for (const char *c = type_name; c != nullptr && *c != '\0'; c++) {
      sge_dstring_append_char(out, (char)tolower((unsigned char)*c));
   }
   return sge_dstring_get_string(out);
}

/**
 * @brief Write the "$schema"/"$id" members for an already-resolved type name.
 *
 * Mirrors qstat's convention; the $id is derived from @p type_name.
 *
 * @param writer     pretty-printer to emit into
 * @param type_name  lowercased object type name used to build the $id URL
 */
static void
spool_json_write_envelope_name(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const char *type_name) {
   writer.Key("$schema");
   writer.String("https://json-schema.org/draft/2020-12/schema");

   dstring id = DSTRING_INIT;
   sge_dstring_sprintf(&id,
      "https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/"
      "source/dist/util/resources/json-schemas/v9.2/ocs-qconf-%s.schema.json",
      type_name);
   writer.Key("$id");
   writer.String(sge_dstring_get_string(&id));
   sge_dstring_free(&id);
}

/**
 * @brief Write the "$schema"/"$id" envelope members for an object's resolved type.
 *
 * @param writer  pretty-printer to emit into
 * @param object  object whose type name drives the $id
 */
static void
spool_json_write_envelope(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const ocs::tom::ConstRawElement object) {
   dstring name = DSTRING_INIT;
   spool_json_write_envelope_name(writer, spool_json_typename(object, &name));
   sge_dstring_free(&name);
}

/**
 * @brief Read a string or a host name field as a C string, for an element that holds the field.
 *
 * @param ep    the element; it holds the field
 * @param nm    the field number
 * @param type  the field's type, FieldType::String or FieldType::Host
 * @return the value, nullptr if unset
 * @throw ocs::tom::FieldException if the element does not hold the field or @p type is not its type
 */
static const char *
spool_json_get_text(const ocs::tom::ConstRawElement ep, const int nm, const ocs::tom::FieldType type) {
   if (type == ocs::tom::FieldType::Host) {
      return ep.get<ocs::tom::Host>(nm).c_str();
   }
   return ep.get<const char *>(nm);
}

/**
 * @brief Read a string or a host name field as a C string, nullptr if the element does not hold it.
 *
 * The soft read: a null element, a field of another type or a field a reduced element
 * leaves out all give nullptr, as an unset value does.
 *
 * @param ep    the element, maybe empty
 * @param nm    the field number
 * @param type  the field's type, FieldType::String or FieldType::Host
 * @return the value, nullptr if unset or not held
 * @throw ocs::tom::FieldException with reason WrongType if the element holds the field with another type than
 *        @p type names (a string type for any @p type other than FieldType::Host)
 */
static const char *
spool_json_get_text_or_null(const ocs::tom::ConstRawElement ep, const int nm, const ocs::tom::FieldType type) {
   if (type == ocs::tom::FieldType::Host) {
      return ep.get_or<ocs::tom::Host>(nm, ocs::tom::Host{}).c_str();
   }
   return ep.get_or<const char *>(nm, nullptr);
}

/**
 * @brief Write one scalar field value (no key) typed from the field's type.
 *
 * The value is read with the checked accessor of the field's type; a list, an object or
 * a reference field has no scalar value and is written as null. Both callers write the
 * value only after testing that the element holds the field.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field; it holds the field
 * @param field   the row of the field
 */
static void
spool_json_write_scalar_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const ocs::tom::ConstRawElement object, const spooling_field &field) {
   if (!field.ops->has_value()) {
      writer.Null();
      return;
   }
   const int nm = field.nm;
   switch (field.ops->type) {
      case ocs::tom::FieldType::Double: writer.Double(object.get<double>(nm)); break;
      case ocs::tom::FieldType::Uint32: writer.Uint64(object.get<std::uint32_t>(nm)); break;
      case ocs::tom::FieldType::Uint64: writer.Uint64(object.get<std::uint64_t>(nm)); break;
      case ocs::tom::FieldType::Int64:  writer.Int64(object.get<std::int64_t>(nm)); break;
      case ocs::tom::FieldType::Int32:  writer.Int(object.get<std::int32_t>(nm)); break;
      case ocs::tom::FieldType::Bool:   writer.Bool(object.get<bool>(nm)); break;
      default: {
         /* a string or a host name, the only other fields with a get operation.
            unset string -> empty string. null, "" and the "NONE" sentinel all mean
            unset, so they render uniformly as "" instead of leaking the internal "NONE"
            token into JSON. The reader maps "" back to the "NONE" sentinel string (not
            C-null), mirroring the flatfile reader, so a required field whose unset value
            is "NONE" (e.g. a ckpt interface's command fields) round-trips correctly. */
         const char *s = spool_json_get_text(object, nm, field.ops->type);
         writer.String((s == nullptr || sge_strnullcasecmp(s, NONE_STR) == 0) ? "" : s);
         break;
      }
   }
}

/**
 * @brief Write an already-parsed numeric complex value honouring the value format.
 *
 * TIME/MEM: COMPACT -> unit/colon string ("2.000G", "0:5:0"), NUMERIC -> bytes/seconds
 * number. INT -> integer and DOUBLE -> double in either mode. Unlimited (DBL_MAX) ->
 * "INFINITY" string (JSON has no infinity literal), so a TIME/MEM value is a
 * number|"INFINITY" union in NUMERIC mode.
 *
 * @param writer  pretty-printer to emit into
 * @param dval    the numeric value
 * @param type    the complex entry type that selects the rendering
 */
static void
spool_json_write_typed_number(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              double dval, ocs::CEntry::Type type) {
   if (dval == DBL_MAX) {
      writer.String(INFINITY_STR);
      return;
   }
   const bool compact = (ocs_json_value_format_opt == OCS_JSON_VALUES_COMPACT);
   if (compact && (type == ocs::CEntry::Type::TIME || type == ocs::CEntry::Type::MEM)) {
      dstring buf = DSTRING_INIT;
      double_print_to_dstring(dval, &buf, type);
      writer.String(sge_dstring_get_string(&buf));
      sge_dstring_free(&buf);
   } else if (type == ocs::CEntry::Type::DOUBLE) {
      writer.Double(dval);
   } else {
      writer.Uint64(static_cast<uint64_t>(dval));
   }
}

/**
 * @brief Write a complex-entry value typed per CE_valtype.
 *
 * MEM/TIME/INT/DOUBLE -> typed value (compact or numeric, see
 * spool_json_write_typed_number), BOOL -> bool; string-like and unfilled (NONE)
 * values fall back to the raw CE_stringval.
 *
 * @param writer  pretty-printer to emit into
 * @param ce      a complex-entry element
 */
static void
spool_json_write_ce_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const ocs::tom::ConstElement<ocs::tom::CE> ce) {
   DENTER(JSON_LAYER);
   const double dval = ce.get<CE_doubleval>();
   const auto type = static_cast<ocs::CEntry::Type>(ce.get<CE_valtype>());
   switch (type) {
      case ocs::CEntry::Type::DOUBLE:
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::TIME:
      case ocs::CEntry::Type::MEM:
         spool_json_write_typed_number(writer, dval, type);
         break;
      case ocs::CEntry::Type::BOOL:
         writer.Bool(dval > 0.0);
         break;
      default: {
         /* STR/CSTR/HOST/RESTR/RSMAP and NONE (unfilled config): raw string value */
         const char *s = ce.get<CE_stringval>();
         s != nullptr ? writer.String(s) : writer.Null();
         break;
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Write a complex-value / threshold sublist as a JSON array.
 *
 * Renders the complex entries as { "name": <name>, "value": <typed> } objects.
 *
 * @param writer  pretty-printer to emit into
 * @param list    list of complex-entry elements, may be reduced
 */
static void
spool_json_write_ce_array(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const ocs::tom::ConstList<ocs::tom::CE> list) {
   DENTER(JSON_LAYER);
   writer.StartArray();
   for (const ocs::tom::ConstElement<ocs::tom::CE> ce : list) {
      writer.StartObject();
      const char *cname = ce.get<CE_name>();
      writer.Key("name");
      cname != nullptr ? writer.String(cname) : writer.Null();
      writer.Key("value");
      spool_json_write_ce_value(writer, ce);
      writer.EndObject();
   }
   writer.EndArray();
   DRETURN_VOID;
}

/**
 * @brief Test whether a sublist holds complex-entry (CE) elements.
 *
 * Detected by the presence of the CE name/value/type fields rather than by descriptor
 * pointer identity - essential because a GDI-transported list carries a rebuilt
 * descriptor that is not the canonical CE pointer.
 *
 * @param list  the list to inspect, not empty
 * @return true if the list's descriptor carries the CE name/value/type fields
 */
static bool
spool_json_is_ce_list(const ocs::tom::ConstRawList list) {
   // present_pos() is the lookup lGetPosInDescr() wraps: a field the (reduced) descriptor leaves out is a miss
   const ocs::tom::FieldDescr *d = list.descr();
   return d != nullptr &&
          ocs::tom::present_pos(d, CE_name) >= 0 &&
          ocs::tom::present_pos(d, CE_stringval) >= 0 &&
          ocs::tom::present_pos(d, CE_valtype) >= 0;
}

/**
 * @brief Write a field as its symbolic string token.
 *
 * Used for the fields of the JSON kind ocs_spool_render::by_name (#ocs_spool_field_ops::json), the
 * enumerations of the complex entry; the word is the one of the field's symbolic kind
 * (#ocs_spool_field_ops::symbolic), as the flatfile writer writes it.
 *
 * A field of a symbolic kind is a Uint32 (asserted where the record is made). Both
 * callers write the value only after testing that the element holds the field.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field; it holds the field
 * @param ops     the record of the field
 */
static void
spool_json_write_symbolic_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const ocs::tom::ConstRawElement object, const ocs_spool_field_ops &ops) {
   dstring buf = DSTRING_INIT;
   const char *s = ocs_object_append_symbolic(&buf, ops.symbolic, object.get<std::uint32_t>(ops.nm));
   s != nullptr ? writer.String(s) : writer.Null();
   sge_dstring_free(&buf);
}

/**
 * @brief Write a string-stored numeric field as a native JSON number.
 *
 * Integral values are written as an integer, others as a double. A null, empty, or
 * "NONE" token -> ""; any other token that does not parse fully to a finite number
 * (e.g. "INFINITY") -> the raw string.
 *
 * @param writer  pretty-printer to emit into
 * @param s       the value of the string field that is semantically numeric
 */
static void
spool_json_write_number_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer, const char *s) {
   if (s == nullptr || *s == '\0' || sge_strnullcasecmp(s, NONE_STR) == 0) {
      writer.String("");
      return;
   }
   char *end = nullptr;
   const double d = strtod(s, &end);
   if (end != s && *end == '\0' && std::isfinite(d) && d == static_cast<double>(static_cast<long long>(d))) {
      writer.Int64(static_cast<long long>(d));
   } else if (end != s && *end == '\0' && std::isfinite(d)) {
      writer.Double(d);
   } else {
      writer.String(s);   /* INFINITY and other non-numeric tokens kept verbatim */
   }
}

/**
 * @brief Write a string-stored, complex-typed override value (e.g. a time/memory limit).
 *
 * null/empty/"NONE" -> ""; a value that does not parse for this type -> the raw string;
 * otherwise it is typed per the selected value format (compact unit/colon string or
 * numeric seconds/bytes; INFINITY -> "INFINITY").
 *
 * @param writer  pretty-printer to emit into
 * @param s       the raw string value
 * @param type    complex entry type that drives parsing/rendering
 */
static void
spool_json_write_avalue(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                        const char *s, ocs::CEntry::Type type) {
   if (s == nullptr || *s == '\0' || sge_strnullcasecmp(s, NONE_STR) == 0) {
      writer.String("");
      return;
   }
   double dval = 0.0;
   if (parse_ulong_val(&dval, nullptr, type, s, nullptr, 0)) {
      spool_json_write_typed_number(writer, dval, type);
   } else {
      writer.String(s);   /* not parseable for this type -> raw string */
   }
}

/**
 * @brief Write a bitfield value as a JSON array of the set bit names.
 *
 * @param writer  pretty-printer to emit into
 * @param value   the bitmask
 * @param names   null-terminated bit-name table (names[i] is the name of bit i)
 */
static void
spool_json_write_bitfield_array(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                uint32_t value, const char *const *names) {
   writer.StartArray();
   uint32_t bitmask = 1;
   for (const char *const *ptr = names; *ptr != nullptr; ptr++) {
      if (bitmask & value) {
         writer.String(*ptr);
      }
      bitmask <<= 1;
   }
   writer.EndArray();
}

/**
 * @brief Write a single-valued enum as its symbolic token.
 *
 * @param writer  pretty-printer to emit into
 * @param value   the enum value (index into @p names)
 * @param names   null-terminated value-name table
 */
static void
spool_json_write_enum_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            uint32_t value, const char *const *names) {
   uint32_t count = 0;
   while (names[count] != nullptr) {
      count++;
   }
   if (value < count) {
      writer.String(names[value]);
   } else {
      writer.Uint64(value);   /* out-of-range value -> keep the raw number */
   }
}

/**
 * @brief Parse a single-valued enum token (or raw number) back into its value.
 *
 * The inverse of spool_json_write_enum_value; a string is matched case-insensitively
 * against the table, a number is taken as the value directly.
 *
 * @param v      the JSON value (symbolic token or number)
 * @param names  null-terminated value-name table
 * @return the enum value
 */
static uint32_t
spool_json_enum_from_value(const rapidjson::Value &v, const char *const *names) {
   if (v.IsString()) {
      for (uint32_t i = 0; names[i] != nullptr; i++) {
         if (sge_strnullcasecmp(v.GetString(), names[i]) == 0) {
            return i;
         }
      }
   }
   std::uint32_t value = 0;   /* numeric form; an unmatched string or a number out of range gives 0 */
   static_cast<void>(spool_json_value_to_u32(v, &value));
   return value;
}

/**
 * @brief Write a gmt64 timestamp as a local ISO date-time string.
 *
 * @param writer  pretty-printer to emit into
 * @param ts      timestamp in gmt64 microseconds since epoch; 0 -> "" (unset)
 */
static void
spool_json_write_datetime(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer, uint64_t ts) {
   if (ts == 0) {
      writer.String("");   /* unset (sge_ctime64(0) would print "now") */
      return;
   }
   dstring buf = DSTRING_INIT;
   sge_ctime64(ts, &buf, true, false);   /* is_xml -> ISO, no microseconds */
   writer.String(sge_dstring_get_string(&buf));
   sge_dstring_free(&buf);
}

/**
 * @brief Parse a local ISO date-time string back into a gmt64 microsecond timestamp.
 *
 * Accepts both the "2026-06-13T14:30:13" and the space-separated form.
 *
 * @param s  the ISO date-time string
 * @return the gmt64 timestamp, or 0 for an empty/unparsable input
 */
static uint64_t
spool_json_datetime_from_string(const char *s) {
   if (s == nullptr || *s == '\0') {
      return 0;
   }
   int y, mo, d, h, mi, sec;
   if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) == 6 ||
       sscanf(s, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &sec) == 6) {
      struct tm tm{};
      tm.tm_year = y - 1900;
      tm.tm_mon = mo - 1;
      tm.tm_mday = d;
      tm.tm_hour = h;
      tm.tm_min = mi;
      tm.tm_sec = sec;
      tm.tm_isdst = -1;
      const time_t t = mktime(&tm);   /* local time -> seconds (inverse of localtime_r) */
      if (t != static_cast<time_t>(-1)) {
         return sge_gmt32_to_gmt64(static_cast<uint32_t>(t));
      }
   }
   return 0;
}

/**
 * @brief Write the scalar value of a field, dispatching on its typing.
 *
 * Routes on the JSON rendering kind of the field (#ocs_spool_field_ops::json) to the
 * date-time, bitfield, enum, symbolic, complex-typed, numeric, or plain scalar branch
 * (a permission list takes the plain branch, as it always did here);
 * a field the element does not hold -> null. The value is read with the checked
 * accessor of the field's type; the kinds are bound to the field types at compile time
 * (date-time is a Uint64, bitfield and enum are Uint32, complex-typed and numeric are
 * strings), so the value type a branch reads is the one the field's type names.
 *
 * @param writer  pretty-printer to emit into
 * @param ep      element holding the field, maybe empty (written as null)
 * @param field   the row of the field to write
 */
static void
spool_json_write_field_scalar(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const ocs::tom::ConstRawElement ep, const spooling_field &field) {
   const int nm = field.nm;
   if (!ep.has(nm)) {
      writer.Null();
      return;
   }
   const ocs_spool_field_ops &ops = *field.ops;
   switch (ops.json) {
      case ocs_spool_render::datetime:
         spool_json_write_datetime(writer, ep.get<std::uint64_t>(nm));
         break;
      case ocs_spool_render::bitfield:
         spool_json_write_bitfield_array(writer, ep.get<std::uint32_t>(nm), ops.names);
         break;
      case ocs_spool_render::enumeration:
         spool_json_write_enum_value(writer, ep.get<std::uint32_t>(nm), ops.names);
         break;
      case ocs_spool_render::by_name:
         spool_json_write_symbolic_value(writer, ep, ops);
         break;
      case ocs_spool_render::avalue:
         spool_json_write_avalue(writer, ep.get<const char *>(nm), ocs_spool_avalue_type_of(ops));
         break;
      case ocs_spool_render::number:
         spool_json_write_number_value(writer, ep.get<const char *>(nm));
         break;
      case ocs_spool_render::perm_list:   // a permission list is not a positional value: written as before
      case ocs_spool_render::plain:
         spool_json_write_scalar_value(writer, ep, field);
         break;
   }
}

/**
 * @brief Write a positional element's "name" field (its first spooling field).
 *
 * For a host-reference name the default marker HOSTREF_DEFAULT ("@/") is shown as
 * "default" (it represents the queue/host default value).
 *
 * @param writer  pretty-printer to emit into
 * @param ep      the positional element
 * @param field   the row of the element's name field
 */
static void
spool_json_write_positional_name(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                 const ocs::tom::ConstRawElement ep, const spooling_field &field) {
   if (field.ops->type == ocs::tom::FieldType::Host && ep.has(field.nm)) {
      const char *h = ep.get<ocs::tom::Host>(field.nm).c_str();
      if (h != nullptr && strcmp(h, HOSTREF_DEFAULT) == 0) {
         writer.String(SPOOL_JSON_DEFAULT_HREF);
      } else {
         writer.String((h != nullptr && *h != '\0') ? h : "");
      }
   } else {
      spool_json_write_field_scalar(writer, ep, field);
   }
}

/**
 * @brief Write a "positional" sublist (unnamed spooling fields) as a JSON array.
 *
 * The names live in the values (e.g. load_values, load_scaling, user_lists, and the
 * cqueue per-host override lists). A single field becomes an array of scalar values
 * (["a","b"]); two or more fields become an array of
 * { "name": <field0>, "value": <field1> } objects. The value (field1) may itself be a
 * list (e.g. the cqueue load_thresholds value is a complex list) -> it is rendered as
 * a nested array (empty -> []) rather than null.
 *
 * The list's elements are of the sub-type of the list field the rows belong to, known
 * here only as the rows' descriptor; the fields are read through the checked run-time accessors.
 *
 * @param answer_list  for returning errors
 * @param list         the positional sublist
 * @param fields       the (unnamed) spooling fields of the sublist
 * @param writer       pretty-printer to emit into
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_positional_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer) {
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   writer.StartArray();
   for (const ocs::tom::ConstRawElement ep : list) {
      if (nfields <= 1) {
         spool_json_write_positional_name(writer, ep, fields[0]);
      } else {
         writer.StartObject();
         writer.Key("name");
         spool_json_write_positional_name(writer, ep, fields[0]);
         writer.Key("value");
         if (fields[1].ops->type == ocs::tom::FieldType::List && ep.has(fields[1].nm)) {
            if (!spool_json_write_list_value(answer_list, ep.get<ocs::tom::ConstRawList>(fields[1].nm),
                                             fields[1].sub_fields, writer)) {
               return false;
            }
         } else {
            spool_json_write_field_scalar(writer, ep, fields[1]);
         }
         writer.EndObject();
      }
   }
   writer.EndArray();
   return true;
}

/**
 * @brief Render a list field's value as a JSON array, choosing the shape from contents.
 *
 * A CE/complex list -> typed {name,value}; a positional (unnamed) sublist -> name-only
 * or {name,value}; a named sublist -> array of keyed objects; an empty or absent
 * list -> [].
 *
 * @param answer_list  for returning errors
 * @param sub_list     the list field value, maybe empty (written as [])
 * @param sub_fields   spooling fields describing the sublist elements
 * @param writer       pretty-printer to emit into
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_list_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer) {
   if (sub_list.empty()) {   // an empty handle as well
      writer.StartArray();
      writer.EndArray();
   } else if (spool_json_is_ce_list(sub_list)) {
      // a list with the complex-entry fields holds complex entries (CE), possibly reduced; checked as the
      // getters checked each element before
      spool_json_write_ce_array(writer, ocs::tom::ConstList<ocs::tom::CE>(sub_list.raw()));
   } else if (sub_fields != nullptr && sub_fields[0].nm != NoName && sub_fields[0].name == nullptr) {
      if (!spool_json_write_positional_array(answer_list, sub_list, sub_fields, writer)) {
         return false;
      }
   } else if (sub_fields != nullptr) {
      if (!spool_json_write_list_array(answer_list, sub_list, writer, sub_fields)) {
         return false;
      }
   } else {
      writer.StartArray();
      writer.EndArray();
   }
   return true;
}

/**
 * @brief Render a single config value (a scalar or a name=value entry's value).
 *
 * null/empty/"NONE" -> "", "true"/"false" -> bool, a known INT/TIME scalar type ->
 * number/time, otherwise a string. Booleans are detected by value, so every true/false
 * config value is typed regardless of its parameter name.
 *
 * @param writer  pretty-printer to emit into
 * @param val     the raw config value string
 * @param type    known complex type of the parameter (INT/TIME), or NONE
 */
static void
spool_json_write_config_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const char *val, ocs::CEntry::Type type) {
   if (val == nullptr || *val == '\0' || sge_strnullcasecmp(val, NONE_STR) == 0) {
      writer.String("");
   } else if (sge_strnullcasecmp(val, "true") == 0) {
      writer.Bool(true);
   } else if (sge_strnullcasecmp(val, "false") == 0) {
      writer.Bool(false);
   } else if (type == ocs::CEntry::Type::INT || type == ocs::CEntry::Type::TIME) {
      spool_json_write_avalue(writer, val, type);
   } else {
      /* a pure number -> JSON number (e.g. PTF_MIN_PRIORITY=20); a colon-time, a value
       * with a unit, or any other token stays a string. */
      char *end = nullptr;
      const double d = strtod(val, &end);
      if (end != val && *end == '\0' && std::isfinite(d)) {
         if (d == static_cast<double>(static_cast<long long>(d))) {
            writer.Int64(static_cast<long long>(d));
         } else {
            writer.Double(d);
         }
      } else {
         writer.String(val);
      }
   }
}

/**
 * @brief Split a " \t,"-delimited config value into a JSON array of strings.
 *
 * Used for list-valued config params (e.g. user_lists); a null/empty/"NONE" value
 * becomes [].
 *
 * @param writer  pretty-printer to emit into
 * @param s       the raw delimited config value
 */
static void
spool_json_write_config_value_list(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                   const char *s) {
   writer.StartArray();
   if (s != nullptr && *s != '\0' && sge_strnullcasecmp(s, NONE_STR) != 0) {
      char *buf = strdup(s);
      struct saved_vars_s *ctx = nullptr;
      for (const char *tok = sge_strtok_r(buf, " \t,", &ctx); tok != nullptr;
           tok = sge_strtok_r(nullptr, " \t,", &ctx)) {
         writer.String(tok);
      }
      sge_free_saved_vars(ctx);
      free(buf);
   }
   writer.EndArray();
}

/**
 * @brief Split a ", "-delimited config KEY=VALUE list into a JSON array of objects.
 *
 * Used for name=value config params (e.g. execd_params); each entry becomes a
 * { "name": KEY, "value": VALUE } object. A null/empty/"NONE" value becomes [].
 *
 * @param writer  pretty-printer to emit into
 * @param s       the raw delimited KEY=VALUE config value
 */
static void
spool_json_write_config_namevalue_list(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                       const char *s) {
   writer.StartArray();
   if (s != nullptr && *s != '\0' && sge_strnullcasecmp(s, NONE_STR) != 0) {
      char *buf = strdup(s);
      struct saved_vars_s *ctx = nullptr;
      for (const char *tok = sge_strtok_r(buf, ", ", &ctx); tok != nullptr;
           tok = sge_strtok_r(nullptr, ", ", &ctx)) {
         const char *eq = strchr(tok, '=');
         writer.StartObject();
         writer.Key("name");
         if (eq != nullptr) {
            writer.String(tok, static_cast<rapidjson::SizeType>(eq - tok));
            writer.Key("value");
            spool_json_write_config_value(writer, eq + 1, ocs::CEntry::Type::NONE);
         } else {
            writer.String(tok);
            writer.Key("value");
            writer.String("");
         }
         writer.EndObject();
      }
      sge_free_saved_vars(ctx);
      free(buf);
   }
   writer.EndArray();
}

/* The six colon-separated characteristics of an RBAC permission rule (RL_perm_list),
 * in order. The last (value_constraint) is everything after the fifth colon, so it may
 * itself contain colons. See ocs::Role::parse_perm_list / sge_role(5). */
static const char *const perm_rule_fields[] = {
   "source", "origin", "operation", "object_type", "object_key", "value_constraint"
};

/**
 * @brief Write a perm_list value as a JSON array of structured rule objects.
 *
 * The stored value is a comma-separated list of rules, each rule six colon-separated
 * fields (the sixth may contain further colons). Each rule becomes a
 * { "source", "origin", "operation", "object_type", "object_key", "value_constraint" }
 * object. A null/empty/"NONE" value (the empty rule set) becomes [].
 *
 * @param writer  pretty-printer to emit into
 * @param s       the raw comma/colon-delimited perm_list string
 */
static void
spool_json_write_perm_list(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                           const char *s) {
   writer.StartArray();
   if (s != nullptr && *s != '\0' && sge_strnullcasecmp(s, NONE_STR) != 0) {
      char *buf = strdup(s);
      struct saved_vars_s *ctx = nullptr;
      for (const char *rule = sge_strtok_r(buf, ",", &ctx); rule != nullptr;
           rule = sge_strtok_r(nullptr, ",", &ctx)) {
         writer.StartObject();
         const char *p = rule;
         /* first five fields are split on the next colon; the sixth is the remainder */
         for (int i = 0; i < 5; i++) {
            const char *colon = strchr(p, ':');
            writer.Key(perm_rule_fields[i]);
            if (colon != nullptr) {
               writer.String(p, static_cast<rapidjson::SizeType>(colon - p));
               p = colon + 1;
            } else {
               writer.String(p);
               p = "";   /* malformed (fewer than 6 fields): remaining fields empty */
            }
         }
         writer.Key(perm_rule_fields[5]);
         writer.String(p);
         writer.EndObject();
      }
      sge_free_saved_vars(ctx);
      free(buf);
   }
   writer.EndArray();
}

/**
 * @brief Parse a perm_list JSON array of rule objects back into the stored string.
 *
 * The inverse of spool_json_write_perm_list: each rule object's six fields are joined
 * with colons and the rules with commas. An empty array maps to the "NONE" sentinel
 * (the empty rule set), which qmaster accepts. A non-array value (e.g. a bare string)
 * is rejected here rather than silently coerced to NONE, which would wipe the rule set.
 * Malformed rules within a well-formed array (missing/empty fields) are caught later by
 * ocs::Role::parse_perm_list on the qmaster side.
 *
 * @param v            the JSON array of rule objects
 * @param out          dstring receiving the joined perm_list string
 * @param answer_list  receives an error if @p v is not an array
 * @return true on success (@p out set), false if @p v is not a JSON array
 */
static bool
spool_json_perm_list_from_array(const rapidjson::Value &v, dstring *out, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   sge_dstring_clear(out);
   if (!v.IsArray()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_ROLE_PERMLIST_NOTARRAY);
      return false;
   }
   if (v.Size() == 0) {
      sge_dstring_append(out, NONE_STR);
      return true;
   }
   for (rapidjson::SizeType i = 0; i < v.Size(); i++) {
      if (i > 0) {
         sge_dstring_append_char(out, ',');
      }
      const rapidjson::Value &rule = v[i];
      for (int f = 0; f < 6; f++) {
         if (f > 0) {
            sge_dstring_append_char(out, ':');
         }
         if (rule.IsObject()) {
            const auto m = rule.FindMember(perm_rule_fields[f]);
            if (m != rule.MemberEnd() && m->value.IsString()) {
               sge_dstring_append(out, m->value.GetString());
            }
         }
      }
   }
   return true;
}

/**
 * @brief Emit the "key": value members of one object (no surrounding braces).
 *
 * Members are keyed by the spooling_field names and typed from the field types.
 * Scalars become native JSON values; CE/complex-value sublists become arrays of typed
 * {name,value}; name-only sublists become arrays of strings; other sublists become
 * arrays of objects. A top-level unnamed name=value sublist (the config entries) is
 * flattened into "<param>": <value> members.
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize, maybe empty (an error)
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the object
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_object_members(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawElement object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields) {
   DENTER(JSON_LAYER);

   SGE_CHECK_POINTER_FALSE(object.raw(), answer_list);
   SGE_CHECK_POINTER_FALSE(fields, answer_list);

   for (int i = 0; fields[i].nm != NoName; i++) {
      const spooling_field &field = fields[i];
      const int nm = field.nm;
      const char *name = field.name;

      /* JSON needs a key. A top-level unnamed name=value sublist (the config entries,
         CONF_entries) is flattened: each "<name>": <value> becomes a member of the
         object, mirroring the flat "parameter value" config format. Other unnamed
         (structural) fields are skipped. */
      if (name == nullptr) {
         const spooling_field *sf = field.sub_fields;
         if (object.has(nm) && field.ops->type == ocs::tom::FieldType::List && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            const auto sub = object.get<ocs::tom::ConstRawList>(nm);
            for (const ocs::tom::ConstRawElement e : sub) {
               // the entries are not tested for their fields: one that does not hold one reads as unset
               const char *key = spool_json_get_text_or_null(e, sf[0].nm, sf[0].ops->type);
               if (key == nullptr) {
                  continue;
               }
               writer.Key(key);
               const char *value = spool_json_get_text_or_null(e, sf[1].nm, sf[1].ops->type);
               /* config list params render as arrays (grounded in the config loader's
                * parsing); everything else is a scalar string */
               switch (config_param_list_type(key)) {
                  case CONF_PARAM_VALUE_LIST:
                     spool_json_write_config_value_list(writer, value);
                     break;
                  case CONF_PARAM_NAMEVALUE_LIST:
                     spool_json_write_config_namevalue_list(writer, value);
                     break;
                  default:
                     /* scalar: typed by value (bool) or by the known INT/TIME type;
                      * "NONE"/empty -> ""; otherwise a string */
                     spool_json_write_config_value(writer, value, config_param_value_type(key));
                     break;
               }
            }
         }
         continue;
      }
      if (!object.has(nm)) {
         continue;
      }

      const ocs::tom::FieldType type = field.ops->type;
      if (type == ocs::tom::FieldType::List) {
         writer.Key(name);
         if (!spool_json_write_list_value(answer_list, object.get<ocs::tom::ConstRawList>(nm), field.sub_fields,
                                          writer)) {
            DRETURN(false);
         }
      } else if (type == ocs::tom::FieldType::Object && field.sub_fields != nullptr) {
         /* a sub-object (e.g. an RQS filter) -> nested JSON object built from its
            sub_fields. An unset (null) sub-object is still emitted as its full empty
            structure (rendered from a transient default element) so the JSON always
            shows the complete syntax, e.g. { "expand": false, "scope": [], "xscope":
            [] }. The reader maps empty arrays to null lists, so this round-trips to
            "no filter" (the matcher treats a null scope as "match all"). The sub-type
            is known here only as the row's descriptor, so the transient element is
            created from it. */
         writer.Key(name);
         auto sub_obj = object.get<ocs::tom::ConstRawElement>(nm);
         const ocs::tom::FieldDescr *sub_descr = field.ops->sub_descr;
         ocs::tom::RawElementPtr empty;   // the transient element, freed on every path
         if (!sub_obj && sub_descr != nullptr) {
            empty = ocs::tom::create_raw_elem(sub_descr);
            sub_obj = empty.get();
         }
         if (!sub_obj) {
            writer.Null();
         } else if (!spool_json_write_object_braced(answer_list, sub_obj, writer, field.sub_fields)) {
            DRETURN(false);
         }
      } else {
         /* the JSON kinds of the field are bound to its type at compile time (date-time is
            a Uint64, bitfield and enum are Uint32, numeric and the permission list are
            strings), so the value type a branch reads is the one the field's type names; a
            complex-typed value (ocs_spool_render::avalue) is written as a plain scalar
            here, as it always was */
         writer.Key(name);
         const ocs_spool_field_ops &ops = *field.ops;
         if (ops.json == ocs_spool_render::datetime) {
            spool_json_write_datetime(writer, object.get<std::uint64_t>(nm));
         } else if (ops.json == ocs_spool_render::bitfield) {
            spool_json_write_bitfield_array(writer, object.get<std::uint32_t>(nm), ops.names);
         } else if (ops.json == ocs_spool_render::enumeration) {
            spool_json_write_enum_value(writer, object.get<std::uint32_t>(nm), ops.names);
         } else if (ops.json == ocs_spool_render::perm_list) {
            spool_json_write_perm_list(writer, object.get<const char *>(nm));
         } else if (ops.json == ocs_spool_render::by_name) {
            spool_json_write_symbolic_value(writer, object, ops);
         } else if (ops.json == ocs_spool_render::number) {
            spool_json_write_number_value(writer, object.get<const char *>(nm));
         } else {
            spool_json_write_scalar_value(writer, object, field);
         }
      }
   }

   DRETURN(true);
}

/**
 * @brief Write one object as a braced JSON object (no envelope).
 *
 * Used for list/sublist elements.
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize, maybe empty (an error)
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the object
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_object_braced(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawElement object,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const spooling_field *fields) {
   writer.StartObject();
   bool ok = spool_json_write_object_members(answer_list, object, writer, fields);
   writer.EndObject();
   return ok;
}

/**
 * @brief Write a list of objects as a JSON array of braced objects.
 *
 * @param answer_list  for returning errors
 * @param list         the list to serialize, maybe empty
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the elements
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_list_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields) {
   DENTER(JSON_LAYER);

   writer.StartArray();
   for (const ocs::tom::ConstRawElement ep : list) {
      if (!spool_json_write_object_braced(answer_list, ep, writer, fields)) {
         DRETURN(false);
      }
   }
   writer.EndArray();

   DRETURN(true);
}

/**
 * @brief Serialize a single object to a pretty-printed JSON document with envelope.
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize, maybe empty (an error)
 * @param fields       spooling fields describing the object
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawElement object,
                             const spooling_field *fields, dstring *out) {
   return spool_json_write_object_ex(answer_list, object, fields, nullptr, out);
}

/**
 * @brief Serialize a single object, with derived keys beside its fields.
 *
 * The document is the one #spool_json_write_object produces, plus one key per
 * entry of @p derived written after the fields. Passing nullptr, or an array
 * whose first key is nullptr, therefore yields a byte-identical document -- which
 * is what lets a caller add the derived view only where it has something to say
 * and leave every other configuration untouched.
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize, maybe empty (an error)
 * @param fields       spooling fields describing the object
 * @param derived      derived keys, terminated by an entry whose key is nullptr;
 *                     may be nullptr
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_object_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawElement object,
                                const spooling_field *fields,
                                const ocs_json_derived_list *derived, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   writer.StartObject();
   spool_json_write_envelope(writer, object);
   bool ok = spool_json_write_object_members(answer_list, object, writer, fields);

   for (const ocs_json_derived_list *d = derived; ok && d != nullptr && d->key != nullptr; d++) {
      writer.Key(d->key);
      writer.StartArray();

      for (const ocs::tom::ConstRawElement ep : ocs::tom::ConstRawList(d->names)) {
         // a hard read, as lGetHost() was: every name element holds the host name field
         const char *name = ep.get<ocs::tom::Host>(d->keynm).c_str();

         writer.String(name != nullptr ? name : "");
      }
      writer.EndArray();
   }
   writer.EndObject();

   if (!ok) {
      DRETURN(false);
   }
   sge_dstring_append(out, sb.GetString());
   sge_dstring_append_char(out, '\n');
   DRETURN(true);
}

/**
 * @brief Serialize a list of objects to a pretty-printed JSON document with envelope.
 *
 * The array is keyed by the element type name (or "objects" for an empty list).
 *
 * @param answer_list  for returning errors
 * @param list         the list to serialize, maybe empty
 * @param fields       spooling fields describing the elements
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                           const spooling_field *fields, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   const ocs::tom::ConstRawElement first = list.front();   // empty for an empty list or an empty handle
   dstring key = DSTRING_INIT;

   writer.StartObject();
   if (first) {
      spool_json_write_envelope(writer, first);
      writer.Key(spool_json_typename(first, &key));
   } else {
      writer.Key("objects");
   }
   bool ok = spool_json_write_list_array(answer_list, list, writer, fields);
   writer.EndObject();
   sge_dstring_free(&key);

   if (!ok) {
      DRETURN(false);
   }
   sge_dstring_append(out, sb.GetString());
   sge_dstring_append_char(out, '\n');
   DRETURN(true);
}

/**
 * @brief Serialize a single object with an explicit type name for the envelope.
 *
 * For objects whose type cannot be resolved by content (e.g. a sharetree STN node,
 * whose descriptor has no distinct primary key) the explicit @p type_name drives the
 * $id instead of object_get_type_name().
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize, maybe empty (an error)
 * @param fields       spooling fields describing the object
 * @param type_name    explicit type name for the $id
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawElement object,
                                   const spooling_field *fields, const char *type_name, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   writer.StartObject();
   spool_json_write_envelope_name(writer, type_name);
   bool ok = spool_json_write_object_members(answer_list, object, writer, fields);
   writer.EndObject();

   if (!ok) {
      DRETURN(false);
   }
   sge_dstring_append(out, sb.GetString());
   sge_dstring_append_char(out, '\n');
   DRETURN(true);
}

/**
 * @brief Serialize a list with separate names for the $id schema and the array key.
 *
 * Most lists use the same token for both (see spool_json_write_typed_list), but an
 * object that is queried both as a list and individually needs a distinct list
 * schema: e.g. the categories use the array key "category" (the per-record object
 * type) but the "category-list" schema id, so the list does not collide with the
 * single-record "category" schema of "qconf -scat <id>".
 *
 * @param answer_list    for returning errors
 * @param list           the list to serialize (an empty handle yields an empty array)
 * @param fields         spooling fields describing the elements
 * @param id_name        type name for the $id (`ocs-qconf-<id_name>.schema.json`)
 * @param envelope_name  the array key wrapping the records
 * @param out            dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_list_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                                    const spooling_field *fields, const char *id_name,
                                    const char *envelope_name, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   writer.StartObject();
   spool_json_write_envelope_name(writer, id_name);
   writer.Key(envelope_name);
   bool ok = spool_json_write_list_array(answer_list, list, writer, fields);
   writer.EndObject();

   if (!ok) {
      DRETURN(false);
   }
   sge_dstring_append(out, sb.GetString());
   sge_dstring_append_char(out, '\n');
   DRETURN(true);
}

/**
 * @brief Serialize a list with an explicit type name for the envelope and array key.
 *
 * For lists whose element type is not a registered object (e.g. -stl thread pools).
 *
 * @param answer_list  for returning errors
 * @param list         the list to serialize, maybe empty
 * @param fields       spooling fields describing the elements
 * @param type_name    explicit type name for the $id and the array key
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                                 const spooling_field *fields, const char *type_name, dstring *out) {
   return spool_json_write_typed_list_ex(answer_list, list, fields, type_name, type_name, out);
}

/**
 * @brief Serialize a bare name list (e.g. managers, admin hosts) as a JSON array.
 *
 * Emits the @p keynm field of each element wrapped in the same $schema/$id envelope as
 * the object lists; comment lines (names starting with '#') are skipped, as the ASCII
 * shower does.
 *
 * @param answer_list  for returning errors
 * @param list         the list of name-bearing elements, maybe empty
 * @param keynm        field id of the name attribute
 * @param out          dstring the JSON document is appended to
 * @return true on success
 */
bool spool_json_write_name_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                                const int keynm, dstring *out) {
   return spool_json_write_name_list_ex(answer_list, list, keynm, nullptr, out);
}

/**
 * @brief Serialize a bare name list with an explicitly named envelope.
 *
 * Same output as spool_json_write_name_list(), except that @p type_name - when given -
 * names the envelope instead of it being derived from the first element's object type.
 * Needed where the elements are not a registered object type: object_get_type_name()
 * yields "unknown" for those, which both breaks the $id (there is no
 * ocs-qconf-unknown-list.schema.json) and makes two different lists indistinguishable.
 *
 * @param answer_list  for returning errors
 * @param list         the list of name-bearing elements, maybe empty
 * @param keynm        field id of the name attribute
 * @param type_name    envelope/$id name, or nullptr to derive it from the first element
 * @param out          dstring the JSON document is appended to
 * @return true on success
 */
bool spool_json_write_name_list_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstRawList list,
                                   const int keynm, const char *type_name, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   const ocs::tom::ConstRawElement first = list.front();   // empty for an empty list or an empty handle
   dstring key = DSTRING_INIT;

   /* same $schema/$id envelope as the object lists, but the array holds bare names */
   writer.StartObject();
   if (type_name != nullptr || first) {
      /* CS-2320: a name list carries its own "<type>-list" schema id so it is
       * distinguishable from the object schema and from a record list that shares
       * the same envelope key (e.g. -sc vs -scel, both "complex_entry"); the array
       * key itself stays the plain object type name. */
      if (type_name == nullptr) {
         type_name = spool_json_typename(first, &key);
      }
      dstring list_id = DSTRING_INIT;
      sge_dstring_sprintf(&list_id, "%s-list", type_name);
      spool_json_write_envelope_name(writer, sge_dstring_get_string(&list_id));
      sge_dstring_free(&list_id);
      writer.Key(type_name);
   } else {
      writer.Key("names");
   }
   writer.StartArray();
   // the name is a string or a host name field; a key of another type has no name to show
   const ocs_spool_field_ops *ops = ocs_spool_field_ops_of(keynm);
   const bool named = ops != nullptr &&
                      (ops->type == ocs::tom::FieldType::String || ops->type == ocs::tom::FieldType::Host);
   for (const ocs::tom::ConstRawElement ep : list) {
      if (!named || !ep.has(keynm)) {
         continue;
      }
      const char *s = spool_json_get_text(ep, keynm, ops->type);
      if (s != nullptr && s[0] != '#') {   /* skip comment lines, as the ASCII shower does */
         writer.String(s);
      }
   }
   writer.EndArray();
   writer.EndObject();
   sge_dstring_free(&key);

   sge_dstring_append(out, sb.GetString());
   sge_dstring_append_char(out, '\n');
   DRETURN(true);
}

/* ===== JSON reader (CS-2313a) ============================================= */

/**
 * @brief Coerce a JSON value to a string (for MT_STRING/MT_HOST fields).
 *
 * E.g. a typed complex-value number -> its decimal string, which qmaster re-types.
 *
 * @param v    the JSON value
 * @param out  buffer receiving the string form
 * @return the string form (pointer into @p out)
 */
static const char *
spool_json_value_to_string(const rapidjson::Value &v, dstring *out) {
   sge_dstring_clear(out);
   if (v.IsString()) {
      sge_dstring_append(out, v.GetString());
   } else if (v.IsBool()) {
      sge_dstring_append(out, v.GetBool() ? "true" : "false");
   } else if (v.IsUint64()) {
      sge_dstring_sprintf(out, "%llu", static_cast<unsigned long long>(v.GetUint64()));
   } else if (v.IsInt64()) {
      sge_dstring_sprintf(out, "%lld", static_cast<long long>(v.GetInt64()));
   } else if (v.IsDouble()) {
      sge_dstring_sprintf(out, "%g", v.GetDouble());
   }
   return sge_dstring_get_string(out);
}

/**
 * @brief Read a JSON value as an unsigned 64 bit integer
 *
 * A JSON integer, an integral float or a decimal string. null reads as 0, as an unset value is written.
 *
 * @param v    the JSON value
 * @param out  receives the value
 * @return true if the value is an integer in the range of the type, false otherwise (out unchanged)
 */
static bool
spool_json_value_to_u64(const rapidjson::Value &v, std::uint64_t *out) {
   if (v.IsNull()) {
      *out = 0;
      return true;
   }
   if (v.IsUint64()) {
      *out = v.GetUint64();
      return true;
   }
   if (v.IsInt64()) {   // a JSON integer that is not a Uint64 is negative
      return false;
   }
   if (v.IsDouble()) {
      const double d = v.GetDouble();
      if (d < 0.0 || d >= 18446744073709551616.0 || d != std::floor(d)) {
         return false;
      }
      *out = static_cast<std::uint64_t>(d);
      return true;
   }
   if (v.IsString()) {
      const char *text = v.GetString();
      char *end = nullptr;
      errno = 0;
      const unsigned long long value = strtoull(text, &end, 10);
      if (end == text || *end != '\0' || errno == ERANGE || strchr(text, '-') != nullptr) {
         return false;
      }
      *out = value;
      return true;
   }
   return false;
}

/**
 * @brief Read a JSON value as a signed 64 bit integer
 *
 * A JSON integer, an integral float or a decimal string. null reads as 0, as an unset value is written.
 *
 * @param v    the JSON value
 * @param out  receives the value
 * @return true if the value is an integer in the range of the type, false otherwise (out unchanged)
 */
static bool
spool_json_value_to_i64(const rapidjson::Value &v, std::int64_t *out) {
   if (v.IsNull()) {
      *out = 0;
      return true;
   }
   if (v.IsInt64()) {
      *out = v.GetInt64();
      return true;
   }
   if (v.IsUint64()) {   // a JSON integer that is not an Int64 is above its range
      return false;
   }
   if (v.IsDouble()) {
      const double d = v.GetDouble();
      if (d < -9223372036854775808.0 || d >= 9223372036854775808.0 || d != std::floor(d)) {
         return false;
      }
      *out = static_cast<std::int64_t>(d);
      return true;
   }
   if (v.IsString()) {
      const char *text = v.GetString();
      char *end = nullptr;
      errno = 0;
      const long long value = strtoll(text, &end, 10);
      if (end == text || *end != '\0' || errno == ERANGE) {
         return false;
      }
      *out = value;
      return true;
   }
   return false;
}

/**
 * @brief Read a JSON value as an unsigned 32 bit integer
 * @param v    the JSON value
 * @param out  receives the value
 * @return true if the value is an integer in the range of the type, false otherwise (out unchanged)
 */
static bool
spool_json_value_to_u32(const rapidjson::Value &v, std::uint32_t *out) {
   std::uint64_t value = 0;
   if (!spool_json_value_to_u64(v, &value) || value > UINT32_MAX) {
      return false;
   }
   *out = static_cast<std::uint32_t>(value);
   return true;
}

/**
 * @brief Read a JSON value as a signed 32 bit integer
 * @param v    the JSON value
 * @param out  receives the value
 * @return true if the value is an integer in the range of the type, false otherwise (out unchanged)
 */
static bool
spool_json_value_to_i32(const rapidjson::Value &v, std::int32_t *out) {
   std::int64_t value = 0;
   if (!spool_json_value_to_i64(v, &value) || value < INT32_MIN || value > INT32_MAX) {
      return false;
   }
   *out = static_cast<std::int32_t>(value);
   return true;
}

/**
 * @brief Report a numeric field value that does not fit the field's type
 * @param answer_list  receives the error, may be nullptr
 * @param field        the row of the field
 * @param v            the JSON value
 */
static void
spool_json_bad_number(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const spooling_field &field,
                      const rapidjson::Value &v) {
   dstring text = DSTRING_INIT;
   answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_FLATFILE_INVALIDNUMBER_SS,
                           spool_json_value_to_string(v, &text), field.name != nullptr ? field.name : "");
   sge_dstring_free(&text);
}

/**
 * @brief Coerce a JSON value (bool, number, or "true"/"1" string) to a bool.
 *
 * @param v  the JSON value
 * @return the boolean value, false if not coercible
 */
static bool
spool_json_value_to_bool(const rapidjson::Value &v) {
   if (v.IsBool())   return v.GetBool();
   if (v.IsNumber()) return v.GetDouble() != 0.0;
   if (v.IsString()) return sge_strnullcasecmp(v.GetString(), "true") == 0 ||
                            strcmp(v.GetString(), "1") == 0;
   return false;
}

/**
 * @brief Coerce a JSON value for a string/host field, mapping unset to "NONE".
 *
 * An unset value (JSON null or an empty string) maps to the "NONE" sentinel *string*,
 * exactly as the flatfile reader stores the "NONE" token from an ASCII file. Producing
 * the string (rather than a C-null) lets qmaster apply its own per-field handling: it
 * nulls the nullable fields (ocs_null_out_none) but keeps the required ones (e.g. a ckpt
 * interface's command fields, which reject null). This makes JSON input behave
 * identically to ASCII input and round-trip for both field kinds.
 *
 * @param v    the JSON value
 * @param out  scratch buffer for the string form
 * @return the string value, or "NONE" if unset (never nullptr)
 */
static const char *
spool_json_string_or_unset(const rapidjson::Value &v, dstring *out) {
   if (v.IsNull()) {
      return NONE_STR;
   }
   const char *s = spool_json_value_to_string(v, out);
   if (s == nullptr || *s == '\0') {
      return NONE_STR;
   }
   return s;
}

/**
 * @brief Store a string or a host name value with the checked setter of the field's type
 *
 * The element holds the field; the callers test that first.
 *
 * @param ep     element to populate, holding the field
 * @param field  the row of a string or host name field
 * @param s      the value, copied; nullptr for unset
 * @throw ocs::tom::FieldException with reason WrongType for a field of another type, OutOfMemory if the value
 *        cannot be copied
 */
static void
spool_json_set_text(const ocs::tom::RawElement ep, const spooling_field &field, const char *s) {
   if (field.ops->type == ocs::tom::FieldType::Host) {
      ep.set(field.nm, ocs::tom::Host(s));
   } else {
      ep.set(field.nm, s);
   }
}

/**
 * @brief Set a field of an element from a JSON value, coercing per field type.
 *
 * The inverse of spool_json_write_scalar_value (bypassing read_func, to mirror the
 * writer which bypassed write_func). The value is stored with the checked setter of
 * the row's field type; a list, an object or a reference field is left as it is.
 *
 * A number that does not fit the field's type, or a string that is not a number, is reported and the field is
 * left as it is.
 *
 * @param ep           element to populate, holding the field
 * @param field        the row of the field
 * @param v            the JSON value
 * @param answer_list  receives an error for a value that does not fit, may be nullptr
 * @return true if the field was set or has no value, false for a value that does not fit
 */
static bool
spool_json_set_field(const ocs::tom::RawElement ep, const spooling_field &field, const rapidjson::Value &v,
                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   if (!field.ops->has_value()) {
      return true;
   }
   const int nm = field.nm;
   switch (field.ops->type) {
      case ocs::tom::FieldType::Double:
         ep.set(nm, v.IsNumber() ? v.GetDouble() : (v.IsString() ? strtod(v.GetString(), nullptr) : 0.0));
         break;
      case ocs::tom::FieldType::Uint32: {
         std::uint32_t value = 0;
         if (!spool_json_value_to_u32(v, &value)) {
            spool_json_bad_number(answer_list, field, v);
            return false;
         }
         ep.set(nm, value);
         break;
      }
      case ocs::tom::FieldType::Uint64: {
         std::uint64_t value = 0;
         if (!spool_json_value_to_u64(v, &value)) {
            spool_json_bad_number(answer_list, field, v);
            return false;
         }
         ep.set(nm, value);
         break;
      }
      case ocs::tom::FieldType::Int64: {
         std::int64_t value = 0;
         if (!spool_json_value_to_i64(v, &value)) {
            spool_json_bad_number(answer_list, field, v);
            return false;
         }
         ep.set(nm, value);
         break;
      }
      case ocs::tom::FieldType::Int32: {
         std::int32_t value = 0;
         if (!spool_json_value_to_i32(v, &value)) {
            spool_json_bad_number(answer_list, field, v);
            return false;
         }
         ep.set(nm, value);
         break;
      }
      case ocs::tom::FieldType::Bool:
         ep.set(nm, spool_json_value_to_bool(v));
         break;
      default: {
         // a string or a host name, the only other fields with a plain value
         dstring s = DSTRING_INIT;
         spool_json_set_text(ep, field, spool_json_string_or_unset(v, &s));
         sge_dstring_free(&s);
         break;
      }
   }
   return true;
}

/**
 * @brief Store a 32 bit unsigned value with the checked setter
 * @param ep     element to populate, holding the field
 * @param field  the row of an unsigned 32 bit field
 * @param u      the value
 */
static void
spool_json_set_u32(const ocs::tom::RawElement ep, const spooling_field &field, const std::uint32_t u) {
   ep.set(field.nm, u);
}

/**
 * @brief Parse a JSON array of bit names back into the bitfield value.
 *
 * The inverse of spool_json_write_bitfield_array (e.g. ["BATCH","INTERACTIVE"],
 * ["ACL","DEPT"]).
 *
 * @param v      the JSON array of bit-name strings
 * @param names  null-terminated bit-name table (names[i] is the name of bit i)
 * @return the reconstructed bitmask
 */
static uint32_t
spool_json_bitfield_from_array(const rapidjson::Value &v, const char *const *names) {
   uint32_t value = 0;
   if (v.IsArray()) {
      for (rapidjson::SizeType i = 0; i < v.Size(); i++) {
         if (!v[i].IsString()) {
            continue;
         }
         uint32_t bitmask = 1;
         for (const char *const *ptr = names; *ptr != nullptr; ptr++) {
            if (sge_strnullcasecmp(v[i].GetString(), *ptr) == 0) {
               value |= bitmask;
               break;
            }
            bitmask <<= 1;
         }
      }
   }
   return value;
}

/**
 * @brief Set a positional element's "name" (first) field from a JSON value.
 *
 * Maps the JSON "default" token back to HOSTREF_DEFAULT ("@/") for a host-reference
 * field (the inverse of spool_json_write_positional_name).
 *
 * @param ep     element to populate, holding the field
 * @param field  the row of the name field
 * @param v      the JSON value
 * @param answer_list  receives an error for a value that does not fit, may be nullptr
 * @return false for a value that does not fit the field
 */
static bool
spool_json_set_positional_name(const ocs::tom::RawElement ep, const spooling_field &field,
                               const rapidjson::Value &v, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   if (field.ops->type == ocs::tom::FieldType::Host && v.IsString() &&
       strcmp(v.GetString(), SPOOL_JSON_DEFAULT_HREF) == 0) {
      spool_json_set_text(ep, field, HOSTREF_DEFAULT);
      return true;
   }
   return spool_json_set_field(ep, field, v, answer_list);
}

/* forward */
static bool spool_json_populate_members(ocs::tom::RawElement ep, const spooling_field *fields, int fields_out[],
                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const rapidjson::Value &obj);

/**
 * @brief Build a sublist from a JSON array, mirroring the writer's shape decisions.
 *
 * Positional (unnamed spooling fields) -> 1 field: array of scalars, 2 fields: array of
 * {name,value}; named fields (incl. CE) -> array of objects keyed by name. An empty
 * array is read as a null (unset) list, mirroring the ASCII reader.
 *
 * The sub-type of the list field is known here only as the descriptor the row's record
 * names, so the list and its elements are created from it; the elements' fields
 * are stored with the checked setters of the sub-rows' field types.
 *
 * @param field        the row of the list field, whose sub-rows describe the elements
 * @param answer_list  for returning errors
 * @param arr          the JSON array
 * @param ok           set to false if a value does not fit its field (reported in @p answer_list), else unchanged
 * @return the owner of the populated list, empty for an empty/unreadable array
 */
static ocs::tom::RawListPtr
spool_json_read_sublist(const spooling_field &field, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const rapidjson::Value &arr, bool *ok) {
   const ocs::tom::FieldDescr *sub_descr = field.ops->sub_descr;
   const spooling_field *fields = field.sub_fields;
   if (sub_descr == nullptr || fields == nullptr || !arr.IsArray() || arr.Size() == 0) {
      /* an empty array is read as a null (unset) list, mirroring the ASCII reader;
       * this also makes an empty RQS filter scope behave as "match all" (the matcher
       * distinguishes a null scope from a present-but-empty one) */
      return {};
   }

   const bool positional = (fields[0].nm != NoName && fields[0].name == nullptr);
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   auto list = ocs::tom::create_raw_list(sub_descr, ocs::tom::Hashing::On, "sublist");
   for (rapidjson::SizeType k = 0; k < arr.Size(); k++) {
      const rapidjson::Value &el = arr[k];
      auto sub_owner = ocs::tom::create_raw_elem(sub_descr);
      const ocs::tom::RawElement sub = sub_owner.get();

      if (positional && nfields <= 1) {
         if (sub.has(fields[0].nm)) {
            if (!spool_json_set_positional_name(sub, fields[0], el, answer_list)) {
               *ok = false;
            }
         }
      } else if (positional && el.IsObject()) {
         const auto mn = el.FindMember("name");
         const auto mv = el.FindMember("value");
         if (sub.has(fields[0].nm) && mn != el.MemberEnd()) {
            if (!spool_json_set_positional_name(sub, fields[0], mn->value, answer_list)) {
               *ok = false;
            }
         }
         if (sub.has(fields[1].nm) && mv != el.MemberEnd()) {
            if (fields[1].ops->json == ocs_spool_render::bitfield) {
               /* bitfield value is a name array -> bitmask */
               spool_json_set_u32(sub, fields[1], spool_json_bitfield_from_array(mv->value, fields[1].ops->names));
            } else if (fields[1].ops->type == ocs::tom::FieldType::List) {
               /* the value is itself a list (e.g. the cqueue threshold value is a
                * complex list) -> read it as a nested sublist */
               sub.set(fields[1].nm, spool_json_read_sublist(fields[1], answer_list, mv->value, ok));
            } else if (!spool_json_set_field(sub, fields[1], mv->value, answer_list)) {
               *ok = false;
            }
         }
      } else if (el.IsObject()) {
         // a value that does not fit its field is the only way populate_members fails on a sublist element
         if (!spool_json_populate_members(sub, fields, nullptr, answer_list, el)) {
            *ok = false;
         }
      }
      list->push_back(std::move(sub_owner));
   }
   return list;
}

/**
 * @brief Gather an object's flat "<param>": <value> members into a name=value sublist.
 *
 * The inverse of the writer's flatten (e.g. CONF_entries). The $schema/$id envelope and
 * any member already consumed by a named field are excluded; list-valued params are
 * re-joined to their stored string form; the value is set through the field's read_func
 * (read_CF_value, which validates) when present. The name and the value are string
 * fields, stored with the checked setters of the sub-rows' field types.
 *
 * @param ep          element to populate
 * @param fields      spooling fields of the object (to exclude named members)
 * @param flatten     the unnamed name=value sublist field to fill
 * @param fields_out  if non-null, receives the flattened field id
 * @param obj         the JSON object
 * @param answer_list receives the message of a value the field's read_func rejects
 * @return false if a value was rejected (the sublist is not stored then), true otherwise
 */
static bool
spool_json_read_flatten(const ocs::tom::RawElement ep, const spooling_field *fields,
                        const spooling_field *flatten, int fields_out[],
                        const rapidjson::Value &obj, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   const ocs::tom::FieldDescr *sub_descr = flatten->ops->sub_descr;
   if (!ep.has(flatten->nm) || sub_descr == nullptr) {
      return true;
   }
   const spooling_field *sf = flatten->sub_fields;
   if (ocs::tom::present_pos(sub_descr, sf[0].nm) < 0 || ocs::tom::present_pos(sub_descr, sf[1].nm) < 0) {
      return true;
   }
   auto cf_list = ocs::tom::create_raw_list(sub_descr, ocs::tom::Hashing::On, "entries");
   for (auto m = obj.MemberBegin(); m != obj.MemberEnd(); ++m) {
      const char *key = m->name.GetString();
      if (key == nullptr || key[0] == '$') {
         continue;   /* skip the $schema/$id envelope */
      }
      bool is_named = false;
      for (int i = 0; fields[i].nm != NoName; i++) {
         if (fields[i].name != nullptr && strcmp(fields[i].name, key) == 0) {
            is_named = true;
            break;
         }
      }
      if (is_named) {
         continue;
      }
      auto cf_owner = ocs::tom::create_raw_elem(sub_descr);
      const ocs::tom::RawElement cf = cf_owner.get();
      spool_json_set_text(cf, sf[0], key);
      dstring vb = DSTRING_INIT;
      const char *vstr;
      if (m->value.IsArray()) {
         /* a config list param: join the array back to the stored string form -
          * ["a","b"] -> "a,b"; [{name,value}] -> "K=V,K2=V2" */
         for (rapidjson::SizeType k = 0; k < m->value.Size(); k++) {
            const rapidjson::Value &el = m->value[k];
            if (k > 0) {
               sge_dstring_append(&vb, ",");
            }
            dstring t = DSTRING_INIT;
            if (el.IsObject()) {
               const auto mn = el.FindMember("name");
               const auto mv = el.FindMember("value");
               if (mn != el.MemberEnd()) {
                  sge_dstring_append(&vb, spool_json_value_to_string(mn->value, &t));
               }
               sge_dstring_append(&vb, "=");
               if (mv != el.MemberEnd()) {
                  /* an empty value (e.g. filter, shown as "") maps back to "NONE" */
                  const char *vv = spool_json_value_to_string(mv->value, &t);
                  sge_dstring_append(&vb, (vv == nullptr || *vv == '\0') ? NONE_STR : vv);
               }
            } else {
               sge_dstring_append(&vb, spool_json_value_to_string(el, &t));
            }
            sge_dstring_free(&t);
         }
         vstr = sge_dstring_get_string(&vb);
      } else {
         vstr = spool_json_value_to_string(m->value, &vb);
      }
      /* config requires a value: an unset/empty param (our writer emits "" or []) maps
       * back to the "NONE" sentinel - also avoids a null reaching read_CF_value */
      if (vstr == nullptr || *vstr == '\0') {
         vstr = NONE_STR;
      }
      // a value the read_func rejects ends the read, as the flatfile reader does
      const bool accepted = sf[1].read_func == nullptr || sf[1].read_func(cf, sf[1].nm, vstr, answer_list) != 0;
      if (accepted && sf[1].read_func == nullptr) {
         spool_json_set_text(cf, sf[1], vstr);
      }
      sge_dstring_free(&vb);
      cf_list->push_back(std::move(cf_owner));
      if (!accepted) {
         return false;   // cf_list frees the entries read so far
      }
   }
   ep.set(flatten->nm, std::move(cf_list));
   if (fields_out != nullptr) {
      add_nm_to_set(fields_out, flatten->nm);
   }
   return true;
}

/**
 * @brief Set the members of an element from a JSON object.
 *
 * Walks the spooling fields, looking each up by name (the inverse of
 * spool_json_write_object_members); a trailing flatten field is un-flattened after the
 * named pass. The fields are stored with the checked setters; the branches by rendering
 * kind name fields that are numbers (date-time, bitfield, enum) or strings (perm list),
 * bound to these field types when the record is made, so the value type a branch
 * stores is the one the field's type names.
 *
 * @param ep          element to populate
 * @param fields      spooling fields describing the object
 * @param fields_out  if non-null, receives the set of field ids found
 * @param answer_list receives an error if a field (e.g. perm_list) is malformed
 * @param obj         the JSON object
 * @return true on success, false if a field was malformed (answer_list set)
 */
static bool
spool_json_populate_members(const ocs::tom::RawElement ep, const spooling_field *fields, int fields_out[],
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const rapidjson::Value &obj) {
   const spooling_field *flatten = nullptr;
   for (int i = 0; fields[i].nm != NoName; i++) {
      const spooling_field &field = fields[i];
      const int nm = field.nm;
      const char *name = field.name;
      if (name == nullptr) {
         /* a top-level unnamed name=value sublist (config entries) is un-flattened
            after the named pass by gathering the remaining members */
         const spooling_field *sf = field.sub_fields;
         if (ep.has(nm) && field.ops->type == ocs::tom::FieldType::List && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            flatten = &field;
         }
         continue;
      }
      if (!ep.has(nm)) {
         continue;
      }
      const auto m = obj.FindMember(name);
      if (m == obj.MemberEnd()) {
         continue;   /* field absent in the JSON -> simply not read */
      }
      if (fields_out != nullptr) {
         add_nm_to_set(fields_out, nm);
      }

      const ocs::tom::FieldType type = field.ops->type;
      if (type == ocs::tom::FieldType::List) {
         bool ok = true;
         ep.set(nm, spool_json_read_sublist(field, answer_list, m->value, &ok));
         if (!ok) {
            return false;
         }
      } else if (type == ocs::tom::FieldType::Object && field.sub_fields != nullptr) {
         /* nested JSON object (e.g. an RQS filter) -> sub-object; null / non-object
          * leaves the field unset, so an absent filter round-trips to null. The
          * sub-type is known here only as the descriptor the row's record names. */
         const ocs::tom::FieldDescr *sub_descr = field.ops->sub_descr;
         if (m->value.IsObject() && sub_descr != nullptr) {
            auto sub = ocs::tom::create_raw_elem(sub_descr);
            if (!spool_json_populate_members(sub.get(), field.sub_fields, nullptr, answer_list, m->value)) {
               return false;   // sub frees itself
            }
            ep.set(nm, std::move(sub));
         }
      } else if (field.ops->json == ocs_spool_render::datetime) {
         /* ISO date-time string -> gmt64 timestamp */
         dstring s = DSTRING_INIT;
         const std::uint64_t timestamp = spool_json_datetime_from_string(spool_json_value_to_string(m->value, &s));
         sge_dstring_free(&s);
         ep.set(nm, timestamp);
      } else if (field.ops->json == ocs_spool_render::bitfield) {
         /* bit-name array -> bitfield */
         spool_json_set_u32(ep, field, spool_json_bitfield_from_array(m->value, field.ops->names));
      } else if (field.ops->json == ocs_spool_render::enumeration) {
         /* symbolic enum token ("USER"/"PROJECT") or raw number -> enum value */
         spool_json_set_u32(ep, field, spool_json_enum_from_value(m->value, field.ops->names));
      } else if (field.ops->json == ocs_spool_render::perm_list) {
         /* array of rule objects -> comma/colon-joined perm_list string; a non-array
          * value is rejected rather than silently wiping the rule set to NONE */
         dstring s = DSTRING_INIT;
         const bool ok = spool_json_perm_list_from_array(m->value, &s, answer_list);
         if (ok) {
            spool_json_set_text(ep, field, sge_dstring_get_string(&s));
         }
         sge_dstring_free(&s);
         if (!ok) {
            return false;
         }
      } else if (field.ops->json == ocs_spool_render::by_name) {
         /* symbolic enum token ("INT", "<=", "YES", ...) -> enum value */
         dstring s = DSTRING_INIT;
         ocs_spool_parse_value(*field.ops, ep, nullptr, spool_json_value_to_string(m->value, &s));
         sge_dstring_free(&s);
      } else if (!spool_json_set_field(ep, field, m->value, answer_list)) {
         return false;
      }
   }
   if (flatten != nullptr && !spool_json_read_flatten(ep, fields, flatten, fields_out, obj, answer_list)) {
      return false;
   }
   return true;
}

/**
 * @brief Parse a JSON document back into a single element.
 *
 * @param answer_list  for returning errors
 * @param descr        descriptor of the element to build
 * @param fields       spooling fields describing the object
 * @param fields_out   if non-null, receives the set of field ids found
 * @param json_text    the JSON document text
 * @return the owner of the parsed element, empty on a JSON syntax error or a malformed field (answer_list set)
 */
ocs::tom::RawElementPtr
spool_json_read_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::FieldDescr *descr,
                       const spooling_field *fields, int fields_out[], const char *json_text) {
   DENTER(JSON_LAYER);

   rapidjson::Document doc;
   doc.Parse(json_text);
   if (doc.HasParseError() || !doc.IsObject()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_FLATFILE_INVALIDJSON);
      DRETURN(ocs::tom::RawElementPtr());
   }

   auto ep = ocs::tom::create_raw_elem(descr);
   if (!spool_json_populate_members(ep.get(), fields, fields_out, answer_list, doc)) {
      ep.reset();
   }
   DRETURN(ep);
}

/**
 * @brief Parse a JSON document back into a list.
 *
 * The objects are read from the document's first array member (the $schema/$id envelope
 * members are skipped).
 *
 * @param answer_list  for returning errors
 * @param descr        descriptor of the list elements
 * @param fields       spooling fields describing the elements
 * @param fields_out   if non-null, receives the set of field ids found (from element 0)
 * @param json_text    the JSON document text
 * @return the owner of the parsed list, empty on a JSON syntax error, a document without an array member or a
 *         malformed field (answer_list set)
 */
ocs::tom::RawListPtr
spool_json_read_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::FieldDescr *descr,
                     const spooling_field *fields, int fields_out[], const char *json_text) {
   DENTER(JSON_LAYER);

   rapidjson::Document doc;
   doc.Parse(json_text);
   if (doc.HasParseError() || !doc.IsObject()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_FLATFILE_INVALIDJSON);
      DRETURN(ocs::tom::RawListPtr());
   }

   /* the document wraps the objects in an array member (skip $schema/$id) */
   const rapidjson::Value *arr = nullptr;
   for (auto m = doc.MemberBegin(); m != doc.MemberEnd(); ++m) {
      if (m->value.IsArray()) {
         arr = &m->value;
         break;
      }
   }
   if (arr == nullptr) {
      DRETURN(ocs::tom::RawListPtr());
   }

   auto list = ocs::tom::create_raw_list(descr, ocs::tom::Hashing::On, "list");
   for (rapidjson::SizeType k = 0; k < arr->Size(); k++) {
      if (!(*arr)[k].IsObject()) {
         continue;
      }
      auto ep = ocs::tom::create_raw_elem(descr);
      if (!spool_json_populate_members(ep.get(), fields, k == 0 ? fields_out : nullptr, answer_list, (*arr)[k])) {
         DRETURN(ocs::tom::RawListPtr());   // ep and list free the elements read so far
      }
      list->push_back(std::move(ep));
   }
   DRETURN(list);
}
