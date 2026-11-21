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
#include <new>
#include <utility>

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QETI.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_RUE.h"

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
   ocs::tom::ListPtr<ocs::tom::QETI> cr_refs_pe;       ///< Utilization of the parallel environment
   ocs::tom::ListPtr<ocs::tom::QETI> cr_refs_global;   ///< Utilization of the global host
   ocs::tom::ListPtr<ocs::tom::QETI> cr_refs_host;     ///< Utilization of the eligible execution hosts
   ocs::tom::ListPtr<ocs::tom::QETI> cr_refs_queue;    ///< Utilization of the eligible queue instances
};

/**
 * @brief The resource utilization entry a QETI element refers to
 * @param cr_ep the QETI element
 * @return the entry (`QETI_resource_instance`); only this file sets the reference, so it is not checked
 */
static ocs::tom::ConstElement<ocs::tom::RUE> sge_qeti_rue(const ocs::tom::ConstElement<ocs::tom::QETI> cr_ep) {
   DENTER(TOP_LAYER);
   auto ref = static_cast<const lListElem *>(cr_ep.get<QETI_resource_instance>());
   DRETURN(ocs::tom::ConstElement<ocs::tom::RUE>::from_typed(ref));
}

/**
 * @brief The utilization diagram entry a QETI element points to next
 * @param cr_ep the QETI element
 * @return the entry (`QETI_queue_end_next`), or an empty handle once the diagram is walked through; only this
 *         file sets the reference, so it is not checked
 */
static ocs::tom::Element<ocs::tom::RDE> sge_qeti_queue_end_next(const ocs::tom::ConstElement<ocs::tom::QETI> cr_ep) {
   DENTER(TOP_LAYER);
   auto ref = static_cast<lListElem *>(cr_ep.get<QETI_queue_end_next>());
   DRETURN(ocs::tom::Element<ocs::tom::RDE>::from_typed(ref));
}

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
 * @param lp QETI resource list, created with the first entry
 * @param name Name of the resource
 * @param rue_lp the resource utilization entries the one of the resource is taken from
 * @param total Total resource amount
 * @param must_exist If true the entry must exist in @p rue_lp.
 *
 * @return 0 on success, -1 if @p must_exist and @p rue_lp has no entry for the resource
 *
 * @note MT-NOTE: sge_qeti_list_add() is not MT safe
 */
static int sge_qeti_list_add(ocs::tom::ListPtr<ocs::tom::QETI> &lp, const char *name,
                             const ocs::tom::ConstList<ocs::tom::RUE> rue_lp, const double total,
                             const bool must_exist) {
   DENTER(TOP_LAYER);

   auto tmp_cr_ref = ocs::tom::find_first_rw<RUE_name>(rue_lp, name);
   if (!tmp_cr_ref) {
      DRETURN(must_exist ? -1 : 0);
   }

   if (!lp) {
      lp = ocs::tom::ListPtr<ocs::tom::QETI>::create("pe_qeti");
   }

   auto ep = ocs::tom::ElementPtr<ocs::tom::QETI>::create();
   ep->set<QETI_resource_instance>(tmp_cr_ref.raw());
   ep->set<QETI_total>(total);
   lp->push_back(std::move(ep));

   DRETURN(0);
}

/**
 * @brief Adds the resource utilizations of one host or queue instance a job takes resources from
 *
 * These are the slots and every consumable the job requests that the object has a capacity for.
 *
 * @param qeti_to_add the QETI resource list of the level, created with the first entry
 * @param rue_list the resource utilization of the object
 * @param total_list the capacities of the object
 * @param centry_list the complex entries
 * @param job the job
 * @return 0 on success, -1 if the object has no utilization entry for its slots
 */
