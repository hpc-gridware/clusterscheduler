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
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Debiting a scheduled job on the objects it consumes
 */
#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_resource_quota.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_QU.h"

#include "sge_resource_utilization.h"
#include "subordinate_schedd.h"
#include "sge_select_queue.h"
#include "debit.h"
#include "sort_hosts.h"
#include "msg_schedd.h"

static int debit_job_from_queues(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::PE> pe,
                                 ocs::tom::ConstList<ocs::tom::JG> granted,
                                 ocs::tom::List<ocs::tom::QU> global_queue_list,
                                 ocs::tom::ConstList<ocs::tom::CE> complex_list, order_t *orders);

static int debit_job_from_hosts(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                ocs::tom::ConstElement<ocs::tom::PE> pe, ocs::tom::ConstList<ocs::tom::JG> granted,
                                ocs::tom::List<ocs::tom::EH> host_list, ocs::tom::ConstList<ocs::tom::CE> complex_list,
                                ocs::tom::ConstList<ocs::tom::CE> load_adjustments, int *sort_hostlist);

static int debit_job_from_rqs(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstList<ocs::tom::JG> granted,
                              ocs::tom::List<ocs::tom::RQS> rqs_list, ocs::tom::ConstElement<ocs::tom::PE> pe,
                              ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::US> acl_list,
                              ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list);

static int debit_job_from_ar(ocs::tom::Element<ocs::tom::AR> ar, ocs::tom::ConstElement<ocs::tom::JB> job,
                             ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ConstElement<ocs::tom::PE> pe,
                             ocs::tom::ConstList<ocs::tom::JG> granted, ocs::tom::ConstList<ocs::tom::CE> centry_list);

/**
 * @brief Debits a scheduled job on everything the assignment touched
 *
 * These objects are changed to represent the debitation:
 *
 * - **host list** - the load is increased according to the granted list, which
 *   also changes the sort order of the hosts. That order is what gives the
 *   queues their position, so `sort_hostlist` is set when it changed.
 * - **queue list** - the number of free slots is reduced, and subordinated
 *   queues that qmaster will suspend are marked as suspended.
 * - **PE** - the number of free slots is reduced.
 * - **resource quota set list** - the usage is increased according to the
 *   granted list.
 *
 * The job, the granted list and the complex list are only read: the granted
 * list says what has to be debited per queue, and the complex list is needed
 * to interpret the job's `-l` requests and the load correction.
 *
 * @param[in]     a                  everything describing the assignment
 * @param[in,out] sort_hostlist      set to 1 if the host list has to be
 *                                   resorted
 * @param[in,out] orders             needed to warn about jobs that are
 *                                   dispatched into a queue and suspended on
 *                                   subordinate within the same interval
 * @param[in]     now                true if this is or will be a running job,
 *                                   false for jobs that only go into the
 *                                   resource schedule
 * @param[in]     type               a string as foreseen for the `type`
 *                                   parameter of serf_record_entry(), may be
 *                                   nullptr
 * @param[in]     for_job_scheduling true when debiting a job, false for an
 *                                   advance reservation
 *
 * @return 0, or -1 if no assignment was passed
 *
 * @warning The debitation is lost if local copies of the global lists are
 *          passed in - the scheduler will then put every job on one queue.
 *          That can be used deliberately to test what scheduling a job to a
 *          specific queue would do.
 *
 * @warning This function does **not** check the consistency of the slot
 *          allocation. Debiting a job from a queue its user has no access to
 *          produces no error.
 */
