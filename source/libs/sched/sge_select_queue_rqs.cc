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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of the resource quota matching
 */

#include <climits>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_resource_quota_service.h"
#include "sgeobj/sge_resource_utilization.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQL.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"

#include "sched/schedd_message.h"
#include "sched/msg_schedd.h"

#include "sge_complex_schedd.h"
#include "sge_schedd_text.h"
#include "sge_select_queue.h"
#include "sge_select_queue_rqs.h"
#include "sort_hosts.h"

static void rqs_can_optimize(ocs::tom::ConstElement<ocs::tom::RQR> rule, bool *host, bool *queue, sge_assignment_t *a);
static void rqs_expand_cqueues(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static void rqs_expand_hosts(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static bool cqueue_shadowed(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static bool host_shadowed(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static void rqs_excluded_hosts(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static void rqs_excluded_cqueues(ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static bool cqueue_shadowed_by(const char *cqname, ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static bool host_shadowed_by(const char *host, ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a);
static bool rqs_set_dynamical_limit(ocs::tom::Element<ocs::tom::RQRL> limit,
                                    ocs::tom::ConstElement<ocs::tom::EH> global_host,
                                    ocs::tom::ConstElement<ocs::tom::EH> exec_host,
                                    ocs::tom::ConstList<ocs::tom::CE> centry);

/**
 * @brief Is the limitation reached for a queue instance
 *
 * The function verifies no limitation is reached for the specific job request
 * and queue instance
 *
 * @param a job info structure
 * @param rule rqsource quota rule (RQR_Type)
 * @param host host name
 * @param queue queue name
 * @param start start time of job
 *
 * @return DISPATCH_OK job can be scheduled DISPATCH_NEVER_CAT no jobs of this category will be scheduled DISPATCH_NOT_AT_TIME job can be scheduled later DISPATCH_MISSING_ATTR rule does not match requested attributes
 *
 * @note MT-NOTE: rqs_limitation_reached() is not MT safe
 */
static dispatch_t rqs_limitation_reached(sge_assignment_t *a, const ocs::tom::Element<ocs::tom::RQR> rule,
                                         const char *host, const char *queue, uint64_t *start) {
   DENTER(TOP_LAYER);
   dispatch_t ret = DISPATCH_MISSING_ATTR;
   // created once and kept for the life of the process, as before: a static owning handle would free it at exit,
   // when the descriptors it needs may be gone already
   static ocs::tom::Element<ocs::tom::CE> implicit_slots_request;
   const auto exec_host = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(a->host_list, host));
   dstring rue_name = DSTRING_INIT;
   dstring reason = DSTRING_INIT;

   if (implicit_slots_request == nullptr) {
      implicit_slots_request = ocs::tom::Element<ocs::tom::CE>::from_typed(lCreateElem(CE_Type));
      ocs::tom::set<CE_name>(implicit_slots_request, SGE_ATTR_SLOTS);
      // slots is an INT; the type has to be set like on any other request, both
      // because it is true and because a CE_valtype of 0 is what marks a request
      // centry_list_fill_request() could not resolve - see ri_time_by_slots()
      ocs::tom::set<CE_valtype>(implicit_slots_request, static_cast<uint32_t>(ocs::CEntry::Type::INT));
      ocs::tom::set<CE_stringval>(implicit_slots_request, "1");
      ocs::tom::set<CE_doubleval>(implicit_slots_request, 1);
   }

   // the limits are changed: rqs_set_dynamical_limit() caches the evaluated value of a dynamic limit in RQRL_dvalue
   for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {
      bool       is_forced = false;
      const char *limit_name = ocs::tom::get<RQRL_name>(limit);
      const auto raw_centry =
         ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(a->centry_list, limit_name));

      if (raw_centry == nullptr) {
         DPRINTF("ignoring limit %s because not defined", limit_name);
         continue;
      } else {
         DPRINTF("checking limit %s\n", ocs::tom::get<CE_name>(raw_centry));
      }

      is_forced = ocs::tom::get<CE_requestable>(raw_centry) == REQU_FORCED;
      lList *job_centry_list = job_get_hard_resource_listRW(a->job);
      // @todo CS-400: we only need job_centry. Have a function searching it in the 3 possible request lists
      auto job_centry = ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(job_centry_list, limit_name));

      /* check for implicit slot and default request */
      if (job_centry == nullptr) {
         if (strcmp(ocs::tom::get<CE_name>(raw_centry), SGE_ATTR_SLOTS) == 0) {
            job_centry = implicit_slots_request;
         } else if (ocs::tom::get<CE_defaultval>(raw_centry) != nullptr && ocs::tom::get<CE_consumable>(raw_centry)) {
            double request;
            parse_ulong_val(&request, nullptr,
                            static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(raw_centry)),
                            ocs::tom::get<CE_defaultval>(raw_centry), nullptr, 0);

            /* default requests with zero value are ignored */
            if (request == 0.0 && ocs::tom::get<CE_relop>(raw_centry) != CMPLXEXCL_OP) {
               continue;
            }
            ocs::tom::set<CE_stringval>(raw_centry, ocs::tom::get<CE_defaultval>(raw_centry));
            ocs::tom::set<CE_doubleval>(raw_centry, request);
            job_centry = raw_centry;
            DPRINTF("using default request for %s!\n", ocs::tom::get<CE_name>(raw_centry));
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

      if (rqs_set_dynamical_limit(limit, ocs::tom::ConstElement<ocs::tom::EH>::from_typed(a->gep), exec_host,
                                  ocs::tom::ConstList<ocs::tom::CE>::from_typed(a->centry_list))) {
         const auto rue_list = ocs::tom::get<RQRL_usage>(limit);
         uint64_t tmp_time = a->start;

         /* create tmp_centry_list */
         auto tmp_centry_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
         auto tmp_centry_elem = raw_centry.clone();
         ocs::tom::set<CE_stringval>(tmp_centry_elem, ocs::tom::get<RQRL_value>(limit));
         ocs::tom::set<CE_doubleval>(tmp_centry_elem, ocs::tom::get<RQRL_dvalue>(limit));
         tmp_centry_list->push_back(std::move(tmp_centry_elem));

         /* create tmp_rue_list */
         auto tmp_rue_list = ocs::tom::ListPtr<ocs::tom::RUE>::create("");
         rqs_get_rue_string(&rue_name, rule, a->user, a->project, host, queue, nullptr);
         const auto rue = ocs::tom::find_first<RUE_name>(rue_list, sge_dstring_get_string(&rue_name));
         auto tmp_rue_elem = rue != nullptr ? rue.clone() : ocs::tom::ElementPtr<ocs::tom::RUE>::create();
         ocs::tom::set<RUE_name>(tmp_rue_elem, limit_name);
         tmp_rue_list->push_back(std::move(tmp_rue_elem));

         sge_dstring_clear(&reason);
         ocs::TopologyString binding_inuse;
         ret = ri_time_by_slots(a, job_centry, nullptr, tmp_centry_list.get(), tmp_rue_list.get(),
                                nullptr, nullptr, &reason, false, 1,
                                DOMINANT_LAYER_RQS, 0.0, &tmp_time, SGE_RQS_NAME, binding_inuse);
         if (ret != DISPATCH_OK) {
            DPRINTF("denied because: %s\n", sge_dstring_get_string(&reason));
            break;
         }

         if (a->is_reservation && ret == DISPATCH_OK) {
            *start = tmp_time;
         }
      }
   }

   sge_dstring_free(&reason);
   sge_dstring_free(&rue_name);

   DRETURN(ret);
}

/**
 * @brief Rule out queues/hosts whenever possible
 *
 * This function tries to rule out hosts and cluster queues after a
 * quota exeeding was found for a limitation rule with specific queue
 * instance.
 * When a limitation was exeeded that applies to the entire
 * cluster 'true' is returned, 'false' otherwise.
 *
 * @param a Scheduler assignment type
 * @param rule The exeeded rule
 * @param rule_name Name of the rule (monitoring only)
 * @param queue_name Cluster queue name
 * @param host_name Host name
 *
 * @return True upon global limits exceeding
 *
 * @note MT-NOTE: rqs_exceeded_sort_out() is MT safe
 */
static bool rqs_exceeded_sort_out(sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::RQR> rule,
                                  const dstring *rule_name, const char *queue_name, const char *host_name) {
   DENTER(TOP_LAYER);

   bool cq_global = is_cqueue_global(rule);
   bool eh_global = is_host_global(rule);

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

      if (ocs::tom::get<RQRF_expand>(ocs::tom::get<RQR_filter_queues>(rule))) {
         if (!a->skip_cqueue_list) {
            a->skip_cqueue_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_cqueue_list->emplace_back<CTI_name>(queue_name);
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

      if (ocs::tom::get<RQRF_expand>(ocs::tom::get<RQR_filter_hosts>(rule))) {
         if (!a->skip_host_list) {
            a->skip_host_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_host_list->emplace_back<CTI_name>(host_name);
         DPRINTF("HOST: resource quota set %s deny job execution at host %s\n", sge_dstring_get_string(rule_name), host_name);
      } else {
         rqs_expand_hosts(rule, a);
         DPRINTF("HOST: resource quota set %s deny job execution at all its hosts\n", sge_dstring_get_string(rule_name));
      }

      DRETURN(false);
   }
}

/**
 * @brief Rule out queues/hosts whenever possible
 *
 * Function wrapper around rqs_exceeded_sort_out() for parallel jobs.
 * In contrast to the sequential case global limit exeeding is handled
 * by adding all cluster queue names to the a->skip_cqueue_list.
 *
 * @param a Scheduler assignment type
 * @param rule The exeeded rule
 * @param rule_name Name of the rule (monitoring only)
 * @param queue_name Cluster queue name
 * @param host_name Host name
 *
 * @note MT-NOTE: rqs_exceeded_sort_out_par() is MT safe
 */
static void rqs_exceeded_sort_out_par(sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::RQR> rule,
                                      const dstring *rule_name, const char *queue_name, const char *host_name) {
   if (rqs_exceeded_sort_out(a, rule, rule_name, queue_name, host_name)) {
      rqs_expand_hosts(rule, a);
   }
}

/**
 * @brief Dertermine number of slots avail within
 *
 * This function iterates for a queue instance over all resource quota sets
 * and evaluates the number of slots available.
 *
 * @param a job info structure (in)
 * @param slots out: # free slots
 * @param qep queue instance (`QU_Type`)
 * @param need_master a master task still has to be placed
 * @param is_master_queue this queue instance already carries the master task
 *
 * @return #DISPATCH_OK when an assignment was found, #DISPATCH_NEVER_CAT when
 *         no job of that category can ever get one
 *
 * @note MT-NOTE: parallel_rqs_slots_by_time() is not MT safe
 *
 * @see ri_time_by_slots()
 */
dispatch_t
parallel_rqs_slots_by_time(sge_assignment_t *a, int *slots, const ocs::tom::Element<ocs::tom::QU> qep, bool need_master,
                           bool is_master_queue) {
   DENTER(TOP_LAYER);

   dispatch_t result = DISPATCH_OK;
   int tslots = INT_MAX;
   const char* queue = ocs::tom::get<QU_qname>(qep);
   const char* host = ocs::tom::get<QU_qhostname>(qep);

   if (lGetNumberOfElem(a->rqs_list) != 0) {
      const char* user = a->user;
      const char* group = a->group;
      const lList *grp_list = a->grp_list;
      const char* project = a->project;
      const char* pe = a->pe_name;
      ocs::tom::Element<ocs::tom::RQL> rql;
      // @todo can we used static dstrings? What size would be needed?
      dstring dstr_rule_name = DSTRING_INIT;
      dstring dstr_rue_string = DSTRING_INIT;
      dstring dstr_limit_name = DSTRING_INIT;
      const auto exec_host = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(a->host_list, host));

      SCHED_PROF_INC(a->pi, par_rqs);

      for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {
         /* ignore disabled rule sets */
         if (!ocs::tom::get<RQS_enabled>(rqs)) {
            continue;
         }
         sge_dstring_clear(&dstr_rule_name);
         const auto rule = ocs::tom::Element<ocs::tom::RQR>::from_typed(
            rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list,
                                  &dstr_rule_name));
         if (rule != nullptr) {
            const char *limit_s;
            rqs_get_rue_string(&dstr_rue_string, rule, user, project, host, queue, pe);
            limit_s = sge_dstring_sprintf(&dstr_limit_name, "%s=%s", sge_dstring_get_string(&dstr_rule_name), sge_dstring_get_string(&dstr_rue_string));

            /* reuse earlier result */
            if ((rql=ocs::tom::find_first_rw<RQL_name>(*a->limit_list, limit_s))) {
               result = (dispatch_t)ocs::tom::get<RQL_result>(rql);
               tslots = std::min(tslots, ocs::tom::get<RQL_slots>(rql));

               // build the minimum
               ocs::tom::and_bits<QU_tagged4schedule>(qep, ocs::tom::get<RQL_tagged4schedule>(rql));

               DPRINTF("parallel_rqs_slots_by_time(%s@%s) result %d slots %d for " SFQ " (cache)\n",
                       queue, host, result, tslots, limit_s);
            } else {
               int ttslots = INT_MAX;

               uint32_t tagged_for_schedule_old = ocs::tom::get<QU_tagged4schedule>(qep);  /* default value or set in match_static_queue() */
               ocs::tom::set<QU_tagged4schedule>(qep, TAG4SCHED_ALL);

               for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {
                  const char *limit_name = ocs::tom::get<RQRL_name>(limit);

                  const auto raw_centry =
         ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(a->centry_list, limit_name));
                  if (raw_centry == nullptr) {
                     DPRINTF("ignoring limit %s because not defined", limit_name);
                     continue;
                  } else {
                     DPRINTF("checking limit %s\n", ocs::tom::get<CE_name>(raw_centry));
                  }

                  lList *job_centry_list = job_get_hard_resource_listRW(a->job); // @todo CS-400 need to check all request lists
                  // @todo do we really need to pass the whole job_centry_list info functions below,
                  //       or could we create a sub-list with just the one job_entry element?
                  //       And would we have to copy-back info like CE_tagged?
                  const auto job_centry = ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(job_centry_list, limit_name));

                  /* found a rule, now check limit */
                  if (ocs::tom::get<CE_consumable>(raw_centry)) {

                     rqs_get_rue_string(&dstr_rue_string, rule, user, project, host, queue, pe);

                     if (rqs_set_dynamical_limit(limit, ocs::tom::ConstElement<ocs::tom::EH>::from_typed(a->gep), exec_host,
                                                 ocs::tom::ConstList<ocs::tom::CE>::from_typed(a->centry_list))) {
                        int tttslots = INT_MAX;
                        result = parallel_limit_slots_by_time(a, &tttslots, raw_centry,
                                                              limit, &dstr_rue_string, qep,
                                                              need_master, is_master_queue);
                        ttslots = std::min(ttslots, tttslots);
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

                     // A dynamic limit carries a load formula rather than a number, and
                     // RQRL_dvalue is filled in only by rqs_set_dynamical_limit(), which
                     // evaluates the formula against the host under consideration. Without
                     // this call the value read below is 0 for a dynamic limit that nothing
                     // else has evaluated in this scheduling run, or one computed for a
                     // different host if something has - and a dynamic limit is host
                     // dependent by definition, so both are wrong (CS-2740). The consumable
                     // branch above and the sequential path in rqs_limitation_reached() both
                     // evaluate the limit before using it.
                     if (!rqs_set_dynamical_limit(limit, ocs::tom::ConstElement<ocs::tom::EH>::from_typed(a->gep), exec_host,
                                                  ocs::tom::ConstList<ocs::tom::CE>::from_typed(a->centry_list))) {
                        result = DISPATCH_NEVER_CAT;
                        break;
                     }

                     // compare_complexes() takes the relation operator from its second
                     // argument. Passing the job request there took it from an element which
                     // never carries one - centry_list_fill_request() copies CE_name,
                     // CE_valtype and CE_consumable into a request and nothing else - so the
                     // operator was 0, resource_cmp() fell through to its default of "no match"
                     // and every parallel job requesting a non consumable limited complex was
                     // rejected, whatever the values (CS-2739). raw_centry is the complex
                     // definition and does carry the operator, so it belongs in the second
                     // position, which is also the order every other caller uses.
                     //
                     // Both values have to be set: compare_complexes() compares against
                     // CE_pj_doubleval and against CE_doubleval and ANDs the two results.
                     ocs::tom::set<CE_stringval>(raw_centry, ocs::tom::get<RQRL_value>(limit));
                     ocs::tom::set<CE_doubleval>(raw_centry, ocs::tom::get<RQRL_dvalue>(limit));
                     ocs::tom::set<CE_pj_doubleval>(raw_centry, ocs::tom::get<RQRL_dvalue>(limit));
                     if (compare_complexes(1, job_centry, raw_centry, availability_text, false, false) != 1) {
                        result = DISPATCH_NEVER_CAT;
                        break;
                     }
                  }

               }

               DPRINTF("parallel_rqs_slots_by_time(%s@%s) result %d slots %d for " SFQ " (fresh)\n",
                       queue, host, result, ttslots, limit_s);

               /* store result for reuse */
               if (!a->limit_list) {
                  a->limit_list = ocs::tom::ListPtr<ocs::tom::RQL>::create("");
               }
               rql = a->limit_list->emplace_back<RQL_name>(limit_s);
               ocs::tom::set<RQL_result>(rql, result);
               ocs::tom::set<RQL_slots>(rql, ttslots);
               ocs::tom::set<RQL_tagged4schedule>(rql, ocs::tom::get<QU_tagged4schedule>(qep));

               /* reset QU_tagged4schedule if necessary */
               ocs::tom::and_bits<QU_tagged4schedule>(qep, tagged_for_schedule_old);

               tslots = std::min(tslots, ttslots);
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

   DPRINTF("parallel_rqs_slots_by_time(%s@%s) final result %d slots %d\n", queue, host, result, *slots);

   DRETURN(result);
}

/**
 * @brief Match resource quota rule any queue instance
 *
 * Check whether a resource quota rule can match any queue instance. If
 * if does not match due to users/projects/pes scope one can rule this
 * out.
 * Note: As long as rqs_match_assignment() is not used for parallel jobs
 *       passing nullptr as PE request is perfectly fine.
 *
 * @param rule Resource quota rule
 * @param a Scheduler assignment
 *
 * @return True if it matches
 *
 * @note MT-NOTE: rqs_match_assignment() is MT safe
 */
static bool rqs_match_assignment(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   return (rqs_filter_match(ocs::tom::get<RQR_filter_projects>(rule), FILTER_PROJECTS, a->project, nullptr, nullptr, nullptr, nullptr) &&
           rqs_filter_match(ocs::tom::get<RQR_filter_users>(rule), FILTER_USERS, a->user, a->acl_list, nullptr, a->group, a->grp_list) &&
           rqs_filter_match(ocs::tom::get<RQR_filter_pes>(rule), FILTER_PES, nullptr, nullptr, nullptr, nullptr, nullptr))?true:false;
}

/**
 * @brief Poke whether a queue/host negation can be made
 *
 * A global limit was hit with 'rule'. This function helps to determine
 * to what exend we can profit from that situation. If there is no
 * previous matching rule within the same rule set any other queue/host
 * can be skipped.
 *
 * @param rule Rule
 * @param host Any previous rule with a host scope?
 * @param queue Any previous rule with a queue scope?
 * @param a Scheduler assignment
 *
 * @note MT-NOTE: rqs_can_optimize() is MT safe
 */
static void rqs_can_optimize(const ocs::tom::ConstElement<ocs::tom::RQR> rule, bool *host, bool *queue, sge_assignment_t *a) {
   bool host_shadowed = false, queue_shadowed = false;

   for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
      // @todo CS-3104 tests the current rule instead of prev
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

/**
 * @brief Determine RQS limit slot amount and debit
 *
 * The function determines the final slot amount due
 * to all resource quota limitations that apply for the queue instance.
 * Both slot amounts get debited from the a->limit_list to keep track
 * of still available amounts per resource quota limit.
 *
 * @param a Assignment data structure
 * @param host hostname
 * @param queue queuename
 * @param slots needed/available slots
 * @param rule_name caller maintained buffer
 * @param rue_name caller maintained buffer
 * @param limit_name caller maintained buffer
 *
 * @note MT-NOTE: check_and_debit_rqs_slots() is MT safe
 */
void
parallel_check_and_debit_rqs_slots(sge_assignment_t *a, const char *host, const char *queue, int *slots,
                                   dstring *rule_name, dstring *rue_name, dstring *limit_name) {
   DENTER(TOP_LAYER);
   const char* user = a->user;
   const char* group = a->group;
   const lList *grp_list = a->grp_list;
   const char* project = a->project;
   const char* pe = a->pe_name;

   /* first step - see how many slots are left */
   for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {

      /* ignore disabled rule sets */
      if (!ocs::tom::get<RQS_enabled>(rqs)) {
         continue;
      }
      sge_dstring_clear(rule_name);
      const auto rule = ocs::tom::ConstElement<ocs::tom::RQR>::from_typed(
         rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list,
                               rule_name));
      if (rule != nullptr) {
         rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
         if (const auto rql = ocs::tom::find_first<RQL_name>(*a->limit_list, sge_dstring_get_string(limit_name));
             rql != nullptr) {
            *slots = std::min(*slots, ocs::tom::get<RQL_slots>(rql));
         } else {
            *slots = 0;
         }
      }

      // The current rqs did not match - cannot use the given host / cqueue.
      if (*slots == 0) {
         break;
      }
   }

   // All matching RQSes had enough resources free.
   // Second step - do the booking - reduce the number of remaining slots.
   if (*slots != 0) {
      for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {

         // ignore disabled rule sets
         if (!ocs::tom::get<RQS_enabled>(rqs)) {
            continue;
         }

         sge_dstring_clear(rule_name);
         const auto rule = ocs::tom::ConstElement<ocs::tom::RQR>::from_typed(
            rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list,
                                  rule_name));
         if (rule != nullptr) {
            rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
            sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
            const auto rql =
                  ocs::tom::find_first_rw<RQL_name>(*a->limit_list, sge_dstring_get_string(limit_name));
            ocs::tom::set<RQL_slots>(rql, ocs::tom::get<RQL_slots>(rql) - *slots);
         }
      }
   }

   DPRINTF("check_and_debit_rqs_slots(%s@%s) slots: %d\n", queue, host, *slots);

   DRETURN_VOID;
}

/**
 * @brief Gives back slots that were tentatively debited on the resource quotas
 *
 * The parallel matching debits the quotas as it goes so that the next queue
 * instance sees the reduced limit. When the attempt fails the bookings have
 * to be undone, which is what this does.
 *
 * @param[in,out] a          the assignment carrying the limit list
 * @param[in]     host       the host the slots were debited for
 * @param[in]     queue      the queue instance the slots were debited for
 * @param[in]     slots      number of slots to give back
 * @param[in,out] rule_name  scratch buffer for the rule name
 * @param[in,out] rue_name   scratch buffer for the usage name
 * @param[in,out] limit_name scratch buffer for the combined limit name
 */
void
parallel_revert_rqs_slot_debitation(sge_assignment_t *a, const char *host, const char *queue,
                                    int slots, dstring *rule_name, dstring *rue_name, dstring *limit_name) {
   DENTER(TOP_LAYER);

   for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {

      // ignore disabled rule sets
      if (!ocs::tom::get<RQS_enabled>(rqs)) {
         continue;
      }

      // find the rule
      sge_dstring_clear(rule_name);
      const char* user = a->user;
      const char* group = a->group;
      const lList *grp_list = a->grp_list;
      const char* project = a->project;
      const char* pe = a->pe_name;
      const auto rule = ocs::tom::ConstElement<ocs::tom::RQR>::from_typed(
         rqs_get_matching_rule(rqs, user, group, grp_list, project, pe, host, queue, a->acl_list, a->hgrp_list,
                               rule_name));
      if (rule != nullptr) {
         rqs_get_rue_string(rue_name, rule, user, project, host, queue, pe);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_name));
         const auto rql = ocs::tom::find_first_rw<RQL_name>(*a->limit_list, sge_dstring_get_string(limit_name));
         DPRINTF("limit: %s %d <--- %d\n", sge_dstring_get_string(limit_name), ocs::tom::get<RQL_slots>(rql), ocs::tom::get<RQL_slots>(rql)+slots);
         ocs::tom::set<RQL_slots>(rql, ocs::tom::get<RQL_slots>(rql) + slots);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Check queue instance suitability due to RQS
 *
 * Checks (or determines earliest time) queue instance suitability
 * according to resource quota set limits.
 * For performance reasons RQS verification results are cached in
 * a->limit_list. In addition unsuited queues and hosts are collected
 * in a->skip_cqueue_list and a->skip_host_list so that ruling out
 * chunks of queue instance becomes quite cheap.
 *
 * @param a assignment
 * @param queue cluster queue name
 * @param host host name
 * @param tt_rqs_all returns earliest time over all resource quotas
 * @param is_global returns true if result is valid for any other queue
 * @param rue_string caller maintained buffer
 * @param limit_name caller maintained buffer
 * @param rule_name caller maintained buffer
 * @param tt_best time of best solution found so far
 *
 * @return usual return values
 *
 * @note MT-NOTE: rqs_by_slots() is MT safe
 */
dispatch_t rqs_by_slots(sge_assignment_t *a, const char *queue, const char *host,
                        uint64_t *tt_rqs_all, bool *is_global, dstring *rue_string, dstring *limit_name, dstring *rule_name, uint64_t tt_best) {
   DENTER(TOP_LAYER);

   dispatch_t result = DISPATCH_OK;

   *is_global = false;

   if (lGetNumberOfElem(a->rqs_list) > 0) {
      SCHED_PROF_INC(a->pi, seq_rqs);
   }

   bool stopped = false;   // the walk stopped at a rule set that denies the job
   for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {
      uint64_t tt_rqs = a->start;
      const char *user = a->user;
      const char *group = a->group;
      const lList *grp_list = a->grp_list;
      const char *project = a->project;

      if (!ocs::tom::get<RQS_enabled>(rqs)) {
         continue;
      }

      sge_dstring_clear(rule_name);
      const auto rule = ocs::tom::Element<ocs::tom::RQR>::from_typed(
         rqs_get_matching_rule(rqs, user, group, grp_list, project, nullptr, host, queue, a->acl_list, a->hgrp_list,
                               rule_name));
      if (rule != nullptr) {
         const char *limit;
         ocs::tom::Element<ocs::tom::RQL> rql;

         /* need unique identifier for cache */
         rqs_get_rue_string(rue_string, rule, user, project, host, queue, nullptr);
         sge_dstring_sprintf(limit_name, "%s=%s", sge_dstring_get_string(rule_name), sge_dstring_get_string(rue_string));
         limit = sge_dstring_get_string(limit_name);

         /* check limit or reuse earlier results */
         if ((rql=ocs::tom::find_first_rw<RQL_name>(*a->limit_list, limit))) {
            tt_rqs = ocs::tom::get<RQL_time>(rql);
            result = (dispatch_t)ocs::tom::get<RQL_result>(rql);
         } else {
            /* Check booked usage */
            result = rqs_limitation_reached(a, rule, host, queue, &tt_rqs);

            if (!a->limit_list) {
               a->limit_list = ocs::tom::ListPtr<ocs::tom::RQL>::create("");
            }
            rql = a->limit_list->emplace_back<RQL_name>(limit);
            ocs::tom::set<RQL_result>(rql, result);
            ocs::tom::set<RQL_time>(rql, tt_rqs);
            /* init with same value as QU_tagged4schedule */
            ocs::tom::set<RQL_tagged4schedule>(rql, 2);

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
         if (result != DISPATCH_OK) {
            stopped = true;
            break;
         }

         if (a->is_reservation && tt_rqs >= tt_best) {
            /* no need to further investigate these ones */
            if (rqs_exceeded_sort_out(a, rule, rule_name, queue, host))
               *is_global = true;
         }

         *tt_rqs_all = std::max(*tt_rqs_all, tt_rqs);
      }
   }

   if (!stopped) {
      result = DISPATCH_OK;
   }

   DSTRING_STATIC(dstr, 64);
   if (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR) {
      DPRINTF("rqs_by_slots(%s@%s) returns <at specified time> %s (" sge_u64 ")\n", queue, host, sge_ctime64(*tt_rqs_all, &dstr), *tt_rqs_all);
   } else {
      DPRINTF("rqs_by_slots(%s@%s) returns <later> %s (" sge_u64 ") (%s)\n", queue, host, sge_ctime64(*tt_rqs_all, &dstr), *tt_rqs_all, *is_global?"global":"not global");
   }

   DRETURN(result);
}

/**
 * @brief Add all matching cqueues to the list
 *
 * The names of all cluster queues that match the rule are added to
 * the skip list without duplicates.
 *
 * @param rule the rule (`RQR_Type`)
 * @param a the assignment whose `skip_cqueue_list` is extended
 *
 * @note MT-NOTE: rqs_expand_cqueues() is not MT safe
 */
static void rqs_expand_cqueues(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   DENTER(TOP_LAYER);

   const auto qfilter = ocs::tom::get<RQR_filter_queues>(rule);

   for (const auto cq : ocs::tom::ConstList<ocs::tom::CQ>::from_typed(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE))) {
      const char *cqname = ocs::tom::get<CQ_name>(cq);
      if (ocs::tom::find_first<CTI_name>(*a->skip_cqueue_list, cqname))
         continue;
      if (rqs_filter_match(qfilter, FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr) &&
          !cqueue_shadowed_by(cqname, rule, a)) {
         if (!a->skip_cqueue_list) {
            a->skip_cqueue_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_cqueue_list->emplace_back<CTI_name>(cqname);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Add all matching hosts to the list
 *
 * The names of all hosts that match the rule are added to
 * the skip list without duplicates.
 *
 * @param rule the rule (`RQR_Type`)
 * @param a the assignment whose `skip_host_list` is extended
 *
 * @note MT-NOTE: rqs_expand_hosts() is MT safe
 */
static void rqs_expand_hosts(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   DENTER(TOP_LAYER);
   const auto hfilter = ocs::tom::get<RQR_filter_hosts>(rule);

   for (const auto eh : ocs::tom::ConstList<ocs::tom::EH>(a->host_list)) {
      const char *hname = ocs::tom::get<EH_name>(eh);
      if (ocs::tom::find_first<CTI_name>(*a->skip_host_list, hname))
         continue;
      if (rqs_filter_match(hfilter, FILTER_HOSTS, hname, nullptr, a->hgrp_list, nullptr, nullptr) &&
          !host_shadowed_by(hname, rule, a)) {
         if (!a->skip_host_list) {
            a->skip_host_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_host_list->emplace_back<CTI_name>(hname);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Check for cluster queue rule before current rule
 *
 * Check whether there is any cluster queue specific rule before the
 * current rule.
 *
 * @code
 * limit queue Q001 to F001=1
 * limit host gridware to F001=0  (--> returns 'true' due to 'Q001' meaning
 *                           that gridware can't be generelly ruled out )
 * @endcode
 *
 * @param rule Current rule
 * @param a Scheduler assignment
 *
 * @return True if shadowed
 *
 * @note MT-NOTE: cqueue_shadowed() is MT safe
 */
static bool cqueue_shadowed(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
      if (rqs_match_assignment(prev, a) && !is_cqueue_global(prev)) {
         return true;
      }
   }
   return false;
}

/**
 * @brief Check for host rule before current rule
 *
 * Check whether there is any host specific rule before the
 * current rule.
 *
 * @code
 * limit host gridware to F001=1
 * limit queue Q001 to F001=0  (--> returns 'true' due to 'gridware' meaning
 *                           that Q001 can't be generelly ruled out )
 * @endcode
 *
 * @param rule Current rule
 * @param a Scheduler assignment
 *
 * @return True if shadowed
 *
 * @note MT-NOTE: host_shadowed() is MT safe
 */
static bool host_shadowed(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
      if (rqs_match_assignment(prev, a) && !is_host_global(prev)) {
         return true;
      }
   }
   return false;
}

/**
 * @brief Find excluded queues
 *
 * Find queues that are excluded by previous rules.
 *
 * @code
 *  limit        projects {*} queues !Q001 to F001=1
 *  limit        to F001=0   ( ---> returns Q001 in a->skip_cqueue_list)
 * @endcode
 *
 * @param rule The rule
 * @param a Scheduler assignement
 *
 * @note MT-NOTE: rqs_excluded_cqueues() is MT safe
 */
static void rqs_excluded_cqueues(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   DENTER(TOP_LAYER);

   int ignored = 0, excluded = 0;

   for (const auto cq : ocs::tom::ConstList<ocs::tom::CQ>::from_typed(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE))) {
      const char *cqname = ocs::tom::get<CQ_name>(cq);
      bool exclude = true;

      if (ocs::tom::find_first<CTI_name>(*a->skip_cqueue_list, cqname)) {
         ignored++;
         continue;
      }

      for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
         // @todo CS-3104 tests the current rule instead of prev
         if (!rqs_match_assignment(rule, a))
            continue;

         if (rqs_filter_match(ocs::tom::get<RQR_filter_queues>(prev), FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr)) {
            exclude = false;
            break;
         }
      }
      if (exclude) {
         if (!a->skip_cqueue_list) {
            a->skip_cqueue_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_cqueue_list->emplace_back<CTI_name>(cqname);
         excluded++;
      }
   }

   if (ignored + excluded == 0) {
      CRITICAL("not a single queue excluded in rqs_excluded_cqueues()\n");
   }

   DRETURN_VOID;
}

/**
 * @brief Find excluded hosts
 *
 * Find hosts that are excluded by previous rules.
 *
 * @code
 *  limit        projects {*} queues !gridware to F001=1
 *  limit        to F001=0   ( ---> returns gridware in skip_host_list)
 * @endcode
 *
 * @param rule The rule
 * @param a Scheduler assignement
 *
 * @note MT-NOTE: rqs_excluded_hosts() is MT safe
 */
static void rqs_excluded_hosts(const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   DENTER(TOP_LAYER);

   int ignored = 0, excluded = 0;

   for (const auto eh : ocs::tom::ConstList<ocs::tom::EH>(a->host_list)) {
      const char *hname = ocs::tom::get<EH_name>(eh);
      bool exclude = true;

      if (ocs::tom::find_first<CTI_name>(*a->skip_host_list, hname)) {
         ignored++;
         continue;
      }

      for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
         // @todo CS-3104 tests the current rule instead of prev
         if (!rqs_match_assignment(rule, a))
            continue;

         if (rqs_filter_match(ocs::tom::get<RQR_filter_hosts>(prev), FILTER_HOSTS, hname, nullptr, a->hgrp_list, nullptr, nullptr)) {
            exclude = false;
            break;
         }
      }
      if (exclude) {
         if (!a->skip_host_list) {
            a->skip_host_list = ocs::tom::ListPtr<ocs::tom::CTI>::create("");
         }
         a->skip_host_list->emplace_back<CTI_name>(hname);
         excluded++;
      }
   }

   if (ignored + excluded == 0) {
      CRITICAL("not a single host excluded in rqs_excluded_hosts()\n");
   }

   DRETURN_VOID;
}

/**
 * @brief Check rules shadowing current cluster queue rule
 *
 * Check if cluster queue in current rule is shadowed.
 *
 * @code
 * limits queues Q001,Q002 to F001=1
 * limits queues Q002,Q003 to F001=1 (--> returns 'true' for Q002 and 'false' for Q003)
 * @endcode
 *
 * @param cqname Cluster queue name to check
 * @param rule Current rule
 * @param a Assignment
 *
 * @return True if shadowed
 *
 * @note MT-NOTE: cqueue_shadowed_by() is MT safe
 */
static bool cqueue_shadowed_by(const char *cqname, const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
      if (rqs_match_assignment(prev, a) &&
          rqs_filter_match(ocs::tom::get<RQR_filter_queues>(prev), FILTER_QUEUES, cqname, nullptr, nullptr, nullptr, nullptr)) {
         return true;
      }
   }

   return false;
}

/**
 * @brief Check if host in current rule is shadowed
 *
 * Check if host in current rule is shadowed.
 *
 * @code
 * limits hosts host1,host2 to F001=1
 * limits hosts host2,host3 to F001=1 (--> returns 'true' for host2 and 'false' for host3)
 * @endcode
 *
 * @param host Host name to check
 * @param rule Current rule
 * @param a Assignment
 *
 * @return True if shadowed
 *
 * @note MT-NOTE: host_shadowed_by() is MT safe
 */
static bool host_shadowed_by(const char *host, const ocs::tom::ConstElement<ocs::tom::RQR> rule, sge_assignment_t *a) {
   for (auto prev = rule.prev(); prev != nullptr; prev = prev.prev()) {
      if (rqs_match_assignment(prev, a) &&
          rqs_filter_match(ocs::tom::get<RQR_filter_hosts>(prev), FILTER_HOSTS, host, nullptr, a->hgrp_list, nullptr, nullptr)) {
         return true;
      }
   }

   return false;
}

/**
 * @brief Evaluate dynamical limit
 *
 * The function evaluates if neccessary the dynamical limit for a host and
 * sets the evaluated double value in the given limitation element (RQRL_dvalue).
 * A evaluation is neccessary if the limit boolean RQRL_dynamic is true. This
 * field is set by qmaster during the rule set verification
 *
 * @param limit limitation (RQRL_Type)
 * @param global_host global host (EH_Type)
 * @param exec_host exec host (EH_Type)
 * @param centry consumable resource list (CE_Type)
 *
 * @return always true
 *
 * @note MT-NOTE: rqs_set_dynamical_limit() is MT safe
 */
static bool
rqs_set_dynamical_limit(const ocs::tom::Element<ocs::tom::RQRL> limit, const ocs::tom::ConstElement<ocs::tom::EH> global_host,
                        const ocs::tom::ConstElement<ocs::tom::EH> exec_host, const ocs::tom::ConstList<ocs::tom::CE> centry) {
   DENTER(TOP_LAYER);

   if (ocs::tom::get<RQRL_dynamic>(limit)) {
      double dynamic_limit = scaled_mixed_load(ocs::tom::get<RQRL_value>(limit), global_host, exec_host, centry,
                                                 ocs::tom::ConstList<ocs::tom::CE>());
      DPRINTF("found a dynamic limit for host %s with value %d\n", static_cast<const char *>(ocs::tom::get<EH_name>(exec_host)), (int)dynamic_limit);
      ocs::tom::set<RQRL_dvalue>(limit, dynamic_limit);
   }

   DRETURN(true);
}

/**
 * @brief The slots free within the resource quotas, for a caller with an untyped queue instance (N-X-2)
 *
 * @param a job info structure
 * @param slots out: # free slots
 * @param qep queue instance (`QU_Type`), checked
 * @param need_master a master task still has to be placed
 * @param is_master_queue this queue instance already carries the master task
 *
 * @return #DISPATCH_OK or #DISPATCH_NEVER_CAT
 */
dispatch_t parallel_rqs_slots_by_time(sge_assignment_t *a, int *slots, lListElem *qep, bool need_master,
                                      bool is_master_queue) {
   return parallel_rqs_slots_by_time(a, slots, ocs::tom::Element<ocs::tom::QU>(qep), need_master, is_master_queue);
}
