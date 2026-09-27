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
 * @brief The `qconf` object descriptor table, and the hooks its rows point at
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/ocs_Role.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/ocs_spool_json.h"

#include <cstring>

#include "uti/sge_stdlib.h"

#include "ocs_QConfObject.h"
#include "msg_qconf.h"

#include "msg_common.h"

/* qconf_build_fields_fn providers (KTD8): objects with a shared static field table
 * report owned=false; objects whose import field list is built on demand report
 * owned=true so the engine frees it. The provider mirrors exactly the field list the
 * matching -A<obj> import handler reads with. */
static spooling_field *qconf_fields_CAL (spool_flatfile_format, bool *o) { *o = false; return CAL_fields;  }
static spooling_field *qconf_fields_CK  (spool_flatfile_format, bool *o) { *o = false; return CK_fields;   }
static spooling_field *qconf_fields_CE  (spool_flatfile_format, bool *o) { *o = false; return CE_fields;   }
static spooling_field *qconf_fields_RL  (spool_flatfile_format, bool *o) { *o = false; return RL_fields;   }
static spooling_field *qconf_fields_PE  (spool_flatfile_format, bool *o) { *o = false; return PE_fields;   }
static spooling_field *qconf_fields_HGRP(spool_flatfile_format, bool *o) { *o = false; return HGRP_fields; }
static spooling_field *qconf_fields_CQ  (spool_flatfile_format, bool *o) { *o = false; return CQ_fields;   }
static spooling_field *qconf_fields_US  (spool_flatfile_format, bool *o) { *o = false; return US_fields;   }
static spooling_field *qconf_fields_RQS (spool_flatfile_format, bool *o) { *o = false; return RQS_fields;  }
static spooling_field *qconf_fields_SC  (spool_flatfile_format, bool *o) { *o = false; return SC_fields;   }
/* import list omits runtime load_values/processors (to_stdout=false) so it round-trips (KTD2) */
static spooling_field *qconf_fields_EH  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_EH_field_list(false, false, false); }
/* -se shows the host as it is now, so the runtime values are included (CS-2796) */
static spooling_field *qconf_show_fields_EH(spool_flatfile_format, bool *o) { *o = true;  return sge_build_EH_field_list(false, true, false); }

/* defined below, next to the json helpers it uses */
static void qconf_show_prepare_EH(lListElem *ep, const lList *master_centry_list);
static spooling_field *qconf_fields_PR  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_PR_field_list(false); }
static spooling_field *qconf_fields_UU  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_UU_field_list(false); }
/* the cluster configuration has no static field table, only a builder (CS-2808) */
static spooling_field *qconf_fields_CONF(spool_flatfile_format, bool *o) { *o = true;  return sge_build_CONF_field_list(false); }
static spooling_field *qconf_fields_STN (spool_flatfile_format fmt, bool *o) {
   *o = true;
   return (fmt == SP_FORM_JSON) ? sge_build_STN_json_field_list() : sge_build_STN_field_list(false, true);
}

/**
 * @brief Validate-hook adapter for parallel environments (CS-2301).
 *
 * pe_validate() takes two extra arguments (a startup flag and the master userset
 * list) that the generic validate hook does not; this wrapper fixes them to the
 * values the qconf client uses (0 / nullptr), matching the editor paths.
 *
 * @param ep   the parsed PE object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_pe_validate(const lListElem *ep, lList **alpp) {
   return pe_validate(const_cast<lListElem *>(ep), alpp, 0, nullptr);
}

/**
 * @brief Validate-hook adapter for roles (CS-2302).
 *
 * ocs::Role::validate() returns a bool and takes a startup flag the generic
 * validate hook does not; this wrapper maps it to the hook's status convention,
 * fixing startup to false as the editor paths do.
 *
 * @param ep   the parsed role object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, STATUS_ESEMANTIC on failure
 */
static int
qconf_role_validate(const lListElem *ep, lList **alpp) {
   return ocs::Role::validate(ep, alpp, false) ? STATUS_OK : STATUS_ESEMANTIC;
}

/**
 * @brief Prepare-hook adapter for exec hosts (CS-2304): resolve the host name.
 *
 * Exec host objects must carry a resolved (canonical) host name before the name
 * is used for the upsert existence-check and the GDI send; this transform
 * resolves EH_name in place, matching what the interactive -ae/-Ae paths do.
 *
 * @param ep   the parsed exec host object (modified in place)
 * @param alpp answer list to receive a message on resolution failure
 * @return STATUS_OK on success, STATUS_ESEMANTIC if the host cannot be resolved
 */
