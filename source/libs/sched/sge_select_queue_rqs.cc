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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <climits>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_parse_num_par.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_resource_quota_service.h"
#include "sgeobj/sge_resource_utilization.h"

#include "sched/schedd_message.h"
#include "sched/msg_schedd.h"

#include "sge_complex_schedd.h"
#include "sge_schedd_text.h"
#include "sge_select_queue.h"
#include "sge_select_queue_rqs.h"
#include "sort_hosts.h"

/****** sge_resource_quota_schedd/rqs_limitation_reached() *********************
*  NAME
*     rqs_limitation_reached() -- is the limitation reached for a queue instance
*
*  SYNOPSIS
*     static bool rqs_limitation_reached(sge_assignment_t *a, lListElem *rule,
*     const char* host, const char* queue)
*
*  FUNCTION
*     The function verifies no limitation is reached for the specific job request
*     and queue instance
*
*  INPUTS
*     sge_assignment_t *a    - job info structure
*     const lListElem *rule        - rqsource quota rule (RQR_Type)
*     const char* host       - host name
*     const char* queue      - queue name
*    u_long64 *start         - start time of job
*
*  RESULT
*     static dispatch_t - DISPATCH_OK job can be scheduled
*                         DISPATCH_NEVER_CAT no jobs of this category will be scheduled
*                         DISPATCH_NOT_AT_TIME job can be scheduled later
*                         DISPATCH_MISSING_ATTR rule does not match requested attributes
*
*  NOTES
*     MT-NOTE: rqs_limitation_reached() is not MT safe
*
*******************************************************************************/
static dispatch_t rqs_limitation_reached(sge_assignment_t *a, const lListElem *rule, const char* host, const char* queue, u_long64 *start)
{
   dispatch_t ret = DISPATCH_MISSING_ATTR;
   const lList *limit_list = nullptr;
   lListElem * limit = nullptr;
   static lListElem *implicit_slots_request = nullptr;
   lListElem *exec_host = host_list_locate(a->host_list, host);
   dstring rue_name = DSTRING_INIT;
   dstring reason = DSTRING_INIT;

   DENTER(TOP_LAYER);

    if (implicit_slots_request == nullptr) {
      implicit_slots_request = lCreateElem(CE_Type);
      lSetString(implicit_slots_request, CE_name, SGE_ATTR_SLOTS);
      lSetString(implicit_slots_request, CE_stringval, "1");
      lSetDouble(implicit_slots_request, CE_doubleval, 1);
   }

   limit_list = lGetList(rule, RQR_limit);
   for_each_rw(limit, limit_list) {
      bool       is_forced = false;
      const char *limit_name = lGetString(limit, RQRL_name);
      lListElem  *raw_centry = centry_list_locate(a->centry_list, limit_name);

      if (raw_centry == nullptr) {
         DPRINTF("ignoring limit %s because not defined", limit_name);
         continue;
      } else {
         DPRINTF("checking limit %s\n", lGetString(raw_centry, CE_name));
      }

      is_forced = lGetUlong(raw_centry, CE_requestable) == REQU_FORCED;
      lList *job_centry_list = job_get_hard_resource_listRW(a->job);
      // @todo CS-400: we only need job_centry. Have a function searching it in the 3 possible request lists
      lListElem *job_centry = centry_list_locate(job_centry_list, limit_name);

      /* check for implicit slot and default request */
      if (job_centry == nullptr) {
         if (strcmp(lGetString(raw_centry, CE_name), SGE_ATTR_SLOTS) == 0) {
            job_centry = implicit_slots_request;
         } else if (lGetString(raw_centry, CE_defaultval) != nullptr && lGetUlong(raw_centry, CE_consumable)) {
            double request;
            parse_ulong_val(&request, nullptr, lGetUlong(raw_centry, CE_valtype), lGetString(raw_centry, CE_defaultval), nullptr, 0);

            /* default requests with zero value are ignored */
            if (request == 0.0 && lGetUlong(raw_centry, CE_relop) != CMPLXEXCL_OP) {
               continue;
            }
            lSetString(raw_centry, CE_stringval, lGetString(raw_centry, CE_defaultval));
            lSetDouble(raw_centry, CE_doubleval, request);
            job_centry = raw_centry;
            DPRINTF("using default request for %s!\n", lGetString(raw_centry, CE_name));
         } else if (is_forced) {
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                           SCHEDD_INFO_NOTREQFORCEDRES);
            ret = DISPATCH_NEVER_CAT;
            break;
         } else {
            /* ignoring because centry was not requested and is no consumable */
            DPRINTF("complex not requested!\n");
            continue;
         }
      }

      {
         lList *tmp_centry_list = lCreateList("", CE_Type);
         lList *tmp_rue_list = lCreateList("", RUE_Type);
         lListElem *tmp_centry_elem = nullptr;
         lListElem *tmp_rue_elem = nullptr;

         if (rqs_set_dynamical_limit(limit, a->gep, exec_host, a->centry_list)) {
            const lList *rue_list = lGetList(limit, RQRL_usage);
            u_long64 tmp_time = a->start;

            /* create tmp_centry_list */
            tmp_centry_elem = lCopyElem(raw_centry);
            lSetString(tmp_centry_elem, CE_stringval, lGetString(limit, RQRL_value));
            lSetDouble(tmp_centry_elem, CE_doubleval, lGetDouble(limit, RQRL_dvalue));
            lAppendElem(tmp_centry_list, tmp_centry_elem);

            /* create tmp_rue_list */
            rqs_get_rue_string(&rue_name, rule, a->user, a->project, host, queue, nullptr);
            tmp_rue_elem = lCopyElem(lGetElemStr(rue_list, RUE_name, sge_dstring_get_string(&rue_name)));
            if (tmp_rue_elem == nullptr) {
               tmp_rue_elem = lCreateElem(RUE_Type);
            }
            lSetString(tmp_rue_elem, RUE_name, limit_name);
            lAppendElem(tmp_rue_list, tmp_rue_elem);

            sge_dstring_clear(&reason);
            ret = ri_time_by_slots(a, job_centry, nullptr, tmp_centry_list,  tmp_rue_list,
                                   nullptr, nullptr, &reason, false, 1,
                                   DOMINANT_LAYER_RQS, 0.0, &tmp_time, SGE_RQS_NAME, nullptr);
            if (ret != DISPATCH_OK) {
               DPRINTF("denied because: %s\n", sge_dstring_get_string(&reason));
               lFreeList(&tmp_rue_list);
               lFreeList(&tmp_centry_list);
               break;
            }

            if (a->is_reservation && ret == DISPATCH_OK) {
               *start = tmp_time;
            }

            lFreeList(&tmp_rue_list);
            lFreeList(&tmp_centry_list);
         }
      }
   }

   sge_dstring_free(&reason);
   sge_dstring_free(&rue_name);

   DRETURN(ret);
}


