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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reaping shepherds and turning what they left behind into a job report
 */
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <cerrno>
#include <cfloat>

#include <pwd.h>
#include <sys/wait.h>
#include <grp.h>

#include "uti/config_file.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/config.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/ocs_Binding.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "gdi/ocs_gdi_security.h"

#include "spool/classic/read_write_job.h"

#include "sge.h"
#include "symbols.h"
#include "admin_mail.h"
#include "mail.h"
#include "exec_job.h"
#include "tmpdir.h"
#include "sge_job_qmaster.h"
#include "execution_states.h"
#include "sge_load_sensor.h"
#include "execd.h"
#include "ocs_execd_systemd.h"
#include "reaper_execd.h"
#include "execd_signal_queue.h"
#include "job_report_execd.h"
#include "load_avg.h"
#include "msg_common.h"
#include "msg_daemons_common.h"
#include "msg_execd.h"
#include "sge_io.h"
#include "ocs_execd_systemd.h"

#if defined(BINDING_SOLARIS)
#  include "uti/sge_uidgid.h"
#  include <sys/processor.h>
#  include <sys/types.h>
#  include <sys/pset.h>
#endif 

#ifdef COMPILE_DC
#  include "ptf.h"
static void unregister_from_ptf(uint32_t jobid, uint32_t jataskid, const char *pe_task_id,
                                ocs::tom::Element<ocs::tom::JR> jr);
#endif

static int clean_up_job(ocs::tom::Element<ocs::tom::JR> jr, int failed, int signal, int is_array,
                        ocs::tom::ConstElement<ocs::tom::JAT> ja_task, const char *job_owner);
static void convert_attribute(ocs::tom::List<ocs::tom::CF> cflp, ocs::tom::Element<ocs::tom::JR> jr, const char *name,
                              uint32_t udefau);
static int extract_ulong_attribute(ocs::tom::List<ocs::tom::CF> cflp, const char *name, uint32_t *valuep);

static ocs::tom::Element<ocs::tom::JR>
execd_job_failure(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                  ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string, int general, int failed);
static int read_dusage(ocs::tom::Element<ocs::tom::JR> jr, const char *jobdir, uint32_t job_id, uint32_t ja_task_id,
                       const char *pe_task_id, int failed);
static void build_derived_final_usage(ocs::tom::Element<ocs::tom::JR> jr, uint32_t job_id, uint32_t ja_task_id,
                                      const char *pe_task_id);

static void examine_job_task_from_file(int startup, char *dir, ocs::tom::Element<ocs::tom::JB> jep,
                                       ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                                       ocs::tom::ConstElement<ocs::tom::PET> petep, const std::vector<pid_t> &pids);

static void clean_up_binding(char* binding);
static void reaper_finish_job_scope(ocs::tom::Element<ocs::tom::JR> jr, ocs::tom::ConstElement<ocs::tom::JB> jep,
                                    ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                                    ocs::tom::ConstElement<ocs::tom::PET> petep);
/* TODO: global c file with #define JAPI_SINGLE_SESSION_KEY "JAPI_SSK" */
static const char *JAPI_SINGLE_SESSION_KEY = "JAPI_SSK";

/** @brief Reap the shepherds that have exited and report their jobs
 *
 * Called when the execd got a SIGCLD or starts up. Looks for finished jobs and reports them to qmaster.
 * The input is what the shepherd wrote into the job directory `<execd_spool>/active_jobs/<jobnumber>/`:
 * the "error" file (errors detected by the shepherd), the "trace" file (information for developers),
 * the "usage" file (usage of the job, see getrusage(2)) and the "pid" file (pid of the shepherd).
 * When everything is done the job directory is removed.
 *
 * Delayed reporting (CR6888406) is enabled for jobs which were started via qsub -sync/DRMAA and whose
 * submission time is before the qmaster restart, or if qmaster is currently down.
 *
 * @param max_count how many to reap in this pass, so one call cannot block the daemon
 * @param is_qmaster_down whether qmaster is currently unreachable
 * @return the number reaped
 */
int sge_reap_children_execd(int max_count, bool is_qmaster_down) {
   DENTER(TOP_LAYER);
   pid_t pid = 999;
   int exit_status, child_signal, core_dumped, failed;
   uint32_t jobid, jataskid;

   int status;
   int reap_count = 0;

   DPRINTF("========================REAPER======================\n");

   pid = 999;
   if (max_count < 1) {
      max_count = 1;
   }

   while (pid > 0) {
      if (reap_count >= max_count) {
         DPRINTF("max. reap count is reached - returning. reaped " sge_u32 " childs.\n", reap_count);
         return 1;
      }

      exit_status = child_signal = core_dumped = failed = 0;

      pid = waitpid(-1, &status, WNOHANG);

      if (pid == 0) {
         DPRINTF("pid==0 - no stopped or exited children\n");
         break;
      }

      if (pid == -1) {
         DPRINTF("pid==-1 - no children not previously waited for\n");
         break;
      }

      if (WIFSTOPPED(status)) {
         DPRINTF("PID %d WIFSTOPPED\n", pid);
         continue;
      }

#ifdef WIFCONTINUED
      if (WIFCONTINUED(status)) {
         DPRINTF("PID %d WIFCONTINUED\n", pid);
         continue;
      }
#endif

      if (WIFSIGNALED(status)) {
         child_signal = WTERMSIG(status);
#ifdef WCOREDUMP
         core_dumped = WCOREDUMP(status);
#else
         core_dumped = status & 80;
#endif
         failed = ESSTATE_DIED_THRU_SIGNAL;
      } else if (WIFEXITED(status)) {
         exit_status = WEXITSTATUS(status);
         if (exit_status)
            failed = ESSTATE_SHEPHERD_EXIT;
      } else {
         /* not signaled and not exited - so what else happened with this guy? */
         WARNING(MSG_WAITPIDNOSIGNOEXIT_PI, pid, status);
         continue;
      }

      /* increase reaped job counter */ 
      reap_count++;

      /* search whether it was a job or one of its tasks */
      // the job list is read through the writable access, the task found is changed below
      ocs::tom::Element<ocs::tom::JB> jep;
      ocs::tom::Element<ocs::tom::JAT> jatep;
      ocs::tom::Element<ocs::tom::PET> petep;
      for (const auto job : ocs::DataStore::master_list_rw<SGE_TYPE_JOB>()) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            if ((int)ocs::tom::get<JAT_pid>(ja_task) == pid) {
               jatep = ja_task;
               break;
            }

            for (const auto pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
               if ((int)ocs::tom::get<PET_pid>(pe_task) == pid) {
                  petep = pe_task;
                  break;
               }
            }

            if (petep) {
               jatep = ja_task;
               break;
            }
         }
         if (jatep) {
            jep = job;
            break;
         }
      }

      /* was it a job ? */
      if (jep && jatep) {
         jobid = ocs::tom::get<JB_job_number>(jep);
         jataskid = ocs::tom::get<JAT_task_number>(jatep);
   
         if (child_signal)
            ERROR(MSG_SHEPHERD_VSHEPHERDOFJOBWXDIEDTHROUGHSIGNALYZ_SUUSI, (petep ? MSG_SLAVE : "" ), jobid, jataskid, core_dumped ? MSG_COREDUMPED: "", child_signal);

         if (exit_status) {
            ERROR(MSG_SHEPHERD_WSHEPHERDOFJOBXYEXITEDWITHSTATUSZ_SUUI, (petep ? MSG_SLAVE : "" ), jobid, jataskid, exit_status);
         }

         /* 
          *  seek job report for this job it must be contained in job report
          *  if not it should be a job kept with SGE_KEEP_ACTIVE (without
          *  keeping job object itself)
          */
         DPRINTF("Job: " sge_u32 ", JA-Task: " sge_u32 ", PE-Task: %s\n", jobid, jataskid,
            petep != nullptr ? ocs::tom::get<PET_id>(petep) : "");
         // the job reports belong to job_report_execd.cc
         auto jr = get_job_report(jobid, jataskid,
                                                                  petep != nullptr ? ocs::tom::get<PET_id>(petep)
                                                                                   : nullptr);
         if (!jr) {
            ERROR(MSG_JOB_MISSINGJOBXYINJOBREPORTFOREXITINGJOBADDINGIT_UU, jobid, jataskid);
            if (petep != nullptr) {
               jr = add_job_report(jobid, jataskid, ocs::tom::get<PET_id>(petep), jep);
            } else {
               jr = add_job_report(jobid, jataskid, nullptr, jep);
            }
         }

         /* when restarting execd it happens that cleanup_old_jobs()
            has already cleaned up this job */
         if (ocs::tom::get<JR_state>(jr) == JEXITING) {
            DPRINTF("State of job " sge_u32 " already changed to JEXITING\n", jobid);
            continue;
         }

         ocs::tom::set<JR_state>(jr, JEXITING);

         if(petep != nullptr) {
            ocs::tom::set<PET_status>(petep, JEXITING);
         } else {
            ocs::tom::set<JAT_status>(jatep, JEXITING);
         }

         clean_up_job(jr, failed, exit_status, job_is_array(jep), jatep, ocs::tom::get<JB_owner>(jep));

         reaper_finish_job_scope(jr, jep, jatep, petep);

         /* set JR_delay_report true if the job is a JAPI/DRMAA job ... 
          * -sync/DRMAA jobs have unique session key, qsub jobs have JAPI_SSK
          *  and other clients have null keys as they are not set anywhere
          * We also check if the sync client was started before the qmaster
          * recovered or qmaster is down now, which means the qsub -sync/DRMAA client needs to yet
          * collect the exit status!
          */
         if (ocs::tom::get<JB_session>(jep) && strncmp(ocs::tom::get<JB_session>(jep), JAPI_SINGLE_SESSION_KEY, 8) != 0 && 
               (is_qmaster_down || ocs::tom::get<JB_submission_time>(jep) < sge_get_qmrestart_time())) {
            ocs::tom::set<JR_delay_report>(jr, true);
            INFO(MSG_EXECD_ENABLEDELEAYDREPORTINGFORJOB_U, jobid);
         } else {
            ocs::tom::set<JR_delay_report>(jr, false);
         }

         flush_job_report(jr);

      } else  if (sge_ls_stop_if_pid(pid)) {
         if (child_signal) {
            ERROR(MSG_STATUS_LOADSENSORDIEDWITHSIGNALXY_SI, core_dumped ? MSG_COREDUMPED: "", child_signal);
         } else {
            WARNING(MSG_STATUS_LOADSENSOREXITEDWITHEXITSTATUS_I, exit_status);
         }
      } else {
         if (child_signal) {
            ERROR(MSG_STATUS_MAILERDIEDTHROUGHSIGNALXY_SI, core_dumped ? MSG_COREDUMPED : "", child_signal);
         } else if (exit_status) {
            ERROR(MSG_STATUS_MAILEREXITEDWITHEXITSTATUS_I, exit_status);
         }
      }
   }
   DPRINTF("reaped " sge_u32 " childs - no child remaining\n", reap_count);

   DRETURN(0);
}

#ifdef COMPILE_DC
/**
 * @brief Unregister a job from the PTF and put its final usage into the job report
 *
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr for the array task itself
 * @param jr the job report that receives the usage
 */
static void unregister_from_ptf(const uint32_t job_id, const uint32_t ja_task_id,
                                const char *pe_task_id, const ocs::tom::Element<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::UA> usage;
   int ptf_error;

   ptf_error = ptf_job_complete(job_id, ja_task_id, pe_task_id, usage);
   if (ptf_error) {
      // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
      ocs::tom::Element<ocs::tom::JAT> ja_task_found;
      ocs::tom::Element<ocs::tom::JB> job;

      /* if the job was a 'short-runner' omit the warning */
      if (execd_get_job_ja_task(job_id, ja_task_id, &job, &ja_task_found, false)) {
         const ocs::tom::ConstElement<ocs::tom::JAT> ja_task(ja_task_found);
         // @todo and I doubt that the code is required at all,
         //       might just have been a workaround for bug CS-1019
         //
         // Take the start time of whatever is being reaped. execd_ck_to_do()
         // sets PET_start_time for a PE task and JAT_start_time for the master
         // task, one or the other, never both - so for a PE task the JAT time
         // belongs to the master task, which in a tightly integrated job
         // outlives its tasks by the whole job runtime. Measured against that,
         // no PE task is ever "short" and the check below never fired for the
         // one case that produces short runners in bulk.
         uint64_t start_time = 0;
         if (pe_task_id != nullptr) {
            const ocs::tom::ConstElement<ocs::tom::PET> pe_task(ja_task_search_pe_task(ja_task, pe_task_id));
            if (pe_task) {
               start_time = ocs::tom::get<PET_start_time>(pe_task);
            }
         } else {
            start_time = ocs::tom::get<JAT_start_time>(ja_task);
         }

         /* check if the job was a short-runner */
         // A start time of 0 means we do not know when it started - an unknown
         // PE task, or one reaped before execd stamped it. Warn in that case,
         // which is what this code did before it could tell the two apart.
         if (start_time > 0) {
            uint64_t time_since_started = sge_get_gmt64() - start_time;
            if (time_since_started <= sge_gmt32_to_gmt64(2)) {
               /* the job was started <= 2 seconds before and ended already
                  hence no warning has to be printed because of bug CR 6326191 */
               DRETURN_VOID;
            }
         }
      }

      DSTRING_STATIC(dstr_error, MAX_STRING_SIZE);
      WARNING(MSG_JOB_REAPINGJOBXPTFCOMPLAINSY_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &dstr_error),
              ptf_errstr(ptf_error));
   } else {
      if (usage) {
         // the job report takes the list over and frees the one it held, as exchanging and freeing did
         ocs::tom::set<JR_usage>(jr, std::move(usage));
      }
   }

   DRETURN_VOID;
}
#endif

