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
#include "sgeobj/msg_sgeobjlib.h"
/* MSG_NORQSFOUND is in the clients' shared catalogue; libs/sgeobj reaches for
 * that header from the library side too, see ocs_QConfModelBase.cc */
#include "msg_clients_common.h"

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
static spooling_field *qconf_fields_CAT (spool_flatfile_format, bool *o) { *o = false; return CAT_fields;  }
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

/* ===== the templates of the types that are created from a name (CS-2823) ====
 *
 * One wrapper per type, because the constructors disagree about their shape: the
 * calendar, the checkpoint environment and the parallel environment take the
 * name and set it themselves, the complex entry also takes an answer list and
 * may refuse a name, and the role, the user and the project build an unnamed
 * template that the caller names. All of them live in libs/sgeobj, which the
 * client and qmaster both link, so the table can carry them - the template used
 * to be built in the client only because this was assumed to be impossible.
 *
 * A name the constructor rejects yields nullptr with the reason in the answer
 * list, which is the right reaction: a half built template must not reach an
 * editor.
 */

/** @brief The template of a calendar
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_CAL([[maybe_unused]] lList **answer_list, const char *name) {
   return sge_generic_cal(const_cast<char *>(name));
}

/** @brief The template of a checkpointing environment
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_CK([[maybe_unused]] lList **answer_list, const char *name) {
   return sge_generic_ckpt(const_cast<char *>(name));
}

/** @brief The template of a parallel environment
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_PE([[maybe_unused]] lList **answer_list, const char *name) {
   return pe_create_template(const_cast<char *>(name));
}

/** @brief The template of a complex entry
 * @param answer_list receives the reason when the name is not a valid one
 * @param name the name the template carries
 * @return the template, or nullptr when the name was refused
 */
static lListElem *
qconf_template_CE(lList **answer_list, const char *name) {
   return centry_create(answer_list, name);
}

/** @brief The template of a role
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_RL([[maybe_unused]] lList **answer_list, const char *name) {
   lListElem *ep = ocs::Role::create_template();
   if (ep != nullptr) {
      lSetString(ep, RL_name, name);
   }
   return ep;
}

/** @brief The template of a user
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_UU([[maybe_unused]] lList **answer_list, const char *name) {
   lListElem *ep = getUserTemplate();
   if (ep != nullptr) {
      lSetString(ep, UU_name, name);
   }
   return ep;
}

/** @brief The template of a project
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_PR([[maybe_unused]] lList **answer_list, const char *name) {
   lListElem *ep = getPrjTemplate();
   if (ep != nullptr) {
      lSetString(ep, PR_name, name);
   }
   return ep;
}

/** @brief The template of a resource quota set
 *
 * One rule with a `slots=0` limit and the set enabled, which is what
 * "qconf -arqs <name>" offered before the switch came here: an element per
 * requested name, each through rqs_set_defaults().
 *
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_RQS([[maybe_unused]] lList **answer_list, const char *name) {
   lListElem *ep = lCreateElem(RQS_Type);
   if (ep != nullptr) {
      lSetString(ep, RQS_name, name);
      ep = rqs_set_defaults(ep);
   }
   return ep;
}

/** @brief The shallow check a rule set had to pass in the client (CS-2823)
 *
 * `rqs_add_del_mod_via_gdi()` ran `rqs_list_verify_attributes(..., in_master =
 * false, ...)` over the whole list before sending it, and sent nothing at all
 * when one rule set failed. With `in_master` false that check is exactly two
 * things: a rule set needs a name, and it needs at least one rule. The deep
 * checks - the filters, the scopes, the limits - belong to qmaster's own
 * modifier and run there as they always did.
 *
 * Running it here keeps that batch behaviour: a validator failure happens in
 * the reading phase, and for a type whose text holds a list the reading phase
 * is all-or-nothing.
 *
 * @param ep the rule set
 * @param alpp receives the reason
 * @return STATUS_OK, or STATUS_ESEMANTIC with the reason in @p alpp
 */
static int
qconf_rqs_validate(const lListElem *ep, lList **alpp) {
   return rqs_verify_attributes(const_cast<lListElem *>(ep), alpp, false, nullptr)
      ? STATUS_OK : STATUS_ESEMANTIC;
}

