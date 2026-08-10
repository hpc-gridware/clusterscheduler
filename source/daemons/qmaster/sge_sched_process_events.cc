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
 * @brief Deciding which events should trigger a scheduling run
 */
#include <cstring>
#include <pthread.h>

#ifdef SOLARISAMD64
#  include <sys/stream.h>
#endif  

/* common/ */
#include <cinttypes>

#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_schedd_conf.h"

#include "mir/sge_mirror.h"
#include "evc/sge_event_client.h"

#include "sge_sched_process_events.h"
#include "sge_sched_prepare_data.h"

/**
 * @brief Hand a batch of freshly delivered events to the scheduler thread
 *
 * The event client's delivery callback. It does not process anything: it moves
 * the events out of `event_list` into #Scheduler_Control under the mutex, sets
 * `triggered` and signals the condition variable, so the scheduler thread wakes
 * up and does the work. Delivery must not block on a scheduling run.
 *
 * The list is moved with lXchgList() rather than copied, so the caller's
 * `event_list` is left empty.
 *
 * @param ec_id the event client id the events were delivered for
 * @param alpp answer list
 * @param event_list a report list, the event are stored in REP_list
 * @param arg unused
 *
 * @note MT-NOTE: is MT safe.
 */
void event_update_func(uint32_t ec_id, lList **alpp, lList *event_list, void *arg) {
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);
   
   if (Scheduler_Control.new_events != nullptr) {
      lList *events = nullptr;
      lXchgList(lFirstRW(event_list), REP_list, &(events));
      lAddList(Scheduler_Control.new_events, &events);
   } else {
      lXchgList(lFirstRW(event_list), REP_list, &(Scheduler_Control.new_events));
   }   
   
   Scheduler_Control.triggered = true;

   DPRINTF("EVENT UPDATE FUNCTION event_update_func() HAS BEEN TRIGGERED\n");

   pthread_cond_signal(&Scheduler_Control.cond_var);

   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);

   DRETURN_VOID;
}

/*********************************************/
/*  event client registration stuff          */
/** @brief Decide whether a job change should trigger a scheduling run
 *
 * @param evc see the description above
 */
void set_job_flushing(sge_evc_class_t *evc) {
   int interval;
   bool flush;

   interval= sconf_get_flush_submit_sec();
   flush = (interval > 0) ? true : false;
   interval--;
   evc->ec_set_flush(evc, sgeE_JOB_ADD, flush, interval);

   interval = sconf_get_flush_finish_sec();
   flush = (interval > 0) ? true : false;
   interval--;
   evc->ec_set_flush(evc, sgeE_JOB_DEL, flush, interval);
   evc->ec_set_flush(evc, sgeE_JOB_FINAL_USAGE, flush, interval);
   evc->ec_set_flush(evc, sgeE_JATASK_DEL, flush, interval);
}

/** @brief Subscribe the scheduler to the events it needs, with the right flush delays
 *
 * The scheduler does not want every event immediately. Subscribing without a
 * flush means the event master delivers on its regular interval; the events
 * that should shorten a scheduling interval - a job submitted, a job finished -
 * are subscribed with a flush delay taken from `flush_submit_sec` /
 * `flush_finish_sec`, so that a new run starts soon after they happen rather
 * than at the end of the interval.
 *
 * The subscription also decides what the mirror keeps, so it has to cover
 * exactly the lists in #scheduler_all_data_t and no more.
 *
 * @param evc the event client the scheduler runs as
 * @param where_what the `where`/`what` filters limiting each subscription to
 *                   the fields the scheduler actually reads
 * @return 0 on success
 */
int subscribe_scheduler(sge_evc_class_t *evc, sge_where_what_t *where_what) {
   DENTER(TOP_LAYER);

   /* subscribe event types for the mirroring interface */
   sge_mirror_subscribe(evc, SGE_TYPE_AR,             nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_CKPT,           nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_CENTRY,         nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_CQUEUE,         nullptr, nullptr, nullptr, where_what->where_cqueue, where_what->what_cqueue);
   sge_mirror_subscribe(evc, SGE_TYPE_EXECHOST,       nullptr, nullptr, nullptr, where_what->where_host, where_what->what_host);
   sge_mirror_subscribe(evc, SGE_TYPE_HGROUP,         nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_CONFIG,         nullptr, sge_process_global_config_event, nullptr, where_what->where_config, where_what->what_config);
   sge_mirror_subscribe(evc, SGE_TYPE_JOB,            nullptr, sge_process_job_event_after, nullptr, where_what->where_job, where_what->what_job);
   sge_mirror_subscribe(evc, SGE_TYPE_JATASK,         nullptr, nullptr, nullptr, where_what->where_jat, where_what->what_jat);
   sge_mirror_subscribe(evc, SGE_TYPE_PE,             nullptr, nullptr, nullptr, nullptr, where_what->what_pe);
   sge_mirror_subscribe(evc, SGE_TYPE_CATEGORY,       nullptr, nullptr, nullptr, nullptr, nullptr);

   /* we do *not* subscribe reduced elements for TYPE_PETASK:
    * event master currently cannot handle this, see IZ 3216
    * sge_mirror_subscribe(evc, SGE_TYPE_PETASK,         nullptr, nullptr, nullptr, nullptr, where_what->what_pet);
    */
   sge_mirror_subscribe(evc, SGE_TYPE_PETASK,         nullptr, nullptr, nullptr, nullptr, nullptr);

   sge_mirror_subscribe(evc, SGE_TYPE_PROJECT,        nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_QINSTANCE,      nullptr, nullptr, nullptr, where_what->where_all_queue, where_what->what_queue);
   sge_mirror_subscribe(evc, SGE_TYPE_RQS,            nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_SCHEDD_CONF,    nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_SCHEDD_MONITOR, nullptr, sge_process_schedd_monitor_event, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_SHARETREE,      nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_USER,           nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_USERSET,        nullptr, nullptr, nullptr, nullptr, nullptr);

   set_job_flushing(evc);

   /* configuration changes and trigger should have immediate effevc->ect */
   evc->ec_set_flush(evc, sgeE_SCHED_CONF, true, 0);
   evc->ec_set_flush(evc, sgeE_SCHEDDMONITOR, true, 0);
   evc->ec_set_flush(evc, sgeE_CONFIG_MOD, true, 0);

   DRETURN(true);
}
