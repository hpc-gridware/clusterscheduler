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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/ocs_spool_json.h"

#include <cstring>

#include "uti/sge_stdlib.h"

#include "ocs_QConfObject.h"
#include "msg_qconf.h"

#include "msg_common.h"
/* MSG_NORQSFOUND is in the clients' shared catalogue; libs/sgeobj reaches for
 * that header from the library side too, see ocs_QConfModelBase.cc */

/* qconf_build_fields_fn providers (KTD8): objects with a shared static field table
 * report owned=false; objects whose import field list is built on demand report
 * owned=true so the engine frees it. The provider mirrors exactly the field list the
 * matching -A<obj> import handler reads with. */
static spooling_field *qconf_fields_CAL (spool_flatfile_format, bool, bool *o) { *o = false; return CAL_fields;  }
static spooling_field *qconf_fields_CK  (spool_flatfile_format, bool, bool *o) { *o = false; return CK_fields;   }
static spooling_field *qconf_fields_CE  (spool_flatfile_format, bool, bool *o) { *o = false; return CE_fields;   }
static spooling_field *qconf_fields_RL  (spool_flatfile_format, bool, bool *o) { *o = false; return RL_fields;   }
static spooling_field *qconf_fields_PE  (spool_flatfile_format, bool, bool *o) { *o = false; return PE_fields;   }
static spooling_field *qconf_fields_HGRP(spool_flatfile_format, bool, bool *o) { *o = false; return HGRP_fields; }
static spooling_field *qconf_fields_CQ  (spool_flatfile_format, bool, bool *o) { *o = false; return CQ_fields;   }
static spooling_field *qconf_fields_US  (spool_flatfile_format, bool, bool *o) { *o = false; return US_fields;   }
static spooling_field *qconf_fields_RQS (spool_flatfile_format, bool, bool *o) { *o = false; return RQS_fields;  }
static spooling_field *qconf_fields_CAT (spool_flatfile_format, bool, bool *o) { *o = false; return CAT_fields;  }
static spooling_field *qconf_fields_SC  (spool_flatfile_format, bool, bool *o) { *o = false; return SC_fields;   }
/* import list omits runtime load_values/processors (to_stdout=false) so it round-trips (KTD2) */
static spooling_field *qconf_fields_EH  (spool_flatfile_format, bool, bool *o) { *o = true;  return sge_build_EH_field_list(false, false, false, false); }
/* -se shows the host as it is now, so the runtime values are included (CS-2796) */
static spooling_field *qconf_show_fields_EH(spool_flatfile_format, bool more_info, bool *o) { *o = true;  return sge_build_EH_field_list(false, true, false, more_info); }

/* defined below, next to the json helpers it uses */
static void qconf_show_prepare_EH(ocs::tom::Element<ocs::tom::EH> ep,
                                  ocs::tom::ConstList<ocs::tom::CE> master_centry_list);
static spooling_field *qconf_fields_PR  (spool_flatfile_format, bool, bool *o) { *o = true;  return sge_build_PR_field_list(false); }
static spooling_field *qconf_fields_UU  (spool_flatfile_format, bool, bool *o) { *o = true;  return sge_build_UU_field_list(false); }
/* the cluster configuration has no static field table, only a builder (CS-2808) */
static spooling_field *qconf_fields_CONF(spool_flatfile_format, bool, bool *o) { *o = true;  return sge_build_CONF_field_list(false); }
static spooling_field *qconf_fields_STN (spool_flatfile_format fmt, bool, bool *o) {
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
qconf_pe_validate(const ocs::tom::Element<ocs::tom::PE> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(pe_validate(ep, alpp, 0, ocs::tom::ConstList<ocs::tom::US>()));
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
qconf_role_validate(const ocs::tom::Element<ocs::tom::RL> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::Role::validate(ep, alpp, false) ? STATUS_OK : STATUS_ESEMANTIC);
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
qconf_eh_resolve(const ocs::tom::Element<ocs::tom::EH> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   if (ocs_resolve_host<EH_name>(ep) != CL_RETVAL_OK) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_SGETEXT_CANTRESOLVEHOST_S, ep.get<EH_name>().c_str());
      DRETURN(STATUS_ESEMANTIC);
   }
   DRETURN(STATUS_OK);
}