/** @brief The template of a host group
 *
 * The name check of hgroup_create() stays **off**, which is what the client's
 * own template always did ("This boolean is needed to create a hgrp templete").
 * The reason is the template's own name: "-ahgrp" without an argument asks for
 * "@template", and verify_obj_name() refuses TEMPLATE as a reserved keyword -
 * so validating here would refuse the nameless add before the editor it is
 * meant to open. Measured on 2026-10-15: with the check on, "qconf -ahgrp"
 * answers "Keyword (\"TEMPLATE\") not allowed as objectname" and the editor
 * never comes up.
 *
 * Nothing is lost by it. A name that cannot become a host group is refused by
 * qmaster when the edited text is written (hgroup_check_name() in
 * sge_hgroup_qmaster.cc), the message is the same one, and it arrives after the
 * editor instead of before it - which is how every other type behaves.
 *
 * @param answer_list unused, the constructor reports nothing with the check off
 * @param name the name the template carries
 * @return the template
 */
static lListElem *
qconf_template_HGRP(lList **answer_list, const char *name) {
   return hgroup_create(answer_list, name, nullptr, false);
}

/** @brief The template of a cluster queue
 *
 * Two steps, as the client's own template always was: the object, then the
 * attribute defaults every queue starts from.
 *
 * @param answer_list receives the reason when the template could not be built
 * @param name the name the template carries
 * @return the template, or nullptr
 */
static lListElem *
qconf_template_CQ(lList **answer_list, const char *name) {
   lListElem *ep = cqueue_create(answer_list, name);
   if (ep != nullptr && !cqueue_set_template_attributes(ep, answer_list)) {
      lFreeElem(&ep);
   }
   return ep;
}

/** @brief The template of a userset
 * @param answer_list receives the reason when the template could not be built
 * @param name the name the template carries
 * @return the template, or nullptr
 */
static lListElem *
qconf_template_US(lList **answer_list, const char *name) {
   return userset_create(answer_list, name);
}

/** @brief The template of a share tree (CS-2823)
 *
 * What "qconf -mstree" offered on a cluster with no share tree: the editor used
 * to open on getSNTemplate() whenever its GET came back empty, which is how a
 * first tree was written at all. The read answers the absent singleton with it
 * now, so the upsert of CS-2312 survives the move into the procedure.
 *
 * @param answer_list unused, the constructor reports nothing
 * @param name unused - a share tree is one object and no name selects it
 * @return the template
 */
static lListElem *
qconf_template_STN([[maybe_unused]] lList **answer_list, [[maybe_unused]] const char *name) {
   return getSNTemplate();
}

/** @brief The template of a host configuration (CS-2823)
 *
 * An empty configuration carrying the host name, which is what "qconf -mconf
 * <host>" has always offered for a host that has none: the switch is an upsert,
 * and the editor it opened was on an object the client had just created when the
 * GET came back empty. The read answers a missing name with this now, so the
 * upsert survives the move into the procedure.
 *
 * Nothing else is filled in. The configuration's entries are what the
 * administrator writes, and a global default list would be wrong for a host
 * configuration, whose entries override the global one.
 *
 * @param answer_list unused, creating the element reports nothing
 * @param name the host the template is for
 * @return the template
 */
static lListElem *
qconf_template_CONF([[maybe_unused]] lList **answer_list, const char *name) {
   lListElem *ep = lCreateElem(CONF_Type);
   if (ep != nullptr && name != nullptr) {
      lSetHost(ep, CONF_name, name);
   }
   return ep;
}

/** @brief The checks a rule set text needs the whole list for (CS-2823)
 *
 * One check, and it was an `if` in qconf_write_from_procedure() until the
 * complex list brought three more: a text that names the same rule set twice is
 * ambiguous, and was refused before it was ever sent. The message says
 * "replacement request" because that is the request it used to guard - the
 * wording is kept, the situation is the same one.
 *
 * @param lp the rule sets one item's text held
 * @param alpp receives the reason
 * @return STATUS_OK, or STATUS_ERROR1 with the reason in @p alpp
 */
static int
qconf_rqs_verify_list(const lList *lp, lList **alpp) {
   for (const lListElem *a = lFirst(lp); a != nullptr; a = lNext(a)) {
      const char *name_a = lGetString(a, RQS_name);
      if (name_a == nullptr) {
         continue;         // a nameless rule set is qconf_rqs_validate()'s to refuse
      }
      for (const lListElem *b = lNext(a); b != nullptr; b = lNext(b)) {
         const char *name_b = lGetString(b, RQS_name);
         if (name_b != nullptr && strcmp(name_a, name_b) == 0) {
            answer_list_add_sprintf(alpp, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                                    MSG_RQS_REQUEST_DUPLICATE_NAME_S, name_a);
            return STATUS_ERROR1;
         }
      }
   }
   return STATUS_OK;
}

