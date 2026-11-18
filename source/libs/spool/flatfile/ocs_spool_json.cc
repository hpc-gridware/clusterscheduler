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
 * @brief Rendering cull objects as JSON, for `qconf -fmt json`
 */

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>

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

#include "cull/cull.h"

#include "sgeobj/sge_object.h"           /* object_get_type_name, the symbolic value renderer/parser */
#include "sgeobj/sge_attr.h"             /* HOSTREF_DEFAULT ("@/") */
#include "sgeobj/sge_centry.h"           /* CE_Type / CE_* for complex-value typing */
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
#include "sgeobj/tom/ocs_tom_CE.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/ocs_spool_fields.h"

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
spool_json_write_object_members(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields);
static bool
spool_json_write_list_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields);
static bool
spool_json_write_object_braced(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const spooling_field *fields);
static bool
spool_json_write_positional_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static bool
spool_json_write_list_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static lUlong
spool_json_value_to_ulong(const rapidjson::Value &v);

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
spool_json_typename(const lListElem *object, dstring *out) {
   const char *type_name = object_get_type_name(object);
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
                          const lListElem *object) {
   dstring name = DSTRING_INIT;
   spool_json_write_envelope_name(writer, spool_json_typename(object, &name));
   sge_dstring_free(&name);
}

/**
 * @brief Write one scalar field value (no key) typed from the field's type.
 *
 * The value is read through the row's typed code; a list, an object or a reference
 * field has no scalar value and is written as null.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field
 * @param field   the row of the field
 */
