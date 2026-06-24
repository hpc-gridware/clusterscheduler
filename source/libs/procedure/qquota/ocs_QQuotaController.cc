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

#include "sched/sort_hosts.h"

#include "ocs_QQuotaController.h"

#include <iostream>


ocs::QQuotaController::~QQuotaController() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

char *
ocs::QQuotaController::qquota_get_next_filter(stringT filter, const char *cp)
{
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
   if (!lGetBool(filter, RQRF_expand) || value == nullptr) {
      for_each_ep_lv(scope, lGetList(filter, RQRF_scope)) {
         view.report_limit_string_value(os, name, lGetString(scope, ST_name), false);
      }
      for_each_ep_lv(scope, lGetList(filter, RQRF_xscope)) {
         view.report_limit_string_value(os, name, lGetString(scope, ST_name), true);
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
   oss_key << lGetString(rqs, RQS_name) << "/" << lGetString(rule, RQR_name) << "," << limit_name << ","
           << (qfilter.user? qfilter.user: "") << "," << (qfilter.project? qfilter.project: "") << ","
           << (qfilter.pe? qfilter.pe: "") << "," << (qfilter.queue? qfilter.queue: "") << ","
           << (qfilter.host? qfilter.host: "");

   // check if output for the key was already done
   if (lGetElemStr(printed_rules, ST_name, oss_key.str().c_str()) != nullptr) {
      return;
   }

   // remember the key to avoid duplicate output
   lAddElemStr(&printed_rules, ST_name, oss_key.str().c_str(), ST_Type);

   // make the output
   view.report_limit_rule_begin(os, lGetString(rqs, RQS_name), lGetString(rule, RQR_name));
   view.report_resource_value(os, limit_name, type, limit, usage);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_users), "users", qfilter.user, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_projects), "projects", qfilter.project, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_pes), "pes", qfilter.pe, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_queues), "queues", qfilter.queue, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_hosts), "hosts", qfilter.host, view);
   view.report_limit_rule_finished(os);
}

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

      if (!lGetBool(rqs, RQS_enabled)) {
         continue;
      }

      for_each_rw_lv(rule, lGetList(rqs, RQS_rule)) {
         const lListElem *user_ep = lFirst(parameter.get_user_list());
         const lListElem *project_ep = lFirst(parameter.get_project_list());
         const lListElem *pe_ep = lFirst(parameter.get_pe_list());
         const lListElem *queue_ep = lFirst(parameter.get_cqueue_list());
         const lListElem *host_ep = lFirst(parameter.get_host_list());
         do {
            if (user_ep != nullptr) {
               DPRINTF("filter for user: %s\n", lGetString(user_ep, ST_name));
               qquota_filter.user = lGetString(user_ep, ST_name);
            }
            do {
               if (project_ep != nullptr) {
                  qquota_filter.project = lGetString(project_ep, ST_name);
               }
               do {
                  if (pe_ep != nullptr) {
                     qquota_filter.pe = lGetString(pe_ep, ST_name);
                  }
                  do {
                     if (queue_ep != nullptr) {
                        qquota_filter.queue = lGetString(queue_ep, ST_name);
                     }
                     do {
                        if (host_ep != nullptr) {
                           qquota_filter.host = lGetString(host_ep, ST_name);
                        }

                        if (rqs_is_matching_rule(rule, qquota_filter.user, nullptr, nullptr,
                                                 qquota_filter.project, qquota_filter.pe, qquota_filter.host,
                                                 qquota_filter.queue, model.get_user_set_list(),
                                                 model.get_hgroup_list())) {

                           for_each_ep_lv(limit, lGetList(rule, RQR_limit)) {
                              const char *limit_name = lGetString(limit, RQRL_name);
                              const lList *rue_list = lGetList(limit, RQRL_usage);
                              lListElem *raw_centry = centry_list_locate(model.get_centry_list(), limit_name);

                              if (raw_centry == nullptr) {
                                 /* undefined centries can be ignored */
                                 DPRINTF("centry %s not defined -> IGNORING\n", limit_name);
                                 continue;
                              }
                              // attribute type drives value parsing (RQRL_value) and
                              // human-readable rendering of the limit (CS-2348)
                              const auto centry_type = static_cast<ocs::CEntry::Type>(lGetUlong(raw_centry, CE_valtype));

                              if (lList *rml = parameter.get_resource_match_list(); rml != nullptr &&
                                  ((centry_list_locate(rml, limit_name) == nullptr) &&
                                  (centry_list_locate(rml, lGetString(raw_centry, CE_shortcut)) == nullptr))) {
                                 DPRINTF("centry %s was not requested on CLI -> IGNORING\n", limit_name);
                                 continue;
                              }

                              // Set a rule name if it was not manually specified
                              if (!lGetString(rule, RQR_name)) {
                                 lSetString(rule, RQR_name, std::to_string(rule_count).c_str());
                              }

                              if (lGetUlong(raw_centry, CE_consumable)) {
                                 /* for consumables we need to walk through the utilization and search for matching values */
                                 DPRINTF("found centry %s - consumable\n", limit_name);
                                 for_each_ep_lv(rue_elem, rue_list) {
                                    uint32_t dominant = 0;
                                    const char *rue_name = lGetString(rue_elem, RUE_name);
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

                                       if ((ugroup = lGetElemStrRW(model.get_user_set_list(), US_name, &qquota_filter.user[1])) != nullptr) {
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
                                          if (host_list == nullptr && lGetElemHost(host_list, HR_name, host) == nullptr) {
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
                                    if (lGetBool(limit, RQRL_dynamic)) {
                                       exec_host = host_list_locate(model.get_exec_host_list(), host);
                                       limit_value = static_cast<uint64_t>(scaled_mixed_load(lGetString(limit, RQRL_value), global_host, exec_host, model.get_centry_list()));
                                       sge_dstring_sprintf(&limit_str, "%d", limit_value);

                                    } else {
                                       lSetDouble(raw_centry, CE_pj_doubleval, lGetDouble(limit, RQRL_dvalue));
                                       sge_get_dominant_stringval(raw_centry, &dominant, &limit_str, nullptr, &limit_value);
                                    }

                                    u_int64_t value_value;
                                    lSetDouble(raw_centry,CE_pj_doubleval, lGetDouble(rue_elem, RUE_utilized_now));
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
                                 if (const char *value_str = lGetString(limit, RQRL_value);
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