static int
qconf_eh_resolve(lListElem *ep, lList **alpp) {
   if (sge_resolve_host(ep, EH_name) != CL_RETVAL_OK) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
      return STATUS_ESEMANTIC;
   }
   return STATUS_OK;
}

/**
 * @brief Validate-hook adapter for cluster queues (CS-2305).
 *
 * cqueue attribute verification (cqueue_verify_attributes) is what the cqueue
 * GDI wrapper runs before ADD/MOD; this maps it to the hook's status convention.
 *
 * @param ep   the parsed cluster queue object to verify
 * @param alpp answer list to receive verification messages
 * @return STATUS_OK on success, STATUS_ESEMANTIC on failure
 */
static int
qconf_cq_verify(const lListElem *ep, lList **alpp) {
   lListElem *e = const_cast<lListElem *>(ep);
   return cqueue_verify_attributes(e, alpp, e, false, nullptr, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr)
      ? STATUS_OK : STATUS_ESEMANTIC;
}

/**
 * @brief Validate-hook adapter for usersets (CS-2310).
 *
 * userset_validate_entries() is what the interactive -Au/-Mu paths run before
 * ADD/MOD; it takes a non-const element, so this wrapper const-casts to fit the
 * generic validate hook signature.
 *
 * @param ep   the parsed userset object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_us_validate(const lListElem *ep, lList **alpp) {
   return userset_validate_entries(const_cast<lListElem *>(ep), alpp);
}

/* ===== CS-2795: the object descriptor table ================================
 * The generic engines above (qconf_apply_path, qconf_delete_path,
 * qconf_export_path, qconf_export_singleton) already drive every file-based
 * option from one implementation each. What every call site still repeated was
 * the tuple identifying the object: target, descriptor, name field, field list,
 * spooling instruction and the object's hooks. This table holds it once.
 *
 * Adding a new configuration object type is now one row plus its field
 * provider, instead of a tuple copied into each of the add, modify, delete and
 * export branches. */

/** @brief One row per object type, indexed by QConfObj.
 *
 * Kept in the order of the enum so that qconf_desc() is a plain index. */
