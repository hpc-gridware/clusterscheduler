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
 * @brief QETI - the queue end time iterator
 *
 * To find out when a job could start, the scheduler has to know at which
 * points in time the utilization of the resources it needs drops. Those
 * points are the end times of the jobs currently holding them, and only
 * those - between two of them nothing changes, so nothing has to be checked.
 *
 * A QETI iterates exactly over those instants, in **descending** order, for
 * one job: sge_qeti_first() gives the latest one and sge_qeti_next() walks
 * backwards towards the present, which is how the scheduler finds the
 * earliest start time rather than just any.
 */

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"

#include "sge_qeti.h"
#include "sge_resource_utilization.h"
#include "sge_select_queue.h"

/* At that point in time we only keep references to in the iterator that
 * allow for efficiently iterating through relevant queue end times in
 * that affect resource utilization of a particular job.
 *
 * Further improvements with sge_qeti_t might allow to notably reduce
 * the time for the actual resource selection: It might be useful for
 * example to enhance sge_qeti_t towards keeping all information required
 * for decide very quickly about eligibility of each host/queue.
 */
/**
 * @brief The references an iterator keeps, one list per resource level
 *
 * Only references are kept, not copies: what is iterated is the resource
 * utilization of the objects the job could run on.
 */
struct sge_qeti_s {
   lList *cr_refs_pe;       ///< Utilization of the parallel environment
   lList *cr_refs_global;   ///< Utilization of the global host
   lList *cr_refs_host;     ///< Utilization of the eligible execution hosts
   lList *cr_refs_queue;    ///< Utilization of the eligible queue instances
};

/*
 *  Assume a parallel job with a license and h_vmem request. At the time
 *  when we seek for reservation the changes with the resource utilization
 *  diagrams relevant for this job are marked with a '+'
 *
 *  After initializing the time iterator using sge_qeti_allocate() the
 *  iterator keeps references to all resource instances (QETI_resource_instance)
 *  shown in the diagram below and all queue end next (QETI_queue_end_next)
 *  references refer to the very end of those resource diagrams.
 *
 *  mpi_pe  slots      +------+-----------+
 *  Global  license    +------+-----------+-----+
 *  Host1   h_vmem     +------+-----------+
 *  Host2   h_vmem     +------+-----------+
 *  Queue1  slots      +------+---+-------+-----+
 *  Queue2  slots   +--+------+---+-------+
 *                  6  5      4   3       2     1
 *
 *  After sge_qeti_first() returned time mark #1 the queue end next
 *  references for Global license and the Queue1 slot point to #2.
 *  Then after sge_qeti_next() returned time mark #2 the queue end
 *  next references for Queue1/2 slot point to #3 whereas all remaining
 *  queue end next references point to #4 ...
 *
 */



/**
 * @brief Adds a resource utilization to QETI resource list
 *
 * @param lpp QETI resource list
 * @param name Name of the resource
 * @param rue_lp Resource utilization entry (RUE_Type)
 * @param total Total resource amount
 * @param must_exist If true the entry must exist in 'lpp'.
 *
 * @return 0 on success
 *
 * @note MT-NOTE: sge_qeti_list_add() is not MT safe
 */
static int sge_qeti_list_add(lList **lpp, const char *name, const lList* rue_lp, double total, bool must_exist)
{
   lListElem *tmp_cr_ref;
   lListElem *ep;

   DENTER(TOP_LAYER);

   if (!(tmp_cr_ref = lGetElemStrRW(rue_lp, RUE_name, name))) {
      DRETURN(must_exist?-1:0);
   }

   if (!*lpp && !(*lpp = lCreateList("pe_qeti", QETI_Type))) {
      DRETURN(-1);
   }

   if (!(ep = lCreateElem(QETI_Type))) {
      lFreeList(lpp);
      DRETURN(-1);
   }

   lSetRef(ep, QETI_resource_instance, tmp_cr_ref);
   lSetDouble(ep, QETI_total, total);
   lAppendElem(*lpp, ep);

   DRETURN(0);
}