/****** sge_resource_quota_schedd/rqs_exceeded_sort_out() **********************
*  NAME
*     rqs_exceeded_sort_out() -- Rule out queues/hosts whenever possible
*
*  SYNOPSIS
*     bool rqs_exceeded_sort_out(sge_assignment_t *a, const lListElem *rule,
*     const dstring *rule_name, const char* queue_name, const char* host_name)
*
*  FUNCTION
*     This function tries to rule out hosts and cluster queues after a
*     quota exeeding was found for a limitation rule with specific queue
*     instance.
*
*     When a limitation was exeeded that applies to the entire
*     cluster 'true' is returned, 'false' otherwise.
*
*  INPUTS
*     sge_assignment_t *a      - Scheduler assignment type
*     const lListElem *rule    - The exeeded rule
*     const dstring *rule_name - Name of the rule (monitoring only)
*     const char* queue_name   - Cluster queue name
*     const char* host_name    - Host name
*
*  RESULT
*     bool - True upon global limits exceeding
*
*  NOTES
*     MT-NOTE: rqs_exceeded_sort_out() is MT safe
*******************************************************************************/
static bool rqs_exceeded_sort_out(sge_assignment_t *a, const lListElem *rule, const dstring *rule_name,
   const char* queue_name, const char* host_name)
{
   bool cq_global = is_cqueue_global(rule);
   bool eh_global = is_host_global(rule);

   DENTER(TOP_LAYER);

   if ((!cq_global && !eh_global) || (cq_global && eh_global &&
         (is_cqueue_expand(rule) || is_host_expand(rule)))) { /* failure at queue instance limit */
      DPRINTF("QUEUE INSTANCE: resource quota set %s deny job execution on %s@%s\n",
              sge_dstring_get_string(rule_name), queue_name, host_name);
      DRETURN(false);
   }

   if (cq_global && eh_global) { /* failure at a global limit */
      bool host_shadowed, queue_shadowed;

      rqs_can_optimize(rule, &host_shadowed, &queue_shadowed, a);
      if (!host_shadowed && !queue_shadowed) {
         DPRINTF("GLOBAL: resource quota set %s deny job execution globally\n", sge_dstring_get_string(rule_name));
         DRETURN(true);
      }

      if (host_shadowed && queue_shadowed) {
         rqs_excluded_cqueues(rule, a);
         rqs_excluded_hosts(rule, a);
         DPRINTF("QUEUE INSTANCE: resource quota set %s deny job execution on %s@%s\n", sge_dstring_get_string(rule_name), queue_name, host_name);
         DRETURN(false);
      }

      if (queue_shadowed) {
         rqs_excluded_cqueues(rule, a);
         DPRINTF("QUEUE: resource quota set %s deny job execution in all its queues\n", sge_dstring_get_string(rule_name));
      } else { /* must be host_shadowed */
         rqs_excluded_hosts(rule, a);
         DPRINTF("HOST: resource quota set %s deny job execution in all its queues\n", sge_dstring_get_string(rule_name));
      }

      DRETURN(false);
   }

   if (!cq_global) { /* failure at a cluster queue limit */

      if (host_shadowed(rule, a)) {
         DPRINTF("QUEUE INSTANCE: resource quota set %s deny job execution on %s@%s\n", sge_dstring_get_string(rule_name), queue_name, host_name);
         DRETURN(false);
      }

      if (lGetBool(lGetObject(rule, RQR_filter_queues), RQRF_expand)) {
         lAddElemStr(&(a->skip_cqueue_list), CTI_name, queue_name, CTI_Type);
         DPRINTF("QUEUE: resource quota set %s deny job execution in queue %s\n", sge_dstring_get_string(rule_name), queue_name);
      } else {
         rqs_expand_cqueues(rule, a);
         DPRINTF("QUEUE: resource quota set %s deny job execution in all its queues\n", sge_dstring_get_string(rule_name));
      }

      DRETURN(false);
   }

   /* must be (!eh_global) */
   { /* failure at a host limit */

      if (cqueue_shadowed(rule, a)) {
         DPRINTF("QUEUE INSTANCE: resource quota set %s deny job execution on %s@%s\n", sge_dstring_get_string(rule_name), queue_name, host_name);
         DRETURN(false);
      }

      if (lGetBool(lGetObject(rule, RQR_filter_hosts), RQRF_expand)) {
         lAddElemStr(&(a->skip_host_list), CTI_name, host_name, CTI_Type);
         DPRINTF("HOST: resource quota set %s deny job execution at host %s\n", sge_dstring_get_string(rule_name), host_name);
      } else {
         rqs_expand_hosts(rule, a);
         DPRINTF("HOST: resource quota set %s deny job execution at all its hosts\n", sge_dstring_get_string(rule_name));
      }

      DRETURN(false);
   }
}