/** @brief The checks a complex list text needs the whole list for (CS-2823)
 *
 * What centry_list_add_del_mod_via_gdi() ran in the client before it sent
 * anything, minus everything qmaster already does: `centry_elem_validate()`
 * runs per element in sge_centry_qmaster.cc on every add and every modify, and
 * it covers the types, the operators, the consumables and - when the urgency is
 * there at all - that it parses. What is left is what one element cannot answer
 * or nobody asks:
 *
 *  - a **name or shortcut that collides** with another element's name or
 *    shortcut. Four comparisons, not one, because the two namespaces are one:
 *    a request may name a complex by either, so a shortcut equal to another
 *    attribute's name makes both unreachable.
 *  - a **missing** shortcut or urgency. The element validator takes an absent
 *    urgency as "nothing to check", and nothing refuses an absent shortcut, so
 *    without this the complex would be stored unusable.
 *
 * The client ran the per-element part of this *inside* the pairwise loop, which
 * meant a text holding exactly one entry was never checked at all. Here each
 * element is checked once, which is what the loop was written to do.
 *
 * @param lp the complex entries one item's text held
 * @param alpp receives the reason
 * @return STATUS_OK, or STATUS_EUNKNOWN with the reason in @p alpp
 */
static int
qconf_ce_verify_list(const lList *lp, lList **alpp) {
   for (const lListElem *a = lFirst(lp); a != nullptr; a = lNext(a)) {
      const char *name_a = lGetString(a, CE_name);
      const char *shortcut_a = lGetString(a, CE_shortcut);

      if (name_a == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                 MSG_CENTRY_NULL_NAME);
         return STATUS_EUNKNOWN;
      }
      if (shortcut_a == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_CENTRY_NULL_SHORTCUT_S, name_a);
         return STATUS_EUNKNOWN;
      }
      if (lGetString(a, CE_urgency_weight) == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                 MSG_CENTRY_NULL_URGENCY);
         return STATUS_EUNKNOWN;
      }

      for (const lListElem *b = lNext(a); b != nullptr; b = lNext(b)) {
         const char *name_b = lGetString(b, CE_name);
         const char *shortcut_b = lGetString(b, CE_shortcut);
         if (name_b == nullptr || shortcut_b == nullptr) {
            continue;      // reported when the loop above reaches that element
         }
         if (strcmp(name_a, name_b) == 0 || strcmp(name_a, shortcut_b) == 0 ||
             strcmp(shortcut_a, name_b) == 0 || strcmp(shortcut_a, shortcut_b) == 0) {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_COMPLEXXALREADYEXISTS_SS, name_a, shortcut_a);
            return STATUS_EUNKNOWN;
         }
      }
   }
   return STATUS_OK;
}

/** @brief A built-in complex survives a replacing write (CS-2523)
 *
 * It cannot be deleted - `sge_del_centry()` refuses every name `get_rsrc()`
 * knows - so a replace that asked would abort and leave the configuration half
 * applied. That is not hypothetical: a backup written before a built-in existed
 * has no row for it, and "devices" broke every stored fixture from 9.0.0 to
 * 9.1.0, on the supported upgrade path.
 *
 * So the replace stops *asking*, and says which ones it kept. The server's
 * refusal stays where it is: this is the courtesy in front of it, not a
 * replacement for it, and "qconf -dce <built-in>" still fails hard because
 * there the intent to delete is unambiguous.
 *
 * @param ep the complex entry the text did not hold
 * @return true when it is a built-in and must be kept
 */
static bool
qconf_ce_retain_builtin(const lListElem *ep) {
   const char *name = lGetString(ep, CE_name);
   if (name == nullptr) {
      return false;
   }
   // both spellings: get_rsrc() answers per queue and per host resource
   return get_rsrc(name, true, nullptr, nullptr, nullptr, nullptr) == 0 ||
          get_rsrc(name, false, nullptr, nullptr, nullptr, nullptr) == 0;
}

/** @brief One row per object type, indexed by QConfObj.
 *
 * Kept in the order of the enum so that qconf_desc() is a plain index. */
/** @brief Whether a share tree's nodes carry ids
 *
 * The root is id 0 and the children count up from it, so a tree that was read
 * with ids has a non-zero one somewhere below the root. A tree of one node is
 * the single case where a numbered and an unnumbered tree look alike, and there
 * the distinction does not matter - see qconf_stn_prepare().
 *
 * @param ep the root of the tree
 * @return true when any node below the root carries a non-zero id
 */
