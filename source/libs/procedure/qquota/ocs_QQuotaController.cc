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
 * @brief Controller of `qquota`: runs the request and drives the view
 */

#include <sstream>
#include "fnmatch.h"

#include "uti/sge_rmon_macros.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge.h"
#include "uti/sge_parse_num_par.h"

#include "sgeobj/ocs_CEntry.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_href.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "sched/sort_hosts.h"

#include "ocs_QQuotaController.h"

#include <iostream>


ocs::QQuotaController::~QQuotaController() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

char *
ocs::QQuotaController::qquota_get_next_filter(stringT filter, const char *cp) {
   // cp is part of a server-supplied RUE_name key; it may be NULL or lack the '/'
   // separators. Never form an invalid pointer from a failed strchr() (the old
   // `strchr(cp,'/')++` produced (char*)1 on a missing separator, which the next
   // call then dereferenced -> SIGSEGV) (CWE-469/CWE-476, CS-2348).
   if (cp == nullptr) {
      snprintf(filter, MAX_STRING_SIZE, "-");
      return nullptr;
   }
   const char *slash = strchr(cp, '/');
   if (slash == nullptr) {
      // no separator: emit "-" and return a pointer to the terminating NUL, so a
      // subsequent call re-scans an empty string instead of an invalid pointer
      snprintf(filter, MAX_STRING_SIZE, "-");
      return const_cast<char *>(cp + strlen(cp));
   }
   const ptrdiff_t len = slash - cp + 1;
   if (len > 1 && len < MAX_STRING_SIZE) {
      snprintf(filter, len, "%s", cp);
   } else {
      snprintf(filter, MAX_STRING_SIZE, "-");
   }
   return const_cast<char *>(slash + 1);
}

/** @brief Report one filter of a rule
 *
 * An expanded filter whose value is known is reported with that value,
 * every other one with its scope and its excluded scope.
 *
 * @param os stream to write to
 * @param filter the filter, nothing is reported for an empty handle
 * @param name the name of the filter, e.g. `users`
 * @param value the value the rule matched with, or nullptr
 * @param view the view to report to
 * @return none
 */
void
ocs::QQuotaController::qquota_print_out_filter(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::RQRF> filter,
                                               const char *name, const char *value, QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   if (filter == nullptr) {
      DRETURN_VOID;
   }
   if (!filter.get<RQRF_expand>() || value == nullptr) {
      for (const auto scope : filter.get<RQRF_scope>()) {
         view.report_limit_string_value(os, name, scope.get<ST_name>(), false);
      }
      for (const auto scope : filter.get<RQRF_xscope>()) {
         view.report_limit_string_value(os, name, scope.get<ST_name>(), true);
      }
   } else {
      view.report_limit_string_value(os, name, value, false);
   }
   DRETURN_VOID;
}

/** @brief Report one limit of a rule, unless it was reported before
 *
 * With several values for one filter (`-u user1,user2`) the same row can
 * match more than once; the key of each reported row is remembered.
 *
 * @param os stream to write to
 * @param rqs the resource quota set of the rule
 * @param rule the rule
 * @param limit_name the limited resource
 * @param type the type of the limited resource, for rendering the values
 * @param usage the current usage
 * @param limit the limit
 * @param qfilter the values the rule matched with
 * @param printed_rules the keys of the rows reported so far, extended by this one
 * @param view the view to report to
 * @return none
 */
