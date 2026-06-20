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

#include "sgeobj/sge_object.h"           /* object_get_type/_name, object_get_subtype */
#include "sgeobj/sge_attr.h"             /* HOSTREF_DEFAULT ("@/") */
#include "sgeobj/sge_centry.h"           /* CE_Type / CE_* for complex-value typing */
#include "sgeobj/sge_qinstance_type.h"   /* queue_types[] for qtype <-> name list */
#include "sgeobj/sge_userset.h"          /* userset_types[] for US_type <-> name list */
#include "sgeobj/sge_sharetree.h"        /* STN_type / STT_USER / STT_PROJECT (enum names) */
#include "sgeobj/sge_ulong.h"            /* double_print_to_dstring (compact time/mem) */
#include "uti/sge_time.h"                /* sge_ctime64 / sge_gmt32_to_gmt64 (ISO date-time) */
#include <ctime>                         /* struct tm, mktime (ISO date-time parse) */
#include "sgeobj/sge_conf.h"             /* config_param_list_type (config list params) */
#include "sgeobj/ocs_CEntry.h"           /* ocs::CEntry::Type */
#include "sgeobj/sge_answer.h"           /* answer_list_add_sprintf */
#include "sgeobj/config.h"               /* add_nm_to_set */
#include "sgeobj/sge_utility.h"          /* SGE_CHECK_POINTER_FALSE */

#include "spool/sge_spooling_utilities.h"

#include "msg_common.h"                  /* MSG_NULLELEMENTPASSEDTO_S (SGE_CHECK_POINTER_FALSE) */

#include "ocs_spool_json.h"

#define JSON_LAYER BASIS_LAYER

/* default value format: compact unit/colon strings for TIME/MEM (see ocs_spool_json.h) */
ocs_json_value_format ocs_json_value_format_opt = OCS_JSON_VALUES_COMPACT;

/* forward declarations (mutually recursive sublist handling) */
static bool
spool_json_write_object_members(lList **answer_list, const lListElem *object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields);
static bool
spool_json_write_list_array(lList **answer_list, const lList *list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields);
static bool
spool_json_write_object_braced(lList **answer_list, const lListElem *object,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const spooling_field *fields);
static bool
spool_json_write_positional_array(lList **answer_list, const lList *list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static bool
spool_json_write_list_value(lList **answer_list, const lList *sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer);
static lUlong
spool_json_value_to_ulong(const rapidjson::Value &v);

/* the JSON token shown for the cqueue default host reference HOSTREF_DEFAULT ("@/") */
static const char *const SPOOL_JSON_DEFAULT_HREF = "default";

/**
 * @brief Sub-element descriptor for a sub-object (lObjectT) field.
 *
 * object_get_subtype() is generated only for CULL SGE_LIST fields, so it does not
 * cover SGE_OBJECT fields (e.g. the RQS filters); those carry their descriptor in
 * the spooling_field clientdata, which is unused for object fields on the ASCII path.
 *
 * @param field  spooling field describing the sub-object attribute
 * @return the sub-element descriptor, or nullptr if none can be determined
 */
static const lDescr *
spool_json_object_subtype(const spooling_field *field)
{
   const lDescr *d = object_get_subtype(field->nm);
   if (d == nullptr) {
      d = static_cast<const lDescr *>(field->clientdata);
   }
   return d;
}

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
spool_json_typename(const lListElem *object, dstring *out)
{
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
                               const char *type_name)
{
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
                          const lListElem *object)
{
   dstring name = DSTRING_INIT;
   spool_json_write_envelope_name(writer, spool_json_typename(object, &name));
   sge_dstring_free(&name);
}

/**
 * @brief Write one scalar field value (no key) typed from the CULL field type.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field
 * @param pos     position of the field in the element's descriptor
 * @param type    CULL field type (lDoubleT, lUlongT, lStringT, ...)
 */
static void
spool_json_write_scalar_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *object, int pos, int type)
{
   switch (type) {
      case lDoubleT:  writer.Double(lGetPosDouble(object, pos)); break;
      case lUlongT:   writer.Uint64(lGetPosUlong(object, pos)); break;
      case lUlong64T: writer.Uint64(lGetPosUlong64(object, pos)); break;
      case lLongT:    writer.Int64(lGetPosLong(object, pos)); break;
      case lIntT:     writer.Int(lGetPosInt(object, pos)); break;
      case lBoolT:    writer.Bool(lGetPosBool(object, pos)); break;
      case lStringT: {
         const char *s = lGetPosString(object, pos);
         /* unset string -> empty string. null, "" and the "NONE" sentinel all mean
            unset, so they render uniformly as "" instead of leaking the internal "NONE"
            token into JSON. The reader maps "" back to the "NONE" sentinel string (not
            C-null), mirroring the flatfile reader, so a required field whose unset value
            is "NONE" (e.g. a ckpt interface's command fields) round-trips correctly. */
         writer.String((s == nullptr || sge_strnullcasecmp(s, NONE_STR) == 0) ? "" : s);
         break;
      }
      case lHostT: {
         const char *s = lGetPosHost(object, pos);
         writer.String((s == nullptr || sge_strnullcasecmp(s, NONE_STR) == 0) ? "" : s);
         break;
      }
      default: writer.Null(); break;
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
                              double dval, ocs::CEntry::Type type)
{
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
 * @param ce      a CE_Type complex-entry element
 */
static void
spool_json_write_ce_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const lListElem *ce)
{
   const double dval = lGetDouble(ce, CE_doubleval);
   const auto type = static_cast<ocs::CEntry::Type>(lGetUlong(ce, CE_valtype));
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
         const char *s = lGetString(ce, CE_stringval);
         s != nullptr ? writer.String(s) : writer.Null();
         break;
      }
   }
}