/************************************************************************
   This is the job clean up and report function. We design this function
   to be as independent from the execd as possible. Maybe we want to 
   make an extra executable later. We have another function cleaning up jobs
   (execd_job_start_failure). This function is called if the job starting 
   failed. At this time there is no config-file present.

   jobid = id of job to reap
   failed = indicates a failure of job execution, see shepherd_states.h
 ************************************************************************/
/** @brief The version the qmaster gave the queue instance a job report names
 *
 * An administrator mail which is sent only once is suppressed per queue instance and sent
 * again as soon as that queue instance changes. The queue version is what tells a change:
 * the qmaster increases it whenever the configuration or the state of the queue instance
 * changes, and the scheduler stores it in the granted destination list of the job, so it is
 * at hand here without asking the qmaster for it.
 *
 * @param jr      the job report, which names the queue the failure is attributed to
 * @param ja_task the array task of the job, may be an empty handle
 * @return the version of that queue instance, or 0 when it cannot be determined
 */
static uint32_t
reaper_get_qversion(const ocs::tom::ConstElement<ocs::tom::JR> jr,
                    const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   uint32_t ret = 0;
   const char *qname = ocs::tom::get<JR_queue_name>(jr);
   if (ja_task && qname != nullptr) {
      const auto gdil_ep = ocs::tom::find_first_in<JAT_granted_destin_identifier_list, JG_qname>(ja_task, qname);
      if (gdil_ep) {
         ret = ocs::tom::get<JG_qversion>(gdil_ep);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Finish the systemd scope of a job or pe task which has ended
 *
 * Called after the job (or pe task) and its epilog have ended, and after clean_up_job() has set
 * JR_failed. Reports the OOM killer as the one who deleted the job when it ended the job scope,
 * and resets the failed state of the scope, see ocs::execd::execd_finish_job_scope().
 *
 * @param jr the job report of the job or pe task
 * @param jep the job
 * @param jatep the ja task
 * @param petep the pe task, an empty handle for the job itself
 */
static void
reaper_finish_job_scope(const ocs::tom::Element<ocs::tom::JR> jr, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                        const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                        const ocs::tom::ConstElement<ocs::tom::PET> petep) {
   DENTER(TOP_LAYER);

#if defined(OCS_WITH_SYSTEMD)
   // the scope name is not spooled, after a restart of sge_execd it has to be built again
   const char *scope_name = petep != nullptr ? ocs::tom::get<PET_systemd_scope>(petep)
                                             : ocs::tom::get<JAT_systemd_scope>(jatep);
   std::string scope;
   if (scope_name != nullptr) {
      scope = scope_name;
   } else if (ocs::execd::execd_use_systemd()) {
      std::string slice;
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      if (!ocs::Job::job_get_systemd_slice_and_scope(jep, jatep, petep, slice, scope,
                                                     &error_dstr)) {
         scope.clear();
      }
   }

   // The job has run in its scope unless it failed before it was started. A job which did not
   // run might find a failed scope of an earlier job with the same name, which is not its own.
   uint32_t failed = ocs::tom::get<JR_failed>(jr);
   bool job_has_run = failed == 0 || failed == SSTATE_FAILURE_AFTER_JOB ||
                      (failed >= SSTATE_BEFORE_PESTOP && failed <= SSTATE_PROCSET_NOTFREED);

   // When the OOM killer ended the job scope, report it as the one who deleted the job.
   // It is the actual cause, even if sge_execd killed the job for exceeding a limit as well.
   if (!scope.empty() && ocs::execd::execd_finish_job_scope(scope.c_str()) && job_has_run) {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      sge_dstring_sprintf(&dstr, "oom-killer@%s", component_get_qualified_hostname());
      ocs::tom::set<JR_deleted_by>(jr, sge_dstring_get_string(&dstr));
      DSTRING_STATIC(id_dstr, MAX_STRING_SIZE);
      const char *pe_task_id = petep != nullptr ? ocs::tom::get<PET_id>(petep) : nullptr;
      WARNING(MSG_EXECD_OOMKILLED_SS,
              job_get_id_string(ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep),
                                pe_task_id, &id_dstr), scope.c_str());
   }
#endif

   DRETURN_VOID;
}

/**
 * @brief Read what the shepherd of a job left behind and fill in the job report
 *
 * Reads the exit status, error, usage and checkpoint files from the active jobs directory and sets the failure
 * state, the general failure and the usage of the job report; sends an administrator mail on a failure.
 *
 * @param jr the job report of the job or PE task
 * @param failed the failure seen by the daemon, see shepherd_states.h, 0 if none
 * @param shepherd_exit_status the exit status of the shepherd
 * @param is_array whether the job is an array job, for the administrator mail
 * @param ja_task the array task, or an empty handle
 * @param job_owner the owner of the job, for the administrator mail
 * @return 0, or -1 if the job report is missing or the active jobs directory cannot be found
 */
static int clean_up_job(const ocs::tom::Element<ocs::tom::JR> jr, int failed, const int shepherd_exit_status,
                        const int is_array, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                        const char *job_owner) {
   DENTER(TOP_LAYER);

   char* binding;
   dstring jobdir = DSTRING_INIT;
   dstring fname  = DSTRING_INIT;

   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   SGE_STRUCT_STAT statbuf;
   char error[10000];
   FILE *fp;
   uint32_t job_id, job_pid, ckpt_arena, general_failure = 0, ja_task_id;
   const char *pe_task_id = nullptr;

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   if (!jr) {
      CRITICAL(SFNMAX, MSG_JOB_CLEANUPJOBCALLEDWITHINVALIDPARAMETERS);
      DRETURN(-1);
   }

   job_id = ocs::tom::get<JR_job_number>(jr);
   ja_task_id = ocs::tom::get<JR_ja_task_number>(jr);
   pe_task_id = ocs::tom::get<JR_pe_task_id_str>(jr);

   DPRINTF("cleanup for job %s\n", job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));

#ifdef COMPILE_DC
   unregister_from_ptf(job_id, ja_task_id, pe_task_id, jr);
#else
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_WALLCLOCK);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_CPU);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_MEM);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_IO);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_IOOPS);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_IOW);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_VMEM);
   ocs::tom::erase_in<JR_usage, UA_name>(jr, USAGE_ATTR_MAXVMEM);
#endif

#if 0  
   /* moved to remove_acked_job_exit() */
   krb_destroy_forwarded_tgt(job_id);
#endif

   /* set directory for job */
   sge_get_active_job_file_path(&jobdir, job_id, ja_task_id, pe_task_id, nullptr);

   if (SGE_STAT(sge_dstring_get_string(&jobdir), &statbuf)) {
      /* This never should happen, cause if we cant create this directory on
         startup we report the job finish immediately */
      ERROR(MSG_JOB_CANTFINDDIRXFORREAPINGJOBYZ_SS, sge_dstring_get_string(&jobdir), job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      sge_dstring_free(&jobdir);       
      return -1;        /* nothing can be done without information */
   }

   /* read config written by exec_job */
   sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "config");
   if (read_config(sge_dstring_get_string(&fname))) {
      /* This should happen very rarely. exec_job() should avoid this 
         condition as far as possible. One possibility for this case is, 
         that the execd dies just after making the jobs active directory. 
         The pain with this case is, that we have not much information
         to report this job to qmaster. */
      ERROR(MSG_JOB_CANTREADCONFIGFILEFORJOBXY_S, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      ocs::tom::set<JR_failed>(jr, ESSTATE_NO_CONFIG);
      ocs::tom::set<JR_err_str>(jr, (char *) MSG_SHEPHERD_EXECDWENTDOWNDURINGJOBSTART);

      sge_dstring_free(&fname);
      sge_dstring_free(&jobdir);
      DRETURN(0);
   }

   *error = '\0';

   DTRACE;

   /* job to core binding: on Solaris the processor set have to be deleted
      and the cores have to be freed */ 
      
   binding = get_conf_val("binding");
   clean_up_binding(binding);

   /*
    * look for exit status of shepherd This is the last file the shepherd
    * creates. So if we can find this shepherd terminated normal.
    */
   sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "exit_status");
   if (!(fp = fopen(sge_dstring_get_string(&fname), "r"))) {
      /* 
       * we trust the exit status of the shepherd if it exited regularly
       * otherwise we assume it died before starting the job (if died through signal or
       * job_dir was found during execd startup)
       */
      if (failed == ESSTATE_SHEPHERD_EXIT) {
         failed = shepherd_exit_status;
      } else {
         failed = SSTATE_BEFORE_PROLOG;
      }

      snprintf(error, sizeof(error), MSG_STATUS_ABNORMALTERMINATIONOFSHEPHERDFORJOBXY_S,
               job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      ERROR(SFNMAX, error);
 
      /* 
       * failed = ESSTATE_SHEPHERD_EXIT or exit status of shepherd if we are
       * parent in case we can't open the exit_status file
       */
   } else {
      int fscanf_count, shepherd_exit_status_file;
         
      fscanf_count = fscanf(fp, "%d", &shepherd_exit_status_file);
      FCLOSE_IGNORE_ERROR(fp);
      if (fscanf_count != 1) {
         snprintf(error, sizeof(error), MSG_STATUS_ABNORMALTERMINATIONFOSHEPHERDFORJOBXYEXITSTATEFILEISEMPTY_S,
                  job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
         ERROR(SFNMAX, error);
         /* 
          * If shepherd died through signal assume job was started, else
          * trust exit status
          */
         if (failed == ESSTATE_SHEPHERD_EXIT)
            failed = shepherd_exit_status;
         else
            failed = ESSTATE_NO_EXITSTATUS;
      } else if (failed != ESSTATE_NO_PID)  /* only set during execd startup */
         failed = shepherd_exit_status_file;
         
      /* 
       * failed is content of exit_status file or ESSTATE_NO_PID or real
       * exit status if file can't be read
       */   
   }


   /*
    * failed is:
    *    exit status of shepherd (one of SSTATE_* values)
    *    ESSTATE_DIED_THRU_SIGNAL
    *    ESSTATE_NO_PID 
    */

   if (failed) {
      if (failed == ESSTATE_DIED_THRU_SIGNAL) {
         snprintf(error, sizeof(error), SFNMAX, MSG_SHEPHERD_DIEDTHROUGHSIGNAL);
      } else if (failed == ESSTATE_NO_PID) {
         snprintf(error, sizeof(error), SFNMAX, MSG_SHEPHERD_NOPIDFILE);
      } else {
         snprintf(error, sizeof(error), MSG_SHEPHERD_EXITEDWISSTATUS_IS, failed, get_sstate_description(failed));
      }
   }

   /* look for error file this overrules errors found yet */
   sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "error");
   if ((fp = fopen(sge_dstring_get_string(&fname), "r"))) {
      int n;
      char *new_line;
      if ((n = fread(error, 1, sizeof(error), fp))) {
         /* Non empty error file. The shepherd encounterd a problem. */ 
         if (!failed)
            failed = ESSTATE_UNEXP_ERRORFILE;
         error[n] = '\0';
         /* ensure only first line of error file is in 'error' */
         if ((new_line=strchr(error, '\n')))
            *new_line = '\0';
         DPRINTF("ERRORFILE: %256s\n", error);
      }
      else if (feof(fp)) {
         DPRINTF("empty error file\n");
      } else {
         ERROR(MSG_JOB_CANTREADERRORFILEFORJOBXY_S, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      }      
      FCLOSE_IGNORE_ERROR(fp);
   } else {
      ERROR(MSG_FILE_NOOPEN_SS, sge_dstring_get_string(&fname), strerror(errno));
      /* There is no error file. */
   }

   DTRACE;

   /*
   ** now that shepherd stops on errors there is no usage file
   ** if the job never ran
   ** but we need a dusage struct because information that has nothing 
   ** to do with the job also goes in there, this should be changed
   ** read_dusage gets the failed parameter to decide what should be there
   */
   if (read_dusage(jr, sge_dstring_get_string(&jobdir), job_id, ja_task_id, pe_task_id, failed)) {
      if (error[0] == '\0') {
         snprintf(error, sizeof(error), MSG_JOB_CANTREADUSAGEFILEFORJOBXY_S, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      }
      
      ERROR(SFNMAX, error);
      
      if (!failed) {
         failed = SSTATE_FAILURE_AFTER_JOB;
      }
      
      DTRACE;
   }

   

   /* map system signals into sge signals to make signo's exchangable */
   ocs::tom::Element<ocs::tom::UA> du=ocs::tom::find_first_in_rw<JR_usage, UA_name>(jr, "signal");
   if (du) {
      int signo = (int)ocs::tom::get<UA_value>(du);

      if (signo) {
         int sge_signo;

         /* Job died through a signal */
         snprintf(error, sizeof(error), MSG_JOB_WXDIEDTHROUGHSIGNALYZ_SSI,
                  job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
                  sge_sys_sig2str(signo), signo);

         DPRINTF("%s\n", error);
         failed = SSTATE_FAILURE_AFTER_JOB;

         if ((sge_signo=sge_map_signal(signo)) != -1)
            ocs::tom::set<UA_value>(du, (double)sge_signo);
      }

      if ((uint32_t)ocs::tom::get<UA_value>(du) == 0xffffffff) {
         if (!failed) {
            failed = SSTATE_FAILURE_AFTER_JOB;
            if (!*error)
               snprintf(error, sizeof(error), SFNMAX, MSG_JOB_CANTREADUSEDRESOURCESFORJOB);
         }
      }
   }
      
   DTRACE;

   /* Be careful: the checkpointing checking is done at the end. It will
    * often override other failure states.
    * If the job finishes, the shepherd must remove the "checkpointed" file
    */  

   sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "checkpointed");
   ckpt_arena = 1;   /* 1 job will be restarted in case of failure *
                      * 2 job will be restarted from ckpt arena    */
   if (!SGE_STAT(sge_dstring_get_string(&fname), &statbuf)) {
      int dummy;

      failed = SSTATE_MIGRATE;
      if ((fp = fopen(sge_dstring_get_string(&fname), "r"))) {
         DPRINTF("found checkpointed file\n");
         if (fscanf(fp, "%d", &dummy)==1) {
            DPRINTF("need restart from ckpt arena\n");
            ckpt_arena = 2;
         }
         FCLOSE_IGNORE_ERROR(fp);
      }   

      sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "job_pid");
      if (!SGE_STAT(sge_dstring_get_string(&fname), &statbuf)) {
         if ((fp = fopen(sge_dstring_get_string(&fname), "r"))) {
            if (!fscanf(fp, sge_u32 , &job_pid))
               job_pid = 0;
            FCLOSE_IGNORE_ERROR(fp);
         } else {
            job_pid = 0;
            ERROR(MSG_JOB_CANTOPENJOBPIDFILEFORJOBXY_S, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
         }
      } else {
         job_pid = 0;
      }
   } else {
      job_pid = 0;
   }

   ocs::tom::set<JR_job_pid>(jr, job_pid);
   
   /* Only used for ckpt jobs: 1 checkpointed, 2 checkpoint in the arena */
   ocs::tom::set<JR_ckpt_arena>(jr, ckpt_arena);

   /* Currently the shepherd doesn't create this file */
   sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "noresources");
   if (!SGE_STAT(sge_dstring_get_string(&fname), &statbuf))
      failed = SSTATE_AGAIN;
   
   /* failed */
   ocs::tom::set<JR_failed>(jr, failed);
   DPRINTF("job report for job " SFN ": failed = %ld\n",
            job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), failed);
   /* err_str */
   if (*error) {
      ocs::tom::set<JR_err_str>(jr, error);
      DPRINTF("job report for job " SFN ": err_str = %s\n",
               job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), error);
   }

   /* the queue instance the failure is attributed to, for the administrator mail */
   uint32_t qversion = reaper_get_qversion(jr, ja_task);

   /* general_failure */
   switch (failed) {
   case SSTATE_BEFORE_PROLOG:
   case SSTATE_CHECK_DAEMON_CONFIG:
   case SSTATE_SYSTEMD_ERROR:

   /* for CR 6252457 this block should be removed, but for a real fix the
      host or queue error detection must be improved
   */
      general_failure = GFSTATE_HOST;
      ocs::tom::set<JR_general_failure>(jr, general_failure);
      job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      break;
   case SSTATE_PROCSET_NOTSET:
   case SSTATE_READ_CONFIG:
   case SSTATE_PROLOG_FAILED:
   case SSTATE_BEFORE_PESTART:
   case SSTATE_PESTART_FAILED:
   case SSTATE_BEFORE_PESTOP:
   case SSTATE_PESTOP_FAILED:
   case SSTATE_BEFORE_EPILOG:
   case SSTATE_EPILOG_FAILED:
      general_failure = GFSTATE_QUEUE;
      ocs::tom::set<JR_general_failure>(jr, general_failure);
      job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      break;
   case SSTATE_ADD_GRP_SET_ERROR:
   case SSTATE_BEFORE_JOB:
   case SSTATE_NO_SHELL:
      {
         int job_caused_failure = 0;
         // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
         ocs::tom::Element<ocs::tom::JB> job_found;
         ocs::tom::Element<ocs::tom::JAT> ja_task_found;
         ocs::tom::ConstElement<ocs::tom::QU> master_queue;

         /* search job and ja_task */
         if (execd_get_job_ja_task(job_id, ja_task_id, &job_found, &ja_task_found, false)) {
            master_queue = responsible_queue(ocs::tom::ConstElement<ocs::tom::JB>(job_found),
                                             ocs::tom::ConstElement<ocs::tom::JAT>(ja_task_found), nullptr);
         }
         const ocs::tom::ConstElement<ocs::tom::JB> job(job_found);

         if ((failed == SSTATE_NO_SHELL) && (job != nullptr) &&
             ((ocs::tom::get<JB_shell_list>(job) != nullptr) ||
             ((master_queue != nullptr) && JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(job))
                                        && JOB_TYPE_IS_NO_SHELL(ocs::tom::get<JB_type>(job))))) {
            job_caused_failure = 1;
         } else if ((failed == SSTATE_NO_SHELL) && (master_queue != nullptr)) {
            char* shell_start_mode = mconf_get_shell_start_mode();
            const char *mode = job_get_shell_start_mode(master_queue, shell_start_mode);
            sge_free(&shell_start_mode);

            if (strcmp(mode, "unix_behavior") == 0) {
               job_caused_failure = 1;
            }
         } else if ((failed == SSTATE_BEFORE_JOB) && (job != nullptr) && JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(job)) &&
                    !sge_is_file(ocs::tom::get<JB_script_file>(job))) {
            job_caused_failure = 1;
         } else if (failed == SSTATE_ADD_GRP_SET_ERROR) {
            job_caused_failure = 1;
         }

         general_failure = job_caused_failure ? GFSTATE_JOB : GFSTATE_QUEUE;
         ocs::tom::set<JR_general_failure>(jr, general_failure);
         job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      }
      break;
   /*
   ** if an error occurred where the job 
   ** is source of the failure 
   */
   case SSTATE_OPEN_OUTPUT:
   case SSTATE_NO_CWD:
   case SSTATE_AFS_PROBLEM:
   case SSTATE_APPERROR:
      general_failure = GFSTATE_JOB;
      ocs::tom::set<JR_general_failure>(jr, general_failure);
      job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      break;
   /*
   ** if an error occurred after the job has been run
   ** it is not as serious
   */
   case SSTATE_PROCSET_NOTFREED:
      general_failure = GFSTATE_NO_HALT;
      ocs::tom::set<JR_general_failure>(jr, general_failure);
      job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      break;
   /*
   ** these are shepherd error conditions met by the execd
   */
   case ESSTATE_NO_CONFIG:
   case ESSTATE_NO_PID:
   case ESSTATE_DIED_THRU_SIGNAL:
   case ESSTATE_SHEPHERD_EXIT:
   case ESSTATE_NO_EXITSTATUS:
   case ESSTATE_UNEXP_ERRORFILE:
   case ESSTATE_UNKNOWN_JOB:
      /*
      ** test for admin mail here
      */
      general_failure = GFSTATE_NO_HALT;
      ocs::tom::set<JR_general_failure>(jr, general_failure);
      job_related_adminmail(EXECD, jr, is_array, job_owner, qversion);
      break;
   default: 
      general_failure = GFSTATE_NO_HALT;
      /*
      ** this is no error, because not all failures apply to general_failure
      */
      break;
   } /* switch */
   
   ocs::tom::set<JR_general_failure>(jr, general_failure);
   DPRINTF("job report for job " SFN ": general_failure = %ld\n",
           job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), general_failure);

   sge_dstring_free(&fname);
   sge_dstring_free(&jobdir);

   DRETURN(0);
}

