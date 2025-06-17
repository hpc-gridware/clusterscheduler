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

#include "spool/classic/read_write_job.h"

#include "reaper_execd.h"
#include "job_report_execd.h"
#include "load_avg.h"
#include "execd_ck_to_do.h"
#include "execd_signal_queue.h"
#include "exec_job.h"
#include "execution_states.h"
#include "msg_execd.h"
#include "sig_handlers.h"
#include "sge.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#  ifdef DEBUG_DC
#     include "sgedefs.h"
#     include "exec_ifm.h"
#  endif
#endif

extern volatile int waiting4osjid;

static bool 
sge_execd_ja_task_is_tightly_integrated(const lListElem *ja_task);
static bool
sge_kill_petasks(const lListElem *job, const lListElem *ja_task);

static int sge_start_jobs();
static int exec_job_or_task(lListElem *jep, lListElem *jatep, lListElem *petep);

#ifdef COMPILE_DC
static void force_job_rlimit(const char* qualified_hostname);
#endif

extern volatile int jobs_to_start;

extern lList *jr_list;

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

   lListElem *jep;
   int write_job = -1;

#ifdef DEBUG_DC
   ptf_show_registered_jobs();
#endif

   /* ck joblist if there are still jobs waiting for osjobid */
   if (waiting4osjid) {
      waiting4osjid = 0;

      for_each_rw(jep, *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB)) {
         lListElem* jatep;
         
         for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
            write_job = 0;
            if (lGetUlong(jatep, JAT_status) == JWAITING4OSJID) {
               switch (register_at_ptf(jep, jatep, nullptr)) {
                  case 0:   
                     /* succeeded */
                     lSetUlong(jatep, JAT_status, JRUNNING);
                  
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

            lListElem *petep;
            for_each_rw (petep, lGetList(jatep, JAT_task_list)) {
               if (lGetUlong(petep, PET_status) == JWAITING4OSJID) {
                  switch (register_at_ptf(jep, jatep, petep)) {
                     case 0:   
                        /* succeeded */
                        lSetUlong(petep, PET_status, JRUNNING);

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
               job_write_spool_file(jep, lGetUlong(jatep, JAT_task_number), 
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

/* force job resource limits */
static void force_job_rlimit(const char* qualified_hostname)
{
   const lListElem *jep;

   DENTER(TOP_LAYER);

   for_each_ep(jep, *ocs::DataStore::get_master_list(SGE_TYPE_JOB)) {
      const lListElem *jatep;

      for_each_ep(jatep, lGetList(jep, JB_ja_tasks)) {
         const lListElem *q=nullptr, *cpu_ep, *vmem_ep, *gdil_ep;
         double cpu_val, vmem_val;
         double s_cpu, h_cpu;
         double s_vmem, h_vmem;
         int cpu_exceeded;
         lList *usage_list;
         u_long32 jobid, jataskid;

         jobid = lGetUlong(jep, JB_job_number);
         jataskid = lGetUlong(jatep, JAT_task_number);

         cpu_val = vmem_val = s_cpu = h_cpu = s_vmem = h_vmem = 0;

         /* retrieve cpu and vmem usage */
         usage_list = ptf_get_job_usage(jobid, jataskid, "*");
         if (usage_list == nullptr) {
            continue;
         }

         if ((cpu_ep = lGetElemStr(usage_list, UA_name, USAGE_ATTR_CPU))) {
            cpu_val = lGetDouble(cpu_ep, UA_value);
         }

         if ((vmem_ep = lGetElemStr(usage_list, UA_name, USAGE_ATTR_VMEM))) {
            vmem_val = lGetDouble(vmem_ep, UA_value);
         }
            
         DPRINTF("JOB " sge_u32 " %s %10.5f %s %10.5f\n", jobid,
            cpu_ep != nullptr ? USAGE_ATTR_CPU : "(" USAGE_ATTR_CPU ")", cpu_val,
            vmem_ep != nullptr ? USAGE_ATTR_VMEM : "(" USAGE_ATTR_VMEM ")", vmem_val);

         /* free no longer needed usage_list */
         lFreeList(&usage_list);
         cpu_ep = vmem_ep = nullptr;

         bool first_gdil_ep = true;
         for_each_ep(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
            double lim;
            char err_str[128];
            size_t err_size = sizeof(err_str) - 1;

            if (sge_hostcmp(qualified_hostname, lGetHost(gdil_ep, JG_qhostname))
                || !(q = lGetObject(gdil_ep, JG_queue))) {
               continue;
            }

            int nslots = lGetUlong(gdil_ep, JG_slots);

            // in case of a parallel job
            // if job_is_first_task is false, we have no slot for the master task,
            // but it still can consume vmem and cpu
            // Problem: @todo CS-547 we cannot differentiate between
            //          - only the master task is running on this host (nslots = 1)
            //          - the master task + one slave task is running on this host (nslots = 1)
            //          only in the second case we have to increase nslots,
            //          but better always increase it and not kill the job erroneously
            if (first_gdil_ep) {
               first_gdil_ep = false;
               const lListElem *pe = lGetObject(jatep, JAT_pe_object);
               if (pe != nullptr) {
                  if (!lGetBool(pe, PE_job_is_first_task)) {
                     nslots++;
                  }
               }
            }

            parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_s_cpu), err_str, err_size);
            if (lim == DBL_MAX) {
               s_cpu = DBL_MAX;
            } else {
               s_cpu += lim * nslots; 
            }

            parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_h_cpu), err_str, err_size);
            if (lim == DBL_MAX) {
               h_cpu = DBL_MAX;
            } else {
               h_cpu += lim * nslots; 
            }

            parse_ulong_val(&lim, nullptr, TYPE_MEM, lGetString(q, QU_s_vmem), err_str, err_size);
            if (lim == DBL_MAX) {
               s_vmem = DBL_MAX;
            } else {
               s_vmem += lim * nslots; 
            }

            parse_ulong_val(&lim, nullptr, TYPE_MEM, lGetString(q, QU_h_vmem), err_str, err_size);
            if (lim == DBL_MAX) {
               h_vmem = DBL_MAX;
            } else {
               h_vmem += lim * nslots; 
            }
         } /* foreach gdil_ep */

         if (h_cpu < cpu_val || h_vmem < vmem_val) {
            cpu_exceeded = (h_cpu < cpu_val);
            WARNING(MSG_JOB_EXCEEDHLIM_USSFF, jobid, cpu_exceeded ? "h_cpu" : "h_vmem",
                    q?lGetString(q, QU_full_name) : "-",
                    cpu_exceeded ? cpu_val : vmem_val, cpu_exceeded ? h_cpu : h_vmem);
            signal_job(jobid, jataskid, SGE_SIGKILL);
            continue;
         }

         if (s_cpu < cpu_val || s_vmem < vmem_val) {
            cpu_exceeded = (s_cpu < cpu_val);
            WARNING(MSG_JOB_EXCEEDSLIM_USSFF, jobid, cpu_exceeded ? "s_cpu" : "s_vmem",
            q?lGetString(q, QU_full_name) : "-",
            cpu_exceeded ? cpu_val : vmem_val, cpu_exceeded ? s_cpu : s_vmem);
            signal_job(jobid, jataskid, SGE_SIGXCPU);
            continue;
         }
      } /* foreach jatep */
   } /* foreach jep */

   DRETURN_VOID;
}
#endif

static u_long64
execd_get_wallclock_limit(const char *qualified_hostname, const lList *gdil_list, int limit_nm, u_long64 now)
{
   u_long64 ret = U_LONG64_MAX;
   const lListElem *gdil;
   const void *iterator;

   gdil = lGetElemHostFirst(gdil_list, JG_qhostname, qualified_hostname, &iterator);
   while (gdil != nullptr) {
      const lListElem *queue;
      const char *limit;
      u_long64 clock_val;

      queue = lGetObject(gdil, JG_queue);
      if (queue != nullptr) {
         limit = lGetString(queue, limit_nm);

         if (strcasecmp(limit, "infinity") == 0) {
            clock_val = U_LONG64_MAX;
         } else {
            u_long32 timestamp32;
            parse_ulong_val(nullptr, &timestamp32, TYPE_TIM, limit, nullptr, 0);
            clock_val = sge_gmt32_to_gmt64(timestamp32);
         }   

         ret = MIN(ret, clock_val);
      }

      gdil = lGetElemHostNext(gdil_list, JG_qhostname, qualified_hostname, &iterator);
   }

   if (ret != U_LONG64_MAX) {
      ret += now;
   }

   return ret;
}

static void
update_wallclock_usage(u_long64 now, const lListElem *job, const lListElem *ja_task)
{
   u_long32 job_id = lGetUlong(job, JB_job_number);
   u_long32 ja_task_id = lGetUlong(ja_task, JAT_task_number);
   u_long64 wallclock = now - lGetUlong64(ja_task, JAT_start_time);

   lListElem *jr = get_job_report(job_id, ja_task_id, nullptr);
   if (jr != nullptr) {
      add_usage(jr, USAGE_ATTR_WALLCLOCK, nullptr, sge_gmt64_to_gmt32_double(wallclock));
   }

   const lListElem *pe_task;
   for_each_ep (pe_task, lGetList(ja_task, JAT_task_list)) {
      // don't update wallclock before job actually started or after it ended */
      u_long32 status = lGetUlong(pe_task, PET_status);
      if (status == JWAITING4OSJID || status == JEXITING) {
         continue;
      }
      wallclock = now - lGetUlong64(pe_task, PET_start_time);
      const char *pe_task_id = lGetString(pe_task, PET_id);
      jr = get_job_report(job_id, ja_task_id, pe_task_id);
      if (jr != nullptr) {
         add_usage(jr, USAGE_ATTR_WALLCLOCK, nullptr, sge_gmt64_to_gmt32_double(wallclock));
      }
   }
}

/******************************************************
 EXECD function

 called by dispatcher on a cyclic basis 
 cyclic means on every received message and on timeout


   as all dispatcher called function returns
   -  0 on success (currently the only value is 0)
   -  1 if we want to quit the dispacher loop
   -  2 if we want to reconnect to qmaster
   
 do cyclic jobs
 ******************************************************/
/* TODO: what are the intended intervals? */
#define SIGNAL_RESEND_INTERVAL 1
#define OLD_JOB_INTERVAL 60

int do_ck_to_do(bool is_qmaster_down) {
   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();
   static u_long64 next_pdc = 0;
   static u_long64 next_signal = 0;
   static u_long64 next_old_job = 0;
   static u_long64 next_report = 0;
   static u_long64 last_report_send = 0;
   lListElem *jep, *jatep;
   int return_value = 0;
   const char *qualified_hostname = component_get_qualified_hostname();

#ifdef KERBEROS
   krb_renew_tgts(Master_Job_List);
#endif

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
   u_long64 pdc_interval = mconf_get_pdc_interval();
   static u_long64 pdc_interval_old = PDC_DISABLED;
   bool do_pdc = false;
   if (pdc_interval == PDC_DISABLED) {
      next_pdc = U_LONG64_MAX;
      do_pdc = false;
   } else if (pdc_interval_old != pdc_interval || next_pdc <= now) {
      next_pdc = now + pdc_interval;
      do_pdc = true;
   }
   pdc_interval_old = pdc_interval;

   // PDC trigger can be ignored if there are no jobs to observe
   if (lGetNumberOfElem(*ocs::DataStore::get_master_list(SGE_TYPE_JOB)) > 0 && do_pdc) {
      // register newly submitted jobs at PTF
      notify_ptf();

      // get online usage of running jobs
      ptf_update_job_usage();

      // check for job limits
      if (check_for_queue_limits()) {
         force_job_rlimit(qualified_hostname);
      }
   }
#endif

   if (sge_sig_handler_dead_children != 0) {
      /* reap max. 10 jobs which generated a SIGCLD */

      /* SIGCHILD signal is blocked from dispatcher(), so
       * we can be sure that sge_sig_handler_dead_children is untouched here
       */
      sge_sig_handler_dead_children = sge_reap_children_execd(10, is_qmaster_down);
   }

   if (next_signal <= now) {
      next_signal = now + sge_gmt32_to_gmt64(SIGNAL_RESEND_INTERVAL);
      /* resend signals to shepherds */
      for_each_rw (jep, *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB)) {
         for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {

            // don't update wallclock before a job actually started or after it ended */
            u_long32 status = lGetUlong(jatep, JAT_status);
            if (status == JWAITING4OSJID || status == JEXITING) {
               continue;
            }

            // update wallclock usage
            // @todo is this the right place? Currently we come here once a second, which is OK as long as
            //       the time resolution is 1s
            //       in average do_ck_to_do() is executed once a second, probably we are waiting in commlib
            //       with a timeout of 1s, which should be shortened
            update_wallclock_usage(now, jep, jatep);

            // limit has not yet been set
            // @todo shouldn't we better do this already when receiving / starting a new job?
            if (lGetUlong64(jep, JB_hard_wallclock_gmt) == 0) {
               u_long64 task_wallclock_limit = lGetUlong64(jatep, JAT_wallclock_limit);
               const lList *gdil_list = lGetList(jatep, JAT_granted_destin_identifier_list);

               lSetUlong64(jep, JB_soft_wallclock_gmt,
                           execd_get_wallclock_limit(qualified_hostname, gdil_list, QU_s_rt, now));
               lSetUlong64(jep, JB_hard_wallclock_gmt,
                           execd_get_wallclock_limit(qualified_hostname, gdil_list, QU_h_rt, now));

               if (task_wallclock_limit != 0) {
                  lSetUlong64(jep, JB_hard_wallclock_gmt,
                              MIN(lGetUlong64(jep, JB_hard_wallclock_gmt), duration_add_offset(now, task_wallclock_limit)));
               }
               if (!mconf_get_simulate_jobs()) {
                  job_write_spool_file(jep, lGetUlong(jatep, JAT_task_number), 
                                       nullptr, SPOOL_WITHIN_EXECD);
               }
            }
            
            if (now >= lGetUlong64(jep, JB_hard_wallclock_gmt)) {
               if (lGetUlong64(jatep, JAT_pending_signal_delivery_time) == 0 ||
                   now > lGetUlong64(jatep, JAT_pending_signal_delivery_time)) {
                  WARNING(MSG_EXECD_EXCEEDHWALLCLOCK_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number));
                  if (sge_execd_ja_task_is_tightly_integrated(jatep)) {
                     sge_kill_petasks(jep, jatep);
                  }
                  if (lGetUlong(jatep, JAT_pid) != 0) {
                     sge_kill(lGetUlong(jatep, JAT_pid), SGE_SIGKILL, 
                              lGetUlong(jep, JB_job_number),
                              lGetUlong(jatep, JAT_task_number),
                              nullptr);
                  }
                  lSetUlong64(jatep, JAT_pending_signal_delivery_time, now + sge_gmt32_to_gmt64(90));
               }    
               continue;
            }

            if (now >= lGetUlong64(jep, JB_soft_wallclock_gmt)) {
               if (lGetUlong64(jatep, JAT_pending_signal_delivery_time) == 0 ||
                   now > lGetUlong64(jatep, JAT_pending_signal_delivery_time)) {
                  WARNING(MSG_EXECD_EXCEEDSWALLCLOCK_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number));
                  if (sge_execd_ja_task_is_tightly_integrated(jatep)) {
                     sge_kill_petasks(jep, jatep);
                  }
                  if (lGetUlong(jatep, JAT_pid) != 0) {
                     sge_kill(lGetUlong(jatep, JAT_pid), SGE_SIGUSR1, 
                              lGetUlong(jep, JB_job_number),
                              lGetUlong(jatep, JAT_task_number),
                              nullptr);
                  }
                  lSetUlong64(jatep, JAT_pending_signal_delivery_time, now + sge_gmt32_to_gmt64(90));
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
      for_each_rw(jep, *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB)) {
         for_each_rw(jatep, lGetList(jep, JB_ja_tasks)) {
            if (lGetUlong64(jatep, JAT_end_time) <= now) {
               simulated_job_exit(jep, jatep);
            }
         }
      }   
   }

   /* do timeout calculation */
   if (sge_get_flush_jr_flag() || next_report <= now || sge_get_flush_lr_flag()) {
      if (next_report <= now) {
         next_report = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());

         /* if pdc_interval is equals load_report time synchronize both calls to
            make the online usage accurate as possible */
         if (sge_gmt32_to_gmt64(mconf_get_load_report_time()) == mconf_get_pdc_interval()) {
            next_pdc = next_report;
         }
      }

      // clock turned back?
      if (last_report_send > now) {
         last_report_send = now;
      }

      // send only 1 load report per second
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

/****** execd_ck_to_do/sge_execd_ja_task_is_tightly_integrated() ***************
*  NAME
*     sge_execd_ja_task_is_tightly_integrated() -- is it a tightly integr. parallel job?
*
*  SYNOPSIS
*     static bool 
*     sge_execd_ja_task_is_tightly_integrated(const lListElem *ja_task) 
*
*  FUNCTION
*     Checks if a certain job (ja_task) is running in a tightly integrated
*     parallel environment.
*
*  INPUTS
*     const lListElem *ja_task - ja_task (in execd context)
*
*  RESULT
*     static bool - true, if it is a tightly integrated job, else false
*
*******************************************************************************/
static bool 
sge_execd_ja_task_is_tightly_integrated(const lListElem *ja_task)
{
   bool ret = false;

   if (ja_task != nullptr) {
      const lListElem *pe = lGetObject(ja_task, JAT_pe_object);
      if (pe != nullptr && lGetBool(pe, PE_control_slaves)) {
         ret = true;
      }
   }

   return ret;
}

/****** execd_ck_to_do/sge_kill_petasks() **************************************
*  NAME
*     sge_kill_petasks() -- kill all pe tasks of a tightly integr. parallel job
*
*  SYNOPSIS
*     static bool 
*     sge_kill_petasks(const lListElem *job, const lListElem *ja_task) 
*
*  FUNCTION
*     Kills all tasks of a tightly integrated parallel job/array task.
*
*  INPUTS
*     const lListElem *job     - the job
*     const lListElem *ja_task - the array task
*
*  RESULT
*     static bool - true, if any task was found and could be signalled, 
*                   else false
*
*******************************************************************************/
static bool
sge_kill_petasks(const lListElem *job, const lListElem *ja_task)
{
   bool ret = false;

   if (job != nullptr && ja_task != nullptr) {
      const lListElem *pe_task;

      for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
         if (sge_kill(lGetUlong(pe_task, PET_pid), SGE_SIGKILL,
                      lGetUlong(job, JB_job_number),
                      lGetUlong(ja_task, JAT_task_number),
                      lGetString(pe_task, PET_id)) == 0) {
            ret = true;
         }
      }
   }

   return ret;
}


/*****************************************************************************/
static int sge_start_jobs()
{
   lListElem *jep, *jatep, *petep;
   int state_changed;
   int jobs_started = 0;

   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(*ocs::DataStore::get_master_list(SGE_TYPE_JOB)) == 0) {
      DPRINTF("No jobs to start\n");
      DRETURN(0);
   }

   for_each_rw(jep, *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB)) {
      for_each_rw(jatep, lGetList(jep, JB_ja_tasks)) {
         state_changed = exec_job_or_task(jep, jatep, nullptr);

         /* visit all tasks */
         for_each_rw(petep, lGetList(jatep, JAT_task_list)) {
            state_changed |= exec_job_or_task(jep, jatep, petep);
         }

         /* now save this job so we are up to date on restart */
         if (state_changed) {
            if (!mconf_get_simulate_jobs()) {
               job_write_spool_file(jep, lGetUlong(jatep, JAT_task_number), nullptr, SPOOL_WITHIN_EXECD);
            }
            jobs_started++;
         }
      }
   }
   DPRINTF("execd_ck_to_do: started " sge_u32 " jobs\n", jobs_started);

   DRETURN(0);
}


static int exec_job_or_task(lListElem *jep, lListElem *jatep, lListElem *petep)
{
   char err_str[256];
   int pid;
   u_long64 now;
   u_long32 job_id, ja_task_id;
   const char *pe_task_id = nullptr;
   const char *qualified_hostname = component_get_qualified_hostname();

   DENTER(TOP_LAYER);

   /* retrieve ids - we need them later on */
   job_id = lGetUlong(jep, JB_job_number);
   ja_task_id = lGetUlong(jatep, JAT_task_number);
   if (petep != nullptr) {
      pe_task_id = lGetString(petep, PET_id);
   }

   /* we only handle idle jobs or tasks */
   /* JG: TODO: make a function is_task_idle(jep, jatep, petep) */
   {
      u_long32 status;

      if (petep != nullptr) {
         status = lGetUlong(petep, PET_status);
      } else {
         status = lGetUlong(jatep, JAT_status);
      }

      if (status != JIDLE) {
         DRETURN(0);
      }
   }

   now = sge_get_gmt64();

   /* JG: TODO: make a function simulate_start_job_or_task() */
   if (mconf_get_simulate_jobs()) {
      const lList *job_args;
      u_long32 duration = 60;

      DPRINTF("Simulating job " sge_u32 "." sge_u32 "\n", job_id, ja_task_id);
      lSetUlong64(jatep, JAT_start_time, now);
      lSetUlong(jatep, JAT_status, JRUNNING);

      /* set time when job shall be reported as finished */
      job_args = lGetList(jep, JB_job_args);
      if (lGetNumberOfElem(job_args) == 1) {
         const char *arg = nullptr;
         char *endptr = nullptr;
         u_long32 duration_in;

         arg = lGetString(lFirst(job_args), ST_name);
         if (arg != nullptr) {
            DPRINTF("Trying to use first argument (" SFQ ") as duration for simulated job\n", arg);
            
            duration_in = strtol(arg, &endptr, 0);
            if (arg != endptr) {
               duration = duration_in;
            }
         }   
      }

      lSetUlong64(jatep, JAT_end_time, now + sge_gmt32_to_gmt64(duration));
      DRETURN(1);
   }

   if (petep != nullptr) {
      lSetUlong64(petep, PET_start_time, now);
#ifdef COMPILE_DC
      lSetUlong(petep, PET_status, JWAITING4OSJID);
      waiting4osjid = 1;
#else
      lSetUlong(petep, PET_status, JRUNNING);
#endif
   } else {
      lSetUlong64(jatep, JAT_start_time, now);
#ifdef COMPILE_DC
      lSetUlong(jatep, JAT_status, JWAITING4OSJID);
      waiting4osjid = 1;
#else
      lSetUlong(jatep, JAT_status, JRUNNING);
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
   
   if (petep != nullptr) {
      lSetUlong(petep, PET_pid, pid);
   } else {
      lSetUlong(jatep, JAT_pid, pid);
   }

   DPRINTF("***EXECING " sge_u32 "." sge_u32 " on %s (tid = %s) (pid = %d)\n",
            job_id, ja_task_id, qualified_hostname, pe_task_id != nullptr ? pe_task_id : "null", pid);

   /* when a ja_task or pe_task has been started, flush the job report */
   {
      lListElem *jr = get_job_report(job_id, ja_task_id, pe_task_id);
      if (jr == nullptr) {
         jr = add_job_report(job_id, ja_task_id, pe_task_id, jep);
         flush_job_report(jr);
      }
   }

   DRETURN(1);
}

#ifdef COMPILE_DC
int register_at_ptf(const lListElem *job, const lListElem *ja_task, const lListElem *pe_task) {
   DENTER(TOP_LAYER);

   u_long32 job_id;
   u_long32 ja_task_id;   
   const char *pe_task_id = nullptr;

   SGE_STRUCT_STAT sb;

   DSTRING_STATIC(id_dstring, MAX_STRING_SIZE);

   job_id = lGetUlong(job, JB_job_number);
   ja_task_id = lGetUlong(ja_task, JAT_task_number);
   if (pe_task != nullptr) {
      pe_task_id = lGetString(pe_task, PET_id);
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
   if (pe_task != nullptr) {
      addgrpid_str = lGetString(pe_task, PET_osjobid);
   } else {
      addgrpid_str = lGetString(ja_task, JAT_osjobid);
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
      std::string scope;
      const char *scope_str = nullptr;
#if defined(OCS_WITH_SYSTEMD)
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      std::string slice;
      if (ocs::Job::job_get_systemd_slice_and_scope(job, ja_task, pe_task, slice, scope, &error_dstr)) {
         scope_str = scope.c_str();
      } else {
         ERROR(MSG_JOB_NOREGISTERPTF_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
               sge_dstring_get_string(&error_dstr));
         DRETURN(1);
      }
#endif
      DPRINTF("Register job with AddGrpId at " gid_t_fmt " PTF\n", addgrpid);
      if ((ptf_error = ptf_job_started(addgrpid, pe_task_id, job, ja_task_id, scope_str))) {
         ERROR(MSG_JOB_NOREGISTERPTF_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), ptf_errstr(ptf_error));
         DRETURN(1);
      }
   }

   /* store addgrpid in job report to be sent to qmaster later on */
{
#if 0
   char addgrpid_str[64];

   snprintf(addgrpid_str, sizeof(addgrpid_str), pid_t_fmt, addgrpid);
#endif
   lListElem *jr;
   if ((jr=get_job_report(job_id, ja_task_id, pe_task_id))) {
      lSetString(jr, JR_osjobid, addgrpid_str);
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
      osjobid_str = lGetString(pe_task, PET_osjobid);
   } else {
      osjobid_str = lGetString(ja_task, JAT_osjobid);
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
#if 0
      char osjobid_str[64];
      sprintf(osjobid_str, OSJOBID_FMT, osjobid);
#endif
      lListElem *jr;
      if ((jr=get_job_report(job_id, ja_task_id, pe_task_id)))
         lSetString(jr, JR_osjobid, osjobid_str);
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
