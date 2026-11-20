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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The scheduler thread inside qmaster, and the lists it schedules from
 */
#include <cstring>
#include <pthread.h>

#ifdef SOLARISAMD64
#  include <sys/stream.h>
#endif

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_thread_ctrl.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_order.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "evc/sge_event_client.h"

#include "sched/sge_orders.h"
#include "sched/sge_job_schedd.h"
#include "sched/sge_serf.h"
#include "sched/schedd_message.h"
#include "sched/msg_schedd.h"
#include "sched/sge_schedd_text.h"
#include "sched/schedd_monitor.h"
#include "sched/sge_interactive_sched.h"
#include "sched/sgeee.h"
#include "sched/load_correction.h"
#include "sched/sge_resource_utilization.h"
#include "sched/suspend_thresholds.h"
#include "sched/sort_hosts.h"
#include "sched/debit.h"

#include "ocs_CategorySchedd.h"
#include <cinttypes>
#include "sge.h"
#include "sge_follow.h"
#include "sge_sched_thread.h"
#include "sge_sched_order.h"
#include "sge_sched_thread_rsmap.h"

scheduler_control_t Scheduler_Control = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        false, false, nullptr,
        false /* new_global_config */
};

#if 0
static void rest_busy(sge_evc_class_t *evc);

static void wait_for_events();
#endif

static int
dispatch_jobs(sge_evc_class_t *evc, scheduler_all_data_t *lists, order_t *orders,
              ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[]);

static dispatch_t
select_assign_debit(ocs::tom::ListPtr<ocs::tom::QU> &queue_list, ocs::tom::ListPtr<ocs::tom::QU> &dis_queue_list,
                    ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                    ocs::tom::List<ocs::tom::PE> pe_list, ocs::tom::ConstList<ocs::tom::CK> ckpt_list,
                    ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::List<ocs::tom::EH> host_list,
                    ocs::tom::ConstList<ocs::tom::US> acl_list, ocs::tom::List<ocs::tom::CE> load_adjustments,
                    ocs::tom::ListPtr<ocs::tom::JC> &user_list, ocs::tom::ListPtr<ocs::tom::JC> &group_list,
                    order_t *orders,
                    double *total_running_job_tickets, int *sort_hostlist, bool is_start, bool is_reserve,
                    bool is_schedule_based, ocs::tom::ListPtr<ocs::tom::LDR> &load_list,
                    ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list, ocs::tom::List<ocs::tom::RQS> rqs_list,
                    ocs::tom::List<ocs::tom::AR> ar_list, sched_prof_t *pi, bool monitor_next_run, uint64_t now);

/** @brief Record that the global configuration changed
 * @param new_value true when it has
 */
void
st_set_flag_new_global_conf(bool new_value) {
   DENTER(TOP_LAYER);
   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);
   Scheduler_Control.new_global_conf = new_value;
   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);
   DRETURN_VOID;
}

/** @brief Has the global configuration changed since the last run?
 * @return true when it has
 */
bool
st_get_flag_new_global_conf() {
   DENTER(TOP_LAYER);

   bool ret = false;

   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);
   ret = Scheduler_Control.new_global_conf;
   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);
   DRETURN(ret);
}

/**
 * @brief Add the global scheduler messages for the queues that cannot take jobs
 *
 * Queues that are suspended, in error, ambiguous or unknown, and the queues already put aside
 * in the snapshot's disabled queue list, each get a "queue not available" message.
 *
 * @param lists the scheduler's snapshot of the cluster
 * @param monitor_next_run whether the scheduling run is monitored
 */
static void
scheduler_global_queue_messages(scheduler_all_data_t *lists, const bool monitor_next_run) {
   DENTER(TOP_LAYER);

   const auto all_queues = ocs::tom::ConstList<ocs::tom::QU>(*lists->all_queue_list);
   if (all_queues != nullptr) {
      // @todo: here are also states missing that make queues unavailable
      const auto where = ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED) ||   // only not suspended queues
                         ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED_ON_SUBORDINATE) ||
                         ocs::tom::field<QU_state>().has_all_bits(QI_ERROR) ||       // no queues in error state
                         ocs::tom::field<QU_state>().has_all_bits(QI_AMBIGUOUS) ||
                         ocs::tom::field<QU_state>().has_all_bits(QI_UNKNOWN);       // only known queues
      const auto not_available = all_queues.select(where);

      for (const auto queue : *not_available) {
         const char *queue_name = ocs::tom::get<QU_full_name>(queue);
         schedd_mes_add_global(nullptr, monitor_next_run, SCHEDD_INFO_QUEUENOTAVAIL_, queue_name);
      }

      for (const auto queue : ocs::tom::ConstList<ocs::tom::QU>(*lists->dis_queue_list)) {
         const char *queue_name = ocs::tom::get<QU_full_name>(queue);
         schedd_mes_add_global(nullptr, monitor_next_run, SCHEDD_INFO_QUEUENOTAVAIL_, queue_name);
      }

      schedd_log_list(monitor_next_run, MSG_SCHEDD_LOGLIST_QUEUESTEMPORARLYNOTAVAILABLEDROPPED,
                      *not_available);
   }

   DRETURN_VOID;
}

/** @brief Run one scheduling pass over the snapshot
 * @param evc the event client
 * @param answer_list receives error messages
 * @param lists the scheduler's snapshot of the cluster
 */
