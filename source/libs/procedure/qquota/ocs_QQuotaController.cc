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


ocs::QQuotaController::QQuotaController() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

ocs::QQuotaController::~QQuotaController() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

char *
ocs::QQuotaController::qquota_get_next_filter(stringT filter, const char *cp)
{
   auto *ret = (char *)strchr(cp, '/');
   ret++;
   if (ret - cp < MAX_STRING_SIZE && ret - cp > 1) {
      snprintf(filter, ret - cp, "%s", cp);
   } else {
      snprintf(filter, MAX_STRING_SIZE, "-");
   }

   return ret;
}

void
ocs::QQuotaController::qquota_print_out_filter(std::ostream &os, lListElem *filter, const char *name, const char *value, dstring *buffer, QQuotaViewBase &view)
{
   DENTER(TOP_LAYER);
   const lListElem *scope;

   if (filter != nullptr) {
      if (!lGetBool(filter, RQRF_expand) || value == nullptr) {
#if 0
         if (report_handler != nullptr) {
#endif
            for_each_ep(scope, lGetList(filter, RQRF_scope)) {
               view.report_limit_string_value(os, name, lGetString(scope, ST_name), false);
            }
            for_each_ep(scope, lGetList(filter, RQRF_xscope)) {
               view.report_limit_string_value(os, name, lGetString(scope, ST_name), true);
            }
#if 0
         } else {
            if (sge_dstring_strlen(buffer) != 0) {
               sge_dstring_append(buffer, " ");
            }
            sge_dstring_append(buffer, name);
            sge_dstring_append(buffer, " ");
            rqs_append_filter_to_dstring(filter, buffer, nullptr);
         }
#endif
      } else {
#if 0
         if (report_handler != nullptr) {
#endif
            view.report_limit_string_value(os, name, value, false);
#if 0
         } else {
            if (sge_dstring_strlen(buffer) != 0) {
               sge_dstring_append(buffer, " ");
            }
            sge_dstring_append(buffer, name);
            sge_dstring_append(buffer, " ");
            sge_dstring_append(buffer, value);
         }
#endif
      }
   }
   DRETURN_VOID;
}

void
ocs::QQuotaController::qquota_print_out_rule(std::ostream &os, lListElem *rule, dstring rule_name, const char *limit_name,
                                  const char *usage_value, const char *limit_value, qquota_filter_t qfilter,
                                  lList *printed_rules, QQuotaViewBase &view)
{
   dstring filter_str = DSTRING_INIT;
   dstring limitation = DSTRING_INIT;
   dstring token = DSTRING_INIT;

   sge_dstring_sprintf(&token, "%s,%s,%s,%s,%s,%s,%s", sge_dstring_get_string(&rule_name),
                                                             limit_name,
                                                             qfilter.user? qfilter.user: "",
                                                             qfilter.project? qfilter.project: "",
                                                             qfilter.pe? qfilter.pe: "",
                                                             qfilter.queue? qfilter.queue: "",
                                                             qfilter.host? qfilter.host: "");

   if (lGetElemStr(printed_rules, ST_name, sge_dstring_get_string(&token)) != nullptr) {
      sge_dstring_free(&token);
      sge_dstring_free(&filter_str);
      sge_dstring_free(&limitation);
      return;
   }

   lAddElemStr(&printed_rules, ST_name, sge_dstring_get_string(&token), ST_Type);

   view.report_limit_rule_begin(os, sge_dstring_get_string(&rule_name));

   view.report_resource_value(os, limit_name, limit_value, usage_value);

   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_users), "users", qfilter.user, &filter_str, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_projects), "projects", qfilter.project, &filter_str, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_pes), "pes", qfilter.pe, &filter_str, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_queues), "queues", qfilter.queue, &filter_str, view);
   qquota_print_out_filter(os, lGetObject(rule, RQR_filter_hosts), "hosts", qfilter.host, &filter_str, view);

   view.report_limit_rule_finished(os, sge_dstring_get_string(&rule_name));
#if 0
      if (usage_value == nullptr) {
         sge_dstring_sprintf(&limitation, "%s=%s", limit_name, limit_value);
      } else {
         sge_dstring_sprintf(&limitation, "%s=%s/%s", limit_name, usage_value, limit_value);
      }
      if (sge_dstring_strlen(&filter_str) == 0) {
         sge_dstring_append(&filter_str, "-");
      }
      printf("XXX %-18s %-20.20s %s\n", sge_dstring_get_string(&rule_name), sge_dstring_get_string(&limitation), sge_dstring_get_string(&filter_str));
#endif

   sge_dstring_free(&token);
   sge_dstring_free(&filter_str);
   sge_dstring_free(&limitation);
   return;
}