/**
 * @brief Write the failure of a job into the file "failed" of its active jobs directory
 *
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr
 * @param jr the job report holding the failure
 */
static void
write_failed_info(const uint32_t job_id, const uint32_t ja_task_id, const char *pe_task_id,
                  const ocs::tom::ConstElement<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(dstr_filename, SGE_PATH_MAX);
   DSTRING_STATIC(dstr_failed, MAX_STRING_SIZE);
   sge_get_active_job_file_path(&dstr_filename, job_id, ja_task_id, pe_task_id, "failed");
   sge_dstring_sprintf(&dstr_failed, sge_u32 ":" SFN2 "\n",
                       ocs::tom::get<JR_failed>(jr), ocs::tom::get<JR_err_str>(jr));
   sge_string2file(sge_dstring_get_string(&dstr_failed), sge_dstring_strlen(&dstr_failed),
                sge_dstring_get_string(&dstr_filename));

   DRETURN_VOID;
}

/* ------------------------- */
/** @brief Delete a finished job's directory once qmaster has acknowledged it
 *
 * Not before: the active_jobs directory is the only record of what happened,
 * and a report that never arrives has to be sendable again.
 *
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr
 * @param jr the job report that was acknowledged
 */
void remove_acked_job_exit(const uint32_t job_id, const uint32_t ja_task_id, const char *pe_task_id,
                           const ocs::tom::Element<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);
   char *exec_file, *script_file, *tmpdir, *job_owner, *qname;
   dstring jobdir = DSTRING_INIT;
   char fname[SGE_PATH_MAX];
   char err_str_buffer[10000];
   dstring err_str;
   SGE_STRUCT_STAT statbuf;
   // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
   ocs::tom::Element<ocs::tom::JB> jep_found;
   ocs::tom::Element<ocs::tom::JAT> jatep_found;
   const char *pe_task_id_str;
   bool do_rm_active_dir = true;

   sge_dstring_init(&err_str, err_str_buffer, sizeof(err_str_buffer));

   if (ja_task_id == 0) {
      ERROR(MSG_SHEPHERD_REMOVEACKEDJOBEXITCALLEDWITHX_U, job_id);
      DRETURN_VOID;
   }

   pe_task_id_str = jr?ocs::tom::get<JR_pe_task_id_str>(jr):nullptr;

   // check if the job shall be kept active
   {
      keep_active_t keep_active = mconf_get_keep_active();
      bool job_failed = ocs::tom::get<JR_failed>(jr) != 0 ? true : false;

      if (keep_active == KEEP_ACTIVE_TRUE ||
          sge_getenv("SGE_KEEP_ACTIVE") ||
          (keep_active == KEEP_ACTIVE_ERROR && job_failed)) {
         do_rm_active_dir = false;
      }

      if (!do_rm_active_dir && job_failed) {
         // write failure information info a file in the active jobs dir
         write_failed_info(job_id, ja_task_id, pe_task_id, jr);
      }
   }

   /* try to find this job in our job list */
   if (execd_get_job_ja_task(job_id, ja_task_id, &jep_found, &jatep_found, false)) {
      const ocs::tom::Element<ocs::tom::JB> jep(jep_found);
      const ocs::tom::Element<ocs::tom::JAT> jatep(jatep_found);
      ocs::tom::Element<ocs::tom::PET> petep;
      ocs::tom::Element<ocs::tom::QU> master_q;
      int used_slots;

      // We get here through an ACK_JOB_EXIT.
      // If it is not a pe task we are removing, then it is the master task.
      // If there are still pe tasks running, we may not yet remove the master task,
      // as we would delete the pe tasks' active_job directories and other data.
      // Just return, the ACK_JOB_EXIT will be repeated by sge_qmaster.
      if (pe_task_id == nullptr &&
          !ocs::tom::get<JAT_task_list>(jatep).empty()) {
         // flush the job report to speed up repeating of the ACK_JOB_EXIT
         flush_job_report(jr);
         DRETURN_VOID;
      }

      DPRINTF("REMOVING WITH jep && jatep\n");
      if (pe_task_id_str) {
         petep = ocs::tom::get<JAT_task_list>(jatep).find_first<PET_id>(pe_task_id_str);

         if (!petep) {
            ERROR(MSG_JOB_XYHASNOTASKZ_UUS, job_id, ja_task_id, pe_task_id_str);
            del_job_report(jr);
            DRETURN_VOID;
         }

         if (ocs::tom::get<JR_state>(jr) != JEXITING) {
            WARNING(MSG_EXECD_GOTACKFORPETASKBUTISNOTINSTATEEXITING_S, pe_task_id_str);
            /* job report has to be deleted */
            del_job_report(jr);
            DRETURN_VOID;
         }

         master_q = responsible_queue(jep, jatep, petep);
      } else {
         master_q = responsible_queue(jep, jatep, nullptr);
      }
     
      /* use mail list of job instead of tasks one */
      if (jr != nullptr && ocs::tom::get<JR_state>(jr) != JSLAVE) {
         reaper_sendmail(jep, jr);
      }

      /* remove job/task active dir */
      if (do_rm_active_dir) {
         DPRINTF("removing active dir: %s\n", sge_dstring_get_string(&jobdir));
         sge_get_active_job_file_path(&jobdir, job_id, ja_task_id, pe_task_id, nullptr);
         if (sge_rmdir(sge_dstring_get_string(&jobdir), &err_str)) {
            ERROR(MSG_FILE_CANTREMOVEDIRECTORY_SS, sge_dstring_get_string(&jobdir), err_str_buffer);
         }
      }

      /* increment # of free slots. In case no slot is used any longer 
         we have to remove queues tmpdir for this job */
      /* decrement the number of used slots.
       * not done for the slave container of tightly integrated parallel jobs
       */
      used_slots = qinstance_slots_used(master_q);
      if (ocs::tom::get<JAT_status>(jatep) == JSLAVE && pe_task_id_str == nullptr) {
         DPRINTF("remove_acked_job_exit for slave container - not decreasing used slots");
      } else {
         used_slots--;
         qinstance_set_slots_used(master_q, used_slots);
         DPRINTF("%s: used slots decreased to %d\n", ocs::tom::get<QU_full_name>(master_q), used_slots);
      }

      /*
       * when the job finished / the last task of a pe job finished
       * delete the tmpdir for this job
       * the slave container of a tightly integrated parallel job 
       * does not have a tmpdir
       */
      if (used_slots == 0) {
         if (ocs::tom::get<JAT_status>(jatep) != JSLAVE || pe_task_id_str != nullptr) {
         sge_remove_tmpdir(ocs::tom::get<QU_tmpdir>(master_q), 
                           ocs::tom::get<JB_owner>(jep), ocs::tom::get<JB_job_number>(jep), 
                           ja_task_id, ocs::tom::get<QU_qname>(master_q));
         }
      }

      if (pe_task_id_str == nullptr) {
         /*
          * Clean-up the job container (master and slave tasks)
          */

         if (!mconf_get_simulate_jobs()) {
            job_remove_spool_file(job_id, ja_task_id, nullptr, SPOOL_WITHIN_EXECD);

            if (!JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(jep)) &&
                ocs::tom::get<JB_exec_file>(jep) &&
                ocs::tom::get<JAT_status>(jatep) != JSLAVE) {

               /*
                * Unlink job_script if there are no other master tasks
                * (e.g. task arrays) for this job present on execd
                */
               if (count_master_tasks(ocs::DataStore::master_list<SGE_TYPE_JOB>(),
                                      job_id) <= 1) {
                  DPRINTF("unlinking script file %s\n", ocs::tom::get<JB_exec_file>(jep));
                  unlink(ocs::tom::get<JB_exec_file>(jep));
               }
            }
         }
      } else {
         DPRINTF("not removing job file: pe_task_id_str = %s\n", pe_task_id_str);
      }

      if (pe_task_id_str != nullptr) {
         /*
          * Clean-up the PE task (a second pass cleans the container)
          */

         /* unchain pe task element from task list */
         ocs::tom::get<JAT_task_list>(jatep).erase(petep);

         /*
          * if this was the last pe_task of an exiting slave job
          * send the slave job final report
          */
         if (ocs::tom::get<JAT_task_list>(jatep).empty()) {
            const auto jr = get_job_report(job_id, ja_task_id, nullptr);
            if (jr != nullptr && ocs::tom::get<JR_state>(jr) == JSLAVE && ISSET(ocs::tom::get<JAT_state>(jatep), JDELETED)) {
               add_usage(jr, "exit_status", nullptr, 0);
               flush_job_report(jr);
            }
         }
      } else {
         /* check if job has queue limits and decrease global flag if necessary */
         modify_queue_limits_flag_for_job(component_get_qualified_hostname(), jep, false);

         if (used_slots == 0 || mconf_get_simulate_jobs()) {
            /* remove the jep element only if all slave tasks are gone, we need to job object to remove the tmpdir */
            ocs::DataStore::master_list_rw<SGE_TYPE_JOB>().erase(jep);
         }
      }

      /* delete the job report */
      del_job_report(jr);

   } else {
      /* must be an ack of an ask job request from qmaster */
      DPRINTF("REMOVING WITHOUT jep && jatep\n");
      /* clean up active jobs entry */
      if (pe_task_id_str == nullptr) {
         ERROR(MSG_SHEPHERD_ACKNOWLEDGEFORUNKNOWNJOBXYZ_UUS, job_id,  ja_task_id, (pe_task_id_str ? pe_task_id_str : MSG_MASTER));

      /*
      ** security hook
      */
         sge_get_active_job_file_path(&jobdir, job_id, ja_task_id, pe_task_id, nullptr);
         if (SGE_STAT(sge_dstring_get_string(&jobdir), &statbuf)) {
            ERROR(MSG_SHEPHERD_CANTFINDACTIVEJOBSDIRXFORREAPINGJOBY_SU, sge_dstring_get_string(&jobdir), job_id);
         } else {
            /*** read config file written by exec_job ***/ 
            snprintf(fname, sizeof(fname), "%s/config", sge_dstring_get_string(&jobdir));
            if (read_config(fname)) {
               /* This should happen very rarely. exec_job() should avoid this 
                  condition as far as possible. One possibility for this case is, 
                  that the execd dies just after making the jobs active directory. 
                  The pain with this case is, that we have not much information
                  to report this job to qmaster. */

               if (sge_rmdir(sge_dstring_get_string(&jobdir), &err_str)) {
                  ERROR(MSG_FILE_CANTREMOVEDIRECTORY_SS, sge_dstring_get_string(&jobdir), err_str_buffer);
               }
            }

            /* do not remove xterm or qlogin starter ! */
            if ((script_file = get_conf_val("script_file")) && strcasecmp(script_file, "INTERACTIVE") && strcasecmp(script_file, "QLOGIN")) {
               if ((exec_file = get_conf_val("exec_file"))) {
                  DPRINTF("removing exec_file %s\n", exec_file);
                  unlink(exec_file);
               }
            }
            
            /* tmpdir */
            if ((!(tmpdir = get_conf_val("queue_tmpdir"))) || (!(qname = get_conf_val("queue"))) || (!(job_owner = get_conf_val("job_owner")))) {
               ERROR(MSG_SHEPHERD_INCORRECTCONFIGFILEFORJOBXY_UU, job_id, ja_task_id);
            } else {
               DPRINTF("removing queue_tmpdir %s\n", tmpdir);
               sge_remove_tmpdir(tmpdir, job_owner, job_id, ja_task_id, qname);
            }
         }

         /* job */
         if (!mconf_get_simulate_jobs()) {
            job_remove_spool_file(job_id, ja_task_id, nullptr, SPOOL_WITHIN_EXECD);
         }

         /* remove job/task active dir */
         if (do_rm_active_dir) {
            DPRINTF("removing active dir: %s\n", sge_dstring_get_string(&jobdir));
            if (sge_rmdir(sge_dstring_get_string(&jobdir), &err_str)) {
               ERROR(MSG_FILE_CANTREMOVEDIRECTORY_SS, sge_dstring_get_string(&jobdir), err_str_buffer);
            }
         }
      }
      del_job_report(jr);
      cleanup_job_report(job_id, ja_task_id);
   }

   sge_dstring_free(&jobdir);

   DRETURN_VOID;
}