static int
sge_add_qeti_resource_container(ocs::tom::ListPtr<ocs::tom::QETI> &qeti_to_add,
                                const ocs::tom::ConstList<ocs::tom::RUE> rue_list,
                                const ocs::tom::ConstList<ocs::tom::CE> total_list,
                                const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                                const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   // implicit slots request
   auto tep = total_list.find_first<CE_name>(SGE_ATTR_SLOTS);
   if (tep != nullptr) {
      if (sge_qeti_list_add(qeti_to_add, SGE_ATTR_SLOTS, rue_list, tep.get<CE_doubleval>(), true)) {
         DRETURN(-1);
      }
   }

   /* explicit requests */
   for (auto jrs : job.get<JB_request_set_list>()) {
      for (auto req : jrs.get<JRS_hard_resource_list>()) {
         auto name = req.get<CE_name>();
         auto centry_config = centry_list.find_first<CE_name>(name);

         if ((centry_config && centry_config.get<CE_consumable>() != CONSUMABLE_NO) &&
             (tep = total_list.find_first<CE_name>(name))) {
            if (sge_qeti_list_add(qeti_to_add, name, rue_list, tep.get<CE_doubleval>(), false)) {
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
sge_qeti_t *sge_qeti_allocate2(const ocs::tom::ConstList<ocs::tom::RUE> cr_list) {
   DENTER(TOP_LAYER);

   auto iter = new (std::nothrow) sge_qeti_t{};
   if (iter == nullptr) {
      DRETURN(nullptr);
   }

   sge_qeti_list_add(iter->cr_refs_pe, SGE_ATTR_SLOTS, cr_list, 10, true);
   DRETURN(iter);
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
sge_qeti_t *sge_qeti_allocate(sge_assignment_t *a) {
   DENTER(TOP_LAYER);

   auto iter = new (std::nothrow) sge_qeti_t{};
   if (iter == nullptr) {
      DRETURN(nullptr);
   }

   auto centry_list = ocs::tom::ConstList<ocs::tom::CE>(a->centry_list);
   auto job = ocs::tom::ConstElement<ocs::tom::JB>(a->job);

   if (a->ar_id == 0) {
      // add "slot" resource utilization entry of parallel environment
      // when running within an AR, slots are limited on the queue level, no need to add them here
      if (sge_qeti_list_add(iter->cr_refs_pe, SGE_ATTR_SLOTS, a->pe.get<PE_resource_utilization>(),
                            a->pe.get<PE_slots>(), true)) {
         sge_qeti_release(&iter);
         DRETURN(nullptr);
      }
   }

   // add references to global resource utilization entries that might affect jobs queue end time
   if (a->gep != nullptr) {
      if (sge_add_qeti_resource_container(iter->cr_refs_global, a->gep.get<EH_resource_utilization>(),
                                          a->gep.get<EH_consumable_config_list>(), centry_list, job) != 0) {
         sge_qeti_release(&iter);
         DRETURN(nullptr);
      }
   }

   // add references to per host resource utilization entries that might affect jobs queue end time
   for (auto hep : ocs::tom::ConstList<ocs::tom::EH>(a->host_list)) {
      const char *eh_name = hep.get<EH_name>();
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
      for (auto qep = ocs::tom::find_first_rw<QU_qhostname>(a->queue_list, eh_name, &queue_iterator); qep;
           qep = ocs::tom::find_next_rw<QU_qhostname>(a->queue_list, eh_name, &queue_iterator)) {

         if (!qinstance_is_pe_referenced(qep, a->pe)) {
            continue;
         }

         // consider only those queues that match this job (statically)
         if (sge_queue_match_static(a, qep, true) != DISPATCH_OK) {
            continue;
         }

         if (sge_add_qeti_resource_container(iter->cr_refs_queue, qep.get<QU_resource_utilization>(),
                                             qep.get<QU_consumable_config_list>(), centry_list, job) != 0) {
            sge_qeti_release(&iter);
            DRETURN(nullptr);
         }

         is_relevant = true;
      }
      if (is_relevant) {
         if (sge_add_qeti_resource_container(iter->cr_refs_host, hep.get<EH_resource_utilization>(),
                                             hep.get<EH_consumable_config_list>(), centry_list, job) != 0) {
            sge_qeti_release(&iter);
            DRETURN(nullptr);
         }
      }
   }

   DPRINTF("QETI: P %d G %d H %d Q %d\n", static_cast<int>(iter->cr_refs_pe->size()),
           static_cast<int>(iter->cr_refs_global->size()), static_cast<int>(iter->cr_refs_host->size()),
           static_cast<int>(iter->cr_refs_queue->size()));

   DRETURN(iter);
}


/**
 * @brief Points every element of a QETI list to the end of its utilization diagram
 * @param cref_lp the QETI list of one level
 */
static void sge_qeti_init_refs(const ocs::tom::List<ocs::tom::QETI> cref_lp) {
   DENTER(TOP_LAYER);

   for (auto cr_ep : cref_lp) {
      auto rue_ep = sge_qeti_rue(cr_ep);
      auto utilization_diagram = rue_ep.get<RUE_utilized>();
      DPRINTF("   QETI INIT: %s %p\n", rue_ep.get<RUE_name>(), utilization_diagram.raw());
      /* back() correctly returns an empty handle (a nullptr reference)
         in case of an empty resource utilization diagram; the reference is to a mutable element */
      cr_ep.set<QETI_queue_end_next>(const_cast<lListElem *>(utilization_diagram.back().raw()));
   }

   DRETURN_VOID;
}

/**
 * @brief The latest queue end time a QETI list points to
 *
 * An empty resource utilization diagram actually means the resource is
 * available now - thus it is skipped when determining the maximum.
 *
 * @param layer the name of the level, for the debug output
 * @param max_time in: the maximum so far, out: the maximum including this level
 * @param cref_lp the QETI list of one level
 */
static void sge_qeti_max_end_time(const char *layer, uint64_t *max_time,
                                  const ocs::tom::ConstList<ocs::tom::QETI> cref_lp) {
   DENTER(TOP_LAYER);
   uint64_t tmp_time = *max_time;
   DSTRING_STATIC(time_str1, 64);
   DSTRING_STATIC(time_str2, 64);

   for (auto cr_ep : cref_lp) {
      auto rue_ep = sge_qeti_rue(cr_ep);
      auto ref = sge_qeti_queue_end_next(cr_ep);
      if (!ref) {
         DPRINTF("   QETI END %s: %s\n", layer, rue_ep.get<RUE_name>());
         continue;
      }
      DPRINTF("   QETI END %s: %s %s (%s)\n",
              layer, rue_ep.get<RUE_name>(),
              sge_ctime64(ref.get<RDE_time>(), &time_str1),
              sge_ctime64(tmp_time, &time_str2));
      tmp_time = std::max(tmp_time, ref.get<RDE_time>());
   }
   *max_time = tmp_time;

   DRETURN_VOID;
}

/**
 * @brief Switches the queue end next references to the next entry whose time is larger or equal a time
 * @param layer the name of the level, for the debug output
 * @param time the time
 * @param cref_lp the QETI list of one level
 */
static void sge_qeti_switch_to_next(const char *layer, uint64_t time, const ocs::tom::List<ocs::tom::QETI> cref_lp) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(time_str, 64);

   time--;
   for (auto cr_ep : cref_lp) {
      auto rue_ep = sge_qeti_rue(cr_ep);
      auto ref = sge_qeti_queue_end_next(cr_ep);
      if (!ref) {
         DPRINTF("   QETI NEXT %s: %s (finished)\n", layer, rue_ep.get<RUE_name>());
         continue;
      }

      while (ref && time < ref.get<RDE_time>()) {
         ref = ref.prev();
      }

      DPRINTF("   QETI NEXT %s: %s set to %s (%p)\n", layer, rue_ep.get<RUE_name>(),
              sge_ctime64(ref ? ref.get<RDE_time>() : 0, &time_str), ref.raw());
      cr_ep.set<QETI_queue_end_next>(ref.raw());
   }

   DRETURN_VOID;
}

/**
 * @brief Switches all queue end next references to the entries before a start time
 *
 * @param qeti the iterator
 * @param start the start time
 *
 * @note MT-NOTE: sge_qeti_next_before() is MT safe
 */
void sge_qeti_next_before(sge_qeti_t *qeti, uint64_t start) {
   DENTER(TOP_LAYER);
   sge_qeti_switch_to_next("P", start, *qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", start, *qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", start, *qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", start, *qeti->cr_refs_queue);
   DRETURN_VOID;
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
uint64_t sge_qeti_first(sge_qeti_t *qeti) {
   DENTER(TOP_LAYER);
   uint64_t all_resources_queue_end_time = 0;
   DSTRING_STATIC(time_str, 64);

   /* (re)init all queue end next references */
   sge_qeti_init_refs(*qeti->cr_refs_pe);
   sge_qeti_init_refs(*qeti->cr_refs_global);
   sge_qeti_init_refs(*qeti->cr_refs_host);
   sge_qeti_init_refs(*qeti->cr_refs_queue);

   /* determine all resources queue end time */
   sge_qeti_max_end_time("P", &all_resources_queue_end_time, *qeti->cr_refs_pe);
   sge_qeti_max_end_time("G", &all_resources_queue_end_time, *qeti->cr_refs_global);
   sge_qeti_max_end_time("H", &all_resources_queue_end_time, *qeti->cr_refs_host);
   sge_qeti_max_end_time("Q", &all_resources_queue_end_time, *qeti->cr_refs_queue);

   DPRINTF("sge_qeti_first() determines %s\n", sge_ctime64(all_resources_queue_end_time, &time_str));

   /* switch to the next entry with all queue end next references whose
      time is larger (?) or equal to all resources queue end time */
   sge_qeti_switch_to_next("P", all_resources_queue_end_time, *qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", all_resources_queue_end_time, *qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", all_resources_queue_end_time, *qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", all_resources_queue_end_time, *qeti->cr_refs_queue);

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
uint64_t sge_qeti_next(sge_qeti_t *qeti) {
   DENTER(TOP_LAYER);
   uint64_t all_resources_queue_end_time = DISPATCH_TIME_NOW;
   DSTRING_STATIC(time_str, 64);

   /* determine all resources queue end time */
   sge_qeti_max_end_time("P", &all_resources_queue_end_time, *qeti->cr_refs_pe);
   sge_qeti_max_end_time("G", &all_resources_queue_end_time, *qeti->cr_refs_global);
   sge_qeti_max_end_time("H", &all_resources_queue_end_time, *qeti->cr_refs_host);
   sge_qeti_max_end_time("Q", &all_resources_queue_end_time, *qeti->cr_refs_queue);

   DPRINTF("sge_qeti_next() determines %s\n", sge_ctime64(all_resources_queue_end_time, &time_str));

   /* switch to the next entry with all queue end next references whose
      time is larger (?) or equal to all resources queue end time */
   sge_qeti_switch_to_next("P", all_resources_queue_end_time, *qeti->cr_refs_pe);
   sge_qeti_switch_to_next("G", all_resources_queue_end_time, *qeti->cr_refs_global);
   sge_qeti_switch_to_next("H", all_resources_queue_end_time, *qeti->cr_refs_host);
   sge_qeti_switch_to_next("Q", all_resources_queue_end_time, *qeti->cr_refs_queue);

   DRETURN(all_resources_queue_end_time);
}

/**
 * @brief Release queue end time iterator
 *
 * Release all resources of the queue end time iterator. Refered
 * resource utilization diagrams are not affected.
 *
 * @param qeti the iterator, nullptr afterwards
 *
 * @note MT-NOTE: sge_qeti_release() is MT safe
 */
void sge_qeti_release(sge_qeti_t **qeti) {
   DENTER(TOP_LAYER);
   if (qeti == nullptr || *qeti == nullptr) {
      DRETURN_VOID;
   }

   // the owning handles free the QETI lists
   delete *qeti;
   *qeti = nullptr;
   DRETURN_VOID;
}