static const qconf_object_desc qconf_object_table[] = {
   /* CAL  */ { ocs::gdi::Target::CAL_LIST,  CAL_Type,  CAL_name,  qconf_fields_CAL,  &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "calendar", MSG_CALENDAR_XISNOTACALENDAR_S, nullptr, nullptr } },
   /* CK   */ { ocs::gdi::Target::CK_LIST,   CK_Type,   CK_name,   qconf_fields_CK,   &qconf_sfi,                 nullptr,      ckpt_validate,       nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "ckpt interface definition", MSG_CKPT_XISNOTCHKPINTERFACEDEF_S, nullptr, nullptr } },
   /* CE   */ { ocs::gdi::Target::CE_LIST,   CE_Type,   CE_name,   qconf_fields_CE,   &qconf_ce_sfi,              nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "complex entry", MSG_CENTRY_DOESNOTEXIST_S, nullptr, nullptr } },
   /* EH   */ { ocs::gdi::Target::EH_LIST,   EH_Type,   EH_name,   qconf_fields_EH,   &qconf_sfi,                 "exechost",   nullptr,             qconf_eh_resolve,  ocs::gdi::SubCommand::NONE,    false, true,  false, SGE_TEMPLATE_NAME,  SGE_GLOBAL_NAME,
              { qconf_show_fields_EH, "execution host", MSG_EXEC_XISNOTANEXECUTIONHOST_S, nullptr, qconf_show_prepare_EH } },
   /* HGRP */ { ocs::gdi::Target::HGRP_LIST, HGRP_Type, HGRP_name, qconf_fields_HGRP, &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "host group list", nullptr, nullptr, nullptr } },
   /* PE   */ { ocs::gdi::Target::PE_LIST,   PE_Type,   PE_name,   qconf_fields_PE,   &qconf_sfi,                 nullptr,      qconf_pe_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "parallel environment", MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S, nullptr, nullptr } },
   /* PR   */ { ocs::gdi::Target::PR_LIST,   PR_Type,   PR_name,   qconf_fields_PR,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "project list", MSG_PROJECT_XISNOKNWOWNPROJECT_S, nullptr, nullptr } },
   /* CQ   */ { ocs::gdi::Target::CQ_LIST,   CQ_Type,   CQ_name,   qconf_fields_CQ,   &qconf_sfi,                 nullptr,      qconf_cq_verify,     nullptr,           ocs::gdi::SubCommand::SET_ALL, false, false, false, nullptr,            nullptr,
              { nullptr, "cqueue list", nullptr, nullptr, nullptr } },
   /* RL   */ { ocs::gdi::Target::RL_LIST,   RL_Type,   RL_name,   qconf_fields_RL,   &qconf_sfi,                 nullptr,      qconf_role_validate, nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "role", MSG_ROLE_DOESNOTEXIST_S, nullptr, nullptr } },
   /* RQS  */ { ocs::gdi::Target::RQS_LIST,  RQS_Type,  RQS_name,  qconf_fields_RQS,  &qconf_rqs_sfi,             nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    true,  true,  false, nullptr,            nullptr,
              { nullptr, "resource quota set list", nullptr, nullptr, nullptr } },
   /* US   */ { ocs::gdi::Target::US_LIST,   US_Type,   US_name,   qconf_fields_US,   &qconf_param_sfi,           nullptr,      qconf_us_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "userset list", nullptr, nullptr, nullptr } },
   /* UU   */ { ocs::gdi::Target::UU_LIST,   UU_Type,   UU_name,   qconf_fields_UU,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "user list", MSG_USER_XISNOKNOWNUSER_S, nullptr, nullptr } },
   /* STN  */ { ocs::gdi::Target::STN_LIST,  STN_Type,  NoName,    qconf_fields_STN,  &qconf_name_value_list_sfi, "sharetree",  nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  true,  nullptr,            nullptr,
              { nullptr, nullptr, nullptr, MSG_OBJ_NOSTREEELEM, nullptr } },
   /* SC: the hand written -ssconf had no empty check at all - it handed a null
    * element to the writer and reported the CULL complaint plus this line. The
    * path cannot occur on a running cluster; inside qmaster a null element must
    * not reach the writer, so the check is explicit here and reports the closer
    * of the two messages (CS-2808). */
   /* SC   */ { ocs::gdi::Target::SC_LIST,   SC_Type,   NoName,    qconf_fields_SC,   &qconf_comma_sfi,           "sched_conf", nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, nullptr, nullptr, MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION, nullptr } },
   /* CONF: read only (CS-2808). -sconf/-mconf keep their own code and never ask
    * this table, so every write field is left empty on purpose. */
   /* CONF */ { ocs::gdi::Target::CONF_LIST, CONF_Type, CONF_name, qconf_fields_CONF, &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { nullptr, "config", nullptr, nullptr, nullptr } },
   /* EV: read only and name-list only (CS-2808). There is no field list because
    * EV_Type is not a registered object type and no -s<obj>/export path reaches
    * this row - show_name_list() renders the key field and nothing else. */
   /* EV   */ { ocs::gdi::Target::EV_LIST,   EV_Type,   EV_host,   nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { nullptr, "scheduling host", nullptr, nullptr, nullptr } },
};

/* CS-2808: the table is indexed by QConfObj and carries no name to grep for, so
 * a value appended to the enum without a row here would read past the end in
 * silence. The bound is spelled out rather than taken from a sentinel, so that
 * appending a value forces a look at this line. */
static_assert(sizeof(qconf_object_table) / sizeof(qconf_object_table[0]) ==
              static_cast<size_t>(QConfObj::EV) + 1,
              "qconf_object_table needs exactly one row per QConfObj value");

/** @brief A private copy of a field list, see the header (CS-2817)
 *
 * @param fields the list to copy
 * @return the copy, or nullptr when @p fields is nullptr
 */
spooling_field *
qconf_copy_fields(const spooling_field *fields) {
   if (fields == nullptr) {
      return nullptr;
   }

   size_t n = 0;
   while (fields[n].nm != NoName) {
      n++;
   }

   // sge_malloc() hands back char*, so the pointer type genuinely changes here
   auto *copy = reinterpret_cast<spooling_field *>(sge_malloc(sizeof(spooling_field) * (n + 1)));
   if (copy != nullptr) {
      memcpy(copy, fields, sizeof(spooling_field) * (n + 1));
   }
   return copy;
}

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in qconf_object_table
 */
const qconf_object_desc &
qconf_desc(QConfObj o) {
   return qconf_object_table[static_cast<size_t>(o)];
}