int
debit_scheduled_job(const sge_assignment_t *a, int *sort_hostlist,
                    order_t *orders, bool now, const char *type,
                    bool for_job_scheduling) {
   DENTER(TOP_LAYER);

   if (!a) {
      DRETURN(-1);
   }

   if (now) {
      if (a->pe) {
         pe_debit_slots(a->pe, a->slots, a->job_id);
      }
      // the members of the assignment are raw until it is converted; they are checked where they arrive (N-X-3)
      const auto job = ocs::tom::ConstElement<ocs::tom::JB>(a->job);
      const auto ja_task = ocs::tom::ConstElement<ocs::tom::JAT>(a->ja_task);
      const auto pe = ocs::tom::ConstElement<ocs::tom::PE>(a->pe);
      const auto granted = ocs::tom::ConstList<ocs::tom::JG>(*a->gdil);
      const auto centries = ocs::tom::ConstList<ocs::tom::CE>(a->centry_list);
      debit_job_from_hosts(job, ja_task, pe, granted, ocs::tom::List<ocs::tom::EH>(a->host_list), centries,
                           a->load_adjustments, sort_hostlist);
      debit_job_from_queues(job, pe, granted, ocs::tom::List<ocs::tom::QU>(a->queue_list), centries, orders);
      debit_job_from_rqs(job, granted, ocs::tom::List<ocs::tom::RQS>(a->rqs_list), pe, centries, a->acl_list,
                         a->hgrp_list);
      debit_job_from_ar(ocs::tom::Element<ocs::tom::AR>(a->ar), job, ja_task, pe, granted, centries);
   }

   add_job_utilization(a, type, for_job_scheduling);

   DRETURN(0);
}

/*
 * Here
 *
 *   - we reduce the amount of free slots in the queue.
 *   - we activte suspend_on_subordinate to prevent
 *     scheduling on queues that will get suspended
 *   - we debit consumable resouces of queue
 *
 * to represent the job again we use the tagged selected queue list
 * (same game as calling sge_create_orders())
 * (would be better to use the granted_destin_identifier_list of the job)
 *
 * order_t *orders    needed to warn on jobs that get dispatched and suspended
 *                    on subordinate in the very same interval 
 */
static int debit_job_from_queues(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                 const ocs::tom::ConstElement<ocs::tom::PE> pe,
                                 const ocs::tom::ConstList<ocs::tom::JG> granted,
                                 const ocs::tom::List<ocs::tom::QU> global_queue_list,
                                 const ocs::tom::ConstList<ocs::tom::CE> centry_list, order_t *orders) {
   DENTER(TOP_LAYER);

   bool master_task = true;
   int qslots, total;
   unsigned int tagged;
   const char *qname;
   ocs::tom::Element<ocs::tom::QU> qep;
   int ret = 0;
   dstring queue_name = DSTRING_INIT;

   /* use each entry in sel_q_list as reference into the global_queue_list */
   const char *last_hostname = nullptr;
   for (const auto gel : granted) {
      tagged = ocs::tom::get<JG_slots>(gel);
      if (tagged) {
         /* find queue */
         qname = ocs::tom::get<JG_qname>(gel);
         if ((qep = ocs::tom::find_first_rw<QU_full_name>(global_queue_list, qname)) == nullptr) {
            master_task = false;
            continue;
         }

         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, ocs::tom::get<JG_qhostname>(gel));

         /* increase used slots */
         qslots = qinstance_slots_used(qep);

         /* precompute suspensions for subordinated queues */
         total = ocs::tom::get<QU_job_slots>(qep);
         for (ocs::tom::ConstElement<ocs::tom::SO> so : ocs::tom::get<QU_subordinate_list>(qep)) {
            if (!tst_sos(qslots, total, so) &&  /* not suspended till now */
                tst_sos(qslots + tagged, total, so)) {   /* but now                */
               sge_dstring_sprintf(&queue_name, "%s@%s", ocs::tom::get<SO_name>(so), static_cast<const char *>(ocs::tom::get<QU_qhostname>(qep)));

               ret |= sos_schedd(sge_dstring_get_string(&queue_name), global_queue_list);

               /* warn on jobs that were dispatched into that queue in
                  the same scheduling interval based on the orders list */
               for (const auto order : *orders->jobStartOrderList) {
                  if (ocs::tom::get<OR_type>(order) != ORT_start_job) {
                     continue;
                  }
                  if (ocs::tom::find_first_in<OR_queuelist, OQ_dest_queue>(order, sge_dstring_get_string(&queue_name))) {
                     WARNING(MSG_SUBORDPOLICYCONFLICT_UUSS, ocs::tom::get<JB_job_number>(job), ocs::tom::get<OR_job_number>(order), qname, sge_dstring_get_string(&queue_name));
                  }
               }

               for (const auto order : *orders->sentOrderList) {
                  if (ocs::tom::get<OR_type>(order) != ORT_start_job) {
                     continue;
                  }
                  if (ocs::tom::find_first_in<OR_queuelist, OQ_dest_queue>(order, sge_dstring_get_string(&queue_name))) {
                     WARNING(MSG_SUBORDPOLICYCONFLICT_UUSS, ocs::tom::get<JB_job_number>(job), ocs::tom::get<OR_job_number>(order), qname, sge_dstring_get_string(&queue_name));
                  }
               }
            }
         }

         DPRINTF("REDUCING SLOTS OF QUEUE %s BY %d\n", qname, tagged);

         qinstance_debit_consumable(qep, job, pe, centry_list, tagged, master_task, do_per_host_booking, nullptr);
      }
      master_task = false;
   }

   sge_dstring_free(&queue_name);

   DRETURN(ret);
}