/****** sge_resource_quota_schedd/rqs_exceeded_sort_out_par() ******************
*  NAME
*     rqs_exceeded_sort_out_par() -- Rule out queues/hosts whenever possible
*
*  SYNOPSIS
*     void rqs_exceeded_sort_out_par(sge_assignment_t *a, const lListElem
*     *rule, const dstring *rule_name, const char* queue_name, const char*
*     host_name)
*
*  FUNCTION
*     Function wrapper around rqs_exceeded_sort_out() for parallel jobs.
*     In contrast to the sequential case global limit exeeding is handled
*     by adding all cluster queue names to the a->skip_cqueue_list.
*
*  INPUTS
*     sge_assignment_t *a      - Scheduler assignment type
*     const lListElem *rule    - The exeeded rule
*     const dstring *rule_name - Name of the rule (monitoring only)
*     const char* queue_name   - Cluster queue name
*     const char* host_name    - Host name
*
*  NOTES
*     MT-NOTE: rqs_exceeded_sort_out_par() is MT safe
*******************************************************************************/
static void rqs_exceeded_sort_out_par(sge_assignment_t *a, const lListElem *rule, const dstring *rule_name,
   const char* queue_name, const char* host_name)
{
   if (rqs_exceeded_sort_out(a, rule, rule_name, queue_name, host_name)) {
      rqs_expand_hosts(rule, a);
   }
}

/****** sge_resource_quota_schedd/parallel_rqs_slots_by_time() ******************
*  NAME
*     parallel_rqs_slots_by_time() -- Dertermine number of slots avail within
*                                      time frame
*
*  SYNOPSIS
*     dispatch_t parallel_rqs_slots_by_time(const sge_assignment_t *a,
*     int *slots, const char *host, const char *queue)
*
*  FUNCTION
*     This function iterates for a queue instance over all resource quota sets
*     and evaluates the number of slots available.
*
*  INPUTS
*     const sge_assignment_t *a - job info structure (in)
*     int *slots                - out: # free slots
*     lListElem *qep            - QU_Type Elem
*
*  RESULT
*     static dispatch_t - DISPATCH_OK        got an assignment
*                       - DISPATCH_NEVER_CAT no assignment for all jobs af that category
*
*  NOTES
*     MT-NOTE: parallel_rqs_slots_by_time() is not MT safe
*
*  SEE ALSO
*     ri_slots_by_time()
*
*******************************************************************************/
dispatch_t
parallel_rqs_slots_by_time(sge_assignment_t *a, int *slots, lListElem *qep, bool need_master,
                           bool is_master_queue)
{
   dispatch_t result = DISPATCH_OK;
   int tslots = INT_MAX;
   const char* queue = lGetString(qep, QU_qname);
   const char* host = lGetHost(qep, QU_qhostname);

   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(a->rqs_list) != 0) {
      const char* user = a->user;
      const char* group = a->group;
      const lList *grp_list = a->grp_list;
      const char* project = a->project;
      const char* pe = a->pe_name;
      lListElem *rql;
      const lListElem *rqs;
      // @todo can we used static dstrings? What size would be needed?
      dstring dstr_rule_name = DSTRING_INIT;
      dstring dstr_rue_string = DSTRING_INIT;
      dstring dstr_limit_name = DSTRING_INIT;
      lListElem *exec_host = host_list_locate(a->host_list, host);

      SCHED_PROF_INC(a->pi, par_rqs);

      for_each_ep(rqs, a->rqs_list) {
         lListElem *rule = nullptr;

         /* ignore disabled rule sets */
         if (!lGetBool(rqs, RQS_enabled)) {
            continue;
         }
         sge_dstring_clear(&dstr_rule_name);
         rule = rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list, &dstr_rule_name);
         if (rule != nullptr) {
            lListElem *limit = nullptr;
            const char *limit_s;
            rqs_get_rue_string(&dstr_rue_string, rule, user, project, host, queue, pe);
            limit_s = sge_dstring_sprintf(&dstr_limit_name, "%s=%s", sge_dstring_get_string(&dstr_rule_name), sge_dstring_get_string(&dstr_rue_string));

            /* reuse earlier result */
            if ((rql=lGetElemStrRW(a->limit_list, RQL_name, limit_s))) {
               result = (dispatch_t)lGetInt(rql, RQL_result);
               tslots = MIN(tslots, lGetInt(rql, RQL_slots));

               // build the minimum
               lAndUlongBitMask(qep, QU_tagged4schedule, lGetUlong(rql, RQL_tagged4schedule));

               DPRINTF("parallel_rqs_slots_by_time(%s@%s) result %d slots %d for " SFQ " (cache)\n",
                       queue, host, result, tslots, limit_s);
            } else {
               int ttslots = INT_MAX;

               u_long32 tagged_for_schedule_old = lGetUlong(qep, QU_tagged4schedule);  /* default value or set in match_static_queue() */
               lSetUlong(qep, QU_tagged4schedule, TAG4SCHED_ALL);

               for_each_rw(limit, lGetList(rule, RQR_limit)) {
                  const char *limit_name = lGetString(limit, RQRL_name);

                  lListElem *raw_centry = centry_list_locate(a->centry_list, limit_name);
                  if (raw_centry == nullptr) {
                     DPRINTF("ignoring limit %s because not defined", limit_name);
                     continue;
                  } else {
                     DPRINTF("checking limit %s\n", lGetString(raw_centry, CE_name));
                  }

                  lList *job_centry_list = job_get_hard_resource_listRW(a->job); // @todo CS-400 need to check all request lists
                  // @todo do we really need to pass the whole job_centry_list info functions below,
                  //       or could we create a sub-list with just the one job_entry element?
                  //       And would we have to copy-back info like CE_tagged?
                  lListElem *job_centry = centry_list_locate(job_centry_list, limit_name);

                  /* found a rule, now check limit */
                  if (lGetUlong(raw_centry, CE_consumable)) {

                     rqs_get_rue_string(&dstr_rue_string, rule, user, project, host, queue, pe);

                     if (rqs_set_dynamical_limit(limit, a->gep, exec_host, a->centry_list)) {
                        int tttslots = INT_MAX;
                        result = parallel_limit_slots_by_time(a, &tttslots, raw_centry,
                                                              limit, &dstr_rue_string, qep,
                                                              need_master, is_master_queue);
                        ttslots = MIN(ttslots, tttslots);
                        if (result == DISPATCH_NOT_AT_TIME) {
                           /* can still be interesting for reservation and as slave task for per_job_consumables */
                           result = DISPATCH_OK;
                        } else if (result != DISPATCH_OK) {
                           break;
                        }
                     } else {
                        result = DISPATCH_NEVER_CAT;
                        break;
                     }
                  } else if (job_centry != nullptr) {
                     char availability_text[2048];

                     lSetString(raw_centry, CE_stringval, lGetString(limit, RQRL_value));
                     if (compare_complexes(1, raw_centry, job_centry, availability_text, false, false) != 1) {
                        result = DISPATCH_NEVER_CAT;
                        break;
                     }
                  }

               }

               DPRINTF("parallel_rqs_slots_by_time(%s@%s) result %d slots %d for " SFQ " (fresh)\n",
                       queue, host, result, ttslots, limit_s);

               /* store result for reuse */
               rql = lAddElemStr(&(a->limit_list), RQL_name, limit_s, RQL_Type);
               lSetInt(rql, RQL_result, result);
               lSetInt(rql, RQL_slots, ttslots);
               lSetUlong(rql, RQL_tagged4schedule, lGetUlong(qep, QU_tagged4schedule));

               /* reset QU_tagged4schedule if necessary */
               lAndUlongBitMask(qep, QU_tagged4schedule, tagged_for_schedule_old);

               tslots = MIN(tslots, ttslots);
            }

            if (result != DISPATCH_OK || tslots == 0) {
               DPRINTF("RQS PARALLEL SORT OUT\n");
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_CANNOTRUNRQSGLOBAL_SS,
                     sge_dstring_get_string(&dstr_rue_string), sge_dstring_get_string(&dstr_rule_name));
               rqs_exceeded_sort_out_par(a, rule, &dstr_rule_name, queue, host);
            }

            if (result != DISPATCH_OK || tslots == 0) {
               break;
            }
         }
      }
      sge_dstring_free(&dstr_rue_string);
      sge_dstring_free(&dstr_rule_name);
      sge_dstring_free(&dstr_limit_name);
   }

   *slots = tslots;

   DPRINTF("parallel_rqs_slots_by_time(%s@%s) finalresult %d slots %d\n", queue, host, result, *slots);

   DRETURN(result);
}