void scheduler_method(sge_evc_class_t *evc, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, scheduler_all_data_t *lists) {
   DENTER(TOP_LAYER);

   order_t orders = ORDER_INIT;
   // the job lists by state; a nullptr entry is a state split_jobs() does not collect
   ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[SPLIT_LAST];
   ocs::tom::ListPtr<ocs::tom::JB> waiting_due_to_pedecessor_list;
   ocs::tom::ListPtr<ocs::tom::JB> waiting_due_to_time_list;
   ocs::tom::ListPtr<ocs::tom::JB> pending_excluded_list;
   ocs::tom::ListPtr<ocs::tom::JB> suspended_list;
   ocs::tom::ListPtr<ocs::tom::JB> finished_list;
   ocs::tom::ListPtr<ocs::tom::JB> pending_list;
   ocs::tom::ListPtr<ocs::tom::JB> pending_excludedlist;
   ocs::tom::ListPtr<ocs::tom::JB> running_list;
   ocs::tom::ListPtr<ocs::tom::JB> error_list;
   ocs::tom::ListPtr<ocs::tom::JB> hold_list;
   ocs::tom::ListPtr<ocs::tom::JB> not_started_list;
   ocs::tom::ListPtr<ocs::tom::JB> deferred_list;
   int prof_job_count, global_mes_count, job_mes_count;

   int i;

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM0);

   // initializations
   prof_job_count = static_cast<uint32_t>(lists->job_list->size());

   sconf_reset_jobs();
   schedd_mes_initialize();
   schedd_order_initialize();

   // split job list into multiple lists according to job states
   // we will schedule the pending jobs
   for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
      splitted_job_lists[i] = nullptr;
   }
   splitted_job_lists[SPLIT_WAITING_DUE_TO_PREDECESSOR] = &waiting_due_to_pedecessor_list;
   splitted_job_lists[SPLIT_WAITING_DUE_TO_TIME] = &waiting_due_to_time_list;
   splitted_job_lists[SPLIT_PENDING_EXCLUDED] = &pending_excluded_list;
   splitted_job_lists[SPLIT_SUSPENDED] = &suspended_list;
   splitted_job_lists[SPLIT_FINISHED] = &finished_list;
   splitted_job_lists[SPLIT_PENDING] = &pending_list;
   splitted_job_lists[SPLIT_PENDING_EXCLUDED_INSTANCES] = &pending_excludedlist;
   splitted_job_lists[SPLIT_RUNNING] = &running_list;
   splitted_job_lists[SPLIT_ERROR] = &error_list;
   splitted_job_lists[SPLIT_HOLD] = &hold_list;
   splitted_job_lists[SPLIT_NOT_STARTED] = &not_started_list;
   splitted_job_lists[SPLIT_DEFERRED] = &deferred_list;
   split_jobs(*lists->job_list, mconf_get_max_aj_instances(), splitted_job_lists, false);

   // generate global queue messages (disabled, suspended, unknown, ...)
   scheduler_global_queue_messages(lists, evc->monitor_next_run);

   // the actual scheduling
   dispatch_jobs(evc, lists, &orders, splitted_job_lists);

   // post-processing
   remove_immediate_jobs(splitted_job_lists[SPLIT_PENDING]->get(), splitted_job_lists[SPLIT_RUNNING]->get(), &orders);

   // send orders to worker threads
   if (!sge_thread_has_shutdown_started()) {
      sge_schedd_send_orders(&orders, orders.configOrderList, nullptr, "C: config orders");
      sge_schedd_send_orders(&orders, orders.jobStartOrderList, nullptr, "C: job start orders");
      sge_schedd_send_orders(&orders, orders.pendingOrderList, nullptr, "C: pending ticket orders");
   }

   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   {
      int clean_jobs[] = {SPLIT_WAITING_DUE_TO_PREDECESSOR,
                          SPLIT_WAITING_DUE_TO_TIME,
                          SPLIT_PENDING_EXCLUDED,
                          SPLIT_PENDING_EXCLUDED_INSTANCES,
                          SPLIT_ERROR,
                          SPLIT_HOLD};
      int max = 6;

      for (int i = 0; i < max; ++i) {
         /* clear SGEEE fields for queued jobs */
         for (const auto job : ocs::tom::ConstList<ocs::tom::JB>(splitted_job_lists[clean_jobs[i]]->get())) {
            sge_create_orders(orders.pendingOrderList, ORT_clear_pri_info, job, ocs::tom::ConstElement<ocs::tom::JAT>(),
                              ocs::tom::ConstList<ocs::tom::JG>(), false);
         }
      }
   }
   sge_build_sgeee_orders(lists, ocs::tom::List<ocs::tom::JB>(), splitted_job_lists[SPLIT_PENDING]->get(),
                          ocs::tom::List<ocs::tom::JB>(), &orders, false, 0, false);

   sge_build_sgeee_orders(lists, ocs::tom::List<ocs::tom::JB>(), splitted_job_lists[SPLIT_NOT_STARTED]->get(),
                          ocs::tom::List<ocs::tom::JB>(), &orders, false, 0, false);

   /* generated scheduler messages, thus we have to call it */
   trash_splitted_jobs(evc->monitor_next_run, splitted_job_lists);

   sge_add_schedd_info(orders.jobStartOrderList, &global_mes_count, &job_mes_count);

   if (prof_is_active(SGE_PROF_SCHEDLIB4)) {
      prof_stop_measurement(SGE_PROF_SCHEDLIB4, nullptr);
      PROFILING("PROF: create pending job orders: %.3f s", prof_get_measurement_utime(SGE_PROF_SCHEDLIB4, false, nullptr));
   }

   sge_schedd_send_orders(&orders, orders.configOrderList, answer_list, "D: config orders");
   sge_schedd_send_orders(&orders, orders.jobStartOrderList, answer_list,
                          "D: job start orders");
   sge_schedd_send_orders(&orders, orders.pendingOrderList, answer_list,
                          "D: pending ticket orders");

   if (Master_Request_Queue.order_list != nullptr) {
      sge_schedd_add_gdi_order_request(&orders, answer_list, &Master_Request_Queue.order_list);
   }

   if (prof_is_active(SGE_PROF_CUSTOM0)) {
      prof_stop_measurement(SGE_PROF_CUSTOM0, nullptr);

      PROFILING("PROF: scheduled in %.3f (u %.3f + s %.3f = %.3f): %d "
                "sequential, %d parallel, " sge_u32 " orders, " sge_u32 " H, " sge_u32 " Q, " sge_u32
                " QA, " sge_u32 " J(qw), " sge_u32 " J(r), " sge_u32 " J(s), " sge_u32 " J(h), " sge_u32
                " J(e), " sge_u32 " J(x), %d J(all), " sge_u32 " C, " sge_u32 " ACL, " sge_u32 " PE, "
                sge_u32 " U, " sge_u32 " D, " sge_u32 " PRJ, " sge_u32 " ST, " sge_u32 " CKPT, "
                sge_u32 " RU, %d gMes, %d jMes, " sge_u32 "/" sge_u32
                " pre-send, %d/%d/%d pe-alg",
                      prof_get_measurement_wallclock(SGE_PROF_CUSTOM0, true, nullptr),
                      prof_get_measurement_utime(SGE_PROF_CUSTOM0, true, nullptr),
                      prof_get_measurement_stime(SGE_PROF_CUSTOM0, true, nullptr),
                      prof_get_measurement_utime(SGE_PROF_CUSTOM0, true, nullptr) + prof_get_measurement_stime(SGE_PROF_CUSTOM0, true, nullptr),
                      sconf_get_fast_jobs(),

                      sconf_get_pe_jobs(),
                      orders.numberSendOrders,
                      static_cast<uint32_t>(lists->host_list->size()),
                      static_cast<uint32_t>(lists->queue_list->size()),
                      static_cast<uint32_t>(lists->all_queue_list->size()),

                      (static_cast<uint32_t>(splitted_job_lists[SPLIT_PENDING]->get().size()) +
                       static_cast<uint32_t>(splitted_job_lists[SPLIT_NOT_STARTED]->get().size())),
                      static_cast<uint32_t>(splitted_job_lists[SPLIT_RUNNING]->get().size()),
                      static_cast<uint32_t>(splitted_job_lists[SPLIT_SUSPENDED]->get().size()),
                      static_cast<uint32_t>(splitted_job_lists[SPLIT_HOLD]->get().size()),
                      static_cast<uint32_t>(splitted_job_lists[SPLIT_ERROR]->get().size()),

                      static_cast<uint32_t>(splitted_job_lists[SPLIT_FINISHED]->get().size()),
                      prof_job_count,
                      static_cast<uint32_t>(lists->centry_list.size()),
                      static_cast<uint32_t>(lists->acl_list->size()),
                      static_cast<uint32_t>(lists->pe_list->size()),

                      static_cast<uint32_t>(lists->user_list->size()),
                      static_cast<uint32_t>(lists->dept_list->size()),
                      static_cast<uint32_t>(lists->project_list->size()),
                      static_cast<uint32_t>(lists->share_tree->size()),
                      static_cast<uint32_t>(lists->ckpt_list.size()),

                      static_cast<uint32_t>(lists->running_per_user->size()),
                      global_mes_count,
                      job_mes_count,
                      orders.numberSendOrders,
                      orders.numberSendPackages,

                      sconf_get_pe_alg_value(SCHEDD_PE_LOW_FIRST),
                      sconf_get_pe_alg_value(SCHEDD_PE_BINARY),
                      sconf_get_pe_alg_value(SCHEDD_PE_HIGH_FIRST)
              );
   }

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM5);

   /* free all job lists, here and not at the end of the scope, so the free is measured */
   for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
      if (splitted_job_lists[i] != nullptr) {
         splitted_job_lists[i]->reset();
         splitted_job_lists[i] = nullptr;
      }
   }

   if (prof_is_active(SGE_PROF_CUSTOM5)) {
      prof_stop_measurement(SGE_PROF_CUSTOM5, nullptr);

      PROFILING("PROF: send orders and cleanup took: %.3f (u %.3f,s %.3f) s",
         prof_get_measurement_wallclock(SGE_PROF_CUSTOM5, true, nullptr),
         prof_get_measurement_utime(SGE_PROF_CUSTOM5, true, nullptr),
         prof_get_measurement_stime(SGE_PROF_CUSTOM5, true, nullptr));
   }

   // print separator line in the schedule file *after* the scheduling run
   serf_new_interval();

   DRETURN_VOID;
}