/** @brief Report that a job could not be started at all
 *
 * The job is cleaned up and its number and the error are reported to qmaster. If general is set,
 * the host has a general problem starting jobs (e.g. a file system that cannot be written): qmaster
 * should not send further jobs and the administrator should be informed.
 *
 * @param jep the job
 * @param jatep the array task, or an empty handle
 * @param petep the PE task, or an empty handle
 * @param error_string what went wrong
 * @param general whether the failure is the host's fault rather than the job's
 * @return the job report that was built, or an empty handle if none could be built
 */
ocs::tom::Element<ocs::tom::JR>
execd_job_start_failure(const ocs::tom::ConstElement<ocs::tom::JB> jep,
                        const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                        const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string,
                        const int general) {
   DENTER(TOP_LAYER);
   DRETURN(execd_job_failure(jep, jatep, petep, error_string, general, SSTATE_FAILURE_BEFORE_JOB));
}

/** @brief Report that a job started but then failed
 * @param jep the job
 * @param jatep the array task, or an empty handle
 * @param petep the PE task, or an empty handle
 * @param error_string what went wrong
 * @param general whether the failure is the host's fault rather than the job's
 * @return the job report that was built, or an empty handle if none could be built
 */
ocs::tom::Element<ocs::tom::JR>
execd_job_run_failure(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                      const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string,
                      const int general) {
   DENTER(TOP_LAYER);
   DRETURN(execd_job_failure(jep, jatep, petep, error_string, general, SSTATE_FAILURE_AFTER_JOB));
}

/**
 * @brief Put the failure of a job into its job report and send an administrator mail
 *
 * @param jep the job
 * @param jatep the array task, or an empty handle
 * @param petep the PE task, or an empty handle
 * @param error_string what went wrong
 * @param general the general failure state, see GFSTATE_*
 * @param failed the failure state, SSTATE_FAILURE_BEFORE_JOB or SSTATE_FAILURE_AFTER_JOB
 * @return the job report, or an empty handle if it could not be created
 */
static ocs::tom::Element<ocs::tom::JR>
execd_job_failure(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                  const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string, const int general,
                  const int failed) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JG> ep;
   uint32_t jobid, jataskid = 0;
   const char *petaskid = nullptr;

   jobid = ocs::tom::get<JB_job_number>(jep);
   if (jatep) {
      jataskid = ocs::tom::get<JAT_task_number>(jatep);
   }
   if (petep) {
      petaskid = ocs::tom::get<PET_id>(petep);
   }

   ERROR((failed==SSTATE_FAILURE_BEFORE_JOB)? MSG_SHEPHERD_CANTSTARTJOBXY_US: MSG_SHEPHERD_PROBLEMSAFTERSTART_DS, jobid, error_string);

   // the job reports belong to job_report_execd.cc
   auto jr = get_job_report(jobid, jataskid, petaskid);
   if (!jr) {
      DPRINTF("no job report found to report job start failure!\n");
      jr = add_job_report(jobid, jataskid, petaskid, jep);
   }

   if (jr) {
      if (petep) {
         ep = ocs::tom::get<PET_granted_destin_identifier_list>(petep).front();
      } else if (jatep) {
         ep = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front();
      }

      if (ep) {
         ocs::tom::set<JR_queue_name>(jr, ocs::tom::get<JG_qname>(ep));
      }

      ocs::tom::set<JR_failed>(jr, failed);
      ocs::tom::set<JR_general_failure>(jr, general);
      ocs::tom::set<JR_err_str>(jr, error_string);

      ocs::tom::set<JR_state>(jr, JEXITING);

      job_related_adminmail(EXECD, jr, job_is_array(jep), ocs::tom::get<JB_owner>(jep),
                            ep ? ocs::tom::get<JG_qversion>(ep) : 0);
   }

   DRETURN(jr);
}


/** @brief Handle a job qmaster says it does not know about
 *
 * Happens after a qmaster restart from an older spool: the job is running here
 * but no longer exists there, so it has to be cleaned up rather than reported.
 * qmaster is told about it the same way as for a normal job finish, and runs into the same functions.
 *
 * @param jobid the job
 * @param jataskid the array task
 * @param qname the queue instance it was running in
 */
