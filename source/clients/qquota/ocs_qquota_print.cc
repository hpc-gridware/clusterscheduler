/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <climits>
#include <math.h>
#include <fnmatch.h>

#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"

#include "sched/sort_hosts.h"
#include "sched/sge_select_queue.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "basis_types.h"
#include "ocs_qquota_print.h"
#include "sge.h"

#include "msg_clients_common.h"

#define HEAD_FORMAT "%-18s %-20.20s %s\n"

typedef struct {
   const char* user;
   const char* project;
   const char* pe;
   const char* queue;
   const char* host;
} qquota_filter_t;

static bool get_all_lists(lList **rqs_l, lList **centry_l, lList **userset_l, lList **hgroup_l, lList **exechost_l, lList *hostref_list, lList **alpp);

static char *qquota_get_next_filter(stringT filter, const char *cp);
static bool qquota_print_out_rule(lListElem *rule, dstring rule_name, const char *limit_name,
                                  const char *usage_value, const char *limit_value, qquota_filter_t filter,
                                  lListElem *centry, report_handler_t* report_handler, lList *printed_rules, lList **alpp);

static bool qquota_print_out_filter(lListElem *filter, const char *name, const char *value, dstring *buffer, report_handler_t *report_handler, lList **alpp);

/****** qquota_output/qquota_output() ********************************************
*  NAME
*     qquota_output() -- qquota output function
*
*  SYNOPSIS
*     bool qquota_output(void *ctx, lList *host_list, lList *resource_match_list, 
*     lList *user_list, lList *pe_list, lList *project_list, lList 
*     *cqueue_list, lList **alpp, report_handler_t* report_handler) 
*
*  FUNCTION
*     print resource quota rule and the limit
*
*  INPUTS
*     void *ctx                        - gdi handler
*     lList *host_list                 - selected hosts
*     lList *resource_match_list       - selected resources
*     lList *user_list                 - selected users
*     lList *pe_list                   - selected pes
*     lList *project_list              - selected projects
*     lList *cqueue_list               - selected cluster queues
*     lList **alpp                     - answer list
*     report_handler_t* report_handler - report handler for xml output
*
*  RESULT
*     bool - true on success
*            false on error
*
*  NOTES
*     MT-NOTE: qquota_output() is MT safe 
*
*******************************************************************************/
bool qquota_output(lList *host_list, lList *resource_match_list, lList *user_list,
                 lList *pe_list, lList *project_list, lList *cqueue_list, lList **alpp,
                 report_handler_t* report_handler) 
{
   lList *rqs_list = nullptr;
   lList *centry_list = nullptr;
   lList *userset_list = nullptr;
   lList *hgroup_list = nullptr;
   lList *exechost_list = nullptr;

   lListElem* global_host = nullptr;
   lListElem* exec_host = nullptr;
   lList* printed_rules = nullptr;  /* Hash list of already printed resource quota rules (possible with -u user1,user2,user3...) */

   bool ret = true;
   int xml_ret = 0;

   qquota_filter_t qquota_filter = { "*",
                                     "*",
                                     "*",
                                     "*",
                                     "*" };

   dstring rule_name = DSTRING_INIT;

   DENTER(TOP_LAYER);

   /* If no user is requested on command line we set the current user as default */
   qquota_filter.user = component_get_username();

   ret = get_all_lists(&rqs_list, &centry_list, &userset_list, &hgroup_list, &exechost_list, host_list, alpp);

   if (ret) {
      const lListElem *rqs = nullptr;
      printed_rules = lCreateList("rule_hash", ST_Type); 
      global_host = host_list_locate(exechost_list, SGE_GLOBAL_NAME);

      if (report_handler != nullptr) {
         xml_ret = report_handler->report_started(report_handler, alpp);
         if (xml_ret != 0) {
            ret = false;
            goto qquota_output_error;
         }
      }

      for_each_ep(rqs, rqs_list) {
         lListElem *rule = nullptr;
         int rule_count = 1;

         if (!lGetBool(rqs, RQS_enabled)) {
            continue;
         }

         for_each_rw(rule, lGetList(rqs, RQS_rule)) {
            const lListElem *user_ep = lFirst(user_list);
            const lListElem *project_ep = lFirst(project_list);
            const lListElem *pe_ep = lFirst(pe_list);
            const lListElem *queue_ep = lFirst(cqueue_list);
            const lListElem *host_ep = lFirst(host_list);
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
                                                     qquota_filter.queue, userset_list, hgroup_list)) {
                              const lListElem *limit = nullptr;

                              for_each_ep(limit, lGetList(rule, RQR_limit)) {
                                 const char *limit_name = lGetString(limit, RQRL_name);
                                 const lList *rue_list = lGetList(limit, RQRL_usage);
                                 lListElem *raw_centry = centry_list_locate(centry_list, limit_name);
                                 const lListElem *rue_elem = nullptr;

                                 if (raw_centry == nullptr) {
                                    /* undefined centries can be ignored */
                                    DPRINTF("centry %s not defined -> IGNORING\n", limit_name);
                                    continue;
                                 }

                                 if ((resource_match_list != nullptr) &&
                                     ((centry_list_locate(resource_match_list, limit_name) == nullptr) &&
                                     (centry_list_locate(resource_match_list, lGetString(raw_centry, CE_shortcut)) == nullptr))) {
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

                                          if ((ugroup = lGetElemStrRW(userset_list, US_name, &qquota_filter.user[1])) != nullptr) {
                                             if (sge_contained_in_access_list(user, nullptr, nullptr, ugroup) == 0) {
                                                continue;
                                             }
                                          }
                                       } else {
                                          if ((strcmp(user, "-") != 0) && (strcmp(qquota_filter.user, "*") != 0)
                                               && (fnmatch(qquota_filter.user, user, 0) != 0)) {
                                             continue;
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
                                          lListElem *hgroup = nullptr;

                                          if ((hgroup = hgroup_list_locate(hgroup_list, qquota_filter.host)) != nullptr) {
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
                                       if (lGetBool(limit, RQRL_dynamic)) {
                                          exec_host = host_list_locate(exechost_list, host); 
                                          sge_dstring_sprintf(&limit_str, "%d", (int)scaled_mixed_load(lGetString(limit, RQRL_value),
                                                                                                       global_host, exec_host, centry_list));

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
                                       ret = qquota_print_out_rule(rule, rule_name, limit_name, 
                                                                   sge_dstring_get_string(&value_str), sge_dstring_get_string(&limit_str),
                                                                   qf, raw_centry, report_handler, printed_rules, alpp);

                                       sge_dstring_free(&limit_str);
                                       sge_dstring_free(&value_str);
                                    }
                                 } else {
                                    /* static values */
                                    qquota_filter_t qf = { nullptr, nullptr, nullptr, nullptr, nullptr };

                                    DPRINTF("found centry %s - static value\n", limit_name);
                                    ret = qquota_print_out_rule(rule, rule_name, limit_name, 
                                                                nullptr, lGetString(limit, RQRL_value),
                                                                qf, raw_centry, report_handler, printed_rules, alpp);

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

      if (report_handler != nullptr) {
         report_handler->report_finished(report_handler, alpp);
      }
   }

qquota_output_error:
   sge_dstring_free(&rule_name);
   lFreeList(&rqs_list);
   lFreeList(&centry_list);
   lFreeList(&userset_list);
   lFreeList(&hgroup_list);
   lFreeList(&exechost_list);
   lFreeList(&printed_rules);

   DRETURN(ret);
}

/****** qquota_output/get_all_lists() ******************************************
*  NAME
*     get_all_lists() -- get all lists from qmaster
*
*  SYNOPSIS
*     static bool get_all_lists(lList **rqs_l, lList
*     **centry_l, lList **userset_l, lList **hgroup_l, lList **exechost_l, 
*     lList *hostref_l, lList **alpp) 
*
*  FUNCTION
*     Gets copies of queue-, job-, complex-, exechost-list  from qmaster.
*      The lists are stored in the .._l pointerpointer-parameters.
*      WARNING: Lists previously stored in this pointers are not destroyed!!
*
*  INPUTS
*     void *context      - gdi context
*     lList **rqs_l     -  resource quota set list (RQS_Type)
*     lList **centry_l   - consumable resource list (CE_Type)
*     lList **userset_l  - userset list (US_Type)
*     lList **hgroup_l   - host group list (HG_Type)
*     lList **exechost_l - exechost list (EH_Type)
*     lList *hostref_l   - selected hosts (ST_Type)
*     lList **alpp       - answer list
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: get_all_lists() is MT safe 
*
*******************************************************************************/
static bool
get_all_lists(lList **rqs_l, lList **centry_l, lList **userset_l,
              lList **hgroup_l, lList **exechost_l, lList *hostref_l, lList **alpp)
{
   const lListElem *ep = nullptr;
   lEnumeration *what = nullptr;
   lCondition *where = nullptr, *nw = nullptr;
   int rqs_id, ce_id, userset_id, hgroup_id, eh_id;
   ocs::gdi::Request gdi_multi{};

   DENTER(TOP_LAYER);

   /*
   ** resource quota sets
   */
   what = lWhat("%T(ALL)", RQS_Type);
   rqs_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::TargetValue::SGE_RQS_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   /*
   ** complexes
   */
   what = lWhat("%T(ALL)", CE_Type);
   ce_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::TargetValue::SGE_CE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   /*
   ** usersets
   */
   what = lWhat("%T(ALL)", US_Type);
   userset_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   /*
   ** host groups
   */
   what = lWhat("%T(ALL)", HGRP_Type);
   hgroup_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::SGE_HGRP_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   /*
   ** exec hosts
   */
   for_each_ep(ep, hostref_l) {
      nw = lWhere("%T(%I h= %s)", EH_Type, EH_name, lGetString(ep, ST_name));
      if (!where)
         where = nw;
      else
         where = lOrWhere(where, nw);
   }
   /* the global host has to be retrieved as well */
   if (where != nullptr) {
      nw = lWhere("%T(%I == %s)", EH_Type, EH_name, SGE_GLOBAL_NAME);
      where = lOrWhere(where, nw);
   }

   nw = lWhere("%T(%I != %s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   if (where)
      where = lAndWhere(where, nw);
   else
      where = nw;

   what = lWhat("%T(%I %I %I %I)", EH_Type, EH_name, EH_load_list, EH_consumable_config_list, EH_resource_utilization);
   eh_id = gdi_multi.request(alpp, ocs::Mode::SEND, ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
   gdi_multi.wait();
   lFreeWhat(&what);
   lFreeWhere(&where);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   /* --- resource quota sets */
   lFreeList(alpp);
   gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_RQS_LIST, rqs_id, rqs_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   /* --- complex attribute */
   lFreeList(alpp);
   gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CE_LIST, ce_id, centry_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   /* --- usersets */
   lFreeList(alpp);
   gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_US_LIST, userset_id, userset_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   /* --- hostgroups */
   lFreeList(alpp);
   gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_HGRP_LIST, hgroup_id, hgroup_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   /* --- exec hosts*/
   lFreeList(alpp);
   gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_EH_LIST, eh_id, exechost_l);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   DRETURN(true);
}

/****** qquota_output/qquota_get_next_filter() *********************************
*  NAME
*     qquota_get_next_filter() -- tokenize rue_name of usage
*
*  SYNOPSIS
*     static char* qquota_get_next_filter(char *filter, const char *cp) 
*
*  FUNCTION
*     The rue_name has the type /user_name/project_name/pe_name/queue_name/host_name.
*     This function tokenizes the rue_name and gives always one element back
*
*  INPUTS
*     char *filter   - store for the token 
*     const char *cp - pointer to rue_name
*
*  RESULT
*     static char* - pointer for the next token
*
*  NOTES
*     MT-NOTE: qquota_get_next_filter() is not MT safe 
*
*******************************************************************************/
static char *qquota_get_next_filter(stringT filter, const char *cp)
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

/****** qquota_output/qquota_print_out_rule() **********************************
*  NAME
*     qquota_print_out_rule() -- print out rule
*
*  SYNOPSIS
*     static bool qquota_print_out_rule(lListElem *rule, dstring rule_name, 
*     const char *limit_name, const char *usage_value, const char *limit_value, 
*     qquota_filter_t qfilter, report_handler_t* report_handler, lList **alpp)
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lListElem *rule                  - resource quota rule (RQR_Type)
*     dstring rule_name                - rule name (eg. ruleset1/rule1)
*     const char *limit_name           - limitation name (eg. slots)
*     const char *usage_value          - debited usage
*     const char *limit_value          - configured limitation
*     qquota_filter_t qfilter          - filter tuple
*     lListElem *centry                - limitation centry element
*     report_handler_t* report_handler - handler for xml output
*     lList **alpp                     - answer list
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: qquota_print_out_rule() is not MT safe 
*
*******************************************************************************/
static bool qquota_print_out_rule(lListElem *rule, dstring rule_name, const char *limit_name,
                                  const char *usage_value, const char *limit_value, qquota_filter_t qfilter,
                                  lListElem *centry, report_handler_t* report_handler, lList *printed_rules, lList **alpp) 
{
   static bool printheader = true;
   bool ret = true;
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
      return ret;
   }

   lAddElemStr(&printed_rules, ST_name, sge_dstring_get_string(&token), ST_Type);

   if (report_handler != nullptr) {
      report_handler->report_limit_rule_begin(report_handler, sge_dstring_get_string(&rule_name), alpp);
   } else {
      if (printheader) {
         printheader = false;
         printf(HEAD_FORMAT, MSG_HEADER_RULE, MSG_HEADER_LIMIT, MSG_HEADER_FILTER);
         printf("--------------------------------------------------------------------------------\n");
      }
   }

   qquota_print_out_filter(lGetObject(rule, RQR_filter_users), "users", qfilter.user, &filter_str, report_handler, alpp);
   qquota_print_out_filter(lGetObject(rule, RQR_filter_projects), "projects", qfilter.project, &filter_str, report_handler, alpp);
   qquota_print_out_filter(lGetObject(rule, RQR_filter_pes), "pes", qfilter.pe, &filter_str, report_handler, alpp);
   qquota_print_out_filter(lGetObject(rule, RQR_filter_queues), "queues", qfilter.queue, &filter_str, report_handler, alpp);
   qquota_print_out_filter(lGetObject(rule, RQR_filter_hosts), "hosts", qfilter.host, &filter_str, report_handler, alpp);

   if (report_handler != nullptr) {
      report_handler->report_resource_value(report_handler, limit_name,
                                            limit_value,
                                            usage_value,
                                            alpp);
      report_handler->report_limit_rule_finished(report_handler, sge_dstring_get_string(&rule_name), alpp);
   } else {
      if (usage_value == nullptr) {
         sge_dstring_sprintf(&limitation, "%s=%s", limit_name, limit_value);
      } else {
         sge_dstring_sprintf(&limitation, "%s=%s/%s", limit_name, usage_value, limit_value);
      }
      if (sge_dstring_strlen(&filter_str) == 0) {
         sge_dstring_append(&filter_str, "-");
      }
      printf(HEAD_FORMAT, sge_dstring_get_string(&rule_name), sge_dstring_get_string(&limitation), sge_dstring_get_string(&filter_str));
   }

   sge_dstring_free(&token);
   sge_dstring_free(&filter_str);
   sge_dstring_free(&limitation);
   return ret;
}

/****** qquota_output/qquota_print_out_filter() ********************************
*  NAME
*     qquota_print_out_filter() -- prints out filter element
*
*  SYNOPSIS
*     static bool qquota_print_out_filter(lListElem *filter, const char *name, 
*     const char *value, dstring *buffer, report_handler_t *report_handler, 
*     lList **alpp) 
*
*  FUNCTION
*     this function prints out the filter configured in the rule
*
*  INPUTS
*     lListElem *filter                - filter element (RQRF_Type)
*     const char *name                 - filter type name
*     const char *value                - filter value
*     dstring *buffer                  - buffer
*     report_handler_t *report_handler - handler for report handler
*     lList **alpp                     - answer list
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: qquota_print_out_filter() is MT safe 
*
*******************************************************************************/
static bool
qquota_print_out_filter(lListElem *filter, const char *name, const char *value,
                        dstring *buffer, report_handler_t *report_handler, lList **alpp) 
{
   bool ret = true;
   const lListElem *scope;
   
   if (filter != nullptr) {
      if (!lGetBool(filter, RQRF_expand) || value == nullptr) {
         if (report_handler != nullptr) {
            for_each_ep(scope, lGetList(filter, RQRF_scope)) {
               report_handler->report_limit_string_value(report_handler, name, lGetString(scope, ST_name), false, alpp);
            }
            for_each_ep(scope, lGetList(filter, RQRF_xscope)) {
               report_handler->report_limit_string_value(report_handler, name, lGetString(scope, ST_name), true, alpp);
            }
         } else {
            if (sge_dstring_strlen(buffer) != 0) {
               sge_dstring_append(buffer, " ");
            }
            sge_dstring_append(buffer, name);
            sge_dstring_append(buffer, " ");
            rqs_append_filter_to_dstring(filter, buffer, alpp);
         }
      } else {
        if (report_handler != nullptr) {
          report_handler->report_limit_string_value(report_handler, name, value, false, alpp);
        } else {
            if (sge_dstring_strlen(buffer) != 0) {
               sge_dstring_append(buffer, " ");
            }
          sge_dstring_append(buffer, name);
          sge_dstring_append(buffer, " ");
          sge_dstring_append(buffer, value);
        }
      }
   }

   return ret;
}