static bool
qconf_stn_has_ids(const lListElem *ep) {
   const lListElem *child;
   for_each_ep(child, lGetList(ep, STN_children)) {
      if (lGetUlong(child, STN_id) != 0 || qconf_stn_has_ids(child)) {
         return true;
      }
   }
   return false;
}

/** @brief Number a share tree's nodes when its text carried no ids (CS-2823)
 *
 * The nested JSON form has no ids: a child is an object inside its parent, so
 * what the flat form expresses by an id reference is the nesting itself. The
 * spooled tree is written with ids, so a tree parsed from JSON has to be
 * numbered before it is stored - the client did this right after its own parse,
 * and the parse is here now.
 *
 * Idempotent, which is why it needs no format argument: a tree that arrived with
 * ids keeps them.
 *
 * @param ep the root of the parsed tree
 * @param answer_list receives the reason when the numbering fails
 * @return STATUS_OK when the tree is numbered
 */
static int
qconf_stn_prepare(lListElem *ep, lList **answer_list) {
   if (qconf_stn_has_ids(ep)) {
      return STATUS_OK;
   }
   return id_sharetree(answer_list, ep, 0, nullptr) ? STATUS_OK : STATUS_EUNKNOWN;
}

static const qconf_object_desc qconf_object_table[] = {
   /* CAL  */ { ocs::gdi::Target::CAL_LIST,  CAL_Type,  CAL_name,  qconf_fields_CAL,  &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "calendar", .not_found_msg = MSG_CALENDAR_XISNOTACALENDAR_S,
                .create_template = qconf_template_CAL } },
   /* CK   */ { ocs::gdi::Target::CK_LIST,   CK_Type,   CK_name,   qconf_fields_CK,   &qconf_sfi,                 nullptr,      ckpt_validate,       nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "ckpt interface definition", .not_found_msg = MSG_CKPT_XISNOTCHKPINTERFACEDEF_S,
                .create_template = qconf_template_CK } },
   /* CE   */ { ocs::gdi::Target::CE_LIST,   CE_Type,   CE_name,   qconf_fields_CE,   &qconf_ce_sfi,              nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "complex entry", .not_found_msg = MSG_CENTRY_DOESNOTEXIST_S,
                .create_template = qconf_template_CE, .list_sfi = &qconf_ce_list_sfi },
              /* as_list stays false: the type's single-object switches are the
               * ones a *read* renders, and "-mc"/"-Mc" say MODE_LIST on the
               * request instead. The write part is what makes the table form
               * writable at all - the whole-list checks, and the built-ins a
               * replace keeps (CS-2823) */
              { .verify_list = qconf_ce_verify_list,
                .retain_on_replace = qconf_ce_retain_builtin,
                .retain_msg = MSG_CENTRY_KEPT_BUILTIN_S } },
   /* EH   */ { ocs::gdi::Target::EH_LIST,   EH_Type,   EH_name,   qconf_fields_EH,   &qconf_sfi,                 "exechost",   nullptr,             qconf_eh_resolve,  ocs::gdi::SubCommand::NONE,    false, true,  false, SGE_TEMPLATE_NAME,  SGE_GLOBAL_NAME,
              { .fields = qconf_show_fields_EH, .list_name = "execution host", .not_found_msg = MSG_EXEC_XISNOTANEXECUTIONHOST_S, .prepare = qconf_show_prepare_EH } },
   /* HGRP */ { ocs::gdi::Target::HGRP_LIST, HGRP_Type, HGRP_name, qconf_fields_HGRP, &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "host group list", .create_template = qconf_template_HGRP } },
   /* PE   */ { ocs::gdi::Target::PE_LIST,   PE_Type,   PE_name,   qconf_fields_PE,   &qconf_sfi,                 nullptr,      qconf_pe_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "parallel environment", .not_found_msg = MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S,
                .create_template = qconf_template_PE } },
   /* PR   */ { ocs::gdi::Target::PR_LIST,   PR_Type,   PR_name,   qconf_fields_PR,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "project list", .not_found_msg = MSG_PROJECT_XISNOKNWOWNPROJECT_S,
                .create_template = qconf_template_PR } },
   /* CQ   */ { ocs::gdi::Target::CQ_LIST,   CQ_Type,   CQ_name,   qconf_fields_CQ,   &qconf_sfi,                 nullptr,      qconf_cq_verify,     nullptr,           ocs::gdi::SubCommand::SET_ALL, false, false, false, nullptr,            nullptr,
              { .list_name = "cqueue list", .create_template = qconf_template_CQ } },
   /* RL   */ { ocs::gdi::Target::RL_LIST,   RL_Type,   RL_name,   qconf_fields_RL,   &qconf_sfi,                 nullptr,      qconf_role_validate, nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "role", .not_found_msg = MSG_ROLE_DOESNOTEXIST_S,
                .create_template = qconf_template_RL } },
   /* RQS: SubCommand::SET_ALL, not NONE - every live resource quota path sends
    * SET_ALL, and the row was only ever unread because the type had a write
    * path of its own. With NONE a rule set's sublists would be merged instead of
    * replaced, so a limit the administrator deleted in the editor would survive
    * (CS-2823). */
   /* RQS  */ { ocs::gdi::Target::RQS_LIST,  RQS_Type,  RQS_name,  qconf_fields_RQS,  &qconf_rqs_sfi,             nullptr,      qconf_rqs_validate,  nullptr,           ocs::gdi::SubCommand::SET_ALL, true,  true,  false, nullptr,            nullptr,
              /* -srqs writes the whole list in one go: qconf_rqs_sfi wraps every
               * record in braces, which a loop over single objects would not do */
              /* -srqs reports an empty result as a whole and never names the one
               * that was missing, so there is no noun; empty_msg carries the
               * message it always printed (CS-2819) */
              { .list_name = "resource quota set list", .create_template = qconf_template_RQS,
                .empty_msg = MSG_NORQSFOUND, .write_as_list = true },
              /* no retain hook and no replace: a modify writes what the text
               * holds and removes nothing (CS-2823) */
              { .verify_list = qconf_rqs_verify_list } },
   /* US   */ { ocs::gdi::Target::US_LIST,   US_Type,   US_name,   qconf_fields_US,   &qconf_param_sfi,           nullptr,      qconf_us_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              /* -su names the userset it could not find, and calls it an access
               * list, as it always did (CS-2819) */
              { .list_name = "userset list", .create_template = qconf_template_US,
                .not_found_noun = "access list" } },
   /* UU   */ { ocs::gdi::Target::UU_LIST,   UU_Type,   UU_name,   qconf_fields_UU,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .list_name = "user list", .not_found_msg = MSG_USER_XISNOKNOWNUSER_S,
                .create_template = qconf_template_UU } },
   /* STN  */ { ocs::gdi::Target::STN_LIST,  STN_Type,  NoName,    qconf_fields_STN,  &qconf_name_value_list_sfi, "sharetree",  nullptr,             qconf_stn_prepare, ocs::gdi::SubCommand::NONE,    false, true,  true,  nullptr,            nullptr,
              { .create_template = qconf_template_STN, .empty_msg = MSG_OBJ_NOSTREEELEM } },
   /* SC: the hand written -ssconf had no empty check at all - it handed a null
    * element to the writer and reported the CULL complaint plus this line. The
    * path cannot occur on a running cluster; inside qmaster a null element must
    * not reach the writer, so the check is explicit here and reports the closer
    * of the two messages (CS-2808). */
   /* SC   */ { ocs::gdi::Target::SC_LIST,   SC_Type,   NoName,    qconf_fields_SC,   &qconf_comma_sfi,           "sched_conf", nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { .empty_msg = MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION } },
   /* CONF: the one type whose text does not carry its own name - the host is the
    * argument of "-mconf" or the basename of the "-Mconf" file, and the read
    * renders it as a comment (name_header). So the write is given the name as a
    * request parameter, see QConfWriteParameter::set_name(). Its modify is also
    * an upsert in qmaster, which is why "-aconf" and "-mconf" are one thing
    * (CS-2823). */
   /* CONF */ { ocs::gdi::Target::CONF_LIST, CONF_Type, CONF_name, qconf_fields_CONF, &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "config", .not_found_msg = MSG_ANSWER_CONFIGXNOTDEFINED_S,
                .create_template = qconf_template_CONF, .name_header = true } },
   /* EV: read only and name-list only (CS-2808). There is no field list because
    * EV_Type is not a registered object type and no -s<obj>/export path reaches
    * this row - show_name_list() renders the key field and nothing else. */
   /* EV   */ { ocs::gdi::Target::EV_LIST,   EV_Type,   EV_host,   nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "scheduling host" } },
   /* CAT: read only, and the only type whose key is a number rather than a name.
    * The list has a JSON schema id of its own so that the enveloped array of
    * -scatl does not collide with the single record of -scat (CS-2819). */
   /* CAT  */ { ocs::gdi::Target::CAT_LIST,  CT_Type,   CT_id,     qconf_fields_CAT,  &qconf_cat_sfi,            "category",   nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "category", .not_found_msg = MSG_CAT_DOESNOTEXIST_U,
                .list_sfi = &qconf_cat_list_sfi, .json_list_id = "category-list" } },
   /* AH, SH, MGR, OP: the four reserved member lists (CS-2820). Each row names a
    * reserved object of an existing type and the sublist that -sh/-ss/-sm/-so
    * report; the write fields are empty because no write path reaches these rows,
    * and there is no field list because the names are rendered by
    * show_name_list() and nothing else - the same reason the EV row has none.
    *
    * The entries are HR_Type and UE_Type, neither of them a registered object
    * type, so the JSON envelope would derive as "unknown" and has to be named.
    * Whether it is also named for an EMPTY list is not a free choice: -sh and -ss
    * produced the generic "names" key there and -sm and -so the full envelope,
    * and scripts see the difference. */
   /* AH   */ { ocs::gdi::Target::HGRP_LIST, HGRP_Type, HGRP_name, nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "administrative host", .json_name_id = "adminhost", .json_name_id_when_empty = false,
                .member = { ADMIN_HOSTGROUP, HGRP_host_list, HR_name, HR_Type, true } } },
   /* SH   */ { ocs::gdi::Target::HGRP_LIST, HGRP_Type, HGRP_name, nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "submit host", .json_name_id = "submithost", .json_name_id_when_empty = false,
                .member = { SUBMIT_HOSTGROUP, HGRP_host_list, HR_name, HR_Type, true } } },
   /* MGR  */ { ocs::gdi::Target::US_LIST,   US_Type,   US_name,   nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "manager", .json_name_id = "manager", .json_name_id_when_empty = true,
                .member = { MANAGER_USERSET, US_entries, UE_name, UE_Type, false } } },
   /* OP   */ { ocs::gdi::Target::US_LIST,   US_Type,   US_name,   nullptr,           &qconf_sfi,                nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,            nullptr,
              { .list_name = "operator", .json_name_id = "operator", .json_name_id_when_empty = true,
                .member = { OPERATOR_USERSET, US_entries, UE_name, UE_Type, false } } },
};