/**
 * @brief Write a complex-value / threshold sublist as a JSON array.
 *
 * Renders the CE_Type elements as { "name": <name>, "value": <typed> } objects.
 *
 * @param writer  pretty-printer to emit into
 * @param list    list of CE_Type complex-entry elements
 */
static void
spool_json_write_ce_array(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const lList *list)
{
   writer.StartArray();
   const lListElem *ce;
   for_each_ep(ce, list) {
      writer.StartObject();
      const char *cname = lGetString(ce, CE_name);
      writer.Key("name");
      cname != nullptr ? writer.String(cname) : writer.Null();
      writer.Key("value");
      spool_json_write_ce_value(writer, ce);
      writer.EndObject();
   }
   writer.EndArray();
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
spool_json_is_ce_list(const lList *list)
{
   const lDescr *d = lGetListDescr(list);
   return d != nullptr &&
          lGetPosInDescr(d, CE_name) >= 0 &&
          lGetPosInDescr(d, CE_stringval) >= 0 &&
          lGetPosInDescr(d, CE_valtype) >= 0;
}

/**
 * @brief Test whether a field is rendered as a symbolic enum token.
 *
 * These are fields whose CULL value is an enum/bitfield that the flatfile (and qstat)
 * render as a symbolic token rather than a raw number - exactly the scalar
 * special-cases of object_append_field_to_dstring() / object_parse_field_from_string()
 * in sgeobj/sge_object.cc. For JSON we emit the same human-readable token (CE_valtype
 * 1 -> "INT", CE_relop 5 -> "<=", CE_requestable -> "YES", ...) and parse it back
 * symbolically on read, so the JSON form matches the ASCII form and round-trips.
 *
 * @param nm  CULL field id
 * @return true if the field is rendered/parsed as a symbolic token
 */
static bool
spool_json_nm_is_symbolic(int nm)
{
   switch (nm) {
      case CE_valtype:
      case CE_relop:
      case CE_requestable:
      case CE_consumable:
         return true;
      default:
         return false;
   }
}

/**
 * @brief Write a field as its symbolic string token.
 *
 * See spool_json_nm_is_symbolic() for the set of symbolic fields.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field
 * @param nm      CULL field id to render symbolically
 */
static void
spool_json_write_symbolic_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const lListElem *object, int nm)
{
   dstring buf = DSTRING_INIT;
   const char *s = object_append_field_to_dstring(object, nullptr, &buf, nm, '\0');
   s != nullptr ? writer.String(s) : writer.Null();
   sge_dstring_free(&buf);
}

/**
 * @brief Test whether a string-stored field is semantically always a number.
 *
 * These are fields that CULL stores as a string but that are semantically always a
 * (finite) number - e.g. CE_urgency_weight, which the scheduler always parses as a
 * double. For JSON we emit a native number. The reader's lStringT path coerces the
 * number back to its decimal string, so no special read handling is needed.
 *
 * @param nm  CULL field id
 * @return true if the field should be emitted as a native JSON number
 */
static bool
spool_json_nm_is_number(int nm)
{
   switch (nm) {
      case CE_urgency_weight:    /* always a finite double */
      case ASTR_value:           /* cqueue string override (e.g. priority) -> number if numeric */
      case PE_allocation_rule:   /* "$pe_slots"/... (string) or a fixed slot count (number) */
      case PE_urgency_slots:     /* "min"/"max"/"avg" (string) or a slot count (number) */
         return true;
      default:
         return false;
   }
}

/**
 * @brief Write a string-stored numeric field as a native JSON number.
 *
 * Integral values are written as an integer, others as a double. A null, empty, or
 * "NONE" token -> ""; any other token that does not parse fully to a finite number
 * (e.g. "INFINITY") -> the raw string.
 *
 * @param writer  pretty-printer to emit into
 * @param object  element holding the field
 * @param nm      CULL field id (a string field that is semantically numeric)
 */
