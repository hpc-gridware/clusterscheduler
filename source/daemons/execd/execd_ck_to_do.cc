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
 * @brief Everything the daemon does between messages: reaping, resending, reporting
 */
#include <cfloat>
#include <cstring>
#include <cerrno>
#include <cstdlib>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/ocs_TerminationManager.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_str.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "spool/classic/read_write_job.h"

#include "reaper_execd.h"
#include "job_report_execd.h"
#include "load_avg.h"
#include "execd_ck_to_do.h"
#include "execd_signal_queue.h"
#include "exec_job.h"
#include "execution_states.h"
#include "sig_handlers.h"
#include "sge.h"

#include "msg_common.h"
#include "msg_execd.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#  ifdef DEBUG_DC
#     include "sgedefs.h"
#     include "exec_ifm.h"
#  endif
#endif

extern volatile int waiting4osjid;

static bool 
sge_execd_ja_task_is_tightly_integrated(ocs::tom::ConstElement<ocs::tom::JAT> ja_task);
static bool
sge_kill_petasks(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::JAT> ja_task);

static int sge_start_jobs();
static int exec_job_or_task(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatep,
                            ocs::tom::Element<ocs::tom::PET> petep);

/**
 * @brief Record in the job report that sge_execd killed the job
 *
 * Records in the job report that sge_execd killed the job because it exceeded
 * a resource limit (e.g. h_rt, h_cpu, h_vmem). The string is added to the
 * accounting record as "execd@<hostname>" once the job report reaches
 * sge_qmaster, unless a value set by qdel takes precedence.
 *
 * @param jobid the job
 * @param jataskid the array task
 */
static void
job_report_set_deleted_by_execd(const uint32_t jobid, const uint32_t jataskid) {
   DENTER(TOP_LAYER);

   const auto jr = get_job_report(jobid, jataskid, nullptr);
   if (jr) {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      sge_dstring_sprintf(&dstr, "execd@%s", component_get_qualified_hostname());
      ocs::tom::set<JR_deleted_by>(jr, sge_dstring_get_string(&dstr));
   }

   DRETURN_VOID;
}

#ifdef COMPILE_DC
static void force_job_rlimit(const char* qualified_hostname);
#endif

extern volatile int jobs_to_start;

#ifdef COMPILE_DC
/**
 * @brief Notify PTF about newly started jobs
 *
 * This function checks the job list for jobs that are in the JWAITING4OSJID state
 * and attempts to register them at PTF. If successful, it changes their state to JRUNNING.
 * If still waiting for osjobid, it sets the (global) waiting4osjid flag to true,
 * which will trigger a re-check in the next call to this function.
 */

static void notify_ptf() {
   DENTER(TOP_LAYER);

   int write_job = -1;

#ifdef DEBUG_DC
   ptf_show_registered_jobs();
#endif

   /* ck joblist if there are still jobs waiting for osjobid */
   if (waiting4osjid) {
      waiting4osjid = 0;

      for (const auto jep : ocs::DataStore::master_list_rw<SGE_TYPE_JOB>()) {
         for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
            write_job = 0;
            if (ocs::tom::get<JAT_status>(jatep) == JWAITING4OSJID) {
               switch (register_at_ptf(jep, jatep, nullptr)) {
                  case 0:   
                     /* succeeded */
                     ocs::tom::set<JAT_status>(jatep, JRUNNING);
                  
                     /* spool state transition */ 
                     write_job = 1;
       
                     break;
                  case 1: 
                     /* still waiting for osjobid - nothing changes */
                     waiting4osjid = 1;
                     break;
                  default:
                     /* should add here some cleanup code to remove job */
                     break;
               }
            }

            for (ocs::tom::Element<ocs::tom::PET> petep : ocs::tom::get<JAT_task_list>(jatep)) {
               if (ocs::tom::get<PET_status>(petep) == JWAITING4OSJID) {
                  switch (register_at_ptf(jep, jatep, petep)) {
                     case 0:   
                        /* succeeded */
                        ocs::tom::set<PET_status>(petep, JRUNNING);

                        /* spool state transition */ 
                        write_job = 1;
                        break;

                     case 1: 
                        /* still waiting for osjobid - nothing changes */
                        waiting4osjid = 1;
                        break;

                     default:
                        /* should add here some cleanup code to remove sub-task */
                        break;
                   }
               }
            }
            if (write_job && !mconf_get_simulate_jobs())
               job_write_spool_file(jep, ocs::tom::get<JAT_task_number>(jatep), 
                                    nullptr, SPOOL_WITHIN_EXECD);
         }
      }

      if (waiting4osjid) {
         DPRINTF("still waiting for osjobids\n");
      } else {
         DPRINTF("got all osjobids\n");
      }   
   }

   DRETURN_VOID;
}

/**
 * @brief
 * Check if the job resource limit is exceeded and apply the limit.
 * If the limit is exceeded, the job is signaled with SGE_SIGKILL (hard limits) or SGE_SIGXCPU (soft limits).
 *
 * @param usage The current usage of the resource.
 * @param limit The limit for the resource.
 * @param limit_name The name of the resource limit (e.g., "h_cpu").
 * @param is_hard_limit True if it is a hard limit, false if it is a soft limit.
 * @param queue The queue instance where the job is running.
 * @param jobid The job ID.
 * @param jataskid The job task ID.
 *
 * @return true if the job was signaled, false otherwise.
 */