/**
 * @brief Debits a job on the hosts it was granted
 *
 * Books the consumables on the global host and on each granted host, raises
 * the load correction of the hosts while load adjustments are in effect, and
 * re-sorts a host whose combined load changed.
 *
 * @param job the job (JB_Type)
 * @param ja_task the array task, for its granted resources
 * @param pe the granted parallel environment, or an empty handle
 * @param granted the granted destination identifiers (JG_Type)
 * @param host_list the execution hosts, debited and re-sorted
 * @param centry_list the complex entries
 * @param load_adjustments the load adjustments; the load correction is only raised while there are any
 * @param sort_hostlist set to 1 when a host's sort value changed; may be nullptr
 *
 * @return always 0
 */
static int debit_job_from_hosts(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                const ocs::tom::ConstElement<ocs::tom::PE> pe,
                                const ocs::tom::ConstList<ocs::tom::JG> granted,
                                const ocs::tom::List<ocs::tom::EH> host_list,
                                const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                                const ocs::tom::ConstList<ocs::tom::CE> load_adjustments, int *sort_hostlist) {
   DENTER(TOP_LAYER);

   lSortOrder *so = nullptr;
   const char *hnm = nullptr;
   const char *load_formula = nullptr;
   uint64_t load_adjustment_decay_time = sge_gmt32_to_gmt64(sconf_get_load_adjustment_decay_time());
   bool is_master_task = true;

   double old_sort_value, new_sort_value;

   so = lParseSortOrderVarArg(lGetListDescr(host_list), "%I+", EH_sort_value);

   const auto global = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(host_list, "global"));
   bool do_per_global_host_booking = true;

   load_formula = sconf_get_load_formula();

   /* debit from hosts */
   const char *last_hostname = nullptr;
   for (const auto gdil_ep : granted) {
      uint32_t ulc_factor;
      int slots = ocs::tom::get<JG_slots>(gdil_ep);

      hnm = ocs::tom::get<JG_qhostname>(gdil_ep);
      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, hnm);
      const auto hep = ocs::tom::Element<ocs::tom::EH>::from_typed(host_list_locate(host_list, hnm));

      if (load_adjustment_decay_time > 0 && !load_adjustments.empty()) {
         /* increase host load for each scheduled job slot */
         ulc_factor = ocs::tom::get<EH_load_correction_factor>(hep);
         ulc_factor += 100 * slots;
         ocs::tom::set<EH_load_correction_factor>(hep, ulc_factor);
      }

      const auto granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(ja_task);
      debit_host_consumable(job, ja_task, granted_resources_list, pe,
                            ocs::tom::Element<ocs::tom::EH>::from_typed(host_list_locate(host_list, SGE_GLOBAL_NAME)),
                            centry_list, slots, is_master_task, do_per_global_host_booking, nullptr);
      debit_host_consumable(job, ja_task, granted_resources_list, pe, hep, centry_list, slots, is_master_task, do_per_host_booking, nullptr);
      is_master_task = false;
      do_per_global_host_booking = false;

      /* compute new combined load for this host and put it into the host */
      old_sort_value = ocs::tom::get<EH_sort_value>(hep);

      new_sort_value = scaled_mixed_load(load_formula, global, hep, centry_list, ocs::tom::ConstList<ocs::tom::CE>());

      if (new_sort_value != old_sort_value) {
         ocs::tom::set<EH_sort_value>(hep, new_sort_value);
         if (sort_hostlist)
            *sort_hostlist = 1;
         DPRINTF("Increasing sort value of Host %s from %f to %f\n", hnm, old_sort_value, new_sort_value);
      }

      lResortElem(so, hep, host_list);
   }

   sge_free(&load_formula);
   lFreeSortOrder(&so);

   DRETURN(0);
}