/**
 * @brief Add the virtual load of recently started jobs, and find out whether it touches the global host
 *
 * Load sensors report load delayed, so the load of an execution host is increased for each job
 * started before the load adjustment decay time (configuration value load_adjustment_decay_time).
 *
 * @param lists the scheduler's snapshot of the cluster
 * @param running_jobs the running jobs (`JB`)
 * @param job_load_adjustments the configured job load adjustments (`CE`)
 * @param monitor_next_run whether the scheduling run is monitored
 */
static void
do_load_adjustment(scheduler_all_data_t *lists, const ocs::tom::ConstList<ocs::tom::JB> running_jobs,
                   const ocs::tom::ConstList<ocs::tom::CE> job_load_adjustments, const bool monitor_next_run) {
   DENTER(TOP_LAYER);
   int global_lc = 0;

   uint64_t decay_time = sge_gmt32_to_gmt64(sconf_get_load_adjustment_decay_time());
   if (decay_time > 0) {
      const auto host_list = lists->host_list.get();

      correct_load(running_jobs,
                   ocs::tom::ConstList<ocs::tom::QU>(*lists->queue_list),
                   host_list,
                   decay_time, monitor_next_run);

      /* is there a "global" load value in the job_load_adjustments?
         if so this will make it necessary to check load thresholds
         of each queue after each dispatched job */
      {
         const auto gep = ocs::tom::ConstElement<ocs::tom::EH>(host_list_locate(host_list, "global"));
         if (gep) {
            for (const auto lcep : job_load_adjustments) {
               const char *attr = ocs::tom::get<CE_name>(lcep);
               if (ocs::tom::find_first_in<EH_load_list, HL_name>(gep, attr)) {
                  DPRINTF("GLOBAL LOAD CORRECTION \"%s\"\n", attr);
                  global_lc = 1;
                  break;
               }
            }
         }
      }
   }

   sconf_set_global_load_correction(global_lc != 0);

   DRETURN_VOID;
}

/**
 * @brief Dispatch the pending jobs of one scheduling run
 *
 * Filters the queues, calculates the tickets, sorts the pending jobs and the hosts and then tries
 * the pending jobs in priority order, creating the start orders and reservations.
 *
 * @param evc the event client
 * @param lists the scheduler's snapshot of the cluster
 * @param orders receives the orders of the run
 * @param splitted_job_lists the job lists split by job state (`JB`), see split_jobs()
 * @return 0 on success, -1 if the queue list could not be split or the ticket calculation failed
 */