static void
spool_json_write_number_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *object, int nm)
{
   const char *s = lGetString(object, nm);
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
 * @brief Complex type used to type a string-stored, semantically numeric override value.
 *
 * ATIME/AINTER values are times (-> seconds), AMEM values are memory (-> bytes). Other
 * value fields are not numerically typed here.
 *
 * @param nm  CULL field id of a cqueue override value attribute
 * @return the complex entry type (TIME/MEM), or NONE if not numerically typed
 */
static ocs::CEntry::Type
spool_json_avalue_ctype(int nm)
{
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
                        const char *s, ocs::CEntry::Type type)
{
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
 * @brief Name table for a bitfield field rendered as an array of set bit names.
 *
 * Bitfield fields are rendered in JSON as an array of the set bit names (bit i ->
 * names[i], e.g. qtype -> ["BATCH","INTERACTIVE"], userset type -> ["ACL","DEPT"])
 * instead of a raw bitmask number.
 *
 * @param nm  CULL field id
 * @return the bit-name table for the field, or nullptr if not a bitfield
 */
static const char *const *
spool_json_bitfield_names(int nm)
{
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
 * @brief Write a bitfield value as a JSON array of the set bit names.
 *
 * @param writer  pretty-printer to emit into
 * @param value   the bitmask
 * @param names   null-terminated bit-name table (names[i] is the name of bit i)
 */
static void
spool_json_write_bitfield_array(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                uint32_t value, const char *const *names)
{
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

/* sharetree node type STN_type (STT_USER=0, STT_PROJECT=1); the table is indexed by the
 * enum value, so the order must match the STT_* constants in sgeobj/sge_sharetree.h. */
static const char *const sharetree_node_types[] = { "USER", "PROJECT", nullptr };

/* scheduler queue sort method SC_queue_sort_method (QSM_LOAD=0, QSM_SEQNUM=1); the table
 * is indexed by the enum value and matches the tokens of write_SC_queue_sort_method. */
static const char *const sched_queue_sort_methods[] = { "load", "seqno", nullptr };

/**
 * @brief Name table for a single-valued enum field rendered as a symbolic token.
 *
 * Unlike a bitfield (an array of set-bit names), this is a plain enum whose value is an
 * index into the table (e.g. STN_type 0 -> "USER", 1 -> "PROJECT"). The flatfile (ASCII)
 * writer renders these via per-field write functions; JSON bypasses those, so the
 * mapping is mirrored here to keep the symbolic form (and round-trip) consistent.
 *
 * @param nm  CULL field id
 * @return the value-name table for the field, or nullptr if not such an enum
 */
static const char *const *
spool_json_enum_names(int nm)
{
   switch (nm) {
      case STN_type:
         return sharetree_node_types;
      case SC_queue_sort_method:
         return sched_queue_sort_methods;
      default:
         return nullptr;
   }
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
                            uint32_t value, const char *const *names)
{
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
spool_json_enum_from_value(const rapidjson::Value &v, const char *const *names)
{
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
 * @brief Test whether a field is an absolute timestamp rendered as an ISO date-time.
 *
 * Absolute-timestamp fields (gmt64 microseconds since epoch) are rendered as a local
 * ISO date-time string ("2026-06-13T14:30:13"); 0/unset -> "".
 *
 * @param nm  CULL field id
 * @return true if the field is a date-time field
 */
static bool
spool_json_nm_is_datetime(int nm)
{
   switch (nm) {
      case UU_delete_time:
         return true;
      default:
         return false;
   }
}

/**
 * @brief Write a gmt64 timestamp as a local ISO date-time string.
 *
 * @param writer  pretty-printer to emit into
 * @param ts      timestamp in gmt64 microseconds since epoch; 0 -> "" (unset)
 */
static void
spool_json_write_datetime(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer, uint64_t ts)
{
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
spool_json_datetime_from_string(const char *s)
{
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
 * Routes through the date-time, bitfield, symbolic, complex-typed, numeric, and plain
 * scalar branches (in that precedence); a field absent from the descriptor -> null.
 *
 * @param writer  pretty-printer to emit into
 * @param ep      element holding the field
 * @param nm      CULL field id to write
 */
static void
spool_json_write_field_scalar(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *ep, int nm)
{
   const lDescr *d = lGetElemDescr(ep);
   const int pos = lGetPosInDescr(d, nm);
   const ocs::CEntry::Type avtype = spool_json_avalue_ctype(nm);
   const char *const *bitnames = spool_json_bitfield_names(nm);
   const char *const *enumnames = spool_json_enum_names(nm);
   if (pos < 0) {
      writer.Null();
   } else if (spool_json_nm_is_datetime(nm)) {
      spool_json_write_datetime(writer, lGetPosUlong64(ep, pos));
   } else if (bitnames != nullptr) {
      spool_json_write_bitfield_array(writer, lGetPosUlong(ep, pos), bitnames);
   } else if (enumnames != nullptr) {
      spool_json_write_enum_value(writer, lGetPosUlong(ep, pos), enumnames);
   } else if (spool_json_nm_is_symbolic(nm)) {
      spool_json_write_symbolic_value(writer, ep, nm);
   } else if (avtype != ocs::CEntry::Type::NONE) {
      spool_json_write_avalue(writer, lGetPosString(ep, pos), avtype);
   } else if (spool_json_nm_is_number(nm)) {
      spool_json_write_number_value(writer, ep, nm);
   } else {
      spool_json_write_scalar_value(writer, ep, pos, lGetPosType(d, pos));
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
 * @param nm      CULL field id of the element's name field
 */
static void
spool_json_write_positional_name(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                 const lListElem *ep, int nm)
{
   const lDescr *d = lGetElemDescr(ep);
   const int pos = lGetPosInDescr(d, nm);
   if (pos >= 0 && lGetPosType(d, pos) == lHostT) {
      const char *h = lGetPosHost(ep, pos);
      if (h != nullptr && strcmp(h, HOSTREF_DEFAULT) == 0) {
         writer.String(SPOOL_JSON_DEFAULT_HREF);
      } else {
         writer.String((h != nullptr && *h != '\0') ? h : "");
      }
   } else {
      spool_json_write_field_scalar(writer, ep, nm);
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
 * @param answer_list  for returning errors
 * @param list         the positional sublist
 * @param fields       the (unnamed) spooling fields of the sublist
 * @param writer       pretty-printer to emit into
 * @return true on success, false on error (answer_list set)
 */
static bool
spool_json_write_positional_array(lList **answer_list, const lList *list,
                                  const spooling_field *fields,
                                  rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer)
{
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   writer.StartArray();
   const lListElem *ep;
   for_each_ep(ep, list) {
      if (nfields <= 1) {
         spool_json_write_positional_name(writer, ep, fields[0].nm);
      } else {
         writer.StartObject();
         writer.Key("name");
         spool_json_write_positional_name(writer, ep, fields[0].nm);
         writer.Key("value");
         const lDescr *d = lGetElemDescr(ep);
         const int vpos = lGetPosInDescr(d, fields[1].nm);
         if (vpos >= 0 && lGetPosType(d, vpos) == lListT) {
            if (!spool_json_write_list_value(answer_list, lGetPosList(ep, vpos), fields[1].sub_fields, writer)) {
               return false;
            }
         } else {
            spool_json_write_field_scalar(writer, ep, fields[1].nm);
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
spool_json_write_list_value(lList **answer_list, const lList *sub_list,
                            const spooling_field *sub_fields,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer)
{
   if (sub_list == nullptr || lGetNumberOfElem(sub_list) == 0) {
      writer.StartArray();
      writer.EndArray();
   } else if (spool_json_is_ce_list(sub_list)) {
      spool_json_write_ce_array(writer, sub_list);
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
                              const char *val, ocs::CEntry::Type type)
{
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
                                   const char *s)
{
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
                                       const char *s)
{
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
spool_json_nm_is_perm_list(int nm)
{
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
                           const char *s)
{
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
spool_json_perm_list_from_array(const rapidjson::Value &v, dstring *out, lList **answer_list)
{
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
spool_json_write_object_members(lList **answer_list, const lListElem *object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields)
{
   DENTER(JSON_LAYER);

   SGE_CHECK_POINTER_FALSE(object, answer_list);
   SGE_CHECK_POINTER_FALSE(fields, answer_list);

   const lDescr *descr = lGetElemDescr(object);

   for (int i = 0; fields[i].nm != NoName; i++) {
      const int nm = fields[i].nm;
      const char *name = fields[i].name;

      /* JSON needs a key. A top-level unnamed name=value sublist (the config entries,
         CONF_entries) is flattened: each "<name>": <value> becomes a member of the
         object, mirroring the flat "parameter value" config format. Other unnamed
         (structural) fields are skipped. */
      if (name == nullptr) {
         const int fpos = lGetPosInDescr(descr, nm);
         const spooling_field *sf = fields[i].sub_fields;
         if (fpos >= 0 && lGetPosType(descr, fpos) == lListT && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            const lList *sub = lGetPosList(object, fpos);
            const lListElem *e;
            for_each_ep(e, sub) {
               const char *key = lGetString(e, sf[0].nm);
               if (key == nullptr) {
                  continue;
               }
               writer.Key(key);
               /* config list params render as arrays (grounded in the config loader's
                * parsing); everything else is a scalar string */
               switch (config_param_list_type(key)) {
                  case CONF_PARAM_VALUE_LIST:
                     spool_json_write_config_value_list(writer, lGetString(e, sf[1].nm));
                     break;
                  case CONF_PARAM_NAMEVALUE_LIST:
                     spool_json_write_config_namevalue_list(writer, lGetString(e, sf[1].nm));
                     break;
                  default:
                     /* scalar: typed by value (bool) or by the known INT/TIME type;
                      * "NONE"/empty -> ""; otherwise a string */
                     spool_json_write_config_value(writer, lGetString(e, sf[1].nm),
                                                   config_param_value_type(key));
                     break;
               }
            }
         }
         continue;
      }
      const int pos = lGetPosInDescr(descr, nm);
      if (pos < 0) {
         continue;
      }

      const int type = lGetPosType(descr, pos);
      if (type == lListT) {
         writer.Key(name);
         if (!spool_json_write_list_value(answer_list, lGetPosList(object, pos), fields[i].sub_fields, writer)) {
            DRETURN(false);
         }
      } else if (type == lObjectT && fields[i].sub_fields != nullptr) {
         /* a sub-object (e.g. an RQS filter) -> nested JSON object built from its
            sub_fields. An unset (null) sub-object is still emitted as its full empty
            structure (rendered from a transient default element) so the JSON always
            shows the complete syntax, e.g. { "expand": false, "scope": [], "xscope":
            [] }. The reader maps empty arrays to null lists, so this round-trips to
            "no filter" (the matcher treats a null scope as "match all"). */
         writer.Key(name);
         const lListElem *real = lGetPosObject(object, pos);
         const lDescr *sub_descr = spool_json_object_subtype(&fields[i]);
         lListElem *tmp = nullptr;
         const lListElem *sub_obj = real;
         if (sub_obj == nullptr && sub_descr != nullptr) {
            tmp = lCreateElem(sub_descr);
            sub_obj = tmp;
         }
         bool ok = true;
         if (sub_obj != nullptr) {
            ok = spool_json_write_object_braced(answer_list, sub_obj, writer, fields[i].sub_fields);
         } else {
            writer.Null();
         }
         lFreeElem(&tmp);
         if (!ok) {
            DRETURN(false);
         }
      } else {
         writer.Key(name);
         const char *const *bitnames = spool_json_bitfield_names(nm);
         const char *const *enumnames = spool_json_enum_names(nm);
         if (spool_json_nm_is_datetime(nm)) {
            spool_json_write_datetime(writer, lGetPosUlong64(object, pos));
         } else if (bitnames != nullptr) {
            spool_json_write_bitfield_array(writer, lGetPosUlong(object, pos), bitnames);
         } else if (enumnames != nullptr) {
            spool_json_write_enum_value(writer, lGetPosUlong(object, pos), enumnames);
         } else if (spool_json_nm_is_perm_list(nm)) {
            spool_json_write_perm_list(writer, lGetPosString(object, pos));
         } else if (spool_json_nm_is_symbolic(nm)) {
            spool_json_write_symbolic_value(writer, object, nm);
         } else if (spool_json_nm_is_number(nm)) {
            spool_json_write_number_value(writer, object, nm);
         } else {
            spool_json_write_scalar_value(writer, object, pos, type);
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
spool_json_write_object_braced(lList **answer_list, const lListElem *object,
                               rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                               const spooling_field *fields)
{
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
spool_json_write_list_array(lList **answer_list, const lList *list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields)
{
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
bool
spool_json_write_object(lList **answer_list, const lListElem *object,
                        const spooling_field *fields, dstring *out)
{
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   writer.StartObject();
   spool_json_write_envelope(writer, object);
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
bool
spool_json_write_list(lList **answer_list, const lList *list,
                      const spooling_field *fields, dstring *out)
{
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
bool
spool_json_write_typed_object(lList **answer_list, const lListElem *object,
                              const spooling_field *fields, const char *type_name, dstring *out)
{
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
 * @param id_name        type name for the $id (ocs-qconf-<id_name>.schema.json)
 * @param envelope_name  the array key wrapping the records
 * @param out            dstring the JSON document is appended to
 * @return true on success, false on error (answer_list set)
 */
bool
spool_json_write_typed_list_ex(lList **answer_list, const lList *list,
                               const spooling_field *fields, const char *id_name,
                               const char *envelope_name, dstring *out)
{
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
bool
spool_json_write_typed_list(lList **answer_list, const lList *list,
                            const spooling_field *fields, const char *type_name, dstring *out)
{
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
bool
spool_json_write_name_list(lList **answer_list, const lList *list, int keynm, dstring *out)
{
   DENTER(JSON_LAYER);

   rapidjson::StringBuffer sb;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
   writer.SetIndent(' ', 3);

   const lListElem *first = lFirst(list);
   dstring key = DSTRING_INIT;

   /* same $schema/$id envelope as the object lists, but the array holds bare names */
   writer.StartObject();
   if (first != nullptr) {
      /* CS-2320: a name list carries its own "<type>-list" schema id so it is
       * distinguishable from the object schema and from a record list that shares
       * the same envelope key (e.g. -sc vs -scel, both "complex_entry"); the array
       * key itself stays the plain object type name. */
      const char *type_name = spool_json_typename(first, &key);
      dstring list_id = DSTRING_INIT;
      sge_dstring_sprintf(&list_id, "%s-list", type_name);
      spool_json_write_envelope_name(writer, sge_dstring_get_string(&list_id));
      sge_dstring_free(&list_id);
      writer.Key(type_name);
   } else {
      writer.Key("names");
   }
   writer.StartArray();
   const lListElem *ep;
   for_each_ep(ep, list) {
      const lDescr *d = lGetElemDescr(ep);
      const int pos = lGetPosInDescr(d, keynm);
      if (pos < 0) {
         continue;
      }
      const char *s = (lGetPosType(d, pos) == lHostT) ? lGetPosHost(ep, pos) : lGetPosString(ep, pos);
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
spool_json_value_to_string(const rapidjson::Value &v, dstring *out)
{
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
spool_json_value_to_ulong(const rapidjson::Value &v)
{
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
spool_json_value_to_bool(const rapidjson::Value &v)
{
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
 * nulls the nullable fields (NULL_OUT_NONE) but keeps the required ones (e.g. a ckpt
 * interface's command fields, which reject null). This makes JSON input behave
 * identically to ASCII input and round-trip for both field kinds.
 *
 * @param v    the JSON value
 * @param out  scratch buffer for the string form
 * @return the string value, or "NONE" if unset (never nullptr)
 */
static const char *
spool_json_string_or_unset(const rapidjson::Value &v, dstring *out)
{
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
 * @brief Set a field (pos/type) of an element from a JSON value, coercing per CULL type.
 *
 * The inverse of spool_json_write_scalar_value (raw set, bypassing read_func, to mirror
 * the writer which bypassed write_func).
 *
 * @param ep    element to populate
 * @param pos   position of the field in the descriptor
 * @param type  CULL field type
 * @param v     the JSON value
 */
static void
spool_json_set_field(lListElem *ep, int pos, int type, const rapidjson::Value &v)
{
   dstring s = DSTRING_INIT;
   switch (type) {
      case lDoubleT:
         lSetPosDouble(ep, pos, v.IsNumber() ? v.GetDouble()
                                : (v.IsString() ? strtod(v.GetString(), nullptr) : 0.0));
         break;
      case lUlongT:
         lSetPosUlong(ep, pos, spool_json_value_to_ulong(v));
         break;
      case lUlong64T:
         lSetPosUlong64(ep, pos, v.IsUint64() ? v.GetUint64()
                                 : static_cast<lUlong64>(spool_json_value_to_ulong(v)));
         break;
      case lLongT:
         lSetPosLong(ep, pos, v.IsInt64() ? v.GetInt64()
                              : static_cast<lLong>(spool_json_value_to_ulong(v)));
         break;
      case lIntT:
         lSetPosInt(ep, pos, v.IsInt() ? v.GetInt()
                             : static_cast<int>(spool_json_value_to_ulong(v)));
         break;
      case lBoolT:
         lSetPosBool(ep, pos, spool_json_value_to_bool(v));
         break;
      case lStringT:
         lSetPosString(ep, pos, spool_json_string_or_unset(v, &s));
         break;
      case lHostT:
         lSetPosHost(ep, pos, spool_json_string_or_unset(v, &s));
         break;
      default:
         break;
   }
   sge_dstring_free(&s);
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
spool_json_bitfield_from_array(const rapidjson::Value &v, const char *const *names)
{
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
 * @param ep    element to populate
 * @param pos   position of the name field in the descriptor
 * @param type  CULL field type of the name field
 * @param v     the JSON value
 */
static void
spool_json_set_positional_name(lListElem *ep, int pos, int type, const rapidjson::Value &v)
{
   if (type == lHostT && v.IsString() && strcmp(v.GetString(), SPOOL_JSON_DEFAULT_HREF) == 0) {
      lSetPosHost(ep, pos, HOSTREF_DEFAULT);
   } else {
      spool_json_set_field(ep, pos, type, v);
   }
}

/* forward */
static bool spool_json_populate_members(lListElem *ep, const lDescr *descr,
                                        const spooling_field *fields, int fields_out[],
                                        lList **answer_list, const rapidjson::Value &obj);

/**
 * @brief Element descriptor of a sublist field.
 *
 * object_get_subtype() (generated from the CULL metadata) covers fields whose subtype
 * is fixed, but the cqueue per-host override attributes are declared CULL_ANY_SUBTYPE,
 * so their element type is absent there. For those, derive it from the href sub-field
 * (AULNG_href -> AULNG_Type, ...).
 *
 * @param nm      CULL field id of the sublist attribute
 * @param fields  spooling fields of the sublist elements (used for the fallback)
 * @return the element descriptor, or nullptr if it cannot be determined
 */
static const lDescr *
spool_json_sublist_subtype(int nm, const spooling_field *fields)
{
   const lDescr *d = object_get_subtype(nm);
   if (d != nullptr || fields == nullptr) {
      return d;
   }
   switch (fields[0].nm) {
      case AULNG_href:    return AULNG_Type;
      case ASTR_href:     return ASTR_Type;
      case AMEM_href:     return AMEM_Type;
      case ATIME_href:    return ATIME_Type;
      case AINTER_href:   return AINTER_Type;
      case ABOOL_href:    return ABOOL_Type;
      case ACELIST_href:  return ACELIST_Type;
      case ASTRLIST_href: return ASTRLIST_Type;
      case AUSRLIST_href: return AUSRLIST_Type;
      case ASOLIST_href:  return ASOLIST_Type;
      case APRJLIST_href: return APRJLIST_Type;
      case AQTLIST_href:  return AQTLIST_Type;
      default:            return nullptr;
   }
}

/**
 * @brief Build a sublist from a JSON array, mirroring the writer's shape decisions.
 *
 * Positional (unnamed spooling fields) -> 1 field: array of scalars, 2 fields: array of
 * {name,value}; named fields (incl. CE) -> array of objects keyed by name. An empty
 * array is read as a null (unset) list, mirroring the ASCII reader.
 *
 * @param nm      CULL field id of the sublist attribute
 * @param fields  spooling fields describing the sublist elements
 * @param arr     the JSON array
 * @return the populated list, or nullptr for an empty/unreadable array
 */
static lList *
spool_json_read_sublist(int nm, const spooling_field *fields, lList **answer_list,
                        const rapidjson::Value &arr)
{
   const lDescr *sub_descr = spool_json_sublist_subtype(nm, fields);
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
         const int p0 = lGetPosInDescr(sub_descr, fields[0].nm);
         if (p0 >= 0) {
            spool_json_set_positional_name(sub, p0, lGetPosType(sub_descr, p0), el);
         }
      } else if (positional && el.IsObject()) {
         const auto mn = el.FindMember("name");
         const auto mv = el.FindMember("value");
         const int p0 = lGetPosInDescr(sub_descr, fields[0].nm);
         const int p1 = lGetPosInDescr(sub_descr, fields[1].nm);
         if (p0 >= 0 && mn != el.MemberEnd()) {
            spool_json_set_positional_name(sub, p0, lGetPosType(sub_descr, p0), mn->value);
         }
         if (p1 >= 0 && mv != el.MemberEnd()) {
            const int vtype = lGetPosType(sub_descr, p1);
            const char *const *bitnames = spool_json_bitfield_names(fields[1].nm);
            if (bitnames != nullptr) {
               /* bitfield value is a name array -> bitmask */
               lSetPosUlong(sub, p1, spool_json_bitfield_from_array(mv->value, bitnames));
            } else if (vtype == lListT) {
               /* the value is itself a list (e.g. the cqueue threshold value is a
                * complex list) -> read it as a nested sublist */
               lSetPosList(sub, p1, spool_json_read_sublist(fields[1].nm, fields[1].sub_fields, answer_list, mv->value));
            } else {
               spool_json_set_field(sub, p1, vtype, mv->value);
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
 * (read_CF_value, which validates) when present.
 *
 * @param ep          element to populate
 * @param descr       descriptor of @p ep
 * @param fields      spooling fields of the object (to exclude named members)
 * @param flatten     the unnamed name=value sublist field to fill
 * @param fields_out  if non-null, receives the flattened field id
 * @param obj         the JSON object
 */
static void
spool_json_read_flatten(lListElem *ep, const lDescr *descr, const spooling_field *fields,
                        const spooling_field *flatten, int fields_out[],
                        const rapidjson::Value &obj)
{
   const int fpos = lGetPosInDescr(descr, flatten->nm);
   const lDescr *sub_descr = spool_json_sublist_subtype(flatten->nm, flatten->sub_fields);
   if (fpos < 0 || sub_descr == nullptr) {
      return;
   }
   const spooling_field *sf = flatten->sub_fields;
   const int kpos = lGetPosInDescr(sub_descr, sf[0].nm);
   const int vpos = lGetPosInDescr(sub_descr, sf[1].nm);
   if (kpos < 0 || vpos < 0) {
      return;
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
      lSetPosString(cf, kpos, key);
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
      if (sf[1].read_func != nullptr) {
         sf[1].read_func(cf, sf[1].nm, vstr, nullptr);
      } else {
         lSetPosString(cf, vpos, vstr);
      }
      sge_dstring_free(&vb);
      lAppendElem(cf_list, cf);
   }
   lSetPosList(ep, fpos, cf_list);
   if (fields_out != nullptr) {
      add_nm_to_set(fields_out, flatten->nm);
   }
}

/**
 * @brief Set the members of an element from a JSON object.
 *
 * Walks the spooling fields, looking each up by name (the inverse of
 * spool_json_write_object_members); a trailing flatten field is un-flattened after the
 * named pass.
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
                            lList **answer_list, const rapidjson::Value &obj)
{
   const spooling_field *flatten = nullptr;
   for (int i = 0; fields[i].nm != NoName; i++) {
      const int nm = fields[i].nm;
      const char *name = fields[i].name;
      if (name == nullptr) {
         /* a top-level unnamed name=value sublist (config entries) is un-flattened
            after the named pass by gathering the remaining members */
         const int fpos = lGetPosInDescr(descr, nm);
         const spooling_field *sf = fields[i].sub_fields;
         if (fpos >= 0 && lGetPosType(descr, fpos) == lListT && sf != nullptr &&
             sf[0].nm != NoName && sf[0].name == nullptr && sf[1].nm != NoName) {
            flatten = &fields[i];
         }
         continue;
      }
      const int pos = lGetPosInDescr(descr, nm);
      if (pos < 0) {
         continue;
      }
      const auto m = obj.FindMember(name);
      if (m == obj.MemberEnd()) {
         continue;   /* field absent in the JSON -> simply not read */
      }
      if (fields_out != nullptr) {
         add_nm_to_set(fields_out, nm);
      }

      const int type = lGetPosType(descr, pos);
      if (type == lListT) {
         lSetPosList(ep, pos, spool_json_read_sublist(nm, fields[i].sub_fields, answer_list, m->value));
      } else if (type == lObjectT && fields[i].sub_fields != nullptr) {
         /* nested JSON object (e.g. an RQS filter) -> sub-object; null / non-object
          * leaves the field unset, so an absent filter round-trips to null */
         const lDescr *sub_descr = spool_json_object_subtype(&fields[i]);
         if (m->value.IsObject() && sub_descr != nullptr) {
            lListElem *sub = lCreateElem(sub_descr);
            if (!spool_json_populate_members(sub, sub_descr, fields[i].sub_fields, nullptr, answer_list, m->value)) {
               lFreeElem(&sub);
               return false;
            }
            lSetPosObject(ep, pos, sub);
         }
      } else if (spool_json_nm_is_datetime(nm)) {
         /* ISO date-time string -> gmt64 timestamp */
         dstring s = DSTRING_INIT;
         lSetPosUlong64(ep, pos, spool_json_datetime_from_string(spool_json_value_to_string(m->value, &s)));
         sge_dstring_free(&s);
      } else if (spool_json_bitfield_names(nm) != nullptr) {
         /* bit-name array -> bitfield */
         lSetPosUlong(ep, pos, spool_json_bitfield_from_array(m->value, spool_json_bitfield_names(nm)));
      } else if (spool_json_enum_names(nm) != nullptr) {
         /* symbolic enum token ("USER"/"PROJECT") or raw number -> enum value */
         lSetPosUlong(ep, pos, spool_json_enum_from_value(m->value, spool_json_enum_names(nm)));
      } else if (spool_json_nm_is_perm_list(nm)) {
         /* array of rule objects -> comma/colon-joined perm_list string; a non-array
          * value is rejected rather than silently wiping the rule set to NONE */
         dstring s = DSTRING_INIT;
         const bool ok = spool_json_perm_list_from_array(m->value, &s, answer_list);
         if (ok) {
            lSetPosString(ep, pos, sge_dstring_get_string(&s));
         }
         sge_dstring_free(&s);
         if (!ok) {
            return false;
         }
      } else if (spool_json_nm_is_symbolic(nm)) {
         /* symbolic enum token ("INT", "<=", "YES", ...) -> enum value */
         dstring s = DSTRING_INIT;
         object_parse_field_from_string(ep, nullptr, nm, spool_json_value_to_string(m->value, &s));
         sge_dstring_free(&s);
      } else {
         spool_json_set_field(ep, pos, type, m->value);
      }
   }
   if (flatten != nullptr) {
      spool_json_read_flatten(ep, descr, fields, flatten, fields_out, obj);
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
spool_json_read_object(lList **answer_list, const lDescr *descr,
                       const spooling_field *fields, int fields_out[], const char *json_text)
{
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
spool_json_read_list(lList **answer_list, const lDescr *descr,
                     const spooling_field *fields, int fields_out[], const char *json_text)
{
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