/* CS-2808: the table is indexed by QConfObj and carries no name to grep for, so
 * a value appended to the enum without a row here would read past the end in
 * silence. The bound is spelled out rather than taken from a sentinel, so that
 * appending a value forces a look at this line. */
static_assert(sizeof(qconf_object_table) / sizeof(qconf_object_table[0]) ==
              static_cast<size_t>(QConfObj::OP) + 1,
              "qconf_object_table needs exactly one row per QConfObj value");

/** @brief Which master list each object type lives in
 *
 * The qmaster-internal `gdi_object_t` table knows this for most types, but it
 * sits in `daemons/qmaster/sge_c_gdi.cc`, a library cannot reach it, and for the
 * three types whose modify is a special case there rather than a hook on the row
 * it carries SGE_TYPE_NONE (CONF, SC) - so it knows the list for the types that
 * never needed it. Stated here instead, where the rest of a type's traits are,
 * and asked by the read and the write path alike (CS-2823). Kept in the order of
 * #QConfObj so that the lookup is a plain index.
 */
static constexpr sge_object_type qconf_master_list[] = {
   SGE_TYPE_CALENDAR,    /* CAL  */
   SGE_TYPE_CKPT,        /* CK   */
   SGE_TYPE_CENTRY,      /* CE   */
   SGE_TYPE_EXECHOST,    /* EH   */
   SGE_TYPE_HGROUP,      /* HGRP */
   SGE_TYPE_PE,          /* PE   */
   SGE_TYPE_PROJECT,     /* PR   */
   SGE_TYPE_CQUEUE,      /* CQ   */
   SGE_TYPE_RL,          /* RL   */
   SGE_TYPE_RQS,         /* RQS  */
   SGE_TYPE_USERSET,     /* US   */
   SGE_TYPE_USER,        /* UU   */
   SGE_TYPE_SHARETREE,   /* STN  */
   SGE_TYPE_SCHEDD_CONF, /* SC   */
   SGE_TYPE_CONFIG,      /* CONF */
   SGE_TYPE_NONE,        /* EV - event clients live in the event master */
   SGE_TYPE_CATEGORY,    /* CAT  */
   SGE_TYPE_HGROUP,      /* AH - @admin_hosts lives in the host group list */
   SGE_TYPE_HGROUP,      /* SH - @submit_hosts likewise */
   SGE_TYPE_USERSET,     /* MGR - the reserved "manager" access list */
   SGE_TYPE_USERSET,     /* OP - the reserved "operator" access list */
};

