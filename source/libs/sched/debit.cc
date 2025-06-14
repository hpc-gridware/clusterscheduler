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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

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

#include "sge_resource_utilization.h"
#include "subordinate_schedd.h"
#include "sge_select_queue.h"
#include "debit.h"
#include "sort_hosts.h"
#include "msg_schedd.h"

static int
debit_job_from_queues(lListElem *job, const lListElem *pe, lList *selected_queue_list, lList *global_queue_list,
                      const lList *complex_list, order_t *orders);

static int
debit_job_from_hosts(lListElem *job, lListElem *ja_task, const lListElem *pe, lList *granted, lList *host_list, const lList *complex_list,
                     const lList *load_adjustments, int *sort_hostlist);

static int
debit_job_from_rqs(lListElem *job, lList *granted, lList *rqs_list, lListElem *pe,
                   const lList *centry_list, const lList *acl_list, const lList *hgrp_list);

static int
debit_job_from_ar(lListElem *ar, lListElem *job, lListElem *ja_task, const lListElem *pe, lList *granted, lList *ar_list, const lList *centry_list);

/* -------------------------------------------------------------

   debit_scheduled_job()

   The following objects will get changed to represent the debitations:

      host_list
         - the load gets increased according the granted list 
         - changes sort order of host list 
           (this sort order is used to get positions for the queues)

      queue_list 
         - the number of free slots gets reduced
         - subordinated queues that will get suspended by 
           the qmaster get marked as suspended

      pe
         - the number of free slots gets reduced
      
      sort_hostlist
         - if the sort order of the host_list is changed, 
           sort_hostlist is set to 1

      orders_list
         - needed to warn on jobs that were dispatched into 
           queues and get suspended on subordinate in the very 
           same interval

      limitation_rule_set_list
         - the load gets increased according the granted list

   The other objects get not changed and are needed to present
   and interprete the debitations on the upper objects:

      job
      granted
         - list that contains one element for each queue
           describing what has to be debited
      complex_list
         - needed to interprete the jobs -l requests and 
           the load correction

   1st NOTE: 
      The debitations will be lost if you pass local copies
      of the global lists to this function and your scheduler
      will try to put all jobs on one queue (or other funny 
      decisions).

      But this can be a feature if you use local copies to
      test what happens if you schedule a job to a specific
      queue (not tested). 

   2nd NOTE: 
      This function is __not__ responsible for any consistency 
      checking of your slot allocation! E.g. you will get no 
      error if you try to debit a job from a queue where the 
      jobs user has no access.

const sge_assignment_t *a - all information describing the assignemnt 
int *sort_hostlist,       - do we have to resort the hostlist? 
order_t *orders,          - needed to warn on jobs that were dispatched into
                            queues and get suspended on subordinate in the very
                            same interval 
bool now,                 - if true this is or will be a running job
                            false for all jobs that must only be put into the
                            resource schedule 
const char *type          - a string as forseen with serf_record_entry() 
                            'type' parameter (may be nullptr)
bool for_job_scheduling   - true if debiting job, false if advance_reservation
      
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
      debit_job_from_hosts(a->job, a->ja_task, a->pe, a->gdil, a->host_list, a->centry_list, a->load_adjustments, sort_hostlist);
      debit_job_from_queues(a->job, a->pe, a->gdil, a->queue_list, a->centry_list, orders);
      debit_job_from_rqs(a->job, a->gdil, a->rqs_list, a->pe, a->centry_list, a->acl_list, a->hgrp_list);
      debit_job_from_ar(a->ar, a->job, a->ja_task, a->pe, a->gdil, a->ar_list, a->centry_list);
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
static int
debit_job_from_queues(lListElem *job, const lListElem *pe, lList *granted, lList *global_queue_list,
                      const lList *centry_list, order_t *orders) {
   bool master_task = true;
   int qslots, total;
   unsigned int tagged;
   const char *qname;
   const lListElem *gel;
   lListElem *qep;
   const lListElem *so;
   int ret = 0;
   dstring queue_name = DSTRING_INIT;

   DENTER(TOP_LAYER);

   /* use each entry in sel_q_list as reference into the global_queue_list */
   const char *last_hostname = nullptr;
   for_each_ep(gel, granted) {

      tagged = lGetUlong(gel, JG_slots);
      if (tagged) {
         /* find queue */
         qname = lGetString(gel, JG_qname);
         if ((qep = lGetElemStrRW(global_queue_list, QU_full_name, qname)) == nullptr) {
            master_task = false;
            continue;
         }

         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, lGetHost(gel, JG_qhostname));

         /* increase used slots */
         qslots = qinstance_slots_used(qep);

         /* precompute suspensions for subordinated queues */
         total = lGetUlong(qep, QU_job_slots);
         for_each_ep(so, lGetList(qep, QU_subordinate_list)) {
            if (!tst_sos(qslots, total, so) &&  /* not suspended till now */
                tst_sos(qslots + tagged, total, so)) {   /* but now                */
               const lListElem *order = nullptr;

               sge_dstring_sprintf(&queue_name, "%s@%s", lGetString(so, SO_name), lGetHost(qep, QU_qhostname));

               ret |= sos_schedd(sge_dstring_get_string(&queue_name), global_queue_list);

               /* warn on jobs that were dispatched into that queue in
                  the same scheduling interval based on the orders list */
               for_each_ep(order, orders->jobStartOrderList) {
                  if (lGetUlong(order, OR_type) != ORT_start_job) {
                     continue;
                  }
                  if (lGetSubStr(order, OQ_dest_queue, sge_dstring_get_string(&queue_name), OR_queuelist)) {
                     WARNING(MSG_SUBORDPOLICYCONFLICT_UUSS, lGetUlong(job, JB_job_number), lGetUlong(order, OR_job_number), qname, sge_dstring_get_string(&queue_name));
                  }
               }

               for_each_ep(order, orders->sentOrderList) {
                  if (lGetUlong(order, OR_type) != ORT_start_job) {
                     continue;
                  }
                  if (lGetSubStr(order, OQ_dest_queue, sge_dstring_get_string(&queue_name), OR_queuelist)) {
                     WARNING(MSG_SUBORDPOLICYCONFLICT_UUSS, lGetUlong(job, JB_job_number), lGetUlong(order, OR_job_number), qname, sge_dstring_get_string(&queue_name));
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

static int
debit_job_from_hosts(lListElem *job, lListElem *ja_task, const lListElem *pe, lList *granted, lList *host_list, const lList *centry_list,
                     const lList *load_adjustments, int *sort_hostlist) {
   lSortOrder *so = nullptr;
   lListElem *hep;
   lListElem *global;
   const char *hnm = nullptr;
   const char *load_formula = nullptr;
   u_long64 load_adjustment_decay_time = sge_gmt32_to_gmt64(sconf_get_load_adjustment_decay_time());
   bool is_master_task = true;

   double old_sort_value, new_sort_value;

   DENTER(TOP_LAYER);

   so = lParseSortOrderVarArg(lGetListDescr(host_list), "%I+", EH_sort_value);

   global = host_list_locate(host_list, "global");
   bool do_per_global_host_booking = true;

   load_formula = sconf_get_load_formula();

   /* debit from hosts */
   const lListElem *gdil_ep;
   const char *last_hostname = nullptr;
   for_each_ep(gdil_ep, granted) {
      u_long32 ulc_factor;
      int slots = lGetUlong(gdil_ep, JG_slots);

      hnm = lGetHost(gdil_ep, JG_qhostname);
      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, hnm);
      hep = host_list_locate(host_list, hnm);

      if (load_adjustment_decay_time > 0 && lGetNumberOfElem(load_adjustments) > 0) {
         /* increase host load for each scheduled job slot */
         ulc_factor = lGetUlong(hep, EH_load_correction_factor);
         ulc_factor += 100 * slots;
         lSetUlong(hep, EH_load_correction_factor, ulc_factor);
      }

      debit_host_consumable(job, ja_task, pe, host_list_locate(host_list, SGE_GLOBAL_NAME), centry_list, slots,
                            is_master_task, do_per_global_host_booking, nullptr);
      debit_host_consumable(job, ja_task, pe, hep, centry_list, slots, is_master_task, do_per_host_booking, nullptr);
      is_master_task = false;
      do_per_global_host_booking = false;

      /* compute new combined load for this host and put it into the host */
      old_sort_value = lGetDouble(hep, EH_sort_value);

      new_sort_value = scaled_mixed_load(load_formula, global, hep, centry_list);

      if (new_sort_value != old_sort_value) {
         lSetDouble(hep, EH_sort_value, new_sort_value);
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

/*
 * jep: JB_Type
 * jatep: JAT_Type
 * hep: EH_Type
 * centry_list: CE_Type
 *
 * If jep and jatep are not given (nullptr) then for all consumables which are
 * defined in hep's complex_values list (EH_consumable_config_list)
 * entries are generated in hep's EH_resource_utilization list
 * @todo: anything to do for RSMAPs?
 *        Do we want to initialize the RUE_utilized_now_resource_map_list?
 */
int
debit_host_consumable(const lListElem *jep, const lListElem *jatep, const lListElem *pe, lListElem *hep,
                      const lList *centry_list, int slots, bool is_master_task, bool do_per_host_booking,
                      bool *just_check) {
   int mods = 0;
   mods += rc_debit_consumable(jep, pe, hep, centry_list, slots, EH_consumable_config_list, EH_resource_utilization,
                               lGetHost(hep, EH_name), is_master_task, do_per_host_booking, just_check);
   if (jep != nullptr && jatep != nullptr) {
      mods += ja_task_debit_host_rsmaps(jatep, hep, slots, just_check);
   }
   return mods;
}

/****** sge_resource_quota_schedd/debit_job_from_rqs() **********************************
*  NAME
*     debit_job_from_rqs() -- debits job in all relevant resource quotas
*
*  SYNOPSIS
*     int debit_job_from_rqs(lListElem *job, lList *granted, lListElem* pe, 
*     lList *centry_list) 
*
*  FUNCTION
*     The function debits in all relevant rule the requested amout of resources.
*
*  INPUTS
*     lListElem *job     - job request (JB_Type)
*     lList *granted     - granted list (JG_Type)
*     lListElem* pe      - granted pe (PE_Type)
*     lList *centry_list - consumable resouces list (CE_Type)
*
*  RESULT
*     int - always 0
*
*  NOTES
*     MT-NOTE: debit_job_from_rqs() is not MT safe 
*
*******************************************************************************/
static int
debit_job_from_rqs(lListElem *job, lList *granted, lList *rqs_list, lListElem *pe,
                   const lList *centry_list, const lList *acl_list, const lList *hgrp_list) {
   DENTER(TOP_LAYER);

   if (lGetUlong(job, JB_ar) != 0) {
      /* don't debit for AR jobs in resource quotas */
      DRETURN(0);
   }

   /* debit for all hosts */
   const lListElem *gdil_ep;
   const char *last_hostname = nullptr;
   bool master_task = true;
   for_each_ep(gdil_ep, granted) {
      lListElem *rqs = nullptr;
      int slots = lGetUlong(gdil_ep, JG_slots);

      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, lGetHost(gdil_ep, JG_qhostname));

      for_each_rw (rqs, rqs_list) {
         rqs_debit_consumable(rqs, job, gdil_ep, pe, centry_list, acl_list, hgrp_list, slots, master_task, do_per_host_booking);
      }
      master_task = false;
   }

   DRETURN(0);
}

static int
debit_job_from_ar(lListElem *ar, lListElem *job, lListElem *ja_task, const lListElem *pe, lList *granted, lList *ar_list, const lList *centry_list) {

   DENTER(TOP_LAYER);

   if (ar != nullptr) {
      lListElem *ar_global_host = lGetSubHostRW(ar, EH_name, SGE_GLOBAL_NAME, AR_reserved_hosts);

      bool master_task = true;
      bool do_per_global_host_booking = true;
      const char *last_hostname = nullptr;
      const lListElem *gel;
      for_each_ep(gel, granted) {
         int slots = lGetUlong(gel, JG_slots);
         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, lGetHost(gel, JG_qhostname));

         lListElem *queue = lGetSubStrRW(ar, QU_full_name, lGetString(gel, JG_qname), AR_reserved_queues);
         qinstance_debit_consumable(queue, job, pe, centry_list, slots, master_task, do_per_host_booking, nullptr);
         if (ar_global_host != nullptr) {
            debit_host_consumable(job, ja_task, pe, ar_global_host, centry_list, slots, master_task,
                                  do_per_global_host_booking, nullptr);
         }
         lListElem *host = lGetSubHostRW(ar, EH_name, lGetHost(gel, JG_qhostname), AR_reserved_hosts);
         if (host != nullptr) {
            debit_host_consumable(job, ja_task, pe, host, centry_list, slots, master_task,
                                  do_per_host_booking, nullptr);
         }
         master_task = false;
         do_per_global_host_booking = false;
      }
   }

   DRETURN(0);
}