/**
 * @brief Validate-hook adapter for cluster queues (CS-2305).
 *
 * cqueue attribute verification (cqueue_verify_attributes) is what the cqueue
 * GDI wrapper runs before ADD/MOD; this maps it to the hook's status convention.
 *
 * @param cqueue the parsed cluster queue object to verify
 * @param alpp   answer list to receive verification messages
 * @return STATUS_OK on success, STATUS_ESEMANTIC on failure
 */
static int
qconf_cq_verify(const ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(cqueue_verify_attributes(cqueue, alpp, cqueue, false, ocs::tom::ConstList<ocs::tom::CAL>(),
                                    ocs::tom::ConstList<ocs::tom::CK>(), ocs::tom::ConstList<ocs::tom::PE>(),
                                    ocs::tom::ConstList<ocs::tom::US>(), ocs::tom::ConstList<ocs::tom::PR>(),
                                    ocs::tom::ConstList<ocs::tom::CE>(), ocs::tom::ConstList<ocs::tom::CQ>(),
                                    ocs::tom::ConstList<ocs::tom::HGRP>())
           ? STATUS_OK : STATUS_ESEMANTIC);
}

/**
 * @brief Validate-hook adapter for checkpointing environments
 *
 * ckpt_validate() takes a read-only element; the hook hands in the parsed one.
 *
 * @param ep   the parsed checkpointing object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_ck_validate(const ocs::tom::Element<ocs::tom::CK> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(ckpt_validate(ep, alpp));
}

/**
 * @brief Validate-hook adapter for usersets (CS-2310).
 *
 * userset_validate_entries() is what the interactive -Au/-Mu paths run before
 * ADD/MOD.
 *
 * @param ep   the parsed userset object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_us_validate(const ocs::tom::Element<ocs::tom::US> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(userset_validate_entries(ep, alpp));
}

/* ===== CS-2795: the object descriptor table ================================
 * The generic engines above (qconf_apply_path, qconf_delete_path,
 * qconf_export_path, qconf_export_singleton) already drive every file-based
 * option from one implementation each. What every call site still repeated was
 * the tuple identifying the object: target, descriptor, name field, field list,
 * spooling instruction and the object's hooks. This table holds it once.
 *
 * Adding a new configuration object type is now one entry type in
 * #ocs_qconf_object_tuple plus its row and its field provider, instead of a
 * tuple copied into each of the add, modify, delete and export branches. */

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
 * A name the constructor rejects yields an empty handle with the reason in the
 * answer list, which is the right reaction: a half built template must not reach
 * an editor.
 */

/** @brief The template of a calendar
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::CAL>
qconf_template_CAL([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(sge_generic_cal(name));
}

/** @brief The template of a checkpointing environment
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::CK>
qconf_template_CK([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(sge_generic_ckpt(name));
}

/** @brief The template of a parallel environment
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::PE>
qconf_template_PE([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(pe_create_template(name));
}

/** @brief The template of a complex entry
 * @param answer_list receives the reason when the name is not a valid one
 * @param name the name the template carries
 * @return the template, or an empty handle when the name was refused
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
qconf_template_CE(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(centry_create(answer_list, name));
}

/** @brief The template of a role
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::RL>
qconf_template_RL([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = ocs::Role::create_template();
   ep->set<RL_name>(name);
   DRETURN(ep);
}

/** @brief The template of a user
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::UU>
qconf_template_UU([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = getUserTemplate();
   ep->set<UU_name>(name);
   DRETURN(ep);
}

/** @brief The template of a project
 * @param answer_list unused, the constructor reports nothing
 * @param name the name the template carries
 * @return the template
 */