void ocs::QQuotaController::process_request(QQuotaParameter &parameter, QQuotaModel &model, QQuotaViewBase &view) {
   DENTER(TOP_LAYER);

   lListElem* exec_host = nullptr;
   std::ostringstream oss;

   qquota_filter_t qquota_filter = { "*", "*", "*", "*", "*" };
   dstring rule_name = DSTRING_INIT;

   /* If no user is requested on command line we set the current user as default */
   qquota_filter.user = component_get_username();


   /* Hash list of already printed resource quota rules (possible with -u user1,user2,user3...) */
   lList* printed_rules = lCreateList("rule_hash", ST_Type);
   lListElem* global_host = host_list_locate(model.exechost_list, SGE_GLOBAL_NAME);

   view.report_started(oss);

   const lListElem *rqs = nullptr;
   for_each_ep(rqs, model.rqs_list) {
      int rule_count = 1;

      if (!lGetBool(rqs, RQS_enabled)) {
         continue;
      }

      lListElem *rule = nullptr;
      for_each_rw(rule, lGetList(rqs, RQS_rule)) {
         const lListElem *user_ep = lFirst(parameter.user_list);
         const lListElem *project_ep = lFirst(parameter.project_list);
         const lListElem *pe_ep = lFirst(parameter.pe_list);
         const lListElem *queue_ep = lFirst(parameter.cqueue_list);
         const lListElem *host_ep = lFirst(parameter.host_list);
         do {
            if (user_ep != nullptr) {
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

                        if (rqs_is_matching_rule(rule, qquota_filter.user, nullptr, nullptr ,qquota_filter.project,
                                                 qquota_filter.pe, qquota_filter.host,
                                                  qquota_filter.queue, model.userset_list, model.hgroup_list)) {
                           const lListElem *limit = nullptr;

                           for_each_ep(limit, lGetList(rule, RQR_limit)) {
                              const char *limit_name = lGetString(limit, RQRL_name);
                              const lList *rue_list = lGetList(limit, RQRL_usage);
                              lListElem *raw_centry = centry_list_locate(model.centry_list, limit_name);
                              const lListElem *rue_elem = nullptr;

                              if (raw_centry == nullptr) {
                                 /* undefined centries can be ignored */
                                 DPRINTF("centry %s not defined -> IGNORING\n", limit_name);
                                 continue;
                              }

                              if ((parameter.resource_match_list != nullptr) &&
                                  ((centry_list_locate(parameter.resource_match_list, limit_name) == nullptr) &&
                                  (centry_list_locate(parameter.resource_match_list, lGetString(raw_centry, CE_shortcut)) == nullptr))) {
                                 DPRINTF("centry %s was not requested on CLI -> IGNORING\n", limit_name);
                                 continue;
                              }

                              if (lGetString(rule, RQR_name)) {
                                 sge_dstring_sprintf(&rule_name, "%s/%s", lGetString(rqs, RQS_name), lGetString(rule, RQR_name));
                              } else {
                                 sge_dstring_sprintf(&rule_name, "%s/%d", lGetString(rqs, RQS_name), rule_count);
                              }

                              if (lGetUlong(raw_centry, CE_consumable)) {
                                 /* for consumables we need to walk through the utilization and search for matching values */
                                 DPRINTF("found centry %s - consumable\n", limit_name);
                                 for_each_ep(rue_elem, rue_list) {
                                    u_long32 dominant = 0;
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

                                       if ((ugroup = lGetElemStrRW(model.userset_list, US_name, &qquota_filter.user[1])) != nullptr) {
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
                                    if (ocs::is_hgroup_name(qquota_filter.host)) {
                                       lListElem *hgroup = nullptr;

                                       if ((hgroup = hgroup_list_locate(model.hgroup_list, qquota_filter.host)) != nullptr) {
                                          lList *host_list = nullptr;
                                          hgroup_find_all_references(hgroup, nullptr, model.hgroup_list, &host_list, nullptr);
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
                                    if (lGetBool(limit, RQRL_dynamic)) {
                                       exec_host = host_list_locate(model.exechost_list, host);
                                       sge_dstring_sprintf(&limit_str, "%d", (int)scaled_mixed_load(lGetString(limit, RQRL_value),
                                                                                                    global_host, exec_host, model.centry_list));

                                    } else {
                                       lSetDouble(raw_centry, CE_pj_doubleval, lGetDouble(limit, RQRL_dvalue));
                                       sge_get_dominant_stringval(raw_centry, &dominant, &limit_str);
                                    }

                                    lSetDouble(raw_centry,CE_pj_doubleval, lGetDouble(rue_elem, RUE_utilized_now));
                                    sge_get_dominant_stringval(raw_centry, &dominant, &value_str);

                                    qf.user = user;
                                    qf.project = project;
                                    qf.pe = pe;
                                    qf.queue = queue;
                                    qf.host = host;
                                    qquota_print_out_rule(oss, rule, rule_name, limit_name,
                                                          sge_dstring_get_string(&value_str), sge_dstring_get_string(&limit_str),
                                                          qf, printed_rules, view);

                                    sge_dstring_free(&limit_str);
                                    sge_dstring_free(&value_str);
                                 }
                              } else {
                                 /* static values */
                                 qquota_filter_t qf = { nullptr, nullptr, nullptr, nullptr, nullptr };

                                 DPRINTF("found centry %s - static value\n", limit_name);
                                 qquota_print_out_rule(oss, rule, rule_name, limit_name,
                                                       nullptr, lGetString(limit, RQRL_value), qf, printed_rules, view);

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

   std::cout << oss.str();

   DRETURN_VOID;
}