void
ocs::QQuotaController::qquota_print_out_rule(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::RQS> rqs,
                                             const ocs::tom::ConstElement<ocs::tom::RQR> rule, const char *limit_name,
                                             const ocs::CEntry::Type type, const uint64_t usage, const uint64_t limit,
                                             const qquota_filter_t qfilter,
                                             const ocs::tom::List<ocs::tom::ST> printed_rules,
                                             QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   // create a unique key
   std::ostringstream oss_key;
   oss_key << rqs.get<RQS_name>() << "/" << rule.get<RQR_name>() << "," << limit_name << ","
           << (qfilter.user? qfilter.user: "") << "," << (qfilter.project? qfilter.project: "") << ","
           << (qfilter.pe? qfilter.pe: "") << "," << (qfilter.queue? qfilter.queue: "") << ","
           << (qfilter.host? qfilter.host: "");

   // check if output for the key was already done
   if (printed_rules.find_first<ST_name>(oss_key.str().c_str()) != nullptr) {
      DRETURN_VOID;
   }

   // remember the key to avoid duplicate output
   printed_rules.emplace_back<ST_name>(oss_key.str().c_str());

   // make the output
   view.report_limit_rule_begin(os, rqs.get<RQS_name>(), rule.get<RQR_name>());
   view.report_resource_value(os, limit_name, type, limit, usage);
   qquota_print_out_filter(os, rule.get<RQR_filter_users>(), "users", qfilter.user, view);
   qquota_print_out_filter(os, rule.get<RQR_filter_projects>(), "projects", qfilter.project, view);
   qquota_print_out_filter(os, rule.get<RQR_filter_pes>(), "pes", qfilter.pe, view);
   qquota_print_out_filter(os, rule.get<RQR_filter_queues>(), "queues", qfilter.queue, view);
   qquota_print_out_filter(os, rule.get<RQR_filter_hosts>(), "hosts", qfilter.host, view);
   view.report_limit_rule_finished(os);

   DRETURN_VOID;
}

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QQuotaController::process_request(QQuotaParameter &parameter, QQuotaModelBase &model, QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;

   qquota_filter_t qquota_filter = { "*", "*", "*", "*", "*" };

   /* If no user is requested on command line we set the current user as default */
   qquota_filter.user = component_get_username();


   /* Hash list of already printed resource quota rules (possible with -u user1,user2,user3...) */
   const auto printed_rules = ocs::tom::ListPtr<ocs::tom::ST>::create("rule_hash");
   const auto global_host =
      ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(model.get_exec_host_list(), SGE_GLOBAL_NAME));

   view.report_started(oss);

   for (const auto rqs : model.get_rqs_list()) {
      int rule_count = 1;

      DTRACE;

      if (!rqs.get<RQS_enabled>()) {
         continue;
      }

      for (const auto rule : rqs.get<RQS_rule>()) {
         auto user_ep = parameter.get_user_list().front();
         auto project_ep = parameter.get_project_list().front();
         auto pe_ep = parameter.get_pe_list().front();
         auto queue_ep = parameter.get_cqueue_list().front();
         auto host_ep = parameter.get_host_list().front();
         do {
            if (user_ep != nullptr) {
               DPRINTF("filter for user: %s\n", user_ep.get<ST_name>());
               qquota_filter.user = user_ep.get<ST_name>();
            }
            do {
               if (project_ep != nullptr) {
                  qquota_filter.project = project_ep.get<ST_name>();
               }
               do {
                  if (pe_ep != nullptr) {
                     qquota_filter.pe = pe_ep.get<ST_name>();
                  }
                  do {
                     if (queue_ep != nullptr) {
                        qquota_filter.queue = queue_ep.get<ST_name>();
                     }
                     do {
                        if (host_ep != nullptr) {
                           qquota_filter.host = host_ep.get<ST_name>();
                        }

                        if (rqs_is_matching_rule(rule, qquota_filter.user, nullptr, ocs::tom::ConstList<ocs::tom::ST>(),
                                                 qquota_filter.project, qquota_filter.pe, qquota_filter.host,
                                                 qquota_filter.queue, model.get_user_set_list(),
                                                 model.get_hgroup_list())) {

                           for (const auto limit : rule.get<RQR_limit>()) {
                              const char *limit_name = limit.get<RQRL_name>();
                              const auto rue_list = limit.get<RQRL_usage>();
                              // the centry is changed below: CE_pj_doubleval carries each value to render
                              const auto raw_centry = ocs::tom::Element<ocs::tom::CE>::from_typed(
                                 centry_list_locate(model.get_centry_list(), limit_name));

                              if (raw_centry == nullptr) {
                                 /* undefined centries can be ignored */
                                 DPRINTF("centry %s not defined -> IGNORING\n", limit_name);
                                 continue;
                              }
                              // attribute type drives value parsing (RQRL_value) and
                              // human-readable rendering of the limit (CS-2348)
                              const auto centry_type =
                                 static_cast<ocs::CEntry::Type>(raw_centry.get<CE_valtype>());

                              if (const auto rml = parameter.get_resource_match_list(); rml != nullptr &&
                                  ((centry_list_locate(rml, limit_name) == nullptr) &&
                                  (centry_list_locate(rml, raw_centry.get<CE_shortcut>()) == nullptr))) {
                                 DPRINTF("centry %s was not requested on CLI -> IGNORING\n", limit_name);
                                 continue;
                              }

                              // Set a rule name if it was not manually specified
                              if (!rule.get<RQR_name>()) {
                                 rule.set<RQR_name>(std::to_string(rule_count).c_str());
                              }

                              if (raw_centry.get<CE_consumable>()) {
                                 // for consumables we need to walk through the utilization and search for
                                 // matching values
                                 DPRINTF("found centry %s - consumable\n", limit_name);
                                 for (const auto rue_elem : rue_list) {
                                    uint32_t dominant = 0;
                                    const char *rue_name = rue_elem.get<RUE_name>();
                                    char *cp = nullptr;
                                    stringT user, project, pe, queue, host;
                                    dstring limit_str = DSTRING_INIT;
                                    dstring value_str = DSTRING_INIT;
                                    qquota_filter_t qf = { nullptr, nullptr, nullptr, nullptr, nullptr };

                                    /* check user name */
                                    cp = qquota_get_next_filter(user, rue_name);
                                    /* usergroups have the same beginning character @ as host groups */
                                    if (is_hgroup_name(qquota_filter.user)) {
                                       if (const auto ugroup =
                                              model.get_user_set_list().find_first<US_name>(&qquota_filter.user[1]);
                                           ugroup != nullptr) {
                                          if (sge_contained_in_access_list(user, nullptr, nullptr, ugroup) == 0) {
                                             continue;
                                          }
                                       }
                                    } else {
                                       if (strcmp(user, "-") != 0 && strcmp(qquota_filter.user, "*") != 0) {
                                          if (is_pattern(qquota_filter.user)) {
                                             if (fnmatch(qquota_filter.user, user, 0) != 0) {
                                                continue;
                                             }
                                          } else {
                                             if (strcmp(qquota_filter.user, user) != 0) {
                                                continue;
                                             }
                                          }
                                       }
                                    }

                                    /* check project */
                                    cp = qquota_get_next_filter(project, cp);
                                    if ((strcmp(project, "-") != 0) && (strcmp(qquota_filter.project, "*") != 0)
                                          && (fnmatch(qquota_filter.project, project, 0) != 0)) {
                                       continue;
                                    }
                                    /* check parallel environment */
                                    cp = qquota_get_next_filter(pe, cp);
                                    if ((strcmp(pe, "-") != 0) && (strcmp(qquota_filter.pe, "*") != 0) &&
                                        (fnmatch(qquota_filter.pe, pe, 0) != 0) ) {
                                       continue;
                                    }
                                    /* check cluster queue */
                                    cp = qquota_get_next_filter(queue, cp);
                                    if ((strcmp(queue, "-") != 0) && (strcmp(qquota_filter.queue, "*") != 0) &&
                                        (fnmatch(qquota_filter.queue, queue, 0) != 0)) {
                                       continue;
                                    }
                                    /* check host name */
                                    cp = qquota_get_next_filter(host, cp);
                                    if (is_hgroup_name(qquota_filter.host)) {
                                       const auto hgroup_list = model.get_hgroup_list();
                                       if (const auto hgroup = hgroup_list_locate(hgroup_list, qquota_filter.host);
                                           hgroup != nullptr) {
                                          ocs::tom::ListPtr<ocs::tom::HR> host_list;
                                          hgroup_find_all_references(hgroup, nullptr, hgroup_list, &host_list,
                                                                     nullptr);

                                          /*
                                           * The row is suppressed unless its host is a member of the
                                           * group the user filtered on.
                                           *
                                           * This used to read "host_list == nullptr && lGetElemHost(...)
                                           * == nullptr", where the second half can never decide
                                           * anything: it is only evaluated when the list is null, and
                                           * then it is null too. So the filter suppressed nothing for
                                           * any non-empty group and everything for an empty one -
                                           * exactly backwards. Measured: with a group holding only
                                           * host-0003, "qquota -h @group" still printed the row of
                                           * host-0002.
                                           *
                                           * A null list needs no case of its own; the lookup answers
                                           * "not a member" for it.
                                           */
                                          if (!host_list->find_first<HR_name>(ocs::tom::Host(host))) {
                                             continue;
                                          }
                                       }
                                    } else {
                                       if ((strcmp(host, "-") != 0) && (strcmp(qquota_filter.host, "*") != 0) &&
                                           (fnmatch(qquota_filter.host, host, 0) != 0) ) {
                                          continue;
                                       }
                                    }
                                    uint64_t limit_value;
                                    if (limit.get<RQRL_dynamic>()) {
                                       const auto exec_host = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(
                                          host_list_locate(model.get_exec_host_list(), host));
                                       limit_value = static_cast<uint64_t>(
                                          scaled_mixed_load(limit.get<RQRL_value>(), global_host, exec_host,
                                                            model.get_centry_list(),
                                                            model.get_job_load_adjustments()));
                                       sge_dstring_sprintf(&limit_str, "%d", limit_value);

                                    } else {
                                       raw_centry.set<CE_pj_doubleval>(limit.get<RQRL_dvalue>());
                                       sge_get_dominant_stringval(raw_centry, &dominant, &limit_str, nullptr,
                                                                  &limit_value);
                                    }

                                    u_int64_t value_value;
                                    raw_centry.set<CE_pj_doubleval>(
                                                                   rue_elem.get<RUE_utilized_now>());
                                    sge_get_dominant_stringval(raw_centry, &dominant, &value_str, nullptr,
                                                               &value_value);

                                    qf.user = user;
                                    qf.project = project;
                                    qf.pe = pe;
                                    qf.queue = queue;
                                    qf.host = host;
                                    qquota_print_out_rule(oss, rqs, rule, limit_name, centry_type, value_value,
                                                          limit_value, qf, *printed_rules, view);

                                    sge_dstring_free(&limit_str);
                                    sge_dstring_free(&value_str);
                                 }
                              } else {
                                 /* static values */

                                 DPRINTF("found centry %s - static value\n", limit_name);

                                 qquota_filter_t qf{};
                                 // RQRL_value is a complex-attribute value spec (e.g. "4G",
                                 // "1:0:0"), server-supplied. Parse it according to the
                                 // attribute's type via parse_ulong_val(), which validates and
                                 // returns a status instead of throwing - std::stoul() here
                                 // crashed qquota on a NULL/non-numeric/out-of-range value and
                                 // silently mis-parsed typed values like "4G" (CWE-248, CS-2348).
                                 uint64_t limit_value = 0;
                                 double dval = 0.0;
                                 if (const char *value_str = limit.get<RQRL_value>();
                                     value_str != nullptr &&
                                     parse_ulong_val(&dval, nullptr, centry_type, value_str, nullptr, 0)) {
                                    limit_value = static_cast<uint64_t>(dval);
                                 }
                                 qquota_print_out_rule(oss, rqs, rule, limit_name,
                                                       centry_type, 0, limit_value, qf, *printed_rules, view);

                              }
                           }
                        }
                     } while ((host_ep = host_ep.next()) != nullptr);
                  } while ((queue_ep = queue_ep.next()) != nullptr);
               } while ((pe_ep = pe_ep.next()) != nullptr);
            } while ((project_ep = project_ep.next()) != nullptr);
         } while ((user_ep = user_ep.next()) != nullptr);
         rule_count++;
      }
   }

   view.report_finished(oss);

   // show the full output
   view.show(out_, oss.str().c_str());

   DRETURN_VOID;
}