static ocs::tom::ElementPtr<ocs::tom::PR>
qconf_template_PR([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = getPrjTemplate();
   ep->set<PR_name>(name);
   DRETURN(ep);
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
static ocs::tom::ElementPtr<ocs::tom::RQS>
qconf_template_RQS([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   ep->set<RQS_name>(name);
   rqs_set_defaults(*ep);
   DRETURN(ep);
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
qconf_rqs_validate(const ocs::tom::Element<ocs::tom::RQS> ep, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   DRETURN(rqs_verify_attributes(ep, alpp, false, ocs::tom::ConstList<ocs::tom::CE>())
           ? STATUS_OK : STATUS_ESEMANTIC);
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
static ocs::tom::ElementPtr<ocs::tom::HGRP>
qconf_template_HGRP(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(hgroup_create(answer_list, name, ocs::tom::ListPtr<ocs::tom::HR>(), false));
}

/** @brief The template of a cluster queue
 *
 * Two steps, as the client's own template always was: the object, then the
 * attribute defaults every queue starts from.
 *
 * @param answer_list receives the reason when the template could not be built
 * @param name the name the template carries
 * @return the template, or an empty handle
 */
static ocs::tom::ElementPtr<ocs::tom::CQ>
qconf_template_CQ(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = cqueue_create(answer_list, name);
   if (ep != nullptr && !cqueue_set_template_attributes(*ep, answer_list)) {
      ep.reset();
   }
   DRETURN(ep);
}

/** @brief The template of a userset
 * @param answer_list receives the reason when the template could not be built
 * @param name the name the template carries
 * @return the template, or an empty handle
 */
static ocs::tom::ElementPtr<ocs::tom::US>
qconf_template_US(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(userset_create(answer_list, name));
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
static ocs::tom::ElementPtr<ocs::tom::STN>
qconf_template_STN([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, [[maybe_unused]] const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(getSNTemplate());
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
static ocs::tom::ElementPtr<ocs::tom::CONF>
qconf_template_CONF([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);
   auto ep = ocs::tom::ElementPtr<ocs::tom::CONF>::create();
   if (name != nullptr) {
      ep->set<CONF_name>(ocs::tom::Host(name));
   }
   DRETURN(ep);
}

/** @brief The checks a rule set text needs the whole list for (CS-2823)
 *
 * One check, and it was an `if` in qconf_write_from_procedure() until the
 * complex list brought three more: a text that names the same rule set twice is
 * ambiguous, and was refused before it was ever sent. The message says
 * "replacement request" because that is the request it used to guard - the
 * wording is kept, the situation is the same one.
 *
 * @param rule_sets the rule sets one item's text held
 * @param alpp receives the reason
 * @return STATUS_OK, or STATUS_ERROR1 with the reason in @p alpp
 */
static int
qconf_rqs_verify_list(const ocs::tom::ConstList<ocs::tom::RQS> rule_sets, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   for (auto a = rule_sets.front(); a != nullptr; a = a.next()) {
      const char *name_a = a.get<RQS_name>();
      if (name_a == nullptr) {
         continue;         // a nameless rule set is qconf_rqs_validate()'s to refuse
      }
      for (auto b = a.next(); b != nullptr; b = b.next()) {
         const char *name_b = b.get<RQS_name>();
         if (name_b != nullptr && strcmp(name_a, name_b) == 0) {
            answer_list_add_sprintf(alpp, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                                    MSG_RQS_REQUEST_DUPLICATE_NAME_S, name_a);
            DRETURN(STATUS_ERROR1);
         }
      }
   }
   DRETURN(STATUS_OK);
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
 * @param entries the complex entries one item's text held
 * @param alpp receives the reason
 * @return STATUS_OK, or STATUS_EUNKNOWN with the reason in @p alpp
 */
static int
qconf_ce_verify_list(const ocs::tom::ConstList<ocs::tom::CE> entries, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   for (auto a = entries.front(); a != nullptr; a = a.next()) {
      const char *name_a = a.get<CE_name>();
      const char *shortcut_a = a.get<CE_shortcut>();

      if (name_a == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                 MSG_CENTRY_NULL_NAME);
         DRETURN(STATUS_EUNKNOWN);
      }
      if (shortcut_a == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_CENTRY_NULL_SHORTCUT_S, name_a);
         DRETURN(STATUS_EUNKNOWN);
      }
      if (a.get<CE_urgency_weight>() == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                 MSG_CENTRY_NULL_URGENCY);
         DRETURN(STATUS_EUNKNOWN);
      }

      for (auto b = a.next(); b != nullptr; b = b.next()) {
         const char *name_b = b.get<CE_name>();
         const char *shortcut_b = b.get<CE_shortcut>();
         if (name_b == nullptr || shortcut_b == nullptr) {
            continue;      // reported when the loop above reaches that element
         }
         if (strcmp(name_a, name_b) == 0 || strcmp(name_a, shortcut_b) == 0 ||
             strcmp(shortcut_a, name_b) == 0 || strcmp(shortcut_a, shortcut_b) == 0) {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_COMPLEXXALREADYEXISTS_SS, name_a, shortcut_a);
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }
   DRETURN(STATUS_OK);
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
qconf_ce_retain_builtin(const ocs::tom::ConstElement<ocs::tom::CE> ep) {
   DENTER(TOP_LAYER);
   const char *name = ep.get<CE_name>();
   if (name == nullptr) {
      DRETURN(false);
   }
   // both spellings: get_rsrc() answers per queue and per host resource
   DRETURN(get_rsrc(name, true, nullptr, nullptr, nullptr, nullptr) == 0 ||
           get_rsrc(name, false, nullptr, nullptr, nullptr, nullptr) == 0);
}

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
qconf_stn_has_ids(const ocs::tom::ConstElement<ocs::tom::STN> ep) {
   for (const auto child : ep.get<STN_children>()) {
      if (child.get<STN_id>() != 0 || qconf_stn_has_ids(child)) {
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
qconf_stn_prepare(const ocs::tom::Element<ocs::tom::STN> ep, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   if (qconf_stn_has_ids(ep)) {
      DRETURN(STATUS_OK);
   }
   DRETURN(id_sharetree(answer_list, ep, 0) ? STATUS_OK : STATUS_EUNKNOWN);
}

/** @brief Does the bulk export of the exec hosts leave this one out? (CS-2826)
 *
 * The exec host list holds two entries that are not hosts: `template` carries
 * the defaults a new host starts from and `global` the settings that apply to
 * all of them. Neither is an object a restore would want to create, so a
 * directory meant to be read back by `-Ae` has no business holding a file for
 * them - which is why the client left them out of its own fetch.
 *
 * strcasecmp() and not sge_hostcmp(), although EH_name is a host field: this is
 * the comparison the client's condition used (`%Ic=`), and the host comparison
 * normalises domains - which would take a real host named `global.<domain>` out
 * of the export as well.
 *
 * @param name the exec host name
 * @return true when the bulk export skips this host
 */
static bool
qconf_eh_export_exclude(const char *name) {
   return name != nullptr &&
          (strcasecmp(name, SGE_TEMPLATE_NAME) == 0 ||
           strcasecmp(name, SGE_GLOBAL_NAME) == 0);
}

/** @brief Does the bulk export of the host groups leave this one out? (CS-2906)
 *
 * Two of the groups a cluster holds cannot be restored from a file of their own:
 *
 *  - `@exec_hosts` is derived from the execution host list, and no GDI write may
 *    touch it - for every role including manager (CS-2438)
 *  - `@@<queue>` carries the host list of its cluster queue and is created with
 *    it, so its add is refused (CS-2677 N-O-1). The queue export writes that
 *    same membership into the queue's own file, so a second file for the group
 *    would give a restore two sources of truth for one list, with the last
 *    writer winning silently
 *
 * The queue file is the one that holds it: the membership is the queue's, the
 * group's lifecycle is the system's.
 *
 * What `@@` means is hgroup_is_queue_group()'s to know, so the product's own
 * predicates answer this rather than a name or a pattern spelled out here.
 *
 * @param name the host group name including the leading '@'
 * @return true when the bulk export skips this group
 */
static bool
qconf_hgrp_export_exclude(const char *name) {
   return hgroup_is_system_maintained(name) || hgroup_is_queue_group(name);
}

/** @brief One entry per object type, at the index of its QConfObj value.
 *
 * The entry types - which object type and key field each value has - are in
 * #ocs_qconf_object_tuple; the order is checked at compile time. Each entry is
 * the untyped row, the master list holding the objects, and the typed hooks.
 *
 * The master list used to be a second array on the same index, see
 * qconf_master_list_type(). It is a member of the entry now, so it cannot get
 * out of step with the rows.
 */
const ocs_qconf_object_tuple ocs_qconf_objects{
   /* CAL  */ { { ocs::gdi::Target::CAL_LIST,  qconf_fields_CAL,  &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "calendar", .not_found_msg = MSG_CALENDAR_XISNOTACALENDAR_S } },
               SGE_TYPE_CALENDAR,
               { .create_template = qconf_template_CAL } },
   /* CK   */ { { ocs::gdi::Target::CK_LIST,   qconf_fields_CK,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "ckpt interface definition", .not_found_msg = MSG_CKPT_XISNOTCHKPINTERFACEDEF_S } },
               SGE_TYPE_CKPT,
               { .validate = qconf_ck_validate, .create_template = qconf_template_CK } },
   /* CE   */ { { ocs::gdi::Target::CE_LIST,   qconf_fields_CE,   &qconf_ce_sfi,              nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "complex entry", .not_found_msg = MSG_CENTRY_DOESNOTEXIST_S,
                   .list_sfi = &qconf_ce_list_sfi },
                 /* as_list stays false: the type's single-object switches are the
                  * ones a *read* renders, and "-mc"/"-Mc" say MODE_LIST on the
                  * request instead. The write part is what makes the table form
                  * writable at all - the whole-list checks, and the built-ins a
                  * replace keeps (CS-2823) */
                 { .retain_msg = MSG_CENTRY_KEPT_BUILTIN_S } },
               SGE_TYPE_CENTRY,
               { .create_template = qconf_template_CE, .verify_list = qconf_ce_verify_list,
                 .retain_on_replace = qconf_ce_retain_builtin } },
   /* EH   */ { { ocs::gdi::Target::EH_LIST,   qconf_fields_EH,   &qconf_sfi,                 "exechost",   ocs::gdi::SubCommand::NONE,    false, true,  false, qconf_eh_export_exclude,
                 { .fields = qconf_show_fields_EH, .list_name = "execution host", .not_found_msg = MSG_EXEC_XISNOTANEXECUTIONHOST_S } },
               SGE_TYPE_EXECHOST,
               { .prepare = qconf_eh_resolve, .show_prepare = qconf_show_prepare_EH } },
   /* HGRP */ { { ocs::gdi::Target::HGRP_LIST, qconf_fields_HGRP, &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, qconf_hgrp_export_exclude,
                 { .list_name = "host group list" } },
               SGE_TYPE_HGROUP,
               { .create_template = qconf_template_HGRP } },
   /* PE   */ { { ocs::gdi::Target::PE_LIST,   qconf_fields_PE,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "parallel environment", .not_found_msg = MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S } },
               SGE_TYPE_PE,
               { .validate = qconf_pe_validate, .create_template = qconf_template_PE } },
   /* PR   */ { { ocs::gdi::Target::PR_LIST,   qconf_fields_PR,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "project list", .not_found_msg = MSG_PROJECT_XISNOKNWOWNPROJECT_S } },
               SGE_TYPE_PROJECT,
               { .create_template = qconf_template_PR } },
   /* CQ   */ { { ocs::gdi::Target::CQ_LIST,   qconf_fields_CQ,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::SET_ALL, false, false, false, nullptr,
                 { .list_name = "cqueue list" } },
               SGE_TYPE_CQUEUE,
               { .validate = qconf_cq_verify, .create_template = qconf_template_CQ } },
   /* RL   */ { { ocs::gdi::Target::RL_LIST,   qconf_fields_RL,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "role", .not_found_msg = MSG_ROLE_DOESNOTEXIST_S } },
               SGE_TYPE_RL,
               { .validate = qconf_role_validate, .create_template = qconf_template_RL } },
   /* RQS: SubCommand::SET_ALL, not NONE - every live resource quota path sends
    * SET_ALL, and the row was only ever unread because the type had a write
    * path of its own. With NONE a rule set's sublists would be merged instead of
    * replaced, so a limit the administrator deleted in the editor would survive
    * (CS-2823). */
   /* RQS  */ { { ocs::gdi::Target::RQS_LIST,  qconf_fields_RQS,  &qconf_rqs_sfi,             nullptr,      ocs::gdi::SubCommand::SET_ALL, true,  true,  false, nullptr,
                 /* -srqs writes the whole list in one go: qconf_rqs_sfi wraps every
                  * record in braces, which a loop over single objects would not do */
                 /* Both messages, because they answer two different questions and
                  * one field was serving both until CS-2928: empty_msg for "you
                  * named nothing, or nothing of what you named exists", and the
                  * noun for "this name you gave is not there". With only the
                  * former, "-srqs known,unknown" was a partial match - not empty,
                  * so nothing reported it - and the read succeeded without ever
                  * mentioning the name it could not find. */
                 { .list_name = "resource quota set list",
                   .empty_msg = MSG_NORQSFOUND, .not_found_noun = "resource quota set",
                   .write_as_list = true },
                 /* no retain hook and no replace: a modify writes what the text
                  * holds and removes nothing (CS-2823) */
                 { } },
               SGE_TYPE_RQS,
               { .validate = qconf_rqs_validate, .create_template = qconf_template_RQS,
                 .verify_list = qconf_rqs_verify_list } },
   /* US   */ { { ocs::gdi::Target::US_LIST,   qconf_fields_US,   &qconf_param_sfi,           nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 /* -su names the userset it could not find, and calls it an access
                  * list, as it always did (CS-2819) */
                 { .list_name = "userset list", .not_found_noun = "access list" } },
               SGE_TYPE_USERSET,
               { .validate = qconf_us_validate, .create_template = qconf_template_US } },
   /* UU   */ { { ocs::gdi::Target::UU_LIST,   qconf_fields_UU,   &qconf_sfi,                 nullptr,      ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .list_name = "user list", .not_found_msg = MSG_USER_XISNOKNOWNUSER_S } },
               SGE_TYPE_USER,
               { .create_template = qconf_template_UU } },
   /* STN  */ { { ocs::gdi::Target::STN_LIST,  qconf_fields_STN,  &qconf_name_value_list_sfi, "sharetree",  ocs::gdi::SubCommand::NONE,    false, true,  true,  nullptr,
                 { .empty_msg = MSG_OBJ_NOSTREEELEM } },
               SGE_TYPE_SHARETREE,
               { .prepare = qconf_stn_prepare, .create_template = qconf_template_STN } },
   /* SC: the hand written -ssconf had no empty check at all - it handed a null
    * element to the writer and reported the complaint of the list library plus this line. The
    * path cannot occur on a running cluster; inside qmaster a null element must
    * not reach the writer, so the check is explicit here and reports the closer
    * of the two messages (CS-2808). */
   /* SC   */ { { ocs::gdi::Target::SC_LIST,   qconf_fields_SC,   &qconf_comma_sfi,           "sched_conf", ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,
                 { .empty_msg = MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION } },
               SGE_TYPE_SCHEDD_CONF,
               { } },
   /* CONF: the one type whose text does not carry its own name - the host is the
    * argument of "-mconf" or the basename of the "-Mconf" file, and the read
    * renders it as a comment (name_header). So the write is given the name as a
    * request parameter, see QConfWriteParameter::set_name(). Its modify is also
    * an upsert in qmaster, which is why "-aconf" and "-mconf" are one thing
    * (CS-2823). */
   /* CONF */ { { ocs::gdi::Target::CONF_LIST, qconf_fields_CONF, &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "config", .not_found_msg = MSG_ANSWER_CONFIGXNOTDEFINED_S, .name_header = true } },
               SGE_TYPE_CONFIG,
               { .create_template = qconf_template_CONF } },
   /* EV: read only and name-list only (CS-2808). There is no field list because
    * EV is not a registered object type and no -s<obj>/export path reaches
    * this row - show_name_list() renders the key field and nothing else. Event
    * clients live in the event master, not in a master list. */
   /* EV   */ { { ocs::gdi::Target::EV_LIST,   nullptr,           &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "scheduling host" } },
               SGE_TYPE_NONE,
               { } },
   /* CAT: read only, and the only type whose key is a number rather than a name.
    * The list has a JSON schema id of its own so that the enveloped array of
    * -scatl does not collide with the single record of -scat (CS-2819). */
   /* CAT  */ { { ocs::gdi::Target::CAT_LIST,  qconf_fields_CAT,  &qconf_cat_sfi,            "category",   ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "category", .not_found_msg = MSG_CAT_DOESNOTEXIST_U,
                   .list_sfi = &qconf_cat_list_sfi, .json_list_id = "category-list" } },
               SGE_TYPE_CATEGORY,
               { } },
   /* AH, SH, MGR, OP: the four reserved member lists (CS-2820). Each row names a
    * reserved object of an existing type and the sublist that -sh/-ss/-sm/-so
    * report; the write fields are empty because no write path reaches these rows,
    * and there is no field list because the names are rendered by
    * show_name_list() and nothing else - the same reason the EV row has none.
    *
    * The entries are HR and UE, neither of them a registered object
    * type, so the JSON envelope would derive as "unknown" and has to be named.
    * Whether it is also named for an EMPTY list is not a free choice: -sh and -ss
    * produced the generic "names" key there and -sm and -so the full envelope,
    * and scripts see the difference. */
   /* AH: @admin_hosts lives in the host group list */
   /* AH   */ { { ocs::gdi::Target::HGRP_LIST, nullptr,           &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "administrative host", .json_name_id = "adminhost", .json_name_id_when_empty = false,
                   .member = { ADMIN_HOSTGROUP, true } } },
               SGE_TYPE_HGROUP,
               { } },
   /* SH: @submit_hosts likewise */
   /* SH   */ { { ocs::gdi::Target::HGRP_LIST, nullptr,           &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "submit host", .json_name_id = "submithost", .json_name_id_when_empty = false,
                   .member = { SUBMIT_HOSTGROUP, true } } },
               SGE_TYPE_HGROUP,
               { } },
   /* MGR: the reserved "manager" access list */
   /* MGR  */ { { ocs::gdi::Target::US_LIST,   nullptr,           &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "manager", .json_name_id = "manager", .json_name_id_when_empty = true,
                   .member = { MANAGER_USERSET, false } } },
               SGE_TYPE_USERSET,
               { } },
   /* OP: the reserved "operator" access list */
   /* OP   */ { { ocs::gdi::Target::US_LIST,   nullptr,           &qconf_sfi,                nullptr,      ocs::gdi::SubCommand::NONE,    false, false, false, nullptr,
                 { .list_name = "operator", .json_name_id = "operator", .json_name_id_when_empty = true,
                   .member = { OPERATOR_USERSET, false } } },
               SGE_TYPE_USERSET,
               { } },
};