static bool
force_job_rlimit_apply_limit(const double usage, const double limit, const char *limit_name,
                             const bool is_hard_limit, const ocs::tom::ConstElement<ocs::tom::QU> queue,
                             const uint32_t jobid, const uint32_t jataskid) {
   DENTER(TOP_LAYER);

   bool signaled = false;

   if (limit < usage) {
      if (is_hard_limit) {
         WARNING(MSG_JOB_EXCEEDHLIM_USSFF, jobid, limit_name,
                 ocs::tom::get<QU_full_name>(queue), usage, limit);
         signal_job(jobid, jataskid, SGE_SIGKILL);
         job_report_set_deleted_by_execd(jobid, jataskid);
      } else {
         WARNING(MSG_JOB_EXCEEDSLIM_USSFF, jobid, limit_name,
                 ocs::tom::get<QU_full_name>(queue), usage, limit);
         signal_job(jobid, jataskid, SGE_SIGXCPU);
      }
      signaled = true;
   }

   DRETURN(signaled);
}


/**
 * @brief Check the usage of a task against its limits and apply them
 *
 * The task is checked against the limits sge_shepherd applies to it, see execd_get_task_limit():
 * for a task of a parallel job either the limit of a single slot or the sum of the limits of all
 * slots of the job on the host.
 *
 * @param jep the job
 * @param jatep the ja_task
 * @param petep the pe task, an empty handle for the master task
 * @param qualified_hostname the name of this host
 * @return true if the job was signaled
 */
static bool
force_job_rlimit_task(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                      const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   uint32_t jobid = ocs::tom::get<JB_job_number>(jep);
   uint32_t jataskid = ocs::tom::get<JAT_task_number>(jatep);

   // the usage of this task only - nullptr if the task does not run on this host
   const char *task_id = petep ? ocs::tom::get<PET_id>(petep) : nullptr;
   auto usage_list = ptf_get_job_usage(jobid, jataskid, task_id);
   if (!usage_list) {
      DRETURN(false);
   }

   double cpu_val = usage_list_get_double_usage(*usage_list, USAGE_ATTR_CPU, 0.0);
   double vmem_val = usage_list_get_double_usage(*usage_list, USAGE_ATTR_VMEM, 0.0);
   double rss_val = usage_list_get_double_usage(*usage_list, USAGE_ATTR_RSS, 0.0);
   // A hard memory limit which was exceeded once is violated, also by a peak between two polls.
   // With systemd, maxrss is the high-water mark the kernel keeps, see
   // ptf_get_usage_from_systemd().
   double maxvmem_val = usage_list_get_double_usage(*usage_list, USAGE_ATTR_MAXVMEM, 0.0);
   double maxrss_val = usage_list_get_double_usage(*usage_list, USAGE_ATTR_MAXRSS, 0.0);
   usage_list.reset();

   DPRINTF("JOB " sge_u32 "." sge_u32 " task %s %s %10.5f %s %10.5f %s %10.5f %s %10.5f %s %10.5f"
           "\n", jobid, jataskid, task_id != nullptr ? task_id : "master", USAGE_ATTR_CPU, cpu_val,
           USAGE_ATTR_VMEM, vmem_val, USAGE_ATTR_RSS, rss_val, USAGE_ATTR_MAXVMEM, maxvmem_val,
           USAGE_ATTR_MAXRSS, maxrss_val);

   // need the queue object - it must have been delivered by sge_qmaster
   const auto q = responsible_queue(nullptr, jatep, petep);
   if (!q) {
      // this should never happen, but if it does, we have to skip this task
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, "gdil_ep->JG_queue");
#if defined (ENABLE_DEBUG_CHECKS)
      ocs::TerminationManager::trigger_abort();
#endif
      DRETURN(false);
   }

   // hard limits
   double h_cpu = execd_get_task_limit(QU_h_cpu, "h_cpu", ocs::CEntry::Type::TIME, q,
                                       jep, jatep, petep, qualified_hostname);
   double h_rss = execd_get_task_limit(QU_h_rss, "h_rss", ocs::CEntry::Type::MEM, q,
                                       jep, jatep, petep, qualified_hostname);
   double h_vmem = execd_get_task_limit(QU_h_vmem, "h_vmem", ocs::CEntry::Type::MEM, q,
                                        jep, jatep, petep, qualified_hostname);
   if (force_job_rlimit_apply_limit(cpu_val, h_cpu, "h_cpu", true, q, jobid, jataskid) ||
       force_job_rlimit_apply_limit(maxrss_val, h_rss, "h_rss", true, q, jobid, jataskid) ||
       force_job_rlimit_apply_limit(maxvmem_val, h_vmem, "h_vmem", true, q, jobid, jataskid)) {
      DRETURN(true);
   }

   // soft limits: the current usage - a job is signaled with SIGXCPU while it exceeds the limit,
   // a single peak would make it get the signal with every check
   double s_cpu = execd_get_task_limit(QU_s_cpu, "s_cpu", ocs::CEntry::Type::TIME, q,
                                       jep, jatep, petep, qualified_hostname);
   double s_rss = execd_get_task_limit(QU_s_rss, "s_rss", ocs::CEntry::Type::MEM, q,
                                       jep, jatep, petep, qualified_hostname);
   double s_vmem = execd_get_task_limit(QU_s_vmem, "s_vmem", ocs::CEntry::Type::MEM, q,
                                        jep, jatep, petep, qualified_hostname);
   bool signaled =
      force_job_rlimit_apply_limit(cpu_val, s_cpu, "s_cpu", false, q, jobid, jataskid) ||
      force_job_rlimit_apply_limit(rss_val, s_rss, "s_rss", false, q, jobid, jataskid) ||
      force_job_rlimit_apply_limit(vmem_val, s_vmem, "s_vmem", false, q, jobid, jataskid);

   DRETURN(signaled);
}