void job_unknown(uint32_t jobid, uint32_t jataskid, char *qname) {
   DENTER(TOP_LAYER);

   ERROR(MSG_SHEPHERD_JATASKXYISKNOWNREPORTINGITTOQMASTER, jobid, jataskid);

   // If there are still pe tasks running for this unknown job,
   // we first create the unknown reports for all the pe tasks.
   // Once they are all handled in sge_qmaster, it will repeat the ACK_SIGNAL_JOB for the master task,
   // which will then call this function again with the master task.
   // If there are no pe tasks running, we can create the job report for the master
   // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
   ocs::tom::Element<ocs::tom::JB> job;
   ocs::tom::Element<ocs::tom::JAT> ja_task_found;
   bool unknown_tasks = false;
   if (execd_get_job_ja_task(jobid, jataskid, &job, &ja_task_found, false)) {
      const ocs::tom::ConstElement<ocs::tom::JAT> ja_task(ja_task_found);
      for (const auto pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
         // create the unknown job report for this pe task
         const char *pe_task_id = ocs::tom::get<PET_id>(pe_task);
         auto jr = get_job_report(jobid, jataskid, pe_task_id);
         if (!jr) {
            jr = add_job_report(jobid, jataskid, pe_task_id, nullptr);
         }
         if (jr) {
            ocs::tom::set<JR_queue_name>(jr, qname);
            ocs::tom::set<JR_failed>(jr, ESSTATE_UNKNOWN_JOB);
            ocs::tom::set<JR_state>(jr, JEXITING);
            ocs::tom::set<JR_err_str>(jr, MSG_JR_ERRSTR_EXECDDONTKNOWJOB);
         }
         unknown_tasks = true;
      }
   }

   // If there are no pe tasks running, we create the unknown job report for the master task.
   if (!unknown_tasks) {
      auto jr = get_job_report(jobid, jataskid, nullptr);
      if (!jr) {
         jr = add_job_report(jobid, jataskid, nullptr, nullptr);
      }
      if (jr) {
         ocs::tom::set<JR_queue_name>(jr, qname);
         ocs::tom::set<JR_failed>(jr, ESSTATE_UNKNOWN_JOB);
         ocs::tom::set<JR_state>(jr, JEXITING);
         ocs::tom::set<JR_err_str>(jr, MSG_JR_ERRSTR_EXECDDONTKNOWJOB);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Triggers the cleanup of the active jobs directory and triggers sync of job states with qmaster.
 *
 * @param startup true if execd is in startup phase
 * @param shepherd_pids pids of the shepherds that have been found
 * @return true if the cleanup was successful
 */
static bool
cleanup_jobs_and_states(bool startup, const std::vector<pid_t> &shepherd_pids) {
   DENTER(TOP_LAYER);

   // check if keep_active is set.
   bool keep_active_is_set = false;
   keep_active_t keep_active = mconf_get_keep_active();
   if (keep_active == KEEP_ACTIVE_TRUE || sge_getenv("SGE_KEEP_ACTIVE") || keep_active == KEEP_ACTIVE_ERROR) {
      keep_active_is_set = true;
   }

   // process all active job directories to sync state with qmaster or remove active job directories
   DIR *cwd = opendir(ACTIVE_DIR);
   if (cwd == nullptr) {
      ERROR(MSG_FILE_CANTOPENDIRECTORYX_SS, ACTIVE_DIR, strerror(errno));
      DRETURN(false);
   }
   SGE_STRUCT_DIRENT *dent;
   while ((dent = SGE_READDIR(cwd)) != nullptr) {
      char string[256];
      const char *job_directory = dent->d_name;
      strcpy(string, job_directory);

      // skip directories "." and ".."
      if (!strcmp(job_directory, ".") || !strcmp(job_directory, "..")) {
         continue;
      }

      // parse jid and tid from directory name (<jid>.<tid>)
      uint32_t jid = 0;
      uint32_t tid = 0;
      if (const char *token = strtok(string, " ."); token != nullptr) {
         char *end;
         uint32_t tmp_id = strtol(token, &end, 10);
         if (*end == '\0') {
            jid = tmp_id;
            token = strtok(nullptr, " \n\t");
            if (token != nullptr) {
               tmp_id = strtol(token, &end, 10);
               if (*end == '\0') {
                  tid = tmp_id;
               }
            }
         }
      }

      // skip directories that are not job directories (should not happen)
      // but we have to handle this case if someone has writen garbage to the active jobs directory
      if (!jid || !tid) {
         WARNING(MSG_SHEPHERD_XISNOTAJOBDIRECTORY_S, job_directory);
         continue;
      }

      // remove active job directory for jobs that are not in the job list
      // or sync state with qmaster for jobs depending on shepherd and job state
      // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
      ocs::tom::Element<ocs::tom::JB> jep_found;
      ocs::tom::Element<ocs::tom::JAT> ja_task_found;
      bool ignore_missing_job_task = true; // missing job and task are expected. no need for error logging
      bool found_job_and_task = execd_get_job_ja_task(jid, tid, &jep_found, &ja_task_found,
                                                      ignore_missing_job_task);
      if (!found_job_and_task) {

         // we have an active job directory for a job that is not in the job list
         // if keep_active is not set we remove the active job directory
         if (!keep_active_is_set) {
            if (startup) {
               ERROR(MSG_SHEPHERD_FOUNDACTIVEJOBDIRXWHILEMISSINGJOBDIRREMOVING_S, job_directory);
            }

            char path[SGE_PATH_MAX];
            snprintf(path, sizeof(path), ACTIVE_DIR"/%s", job_directory);
            sge_rmdir(path, nullptr);
            INFO("removed active jobs directory %s", path);
         }
      } else {
         // here we have a job and task that is still in the job list
         // check if shepherd is still running and handle state changes
         // by syncing state via job report with qmaster
         const ocs::tom::Element<ocs::tom::JB> jep(jep_found);
         const ocs::tom::ConstElement<ocs::tom::JAT> ja_task(ja_task_found);
         char dir[SGE_PATH_MAX];
         if (ocs::tom::get<JAT_status>(ja_task) != JSLAVE) {
            snprintf(dir, sizeof(dir), "%s/%s", ACTIVE_DIR, job_directory);
            examine_job_task_from_file(startup, dir, jep, ja_task, nullptr, shepherd_pids);
         }
         for (const auto pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
            snprintf(dir, sizeof(dir), "%s/%s/%s", ACTIVE_DIR, job_directory, ocs::tom::get<PET_id>(pe_task));
            examine_job_task_from_file(startup, dir, jep, ja_task, pe_task, shepherd_pids);
         }
      }
   }

   // close directory
   closedir(cwd);
   DRETURN(true);
}


/**
 * @brief Enforces the cleanup of old jobs.
 *
 * This function can be called to enforce the expensive handling of clean_up_old_jobs()
 * This is required when
 *    - the keep active setting changes
 *    - or when state changes (delete, reschedule) are triggered during downtime of the execution daemon
 */
static bool enforce_cleanup_old_jobs = true;
/** @brief Clean up jobs left behind by a previous daemon even if they look alive
 *
 * Set when the daemon is told to start fresh rather than adopt what it finds.
 */
void set_enforce_cleanup_old_jobs() {
   enforce_cleanup_old_jobs = true;
}

/**
 * @brief Shepherds that were already running when this execd started.
 *
 * They are not children of this process, so waitpid() in sge_reap_children_execd()
 * never sees them and their exit produces no SIGCHLD here. Nothing else notices
 * them either: the lost-SIGCHLD remedy in execd_signal_queue.cc:607 sits in the
 * signal delivery path, and a job that simply runs to its end is never signalled.
 *
 * Without a second mechanism such a job therefore stays in state "running" forever
 * once its shepherd has finished. Reproduced with "sgeexecd softstop" followed by a
 * restart -- which is the documented way to restart an execd without killing its
 * jobs, so this is a normal administrative operation, not a corner case.
 *
 * The startup reconciliation records what it found alive here, and
 * check_adopted_shepherds() polls it. That poll touches only these few processes
 * -- it does not bring back the per-minute `ps` over the host's whole process
 * table that CS-2532 removed.
 */
struct adopted_shepherd_t {
   pid_t pid;         ///< pid of the shepherd, as written to `active_jobs/<job>/pid`
   uint32_t job_id;   ///< the job it supervises -- part of the shepherd's process name

};
static std::vector<adopted_shepherd_t> adopted_shepherds;

/**
 * @brief Is this pid still the shepherd of that job?
 *
 * Asking it that precisely matters because pids get reused. "Does a process with
 * this pid exist" would also be answered yes by another cluster's shepherd, or by
 * any unrelated process that happened to get the number -- and a job whose
 * shepherd has long finished would never be reaped.
 *
 * On Linux the answer comes from /proc/<pid>/cmdline: its first entry is argv[0],
 * which the execd itself set to "sge_shepherd-<job_id>" before the exec
 * (exec_job.cc:2092, execlp(shepherd_path, ps_name, ...)). That ties the pid to
 * THIS job without forking anything.
 *
 * Elsewhere there is no equally cheap and portable lookup, so the weaker question
 * is asked via kill(pid, 0). That is not a regression: it is strictly more than
 * the code did before, where nothing looked at these processes at all.
 *
 * @param pid    pid to check
 * @param job_id job the pid is supposed to belong to
 * @return true if the process is still that job's shepherd
 */
static bool
adopted_shepherd_still_running(pid_t pid, uint32_t job_id) {
#if defined(LINUX)
   char path[SGE_PATH_MAX];
   snprintf(path, sizeof(path), "/proc/" pid_t_fmt "/cmdline", pid);

   FILE *fp = fopen(path, "r");
   if (fp == nullptr) {
      // Process gone -- or no /proc at all. Answering "gone" only marks the
      // reconciliation as due, and that one re-reads the process table and
      // decides. The fail-safe direction is therefore the one that checks.
      return false;
   }

   // argv is stored NUL separated, so the first entry ends at the first NUL and
   // a plain strcmp() against it compares exactly argv[0].
   char cmdline[SGE_PATH_MAX];
   size_t len = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
   fclose(fp);
   cmdline[len] = '\0';

   char expected[SGE_PATH_MAX];
   snprintf(expected, sizeof(expected), "%s-" sge_u32, SGE_SHEPHERD, job_id);
   return strcmp(cmdline, expected) == 0;
#else
   // ESRCH is the only answer that means "gone"; EPERM means it exists and is
   // owned by someone else. The job is not part of the question here -- keep the
   // parameter out of the unused warnings rather than out of the signature, so
   // both branches stay callable in the same way.
   (void) job_id;
   return !(kill(pid, 0) == -1 && errno == ESRCH);
#endif
}

static void
check_adopted_shepherds() {
   DENTER(TOP_LAYER);

   if (adopted_shepherds.empty()) {
      DRETURN_VOID;
   }

   auto gone = std::remove_if(adopted_shepherds.begin(), adopted_shepherds.end(),
                              [](const adopted_shepherd_t &shepherd) {
                                 return !adopted_shepherd_still_running(shepherd.pid, shepherd.job_id);
                              });
   if (gone != adopted_shepherds.end()) {
      adopted_shepherds.erase(gone, adopted_shepherds.end());

      // Deliberately not acting on the pid itself: only mark the reconciliation
      // as due -- it re-reads the process table and decides.
      set_enforce_cleanup_old_jobs();
   }

   DRETURN_VOID;
}

/**
 * @brief Cleans up old jobs and reports them to the qmaster.
 *
 * The work itself is a reconciliation between what is on disk and what the execd has
 * in memory:
 *
 * - look for old jobs hanging around in the active jobs directory on disk
 * - to check if the shepherd is still running for jobs
 * - to identify jobs that have exited without sending a SIGCLD to the execd (e.g. due to execd downtime)
 * - to report the updated job state to the qmaster
 *
 * All this needs only to be done once during the lifetime of the execd process and should be repeated
 * in situations where set_enforce_cleanup_old_jobs() is called. There are three of them:
 *
 * - a KEEP_ACTIVE change (execd_get_new_conf.cc:99). While KEEP_ACTIVE was on, finished jobs
 *   deliberately left their active job directory behind. Switching it off makes those directories
 *   garbage that nothing else will collect, so the reconciliation has to run once more --
 *   this is a second entry point besides startup, not a variant of it.
 * - state changes (delete, reschedule) that a job went through while the execd was down.
 * - a shepherd adopted at startup has ended (check_adopted_shepherds()). Nothing else can
 *   notice that: it is not our child, so there is no SIGCHLD.
 *
 * The function is nevertheless *called* once per OLD_JOB_INTERVAL (execd_ck_to_do.cc)
 * and returns immediately unless a cleanup is due. That periodic call is the poll that picks up
 * such a later set_enforce_cleanup_old_jobs() and that checks the adopted shepherds, so it has to
 * stay -- but it must not be mistaken for the cleanup itself running periodically. See CS-2532.
 *
 * During startup, the function produces more output.
 *
 * @param startup Indicates if execd is in the startup phase.
 * @return true if the cleanup was successful, false otherwise.
 */
bool
clean_up_old_jobs(bool startup) {
   DENTER(TOP_LAYER);

   // Simulated jobs have no shepherd and no active job directory -- there is nothing
   // to reconcile, whether or not the cleanup is due.
   if (mconf_get_simulate_jobs()) {
      DRETURN(true);
   }

   // Run only when the cleanup is actually due: at startup, and whenever
   // set_enforce_cleanup_old_jobs() marks it due again (a KEEP_ACTIVE change, or
   // state changes that happened while the execd was down). The flag is consumed
   // here, so a due cleanup runs exactly once.
   //
   // This used to fall through to the scan whenever the job list was NOT empty
   // (CS-2532), which inverted the intent: the scan was skipped on an idle execd
   // and performed once per OLD_JOB_INTERVAL on a busy one. Each pass forks a
   // shell and a `ps` over the host's entire process table, and with several
   // execds per host they all do it, every minute, counting each other's
   // shepherds -- measured at up to 392 on a host running 34 of them.
   //
   // Scanning the process table periodically is not needed for correctness, with
   // one exception that is handled separately below. A shepherd that exits is
   // reaped through SIGCHLD via sge_reap_children_execd() (waitpid, :141), and the
   // case of a lost SIGCHLD has its own remedy: execd_signal_queue.cc:607 sets
   // sge_sig_handler_dead_children when a signal cannot be delivered while the
   // active job directory is still there. Reconciling against `ps` is a startup
   // concern -- it answers "what survived my downtime", which is a question only
   // the startup (or an enforced re-check) has.
   //
   // The exception: both of those cover shepherds that are OUR children. A
   // shepherd adopted at startup is not, and nothing signals a job that simply
   // runs to its end, so neither mechanism ever fires for it. Polling those few
   // pids restores that -- see adopted_shepherds. It is skipped during startup
   // because the list is filled by the startup reconciliation itself.
   //
   // The job list is deliberately NOT consulted: at startup it may be empty while
   // active job directories from before the restart are still on disk, and those
   // are exactly what has to be cleaned up.
   if (!startup) {
      check_adopted_shepherds();

#if defined(OCS_WITH_SYSTEMD)
      // Safety net for the systemd slice of a tightly integrated job: it is normally
      // removed when qmaster acknowledges the job exit, and this catches the ones
      // where that never happened - the job ended while this execd was down, so
      // qmaster finished it on its own and there is no acknowledgement to come.
      //
      // Not at startup: the jobs found on disk are reported to qmaster first, and the
      // answer - an acknowledgement, or the request to delete the job - takes the
      // slice with it. Removing a slice here before that exchange has had a chance to
      // happen would take the regular path's work away from it for no gain.
      //
      // Reading one directory, so it rides along here rather than on a timer of its
      // own, and it sits outside the enforce_cleanup_old_jobs gate below because a
      // slice can be orphaned without anything marking the reconciliation as due.
      ocs::execd::execd_cleanup_stale_job_slices();
#endif
   }

   if (!enforce_cleanup_old_jobs) {
      DRETURN(true);
   }
   enforce_cleanup_old_jobs = false;

   if (startup) {
      INFO(SFNMAX, MSG_SHEPHERD_CKECKINGFOROLDJOBS);
   } else {
      INFO(SFNMAX, MSG_SHEPHERD_CKECKINGFOROLDJOBSAFTER);
   }

   // expensive operation: find all shepherd processes via ps command
   std::vector<pid_t> shepherd_pids;
   int number_of_shepherd = sge_get_pids(shepherd_pids, SGE_SHEPHERD, PSCMD);
   if (number_of_shepherd == -1) {
      ERROR(SFNMAX, MSG_SHEPHERD_CANTGETPROCESSESFROMPSCOMMAND);
      DRETURN(false);
   }
   INFO(MSG_EXECD_FOUND_N_SHEPHERD_PROCS_I, number_of_shepherd);

   // Check for old jobs. Either remove them or report them to the qmaster depending on the state.
   cleanup_jobs_and_states(startup, shepherd_pids);

   DRETURN(true);
}

/**
 * @brief Check a job or PE task found in the active jobs directory, and report it when its shepherd is gone
 *
 * @param startup whether the daemon is starting up
 * @param dir the active jobs directory of the job or PE task
 * @param jep the job
 * @param jatep the array task
 * @param petep the PE task, or an empty handle for the array task itself
 * @param pids the pids of the shepherds running on this host
 */
static void
examine_job_task_from_file(const int startup, char *dir, const ocs::tom::Element<ocs::tom::JB> jep,
                           const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                           const ocs::tom::ConstElement<ocs::tom::PET> petep, const std::vector<pid_t> &pids) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JR> jr;
   bool shepherd_alive;  /* true -> this shepherd is in the process table */
   FILE *fp = nullptr;
   SGE_STRUCT_STAT statbuf;
   char fname[SGE_PATH_MAX];
   pid_t pid;           /* pid of shepherd */
   char err_str[10000];
   uint32_t jobid, jataskid;
   const char *pe_task_id_str = nullptr;
   static uint64_t startup_time = sge_get_gmt64();

   jobid = ocs::tom::get<JB_job_number>(jep);
   jataskid = ocs::tom::get<JAT_task_number>(jatep);
   if (petep) {
      pe_task_id_str = ocs::tom::get<PET_id>(petep);
   }

   if (SGE_STAT(dir, &statbuf)) {
      ERROR(MSG_SHEPHERD_CANTSTATXY_SS, dir, strerror(errno));
      DRETURN_VOID;
   }

   if (!(statbuf.st_mode & S_IFDIR)) {
      ERROR(MSG_FILE_XISNOTADIRECTORY_S, dir);
      DRETURN_VOID;
   }

   DPRINTF("Found job directory: %s\n", dir);
   if (startup) {
      INFO(MSG_SHEPHERD_FOUNDDIROFJOBX_S, dir);
   }
   
   /* Look for pid of shepherd */
   snprintf(fname, sizeof(fname), "%s/pid", dir);
   if (!(fp = fopen(fname, "r"))) {
      /* Is it 
            1. a job started before startup of execd
               In this case the job was started by the old execd 
               and there must be a pid fild -> Logging
         or is it 
            2. a newly started job
               In this case the shepherd had not enough time 
               to write the pid file -> No Logging */
      if (startup && startup_time >= ocs::tom::get<JAT_start_time>(jatep)) {
         DSTRING_STATIC(dstr, 100);
         ERROR(MSG_SHEPHERD_CANTREADPIDFILEXFORJOBYSTARTTIMEZX_SSSS, fname, dir, sge_ctime64(ocs::tom::get<JAT_start_time>(jatep), &dstr), strerror(errno));
         /* seek job report for this job - it must be contained in job report
            If this is a newly started execd we can assume the execd was broken
            in the interval between making the jobs active directory and writing
            the shepherds pid (done by the started shepherd). So we just remove
            and report this job. */
         if (!(jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
            CRITICAL(MSG_SHEPHERD_MISSINGJOBXINJOBREPORTFOREXITINGJOB_U, jobid);
            jr = add_job_report(jobid, jataskid, pe_task_id_str, nullptr);
         }
         ocs::tom::set<JR_state>(jr, JEXITING);
         clean_up_job(jr, ESSTATE_NO_PID, 0, job_is_array(jep), jatep, ocs::tom::get<JB_owner>(jep));  /* failed before execution */
      }
      DRETURN_VOID;
   }
   
   if (fscanf(fp, pid_t_fmt, &pid) != 1) {
      /* most probably a newly started job
         shepherd usually just had not enough time for writing the pid file 
         if these warnings do appear frequently one might consider having
         execd (and thus the shepherds) do local spooling instead of via NFS */
      WARNING(MSG_SHEPHERD_CANTREADPIDFROMPIDFILEXFORJOBY_SS, fname, dir);
      FCLOSE_IGNORE_ERROR(fp);
      DRETURN_VOID;
   }
   FCLOSE_IGNORE_ERROR(fp);

   /* look whether shepherd is still alive */ 
   shepherd_alive = sge_contains_pid(pid, pids);

   /* report this information */
   if (shepherd_alive) {
      snprintf(err_str, sizeof(err_str), MSG_SHEPHERD_SHEPHERDFORJOBXHASPIDYANDISZALIVE_SP, dir, pid);
   } else {
      snprintf(err_str, sizeof(err_str), MSG_SHEPHERD_SHEPHERDFORJOBXHASPIDYANDISNOTALIVE_SP, dir, pid);
   }
   if (startup) {
      INFO(SFNMAX, err_str);
      modify_queue_limits_flag_for_job(component_get_qualified_hostname(), jep, true);
   } else {
      DPRINTF(err_str);
   }

   if (shepherd_alive) {     /* shepherd alive -> nothing to do */

      if (startup) {
         // A shepherd that is already running while we are still starting up cannot
         // be one of ours, so its exit will not reach us as a SIGCHLD. Remember it;
         // check_adopted_shepherds() takes it from here. Only the startup fills this
         // list -- on a later enforced run the alive shepherds are our own children.
         adopted_shepherds.push_back({pid, jobid});

         /*
            at startup we need to change the
            state of not exited jobs from JWRITTEN
            to JRUNNING
         */
         if ((jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
            ocs::tom::set<JR_state>(jr, JRUNNING);
         } else {
            /* found job in active jobs directory 
               but not in spool directory of execd */
            ERROR(MSG_SHEPHERD_INCONSISTENTDATAFORJOBX_U, jobid);
            jr = add_job_report(jobid, jataskid, pe_task_id_str, nullptr);
            ocs::tom::set<JR_state>(jr, JEXITING);
         }
      }
      DRETURN_VOID;
   }

   /* seek job report for this job - it must be contained in job report */
   if (!(jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
      CRITICAL(MSG_SHEPHERD_MISSINGJOBXYINJOBREPORT_UU, jobid, jataskid);
      jr = add_job_report(jobid, jataskid, pe_task_id_str, jep);
      DRETURN_VOID;
   }

   /* if the state is already JEXITING work is done  
      for this job and we wait for ACK from qmaster */
   if (ocs::tom::get<JR_state>(jr)==JEXITING) {
      DPRINTF("State of job " sge_u32 "." sge_u32" already changed to JEXITING\n", jobid, jataskid);
      DRETURN_VOID;
   }

   clean_up_job(jr, 0, 0, job_is_array(jep), jatep, ocs::tom::get<JB_owner>(jep));
   reaper_finish_job_scope(jr, jep, jatep, petep);
   ocs::tom::set<JR_state>(jr, JEXITING);
   

   flush_job_report(jr);

   DRETURN_VOID;
}

/**
 * @brief Fill the usage of a job report
 *
 * Takes the data retrieved from the config file and the data from the "usage" file as the
 * shepherd has written it. If "usage" cannot be read, exit_status is 0xffffffff.
 *
 * @param jr the job report
 * @param jobdir the active jobs directory of the job or PE task
 * @param jobid the job
 * @param jataskid the array task
 * @param pe_task_id the PE task, or nullptr
 * @param failed the failure state found so far, see shepherd_states.h
 * @return 0, or -1 if the usage file cannot be read or closed
 */
static int
read_dusage(const ocs::tom::Element<ocs::tom::JR> jr, const char *jobdir, const uint32_t jobid,
            const uint32_t jataskid, const char *pe_task_id, const int failed) {
   DENTER(TOP_LAYER);

   char pid_file[SGE_PATH_MAX];
   FILE *fp;
   uint32_t pid;

   pid = 0xffffffff;

   snprintf(pid_file, sizeof(pid_file), "%s/pid", jobdir);

   if (failed != ESSTATE_NO_PID) {
      fp = fopen(pid_file, "r");
      if (fp != nullptr) {
         if (fscanf(fp, sge_u32 , &pid) != 1) {
            ERROR(MSG_EXECD_ERRORREADINGPIDOFJOB_UU, jobid, jataskid);
         }
         FCLOSE(fp);
      } else {
         ERROR(MSG_SHEPHERD_CANTOPENPIDFILEXFORJOBYZ_SUU, pid_file, jobid, jataskid);
      }
   }

   if (failed != ESSTATE_NO_CONFIG) {
      dstring buffer = DSTRING_INIT;
      const char *qinstance_name = nullptr;
   
      qinstance_name = sge_dstring_sprintf(&buffer, SFN "@" SFN,
                                           get_conf_val("queue"),
                                           get_conf_val("host"));
      ocs::tom::set<JR_queue_name>(jr, qinstance_name);
      qinstance_name = nullptr;
      sge_dstring_free(&buffer);

      add_usage(jr, "submission_time", get_conf_val("submission_time"), (double)0);
      add_usage(jr, "priority", get_conf_val("priority"),        (double)0);
   }

   /* read "usage" file */
   /*
   ** there may be more failure states where there is no usage file
   ** but it is best if we try to read it and ignore the error if reading failed
   ** if we have an error already
   */
   if (!failed || (failed > SSTATE_BEFORE_JOB)) {
      char usage_file[SGE_PATH_MAX];
      snprintf(usage_file, sizeof(usage_file), "%s/usage", jobdir);
      fp = fopen(usage_file, "r");
      if (fp) {
         char buf[10000];
         uint32_t wait_status;

         // freed on every way out, also when FCLOSE() jumps to FCLOSE_ERROR
         const auto cflp = read_config_list(fp, nullptr, false, "=", 0, buf, sizeof(buf)).second;
         FCLOSE(fp);

         if (extract_ulong_attribute(*cflp, "wait_status", &wait_status)==0)
            ocs::tom::set<JR_wait_status>(jr, wait_status);

         convert_attribute(*cflp, jr, "exit_status",   1);
         convert_attribute(*cflp, jr, "signal",        0);
         convert_attribute(*cflp, jr, "start_time",    0);
         convert_attribute(*cflp, jr, "end_time",      0);

         convert_attribute(*cflp, jr, "ru_wallclock",  0);

         convert_attribute(*cflp, jr, "ru_utime",      0);
         convert_attribute(*cflp, jr, "ru_stime",      0);
         convert_attribute(*cflp, jr, "ru_maxrss",     0);
         convert_attribute(*cflp, jr, "ru_ixrss",      0);
         convert_attribute(*cflp, jr, "ru_ismrss",     0);
         convert_attribute(*cflp, jr, "ru_idrss",      0);
         convert_attribute(*cflp, jr, "ru_isrss",      0);
         convert_attribute(*cflp, jr, "ru_minflt",     0);
         convert_attribute(*cflp, jr, "ru_majflt",     0);
         convert_attribute(*cflp, jr, "ru_nswap",      0);
         convert_attribute(*cflp, jr, "ru_inblock",    0);
         convert_attribute(*cflp, jr, "ru_oublock",    0);
         convert_attribute(*cflp, jr, "ru_msgsnd",     0);
         convert_attribute(*cflp, jr, "ru_msgrcv",     0);
         convert_attribute(*cflp, jr, "ru_nsignals",   0);
         convert_attribute(*cflp, jr, "ru_nvcsw",      0);
         convert_attribute(*cflp, jr, "ru_nivcsw",     0);

         build_derived_final_usage(jr, jobid, jataskid, pe_task_id);

         // report whatever is remaining in the usage list. Each shepherd usage
         // line is classified via the CS-849 discrimination rule
         // (usage_parse_value): the resulting element carries either UA_value
         // (numeric) or UA_svalue (string) but never both. The factory-append
         // shape bypasses add_usage()'s numeric-only path entirely.
         for (const auto ep : *cflp) {
            const char *name = ocs::tom::get<CF_name>(ep);
            const char *value = ocs::tom::get<CF_value>(ep);
            DEBUG("additional usage variable %s = %s", name, value);
            auto parsed = usage_parse_value(name, value);
            if (!parsed) {
               continue;
            }
            const auto usage_list = jr.get_or_create<JR_usage>("usage");
            // replace any earlier entry with the same name (e.g. one added by
            // build_derived_final_usage) so the shepherd file wins
            const auto existing = usage_list.find_first<UA_name>(name);
            if (existing) {
               usage_list.erase(existing);
            }
            usage_list.push_back(std::move(parsed));
         }
      } else {
         ERROR(MSG_SHEPHERD_CANTOPENUSAGEFILEXFORJOBYZX_SUUS, usage_file, jobid, jataskid, strerror(errno));
         DRETURN(-1);
      }
   }


#if 0
   {
      lListElem *ep;

      if (lGetList(jr, JR_usage)) {
         DPRINTF("resulting usage attributes:\n");
      } else {
         DPRINTF("empty usage list\n");
      }   

      for_each_ep(ep, lGetList(jr, JR_usage)) {
         DPRINTF(("    \"%s\" = %f\n",
            lGetString(ep, UA_name),
            lGetDouble(ep, UA_value)));
      }
   }
#endif
   DRETURN(0);
FCLOSE_ERROR:
   ERROR(MSG_FILE_ERRORCLOSEINGXY_SS, "usage or pid", strerror(errno));
   DRETURN(-1);
}


/**
 * @brief Add the final usage derived from the usage the shepherd reported to a job report
 *
 * cpu, mem, io, maxvmem and maxrss, real or reserved as configured, both for the accounting and for the
 * sharetree; also the wallclock time.
 *
 * @param jr the job report
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr
 */
static void
build_derived_final_usage(const ocs::tom::Element<ocs::tom::JR> jr, const uint32_t job_id, const uint32_t ja_task_id,
                          const char *pe_task_id) {
   DENTER(TOP_LAYER);

   double ru_cpu, pdc_cpu;
   double cpu, r_cpu,
          mem, r_mem,
          io, ioops, iow, r_io, r_ioops, r_iow, maxvmem, r_maxvmem, maxrss, r_maxrss, maxpss = 0;

   // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
   ocs::tom::Element<ocs::tom::JB> job;
   ocs::tom::Element<ocs::tom::JAT> ja_task_found;
   ocs::tom::ConstElement<ocs::tom::PET> pe_task;

   bool accounting_summary = false;

   const ocs::tom::ConstList<ocs::tom::UA> usage_list = ocs::tom::get<JR_usage>(jr);

   bool have_mem_details = static_cast<bool>(usage_list.find_first<UA_name>(USAGE_ATTR_MAXPSS));

   /* cpu    = std::max(sum of "ru_utime" and "ru_stime" , PDC "cpu" usage) */
   ru_cpu = usage_list_get_double_usage(usage_list, "ru_utime", 0) +
            usage_list_get_double_usage(usage_list, "ru_stime", 0);
   pdc_cpu = usage_list_get_double_usage(usage_list, USAGE_ATTR_CPU, 0);
   cpu = std::max(ru_cpu, pdc_cpu);

   /* build reserved usage if required */ 
   r_cpu = r_mem = r_maxvmem = r_maxrss = 0.0;
   if (execd_get_job_ja_task(job_id, ja_task_id, &job, &ja_task_found, false)) {
      const ocs::tom::ConstElement<ocs::tom::JAT> ja_task(ja_task_found);
      if (pe_task_id != nullptr) {
         pe_task = ocs::tom::find_first_in<JAT_task_list, PET_id>(ja_task, pe_task_id);
      }

      double wallclock;
      if (mconf_get_acct_reserved_usage() || mconf_get_sharetree_reserved_usage()) {
         // reserved usage
         uint64_t end_time = usage_list_get_ulong64_usage(usage_list, "end_time", 0);
         const auto pe = ocs::tom::get<JAT_pe_object>(ja_task);
         if (pe != nullptr && ocs::tom::get<PE_accounting_summary>(pe)) {
            accounting_summary = true;
         }

         build_reserved_usage(end_time, ja_task, pe_task, &wallclock, &r_cpu, &r_mem, &r_maxvmem, &r_maxrss);
      } else {
         // non-reserved usage, need to calculate wallclock
         uint64_t start_time;
         if (pe_task == nullptr) {
            start_time = ocs::tom::get<JAT_start_time>(ja_task);
         } else {
            start_time = ocs::tom::get<PET_start_time>(pe_task);
         }
         wallclock = sge_gmt64_to_gmt32_double(sge_get_gmt64() - start_time);
      }
      add_usage(jr, USAGE_ATTR_WALLCLOCK, nullptr, wallclock);
   }

   /* mem    = PDC "mem" usage or zero */
   mem = usage_list_get_double_usage(usage_list, USAGE_ATTR_MEM, 0);

   /* io     = PDC "io" usage or zero */
   io = usage_list_get_double_usage(usage_list, USAGE_ATTR_IO, 0);
   /* ioops  = PDC "io operations" or zero */
   ioops = usage_list_get_double_usage(usage_list, USAGE_ATTR_IOOPS, 0);
   /* iow    = PDC "io wait time" or zero */
   iow = usage_list_get_double_usage(usage_list, USAGE_ATTR_IOW, 0);

   r_io = io;
   r_ioops = ioops;
   r_iow = iow;

   /* maxvmem and maxrss*/
   maxvmem = usage_list_get_double_usage(usage_list, USAGE_ATTR_MAXVMEM, 0);
   maxrss = usage_list_get_double_usage(usage_list, USAGE_ATTR_MAXRSS, 0);
   if (have_mem_details) {
      maxpss = usage_list_get_double_usage(usage_list, USAGE_ATTR_MAXPSS, 0);
   }

   DPRINTF("CPU/MEM/IO: M(%f/%f/%f) R(%f/%f/%f) acct: %s stree: %s\n", cpu, mem, io, r_cpu, r_mem, r_io,
           mconf_get_acct_reserved_usage()?"R":"M", mconf_get_sharetree_reserved_usage()?"R":"M");

   /* Report reserved usage or real usage.
    * If accounting_summary is activated for a tightly integrated
    * parallel job, we do not send any usage for the pe tasks.
    */
   if (mconf_get_acct_reserved_usage()) {
      if (!(accounting_summary && pe_task_id != nullptr)) {
         add_usage(jr, USAGE_ATTR_CPU_ACCT, nullptr, r_cpu);
         add_usage(jr, USAGE_ATTR_MEM_ACCT, nullptr, r_mem);
         add_usage(jr, USAGE_ATTR_IO_ACCT,  nullptr, r_io);
         add_usage(jr, USAGE_ATTR_IOOPS_ACCT,  nullptr, r_ioops);
         add_usage(jr, USAGE_ATTR_IOW_ACCT, nullptr, r_iow);
         if (r_maxvmem != DBL_MAX) {
            add_usage(jr, USAGE_ATTR_MAXVMEM_ACCT, nullptr, r_maxvmem);
         }
         if (r_maxrss != DBL_MAX) {
            add_usage(jr, USAGE_ATTR_MAXRSS_ACCT, nullptr, r_maxrss);
         }
      }
   } else {
      add_usage(jr, USAGE_ATTR_CPU_ACCT, nullptr, cpu);
      add_usage(jr, USAGE_ATTR_MEM_ACCT, nullptr, mem);
      add_usage(jr, USAGE_ATTR_IO_ACCT,  nullptr, io);
      add_usage(jr, USAGE_ATTR_IOOPS_ACCT,  nullptr, ioops);
      add_usage(jr, USAGE_ATTR_IOW_ACCT, nullptr, iow);
      if (maxvmem != DBL_MAX) {
         add_usage(jr, USAGE_ATTR_MAXVMEM_ACCT, nullptr, maxvmem);
      }
      if (maxrss != DBL_MAX) {
         add_usage(jr, USAGE_ATTR_MAXRSS_ACCT, nullptr, maxrss);
      }
      if (have_mem_details && maxpss != DBL_MAX) {
         add_usage(jr, USAGE_ATTR_MAXPSS_ACCT, nullptr, maxpss);
      }
   }

   /* We might have switched off sending of pe task job reports when starting the task
    * with accounting_summary & sharetree_reserved_usage - see execd_job_exec.cc. The
    * final report has to go out in any case: it is not only carrying usage, it is the
    * JEXITING notification. Without it sge_qmaster never learns that the pe task ended,
    * never acknowledges it, and the task is never removed from JAT_task_list. The
    * acknowledgement of the master task is deferred while pe tasks are left (see
    * remove_acked_job_exit() above), so the whole teardown of the job stalls - the
    * execd retries the acknowledgement once a second forever, the job stays in the
    * execd's job list, and with systemd the job slice is never stopped.
    *
    * This used to be done only when acct_reserved_usage was off, i.e. only when *real*
    * usage was wanted, which left the report suppressed for exactly the combination
    * sharetree_reserved_usage + acct_reserved_usage + accounting_summary.
    */
   if (pe_task_id != nullptr && ocs::tom::get<JR_no_send>(jr)) {
      ocs::tom::set<JR_no_send>(jr, false);
   }

   /* Report reserved usage or real usage.
    * If accounting_summary is activated for a tightly integrated
    * parallel job, we do not send any usage for the pe tasks.
    */
   if (mconf_get_sharetree_reserved_usage()) {
      if (!(accounting_summary && pe_task_id != nullptr)) {
         add_usage(jr, USAGE_ATTR_CPU, nullptr, r_cpu);
         add_usage(jr, USAGE_ATTR_MEM, nullptr, r_mem);
         add_usage(jr, USAGE_ATTR_IO,  nullptr, r_io);
         add_usage(jr, USAGE_ATTR_IOOPS,  nullptr, r_ioops);
         add_usage(jr, USAGE_ATTR_IOW, nullptr, r_iow);
         if (r_maxvmem != DBL_MAX) {
            add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, r_maxvmem);
         }
         if (r_maxrss != DBL_MAX) {
            add_usage(jr, USAGE_ATTR_MAXRSS, nullptr, r_maxrss);
         }
      }
   } else {
      // @todo What sense does this code make? We fetched the values from the jr->usage list and here write them back?
      add_usage(jr, USAGE_ATTR_CPU, nullptr, cpu);
      add_usage(jr, USAGE_ATTR_MEM, nullptr, mem);
      add_usage(jr, USAGE_ATTR_IO,  nullptr, io);
      add_usage(jr, USAGE_ATTR_IOOPS,  nullptr, ioops);
      add_usage(jr, USAGE_ATTR_IOW, nullptr, iow);
      if (maxvmem != DBL_MAX) {
         add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, maxvmem);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Move a value of the usage file into the usage of a job report
 *
 * The entry is taken out of the usage file's list, so that what is left is reported as additional usage.
 *
 * @param cflp the entries of the usage file, or an empty handle
 * @param jr the job report
 * @param name the name of the value
 * @param udefault the value to report if the usage file has none
 */
static void
convert_attribute(const ocs::tom::List<ocs::tom::CF> cflp, const ocs::tom::Element<ocs::tom::JR> jr, const char *name,
                  const uint32_t udefault) {
   DENTER(TOP_LAYER);

   const char *s;

   s = get_conf_value(nullptr, cflp, name);
   add_usage(jr, name, s, (double)udefault);
   // an empty list stays rather than being freed as before: nothing reads it as "no entries"
   if (cflp) {
      cflp.erase<CF_name>(name);
   }

   DRETURN_VOID;
}

/**
 * @brief Take an unsigned value out of the usage file
 *
 * @param cflp the entries of the usage file, or an empty handle
 * @param name the name of the value
 * @param valuep where the value is stored
 * @return 0, or -1 if the usage file has no such value or it is not a number
 */
static int
extract_ulong_attribute(const ocs::tom::List<ocs::tom::CF> cflp, const char *name, uint32_t *valuep) {
   DENTER(TOP_LAYER);

   const char *s;
   int ret;

   if (!(s = get_conf_value(nullptr, cflp, name))) {
      DRETURN(-1);
   }
   ret = sscanf(s, sge_u32, valuep);
   if (cflp) {
      cflp.erase<CF_name>(name);
   }
   DRETURN((ret == 1)?0:-1);
}


/* send mail to users if requested */
/** @brief Mail the user about a job that has ended, if they asked to be told
 * @param jep the job
 * @param jr the job report, for the usage and exit status
 */
void
reaper_sendmail(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   uint32_t mail_options;
   char sge_mail_subj[1024];
   char sge_mail_body[10*2048];
   char sge_mail_start[128];
   char sge_mail_end[128];
   uint32_t jobid, taskid, failed;
   double ru_utime, ru_stime, ru_wallclock;
   double ru_cpu = 0.0, ru_maxvmem = 0.0;
   int exit_status = -1, signo = -1;
   const char *q, *h, *u;
   ocs::tom::ConstElement<ocs::tom::UA> ep;
   const char *pe_task_id_str;
   dstring ds;
   char buffer[128];
   dstring cpu_string = DSTRING_INIT;
   dstring maxvmem_string = DSTRING_INIT;
   const char *qualified_hostname = component_get_qualified_hostname();

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   ocs::tom::ConstList<ocs::tom::MR> mail_users = ocs::tom::get<JB_mail_list>(jep);
   mail_options = ocs::tom::get<JB_mail_options>(jep); 
   pe_task_id_str = ocs::tom::get<JR_pe_task_id_str>(jr);

   if (!(q=ocs::tom::get<JR_queue_name>(jr))) {
      q = MSG_MAIL_UNKNOWN_NAME;
   }

   h = qualified_hostname;

   if (!(u=ocs::tom::get<JB_owner>(jep)))
      u = MSG_MAIL_UNKNOWN_NAME;

   /* JG: TODO (397): Extend usage module: usage_list_get_ctime_usage() 
    *                 and use the other usage_list_get functions.
    */

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "start_time")))
      strcpy(sge_mail_start, sge_ctime64(ocs::tom::get<UA_value>(ep), &ds));
   else   
      strcpy(sge_mail_start, MSG_MAIL_UNKNOWN_NAME);

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "end_time")))
      strcpy(sge_mail_end, sge_ctime64(ocs::tom::get<UA_value>(ep), &ds));
   else   
      strcpy(sge_mail_end, MSG_MAIL_UNKNOWN_NAME);

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "ru_utime")))
      ru_utime = ocs::tom::get<UA_value>(ep); 
   else
      ru_utime = 0;

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "ru_stime")))
      ru_stime = ocs::tom::get<UA_value>(ep); 
   else
      ru_stime = 0;

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "ru_wallclock")))
      ru_wallclock = ocs::tom::get<UA_value>(ep); 
   else
      ru_wallclock = 0;

   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, USAGE_ATTR_CPU_ACCT)))
      ru_cpu = ocs::tom::get<UA_value>(ep);
   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, USAGE_ATTR_MAXVMEM_ACCT)))
      ru_maxvmem = ocs::tom::get<UA_value>(ep);

   jobid = ocs::tom::get<JR_job_number>(jr);
   taskid = ocs::tom::get<JR_ja_task_number>(jr);

   failed = ocs::tom::get<JR_failed>(jr);
   
   if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "exit_status")))
      exit_status = (int)ocs::tom::get<UA_value>(ep);
   
   double_print_time_to_dstring(ru_cpu, &cpu_string, true);
   double_print_memory_to_dstring(ru_maxvmem, &maxvmem_string);

	/* send job exit mail only for master task */ 
   if ((VALID(MAIL_AT_EXIT, mail_options)) && !failed && !pe_task_id_str) {
      dstring utime_string = DSTRING_INIT;
      dstring stime_string = DSTRING_INIT;
      dstring wtime_string = DSTRING_INIT;

      DPRINTF("mail VALID at EXIT\n");
      double_print_time_to_dstring(ru_utime, &utime_string, true);
      double_print_time_to_dstring(ru_stime, &stime_string, true);
      double_print_time_to_dstring(ru_wallclock, &wtime_string, true);
      if (job_is_array(jep)) {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_JA_TASK_COMP_UUS,
                  jobid, taskid, ocs::tom::get<JB_job_name>(jep));
         snprintf(sge_mail_body, sizeof(sge_mail_body), MSG_MAIL_BODY_COMP_SSSSSSSSSSSI, sge_mail_subj, u, q, h,
                 sge_mail_start, sge_mail_end, sge_dstring_get_string(&utime_string),
                 sge_dstring_get_string(&stime_string), sge_dstring_get_string(&wtime_string),
                 (ru_cpu     == 0.0) ? "NA":sge_dstring_get_string(&cpu_string),
                 (ru_maxvmem == 0.0) ? "NA":sge_dstring_get_string(&maxvmem_string),
                 exit_status);
      } else {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_JOB_COMP_US,
                  jobid, ocs::tom::get<JB_job_name>(jep));
         snprintf(sge_mail_body, sizeof(sge_mail_body), MSG_MAIL_BODY_COMP_SSSSSSSSSSSI, sge_mail_subj, u, q, h,
                  sge_mail_start, sge_mail_end, sge_dstring_get_string(&utime_string),
                  sge_dstring_get_string(&stime_string), sge_dstring_get_string(&wtime_string),
                  (ru_cpu     == 0.0) ? "NA":sge_dstring_get_string(&cpu_string),
                  (ru_maxvmem == 0.0) ? "NA":sge_dstring_get_string(&maxvmem_string),
                  exit_status);
      }

      cull_mail(EXECD, mail_users, sge_mail_subj, sge_mail_body, MSG_MAIL_TYPE_COMP);
      sge_dstring_free(&utime_string);
      sge_dstring_free(&stime_string);
      sge_dstring_free(&wtime_string);
   }

   if (((VALID(MAIL_AT_ABORT, mail_options)) 
       || (VALID(MAIL_AT_EXIT, mail_options))) && 
       (failed || ocs::tom::get<JR_general_failure>(jr)==GFSTATE_JOB)) {
      char exitstr[256];
      const char *err_str;
      const char *action, *comment = "";

      if (failed==SSTATE_MIGRATE) {
         action = MSG_MAIL_ACTION_MIGR;
      } else if (failed==SSTATE_AGAIN) {
         action = MSG_MAIL_ACTION_RESCH;
      } else if (failed==SSTATE_APPERROR) {
         action = MSG_MAIL_ACTION_APPERROR;
      } else if (ocs::tom::get<JR_general_failure>(jr)==GFSTATE_JOB) {
         action = MSG_MAIL_ACTION_ERR;
         comment = MSG_MAIL_ACTION_ERR_COMMENT;
      } else {
         action = MSG_MAIL_ACTION_ABORT;
      }

      if ((ep=ocs::tom::find_first_in<JR_usage, UA_name>(jr, "signal")))
         signo = (uint32_t)ocs::tom::get<UA_value>(ep);

      if (!(err_str=ocs::tom::get<JR_err_str>(jr)))
         err_str = MSG_UNKNOWNREASON;

      DPRINTF("MAIL VALID at ABORT\n");
      snprintf(exitstr, sizeof(exitstr), "%d", exit_status);
      if (pe_task_id_str == nullptr) {
         if (job_is_array(jep)) {
            snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_JA_TASK_STATE_UUSS, jobid,
                     taskid, ocs::tom::get<JB_job_name>(jep), action);
            snprintf(sge_mail_body, sizeof(sge_mail_body), MSG_MAIL_BODY_STATE_SSSSSSSSSSSSS, sge_mail_subj, exitstr,
                     sge_sig2str(signo), u, q, h, sge_mail_start, sge_mail_end,
                     (ru_cpu     == 0.0) ? "NA":sge_dstring_get_string(&cpu_string),
                     (ru_maxvmem == 0.0) ? "NA":sge_dstring_get_string(&maxvmem_string),
                     get_sstate_description(failed), err_str, comment);
         } else {
            snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_JOB_STATE_USS,
                     jobid, ocs::tom::get<JB_job_name>(jep), action);
            snprintf(sge_mail_body, sizeof(sge_mail_body), MSG_MAIL_BODY_STATE_SSSSSSSSSSSSS, sge_mail_subj, exitstr,
                     sge_sig2str(signo), u, q, h, sge_mail_start, sge_mail_end,
                     (ru_cpu     == 0.0) ? "NA":sge_dstring_get_string(&cpu_string),
                     (ru_maxvmem == 0.0) ? "NA":sge_dstring_get_string(&maxvmem_string),
                     get_sstate_description(failed), err_str, comment);
         }
         cull_mail(EXECD, mail_users, sge_mail_subj, sge_mail_body, MSG_MAIL_TYPE_STATE);
      }
   }

   sge_dstring_free(&cpu_string);
   sge_dstring_free(&maxvmem_string);
   DRETURN();
}