/****** sge_resource_quota_schedd/rqs_match_assignment() ***********************
*  NAME
*     rqs_match_assignment() -- match resource quota rule any queue instance
*
*  SYNOPSIS
*     static bool rqs_match_assignment(const lListElem *rule, sge_assignment_t
*     *a)
*
*  FUNCTION
*     Check whether a resource quota rule can match any queue instance. If
*     if does not match due to users/projects/pes scope one can rule this
*     out.
*
*     Note: As long as rqs_match_assignment() is not used for parallel jobs
*           passing nullptr as PE request is perfectly fine.
*
*  INPUTS
*     const lListElem *rule - Resource quota rule
*     sge_assignment_t *a   - Scheduler assignment
*
*  RESULT
*     static bool - True if it matches
*
*  NOTES
*     MT-NOTE: rqs_match_assignment() is MT safe
*******************************************************************************/
static bool rqs_match_assignment(const lListElem *rule, sge_assignment_t *a)
{
   return (rqs_filter_match(lGetObject(rule, RQR_filter_projects), FILTER_PROJECTS, a->project, nullptr, nullptr, nullptr, nullptr) &&
           rqs_filter_match(lGetObject(rule, RQR_filter_users), FILTER_USERS, a->user, a->acl_list, nullptr, a->group, a->grp_list) &&
           rqs_filter_match(lGetObject(rule, RQR_filter_pes), FILTER_PES, nullptr, nullptr, nullptr, nullptr, nullptr))?true:false;
}


/****** sge_resource_quota_schedd/rqs_can_optimize() ***************************
*  NAME
*     rqs_can_optimize() -- Poke whether a queue/host negation can be made
*
*  SYNOPSIS
*     static void rqs_can_optimize(const lListElem *rule, bool *host, bool
*     *queue, sge_assignment_t *a)
*
*  FUNCTION
*     A global limit was hit with 'rule'. This function helps to determine
*     to what exend we can profit from that situation. If there is no
*     previous matching rule within the same rule set any other queue/host
*     can be skipped.
*
*  INPUTS
*     const lListElem *rule - Rule
*     bool *host            - Any previous rule with a host scope?
*     bool *queue           - Any previous rule with a queue scope?
*     sge_assignment_t *a   - Scheduler assignment
*
*  NOTES
*     MT-NOTE: rqs_can_optimize() is MT safe
*******************************************************************************/
void rqs_can_optimize(const lListElem *rule, bool *host, bool *queue, sge_assignment_t *a)
{
   bool host_shadowed = false, queue_shadowed = false;

   const lListElem *prev = rule;
   while ((prev = lPrev(prev))) {
      if (!rqs_match_assignment(rule, a))
         continue;
      if (!is_host_global(prev))
         host_shadowed = true;
      if (!is_cqueue_global(prev))
         queue_shadowed = true;
   }

   *host = host_shadowed;
   *queue = queue_shadowed;

   return;
}

