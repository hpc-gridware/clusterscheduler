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
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"

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

void
ocs::QQuotaController::qquota_print_out_filter(std::ostream &os, lListElem *filter, const char *name, const char *value, QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   if (filter == nullptr) {
      DRETURN_VOID;
   }
   if (!ocs::tom::get<RQRF_expand>(filter) || value == nullptr) {
      for_each_ep_lv(scope, ocs::tom::get<RQRF_scope>(filter)) {
         view.report_limit_string_value(os, name, ocs::tom::get<ST_name>(scope), false);
      }
      for_each_ep_lv(scope, ocs::tom::get<RQRF_xscope>(filter)) {
         view.report_limit_string_value(os, name, ocs::tom::get<ST_name>(scope), true);
      }
   } else {
      view.report_limit_string_value(os, name, value, false);
   }
   DRETURN_VOID;
}

void
ocs::QQuotaController::qquota_print_out_rule(std::ostream &os, const lListElem *rqs, lListElem *rule, const char *limit_name,
                                  ocs::CEntry::Type type, uint64_t usage, uint64_t limit, qquota_filter_t qfilter,
                                  lList *printed_rules, QQuotaViewBase &view) {
   // create a unique key
   std::ostringstream oss_key;
   oss_key << ocs::tom::get<RQS_name>(rqs) << "/" << ocs::tom::get<RQR_name>(rule) << "," << limit_name << ","
           << (qfilter.user? qfilter.user: "") << "," << (qfilter.project? qfilter.project: "") << ","
           << (qfilter.pe? qfilter.pe: "") << "," << (qfilter.queue? qfilter.queue: "") << ","
           << (qfilter.host? qfilter.host: "");

   // check if output for the key was already done
   if (static_cast<const lListElem *>(ocs::tom::find_first<ST_name>(printed_rules, oss_key.str().c_str())) != nullptr) {
      return;
   }

   // remember the key to avoid duplicate output
   ocs::tom::emplace_back<ST_name>(&printed_rules, oss_key.str().c_str());

   // make the output
   view.report_limit_rule_begin(os, ocs::tom::get<RQS_name>(rqs), ocs::tom::get<RQR_name>(rule));
   view.report_resource_value(os, limit_name, type, limit, usage);
   qquota_print_out_filter(os, ocs::tom::get<RQR_filter_users>(rule), "users", qfilter.user, view);
   qquota_print_out_filter(os, ocs::tom::get<RQR_filter_projects>(rule), "projects", qfilter.project, view);
   qquota_print_out_filter(os, ocs::tom::get<RQR_filter_pes>(rule), "pes", qfilter.pe, view);
   qquota_print_out_filter(os, ocs::tom::get<RQR_filter_queues>(rule), "queues", qfilter.queue, view);
   qquota_print_out_filter(os, ocs::tom::get<RQR_filter_hosts>(rule), "hosts", qfilter.host, view);
   view.report_limit_rule_finished(os);
}

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 */
void ocs::QQuotaController::process_request(QQuotaParameter &parameter, QQuotaModelBase &model, QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   lListElem* exec_host = nullptr;
   std::ostringstream oss;

   qquota_filter_t qquota_filter = { "*", "*", "*", "*", "*" };

   /* If no user is requested on command line we set the current user as default */
   qquota_filter.user = component_get_username();


   /* Hash list of already printed resource quota rules (possible with -u user1,user2,user3...) */
   lList* printed_rules = lCreateList("rule_hash", ST_Type);
   lListElem* global_host = host_list_locate(model.get_exec_host_list(), SGE_GLOBAL_NAME);

   view.report_started(oss);

   for_each_rw_lv(rqs, model.get_rqs_list()) {
      int rule_count = 1;

      DTRACE;

      if (!ocs::tom::get<RQS_enabled>(rqs)) {
         continue;
      }

      for_each_rw_lv(rule, ocs::tom::get<RQS_rule>(rqs)) {
         const lListElem *user_ep = lFirst(parameter.get_user_list());
         const lListElem *project_ep = lFirst(parameter.get_project_list());
         const lListElem *pe_ep = lFirst(parameter.get_pe_list());
         const lListElem *queue_ep = lFirst(parameter.get_cqueue_list());
         const lListElem *host_ep = lFirst(parameter.get_host_list());
         do {
            if (user_ep != nullptr) {
               DPRINTF("filter for user: %s\n", ocs::tom::get<ST_name>(user_ep));
               qquota_filter.user = ocs::tom::get<ST_name>(user_ep);
            }
            do {
               if (project_ep != nullptr) {
                  qquota_filter.project = ocs::tom::get<ST_name>(project_ep);
               }
               do {
                  if (pe_ep != nullptr) {
                     qquota_filter.pe = ocs::tom::get<ST_name>(pe_ep);
                  }
                  do {
                     if (queue_ep != nullptr) {
                        qquota_filter.queue = ocs::tom::get<ST_name>(queue_ep);
                     }
                     do {
                        if (host_ep != nullptr) {
                           qquota_filter.host = ocs::tom::get<ST_name>(host_ep);
                        }

                        if (rqs_is_matching_rule(rule, qquota_filter.user, nullptr, nullptr,
                                                 qquota_filter.project, qquota_filter.pe, qquota_filter.host,
                                                 qquota_filter.queue, model.get_user_set_list(),
                                                 model.get_hgroup_list())) {

                           for_each_ep_lv(limit, ocs::tom::get<RQR_limit>(rule)) {
                              const char *limit_name = ocs::tom::get<RQRL_name>(limit);
                              const lList *rue_list = ocs::tom::get<RQRL_usage>(limit);
                              lListElem *raw_centry = centry_list_locate(model.get_centry_list(), limit_name);

                              if (raw_centry == nullptr) {
                                 /* undefined centries can be ignored */
                                 DPRINTF("centry %s not defined -> IGNORING\n", limit_name);
                                 continue;
                              }
                              // attribute type drives value parsing (RQRL_value) and
                              // human-readable rendering of the limit (CS-2348)
                              const auto centry_type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(raw_centry));

                              if (lList *rml = parameter.get_resource_match_list(); rml != nullptr &&
                                  ((centry_list_locate(rml, limit_name) == nullptr) &&
                                  (centry_list_locate(rml, ocs::tom::get<CE_shortcut>(raw_centry)) == nullptr))) {
                                 DPRINTF("centry %s was not requested on CLI -> IGNORING\n", limit_name);
                                 continue;
                              }

                              // Set a rule name if it was not manually specified
                              if (!ocs::tom::get<RQR_name>(rule)) {
                                 ocs::tom::set<RQR_name>(rule, std::to_string(rule_count).c_str());
                              }

                              if (ocs::tom::get<CE_consumable>(raw_centry)) {
                                 /* for consumables we need to walk through the utilization and search for matching values */
                                 DPRINTF("found centry %s - consumable\n", limit_name);
                                 for_each_ep_lv(rue_elem, rue_list) {
                                    uint32_t dominant = 0;
                                    const char *rue_name = ocs::tom::get<RUE_name>(rue_elem);
                                    char *cp = nullptr;
                                    stringT user, project, pe, queue, host;
                                    dstring limit_str = DSTRING_INIT;
                                    dstring value_str = DSTRING_INIT;
                                    qquota_filter_t qf = { nullptr, nullptr, nullptr, nullptr, nullptr };

                                    /* check user name */
                                    cp = qquota_get_next_filter(user, rue_name);
                                    /* usergroups have the same beginning character @ as host groups */
                                    if (is_hgroup_name(qquota_filter.user)) {
                                       lListElem *ugroup = nullptr;

                                       if ((ugroup = ocs::tom::find_first_rw<US_name>(model.get_user_set_list(), &qquota_filter.user[1])) != nullptr) {
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
                                       const lList *hgroup_list = model.get_hgroup_list();
                                       if (lListElem *hgroup = hgroup_list_locate(hgroup_list, qquota_filter.host); hgroup != nullptr) {
                                          lList *host_list = nullptr;
                                          hgroup_find_all_references(hgroup, nullptr, hgroup_list, &host_list, nullptr);

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
                                          if (static_cast<const lListElem *>(ocs::tom::find_first<HR_name>(host_list, host)) == nullptr) {
                                             lFreeList(&host_list);
                                             continue;
                                          }
                                          lFreeList(&host_list);
                                       }
                                    } else {
                                       if ((strcmp(host, "-") != 0) && (strcmp(qquota_filter.host, "*") != 0) &&
                                           (fnmatch(qquota_filter.host, host, 0) != 0) ) {
                                          continue;
                                       }
                                    }
                                    uint64_t limit_value;
                                    if (ocs::tom::get<RQRL_dynamic>(limit)) {
                                       exec_host = host_list_locate(model.get_exec_host_list(), host);
                                       limit_value = static_cast<uint64_t>(scaled_mixed_load(ocs::tom::get<RQRL_value>(limit), global_host, exec_host, model.get_centry_list(),
                                                                                   model.get_job_load_adjustments()));
                                       sge_dstring_sprintf(&limit_str, "%d", limit_value);

                                    } else {
                                       ocs::tom::set<CE_pj_doubleval>(raw_centry, ocs::tom::get<RQRL_dvalue>(limit));
                                       sge_get_dominant_stringval(raw_centry, &dominant, &limit_str, nullptr, &limit_value);
                                    }

                                    u_int64_t value_value;
                                    ocs::tom::set<CE_pj_doubleval>(raw_centry, ocs::tom::get<RUE_utilized_now>(rue_elem));
                                    sge_get_dominant_stringval(raw_centry, &dominant, &value_str, nullptr, &value_value);

                                    qf.user = user;
                                    qf.project = project;
                                    qf.pe = pe;
                                    qf.queue = queue;
                                    qf.host = host;
                                    qquota_print_out_rule(oss, rqs, rule, limit_name, centry_type, value_value, limit_value, qf, printed_rules, view);

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
                                 if (const char *value_str = ocs::tom::get<RQRL_value>(limit);
                                     value_str != nullptr &&
                                     parse_ulong_val(&dval, nullptr, centry_type, value_str, nullptr, 0)) {
                                    limit_value = static_cast<uint64_t>(dval);
                                 }
                                 qquota_print_out_rule(oss, rqs, rule, limit_name,
                                                       centry_type, 0, limit_value, qf, printed_rules, view);

                              }
                           }
                        }
                     } while ((host_ep = lNext(host_ep)));
                  } while ((queue_ep = lNext(queue_ep)));
               } while ((pe_ep = lNext(pe_ep)));
            } while ((project_ep = lNext(project_ep)));
         } while ((user_ep = lNext(user_ep)));
         rule_count++;
      }
   }

   view.report_finished(oss);

   // show the full output
   view.show(out_, oss.str().c_str());

   DRETURN_VOID;
}