static int
dispatch_jobs(sge_evc_class_t *evc, scheduler_all_data_t *lists, order_t *orders,
              ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[]) {
   DENTER(TOP_LAYER);

   // the running jobs per user, created with the first counter; the group list is never filled
   ocs::tom::ListPtr<ocs::tom::JC> user_list;
   ocs::tom::ListPtr<ocs::tom::JC> group_list;
   ocs::tom::Element<ocs::tom::JB> orig_job;
   ocs::tom::Element<ocs::tom::CT> cat;
   // the queues split off, created by the sge_split_* functions with the first queue
   ocs::tom::ListPtr<ocs::tom::QU> none_avail_queues;
   ocs::tom::ListPtr<ocs::tom::LDR> consumable_load_list;
   sched_prof_t pi;

   double total_running_job_tickets = 0;
   uint32_t nreservation = 0;
   int fast_runs = 0;         /* sequential jobs */
   int fast_soft_runs = 0;    /* sequential jobs with soft requests */
   int pe_runs = 0;           /* pe jobs */
   int pe_soft_runs = 0;      /* pe jobs  with soft requests */
   int sort_hostlist = 1;     /* hostlist has to be sorted. Info returned by select_assign_debit() */

   /* e.g. if load correction was computed */
   uint32_t max_reserve = sconf_get_max_reservations();
   bool is_schedule_based = max_reserve > 0; // check resource availability from the resource diagram
   uint64_t now = sge_get_gmt64();

   uint32_t queue_sort_method = sconf_get_queue_sort_method();
   uint32_t maxujobs = sconf_get_maxujobs();
   const auto job_load_adjustments = sconf_get_job_load_adjustments();
   sconf_set_host_order_changed(false);

   // the members of the snapshot whose list does not change during the run; the queue lists do
   // (the sge_split_* functions replace them) and are read through their owning handles
   const auto host_list = lists->host_list.get();
   const auto centry_list = lists->centry_list;
   const auto pe_list = lists->pe_list.get();
   const auto ckpt_list = lists->ckpt_list;
   const auto acl_list = ocs::tom::ConstList<ocs::tom::US>(*lists->acl_list);
   const auto hgrp_list = lists->hgrp_list;
   const auto rqs_list = lists->rqs_list.get();
   const auto ar_list = lists->ar_list.get();

   // adjust virtual load added at job start (if configured)
   do_load_adjustment(lists, ocs::tom::ConstList<ocs::tom::JB>(splitted_job_lists[SPLIT_RUNNING]->get()),
                      *job_load_adjustments, evc->monitor_next_run);

   if (max_reserve != 0 || !ar_list.empty()) {
      const auto dis_queue_elem = lists->dis_queue_list->front();
      /*------------------------------------------------------------------------------
       * ENSURE RUNNING JOBS ARE REFLECTED IN PER RESOURCE SCHEDULE (resource diagram)
       * as well as suspended jobs and calendar events
       * Advance reservations are already booked in the resource diagram
       * (by worker threads processing qrsub requests).
       * Resource reservations will be added during the scheduling run.
       *------------------------------------------------------------------------------
       */

      // queues could have been disabled in the meantime, we still need to book into them
      // @todo what about suspended queues etc. - are they also in the dis_queue_list?
      // @todo see ensure_valid_what_and_where()
      // neither list is replaced: the queues move over and back, the disabled queue list stays, also while empty
      // without a queue list nothing moves over and back, as with lAppendList() on a null list
      const bool move_dis_queues = dis_queue_elem != nullptr && lists->queue_list != nullptr;
      if (move_dis_queues) {
         lists->queue_list->splice(*lists->dis_queue_list);
      }

      prepare_resource_schedules(splitted_job_lists[SPLIT_RUNNING]->get(),
                                 splitted_job_lists[SPLIT_SUSPENDED]->get(),
                                 pe_list, host_list, *lists->queue_list,
                                 rqs_list, centry_list, acl_list, hgrp_list, ar_list, true, now);

      if (move_dis_queues) {
         // the disabled queues go back from their first one on; the disabled queue list exists and is not
         // replaced, it takes them at its end
         lists->dis_queue_list->splice(lists->queue_list->split_at(dis_queue_elem));
      }
   }

   /*---------------------------------------------------------------------
    * CAPACITY CORRECTION
    * We reduce the capacity of configured exec host consumables by
    * use detected outside of Cluster Scheduler control
    * (by comparing load value vs. available due to our internal booking).
    *---------------------------------------------------------------------*/
   correct_capacities(host_list, centry_list, ocs::tom::ConstList<ocs::tom::CE>());

   /*---------------------------------------------------------------------
    * KEEP SUSPEND THRESHOLD QUEUES
    *---------------------------------------------------------------------*/
   if (sge_split_queue_load(
           evc->monitor_next_run,
           *lists->queue_list,      /* queue list                             */
           &none_avail_queues,      /* list of queues in suspend alarm state  */
           host_list,
           centry_list,
           *job_load_adjustments,
           ocs::tom::ConstList<ocs::tom::JG>(), false, false,
           QU_suspend_thresholds)) {
      none_avail_queues.reset();
      DPRINTF("couldn't split queue list with regard to suspend thresholds\n");
      DRETURN(-1);
   }

   unsuspend_job_in_queues(lists->queue_list.get(), splitted_job_lists[SPLIT_SUSPENDED]->get(), orders);
   suspend_job_in_queues(none_avail_queues.get(), splitted_job_lists[SPLIT_RUNNING]->get(), orders);
   none_avail_queues.reset();

   /*---------------------------------------------------------------------
    * FILTER QUEUES
    *---------------------------------------------------------------------*/
   /* split queues into overloaded and non-overloaded queues */
   if (sge_split_queue_load(evc->monitor_next_run, *lists->queue_list, nullptr,
                            host_list, centry_list, *job_load_adjustments,
                            ocs::tom::ConstList<ocs::tom::JG>(), false, true, QU_load_thresholds)) {
      DPRINTF("couldn't split queue list concerning load\n");
      DRETURN(-1);
   }

   /* remove cal_disabled queues - needed them for implementing suspend thresholds */
   if (sge_split_cal_disabled(evc->monitor_next_run, lists->queue_list, lists->dis_queue_list)) {
      DPRINTF("couldn't split queue list concerning cal_disabled state\n");
      DRETURN(-1);
   }

   /* trash disabled queues - needed them for implementing suspend thresholds */
   if (sge_split_disabled(evc->monitor_next_run, lists->queue_list, none_avail_queues)) {
      DPRINTF("couldn't split queue list concerning disabled state\n");
      none_avail_queues.reset();
      DRETURN(-1);
   }

   none_avail_queues.reset();
   if (sge_split_queue_slots_free(evc->monitor_next_run, *lists->queue_list, none_avail_queues)) {
      DPRINTF("couldn't split queue list concerning free slots\n");
      none_avail_queues.reset();
      DRETURN(-1);
   }
   if (lists->dis_queue_list != nullptr) {
      lists->dis_queue_list->splice(std::move(none_avail_queues));
   } else {
      lists->dis_queue_list = std::move(none_avail_queues);
   }


   /*---------------------------------------------------------------------
    * FILTER JOBS
    *---------------------------------------------------------------------*/

   DPRINTF("STARTING FILTERING JOBS WITH " sge_u32 " PENDING JOBS\n",
           static_cast<uint32_t>(splitted_job_lists[SPLIT_PENDING]->get().size()));

   // update the running jobs per user (in JC objects)
   user_list_init_jc(user_list, splitted_job_lists);

   /*--------------------------------------------------------------------
    * CALL SGEEE SCHEDULER TO
    * CALCULATE TICKETS FOR EACH JOB  - IN SUPPORT OF SGEEE
    *------------------------------------------------------------------*/
   {
      int ret;
      PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);

      ret = sgeee_scheduler(lists,
                            splitted_job_lists[SPLIT_RUNNING]->get(),
                            splitted_job_lists[SPLIT_FINISHED]->get(),
                            splitted_job_lists[SPLIT_PENDING]->get(),
                            orders);

      sge_schedd_send_orders(orders, orders->configOrderList, nullptr, "A: config orders");
      sge_schedd_send_orders(orders, orders->jobStartOrderList, nullptr, "A: job start orders");
      sge_schedd_send_orders(orders, orders->pendingOrderList, nullptr, "A: pending ticket orders");

      if (prof_is_active(SGE_PROF_CUSTOM1)) {
         prof_stop_measurement(SGE_PROF_CUSTOM1, nullptr);

         PROFILING("PROF: job-order calculation took %.3f s", prof_get_measurement_wallclock(SGE_PROF_CUSTOM1, true, nullptr));
      }

      if (ret != 0) {
         user_list.reset();
         group_list.reset();
         DRETURN(-1);
      }
   }

   // shortcut: all queues are full, no need to continue
   const bool no_queues = lists->queue_list->empty();
   const bool no_dis_queues = lists->dis_queue_list->empty();
   if ((max_reserve == 0 && no_queues) || /* no reservation and no queues avail */
       (no_queues && no_dis_queues)) {    /* reservation and no queues avail */
      DPRINTF("queues dropped because of overload or full: ALL\n");
      schedd_mes_add_global(nullptr, evc->monitor_next_run, SCHEDD_INFO_ALLALARMOVERLOADED_);
      user_list.reset();
      group_list.reset();
      DRETURN(0);
   }

   // we need to do this *after* the SGEEE scheduler to make sure that we keep
   // the jobs with high priority
   job_lists_split_with_reference_to_max_running(evc->monitor_next_run, splitted_job_lists,
                                                 *user_list,
                                                 nullptr,
                                                 maxujobs);

   DPRINTF("AFTER FILTERING WE HAVE " sge_u32 " PENDING JOBS\n",
           static_cast<uint32_t>(splitted_job_lists[SPLIT_PENDING]->get().size()));

   if ((*splitted_job_lists[SPLIT_PENDING])->empty()) {
      /* no jobs to schedule */
      schedd_log(MSG_SCHEDD_MON_NOPENDJOBSTOPERFORMSCHEDULINGON, nullptr, evc->monitor_next_run);
      user_list.reset();
      group_list.reset();
      DRETURN(0);
   }

   /* 
    * Order Jobs in descending order according to tickets and 
    * then job number 
    */
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM3);

   ocs::Job::sgeee_sort_jobs(splitted_job_lists[SPLIT_PENDING]->get());

   if (prof_is_active(SGE_PROF_CUSTOM3)) {
      prof_stop_measurement(SGE_PROF_CUSTOM3, nullptr);

      PROFILING("PROF: job sorting took %.3f s", prof_get_measurement_wallclock(SGE_PROF_CUSTOM3, false, nullptr));
   }

   /*---------------------------------------------------------------------
    * SORT HOSTS
    *---------------------------------------------------------------------*/
   /*
      there are two possibilities for SGE administrators
      selecting queues:

      sort by seq_no
         the sequence number from configuration is used for sorting

      sort by load (using a load formula)
         the least loaded queue gets filled first

         to do this we sort the hosts using the load formula
         because there may be more queues than hosts and
         the queue load is identically to the host load

   */
   switch (queue_sort_method) {
      case QSM_LOAD:
      case QSM_SEQNUM:
      default:

         DPRINTF("sorting hosts by load\n");
         sort_host_list(host_list, centry_list);

         break;
   }

   /* generate a consumable load list structure. It stores which queues
      are using consumables in their load threshold. */
   consumable_load_list = sge_create_load_list(*lists->queue_list, host_list, centry_list);


   /*---------------------------------------------------------------------
    * DISPATCH JOBS TO QUEUES
    *---------------------------------------------------------------------*/

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM4);

   /*
    * loop over the jobs that are left in priority order
    */
   {
      bool is_immediate_array_job = false;
      bool do_prof = prof_is_active(SGE_PROF_CUSTOM4);
      uint64_t tnow = sge_get_gmt64();
      uint32_t min_intermediate_jobs{std::numeric_limits<uint32_t>::max()};
      uint32_t max_intermediate_jobs{0};
      uint32_t avg_intermediate_jobs{0};
      uint32_t num_intermediate_jobs{0};
      uint32_t num_intermediate_sends{0};

      memset(&pi, 0, sizeof(sched_prof_t));

      // the pending list stays the same list during the loop; jobs leave it, it is not replaced
      const auto pending_jobs = splitted_job_lists[SPLIT_PENDING]->get();

      while ((orig_job = pending_jobs.front()) != nullptr) {
         dispatch_t result = DISPATCH_NEVER_CAT;
         uint32_t job_id;
         bool is_pjob_resort = false;
         bool is_reserve;
         bool is_start;

         job_id = ocs::tom::get<JB_job_number>(orig_job);

         /* 
          * We don't try to get a reservation, if 
          * - reservation is generally disabled 
          * - maximum number of reservations is exceeded 
          * - it's not desired for the job
          * - the job is an immediate one
          * - if the job reservation is disabled by category
          */
         if (nreservation < max_reserve &&
             ocs::tom::get<JB_reserve>(orig_job) &&
             !JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(orig_job)) &&
             !ocs::CategorySchedd::job_is_category_reservation_rejected(orig_job)) {
            is_reserve = true;
         } else {
            is_reserve = false;
         }

         // Don't need to look for a 'now' assignment if the last job
         // of this category got no 'now' assignment either
         is_start = ocs::CategorySchedd::job_is_category_rejected(orig_job) == 0;

         if (is_start || is_reserve) {
            /* sort the hostlist */
            if (sort_hostlist) {
               host_list.sort<ocs::tom::asc<EH_sort_value>>();
               sort_hostlist = 0;
               sconf_set_host_order_changed(true);
            } else {
               sconf_set_host_order_changed(false);
            }

            is_immediate_array_job = (is_immediate_array_job ||
                                      (JOB_TYPE_IS_ARRAY(ocs::tom::get<JB_type>(orig_job)) &&
                                       JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(orig_job))));


            const auto job = orig_job.clone();
            if (!job) {
               break;
            }

            const auto next_task = job_get_next_task(*job);
            if (!next_task) {
               DPRINTF("Found job " sge_u32 " with no job array tasks\n", job_id);
            } else {
               const auto ja_task = next_task->first;
               const uint32_t ja_task_id = next_task->second;
               DPRINTF("Found pending job " sge_u32 "." sge_u32 ". Try %sto start and %sto reserve\n",
                       job_id, ja_task_id, is_start ? "" : "not ", is_reserve ? "" : "not ");
               DPRINTF("-----------------------------------------\n");

               result = select_assign_debit(
                       lists->queue_list,
                       lists->dis_queue_list,
                       *job, ja_task,
                       pe_list,
                       ckpt_list,
                       centry_list,
                       host_list,
                       acl_list,
                       *job_load_adjustments,
                       user_list,
                       group_list,
                       orders,
                       &total_running_job_tickets,
                       &sort_hostlist,
                       is_start,
                       is_reserve,
                       is_schedule_based,
                       consumable_load_list,
                       hgrp_list,
                       rqs_list,
                       ar_list,
                       do_prof ? &pi : nullptr,
                       evc->monitor_next_run,
                       now);
            }
         } else {
            DPRINTF("SKIP JOB " sge_u32 " - it cannot be started now and we will not do a reservation\n", job_id);
         }

         // collect profiling data - only if we actually did some scheduling
         if (is_start || is_reserve) {
            switch (sconf_get_last_dispatch_type()) {
               case DISPATCH_TYPE_FAST :
                  fast_runs++;
               break;
               case DISPATCH_TYPE_FAST_SOFT_REQ :
                  fast_soft_runs++;
               break;
               case DISPATCH_TYPE_PE :
                  pe_runs++;
               break;
               case DISPATCH_TYPE_PE_SOFT_REQ:
                  pe_soft_runs++;
               break;
            }
         }

         switch (result) {
            case DISPATCH_OK: /* now assignment */
            {
               char *owner = strdup(ocs::tom::get<JB_owner>(orig_job));
               /* here the job got an assignment that will cause it be started immediately */

               DPRINTF("Found NOW assignment\n");

               schedd_mes_rollback();
               if (job_count_pending_tasks(orig_job, true) < 2)
                  is_pjob_resort = false;
               else
                  is_pjob_resort = true;

               // job_move_first_pending_to_running() may free the job; it empties orig_job then
               job_move_first_pending_to_running(orig_job, splitted_job_lists);

               /* 
                * after sge_move_to_running() orig_job can be removed and job 
                * should be used instead 
                */
               orig_job = nullptr;

               /* 
                * drop idle jobs that exceed maxujobs limit 
                * should be done after resort_job() 'cause job is referenced 
                */
               job_lists_split_with_reference_to_max_running(evc->monitor_next_run, splitted_job_lists,
                                                             *user_list,
                                                             owner,
                                                             maxujobs);
               sge_free(&owner);

               // do not send job start orders in between, if we have an immediate array job.
               // and only if we have at least 10 jobs to start (why?)
               const auto num_jobs = static_cast<uint32_t>(orders->jobStartOrderList->size());
               if (!is_immediate_array_job  && num_jobs > 0 /* 10 */) {
                  uint64_t later = sge_get_gmt64();
                  uint64_t passed = later - tnow;

                  // send orders every 100ms
                  if (passed > 100 * 1000) {
                     ocs::tom::ListPtr<ocs::tom::AN> answer_list;
                     // Why the config and pending order here? Send them earlier or at the end!
                     // We actually send them before the loop over all jobs, so the calls here won't do anything.
                     if (do_prof) {
                        min_intermediate_jobs = std::min(min_intermediate_jobs, num_jobs);
                        max_intermediate_jobs = std::max(max_intermediate_jobs, num_jobs);
                        avg_intermediate_jobs = (avg_intermediate_jobs * num_intermediate_sends + num_jobs) /
                                                (num_intermediate_sends + 1);
                        num_intermediate_jobs += num_jobs;
                        num_intermediate_sends++;
                     }

                     sge_schedd_send_orders(orders, orders->configOrderList, &answer_list, "B: config orders");
                     sge_schedd_send_orders(orders, orders->jobStartOrderList, &answer_list, "B: job start orders");
                     sge_schedd_send_orders(orders, orders->pendingOrderList, &answer_list, "B: pending ticket orders");
                     answer_list_output(&answer_list);
                     tnow = later;
                  }
               }
            }
            break;

            case DISPATCH_NOT_AT_TIME: /* reservation */
               /* here the job got a reservation but can't be started now */
               DPRINTF("Got a RESERVATION\n");
               nreservation++;

               /* mark the category as rejected */
               // the category reference of the job is set by refresh_cat_data_in_job(): taken unchecked
               cat = ocs::tom::Element<ocs::tom::CT>::from_typed(
                       static_cast<lListElem *>(ocs::tom::get<JB_category>(orig_job)));
               if (cat) {
                  DPRINTF("SKIP JOB (R)" sge_u32 " of category '%s' (rc: " sge_u32 ")\n", job_id,
                          ocs::tom::get<CT_str>(cat), ocs::tom::get<CT_refcount>(cat));
                  ocs::CategorySchedd::job_reject_category(orig_job, false);
               }
               /* here no reservation was made for a job that couldn't be started now
                  or the job is not dispatch-able at all */
               schedd_mes_commit(pending_jobs, 0, cat);

               /* Remove pending job if there are no pending tasks anymore (including the current) */
               if (job_count_pending_tasks(orig_job, true) < 2 || (nreservation >= max_reserve)) {

                  auto not_started_job = pending_jobs.extract(orig_job);
                  if ((*(splitted_job_lists[SPLIT_NOT_STARTED])) == nullptr) {
                     // the list belongs to the caller and gets the layout of the (possibly reduced) jobs
                     *(splitted_job_lists[SPLIT_NOT_STARTED]) =
                             ocs::tom::ListPtr<ocs::tom::JB>::create_like("", pending_jobs);
                  }
                  (*(splitted_job_lists[SPLIT_NOT_STARTED]))->push_back(std::move(not_started_job));
                  is_pjob_resort = false;
               } else {
                  uint32_t ja_task_number = range_list_get_first_id(ocs::tom::get<JB_ja_n_h_ids>(orig_job), nullptr);
                  object_delete_range_id<JB_ja_n_h_ids>(orig_job, nullptr, ja_task_number);
                  is_pjob_resort = true;
               }
               orig_job = nullptr;
               break;

            case DISPATCH_NEVER_CAT: /* never this category */
               if (is_start || is_reserve) {
                  // before deleting the element mark the category as rejected
                  // do this only once, not when we skipped scheduling as the category was already rejected
                  // the category reference of the job is set by refresh_cat_data_in_job(): taken unchecked
                  cat = ocs::tom::Element<ocs::tom::CT>::from_typed(
                          static_cast<lListElem *>(ocs::tom::get<JB_category>(orig_job)));
                  if (cat) {
                     DPRINTF("SKIP JOB (N)" sge_u32 " of category '%s' (rc: " sge_u32 ")\n", job_id,
                             ocs::tom::get<CT_str>(cat), ocs::tom::get<CT_refcount>(cat));
                     ocs::CategorySchedd::job_reject_category(orig_job, is_reserve);
                  }
               }
               /* fall through to DISPATCH_NEVER_JOB */

            case DISPATCH_NEVER_JOB: /* never this particular job */
               if (is_start || is_reserve) {
                  DPRINTF("SKIP JOB (J)" sge_u32 "\n", job_id);
                  // here no reservation was made for a job that couldn't be started now
                  // or the job cannot be dispatched at all
                  // do this only once, not when we skipped scheduling as the category was already rejected
                  schedd_mes_commit(pending_jobs, 0, cat);
               }

               if (JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(orig_job))) { /* immediate job */
                  /* remove job from pending list and generate remove immediate orders
                     for all tasks including already assigned ones */
                  remove_immediate_job(pending_jobs, orig_job, orders, 0);
                  // a running job is in the list once, so the first match by job number is the job
                  const auto running_jobs = splitted_job_lists[SPLIT_RUNNING]->get();
                  const auto rjob = running_jobs.find_first<JB_job_number>(job_id);
                  if (rjob != nullptr) {
                     remove_immediate_job(running_jobs, rjob, orders, 1);
                  }
               } else {
                  /* prevent that we get the same job next time again */
                  auto not_started_job = pending_jobs.extract(orig_job);
                  if ((*(splitted_job_lists[SPLIT_NOT_STARTED])) == nullptr) {
                     // the list belongs to the caller and gets the layout of the (possibly reduced) jobs
                     *(splitted_job_lists[SPLIT_NOT_STARTED]) =
                             ocs::tom::ListPtr<ocs::tom::JB>::create_like("", pending_jobs);
                  }
                  (*(splitted_job_lists[SPLIT_NOT_STARTED]))->push_back(std::move(not_started_job));
               }
               orig_job = nullptr;
               break;

            case DISPATCH_MISSING_ATTR :  /* should not happen */
               DPRINTF("DISPATCH_MISSING_ATTR\n");
            default:
               break;
         }

         /*------------------------------------------------------------------ 
          * SGEEE mode - if we dispatch a job sub-task and the job has more
          * sub-tasks, then the job is still first in the job list.
          * We need to remove and reinsert the job back into the sorted job
          * list in case another job is higher priority (i.e. has more tickets)
          *------------------------------------------------------------------*/

         /* no more free queues - then exit */
         if (lists->queue_list->empty()) {
            DPRINTF("no more free queues\n");
            break;
         }

         if (is_pjob_resort) {
            sgeee_resort_pending_jobs(splitted_job_lists[SPLIT_PENDING]->get());
         }

      } /* end of while */

      if (do_prof) {
         PROFILING("PROF: " sge_u32" intermediate start orders were sent in " sge_u32 " sends. Min: " sge_u32
                   ", Max: " sge_u32 ", Avg: " sge_u32,
                   num_intermediate_jobs, num_intermediate_sends,
                   num_intermediate_sends > 0 ? min_intermediate_jobs : 0,
                   max_intermediate_jobs, avg_intermediate_jobs);
      }
   }

   if (prof_is_active(SGE_PROF_CUSTOM4)) {
      static bool first_time = true;
      prof_stop_measurement(SGE_PROF_CUSTOM4, nullptr);
      PROFILING("PROF: job dispatching took %.3f s (%d fast, %d fast_soft, %d pe, %d pe_soft, " sge_u32 " res)",
              prof_get_measurement_wallclock(SGE_PROF_CUSTOM4, false, nullptr),
              fast_runs,
              fast_soft_runs,
              pe_runs,
              pe_soft_runs,
              nreservation);

      if (first_time) {
         PROFILING("PROF: parallel matching   %12.12s %12.12s %12.12s %12.12s %12.12s %12.12s %12.12s",
                 "global", "rqs", "cqstatic", "hstatic",
                 "qstatic", "hdynamic", "qdyn");
         PROFILING("PROF: sequential matching %12.12s %12.12s %12.12s %12.12s %12.12s %12.12s %12.12s",
                 "global", "rqs", "cqstatic", "hstatic",
                 "qstatic", "hdynamic", "qdyn");
         first_time = false;
      }
      PROFILING("PROF: parallel matching   %12d %12d %12d %12d %12d %12d %12d",
              pi.par_global, pi.par_rqs, pi.par_cqstat, pi.par_hstat,
              pi.par_qstat, pi.par_hdyn, pi.par_qdyn);
      PROFILING("PROF: sequential matching %12d %12d %12d %12d %12d %12d %12d",
              pi.seq_global, pi.seq_rqs, pi.seq_cqstat, pi.seq_hstat,
              pi.seq_qstat, pi.seq_hdyn, pi.seq_qdyn);
   }

   user_list.reset();
   group_list.reset();
   consumable_load_list.reset();

   /* reset last dispatch type. Indicator for sorting queues */
   sconf_set_last_dispatch_type(DISPATCH_TYPE_NONE);

   DRETURN(0);
}