/* the second array on the same index - see the note at QConfObj (CS-2808) */
static_assert(sizeof(qconf_master_list) / sizeof(qconf_master_list[0]) ==
              static_cast<size_t>(QConfObj::OP) + 1,
              "qconf_master_list needs exactly one row per QConfObj value");

/** @brief Which of qmaster's master lists holds the objects of a type, see the header
 * @param o the object type
 * @return its master list type, SGE_TYPE_NONE when it is not held in one
 */
sge_object_type
qconf_master_list_type(const QConfObj o) {
   return qconf_master_list[static_cast<size_t>(o)];
}

const char *
qconf_object_label(const qconf_object_desc &desc, const char *name) {
   if (name != nullptr) {
      return name;
   }
   return (desc.json_type_name != nullptr) ? desc.json_type_name : "";
}

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

/** @brief Whether an object's name attribute is a host field
 *
 * Host names are compared case- and domain-insensitively, plain strings byte by
 * byte, so which of the two a name field is decides how it is looked up
 * (CS-2304: EH_name and friends are lHostT).
 *
 * @param descr the object's CULL descriptor
 * @param name_nm the name attribute
 * @return true when the attribute is a host field
 */
bool
qconf_name_is_host(const lDescr *descr, const int name_nm) {
   return lGetType(descr, name_nm) == lHostT;
}