/**
 * @brief Debits the consumables of a job on one host
 *
 * If `jep` and `jatep` are not given (nullptr), then an entry is generated in
 * the host's `EH_resource_utilization` list for every consumable defined in
 * its `EH_consumable_config_list` - that is how the utilization of a host is
 * initialized.
 *
 * @param[in]     jep                     the job (`JB_Type`), or nullptr to
 *                                        only initialize the utilization
 * @param[in]     jatep                   the array task (`JAT_Type`), or
 *                                        nullptr
 * @param[in]     granted_resources_list  the granted resources, used for the
 *                                        RSMAP and binding bookings
 * @param[in]     pe                      the granted PE (`PE_Type`)
 * @param[in,out] hep                     the host to debit on (`EH_Type`)
 * @param[in]     centry_list             the complex entries (`CE_Type`)
 * @param[in]     slots                   number of slots granted on this host
 * @param[in]     is_master_task          whether the master task runs here
 * @param[in]     do_per_host_booking     whether the per host consumables have
 *                                        to be booked in this call
 * @param[in,out] just_check              if given, nothing is booked and the
 *                                        flag reports whether it would fit
 *
 * @return the number of modifications made
 *
 * @todo Anything to do for RSMAPs? Do we want to initialize the
 *       `RUE_utilized_now_resource_map_list`?
 */
int debit_host_consumable(const ocs::tom::ConstElement<ocs::tom::JB> jep,
                          const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                          const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                          const ocs::tom::ConstElement<ocs::tom::PE> pe, const ocs::tom::Element<ocs::tom::EH> hep,
                          const ocs::tom::ConstList<ocs::tom::CE> centry_list, int slots, bool is_master_task,
                          bool do_per_host_booking, bool *just_check) {
   DENTER(TOP_LAYER);
   int mods = 0;
   mods += rc_debit_consumable(jep, pe, hep, centry_list, slots, EH_consumable_config_list, EH_resource_utilization,
                               ocs::tom::get<EH_name>(hep), is_master_task, do_per_host_booking, just_check);
   if (jep != nullptr && jatep != nullptr) {
      mods += ja_task_debit_host_rsmaps(granted_resources_list, hep, slots, just_check);
      mods += ja_task_debit_host_bindings(granted_resources_list, hep, slots, just_check);
   }
   DRETURN(mods);
}

/**
 * @brief Debits job in all relevant resource quotas
 *
 * The function debits in all relevant rule the requested amout of resources.
 *
 * @param job job request (JB_Type)
 * @param granted granted list (JG_Type)
 * @param rqs_list the resource quota sets to debit
 * @param pe granted pe (PE_Type)
 * @param centry_list consumable resouces list (CE_Type)
 * @param acl_list the access lists, to find the matching rules
 * @param hgrp_list the host groups, to find the matching rules
 *
 * @return always 0
 *
 * @note MT-NOTE: debit_job_from_rqs() is not MT safe
 */
static int debit_job_from_rqs(const ocs::tom::ConstElement<ocs::tom::JB> job,
                              const ocs::tom::ConstList<ocs::tom::JG> granted,
                              const ocs::tom::List<ocs::tom::RQS> rqs_list,
                              const ocs::tom::ConstElement<ocs::tom::PE> pe,
                              const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                              const ocs::tom::ConstList<ocs::tom::US> acl_list,
                              const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list) {
   DENTER(TOP_LAYER);

   if (ocs::tom::get<JB_ar>(job) != 0) {
      /* don't debit for AR jobs in resource quotas */
      DRETURN(0);
   }

   /* debit for all hosts */
   const char *last_hostname = nullptr;
   bool master_task = true;
   for (const auto gdil_ep : granted) {
      int slots = ocs::tom::get<JG_slots>(gdil_ep);
      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, ocs::tom::get<JG_qhostname>(gdil_ep));
      for (const auto rqs : rqs_list) {
         rqs_debit_consumable(rqs, job, gdil_ep, pe, centry_list, acl_list, hgrp_list, slots, master_task, do_per_host_booking);
      }
      master_task = false;
   }

   DRETURN(0);
}