/**
 * @brief Force the job resource limits
 *
 * Checks the master task and every pe task of the jobs running on this host against the limits
 * sge_shepherd applies to the task, and signals a job which exceeds a limit.
 *
 * @param qualified_hostname the name of this host
 */
static void force_job_rlimit(const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   for (const auto jep : ocs::DataStore::master_list<SGE_TYPE_JOB>()) {
      for (ocs::tom::ConstElement<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         // a signaled job is not checked further
         if (force_job_rlimit_task(jep, jatep, nullptr, qualified_hostname)) {
            continue;
         }
         for (ocs::tom::ConstElement<ocs::tom::PET> petep : ocs::tom::get<JAT_task_list>(jatep)) {
            if (force_job_rlimit_task(jep, jatep, petep, qualified_hostname)) {
               break;
            }
         }
      } /* foreach jatep */
   } /* foreach jep */

   DRETURN_VOID;
}
#endif

/**
 * @brief The earliest wallclock limit of the queue instances of this host an array task was granted
 *
 * @param qualified_hostname the name of this host, as the granted destination list names it
 * @param gdil_list the granted destination identifier list of the array task
 * @param limit_nm the limit field of the queue instance, QU_s_rt or QU_h_rt
 * @param now the time the limit is measured from
 * @return the point in time the limit is reached, the maximum of uint64_t if there is no limit
 */
static uint64_t
execd_get_wallclock_limit(const char *qualified_hostname, const ocs::tom::ConstList<ocs::tom::JG> gdil_list,
                          const int limit_nm, const uint64_t now) {
   DENTER(TOP_LAYER);

   uint64_t ret = std::numeric_limits<uint64_t>::max();

   for (const auto gdil : gdil_list.find_all<JG_qhostname>(ocs::tom::Host(qualified_hostname))) {
      const char *limit;
      uint64_t clock_val;

      const ocs::tom::ConstElement<ocs::tom::QU> queue(ocs::tom::get<JG_queue>(gdil));
      if (queue) {
         // the limit field is known only at run time
         limit = lGetString(queue, limit_nm);

         if (strcasecmp(limit, INFINITY_STR) == 0) {
            clock_val = std::numeric_limits<uint64_t>::max();
         } else {
            uint32_t timestamp32;
            parse_ulong_val(nullptr, &timestamp32, ocs::CEntry::Type::TIME, limit, nullptr, 0);
            clock_val = sge_gmt32_to_gmt64(timestamp32);
         }   

         ret = std::min(ret, clock_val);
      }
   }

   if (ret != std::numeric_limits<uint64_t>::max()) {
      ret += now;
   }

   DRETURN(ret);
}

/** @brief Fix the wallclock limits of an array task which has just been started
 *
 * The limits are absolute points in time: the earliest of the h_rt and the s_rt of the queue
 * instances of this host which the task was granted, and of the limit the job requested itself.
 * They have to be computed from the time the task was started. Computing them later grants the
 * task whatever time has passed since on top of its limit, and after a restart of the daemon,
 * which recomputes a limit it does not find in the spool file, the time the daemon was down
 * would be granted as well.
 *
 * @param jep the job
 * @param jatep the array task of the job which has been started
 * @param qualified_hostname the name of this host, as the granted destination list names it
 * @param start_time the time the array task was started
 */
static void
execd_set_wallclock_limits(const ocs::tom::Element<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                           const char *qualified_hostname, const uint64_t start_time) {
   DENTER(TOP_LAYER);

   const auto gdil_list = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);
   ocs::tom::set<JB_soft_wallclock_gmt>(jep,
               execd_get_wallclock_limit(qualified_hostname, gdil_list, QU_s_rt, start_time));
   ocs::tom::set<JB_hard_wallclock_gmt>(jep,
               execd_get_wallclock_limit(qualified_hostname, gdil_list, QU_h_rt, start_time));

   uint64_t task_wallclock_limit = ocs::tom::get<JAT_wallclock_limit>(jatep);
   if (task_wallclock_limit != 0) {
      ocs::tom::set<JB_hard_wallclock_gmt>(jep,
                  std::min(ocs::tom::get<JB_hard_wallclock_gmt>(jep),
                           duration_add_offset(start_time, task_wallclock_limit)));
   }

   DPRINTF("job " sge_u32 "." sge_u32 " started at " sge_u64 ": soft wallclock " sge_u64
           ", hard wallclock " sge_u64 "\n", ocs::tom::get<JB_job_number>(jep),
           ocs::tom::get<JAT_task_number>(jatep), start_time,
           ocs::tom::get<JB_soft_wallclock_gmt>(jep), ocs::tom::get<JB_hard_wallclock_gmt>(jep));

   DRETURN_VOID;
}

/**
 * @brief Update the wallclock usage in the job reports of an array task and of its PE tasks
 *
 * The PE tasks which have not started yet or are exiting are left alone.
 *
 * @param now the current time
 * @param job the job
 * @param ja_task the array task
 */