/****** sge_resource_quota_schedd/check_and_debit_rqs_slots() *********************
*  NAME
*     check_and_debit_rqs_slots() -- Determine RQS limit slot amount and debit
*
*  SYNOPSIS
*     static void check_and_debit_rqs_slots(sge_assignment_t *a, const char
*     *host, const char *queue, int *slots, dstring
*     *rule_name, dstring *rue_name, dstring *limit_name)
*
*  FUNCTION
*     The function determines the final slot amount due
*     to all resource quota limitations that apply for the queue instance.
*     Both slot amounts get debited from the a->limit_list to keep track
*     of still available amounts per resource quota limit.
*
*  INPUTS
*     sge_assignment_t *a - Assignment data structure
*     const char *host    - hostname
*     const char *queue   - queuename
*     int *slots          - needed/available slots
*     dstring *rule_name  - caller maintained buffer
*     dstring *rue_name   - caller maintained buffer
*     dstring *limit_name - caller maintained buffer
*
*  NOTES
*     MT-NOTE: check_and_debit_rqs_slots() is MT safe
*******************************************************************************/
void parallel_check_and_debit_rqs_slots(sge_assignment_t *a, const char *host, const char *queue,
      int *slots, dstring *rule_name, dstring *rue_name, dstring *limit_name)
{
   const lListElem *rqs, *rule;
   const char* user = a->user;
   const char* group = a->group;
   const lList *grp_list = a->grp_list;
   const char* project = a->project;
   const char* pe = a->pe_name;

   DENTER(TOP_LAYER);

   /* first step - see how many slots are left */
   for_each_ep(rqs, a->rqs_list) {

      /* ignore disabled rule sets */
      if (!lGetBool(rqs, RQS_enabled)) {
         continue;
      }
      sge_dstring_clear(rule_name);
      rule = rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list, rule_name);
      if (rule != nullptr) {
         const lListElem *rql;
         rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
         if ((rql = lGetElemStr(a->limit_list, RQL_name, sge_dstring_get_string(limit_name)))) {
            *slots = MIN(*slots, lGetInt(rql, RQL_slots));
         } else {
            *slots = 0;
         }
      }

      if (*slots == 0) {
         break;
      }
   }

   /* second step - reduce number of remaining slots  */
   if (*slots != 0) {
      for_each_ep(rqs, a->rqs_list) {

         /* ignore disabled rule sets */
         if (!lGetBool(rqs, RQS_enabled)) {
            continue;
         }
         sge_dstring_clear(rule_name);
         rule = rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list, rule_name);
         if (rule != nullptr) {
            lListElem *rql;
            rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
            sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
            rql = lGetElemStrRW(a->limit_list, RQL_name, sge_dstring_get_string(limit_name));
            lSetInt(rql, RQL_slots,      lGetInt(rql, RQL_slots) - *slots);
         }
      }
   }

   DPRINTF("check_and_debit_rqs_slots(%s@%s) slots: %d\n", queue, host, *slots);

   DRETURN_VOID;
}

void parallel_revert_rqs_slot_debitation(sge_assignment_t *a, const char *host, const char *queue,
      int slots, dstring *rule_name, dstring *rue_name, dstring *limit_name)
{
   const lListElem *rqs, *rule;
   const char* user = a->user;
   const char* group = a->group;
   const lList *grp_list = a->grp_list;
   const char* project = a->project;
   const char* pe = a->pe_name;

   DENTER(TOP_LAYER);

   for_each_ep(rqs, a->rqs_list) {

      /* ignore disabled rule sets */
      if (!lGetBool(rqs, RQS_enabled)) {
         continue;
      }
      sge_dstring_clear(rule_name);
      rule = rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list, rule_name);
      if (rule != nullptr) {
         lListElem *rql;
         rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
         rql = lGetElemStrRW(a->limit_list, RQL_name, sge_dstring_get_string(limit_name));
         DPRINTF("limit: %s %d <--- %d\n", sge_dstring_get_string(limit_name), lGetInt(rql, RQL_slots), lGetInt(rql, RQL_slots)+slots);
         lSetInt(rql, RQL_slots,      lGetInt(rql, RQL_slots) + slots);
      }
   }

   DRETURN_VOID;
}