/**
 * @brief Debits a job of an advance reservation in the reserved queues and hosts
 *
 * The reservation holds its own copies of the queue instances and hosts it
 * reserved; the job is debited there, not in the master lists.
 *
 * @param ar          the advance reservation of the job, or nullptr for a job outside one
 * @param job         the job (JB_Type)
 * @param ja_task     the array task (JAT_Type)
 * @param pe          the granted parallel environment, or nullptr
 * @param granted     the granted destination identifiers (JG_Type)
 * @param centry_list the system wide attribute configuration (CE_Type)
 *
 * @return always 0
 */
static int debit_job_from_ar(const ocs::tom::Element<ocs::tom::AR> ar, const ocs::tom::ConstElement<ocs::tom::JB> job,
                             const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                             const ocs::tom::ConstElement<ocs::tom::PE> pe,
                             const ocs::tom::ConstList<ocs::tom::JG> granted,
                             const ocs::tom::ConstList<ocs::tom::CE> centry_list) {
   DENTER(TOP_LAYER);

   if (ar != nullptr) {
      const auto ar_global_host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, SGE_GLOBAL_NAME);

      bool master_task = true;
      bool do_per_global_host_booking = true;
      const char *last_hostname = nullptr;
      for (const auto gel : granted) {
         int slots = ocs::tom::get<JG_slots>(gel);
         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, ocs::tom::get<JG_qhostname>(gel));
         const auto granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(ja_task);
         const auto queue =
            ocs::tom::find_first_in_rw<AR_reserved_queues, QU_full_name>(ar, ocs::tom::get<JG_qname>(gel));
         qinstance_debit_consumable(queue, job, pe, centry_list, slots, master_task, do_per_host_booking, nullptr);
         if (ar_global_host != nullptr) {
            debit_host_consumable(job, ja_task, granted_resources_list, pe, ar_global_host, centry_list, slots, master_task,
                                  do_per_global_host_booking, nullptr);
         }
         const auto host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, ocs::tom::get<JG_qhostname>(gel));
         if (host != nullptr) {
            debit_host_consumable(job, ja_task, granted_resources_list, pe, host, centry_list, slots, master_task,
                                  do_per_host_booking, nullptr);
         }
         master_task = false;
         do_per_global_host_booking = false;
      }
   }

   DRETURN(0);
}

/**
 * @brief Books a job's consumables on a host, for a caller with untyped objects (N-X-2)
 *
 * @param[in]     jep                     the job (`JB_Type`), checked, or nullptr
 * @param[in]     jatep                   the array task (`JAT_Type`), checked, or nullptr
 * @param[in]     granted_resources_list  the granted resources (`GRU_Type`), checked, or nullptr
 * @param[in]     pe                      the granted PE (`PE_Type`), checked, or nullptr
 * @param[in,out] hep                     the host to debit on (`EH_Type`), checked
 * @param[in]     centry_list             the complex entries (`CE_Type`), checked
 * @param[in]     slots                   number of slots granted on this host
 * @param[in]     is_master_task          whether the master task runs here
 * @param[in]     do_per_host_booking     whether the per host consumables have to be booked in this call
 * @param[in,out] just_check              if given, nothing is booked and the flag reports whether it would fit
 *
 * @return the number of modifications made
 */
int debit_host_consumable(const lListElem *jep, const lListElem *jatep, const lList *granted_resources_list,
                          const lListElem *pe, lListElem *hep, const lList *centry_list, int slots, bool is_master_task,
                          bool do_per_host_booking, bool *just_check) {
   return debit_host_consumable(ocs::tom::ConstElement<ocs::tom::JB>(jep), ocs::tom::ConstElement<ocs::tom::JAT>(jatep),
                                ocs::tom::ConstList<ocs::tom::GRU>(granted_resources_list),
                                ocs::tom::ConstElement<ocs::tom::PE>(pe), ocs::tom::Element<ocs::tom::EH>(hep),
                                ocs::tom::ConstList<ocs::tom::CE>(centry_list), slots, is_master_task,
                                do_per_host_booking, just_check);
}