/**
 * @brief Selects resources for 'job', add appropriate order to the 'orders_list',
 *
 * @param queue_list the queues that can take jobs (`QU`); queues that become full move to @p dis_queue_list,
 *        a list left empty is freed, and for a reservation a missing one is created
 * @param dis_queue_list the queues that cannot take jobs now (`QU`), created when the first queue moves there
 * @param job a copy of the job being scheduled
 * @param ja_task the task being scheduled
 * @param pe_list the parallel environments
 * @param ckpt_list the checkpointing environments
 * @param centry_list the complex entries
 * @param host_list the execution hosts
 * @param acl_list the access lists
 * @param load_adjustments the job load adjustments
 * @param user_list the running jobs per user (`JC`), increased for a started job, created with the first one
 * @param group_list the running jobs per group (`JC`), unused
 * @param orders receives the start order
 * @param total_running_job_tickets increased by the tickets of a started job
 * @param sort_hostlist set when the host list has to be sorted again
 * @param is_start try to find a now assignment
 * @param is_reserve try to find a reservation assignment
 * @param is_schedule_based whether resource availability is checked in the resource diagram
 * @param load_list the consumable load list (`LDR`), queues that become unusable are removed from it; freed
 *        when it becomes empty
 * @param hgrp_list the host groups
 * @param rqs_list the resource quota sets
 * @param ar_list the advance reservations
 * @param pi the profiling counters, or nullptr
 * @param monitor_next_run whether the scheduling run is monitored
 * @param now the time of the scheduling run
 *
 * @return 0 ok got an assignment now 1 got a reservation assignment -1 will never get an assignment for that
 *         category -2 will never get an assignment for that particular job
 */