/****** sge_resource_quota_schedd/rqs_by_slots() ***********************************
*  NAME
*     rqs_by_slots() -- Check queue instance suitability due to RQS
*
*  SYNOPSIS
*     dispatch_t rqs_by_slots(sge_assignment_t *a, const char *queue,
*     const char *host, u_long64 *tt_rqs_all, bool *is_global,
*     dstring *rue_string, dstring *limit_name, dstring *rule_name)
*
*  FUNCTION
*     Checks (or determines earliest time) queue instance suitability
*     according to resource quota set limits.
*
*     For performance reasons RQS verification results are cached in
*     a->limit_list. In addition unsuited queues and hosts are collected
*     in a->skip_cqueue_list and a->skip_host_list so that ruling out
*     chunks of queue instance becomes quite cheap.
*
*  INPUTS
*     sge_assignment_t *a  - assignment
*     const char *queue    - cluster queue name
*     const char *host     - host name
*     u_long64 *tt_rqs_all - returns earliest time over all resource quotas
*     bool *is_global      - returns true if result is valid for any other queue
*     dstring *rue_string  - caller maintained buffer
*     dstring *limit_name  - caller maintained buffer
*     dstring *rule_name   - caller maintained buffer
*     u_long64 tt_best     - time of best solution found so far
*
*  RESULT
*     static dispatch_t - usual return values
*
*  NOTES
*     MT-NOTE: rqs_by_slots() is MT safe
*******************************************************************************/
dispatch_t rqs_by_slots(sge_assignment_t *a, const char *queue, const char *host,
  u_long64 *tt_rqs_all, bool *is_global, dstring *rue_string, dstring *limit_name, dstring *rule_name, u_long64 tt_best)
{
   const lListElem *rqs;
   dispatch_t result = DISPATCH_OK;

   DENTER(TOP_LAYER);

   *is_global = false;

   if (lGetNumberOfElem(a->rqs_list) > 0) {
      SCHED_PROF_INC(a->pi, seq_rqs);
   }

   for_each_ep(rqs, a->rqs_list) {
      u_long64 tt_rqs = a->start;
      const char *user = a->user;
      const char *group = a->group;
      const lList *grp_list = a->grp_list;
      const char *project = a->project;
      const lListElem *rule;

      if (!lGetBool(rqs, RQS_enabled)) {
         continue;
      }

      sge_dstring_clear(rule_name);
      rule = rqs_get_matching_rule(rqs, user, group, grp_list, project, nullptr, host, queue, a->acl_list, a->hgrp_list, rule_name);
      if (rule != nullptr) {
         const char *limit;
         lListElem *rql;

         /* need unique identifier for cache */
         rqs_get_rue_string(rue_string, rule, user, project, host, queue, nullptr);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_string));
         limit = sge_dstring_get_string(limit_name);

         /* check limit or reuse earlier results */
         if ((rql=lGetElemStrRW(a->limit_list, RQL_name, limit))) {
            tt_rqs = lGetUlong64(rql, RQL_time);
            result = (dispatch_t)lGetInt(rql, RQL_result);
         } else {
            /* Check booked usage */
            result = rqs_limitation_reached(a, rule, host, queue, &tt_rqs);

            rql = lAddElemStr(&(a->limit_list), RQL_name, limit, RQL_Type);
            lSetInt(rql, RQL_result, result);
            lSetUlong64(rql, RQL_time, tt_rqs);
            /* init with same value as QU_tagged4schedule */
            lSetUlong(rql, RQL_tagged4schedule, 2);

            if (result != DISPATCH_OK && result != DISPATCH_MISSING_ATTR) {
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_CANNOTRUNRQSGLOBAL_SS,
                     sge_dstring_get_string(rue_string), sge_dstring_get_string(rule_name));
               if (rqs_exceeded_sort_out(a, rule, rule_name, queue, host)) {
                  *is_global = true;
               }
            }
         }

         if (result == DISPATCH_MISSING_ATTR) {
            result = DISPATCH_OK;
            continue;
         }
         if (result != DISPATCH_OK)
            break;

         if (a->is_reservation && tt_rqs >= tt_best) {
            /* no need to further investigate these ones */
            if (rqs_exceeded_sort_out(a, rule, rule_name, queue, host))
               *is_global = true;
         }

         *tt_rqs_all = MAX(*tt_rqs_all, tt_rqs);
      }
   }

   if (!rqs) {
      result = DISPATCH_OK;
   }

   if (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR) {
      DPRINTF("rqs_by_slots(%s@%s) returns <at specified time> " sge_u64 "\n", queue, host, tt_rqs_all);
   } else {
      DPRINTF("rqs_by_slots(%s@%s) returns <later> " sge_u64 " (%s)\n", queue, host, tt_rqs_all, *is_global?"global":"not global");
   }

   DRETURN(result);
}

/****** sge_resource_quota_schedd/rqs_expand_cqueues() *************************
*  NAME
*     rqs_expand_cqueues() -- Add all matching cqueues to the list
*
*  SYNOPSIS
*     void rqs_expand_cqueues(const lListElem *rule)
*
*  FUNCTION
*     The names of all cluster queues that match the rule are added to
*     the skip list without duplicates.
*
*  INPUTS
*     const lListElem *rule    - RQR_Type
*
*  NOTES
*     MT-NOTE: rqs_expand_cqueues() is not MT safe
*******************************************************************************/
void rqs_expand_cqueues(const lListElem *rule, sge_assignment_t *a)
{
   const lListElem *cq;
   const char *cqname;
   lListElem *qfilter = lGetObject(rule, RQR_filter_queues);

   DENTER(TOP_LAYER);

   for_each_ep(cq, *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE)) {
      cqname = lGetString(cq, CQ_name);
      if (lGetElemStr(a->skip_cqueue_list, CTI_name, cqname))
         continue;
      if (rqs_filter_match(qfilter, FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr) && !cqueue_shadowed_by(cqname, rule, a))
         lAddElemStr(&(a->skip_cqueue_list), CTI_name, cqname, CTI_Type);
   }

   DRETURN_VOID;
}