static int
sge_add_qeti_resource_container(lList **qeti_to_add, const lList* rue_list,
                                const lList* total_list, const lList* centry_list, const lListElem *job)
{

   DENTER(TOP_LAYER);

   // implicit slots request
   const lListElem *tep = lGetElemStr(total_list, CE_name, SGE_ATTR_SLOTS);
   if (tep != nullptr) {
      if (sge_qeti_list_add(qeti_to_add, SGE_ATTR_SLOTS, rue_list, lGetDouble(tep, CE_doubleval), true)) {
         DRETURN(-1);
      }
   }

   /* explicit requests */
   for_each_ep_lv(jrs, lGetList(job, JB_request_set_list)) {
      for_each_ep_lv(req, lGetList(jrs, JRS_hard_resource_list)) {
         const char *name = lGetString(req, CE_name);
         const lListElem *centry_config = lGetElemStr(centry_list, CE_name, name);

         if ((centry_config && lGetUlong(centry_config, CE_consumable) != CONSUMABLE_NO) &&
             (tep = lGetElemStr(total_list, CE_name, name))) {
            if (sge_qeti_list_add(qeti_to_add, name, rue_list, lGetDouble(tep, CE_doubleval), false)) {
               DRETURN(-1);
            }
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Creates an iterator over one resource utilization list
 *
 * @param[in] cr_list the resource utilization list to iterate
 *
 * @return the iterator, or nullptr if it could not be allocated
 *
 * @note Only used by the module test.
 */
sge_qeti_t *sge_qeti_allocate2(lList *cr_list)
{
   sge_qeti_t *iter;

   if (!(iter = (sge_qeti_t *)calloc(1, sizeof(sge_qeti_t)))) {
      return nullptr;
   }

   sge_qeti_list_add(&iter->cr_refs_pe, SGE_ATTR_SLOTS, cr_list, 10, true);
   return iter;
}

/**
 * @brief Creates an iterator over the relevant queue end times of a job
 *
 * Collects the utilization lists of every level the job takes resources from
 * - PE, global, hosts, queue instances - so that the iterator only has to
 * walk what can actually change for this job.
 *
 * @param[in] a the assignment describing the job and what it requests
 *
 * @return the iterator, or nullptr if it could not be allocated
 */
sge_qeti_t *sge_qeti_allocate(sge_assignment_t *a)
{
   DENTER(TOP_LAYER);

   sge_qeti_t *iter = (sge_qeti_t *)calloc(1, sizeof(sge_qeti_t));
   if (iter == nullptr) {
      DRETURN(nullptr);
   }

   if (a->ar_id == 0) {
      // add "slot" resource utilization entry of parallel environment
      // when running within an AR, slots are limited on the queue level, no need to add them here
      if (sge_qeti_list_add(&iter->cr_refs_pe, SGE_ATTR_SLOTS,
                     lGetList(a->pe, PE_resource_utilization), lGetUlong(a->pe, PE_slots), true)) {
         sge_qeti_release(&iter);
         DRETURN(nullptr);
      }
   }

   // add references to global resource utilization entries that might affect jobs queue end time
   if (a->gep != nullptr) {
      if (sge_add_qeti_resource_container(&iter->cr_refs_global,
               lGetList(a->gep, EH_resource_utilization),
               lGetList(a->gep, EH_consumable_config_list),
               a->centry_list, a->job)!=0) {
         sge_qeti_release(&iter);
         DRETURN(nullptr);
      }
   }

   // add references to per host resource utilization entries that might affect jobs queue end time
   for_each_ep_lv(hep, a->host_list) {
      const char *eh_name = lGetHost(hep, EH_name);
      // we already handled the global host above
      if (hep == a->gep) {
         continue;
      }

      if (sge_host_match_static(a, hep) == DISPATCH_NEVER_CAT) {
         continue;
      }

      // There must be at least one queue referenced with the parallel
      // environment that resides at this host. And secondly we only
      // consider those hosts that match this job (statically)
      int is_relevant = false;
      const void *queue_iterator = nullptr;
      lListElem *next_queue, *qep;
      for (next_queue = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator);
          (qep = next_queue);
           next_queue = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator)) {

         if (!qinstance_is_pe_referenced(qep, a->pe)) {
            continue;
         }

         // consider only those queues that match this job (statically)
         if (sge_queue_match_static(a, qep, true) != DISPATCH_OK) {
            continue;
         }

         if (sge_add_qeti_resource_container(&iter->cr_refs_queue,
                  lGetList(qep, QU_resource_utilization), lGetList(qep, QU_consumable_config_list),
                        a->centry_list, a->job)!=0) {
            sge_qeti_release(&iter);
            DRETURN(nullptr);
         }

         is_relevant = true;
      }
      if (is_relevant) {
         if (sge_add_qeti_resource_container(&iter->cr_refs_host, lGetList(hep, EH_resource_utilization),
                                             lGetList(hep, EH_consumable_config_list), a->centry_list, a->job)!=0) {
            sge_qeti_release(&iter);
            DRETURN(nullptr);
         }
      }
   }

   DPRINTF("QETI: P %d G %d H %d Q %d\n", lGetNumberOfElem(iter->cr_refs_pe), lGetNumberOfElem(iter->cr_refs_global),
           lGetNumberOfElem(iter->cr_refs_host), lGetNumberOfElem(iter->cr_refs_queue));

   DRETURN(iter);
}


static void sge_qeti_init_refs(lList *cref_lp)
{
   DENTER(TOP_LAYER);
   const lList *utilization_diagram;
   const lListElem *rue_ep;

   for_each_rw_lv(cr_ep, cref_lp) {
      rue_ep = (const lListElem *)lGetRef(cr_ep, QETI_resource_instance);
      utilization_diagram = lGetList((lListElem *)lGetRef(cr_ep, QETI_resource_instance), RUE_utilized);
      DPRINTF("   QETI INIT: %s %p\n", lGetString(rue_ep, RUE_name), utilization_diagram);
      /* lLast() correctly returns a nullptr reference
         in case of an empty resource utilization diagram */
      lSetRef(cr_ep, QETI_queue_end_next, lLastRW(utilization_diagram));
   }

   DRETURN_VOID;
}

/* an empty resource utilization diagrams actually means the resource
   is available now - thus we can skip it when determining the maximum */
static void sge_qeti_max_end_time(const char *layer, uint64_t *max_time, const lList *cref_lp)
{
   DENTER(TOP_LAYER);
   lListElem *ref;
   uint64_t tmp_time = *max_time;
   lListElem *rue_ep;
   DSTRING_STATIC(time_str1, 64);
   DSTRING_STATIC(time_str2, 64);

   for_each_ep_lv(cr_ep, cref_lp) {
      rue_ep = (lListElem *)lGetRef(cr_ep, QETI_resource_instance);
      if (!(ref = (lListElem *)lGetRef(cr_ep, QETI_queue_end_next))) {
         DPRINTF("   QETI END %s: %s\n", layer, lGetString(rue_ep, RUE_name));
         continue;
      }
      DPRINTF("   QETI END %s: %s %s (%s)\n",
              layer, lGetString(rue_ep, RUE_name),
              sge_ctime64(lGetUlong64(ref, RDE_time), &time_str1),
              sge_ctime64(tmp_time, &time_str2));
      tmp_time = std::max(tmp_time, lGetUlong64(ref, RDE_time));
   }
   *max_time = tmp_time;

   DRETURN_VOID;
}

/* switch queue end next references to the next entry
   whose time is larger or equal the specified time */
static void sge_qeti_switch_to_next(const char *layer, uint64_t time, lList *cref_lp)
{
   DENTER(TOP_LAYER);
   lListElem *ref;
   lListElem *rue_ep;
   DSTRING_STATIC(time_str, 64);

   time--;
   for_each_rw_lv(cr_ep, cref_lp) {
      rue_ep = (lListElem *)lGetRef(cr_ep, QETI_resource_instance);
      if (!(ref = (lListElem *)lGetRef(cr_ep, QETI_queue_end_next))) {
         DPRINTF("   QETI NEXT %s: %s (finished)\n", layer, lGetString(rue_ep, RUE_name));
         continue;
      }

      while (ref && time < lGetUlong64(ref, RDE_time)) {
         ref = lPrevRW(ref);
      }

      DPRINTF("   QETI NEXT %s: %s set to %s (%p)\n", layer, lGetString(rue_ep, RUE_name),
              sge_ctime64(ref != nullptr ? lGetUlong64(ref, RDE_time) : 0, &time_str), ref);
      lSetRef(cr_ep, QETI_queue_end_next, ref);
   }

   DRETURN_VOID;
}

/**
 * @brief All queue end next references are set in a way that will
 *
 * @param qeti
 * @param start
 *
 * @note MT-NOTE: sge_qeti_next_before() is MT safe
 */
void sge_qeti_next_before(sge_qeti_t *qeti, uint64_t start)
{
   sge_qeti_switch_to_next("P", start, qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", start, qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", start, qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", start, qeti->cr_refs_queue);
}


/**
 * @brief Rewinds the iterator and returns the latest queue end time
 *
 * All levels are switched to their last entry, so the following
 * sge_qeti_next() calls walk backwards from here towards the present.
 *
 * @param[in,out] qeti the iterator
 *
 * @return the latest time at which the utilization of a resource of this job
 *         changes
 *
 * @note MT-NOTE: sge_qeti_first() is MT safe
 */
uint64_t sge_qeti_first(sge_qeti_t *qeti)
{
   DENTER(TOP_LAYER);
   uint64_t all_resources_queue_end_time = 0;
   DSTRING_STATIC(time_str, 64);

   /* (re)init all queue end next references */
   sge_qeti_init_refs(qeti->cr_refs_pe);
   sge_qeti_init_refs(qeti->cr_refs_global);
   sge_qeti_init_refs(qeti->cr_refs_host);
   sge_qeti_init_refs(qeti->cr_refs_queue);

   /* determine all resources queue end time */
   sge_qeti_max_end_time("P", &all_resources_queue_end_time, qeti->cr_refs_pe);
   sge_qeti_max_end_time("G", &all_resources_queue_end_time, qeti->cr_refs_global);
   sge_qeti_max_end_time("H", &all_resources_queue_end_time, qeti->cr_refs_host);
   sge_qeti_max_end_time("Q", &all_resources_queue_end_time, qeti->cr_refs_queue);

   DPRINTF("sge_qeti_first() determines %s\n", sge_ctime64(all_resources_queue_end_time, &time_str));

   /* switch to the next entry with all queue end next references whose
      time is larger (?) or equal to all resources queue end time */
   sge_qeti_switch_to_next("P", all_resources_queue_end_time, qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", all_resources_queue_end_time, qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", all_resources_queue_end_time, qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", all_resources_queue_end_time, qeti->cr_refs_queue);

   DRETURN(all_resources_queue_end_time);
}

/**
 * @brief Returns the next earlier queue end time
 *
 * @param[in,out] qeti the iterator
 *
 * @return the next time at which the utilization changes, or
 *         `DISPATCH_TIME_NOW` once the iterator has reached the present
 *
 * @note MT-NOTE: sge_qeti_next() is MT safe
 */
uint64_t sge_qeti_next(sge_qeti_t *qeti)
{
   DENTER(TOP_LAYER);
   uint64_t all_resources_queue_end_time = DISPATCH_TIME_NOW;
   DSTRING_STATIC(time_str, 64);

   /* determine all resources queue end time */
   sge_qeti_max_end_time("P", &all_resources_queue_end_time, qeti->cr_refs_pe);
   sge_qeti_max_end_time("G", &all_resources_queue_end_time, qeti->cr_refs_global);
   sge_qeti_max_end_time("H", &all_resources_queue_end_time, qeti->cr_refs_host);
   sge_qeti_max_end_time("Q", &all_resources_queue_end_time, qeti->cr_refs_queue);

   DPRINTF("sge_qeti_next() determines %s\n", sge_ctime64(all_resources_queue_end_time, &time_str));

   /* switch to the next entry with all queue end next references whose
      time is larger (?) or equal to all resources queue end time */
   sge_qeti_switch_to_next("P", all_resources_queue_end_time, qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", all_resources_queue_end_time, qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", all_resources_queue_end_time, qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", all_resources_queue_end_time, qeti->cr_refs_queue);

   DRETURN(all_resources_queue_end_time);
}

/**
 * @brief Release queue end time iterator
 *
 * Release all resources of the queue end time iterator. Refered
 * resource utilization diagrams are not affected.
 *
 * @param qeti
 *
 * @note MT-NOTE: sge_qeti_release() is MT safe
 */
void sge_qeti_release(sge_qeti_t **qeti)
{
   if (qeti == nullptr || *qeti == nullptr) {
      return;
   }

   lFreeList(&((*qeti)->cr_refs_pe));
   lFreeList(&((*qeti)->cr_refs_global));
   lFreeList(&((*qeti)->cr_refs_host));
   lFreeList(&((*qeti)->cr_refs_queue));
   sge_free(qeti);
}