/**
 * @brief Fill complex-value sublists with their CULL type/numeric value (CS-2313a).
 *
 * The raw complex_values / threshold entries stored on an exec host or queue carry
 * only a name + string value (CE_valtype is NONE). To emit native JSON numbers
 * (slots -> 32, h_vmem -> bytes, h_rt -> seconds) the writer needs CE_valtype and
 * CE_doubleval, which centry_list_fill_request() derives from the centry
 * definitions - the same fill qstat performs before display.
 *
 * Runs only for -fmt json (ASCII output uses CE_stringval and is unaffected), is
 * best-effort (unresolved entries keep their string value), and mutates only the
 * throwaway in-memory object that is about to be serialized.
 *
 * @param obj       the object whose complex sublists should be filled (modified)
 * @param ce_fields the CE_Type sublist field ids to fill (e.g. EH_consumable_config_list)
 * @param n_fields  number of entries in @p ce_fields
 */
void qconf_json_fill_complex(lListElem *obj, const int *ce_fields, int n_fields,
                             const lList *master_centry_list) {
   if (obj == nullptr || master_centry_list == nullptr) {
      return;
   }
   for (int i = 0; i < n_fields; i++) {
      lList *sub = lGetListRW(obj, ce_fields[i]);
      if (sub != nullptr && lGetNumberOfElem(sub) > 0) {
         lList *fill_alp = nullptr;
         centry_list_fill_request(sub, &fill_alp, master_centry_list, true, true, true);
         lFreeList(&fill_alp);
      }
   }
}

/**
 * @brief Render an exec host's load_values typed by their complex definition (CS-2313a).
 *
 * For -fmt json, values such as mem_total are rendered as bytes, num_proc as an
 * integer, np_load_avg as a double, ... instead of as raw strings. HL_Type load values
 * carry no type, so we build a CE_Type list (name + string value), resolve each entry's
 * type from the master complex list (tolerant of load names that are not defined
 * complexes), parse numeric values into CE_doubleval, and swap the typed CE list into
 * the load-list field. The generic JSON writer then renders it via its typed
 * complex-value path; unmatched or string-typed loads keep their string value. Runs
 * only for -fmt json and mutates only the throwaway object about to be shown.
 *
 * @param host          the (throwaway) exec host element to be shown
 * @param load_list_nm  CULL field id of the load-value list (e.g. EH_load_list)
 */
static void
qconf_json_type_load_values(lListElem *host, int load_list_nm, const lList *master_centry_list) {
   if (host == nullptr || master_centry_list == nullptr) {
      return;
   }
   const lList *hl = lGetList(host, load_list_nm);
   if (hl == nullptr || lGetNumberOfElem(hl) == 0) {
      return;
   }

   lList *ce_list = lCreateList("load_values", CE_Type);
   const lListElem *hlep;
   for_each_ep(hlep, hl) {
      lListElem *ce = lCreateElem(CE_Type);
      lSetString(ce, CE_name, lGetString(hlep, HL_name));
      lSetString(ce, CE_stringval, lGetString(hlep, HL_value));
      lAppendElem(ce_list, ce);
   }

   /* fill CE_valtype from the complex definitions (skips unknown load names) */
   centry_list_fill_config(ce_list, master_centry_list);

   /* parse numeric values into CE_doubleval so the writer emits native numbers */
   for (lListElem *ce = lFirstRW(ce_list); ce != nullptr; ce = lNextRW(ce)) {
      const auto type = static_cast<ocs::CEntry::Type>(lGetUlong(ce, CE_valtype));
      const char *s = lGetString(ce, CE_stringval);
      double dval = 0.0;
      switch (type) {
         case ocs::CEntry::Type::INT:
         case ocs::CEntry::Type::TIME:
         case ocs::CEntry::Type::MEM:
         case ocs::CEntry::Type::DOUBLE:
         case ocs::CEntry::Type::BOOL:
            if (s != nullptr && parse_ulong_val(&dval, nullptr, type, s, nullptr, 0)) {
               lSetDouble(ce, CE_doubleval, dval);
            }
            break;
         default:
            break;   /* string-like / unmatched -> writer emits CE_stringval */
      }
   }

   lSetList(host, load_list_nm, ce_list);
}

/** @brief Type the numeric fields of an exec host before it is rendered.
 *
 * CS-2313a: complex_values and the load values are strings in CULL, so -fmt
 * json would quote them without this.
 *
 * @param ep the exec host about to be written
 */
static void
qconf_show_prepare_EH(lListElem *ep, const lList *master_centry_list) {
   static const int eh_ce_fields[] = { EH_consumable_config_list };
   qconf_json_fill_complex(ep, eh_ce_fields, 1, master_centry_list);
   qconf_json_type_load_values(ep, EH_load_list, master_centry_list);
}