/****** sge_resource_quota_schedd/rqs_expand_hosts() ***************************
*  NAME
*     rqs_expand_hosts() -- Add all matching hosts to the list
*
*  SYNOPSIS
*     void rqs_expand_hosts(const lListElem *rule, lList **skip_host_list,
*     const lList *host_list, lList *hgrp_list)
*
*  FUNCTION
*     The names of all hosts that match the rule are added to
*     the skip list without duplicates.
*
*  INPUTS
*     const lListElem *rule  - RQR_Type
*     const lList *host_list - EH_Type
*
*  NOTES
*     MT-NOTE: rqs_expand_hosts() is MT safe
*******************************************************************************/
void rqs_expand_hosts(const lListElem *rule, sge_assignment_t *a)
{
   const lListElem *eh;
   const char *hname;
   lListElem *hfilter = lGetObject(rule, RQR_filter_hosts);

   for_each_ep(eh, a->host_list) {
      hname = lGetHost(eh, EH_name);
      if (lGetElemStr(a->skip_host_list, CTI_name, hname))
         continue;
      if (rqs_filter_match(hfilter, FILTER_HOSTS, hname, nullptr, a->hgrp_list, nullptr, nullptr) && !host_shadowed_by(hname, rule, a))
         lAddElemStr(&(a->skip_host_list), CTI_name, hname, CTI_Type);
   }

   return;
}

/****** sge_resource_quota_schedd/cqueue_shadowed() ****************************
*  NAME
*     cqueue_shadowed() -- Check for cluster queue rule before current rule
*
*  SYNOPSIS
*     static bool cqueue_shadowed(const lListElem *rule, sge_assignment_t *a)
*
*  FUNCTION
*     Check whether there is any cluster queue specific rule before the
*     current rule.
*
*  INPUTS
*     const lListElem *rule - Current rule
*     sge_assignment_t *a   - Scheduler assignment
*
*  RESULT
*     static bool - True if shadowed
*
*  EXAMPLE
*     limit queue Q001 to F001=1
*     limit host gridware to F001=0  (--> returns 'true' due to 'Q001' meaning
*                               that gridware can't be generelly ruled out )
*
*  NOTES
*     MT-NOTE: cqueue_shadowed() is MT safe
*******************************************************************************/
bool cqueue_shadowed(const lListElem *rule, sge_assignment_t *a)
{
   while ((rule = lPrev(rule))) {
      if (rqs_match_assignment(rule, a) && !is_cqueue_global(rule)) {
         return true;
      }
   }
   return false;
}

/****** sge_resource_quota_schedd/host_shadowed() ******************************
*  NAME
*     host_shadowed() -- Check for host rule before current rule
*
*  SYNOPSIS
*     static bool host_shadowed(const lListElem *rule, sge_assignment_t *a)
*
*  FUNCTION
*     Check whether there is any host specific rule before the
*     current rule.
*
*  INPUTS
*     const lListElem *rule - Current rule
*     sge_assignment_t *a   - Scheduler assignment
*
*  RESULT
*     static bool - True if shadowed
*
*  EXAMPLE
*     limit host gridware to F001=1
*     limit queue Q001 to F001=0  (--> returns 'true' due to 'gridware' meaning
*                               that Q001 can't be generelly ruled out )
*
*  NOTES
*     MT-NOTE: host_shadowed() is MT safe
*******************************************************************************/
bool host_shadowed(const lListElem *rule, sge_assignment_t *a)
{
   while ((rule = lPrev(rule))) {
      if (rqs_match_assignment(rule, a) && !is_host_global(rule)) {
         return true;
      }
   }
   return false;
}

/****** sge_resource_quota_schedd/rqs_excluded_cqueues() ***********************
*  NAME
*     rqs_excluded_cqueues() -- Find excluded queues
*
*  SYNOPSIS
*     static void rqs_excluded_cqueues(const lListElem *rule, sge_assignment_t *a)
*
*  FUNCTION
*     Find queues that are excluded by previous rules.
*
*  INPUTS
*     const lListElem *rule    - The rule
*     sge_assignment_t *a      - Scheduler assignement
*
*  EXAMPLE
*      limit        projects {*} queues !Q001 to F001=1
*      limit        to F001=0   ( ---> returns Q001 in a->skip_cqueue_list)
*
*  NOTES
*     MT-NOTE: rqs_excluded_cqueues() is MT safe
*******************************************************************************/
void rqs_excluded_cqueues(const lListElem *rule, sge_assignment_t *a)
{
   const lListElem *cq;
   const lListElem *prev;
   int ignored = 0, excluded = 0;

   DENTER(TOP_LAYER);

   for_each_ep(cq, *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE)) {
      const char *cqname = lGetString(cq, CQ_name);
      bool exclude = true;

      if (lGetElemStr(a->skip_cqueue_list, CTI_name, cqname)) {
         ignored++;
         continue;
      }

      prev = rule;
      while ((prev = lPrev(prev))) {
         if (!rqs_match_assignment(rule, a))
            continue;

         if (rqs_filter_match(lGetObject(prev, RQR_filter_queues), FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr)) {
            exclude = false;
            break;
         }
      }
      if (exclude) {
         lAddElemStr(&(a->skip_cqueue_list), CTI_name, cqname, CTI_Type);
         excluded++;
      }
   }

   if (ignored + excluded == 0) {
      CRITICAL("not a single queue excluded in rqs_excluded_cqueues()\n");
   }

   DRETURN_VOID;
}