static void
update_wallclock_usage(const uint64_t now, const ocs::tom::ConstElement<ocs::tom::JB> job,
                       const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   uint32_t job_id = ocs::tom::get<JB_job_number>(job);
   uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   uint64_t wallclock = now - ocs::tom::get<JAT_start_time>(ja_task);

   auto jr = get_job_report(job_id, ja_task_id, nullptr);
   if (jr) {
      add_usage(jr, USAGE_ATTR_WALLCLOCK, nullptr, sge_gmt64_to_gmt32_double(wallclock));
   }

   for (ocs::tom::ConstElement<ocs::tom::PET> pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
      // don't update wallclock before job actually started or after it ended */
      uint32_t status = ocs::tom::get<PET_status>(pe_task);
      if (status == JWAITING4OSJID || status == JEXITING) {
         continue;
      }
      wallclock = now - ocs::tom::get<PET_start_time>(pe_task);
      const char *pe_task_id = ocs::tom::get<PET_id>(pe_task);
      jr = get_job_report(job_id, ja_task_id, pe_task_id);
      if (jr) {
         add_usage(jr, USAGE_ATTR_WALLCLOCK, nullptr, sge_gmt64_to_gmt32_double(wallclock));
      }
   }

   DRETURN_VOID;
}

/* TODO: what are the intended intervals? */
/** @brief Seconds before a signal a job has not reacted to is sent again */
#define SIGNAL_RESEND_INTERVAL 1
/** @brief How often to look for jobs left behind by a previous daemon
 *
 * This is a poll interval, not the interval of the expensive scan: clean_up_old_jobs()
 * returns immediately unless the reconciliation is due (see CS-2532). What it does bound
 * is how long a shepherd adopted at startup can stay unnoticed after it ended - it is
 * not a child of this execd, so its exit produces no SIGCHLD and only
 * check_adopted_shepherds(), called from there, sees it. A job whose adopted shepherd
 * was killed therefore stays "running" here for up to this interval, and everything
 * that hangs off the job going away waits with it.
 */
#define OLD_JOB_INTERVAL 60

/**
 * @brief Everything the daemon does between messages
 *
 * Reaps shepherds, resends signals a job has not reacted to, triggers the load
 * report, adjusts priorities and cleans up old jobs. Called by the dispatcher
 * on a cyclic basis, which means on every received message and on timeout.
 *
 * @param is_qmaster_down whether qmaster is currently unreachable
 * @return as every function the dispatcher calls: 0 on success, 1 to quit the
 *         dispatcher loop, 2 to reconnect to qmaster
 */