/** @brief Read an object's name attribute as a string
 *
 * Whether the field is a plain string or a host field (CS-2304), what the
 * caller wants is its text.
 *
 * @param ep the object
 * @param name_nm the name attribute
 * @return the name, owned by @p ep
 */
const char *
qconf_get_name(const lListElem *ep, const int name_nm) {
   if (qconf_name_is_host(lGetElemDescr(ep), name_nm)) {
      return lGetHost(ep, name_nm);
   }
   return lGetString(ep, name_nm);
}

/** @brief Write an object's name attribute
 *
 * The counterpart of qconf_get_name(): whether the field is a plain string or a
 * host field decides how the name goes in, the same way it decides how it comes
 * out (CS-2304).
 *
 * @param ep the object
 * @param name_nm the name attribute
 * @param name the name to set
 */
void
qconf_set_name(lListElem *ep, const int name_nm, const char *name) {
   if (qconf_name_is_host(lGetElemDescr(ep), name_nm)) {
      lSetHost(ep, name_nm, name);
   } else {
      lSetString(ep, name_nm, name);
   }
}

/** @brief Whether a name is present in a list of objects
 *
 * Compared the way the name field demands, see qconf_name_is_host().
 *
 * @param names the list to search
 * @param descr the object's CULL descriptor
 * @param name_nm the name attribute
 * @param name the name to look for
 * @return true when the list holds an element of that name
 */
bool
qconf_name_in_list(const lList *names, const lDescr *descr, const int name_nm, const char *name) {
   if (names == nullptr || name == nullptr) {
      return false;
   }
   return qconf_name_is_host(descr, name_nm)
      ? (lGetElemHost(names, name_nm, name) != nullptr)
      : (lGetElemStr(names, name_nm, name) != nullptr);
}

/** @brief Read a whole list of objects out of a text or a file (CS-2823)
 *
 * The counterpart of qconf_parse_object() for a type whose text holds a list
 * rather than one object - the resource quota sets and the complex list. The
 * list is one unit: a text that does not parse yields nothing, and an element
 * the validator refuses discards the whole list, which is what the client's own
 * reader did with such a file (rqs_modify_from_file() sent nothing at all when
 * one rule set was rejected).
 *
 * `fields_out` is deliberately not used. The resource quota sets cannot be
 * tracked that way - the same field name appears once per rule - which is why
 * the client passed a null pointer there too, so a list has no
 * unprocessed-field check.
 *
 * @param answer_list receives the messages of this parse
 * @param descr the CULL descriptor of one element
 * @param fields the field list to read
 * @param format the flatfile format the text is in
 * @param filename the file to read, or the name a text is reported under
 * @param content the text, or nullptr to read from @p filename
 * @param validate the object type's validator, run per element, or nullptr
 * @param sfi the object type's spooling instruction
 * @param prepare the object type's transform, run per element, or nullptr
 * @param parse_values whether attribute values are parsed
 * @return the list, which the caller owns, or nullptr when it could not be read
 */