/****** sge_resource_quota_schedd/rqs_excluded_hosts() *************************
*  NAME
*     rqs_excluded_hosts() -- Find excluded hosts
*
*  SYNOPSIS
*     static void rqs_excluded_hosts(const lListElem *rule, sge_assignment_t *a)
*
*  FUNCTION
*     Find hosts that are excluded by previous rules.
*
*  INPUTS
*     const lListElem *rule    - The rule
*     sge_assignment_t *a      - Scheduler assignement
*
*  EXAMPLE
*      limit        projects {*} queues !gridware to F001=1
*      limit        to F001=0   ( ---> returns gridware in skip_host_list)
*
*  NOTES
*     MT-NOTE: rqs_excluded_hosts() is MT safe
*******************************************************************************/
void rqs_excluded_hosts(const lListElem *rule, sge_assignment_t *a)
{
   const lListElem *eh;
   const lListElem *prev;
   int ignored = 0, excluded = 0;

   DENTER(TOP_LAYER);

   for_each_ep(eh, a->host_list) {
      const char *hname = lGetHost(eh, EH_name);
      bool exclude = true;

      if (lGetElemStr(a->skip_host_list, CTI_name, hname)) {
         ignored++;
         continue;
      }

      prev = rule;
      while ((prev = lPrev(prev))) {
         if (!rqs_match_assignment(rule, a))
            continue;

         if (rqs_filter_match(lGetObject(prev, RQR_filter_hosts), FILTER_HOSTS, hname, nullptr, a->hgrp_list, nullptr, nullptr)) {
            exclude = false;
            break;
         }
      }
      if (exclude) {
         lAddElemStr(&(a->skip_host_list), CTI_name, hname, CTI_Type);
         excluded++;
      }
   }

   if (ignored + excluded == 0) {
      CRITICAL("not a single host excluded in rqs_excluded_hosts()\n");
   }

   DRETURN_VOID;
}

/****** sge_resource_quota_schedd/cqueue_shadowed_by() *************************
*  NAME
*     cqueue_shadowed_by() -- Check rules shadowing current cluster queue rule
*
*  SYNOPSIS
*     static bool cqueue_shadowed_by(const char *cqname, const lListElem *rule,
*     sge_assignment_t *a)
*
*  FUNCTION
*     Check if cluster queue in current rule is shadowed.
*
*  INPUTS
*     const char *cqname    - Cluster queue name to check
*     const lListElem *rule - Current rule
*     sge_assignment_t *a   - Assignment
*
*  RESULT
*     static bool - True if shadowed
*
*  EXAMPLE
*     limits queues Q001,Q002 to F001=1
*     limits queues Q002,Q003 to F001=1 (--> returns 'true' for Q002 and 'false' for Q003)
*
*  NOTES
*     MT-NOTE: cqueue_shadowed_by() is MT safe
*******************************************************************************/
bool cqueue_shadowed_by(const char *cqname, const lListElem *rule, sge_assignment_t *a)
{
   while ((rule = lPrev(rule))) {
      if (rqs_match_assignment(rule, a) &&
          rqs_filter_match(lGetObject(rule, RQR_filter_queues), FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr)) {
         return true;
      }
   }

   return false;
}

/****** sge_resource_quota_schedd/host_shadowed_by() ***************************
*  NAME
*     host_shadowed_by() -- ???
*
*  SYNOPSIS
*     static bool host_shadowed_by(const char *host, const lListElem *rule,
*     sge_assignment_t *a)
*
*  FUNCTION
*     Check if host in current rule is shadowed.
*
*  INPUTS
*     const char *cqname    - Host name to check
*     const lListElem *rule - Current rule
*     sge_assignment_t *a   - Assignment
*
*  RESULT
*     static bool - True if shadowed
*
*  EXAMPLE
*     limits hosts host1,host2 to F001=1
*     limits hosts host2,host3 to F001=1 (--> returns 'true' for host2 and 'false' for host3)
*
*  NOTES
*     MT-NOTE: host_shadowed_by() is MT safe
*******************************************************************************/
bool host_shadowed_by(const char *host, const lListElem *rule, sge_assignment_t *a)
{
   while ((rule = lPrev(rule))) {
      if (rqs_match_assignment(rule, a) &&
          rqs_filter_match(lGetObject(rule, RQR_filter_hosts), FILTER_HOSTS, host, nullptr, a->hgrp_list, nullptr, nullptr)) {
         return true;
      }
   }

   return false;
}

/****** sge_resource_quota_schedd/rqs_set_dynamical_limit() ***********************
*  NAME
*     rqs_set_dynamical_limit() -- evaluate dynamical limit
*
*  SYNOPSIS
*     bool rqs_set_dynamical_limit(lListElem *limit, lListElem
*     *global_host, lListElem *exec_host, lList *centry)
*
*  FUNCTION
*     The function evaluates if neccessary the dynamical limit for a host and
*     sets the evaluated double value in the given limitation element (RQRL_dvalue).
*
*     A evaluation is neccessary if the limit boolean RQRL_dynamic is true. This
*     field is set by qmaster during the rule set verification
*
*  INPUTS
*     lListElem *limit       - limitation (RQRL_Type)
*     lListElem *global_host - global host (EH_Type)
*     lListElem *exec_host   - exec host (EH_Type)
*     lList *centry          - consumable resource list (CE_Type)
*
*  RESULT
*     bool - always true
*
*  NOTES
*     MT-NOTE: rqs_set_dynamical_limit() is MT safe
*
*******************************************************************************/
bool
rqs_set_dynamical_limit(lListElem *limit, lListElem *global_host, lListElem *exec_host, const lList *centry) {

   DENTER(TOP_LAYER);

   if (lGetBool(limit, RQRL_dynamic)) {
      double dynamic_limit = scaled_mixed_load(lGetString(limit, RQRL_value), global_host, exec_host, centry);
      DPRINTF("found a dynamic limit for host %s with value %d\n", lGetHost(exec_host, EH_name), (int)dynamic_limit);
      lSetDouble(limit, RQRL_dvalue, dynamic_limit);
   }

   DRETURN(true);
}