/** @brief Which of qmaster's master lists holds the objects of a type, see the header
 *
 * The qmaster-internal `gdi_object_t` table knows this for most types, but it
 * sits in `daemons/qmaster/sge_c_gdi.cc`, a library cannot reach it, and for the
 * three types whose modify is a special case there rather than a hook on the row
 * it carries SGE_TYPE_NONE (CONF, SC) - so it knows the list for the types that
 * never needed it. Stated in the entry instead, where the rest of a type's traits
 * are, and asked by the read and the write path alike (CS-2823).
 *
 * @param o the object type
 * @return its master list type, SGE_TYPE_NONE when it is not held in one
 */
sge_object_type
qconf_master_list_type(const QConfObj o) {
   return ocs_with_qconf_object(o, [](const auto &entry) {
      return entry.master_list;
   });
}

/** @brief What to call an object in a message, see the header
 * @param desc the object type's descriptor
 * @param name the object's name, or nullptr for a singleton
 * @return a label for a message, never nullptr
 */
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

   auto *copy = ocs::uti::c_alloc<spooling_field>(n + 1);
   memcpy(copy, fields, sizeof(spooling_field) * (n + 1));
   return copy;
}

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in #ocs_qconf_objects
 */
const qconf_object_desc &
qconf_desc(const QConfObj o) {
   return *ocs_with_qconf_object(o, [](const auto &entry) {
      return &entry.desc;
   });
}