int do_ck_to_do(bool is_qmaster_down) {
   DENTER(TOP_LAYER);

   uint64_t now = sge_get_gmt64();
   static uint64_t next_pdc = 0;
   static uint64_t next_signal = 0;
   static uint64_t next_old_job = 0;
   static uint64_t next_report = 0;
   static uint64_t last_report_send = 0;
   int return_value = 0;
   const char *qualified_hostname = component_get_qualified_hostname();


   // start jobs if present
   // when job start orders are received, they are not started immediately,
   // but are only added to job list and spooled
   // status JIDLE tells us that the job is ready to be started
   if (jobs_to_start) {
      /* reset jobs_to_start before starting jobs. We may loose
       * a job start if we reset jobs_to_start after sge_start_jobs()
       */
      jobs_to_start = 0;
      sge_start_jobs();
   }

#ifdef COMPILE_DC
   // Find if we need to trigger PDC. It will be triggered
   // * if the PDC_INTERVAL has been changed from PDC_DISABLED to a valid value
   // * if the PDC_INTERVAL has been changed to a new value
   // * or when it was not changed but the time has come to trigger it again
   uint64_t pdc_interval = mconf_get_pdc_interval();
   static uint64_t pdc_interval_old = PDC_DISABLED;
   bool do_pdc = false;
   if (pdc_interval == PDC_DISABLED) {
      next_pdc = std::numeric_limits<uint64_t>::max();
      do_pdc = false;
   } else if (pdc_interval_old != pdc_interval || next_pdc <= now) {
      next_pdc = now + pdc_interval;
      do_pdc = true;
   }
   pdc_interval_old = pdc_interval;

   const bool have_jobs = !ocs::DataStore::master_list<SGE_TYPE_JOB>().empty();

   // Registering a job at the PTF is what moves it from JWAITING4OSJID to
   // JRUNNING, so it must not depend on the usage collector's schedule. A task
   // left in JWAITING4OSJID gets no wallclock update and no h_rt/s_rt
   // enforcement, and the PTF does not know it at reaping time (CS-2495).
   // notify_ptf() returns immediately unless a task is actually waiting.
   if (have_jobs) {
      notify_ptf();
   }

   // PDC trigger can be ignored if there are no jobs to observe
   if (have_jobs && do_pdc) {
      // get online usage of running jobs
      ptf_update_job_usage();

      // check for job limits
      if (check_for_queue_limits()) {
         force_job_rlimit(qualified_hostname);
      }
   }
#endif

   if (sge_sig_handler_dead_children != 0) {
      /* reap max. 10 jobs which generated a SIGCLD
       * @todo The 10 jobs as cap is not a well-founded decision.
       *       We should better limit by time, e.g., stop after 500ms.
       */

      /* SIGCHILD signal is blocked from dispatcher(), so
       * we can be sure that sge_sig_handler_dead_children is untouched here
       */
      sge_sig_handler_dead_children = sge_reap_children_execd(10, is_qmaster_down);
   }

   if (next_signal <= now) {
      next_signal = now + sge_gmt32_to_gmt64(SIGNAL_RESEND_INTERVAL);
      /* resend signals to shepherds */
      for (const auto jep : ocs::DataStore::master_list_rw<SGE_TYPE_JOB>()) {
         for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {

            // don't update wallclock before a job actually started or after it ended */
            uint32_t status = ocs::tom::get<JAT_status>(jatep);
            if (status == JWAITING4OSJID || status == JEXITING) {
               continue;
            }

            // update wallclock usage
            // @todo is this the right place? Currently we come here once a second, which is OK as long as
            //       the time resolution is 1s
            //       in average do_ck_to_do() is executed once a second, probably we are waiting in commlib
            //       with a timeout of 1s, which should be shortened
            update_wallclock_usage(now, jep, jatep);

            // the limits are fixed when the task is started, so normally there is nothing to do
            // here. A task of a simulated job never went through the start, and a task started by
            // an older version of the daemon may have been spooled without them, so fall back to
            // the start time of the task, which is spooled.
            if (ocs::tom::get<JB_hard_wallclock_gmt>(jep) == 0) {
               uint64_t start_time = ocs::tom::get<JAT_start_time>(jatep);
               if (start_time == 0) {
                  start_time = now;
               }
               execd_set_wallclock_limits(jep, jatep, qualified_hostname, start_time);
               if (!mconf_get_simulate_jobs()) {
                  job_write_spool_file(jep, ocs::tom::get<JAT_task_number>(jatep),
                                       nullptr, SPOOL_WITHIN_EXECD);
               }
            }
            
            if (now >= ocs::tom::get<JB_hard_wallclock_gmt>(jep)) {
               if (ocs::tom::get<JAT_pending_signal_delivery_time>(jatep) == 0 ||
                   now > ocs::tom::get<JAT_pending_signal_delivery_time>(jatep)) {
                  WARNING(MSG_EXECD_EXCEEDHWALLCLOCK_UU, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep));
                  if (sge_execd_ja_task_is_tightly_integrated(jatep)) {
                     sge_kill_petasks(jep, jatep);
                  }
                  if (ocs::tom::get<JAT_pid>(jatep) != 0) {
                     sge_kill(ocs::tom::get<JAT_pid>(jatep), SGE_SIGKILL,
                              ocs::tom::get<JB_job_number>(jep),
                              ocs::tom::get<JAT_task_number>(jatep),
                              nullptr);
                  }
                  job_report_set_deleted_by_execd(ocs::tom::get<JB_job_number>(jep),
                                                  ocs::tom::get<JAT_task_number>(jatep));
                  ocs::tom::set<JAT_pending_signal_delivery_time>(jatep, now + sge_gmt32_to_gmt64(90));
               }    
               continue;
            }

            if (now >= ocs::tom::get<JB_soft_wallclock_gmt>(jep)) {
               if (ocs::tom::get<JAT_pending_signal_delivery_time>(jatep) == 0 ||
                   now > ocs::tom::get<JAT_pending_signal_delivery_time>(jatep)) {
                  WARNING(MSG_EXECD_EXCEEDSWALLCLOCK_UU, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep));
                  if (sge_execd_ja_task_is_tightly_integrated(jatep)) {
                     sge_kill_petasks(jep, jatep);
                  }
                  if (ocs::tom::get<JAT_pid>(jatep) != 0) {
                     sge_kill(ocs::tom::get<JAT_pid>(jatep), SGE_SIGUSR1, 
                              ocs::tom::get<JB_job_number>(jep),
                              ocs::tom::get<JAT_task_number>(jatep),
                              nullptr);
                  }
                  ocs::tom::set<JAT_pending_signal_delivery_time>(jatep, now + sge_gmt32_to_gmt64(90));
               }
               continue;
            }
         }
      }
   }

   if (next_old_job <= now) {
      next_old_job = now + sge_gmt32_to_gmt64(OLD_JOB_INTERVAL);
      clean_up_old_jobs(false);
   }

   /* check for end of simulated jobs */
   if (mconf_get_simulate_jobs()) {
      for (const auto jep : ocs::DataStore::master_list_rw<SGE_TYPE_JOB>()) {
         for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
            if (ocs::tom::get<JAT_end_time>(jatep) <= now) {
               simulated_job_exit(jep, jatep);
            }
         }
      }   
   }

   /* do timeout calculation */
   // the next report was scheduled with the load_report_time at that time, a lowered
   // load_report_time has to take effect now: sge_qmaster expects reports in the new interval
   const uint64_t load_report_interval = sge_gmt32_to_gmt64(mconf_get_load_report_time());
   if (next_report > now + load_report_interval) {
      next_report = now + load_report_interval;
   }
   sge_limit_report_times(now + load_report_interval, execd_report_sources);
   if (sge_get_flush_jr_flag() || next_report <= now || sge_get_flush_lr_flag()) {
      if (next_report <= now) {
         next_report = now + load_report_interval;

         /* if pdc_interval is equals load_report time synchronize both calls to
            make the online usage accurate as possible */
         if (load_report_interval == mconf_get_pdc_interval()) {
            next_pdc = next_report;
         }
      }

      // clock turned back?
      if (last_report_send > now) {
         last_report_send = now;
      }

      // send only 1 load report per second
      // @todo one per 100ms significantly reduces latency, see CS-2447
      //       make it configurable
      if (last_report_send + sge_gmt32_to_gmt64(1) <= now) {
         last_report_send = now;

         update_job_usage(qualified_hostname);

         /* send all reports */
         if (sge_send_all_reports(now, 0, execd_report_sources) == 1) {
            return_value = 2;
         }
      }
   }

   /* handle shutdown */
   if (shut_me_down != 0) {
      return_value = 1;
   }
   DRETURN(return_value);
}

