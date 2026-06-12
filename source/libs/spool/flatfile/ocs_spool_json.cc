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
#include <cstdint>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/ocs_JsonUtil.h"            /* rapidjson Writer + write_json helpers */
#include <rapidjson/prettywriter.h>      /* indented (pretty) JSON output */

#include "cull/cull.h"

#include "sgeobj/sge_object.h"           /* object_get_type/_name for the JSON $id */
#include "sgeobj/sge_centry.h"           /* CE_Type / CE_* for complex-value typing */
#include "sgeobj/ocs_CEntry.h"           /* ocs::CEntry::Type */
#include "sgeobj/sge_utility.h"          /* SGE_CHECK_POINTER_FALSE */

#include "spool/sge_spooling_utilities.h"

#include "msg_common.h"                  /* MSG_NULLELEMENTPASSEDTO_S (SGE_CHECK_POINTER_FALSE) */

#include "ocs_spool_json.h"

#define JSON_LAYER BASIS_LAYER

/* forward declarations (mutually recursive sublist handling) */
static bool
spool_json_write_object_members(lList **answer_list, const lListElem *object,
                                rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                const spooling_field *fields);
static bool
spool_json_write_list_array(lList **answer_list, const lList *list,
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                            const spooling_field *fields);

/* lowercase object type name (e.g. "CALENDAR" -> "calendar") into @p out;
 * used for the JSON $id and the list wrapper key. */
static const char *
spool_json_typename(const lListElem *object, dstring *out)
{
   const char *type_name = object_get_name(object_get_type(object));
   sge_dstring_clear(out);
   for (const char *c = type_name; c != nullptr && *c != '\0'; c++) {
      sge_dstring_append_char(out, (char)tolower((unsigned char)*c));
   }
   return sge_dstring_get_string(out);
}

/* emit the "$schema"/"$id" members of the top-level document, mirroring qstat's
 * convention. The $id is derived from the object's type name. */
static void
spool_json_write_envelope(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const lListElem *object)
{
   writer.Key("$schema");
   writer.String("https://json-schema.org/draft/2020-12/schema");

   dstring name = DSTRING_INIT;
   dstring id = DSTRING_INIT;
   sge_dstring_sprintf(&id,
      "https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/"
      "source/dist/util/resources/json-schemas/v9.2/ocs-qconf-%s.schema.json",
      spool_json_typename(object, &name));
   writer.Key("$id");
   writer.String(sge_dstring_get_string(&id));
   sge_dstring_free(&id);
   sge_dstring_free(&name);
}

/* write one scalar field value (no key) typed from the CULL field type. */
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
         s != nullptr ? writer.String(s) : writer.Null();
         break;
      }
      case lHostT: {
         const char *s = lGetPosHost(object, pos);
         s != nullptr ? writer.String(s) : writer.Null();
         break;
      }
      default: writer.Null(); break;
   }
}

/* write a complex-entry value typed per CE_valtype, mirroring qstat's
 * ocs::ProcedureView::show_resource_as_JSON_type(): MEM/TIME/INT -> number (bytes/
 * seconds/count from CE_doubleval), DOUBLE -> number, BOOL -> bool; string-like and
 * unfilled (NONE) values fall back to the raw CE_stringval. */
static void
spool_json_write_ce_value(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                          const lListElem *ce)
{
   switch (static_cast<ocs::CEntry::Type>(lGetUlong(ce, CE_valtype))) {
      case ocs::CEntry::Type::DOUBLE:
         writer.Double(lGetDouble(ce, CE_doubleval));
         break;
      case ocs::CEntry::Type::BOOL:
         writer.Bool(lGetDouble(ce, CE_doubleval) > 0.0);
         break;
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::TIME:
      case ocs::CEntry::Type::MEM:
         writer.Uint64(static_cast<uint64_t>(lGetDouble(ce, CE_doubleval)));
         break;
      default: {
         /* STR/CSTR/HOST/RESTR/RSMAP and NONE (unfilled config): raw string value */
         const char *s = lGetString(ce, CE_stringval);
         s != nullptr ? writer.String(s) : writer.Null();
         break;
      }
   }
}