/**
 * @brief Render an exec host's load_values typed by their complex definition (CS-2313a).
 *
 * For -fmt json, values such as mem_total are rendered as bytes, num_proc as an
 * integer, np_load_avg as a double, ... instead of as raw strings. HL load values
 * carry no type, so we build a CE list (name + string value), resolve each entry's
 * type from the master complex list (tolerant of load names that are not defined
 * complexes), parse numeric values into CE_doubleval, and swap the typed CE list into
 * the load-list field. The generic JSON writer then renders it via its typed
 * complex-value path; unmatched or string-typed loads keep their string value. Runs
 * only for -fmt json and mutates only the throwaway object about to be shown.
 *
 * The one untyped step is the swap: the list swapped in is a `CE` list in a
 * field declared to hold `HL`, which neither a typed setter nor the checked
 * run-time setter accepts; the core stores it.
 *
 * @param host               the (throwaway) exec host element to be shown
 * @param master_centry_list the complex entries the types are resolved from
 * @return none
 */
static void
qconf_json_type_load_values(const ocs::tom::Element<ocs::tom::EH> host,
                            const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);

   if (!host || !master_centry_list) {
      DRETURN_VOID;
   }
   const auto hl = host.get<EH_load_list>();
   if (hl.empty()) {
      DRETURN_VOID;
   }

   auto ce_list = ocs::tom::ListPtr<ocs::tom::CE>::create("load_values");
   for (const auto hlep : hl) {
      auto ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
      ce->set<CE_name>(hlep.get<HL_name>());
      ce->set<CE_stringval>(hlep.get<HL_value>());
      ce_list->push_back(std::move(ce));
   }

   /* fill CE_valtype from the complex definitions (skips unknown load names) */
   centry_list_fill_config(*ce_list, master_centry_list);

   /* parse numeric values into CE_doubleval so the writer emits native numbers */
   for (const auto ce : *ce_list) {
      const auto type = static_cast<ocs::CEntry::Type>(ce.get<CE_valtype>());
      const char *s = ce.get<CE_stringval>();
      double dval = 0.0;
      switch (type) {
         case ocs::CEntry::Type::INT:
         case ocs::CEntry::Type::TIME:
         case ocs::CEntry::Type::MEM:
         case ocs::CEntry::Type::DOUBLE:
         case ocs::CEntry::Type::BOOL:
            if (s != nullptr && parse_ulong_val(&dval, nullptr, type, s, nullptr, 0)) {
               ce.set<CE_doubleval>(dval);
            }
            break;
         default:
            break;   /* string-like / unmatched -> writer emits CE_stringval */
      }
   }

   // a CE list into the HL field of the throwaway host, see above. The run-time setter ocs::tom::set() checks
   // the sub-type the model names and would refuse it, so the field is stored through the core, which checks
   // only the field type; the host holds EH_load_list (read above), the element takes the list over
   const int load_list_pos = ocs::tom::checked_pos(host.raw(), EH_load_list, ocs::tom::FieldType::List);
   ocs::tom::store_list(host.raw(), load_list_pos, ce_list.release());

   DRETURN_VOID;
}

/** @brief Type the numeric fields of an exec host before it is rendered.
 *
 * CS-2313a: complex_values and the load values are strings in the object model, so -fmt
 * json would quote them without this.
 *
 * @param ep the exec host about to be written
 * @param master_centry_list the complex entries the types are resolved from
 * @return none
 */
static void
qconf_show_prepare_EH(const ocs::tom::Element<ocs::tom::EH> ep,
                      const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);
   ocs_qconf_json_fill_complex<ocs::tom::EH, EH_consumable_config_list>(ep, master_centry_list);
   qconf_json_type_load_values(ep, master_centry_list);
   DRETURN_VOID;
}