/**
 * @brief Is it a tightly integr. parallel job?
 *
 * Checks if a certain job (ja_task) is running in a tightly integrated
 * parallel environment.
 *
 * @param ja_task ja_task (in execd context), or an empty handle
 *
 * @return true, if it is a tightly integrated job, else false
 */
static bool
sge_execd_ja_task_is_tightly_integrated(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (ja_task) {
      const auto pe = ocs::tom::get<JAT_pe_object>(ja_task);
      if (pe && ocs::tom::get<PE_control_slaves>(pe)) {
         ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Kill all pe tasks of a tightly integr. parallel job
 *
 * Kills all tasks of a tightly integrated parallel job/array task.
 *
 * @param job the job, or an empty handle
 * @param ja_task the array task, or an empty handle
 *
 * @return true, if any task was found and could be signalled, else false
 */
static bool
sge_kill_petasks(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (job && ja_task) {
      for (ocs::tom::ConstElement<ocs::tom::PET> pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
         if (sge_kill(ocs::tom::get<PET_pid>(pe_task), SGE_SIGKILL,
                      ocs::tom::get<JB_job_number>(job),
                      ocs::tom::get<JAT_task_number>(ja_task),
                      ocs::tom::get<PET_id>(pe_task)) == 0) {
            ret = true;
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Start the idle array tasks and PE tasks of the job list
 *
 * A job whose state changed is spooled, so the daemon is up to date on a restart.
 *
 * @return always 0
 */
static int sge_start_jobs() {
   DENTER(TOP_LAYER);
   int state_changed;
   int jobs_started = 0;

   if (ocs::DataStore::master_list<SGE_TYPE_JOB>().empty()) {
      DPRINTF("No jobs to start\n");
      DRETURN(0);
   }

   for (const auto jep : ocs::DataStore::master_list_rw<SGE_TYPE_JOB>()) {
      for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         state_changed = exec_job_or_task(jep, jatep, nullptr);

         /* visit all tasks */
         for (ocs::tom::Element<ocs::tom::PET> petep : ocs::tom::get<JAT_task_list>(jatep)) {
            state_changed |= exec_job_or_task(jep, jatep, petep);
         }

         /* now save this job so we are up to date on restart */
         if (state_changed) {
            if (!mconf_get_simulate_jobs()) {
               job_write_spool_file(jep, ocs::tom::get<JAT_task_number>(jatep), nullptr, SPOOL_WITHIN_EXECD);
            }
            jobs_started++;
         }
      }
   }
   DPRINTF("execd_ck_to_do: started " sge_u32 " jobs\n", jobs_started);

   DRETURN(0);
}


/**
 * @brief Start an idle array task or PE task: simulate it, or fork its shepherd
 *
 * A failure to start is reported with a job report, see execd_job_start_failure().
 *
 * @param jep the job
 * @param jatep the array task
 * @param petep the PE task, or an empty handle for the array task itself
 * @return 1 if the task was started, 0 if it was not idle or could not be started
 */
static int exec_job_or_task(const ocs::tom::Element<ocs::tom::JB> jep, const ocs::tom::Element<ocs::tom::JAT> jatep,
                            const ocs::tom::Element<ocs::tom::PET> petep) {
   DENTER(TOP_LAYER);

   char err_str[10000];
   int pid;
   uint64_t now;
   uint32_t job_id, ja_task_id;
   const char *pe_task_id = nullptr;
   const char *qualified_hostname = component_get_qualified_hostname();

   /* retrieve ids - we need them later on */
   job_id = ocs::tom::get<JB_job_number>(jep);
   ja_task_id = ocs::tom::get<JAT_task_number>(jatep);
   if (petep) {
      pe_task_id = ocs::tom::get<PET_id>(petep);
   }

   /* we only handle idle jobs or tasks */
   /* JG: TODO: make a function is_task_idle(jep, jatep, petep) */
   {
      uint32_t status;

      if (petep) {
         status = ocs::tom::get<PET_status>(petep);
      } else {
         status = ocs::tom::get<JAT_status>(jatep);
      }

      if (status != JIDLE) {
         DRETURN(0);
      }
   }

   now = sge_get_gmt64();

   /* JG: TODO: make a function simulate_start_job_or_task() */
   if (mconf_get_simulate_jobs()) {
      uint32_t duration = 60;

      DPRINTF("Simulating job " sge_u32 "." sge_u32 "\n", job_id, ja_task_id);
      ocs::tom::set<JAT_start_time>(jatep, now);
      ocs::tom::set<JAT_status>(jatep, JRUNNING);

      /* set time when job shall be reported as finished */
      const auto job_args = ocs::tom::get<JB_job_args>(jep);
      if (job_args.size() == 1) {
         const char *arg = nullptr;
         char *endptr = nullptr;
         uint32_t duration_in;

         arg = ocs::tom::get<ST_name>(job_args.front());
         if (arg != nullptr) {
            DPRINTF("Trying to use first argument (" SFQ ") as duration for simulated job\n", arg);
            
            duration_in = strtol(arg, &endptr, 0);
            if (arg != endptr) {
               duration = duration_in;
            }
         }   
      }

      ocs::tom::set<JAT_end_time>(jatep, now + sge_gmt32_to_gmt64(duration));
      DRETURN(1);
   }

   if (petep) {
      ocs::tom::set<PET_start_time>(petep, now);
#ifdef COMPILE_DC
      ocs::tom::set<PET_status>(petep, JWAITING4OSJID);
      waiting4osjid = 1;
#else
      ocs::tom::set<PET_status>(petep, JRUNNING);
#endif
   } else {
      ocs::tom::set<JAT_start_time>(jatep, now);
#ifdef COMPILE_DC
      ocs::tom::set<JAT_status>(jatep, JWAITING4OSJID);
      waiting4osjid = 1;
#else
      ocs::tom::set<JAT_status>(jatep, JRUNNING);
#endif
   }

   if (getenv("FAILURE_BEFORE_EXEC")) {
      pid = -1; 
      strcpy(err_str, "FAILURE_BEFORE_EXEC");
   } else {
      pid = sge_exec_job(jep, jatep, petep, err_str, 256);
   }   

   if (pid < 0) {
      switch (pid) {
         case -1:
            execd_job_start_failure(jep, jatep, petep, err_str, GFSTATE_NO_HALT);
            break;
         case -2:
            execd_job_start_failure(jep, jatep, petep, err_str, GFSTATE_QUEUE);
            break;
         case -3:
            execd_job_start_failure(jep, jatep, petep, err_str, GFSTATE_JOB);
            break;
         default:
            execd_job_start_failure(jep, jatep, petep, err_str, GFSTATE_HOST);
            break;
      }
      DRETURN(0);
   }
   DPRINTF("TIME IN EXECD FOR STARTING THE JOB: " sge_u64 "\n", sge_get_gmt64() - now);
   
   if (petep) {
      ocs::tom::set<PET_pid>(petep, pid);
   } else {
      ocs::tom::set<JAT_pid>(jatep, pid);

      // the wallclock limits have to be measured from the start of the task. sge_start_jobs()
      // spools the job as soon as this function returns, so a restart of the daemon finds them.
      execd_set_wallclock_limits(jep, jatep, qualified_hostname, now);
   }

   DPRINTF("***EXECING " sge_u32 "." sge_u32 " on %s (tid = %s) (pid = %d)\n",
            job_id, ja_task_id, qualified_hostname, pe_task_id != nullptr ? pe_task_id : "null", pid);

   /* when a ja_task or pe_task has been started, flush the job report */
   {
      auto jr = get_job_report(job_id, ja_task_id, pe_task_id);
      if (!jr) {
         jr = add_job_report(job_id, ja_task_id, pe_task_id, jep);
         flush_job_report(jr);
      }
   }

   DRETURN(1);
}

#ifdef COMPILE_DC
/**
 * @brief Hand a newly started job to the priority translation facility
 * @param job the job
 * @param ja_task the array task
 * @param pe_task the PE task, or an empty handle
 * @return 0 on success, 1 while the job is still waiting for its additional group id or could not be
 *         registered
 */
int register_at_ptf(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                    const ocs::tom::ConstElement<ocs::tom::PET> pe_task) {
   DENTER(TOP_LAYER);

   uint32_t job_id;
   uint32_t ja_task_id;
   const char *pe_task_id = nullptr;

   SGE_STRUCT_STAT sb;

   DSTRING_STATIC(id_dstring, MAX_STRING_SIZE);

   job_id = ocs::tom::get<JB_job_number>(job);
   ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   if (pe_task) {
      pe_task_id = ocs::tom::get<PET_id>(pe_task);
   }

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   // Check if the addgrpid file exists,
   // this means that the shepherd has started the job.
   DSTRING_STATIC(addgrpid_path, SGE_PATH_MAX);
   sge_get_active_job_file_path(&addgrpid_path, job_id, ja_task_id, pe_task_id, ADDGRPID);
   DPRINTF("Registering job %s with PTF\n", job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));

   if (SGE_STAT(sge_dstring_get_string(&addgrpid_path), &sb) && errno == ENOENT) {
      DPRINTF("still waiting for addgrpid of job %s\n", job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      DRETURN(1);
   }  

   gid_t addgrpid;
#if 1
   // We store the addgrpid in the ja_task/pe_task when writing the config file,
   // and the job is spooled immediately after forking the shepherd,
   // so we can use it here, even when sge_execd gets restarted.
   const char *addgrpid_str;
   if (pe_task) {
      addgrpid_str = ocs::tom::get<PET_osjobid>(pe_task);
   } else {
      addgrpid_str = ocs::tom::get<JAT_osjobid>(ja_task);
   }
   addgrpid = static_cast<gid_t>(std::stoul(addgrpid_str));
#else
   // We need to read the addgrpid file.
   FILE *fp;
   if (!(fp = fopen(sge_dstring_get_string(&addgrpid_path), "r"))) {
      ERROR(MSG_EXECD_NOADDGIDOPEN_SSS, sge_dstring_get_string(&addgrpid_path), job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), strerror(errno));
      DRETURN(-1);
   }

   /* read addgrpid */
   int success = fscanf(fp, gid_t_fmt, &addgrpid) == 1;
   FCLOSE(fp);
   if (!success) {
      // can happen that shepherd has opened the file but not yet written
      DRETURN(1);
   }
#endif

   {
      int ptf_error;

      // when running jobs in systemd scopes store the scope id
      // we need it to retrieve usage information
      // it is already stored and spooled in the ja_task/pe_task
      // similar to the osjobid/addgrpid
      const char *scope_str = nullptr;
#if defined(OCS_WITH_SYSTEMD)
      if (pe_task) {
         scope_str = ocs::tom::get<PET_systemd_scope>(pe_task);
      } else {
         scope_str = ocs::tom::get<JAT_systemd_scope>(ja_task);
      }
#endif

      // We need the usage collection mode to register the job,
      // and we stored it in the ja_task/pe_task when starting the job.
      usage_collection_t usage_collection;
      if (pe_task) {
         usage_collection = static_cast<usage_collection_t>(ocs::tom::get<PET_usage_collection>(pe_task));
      } else {
         usage_collection = static_cast<usage_collection_t>(ocs::tom::get<JAT_usage_collection>(ja_task));
      }

      // If we do not want to get usage via Systemd, then pass nullptr as scope_str
      if (scope_str != nullptr) {
         if (usage_collection != USAGE_COLLECTION_DEFAULT && usage_collection != USAGE_COLLECTION_HYBRID) {
            scope_str = nullptr;
         }
      }

      // If we do not want to get usage via PTF, then pass 0 as addgrpid
      if (addgrpid != 0) {
         // if we have a systemd scope and it is not hybrid, we use only systemd
         if (scope_str != nullptr &&
             usage_collection != USAGE_COLLECTION_HYBRID && usage_collection != USAGE_COLLECTION_PDC) {
            addgrpid = 0;
         } else if (usage_collection == USAGE_COLLECTION_NONE) {
            addgrpid = 0;
         }
      }

      DPRINTF("Register job with AddGrpId " gid_t_fmt " and systemd scope " SFN " at PTF\n", addgrpid, scope_str != nullptr ? scope_str : "null");
      if ((ptf_error = ptf_job_started(addgrpid, pe_task_id, job, ja_task_id, scope_str, usage_collection))) {
         ERROR(MSG_JOB_NOREGISTERPTF_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), ptf_errstr(ptf_error));
         DRETURN(1);
      }
   }

   /* store addgrpid in job report to be sent to qmaster later on */
{
   // @todo CS-1409 - JR_osjobid is probably not needed at all
   const auto jr = get_job_report(job_id, ja_task_id, pe_task_id);
   if (jr) {
      ocs::tom::set<JR_osjobid>(jr, addgrpid_str);
   }
   DPRINTF("job %s: addgrpid = %s\n", job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), addgrpid_str);
}
#else
   /* read osjobid if possible */
   DSTRING_STATIC(osjobid_path, SGE_PATH_MAX);
   sge_get_active_job_file_path(&osjobid_path, job_id, ja_task_id, pe_task_id, OSJOBID);

   DPRINTF(("Registering job %s with PTF\n", job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring)));

   if (SGE_STAT(sge_dstring_get_string(&osjobid_path), &sb) && errno == ENOENT) {
      DPRINTF(("still waiting for osjobid of job %s\n",
            job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring)));
      sge_dstring_free(&osjobid_path);
      DRETURN(1);
   }

   osjobid_t osjobid;
#if 1
   const char *osjobid_str;
   if (pe_task != nullptr) {
      osjobid_str = ocs::tom::get<PET_osjobid>(pe_task);
   } else {
      osjobid_str = ocs::tom::get<JAT_osjobid>(ja_task);
   }
   osjobid = static_cast<gid_t>(std::stoul(osjobid_str));
#else
   if (!(fp=fopen(sge_dstring_get_string(&osjobid_path), "r"))) {
      ERROR(MSG_EXECD_NOOSJOBIDOPEN_SSS, sge_dstring_get_string(&osjobid_path), job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), strerror(errno));
      sge_dstring_free(&osjobid_path);
      DRETURN(-1);
   }

   sge_dstring_free(&osjobid_path);

   success = (fscanf(fp, OSJOBID_FMT, &osjobid)==1);
   FCLOSE(fp);
   if (!success) {
      /* can happen that shepherd has opend the file but not written */
      DRETURN(1);
   }
#endif
   {
      int ptf_error;
      if ((ptf_error = ptf_job_started(osjobid, pe_task_id, job, ja_task_id))) {
         ERROR(MSG_JOB_NOREGISTERPTF_SS,  job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), ptf_errstr(ptf_error));
         DRETURN(-1);
      }
   }

   /* store osjobid in job report to be sent to qmaster later on */
   {
      const auto jr = get_job_report(job_id, ja_task_id, pe_task_id);
      if (jr)
         ocs::tom::set<JR_osjobid>(jr, osjobid_str);
      DPRINTF(("job %s: osjobid = %s\n",
               job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
               osjobid_str));
   }
#endif

   DRETURN(0);
#if 0
   FCLOSE_ERROR:
      DRETURN(1);
#endif
}

#endif