/* a complex-value / threshold sublist (CE_Type elements) as a JSON array of
 * { "name": <name>, "value": <typed> } objects. */
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

/* true if a sublist holds complex-entry (CE) elements, detected by the presence of
 * the CE name/value/type fields rather than by descriptor pointer identity -
 * essential because a GDI-transported list carries a rebuilt descriptor that is not
 * the canonical CE_Type pointer. */
static bool
spool_json_is_ce_list(const lList *list)
{
   const lDescr *d = lGetListDescr(list);
   return d != nullptr &&
          lGetPosInDescr(d, CE_name) >= 0 &&
          lGetPosInDescr(d, CE_stringval) >= 0 &&
          lGetPosInDescr(d, CE_valtype) >= 0;
}

/* write the scalar value of field @p nm of @p ep, or null if absent */
static void
spool_json_write_field_scalar(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                              const lListElem *ep, int nm)
{
   const lDescr *d = lGetElemDescr(ep);
   const int pos = lGetPosInDescr(d, nm);
   if (pos < 0) {
      writer.Null();
   } else {
      spool_json_write_scalar_value(writer, ep, pos, lGetPosType(d, pos));
   }
}

/* a "positional" sublist whose spooling fields are unnamed (the names live in the
 * values - e.g. load_values, load_scaling, user_lists). A single field becomes an
 * array of scalar values (["a","b"]); two or more fields become an array of
 * { "name": <field0>, "value": <field1> } objects. */
static void
spool_json_write_positional_array(rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
                                  const lList *list, const spooling_field *fields)
{
   int nfields = 0;
   while (fields[nfields].nm != NoName) {
      nfields++;
   }

   writer.StartArray();
   const lListElem *ep;
   for_each_ep(ep, list) {
      if (nfields <= 1) {
         spool_json_write_field_scalar(writer, ep, fields[0].nm);
      } else {
         writer.StartObject();
         writer.Key("name");
         spool_json_write_field_scalar(writer, ep, fields[0].nm);
         writer.Key("value");
         spool_json_write_field_scalar(writer, ep, fields[1].nm);
         writer.EndObject();
      }
   }
   writer.EndArray();
}

/* emit the "key": value members of one object (no surrounding braces), keyed by the
 * spooling_field names and typed from the CULL field types. Scalars become native
 * JSON values; CE/complex-value sublists become arrays of typed {name,value};
 * name-only sublists become arrays of strings; other sublists become arrays of
 * objects. */
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

      /* JSON needs a key; structural/unnamed fields are skipped */
      if (name == nullptr) {
         continue;
      }
      const int pos = lGetPosInDescr(descr, nm);
      if (pos < 0) {
         continue;
      }

      const int type = lGetPosType(descr, pos);
      if (type == lListT) {
         writer.Key(name);
         const lList *sub_list = lGetPosList(object, pos);
         const spooling_field *sub_fields = fields[i].sub_fields;
         if (sub_list == nullptr || lGetNumberOfElem(sub_list) == 0) {
            writer.StartArray();
            writer.EndArray();
         } else if (spool_json_is_ce_list(sub_list)) {
            /* complex values / thresholds: typed {name,value} per CE_valtype */
            spool_json_write_ce_array(writer, sub_list);
         } else if (sub_fields != nullptr && sub_fields[0].nm != NoName && sub_fields[0].name == nullptr) {
            /* positional name=value / name-only sublist (unnamed fields):
             * user_lists -> ["a","b"], load_values -> [{name,value}] */
            spool_json_write_positional_array(writer, sub_list, sub_fields);
         } else if (sub_fields != nullptr) {
            /* sublist with real named fields -> array of keyed objects */
            if (!spool_json_write_list_array(answer_list, sub_list, writer, sub_fields)) {
               DRETURN(false);
            }
         } else {
            writer.StartArray();
            writer.EndArray();
         }
      } else {
         writer.Key(name);
         spool_json_write_scalar_value(writer, object, pos, type);
      }
   }

   DRETURN(true);
}

/* one object as a braced JSON object (no envelope); used for list/sublist elements */
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

/* a list of objects as a JSON array */
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