/**
 * @brief Make pe slave job report exit
 *
 * When the master task of a tightly integrated pe job exited,
 * qmaster sends a request to all slave execds to report
 * the exit of the slave job container once all pe tasks
 * have finished.
 * This function sets the slave job to status JEXITING,
 * if all pe tasks already exited, it triggers sending
 * of the final slave job report.
 * The ACK_SIGNAL_SLAVE is repeated by sge_qmaster until all
 * slave tasks have finished and sge_execd sent the final
 * report for this job.
 *
 * @param job_id job id of the slave job
 * @param ja_task_id ja_task id of the slave job
 *
 * @note MT-NOTE: execd_slave_job_exit() is MT safe
 */
void execd_slave_job_exit(const uint32_t job_id, const uint32_t ja_task_id) {
   DENTER(TOP_LAYER);

   // out parameters of execd_get_job_ja_task(); the objects belong to the execd's job list
   ocs::tom::Element<ocs::tom::JB> job;
   ocs::tom::Element<ocs::tom::JAT> ja_task_found;

   if (execd_get_job_ja_task(job_id, ja_task_id, &job, &ja_task_found, false)) {
      const ocs::tom::ConstElement<ocs::tom::JAT> ja_task(ja_task_found);

      /* kill possibly still running tasks */
      signal_job(job_id, ja_task_id, SGE_SIGKILL);

      /* if there are no pe_tasks running
       * we can finish the slave job right away
       */
      if (ocs::tom::get<JAT_task_list>(ja_task).empty()) {
         const auto jr = get_job_report(job_id, ja_task_id, nullptr);

         // qmaster only has to see *an* exit status here to know that this host is
         // done with the job - the value itself carries no information for a slave
         // container, whose job report has none of its own.
         //
         // The master host gets this request as well: ack_all_slaves() walks the
         // whole gdil and the master host is one of its entries. There the report is
         // the master task's own and already carries the status the job ended with,
         // so overwriting it would replace it with a zero and make the accounting
         // record of a failed job indistinguishable from a successful one (CS-2632).
         if (jr && !ocs::tom::find_first_in<JR_usage, UA_name>(jr, "exit_status")) {
            add_usage(jr, "exit_status", nullptr, 0);
         }
         flush_job_report(jr);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Releases the resources used by a job for core binding
 *
 * Checks which cores the job was bound to and releases them (i.e.
 * the as used marked cores where marked as free so that other jobs
 * can be bound to any of these cores).
 *
 * @param binding Pointer to the "binding" line out of the config file.
 *
 * @note static void - void
 *
 * @note MT-NOTE: clean_up_binding() is not MT safe
 */
static void clean_up_binding(char *binding) {
   DENTER(TOP_LAYER);

   if (binding == nullptr || strcasecmp("nullptr", binding) == 0
      || strcasecmp("no_job_binding", binding) == 0) {
      /* no binding was instructed */
      DRETURN_VOID;
   }
   
#if defined(BINDING_SOLARIS)
   if (strstr(binding, "psrset:") != nullptr) {
      /* we are on Solaris and a processor set was created -> deaccount it and delete it */
      int processor_set_id = -1;

      /* check if just the enviroment variable SGE_BINDING was set 
         or the pe_hostfile was written */
      if ((strstr(binding,"env_") != nullptr) || (strstr(binding,"pe_") != nullptr)) {

         char* topo;
         /* no processor set was created */
         DPRINTF("Environment variable or pe_hostfile was set for binding");
         /* do not delete processor set (because it was not created) 
            but free resources */
         if ((sge_strtok(binding, ":") != nullptr)
             && (sge_strtok(nullptr, ":") != nullptr)
             && (topo = sge_strtok(nullptr, ":")) != nullptr) {
            
             /* update the string which represents the currently used cores 
                on execution daemon host */
             free_topology(topo, -1);
         }

      } else if (sge_strtok(binding, ":") != nullptr) {
         /* parse the psrset number right after "psrset:" */
         /* parse the rest of the line */
         char* pset_id;
         /* topology used by job */ 
         char* topo; 

         if ((pset_id = sge_strtok(nullptr, ":")) != nullptr) {
            /* finally get the processor set id */
            processor_set_id = atoi(pset_id);
            /* check if a processor set was created (is not the case 
               when the job was using ALL available cores -> no 
               processor set can be created because it is not allowed 
               from OS) */
            if (processor_set_id != -1) {

               /* must be root in order to delete processor set */
               sge_switch2start_user();
            
               if (pset_destroy((psetid_t)processor_set_id) != 0) {
                  /* couldn't delete pset */
                  INFO("Couldn't delete processor set");
               }

               sge_switch2admin_user();

               /* release the resources used by the job */
               if ((topo = sge_strtok(nullptr, ":")) != nullptr) {
                  /* update the string which represents the currently used cores 
                     on execution daemon host */
                  free_topology(topo, -1);
               } else {
                  WARNING("No resource string found in config entry binding");
               }
            } else {
               /* processor set id was -1 -> we don't have to delete it */
               DPRINTF("No processor set was generated for this job!");
            }
         }
      }

   }
#endif

#if defined(OCS_HWLOC)
   /* on Linux the used topology can be found just after the last ":" */
   /* -> find the used topology and release it */
   
   if (strstr(binding, ":") != nullptr) {
      /* there was an order from execd to shepherd for binding */
      /* seach the string after the last ":" -> this 
         is the topology used by the job */
      char* topo = nullptr;
      topo = strrchr(binding, ':');
      free_topology(++topo, -1);
      INFO("topology used by job freed");
   } else {
      /* couldn't find valid topology string in config file */
      WARNING("No resource string found in config entry binding");
   }

#endif

   DRETURN_VOID;
}

/**
 * @brief Counts the number of master tasks present for a job
 *
 * Counts the number of master tasks for a specific job that are present
 * on the execd.
 *
 * @param lp the job list
 * @param job_id Job number to count master tasks for
 *
 * @return Number of master tasks found
 *
 * @note MT-NOTE: count_master_tasks() is MT safe
 */
int count_master_tasks(const ocs::tom::ConstList<ocs::tom::JB> lp, const uint32_t job_id) {
   DENTER(TOP_LAYER);

   int master_jobs = 0;

   for (const auto jep : lp.find_all<JB_job_number>(job_id)) {
      for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         if (ocs::tom::get<JAT_status>(jatep) != JSLAVE) {
            master_jobs++;
         }
      }
   }

   DPRINTF("Found %d master jobs for " sge_u32, master_jobs, job_id);

   DRETURN(master_jobs);
}

/** @brief End a job that is only being simulated
 *
 * Simulated hosts run no shepherd, so their jobs have to be finished by hand.
 *
 * @param jep the job
 * @param jatep the array task
 * @param sig the signal to report the job as having died from, 0 for a normal exit
 */
void simulated_job_exit(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::Element<ocs::tom::JAT> jatep,
                        const uint32_t sig) {
   DENTER(TOP_LAYER);

   uint32_t jobid = ocs::tom::get<JB_job_number>(jep);
   uint32_t jataskid = ocs::tom::get<JAT_task_number>(jatep);

   uint32_t state = ocs::tom::get<JAT_status>(jatep);
   if (state == JEXITING) {
      DPRINTF("Simulated job " sge_u32"." sge_u32" is already in state JEXITING\n", jobid, jataskid);
   } else {
      DPRINTF("Simulated job " sge_u32"." sge_u32" is %s\n", jobid, jataskid, sig == 0 ? "exiting" : "killed");

      // the job reports belong to job_report_execd.cc
      auto jr = get_job_report(jobid, jataskid, nullptr);
      if (!jr) {
         ERROR(MSG_JOB_MISSINGJOBXYINJOBREPORTFOREXITINGJOBADDINGIT_UU, jobid, jataskid);
         jr = add_job_report(jobid, jataskid, nullptr, jep);
      }

      ocs::tom::set<JR_state>(jr, JEXITING);
      add_usage(jr, "submission_time", nullptr, ocs::tom::get<JB_submission_time>(jep));

      uint64_t start_time = ocs::tom::get<JAT_start_time>(jatep);
      add_usage(jr, "start_time", nullptr, start_time);

      // get the current time as end time - it shall contain overhead in sge_execd
      uint64_t end_time = sge_get_gmt64();
      ocs::tom::set<JAT_end_time>(jatep, end_time);
      add_usage(jr, "end_time", nullptr, end_time);

      double wallclock = sge_gmt64_to_gmt32_double(end_time - start_time);
      add_usage(jr, "ru_wallclock", nullptr, wallclock);
      add_usage(jr, USAGE_ATTR_CPU_ACCT, nullptr, wallclock * 0.5);
      add_usage(jr, "ru_utime", nullptr, wallclock * 0.4);
      add_usage(jr, "ru_stime", nullptr, wallclock * 0.1);

      if (sig > 0) {
         add_usage(jr, "exit_status", nullptr, 137);
         add_usage(jr, "signal", nullptr, sig);
      } else {
         add_usage(jr, "exit_status", nullptr, 0);
      }

      ocs::tom::set<JAT_status>(jatep, JEXITING);
      flush_job_report(jr);
   }


   DRETURN_VOID;
}