lList *
qconf_parse_list(lList **answer_list, const lDescr *descr, spooling_field *fields,
                 const spool_flatfile_format format, const char *filename, const char *content,
                 int (*validate)(const lListElem *, lList **),
                 const spool_flatfile_instr *sfi,
                 int (*prepare)(lListElem *, lList **),
                 const bool parse_values) {
   DENTER(TOP_LAYER);

   lList *local = nullptr;
   lList *lp = spool_flatfile_read_list(&local, descr, fields, nullptr,
                                        parse_values, sfi, format,
                                        nullptr, filename, content);
   bool failed = answer_list_has_error(&local) || lp == nullptr;

   for (lListElem *ep = lFirstRW(lp); !failed && ep != nullptr; ep = lNextRW(ep)) {
      if (prepare != nullptr && prepare(ep, &local) != STATUS_OK) {
         failed = true;
      } else if (validate != nullptr && validate(ep, &local) != STATUS_OK) {
         failed = true;
      }
   }

   if (failed) {
      lFreeList(&lp);
   }

   answer_list_append_list(answer_list, &local);
   DRETURN(lp);
}


/** @brief Parse one object from a flat file or from text, collecting the messages
 *
 * The chain qconf has always run before it sends an object: read it, make sure
 * no field was left unprocessed, let the object type's `prepare` transform it
 * (resolving a host name, for instance), then let its `validate` judge it.
 *
 * It used to be a private function of the client. It lives here because a
 * stored procedure runs inside qmaster and needs the very same chain - and
 * because having it twice would mean two behaviours drifting apart, with only
 * one of them covered by the characterisation test.
 *
 * The one deliberate difference to the client's old copy: it **collects** its
 * messages in @p answer_list rather than printing them. Printing is not an
 * option in the daemon, and the messages are what travels back to the caller
 * as the request's answers. The client keeps a thin wrapper that collects and
 * then prints.
 *
 * Parsing goes into a local list first and is appended on the way out. That is
 * not tidiness: the test for "did this read fail" is whether an **error** was
 * produced, not only whether the reader returned nothing, and asking that of a
 * list shared by a whole batch would let one bad item discard every item after
 * it.
 *
 * @param answer_list receives the messages of this parse
 * @param descr the object's CULL descriptor
 * @param fields the field list to read
 * @param format the flatfile format the text is in
 * @param filename the file to read, `-` for stdin, or nullptr when @p content is given
 * @param content the object's text, or nullptr to read from @p filename
 * @param validate the object type's validator, or nullptr
 * @param sfi the object type's spooling instruction
 * @param prepare the object type's transform, or nullptr
 * @param parse_values whether attribute values are parsed
 * @return the parsed object, which the caller owns, or nullptr when it could not be read
 */
lListElem *
qconf_parse_object(lList **answer_list, const lDescr *descr, spooling_field *fields,
                   const spool_flatfile_format format, const char *filename, const char *content,
                   int (*validate)(const lListElem *, lList **),
                   const spool_flatfile_instr *sfi,
                   int (*prepare)(lListElem *, lList **),
                   const bool parse_values) {
   DENTER(TOP_LAYER);

   lList *local = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   fields_out[0] = NoName;

   /* CS-2299 H4: "-" reads the object from stdin. spool_flatfile_read_object()
    * only closes the stream when it opened it itself, so passing stdin is safe. */
   const bool from_stdin = (content == nullptr && filename != nullptr && strcmp(filename, "-") == 0);
   lListElem *ep = spool_flatfile_read_object(&local, descr, nullptr, fields, fields_out,
                                              parse_values, sfi, format,
                                              from_stdin ? stdin : nullptr, filename, content);
   bool failed = answer_list_has_error(&local);

   if (!failed && ep != nullptr &&
       spool_get_unprocessed_field(fields, fields_out, &local) != NoName) {
      failed = true;
   }
   /* CS-2304: the transform runs before the name is used, so a prepare that
    * fails must stop the object from being applied. */
   if (!failed && ep != nullptr && prepare != nullptr && prepare(ep, &local) != STATUS_OK) {
      failed = true;
   }
   if (!failed && ep != nullptr && validate != nullptr && validate(ep, &local) != STATUS_OK) {
      failed = true;
   }
   if (failed) {
      lFreeElem(&ep);
   }

   answer_list_append_list(answer_list, &local);
   DRETURN(ep);
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