static void
spool_json_write_scalar_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *object, const spooling_field &field) {
   if (field.ops->get == nullptr) {
      writer.Null();
      return;
   }
   const ocs_spool_value value = field.ops->get(object);
   switch (field.ops->type) {
      case ocs::tom::FieldType::Double: writer.Double(value.dbl); break;
      case ocs::tom::FieldType::Uint32: writer.Uint64(value.u32); break;
      case ocs::tom::FieldType::Uint64: writer.Uint64(value.u64); break;
      case ocs::tom::FieldType::Int64:  writer.Int64(value.i64); break;
      case ocs::tom::FieldType::Int32:  writer.Int(value.i32); break;
      case ocs::tom::FieldType::Bool:   writer.Bool(value.b); break;
      default: {
         /* a string or a host name, the only other fields with a get operation.
            unset string -> empty string. null, "" and the "NONE" sentinel all mean
            unset, so they render uniformly as "" instead of leaking the internal "NONE"
            token into JSON. The reader maps "" back to the "NONE" sentinel string (not
            C-null), mirroring the flatfile reader, so a required field whose unset value
            is "NONE" (e.g. a ckpt interface's command fields) round-trips correctly. */
         const char *s = value.str;
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
 * descriptor that is not the canonical CE_Type pointer.
 *
 * @param list  the list to inspect
 * @return true if the list's descriptor carries the CE name/value/type fields
 */
static bool
spool_json_is_ce_list(const lList *list) {
   const lDescr *d = lGetListDescr(list);
   return d != nullptr &&
          lGetPosInDescr(d, CE_name) >= 0 &&
          lGetPosInDescr(d, CE_stringval) >= 0 &&
          lGetPosInDescr(d, CE_valtype) >= 0;
}

/**
 * @brief Write a field as its symbolic string token.
 *
 * Used for the fields of the JSON kind ocs_spool_render::by_name (#ocs_spool_field_ops::json).
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field
 * @param nm      CULL field id to render symbolically
 */
static void
spool_json_write_symbolic_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const lListElem *object, int nm) {
   dstring buf = DSTRING_INIT;
   const char *s = object_append_field_to_dstring(object, nullptr, &buf, nm, '\0');
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
static lUlong
spool_json_enum_from_value(const rapidjson::Value &v, const char *const *names) {
   if (v.IsString()) {
      for (uint32_t i = 0; names[i] != nullptr; i++) {
         if (sge_strnullcasecmp(v.GetString(), names[i]) == 0) {
            return i;
         }
      }
   }
   return spool_json_value_to_ulong(v);   /* numeric form (or unmatched string) */
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
 * date-time, bitfield, enum, symbolic, complex-typed, numeric, or plain scalar branch;
 * a field absent from the descriptor -> null. The value is read through the row's
 * typed code; the kinds are bound to the field types at compile time (date-time,
 * bitfield, enum are numbers, complex-typed and numeric are strings), so the member of
 * the value a branch reads is the one the field's type names.
 *
 * @param writer  pretty-printer to emit into
 * @param ep      element holding the field
 * @param field   the row of the field to write
 */
static void
spool_json_write_field_scalar(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *ep, const spooling_field &field) {
   if (lGetPosInDescr(lGetElemDescr(ep), field.nm) < 0) {
      writer.Null();
      return;
   }
   const ocs_spool_field_ops &ops = *field.ops;
   switch (ops.json) {
      case ocs_spool_render::datetime:
         spool_json_write_datetime(writer, ops.get(ep).u64);
         break;
      case ocs_spool_render::bitfield:
         spool_json_write_bitfield_array(writer, ops.get(ep).u32, ops.names);
         break;
      case ocs_spool_render::enumeration:
         spool_json_write_enum_value(writer, ops.get(ep).u32, ops.names);
         break;
      case ocs_spool_render::by_name:
         spool_json_write_symbolic_value(writer, ep, field.nm);
         break;
      case ocs_spool_render::avalue:
         spool_json_write_avalue(writer, ops.get(ep).str, ops.avalue_type);
         break;
      case ocs_spool_render::number:
         spool_json_write_number_value(writer, ops.get(ep).str);
         break;
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
                                 const lListElem *ep, const spooling_field &field) {
   if (field.ops->type == ocs::tom::FieldType::Host && lGetPosInDescr(lGetElemDescr(ep), field.nm) >= 0) {
      const char *h = field.ops->get(ep).str;
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
 * here only as the rows' descriptor; the fields are read through the rows' typed code.
 *
 * @param answer_list  for returning errors
 * @param list         the positional sublist
 * @param fields       the (unnamed) spooling fields of the sublist
 * @param writer       pretty-printer to emit into
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_positional_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer) {
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   writer.StartArray();
   const lListElem *ep;
   for_each_ep(ep, list) {
      if (nfields <= 1) {
         spool_json_write_positional_name(writer, ep, fields[0]);
      } else {
         writer.StartObject();
         writer.Key("name");
         spool_json_write_positional_name(writer, ep, fields[0]);
         writer.Key("value");
         if (fields[1].ops->type == ocs::tom::FieldType::List &&
             lGetPosInDescr(lGetElemDescr(ep), fields[1].nm) >= 0) {
            if (!spool_json_write_list_value(answer_list, fields[1].ops->get_list(ep), fields[1].sub_fields,
                                             writer)) {
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
 * @param sub_list     the list field value
 * @param sub_fields   spooling fields describing the sublist elements
 * @param writer       pretty-printer to emit into
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_list_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer) {
   if (sub_list == nullptr || lGetNumberOfElem(sub_list) == 0) {
      writer.StartArray();
      writer.EndArray();
   } else if (spool_json_is_ce_list(sub_list)) {
      // a list with the complex-entry fields holds complex entries (CE_Type), possibly reduced; checked as the
      // getters checked each element before
      spool_json_write_ce_array(writer, ocs::tom::ConstList<ocs::tom::CE>(sub_list));
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
 * @brief Test whether a field is an RBAC permission rule list (rendered as objects).
 *
 * @param nm  CULL field id
 * @return true for the role perm_list field
 */
static bool
spool_json_nm_is_perm_list(int nm) {
   return nm == RL_perm_list;
}

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
 * Members are keyed by the spooling_field names and typed from the CULL field types.
 * Scalars become native JSON values; CE/complex-value sublists become arrays of typed
 * {name,value}; name-only sublists become arrays of strings; other sublists become
 * arrays of objects. A top-level unnamed name=value sublist (the config entries) is
 * flattened into "<param>": <value> members.
 *
 * @param answer_list  for returning errors
 * @param object       the object to serialize
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the object
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_object_members(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields) {
   DENTER(JSON_LAYER);

   SGE_CHECK_POINTER_FALSE(object, answer_list);
   SGE_CHECK_POINTER_FALSE(fields, answer_list);

   const lDescr *descr = lGetElemDescr(object);

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
         if (lGetPosInDescr(descr, nm) >= 0 && field.ops->type == ocs::tom::FieldType::List && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            const lList *sub = field.ops->get_list(object);
            const lListElem *e;
            for_each_ep(e, sub) {
               const char *key = sf[0].ops->get(e).str;
               if (key == nullptr) {
                  continue;
               }
               writer.Key(key);
               const char *value = sf[1].ops->get(e).str;
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
      if (lGetPosInDescr(descr, nm) < 0) {
         continue;
      }

      const ocs::tom::FieldType type = field.ops->type;
      if (type == ocs::tom::FieldType::List) {
         writer.Key(name);
         if (!spool_json_write_list_value(answer_list, field.ops->get_list(object), field.sub_fields, writer)) {
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
         const lListElem *real = field.ops->get_object(object);
         const lDescr *sub_descr = field.ops->sub_descr;
         lListElem *tmp = nullptr;
         const lListElem *sub_obj = real;
         if (sub_obj == nullptr && sub_descr != nullptr) {
            tmp = lCreateElem(sub_descr);
            sub_obj = tmp;
         }
         bool ok = true;
         if (sub_obj != nullptr) {
            ok = spool_json_write_object_braced(answer_list, sub_obj, writer, field.sub_fields);
         } else {
            writer.Null();
         }
         lFreeElem(&tmp);
         if (!ok) {
            DRETURN(false);
         }
      } else {
         /* the JSON kinds of the field are bound to its type at compile time (date-time,
            bitfield, enum are numbers, numeric is a string) and the perm list is a
            string, so the member of the value a branch reads is the one the field's type
            names; a complex-typed value (ocs_spool_render::avalue) is written as a plain
            scalar here, as it always was */
         writer.Key(name);
         const ocs_spool_field_ops &ops = *field.ops;
         if (ops.json == ocs_spool_render::datetime) {
            spool_json_write_datetime(writer, ops.get(object).u64);
         } else if (ops.json == ocs_spool_render::bitfield) {
            spool_json_write_bitfield_array(writer, ops.get(object).u32, ops.names);
         } else if (ops.json == ocs_spool_render::enumeration) {
            spool_json_write_enum_value(writer, ops.get(object).u32, ops.names);
         } else if (spool_json_nm_is_perm_list(nm)) {
            spool_json_write_perm_list(writer, ops.get(object).str);
         } else if (ops.json == ocs_spool_render::by_name) {
            spool_json_write_symbolic_value(writer, object, nm);
         } else if (ops.json == ocs_spool_render::number) {
            spool_json_write_number_value(writer, ops.get(object).str);
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
 * @param object       the object to serialize
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the object
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_object_braced(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
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
 * @param list         the list to serialize
 * @param writer       pretty-printer to emit into
 * @param fields       spooling fields describing the elements
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_list_array(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields) {
   DENTER(JSON_LAYER);

   writer.StartArray();
   const lListElem *ep;
   for_each_ep(ep, list) {
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
 * @param object       the object to serialize
 * @param fields       spooling fields describing the object
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
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
 * @param object       the object to serialize
 * @param fields       spooling fields describing the object
 * @param derived      derived keys, terminated by an entry whose key is nullptr;
 *                     may be nullptr
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_object_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
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

      const lListElem *ep;
      for_each_ep(ep, d->names) {
         const char *name = lGetHost(ep, d->keynm);

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
 * @param list         the list to serialize
 * @param fields       spooling fields describing the elements
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
                           const spooling_field *fields, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   const lListElem *first = lFirst(list);
   dstring key = DSTRING_INIT;

   writer.StartObject();
   if (first != nullptr) {
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
 * @param object       the object to serialize
 * @param fields       spooling fields describing the object
 * @param type_name    explicit type name for the $id
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *object,
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
 * @param list           the list to serialize (a null list yields an empty array)
 * @param fields         spooling fields describing the elements
 * @param id_name        type name for the $id (`ocs-qconf-<id_name>.schema.json`)
 * @param envelope_name  the array key wrapping the records
 * @param out            dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_list_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
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
 * @param list         the list to serialize
 * @param fields       spooling fields describing the elements
 * @param type_name    explicit type name for the $id and the array key
 * @param out          dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool spool_json_write_typed_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list,
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
 * @param list         the list of name-bearing elements
 * @param keynm        CULL field id of the name attribute
 * @param out          dstring the JSON document is appended to
 * @return true on success
 */
bool spool_json_write_name_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list, int keynm,
                                dstring *out) {
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
 * @param list         the list of name-bearing elements
 * @param keynm        CULL field id of the name attribute
 * @param type_name    envelope/$id name, or nullptr to derive it from the first element
 * @param out          dstring the JSON document is appended to
 * @return true on success
 */
bool spool_json_write_name_list_ex(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *list, int keynm,
                                   const char *type_name, dstring *out) {
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   const lListElem *first = lFirst(list);
   dstring key = DSTRING_INIT;

   /* same $schema/$id envelope as the object lists, but the array holds bare names */
   writer.StartObject();
   if (type_name != nullptr || first != nullptr) {
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
   const lListElem *ep;
   for_each_ep(ep, list) {
      if (!named || lGetPosInDescr(lGetElemDescr(ep), keynm) < 0) {
         continue;
      }
      const char *s = ops->get(ep).str;
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
 * @brief Coerce a JSON value to a string (for lStringT/lHostT fields).
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
 * @brief Coerce a JSON value (number or numeric string) to a CULL ulong.
 *
 * @param v  the JSON value
 * @return the unsigned long value, or 0 if not coercible
 */
static lUlong
spool_json_value_to_ulong(const rapidjson::Value &v) {
   if (v.IsUint64()) return static_cast<lUlong>(v.GetUint64());
   if (v.IsInt64())  return static_cast<lUlong>(v.GetInt64());
   if (v.IsDouble()) return static_cast<lUlong>(v.GetDouble());
   if (v.IsString()) return static_cast<lUlong>(strtoul(v.GetString(), nullptr, 10));
   return 0;
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
 * @brief Coerce a JSON value for a CULL string/host field, mapping unset to "NONE".
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
 * @brief Set a field of an element from a JSON value, coercing per field type.
 *
 * The inverse of spool_json_write_scalar_value (bypassing read_func, to mirror the
 * writer which bypassed write_func). The value is stored through the row's typed
 * code; a list, an object or a reference field is left as it is.
 *
 * @param ep     element to populate, holding the field
 * @param field  the row of the field
 * @param v      the JSON value
 */
static void
spool_json_set_field(lListElem *ep, const spooling_field &field, const rapidjson::Value &v) {
   if (field.ops->set == nullptr) {
      return;
   }
   dstring s = DSTRING_INIT;
   ocs_spool_value value{};
   switch (field.ops->type) {
      case ocs::tom::FieldType::Double:
         value.dbl = v.IsNumber() ? v.GetDouble() : (v.IsString() ? strtod(v.GetString(), nullptr) : 0.0);
         break;
      case ocs::tom::FieldType::Uint32:
         value.u32 = spool_json_value_to_ulong(v);
         break;
      case ocs::tom::FieldType::Uint64:
         value.u64 = v.IsUint64() ? v.GetUint64() : static_cast<lUlong64>(spool_json_value_to_ulong(v));
         break;
      case ocs::tom::FieldType::Int64:
         value.i64 = v.IsInt64() ? v.GetInt64() : static_cast<lLong>(spool_json_value_to_ulong(v));
         break;
      case ocs::tom::FieldType::Int32:
         value.i32 = v.IsInt() ? v.GetInt() : static_cast<int>(spool_json_value_to_ulong(v));
         break;
      case ocs::tom::FieldType::Bool:
         value.b = spool_json_value_to_bool(v);
         break;
      default:
         // a string or a host name, the only other fields with a set operation
         value.str = spool_json_string_or_unset(v, &s);
         break;
   }
   field.ops->set(ep, value);
   sge_dstring_free(&s);
}

/**
 * @brief Store a string or host name value through a row's typed code
 * @param ep     element to populate, holding the field
 * @param field  the row of a string or host name field
 * @param s      the value, copied; nullptr for unset
 */
static void
spool_json_set_string(lListElem *ep, const spooling_field &field, const char *s) {
   ocs_spool_value value{};
   value.str = s;
   field.ops->set(ep, value);
}

/**
 * @brief Store a 32 bit unsigned value through a row's typed code
 * @param ep     element to populate, holding the field
 * @param field  the row of an unsigned 32 bit field
 * @param u      the value
 */
static void
spool_json_set_u32(lListElem *ep, const spooling_field &field, const uint32_t u) {
   ocs_spool_value value{};
   value.u32 = u;
   field.ops->set(ep, value);
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
 */
static void
spool_json_set_positional_name(lListElem *ep, const spooling_field &field, const rapidjson::Value &v) {
   if (field.ops->type == ocs::tom::FieldType::Host && v.IsString() &&
       strcmp(v.GetString(), SPOOL_JSON_DEFAULT_HREF) == 0) {
      spool_json_set_string(ep, field, HOSTREF_DEFAULT);
   } else {
      spool_json_set_field(ep, field, v);
   }
}

/* forward */
static bool spool_json_populate_members(lListElem *ep, const lDescr *descr,
                                        const spooling_field *fields, int fields_out[],
                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const rapidjson::Value &obj);

/**
 * @brief Build a sublist from a JSON array, mirroring the writer's shape decisions.
 *
 * Positional (unnamed spooling fields) -> 1 field: array of scalars, 2 fields: array of
 * {name,value}; named fields (incl. CE) -> array of objects keyed by name. An empty
 * array is read as a null (unset) list, mirroring the ASCII reader.
 *
 * The sub-type of the list field is known here only as the descriptor the row's typed
 * code names, so the list and its elements are created from it; the elements' fields
 * are stored through the typed code of the sub-rows.
 *
 * @param field        the row of the list field, whose sub-rows describe the elements
 * @param answer_list  for returning errors
 * @param arr          the JSON array
 * @return the populated list, or nullptr for an empty/unreadable array
 */
static lList *
spool_json_read_sublist(const spooling_field &field, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const rapidjson::Value &arr) {
   const lDescr *sub_descr = field.ops->sub_descr;
   const spooling_field *fields = field.sub_fields;
   if (sub_descr == nullptr || fields == nullptr || !arr.IsArray() || arr.Size() == 0) {
      /* an empty array is read as a null (unset) list, mirroring the ASCII reader;
       * this also makes an empty RQS filter scope behave as "match all" (the matcher
       * distinguishes a null scope from a present-but-empty one) */
      return nullptr;
   }

   const bool positional = (fields[0].nm != NoName && fields[0].name == nullptr);
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   lList *list = lCreateList("sublist", sub_descr);
   for (rapidjson::SizeType k = 0; k < arr.Size(); k++) {
      const rapidjson::Value &el = arr[k];
      lListElem *sub = lCreateElem(sub_descr);

      if (positional && nfields <= 1) {
         if (lGetPosInDescr(sub_descr, fields[0].nm) >= 0) {
            spool_json_set_positional_name(sub, fields[0], el);
         }
      } else if (positional && el.IsObject()) {
         const auto mn = el.FindMember("name");
         const auto mv = el.FindMember("value");
         if (lGetPosInDescr(sub_descr, fields[0].nm) >= 0 && mn != el.MemberEnd()) {
            spool_json_set_positional_name(sub, fields[0], mn->value);
         }
         if (lGetPosInDescr(sub_descr, fields[1].nm) >= 0 && mv != el.MemberEnd()) {
            if (fields[1].ops->json == ocs_spool_render::bitfield) {
               /* bitfield value is a name array -> bitmask */
               spool_json_set_u32(sub, fields[1], spool_json_bitfield_from_array(mv->value, fields[1].ops->names));
            } else if (fields[1].ops->type == ocs::tom::FieldType::List) {
               /* the value is itself a list (e.g. the cqueue threshold value is a
                * complex list) -> read it as a nested sublist */
               fields[1].ops->set_list(sub, spool_json_read_sublist(fields[1], answer_list, mv->value));
            } else {
               spool_json_set_field(sub, fields[1], mv->value);
            }
         }
      } else if (el.IsObject()) {
         /* sublists never carry a perm_list field, so populate_members cannot fail
          * here; the bool is discarded (read_sublist returns the list, not a status) */
         (void) spool_json_populate_members(sub, sub_descr, fields, nullptr, answer_list, el);
      }
      lAppendElem(list, sub);
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
 * fields, stored through the typed code of the sub-rows.
 *
 * @param ep          element to populate
 * @param descr       descriptor of @p ep
 * @param fields      spooling fields of the object (to exclude named members)
 * @param flatten     the unnamed name=value sublist field to fill
 * @param fields_out  if non-null, receives the flattened field id
 * @param obj         the JSON object
 * @param answer_list receives the message of a value the field's read_func rejects
 * @return false if a value was rejected (the sublist is not stored then), true otherwise
 */
static bool
spool_json_read_flatten(lListElem *ep, const lDescr *descr, const spooling_field *fields,
                        const spooling_field *flatten, int fields_out[],
                        const rapidjson::Value &obj, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   const lDescr *sub_descr = flatten->ops->sub_descr;
   if (lGetPosInDescr(descr, flatten->nm) < 0 || sub_descr == nullptr) {
      return true;
   }
   const spooling_field *sf = flatten->sub_fields;
   if (lGetPosInDescr(sub_descr, sf[0].nm) < 0 || lGetPosInDescr(sub_descr, sf[1].nm) < 0) {
      return true;
   }
   lList *cf_list = lCreateList("entries", sub_descr);
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
      lListElem *cf = lCreateElem(sub_descr);
      spool_json_set_string(cf, sf[0], key);
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
         spool_json_set_string(cf, sf[1], vstr);
      }
      sge_dstring_free(&vb);
      lAppendElem(cf_list, cf);
      if (!accepted) {
         lFreeList(&cf_list);
         return false;
      }
   }
   flatten->ops->set_list(ep, cf_list);
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
 * named pass. The fields are stored through the rows' typed code; the branches by field
 * number name fields that are numbers (date-time, bitfield, enum) or strings (perm list),
 * so the member of the value they store is the one the field's type names.
 *
 * @param ep          element to populate
 * @param descr       descriptor of @p ep
 * @param fields      spooling fields describing the object
 * @param fields_out  if non-null, receives the set of field ids found
 * @param answer_list receives an error if a field (e.g. perm_list) is malformed
 * @param obj         the JSON object
 * @return true on success, false if a field was malformed (answer_list set)
 */
static bool
spool_json_populate_members(lListElem *ep, const lDescr *descr,
                            const spooling_field *fields, int fields_out[],
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
         if (lGetPosInDescr(descr, nm) >= 0 && field.ops->type == ocs::tom::FieldType::List && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            flatten = &field;
         }
         continue;
      }
      if (lGetPosInDescr(descr, nm) < 0) {
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
         field.ops->set_list(ep, spool_json_read_sublist(field, answer_list, m->value));
      } else if (type == ocs::tom::FieldType::Object && field.sub_fields != nullptr) {
         /* nested JSON object (e.g. an RQS filter) -> sub-object; null / non-object
          * leaves the field unset, so an absent filter round-trips to null. The
          * sub-type is known here only as the descriptor the row's typed code names. */
         const lDescr *sub_descr = field.ops->sub_descr;
         if (m->value.IsObject() && sub_descr != nullptr) {
            lListElem *sub = lCreateElem(sub_descr);
            if (!spool_json_populate_members(sub, sub_descr, field.sub_fields, nullptr, answer_list, m->value)) {
               lFreeElem(&sub);
               return false;
            }
            field.ops->set_object(ep, sub);
         }
      } else if (field.ops->json == ocs_spool_render::datetime) {
         /* ISO date-time string -> gmt64 timestamp */
         dstring s = DSTRING_INIT;
         ocs_spool_value value{};
         value.u64 = spool_json_datetime_from_string(spool_json_value_to_string(m->value, &s));
         field.ops->set(ep, value);
         sge_dstring_free(&s);
      } else if (field.ops->json == ocs_spool_render::bitfield) {
         /* bit-name array -> bitfield */
         spool_json_set_u32(ep, field, spool_json_bitfield_from_array(m->value, field.ops->names));
      } else if (field.ops->json == ocs_spool_render::enumeration) {
         /* symbolic enum token ("USER"/"PROJECT") or raw number -> enum value */
         spool_json_set_u32(ep, field, spool_json_enum_from_value(m->value, field.ops->names));
      } else if (spool_json_nm_is_perm_list(nm)) {
         /* array of rule objects -> comma/colon-joined perm_list string; a non-array
          * value is rejected rather than silently wiping the rule set to NONE */
         dstring s = DSTRING_INIT;
         const bool ok = spool_json_perm_list_from_array(m->value, &s, answer_list);
         if (ok) {
            spool_json_set_string(ep, field, sge_dstring_get_string(&s));
         }
         sge_dstring_free(&s);
         if (!ok) {
            return false;
         }
      } else if (field.ops->json == ocs_spool_render::by_name) {
         /* symbolic enum token ("INT", "<=", "YES", ...) -> enum value */
         dstring s = DSTRING_INIT;
         object_parse_field_from_string(ep, nullptr, nm, spool_json_value_to_string(m->value, &s));
         sge_dstring_free(&s);
      } else {
         spool_json_set_field(ep, field, m->value);
      }
   }
   if (flatten != nullptr && !spool_json_read_flatten(ep, descr, fields, flatten, fields_out, obj, answer_list)) {
      return false;
   }
   return true;
}

/**
 * @brief Parse a JSON document back into a single CULL element.
 *
 * @param answer_list  for returning errors
 * @param descr        descriptor of the element to build
 * @param fields       spooling fields describing the object
 * @param fields_out   if non-null, receives the set of field ids found
 * @param json_text    the JSON document text
 * @return the parsed element, or nullptr on a JSON syntax error (answer_list set)
 */
lListElem *
spool_json_read_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                       const spooling_field *fields, int fields_out[], const char *json_text) {
   DENTER(JSON_LAYER);

   rapidjson::Document doc;
   doc.Parse(json_text);
   if (doc.HasParseError() || !doc.IsObject()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_FLATFILE_INVALIDJSON);
      DRETURN(nullptr);
   }

   lListElem *ep = lCreateElem(descr);
   if (!spool_json_populate_members(ep, descr, fields, fields_out, answer_list, doc)) {
      lFreeElem(&ep);
      DRETURN(nullptr);
   }
   DRETURN(ep);
}

/**
 * @brief Parse a JSON document back into a CULL list.
 *
 * The objects are read from the document's first array member (the $schema/$id envelope
 * members are skipped).
 *
 * @param answer_list  for returning errors
 * @param descr        descriptor of the list elements
 * @param fields       spooling fields describing the elements
 * @param fields_out   if non-null, receives the set of field ids found (from element 0)
 * @param json_text    the JSON document text
 * @return the parsed list, or nullptr on a JSON syntax error (answer_list set)
 */
lList *
spool_json_read_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                     const spooling_field *fields, int fields_out[], const char *json_text) {
   DENTER(JSON_LAYER);

   rapidjson::Document doc;
   doc.Parse(json_text);
   if (doc.HasParseError() || !doc.IsObject()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_FLATFILE_INVALIDJSON);
      DRETURN(nullptr);
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
      DRETURN(nullptr);
   }

   lList *list = lCreateList("list", descr);
   for (rapidjson::SizeType k = 0; k < arr->Size(); k++) {
      if (!(*arr)[k].IsObject()) {
         continue;
      }
      lListElem *ep = lCreateElem(descr);
      if (!spool_json_populate_members(ep, descr, fields, k == 0 ? fields_out : nullptr, answer_list, (*arr)[k])) {
         lFreeElem(&ep);
         lFreeList(&list);
         DRETURN(nullptr);
      }
      lAppendElem(list, ep);
   }
   DRETURN(list);
}