static dispatch_t
select_assign_debit(ocs::tom::ListPtr<ocs::tom::QU> &queue_list, ocs::tom::ListPtr<ocs::tom::QU> &dis_queue_list,
                    const ocs::tom::Element<ocs::tom::JB> job, const ocs::tom::Element<ocs::tom::JAT> ja_task,
                    const ocs::tom::List<ocs::tom::PE> pe_list, const ocs::tom::ConstList<ocs::tom::CK> ckpt_list,
                    const ocs::tom::ConstList<ocs::tom::CE> centry_list, const ocs::tom::List<ocs::tom::EH> host_list,
                    const ocs::tom::ConstList<ocs::tom::US> acl_list,
                    const ocs::tom::List<ocs::tom::CE> load_adjustments, ocs::tom::ListPtr<ocs::tom::JC> &user_list,
                    ocs::tom::ListPtr<ocs::tom::JC> &group_list, order_t *orders, double *total_running_job_tickets,
                    int *sort_hostlist,
                    const bool is_start, const bool is_reserve, const bool is_schedule_based,
                    ocs::tom::ListPtr<ocs::tom::LDR> &load_list, const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list,
                    const ocs::tom::List<ocs::tom::RQS> rqs_list, const ocs::tom::List<ocs::tom::AR> ar_list,
                    sched_prof_t *pi, const bool monitor_next_run, const uint64_t now) {
   DENTER(TOP_LAYER);
   dispatch_t result = DISPATCH_NEVER_CAT;
   const char *pe_name, *ckpt_name;
   sge_assignment_t a{};
   bool is_computed_reservation = false;

   assignment_init(&a, job, ja_task, load_adjustments);
   assignment_init_ar(&a, ar_list);
   a.queue_list = queue_list.get();
   a.host_list = host_list;
   a.centry_list = centry_list;
   a.acl_list = acl_list;
   a.hgrp_list = hgrp_list;
   a.rqs_list = rqs_list;
   a.pi = pi;
   a.monitor_next_run = monitor_next_run;
   a.now = now;

   /* in reservation scheduling mode a non-zero duration always must be defined */
   job_get_duration(&a.duration, job);

   if (is_reserve) {
      if (queue_list == nullptr) {
         // the list gets the reduced layout of the disabled queues
         queue_list = ocs::tom::ListPtr<ocs::tom::QU>::create_like("temp queue", *dis_queue_list);
         a.queue_list = queue_list.get();
      }
      is_computed_reservation = true;
      // the disabled queue list is not replaced, it stays, also while empty
      queue_list->splice(*dis_queue_list);
   }

   a.duration = duration_add_offset(a.duration, sge_gmt32_to_gmt64(sconf_get_duration_offset()));
   a.is_schedule_based = is_schedule_based;

   /*------------------------------------------------------------------ 
    * seek for ckpt interface definition requested by the job 
    * in case of ckpt jobs 
    *------------------------------------------------------------------*/
   if ((ckpt_name = ocs::tom::get<JB_checkpoint_name>(job))) {
      a.ckpt = ocs::tom::ConstElement<ocs::tom::CK>(ckpt_list_locate(ckpt_list, ckpt_name));
      if (a.ckpt == nullptr) {
         schedd_mes_add(a.monitor_alpp, a.monitor_next_run, a.job_id, SCHEDD_INFO_CKPTNOTFOUND_);
         assignment_release(&a);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /*------------------------------------------------------------------ 
    * if a global host pointer exists it is needed everywhere 
    * down in the assignment code (tired of searching/passing it).
    *------------------------------------------------------------------*/
   a.gep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(host_list, SGE_GLOBAL_NAME));

   // in case of running in an AR, we swap resources to the AR state
   sge_ar_swap_resource_lists(a);

   if ((pe_name = ocs::tom::get<JB_pe>(job)) != nullptr) {
      /*------------------------------------------------------------------
       * SELECT POSSIBLE QUEUE(S) FOR THIS PE JOB
       *------------------------------------------------------------------*/

      if (is_start) {

         DPRINTF("LOOKING FOR IMMEDIATE PARALLEL ASSIGMENT FOR JOB "
                 sge_u32"." sge_u32 " REQUESTING PE \"%s\" DURATION " sge_u64 "\n",
                 a.job_id, a.ja_task_id, pe_name, a.duration);

         a.start = DISPATCH_TIME_NOW;
         a.is_reservation = false;
         result = sge_select_parallel_environment(&a, pe_list);
#if ENABLE_DEBUG_CHECKS
         if (result == DISPATCH_NOT_AT_TIME) {
            // CS-1108: once all cleanup has been done, sge_select_parallel_environment
            // will no longer return DISPATCH_NOT_AT_TIME but DISPATCH_NEVER_CAT
            CRITICAL("===> we should no longer get DISPATCH_NOT_AT_TIME (parallel)");
            ocs::TerminationManager::trigger_abort();
         }
#endif
      }


      // when DISPATCH_NEVER_JOB is returned this means that the job is not scheduled as it is in reschedule_unknown
      // lists - shall we then do a reservation? So far (before CS-1108) this has not been done.
      if (result == DISPATCH_NEVER_CAT) {
         if (is_reserve) {
            DPRINTF("LOOKING FOR PARALLEL RESERVATION FOR JOB "
                    sge_u32"." sge_u32 " REQUESTING PE \"%s\" DURATION " sge_u32 "\n",
                    a.job_id, a.ja_task_id, pe_name, a.duration);
            is_computed_reservation = true;
            a.start = DISPATCH_TIME_QUEUE_END;
            a.is_reservation = true;
            assignment_clear_cache(&a);

            result = sge_select_parallel_environment(&a, pe_list);

            if (result == DISPATCH_OK) {
               result = DISPATCH_NOT_AT_TIME; /* this job got a reservation */
            }
         } else {
            result = DISPATCH_NEVER_CAT;
         }
      }

   } else {
      /*------------------------------------------------------------------
       *  POSSIBLE QUEUE(S)select FOR THIS SEQUENTIAL JOB
       *------------------------------------------------------------------*/

      a.slots = 1;

      if (is_start) {
         DPRINTF("LOOKING FOR IMMEDIATE SEQUENTIAL ASSIGNMENT OF JOB "
                 sge_u32"." sge_u32 " DURATION " sge_u64 "\n", a.job_id, a.ja_task_id, a.duration);

         a.start = DISPATCH_TIME_NOW;
         a.is_reservation = false;
         result = sge_sequential_assignment(&a);

         DPRINTF("sge_sequential_assignment(immediate) returned %d\n", result);
#if ENABLE_DEBUG_CHECKS
         if (result == DISPATCH_NOT_AT_TIME) {
            // CS-1108: once all cleanup has been done, sge_select_parallel_environment
            // will no longer return DISPATCH_NOT_AT_TIME but DISPATCH_NEVER_CAT
            CRITICAL("===> we should no longer get DISPATCH_NOT_AT_TIME (sequential)");
            ocs::TerminationManager::trigger_abort();
         }
#endif
      }

      /* try to reserve for jobs that can be dispatched with the current configuration */
      if (result == DISPATCH_NEVER_CAT) {
         if (is_reserve) {
            DPRINTF("LOOKING FOR SEQUENTIAL RESERVATION FOR JOB "
                            sge_u32"." sge_u32 " DURATION " sge_u64 "\n",
                            a.job_id, a.ja_task_id, a.duration);
            a.start = DISPATCH_TIME_QUEUE_END;
            a.is_reservation = true;
            assignment_clear_cache(&a);

            result = sge_sequential_assignment(&a);
            if (result == DISPATCH_OK) {
               result = DISPATCH_NOT_AT_TIME; /* this job got a reservation */
            }
         } else {
            result = DISPATCH_NEVER_CAT;
         }
      }
   }

   // Choose the resource map instances while the hosts still show what matching worked from.
   // Inside an advance reservation that is the reservation's own capacity and utilization,
   // which the swap below puts back; choosing afterwards would read the host's real lists and
   // could reach a different set than matching had in mind. The booking further down applies
   // what is chosen here. A failure is not acted on yet - the swap has to happen first, or the
   // reservation's lists stay in the scheduler's host list - so it is left to
   // add_granted_resource_list(), which refuses a map that has no instances recorded.
   if (result == DISPATCH_OK || result == DISPATCH_NOT_AT_TIME) {
      select_granted_rsmap_instances(&a, ja_task, job, host_list);
   }

   // in case of running in an AR, we swap resources back to the original state
   sge_ar_swap_resource_lists(a);

   /*------------------------------------------------------------------
    * BUILD ORDERS LIST THAT TRANSFERS OUR DECISIONS TO QMASTER
    *------------------------------------------------------------------*/
   if (result == DISPATCH_NEVER_CAT || result == DISPATCH_NEVER_JOB) {
      /* failed scheduling this job */
      if (JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(job))) { /* immediate job */
         /* generate order for removing it at qmaster */
         order_remove_immediate(job, ja_task, orders);
      }
      assignment_release(&a);
      DRETURN(result);
   }

   // The granted resource list carries the resource map instances into the booking, and a
   // reservation has to book them as much as a job which starts: a reservation which held an
   // amount and no instances would let a later job take the very instances it is waiting for,
   // and the job would find the constraint unsatisfiable when its time came. DISPATCH_NOT_AT_TIME
   // means a reservation was found, so a.gdil is the reserved assignment and a.start its time.
   if (result == DISPATCH_OK || result == DISPATCH_NOT_AT_TIME) {
      // create the granted resource list containing all granted consumables
      // including RSMAPs and the info which RSMAP ids were granted
      if (!add_granted_resource_list(&a, ja_task, job, host_list)) {
         // A resource map could not be granted on a host which matching had selected - only a
         // resource map reaches this, every other consumable is debited from the request and
         // has nothing to grant. Matching and booking have therefore disagreed about the same
         // data, which is a fault in the scheduler and not a host which filled up.
         //
         // The task is not started. It used to be, with whatever could be granted: for a
         // resource map that means the instance ids, so the job would run with SGE_HGR_<name>
         // empty and no device isolation, holding a card the accounting says it holds and the
         // system never gave it. Leaving the job pending is the lesser fault, and the reason is
         // on the job (see gru_report_booking_failure) rather than only in the messages file.
         //
         // A reservation which cannot be given its instances is simply not made. The same
         // answer serves: DISPATCH_NEVER_JOB puts the job aside for this run, and because the
         // reservation is abandoned rather than recorded, nreservation is not spent on it.
         // Rejecting the whole category would be wrong here - another job in it may well
         // reserve, on another host or at another time.
         //
         // Nothing is committed at this point - the start order is built below and the
         // resources are debited after it - so returning here leaves the cluster as it was.
         // The partially filled list is taken off the task so that a later run does not find
         // it.
         ocs::tom::set<JAT_granted_resources_list>(ja_task, nullptr);
         assignment_release(&a);
         DRETURN(DISPATCH_NEVER_JOB);
      }
   }

   if (result == DISPATCH_OK) {
      /* in SGEEE we must account for job tickets on hosts due to parallel jobs */
      {
         double job_tickets_per_slot;
         double job_ftickets_per_slot;
         double job_otickets_per_slot;
         double job_stickets_per_slot;

         job_tickets_per_slot = (double) ocs::tom::get<JAT_tix>(ja_task) / a.slots;
         job_ftickets_per_slot = (double) ocs::tom::get<JAT_fticket>(ja_task) / a.slots;
         job_otickets_per_slot = (double) ocs::tom::get<JAT_oticket>(ja_task) / a.slots;
         job_stickets_per_slot = (double) ocs::tom::get<JAT_sticket>(ja_task) / a.slots;

         for (const auto granted_el : *a.gdil) {
            uint32_t granted_slots = ocs::tom::get<JG_slots>(granted_el);
            ocs::tom::set<JG_ticket>(granted_el, job_tickets_per_slot * granted_slots);
            ocs::tom::set<JG_oticket>(granted_el, job_otickets_per_slot * granted_slots);
            ocs::tom::set<JG_fticket>(granted_el, job_ftickets_per_slot * granted_slots);
            ocs::tom::set<JG_sticket>(granted_el, job_stickets_per_slot * granted_slots);
         }
         *total_running_job_tickets += ocs::tom::get<JAT_tix>(ja_task);
      }

      if (a.pe) {
         DPRINTF("got PE %s\n", ocs::tom::get<PE_name>(a.pe));
         ocs::tom::set<JAT_granted_pe>(ja_task, ocs::tom::get<PE_name>(a.pe));
      }

      sge_create_orders(orders->jobStartOrderList, ORT_start_job, job, ja_task, *a.gdil, true);

      /* increase the number of jobs a user/group has running */
      sge_inc_jc(user_list, ocs::tom::get<JB_owner>(job), 1);
   }

   /*------------------------------------------------------------------
    * DEBIT JOBS RESOURCES IN DIFFERENT OBJECTS
    *
    * We have not enough time to wait till qmaster updates our lists.
    * So we do the work by ourselfs for being able to send more than
    * one order per scheduling epoch.
    *------------------------------------------------------------------*/

   /* when a job is started *now* the RUE_utilized_now must always change */
   /* if jobs with reservations are ahead then we also must change the RUE_utilized */
   if (result == DISPATCH_OK) {
      if (a.start == DISPATCH_TIME_NOW) {
         a.start = now;
      }
      debit_scheduled_job(&a, sort_hostlist, orders, true,
                          SCHEDULING_RECORD_ENTRY_TYPE_STARTING, true);
   } else {
      debit_scheduled_job(&a, sort_hostlist, orders, false,
                          SCHEDULING_RECORD_ENTRY_TYPE_RESERVING, true);
   }

   /*------------------------------------------------------------------
    * REMOVE QUEUES THAT ARE NO LONGER USEFUL FOR FURTHER SCHEDULING
    *------------------------------------------------------------------*/
   if (result == DISPATCH_OK || is_computed_reservation) {
      /* @todo (CS-449) is_computed_reservation might be incorrect (it is initialized to true for every job
       *    which requests a reservation, is again set to true for parallel jobs but not for
       *    sequential jobs). And it is true even if no reservation could be found.
       * @todo (CS-449) can the fact that a reservation was done lead to queues in suspended or alarm state?
       *    It does not consume slots *now*, so does it have any effect on sge_split_queue_slots_free()?
       */
      ocs::tom::ListPtr<ocs::tom::QU> disabled_queues;

      if (load_list == nullptr) {
         sge_split_suspended(monitor_next_run, queue_list, dis_queue_list);
         sge_split_queue_slots_free(monitor_next_run, *queue_list, dis_queue_list);
      } else {
         sge_split_suspended(monitor_next_run, queue_list, disabled_queues);
         sge_split_queue_slots_free(monitor_next_run, *queue_list, disabled_queues);

         sge_remove_queue_from_load_list(load_list, *disabled_queues);
         if (dis_queue_list == nullptr) {
            dis_queue_list = std::move(disabled_queues);
         } else {
            dis_queue_list->splice(std::move(disabled_queues));
         }
      }

      // clear the QU_tagged4schedule - we re/mis-use it in sge_load_list_alarm() and sge_split_queue_load()
      qinstance_list_set_tag<QU_tagged4schedule>(*queue_list, 0);

      bool is_consumable_load_alarm = sge_load_list_alarm(monitor_next_run, *load_list, host_list, centry_list);

      /* split queues into overloaded and non-overloaded queues */
      if (sge_split_queue_load(
              monitor_next_run,
              *queue_list,                                    /* source list                              */
              &disabled_queues,
              host_list,                                      /* host list contains load values           */
              centry_list,
              a.load_adjustments,
              *a.gdil,
              is_consumable_load_alarm,
              false, QU_load_thresholds)) {   /* use load thresholds here */

         DPRINTF("couldn't split queue list concerning load\n");
         assignment_release(&a);
         DRETURN(DISPATCH_NEVER);
      }
      if (load_list != nullptr) {
         sge_remove_queue_from_load_list(load_list, *disabled_queues);
      }

      if (dis_queue_list == nullptr) {
         dis_queue_list = std::move(disabled_queues);
      } else {
         dis_queue_list->splice(std::move(disabled_queues));
      }
   }

   /* no longer needed - having built the order 
      and debited the job everywhere */
   assignment_release(&a);
   DRETURN(result);
}


