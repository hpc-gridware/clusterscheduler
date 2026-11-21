
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
 * @brief The priority translation facility: turning tickets into nice values
 *
 * @see ptf.h for how the feedback loop works.
 */

#if defined(SOLARIS)
#  include <sys/param.h>
#endif

#if defined(COMPILE_DC)

/** @brief Take usage from the process data collector rather than from wait3()
 *
 * Set on every platform where the collector runs; without it the PTF sees only
 * what the shepherd reports when a job ends, which is too late to adjust
 * anything.
 */
#define USE_DC

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <utility>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
#  include <sys/resource.h>
#endif


#include "comm/commlib.h"

#include "uti/ocs_Systemd.h"
#include "uti/sge_language.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_usage.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JL.h"
#include "sgeobj/tom/ocs_tom_JO.h"
#include "sgeobj/tom/ocs_tom_JP.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "ocs_common_systemd.h"
#include "ptf.h"

#include "execd.h"

#include <cinttypes>
#include "msg_execd.h"
#include "sgedefs.h"
#include "exec_ifm.h"
#include "pdc.h"
#include "ocs_execd_systemd.h"

/*
 *
 * PTF Data Structures
 *
 *    job_ticket_list     list of jobs and their associated tickets which is
 *                        sent from the SGE scheduler to the PTF.
 *
 *    job_ticket_entry    contains the job ID and job tickets for each job.
 *
 *    job_list            local list of jobs and all the associated attributes
 *
 *    job                 entry contained in the job list.
 *
 *
 * Notes:
 *
 *   Make sure JL_usage is always set to at least PTF_MIN_JOB_USAGE
 *
 *   Where do we create the usage list?  Probably in the routine that
 *   we get the usage from the Data Collector.
 *
 *   When does usage get reset to PTF_MIN_JOB_USAGE?  Possibly, upon
 *   receiving a new job tickets list.
 *
 *   When do we update the usage in the qmaster?  Possibly, after
 *   receiving a new job tickets list.
 *
 *   Make sure we don't delete a job from the job list until all the
 *   usage has been collected and reported back to the qmaster.
 *
 */

/** @brief Advance a pointer by a byte count, whatever it points at
 *
 * The collector hands over a job followed by a variable number of processes in
 * one block, so walking it means stepping by bytes.
 *
 * @param type the pointer's target type
 * @param ptr the pointer, advanced in place
 * @param nbyte how far to advance it
 */
#define INCPTR(type, ptr, nbyte) ptr = (type *)((char *)ptr + nbyte)

/** @brief #INCPTR for a job pointer
 * @param ptr the pointer, advanced in place
 * @param nbyte how far to advance it
 */
#define INCJOBPTR(ptr, nbyte) INCPTR(struct psJob_s, ptr, nbyte)

/** @brief #INCPTR for a process pointer
 * @param ptr the pointer, advanced in place
 * @param nbyte how far to advance it
 */
#define INCPROCPTR(ptr, nbyte) INCPTR(struct psProc_s, ptr, nbyte)

static void ptf_calc_job_proportion_pass0(ocs::tom::ConstElement<ocs::tom::JL> job, u_long *sum_of_job_tickets,
                                          double *sum_of_last_usage);

static void ptf_calc_job_proportion_pass1(ocs::tom::Element<ocs::tom::JL> job, u_long sum_of_job_tickets,
                                          double sum_of_last_usage, double *sum_proportion);

static void ptf_calc_job_proportion_pass2(ocs::tom::Element<ocs::tom::JL> job, u_long sum_of_job_tickets,
                                          double sum_proportion, double *sum_adjusted_proportion,
                                          double *sum_last_interval_usage);

static void ptf_calc_job_proportion_pass3(ocs::tom::Element<ocs::tom::JL> job, double sum_adjusted_proportion,
                                          double sum_last_interval_usage, double *min_share, double *max_share,
                                          double *max_ticket_share);

static void ptf_set_OS_scheduling_parameters(ocs::tom::List<ocs::tom::JL> job_list, double min_share,
                                             double max_share, double max_ticket_share);

static void ptf_get_usage_from_data_collector();

static ocs::tom::Element<ocs::tom::JL>
ptf_process_job(osjobid_t os_job_id, const char *task_id_str, ocs::tom::ConstElement<ocs::tom::JB> new_job,
                uint32_t jataskid, const char *systemd_scope, usage_collection_t usage_collection);

static ocs::tom::Element<ocs::tom::JO>
ptf_get_job_osjob_by_osjobid(ocs::tom::List<ocs::tom::JL> job_list, osjobid_t os_job_id,
                             ocs::tom::Element<ocs::tom::JL> *job_elem);

static ocs::tom::Element<ocs::tom::JO>
ptf_get_osjob_by_ids(ocs::tom::List<ocs::tom::JO> osjoblist, uint32_t ja_task_id, const char *pe_task_id);

static void ptf_set_job_priority(ocs::tom::ConstElement<ocs::tom::JL> job);

static ocs::tom::ListPtr<ocs::tom::UA>
_ptf_get_job_usage(ocs::tom::ConstElement<ocs::tom::JL> job, u_long ja_task_id, const char *task_id);

static osjobid_t ptf_get_osjobid(ocs::tom::ConstElement<ocs::tom::JO> osjob);


static void ptf_set_osjobid(ocs::tom::Element<ocs::tom::JO> osjob, osjobid_t osjobid);

static void ptf_set_native_job_priority(ocs::tom::ConstElement<ocs::tom::JL> job,
                                        ocs::tom::ConstElement<ocs::tom::JO> osjob, long pri);

static ocs::tom::Element<ocs::tom::JL> ptf_get_job(u_long job_id);

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

static void ptf_setpriority_addgrpid(ocs::tom::ConstElement<ocs::tom::JL> job,
                                     ocs::tom::ConstElement<ocs::tom::JO> osjob, long pri);

#endif

/**
 * @brief Every job the PTF is currently adjusting (`JL`)
 *
 * Stays a raw list: ocs_execd_systemd.cc declares it `extern lList *` and walks it. ptf.cc creates and frees it
 * and reads it through checked non-owning handles.
 */
lList *ptf_jobs = nullptr;

static int is_ptf_running = 0;

/**
 * @brief Return the job id
 *
 * The function returns the os job id contained in the CULL element
 *
 * @param osjob the os job
 *
 * @return os job id (job id / ash / supplementary gid)
 */
static osjobid_t
ptf_get_osjobid(const ocs::tom::ConstElement<ocs::tom::JO> osjob) {
   DENTER(TOP_LAYER);

   osjobid_t osjobid;

#if !defined(LINUX) && !defined(SOLARIS) && !defined(DARWIN) && !defined(FREEBSD) && !defined(NETBSD)

   osjobid = ocs::tom::get<JO_OS_job_ID2>(osjob);
   osjobid = (osjobid << 32) + ocs::tom::get<JO_OS_job_ID>(osjob);

#else

   osjobid = osjob.get<JO_OS_job_ID>();

#endif

   DRETURN(osjobid);
}

/**
 * @brief Set os job id
 *
 * Set the attribute of "osjob" containing the os job id
 *
 * @param osjob the os job
 * @param osjobid os job id (job id / ash / supplementary gid)
 */
static void
ptf_set_osjobid(const ocs::tom::Element<ocs::tom::JO> osjob, const osjobid_t osjobid) {
   DENTER(TOP_LAYER);

#if !defined(LINUX) && !defined(SOLARIS) && !defined(DARWIN) && !defined(FREEBSD) && !defined(NETBSD)

   ocs::tom::set<JO_OS_job_ID2>(osjob, ((u_osjobid_t) osjobid) >> 32);
   ocs::tom::set<JO_OS_job_ID>(osjob, osjobid & 0xffffffff);

#else

   osjob.set<JO_OS_job_ID>(osjobid);

#endif

   DRETURN_VOID;
}

/**
 * @brief Create a new usage list from an existing list
 *
 * This method creates a new usage list or makes a copy of a old
 * usage list and zeros out the usage values.
 *
 * @param name name of the new list
 * @param usage_collection which usage attributes to include
 *
 * @return the new usage list, owned by the caller
 */
ocs::tom::ListPtr<ocs::tom::UA>
ptf_build_usage_list(const char *name, const usage_collection_t usage_collection) {
   DENTER(TOP_LAYER);

   auto usage_list = ocs::tom::ListPtr<ocs::tom::UA>::create(name);

   usage_list->emplace_back<UA_name>(USAGE_ATTR_WALLCLOCK);

   if (usage_collection != USAGE_COLLECTION_NONE) {
      usage_list->emplace_back<UA_name>(USAGE_ATTR_IO);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_IOOPS);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_IOW);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_MEM);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_CPU);

#if defined(LINUX) || defined(SOLARIS) || defined(FREEBSD) || defined(DARWIN)
      usage_list->emplace_back<UA_name>(USAGE_ATTR_VMEM);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_MAXVMEM);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_RSS);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_MAXRSS);
#endif

#if defined(LINUX)
   if (mconf_get_enable_mem_details()) {
      usage_list->emplace_back<UA_name>(USAGE_ATTR_PSS);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_MAXPSS);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_PMEM);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_SMEM);
   }
#endif
   }

   DRETURN(usage_list);
}

/**
 * @brief Set static priority
 *
 * If execd switches from SGEEE to SGE mode this functions is used to
 * reinitialize static priorities of all jobs currently running.
 *
 * @param job_id job id
 * @param ja_task_id task number
 * @param pe_task_id_str pe task id string or nullptr
 * @param priority new static priority
 */
void ptf_reinit_queue_priority(uint32_t job_id, uint32_t ja_task_id,
                               const char *pe_task_id_str, int priority) {
   DENTER(TOP_LAYER);

   if (!job_id || !ja_task_id) {
      DRETURN_VOID;
   }

   for (const auto job_elem : ocs::tom::List<ocs::tom::JL>(ptf_jobs)) {

      if (job_elem.get<JL_job_ID>() == job_id) {
         DPRINTF("\tjob id: " sge_u32 "\n", job_elem.get<JL_job_ID>());
         for (const auto os_job : job_elem.get<JL_OS_job_list>()) {
            if (os_job.get<JO_ja_task_ID>() == ja_task_id &&
                ((!pe_task_id_str && !os_job.get<JO_task_id_str>())
                 || (pe_task_id_str && os_job.get<JO_task_id_str>() &&
                     !strcmp(pe_task_id_str,
                             os_job.get<JO_task_id_str>())))) {

               DPRINTF("\t\tChanging priority for osjobid: " sge_u32 " jatask " sge_u32 " petask %s\n",
                       os_job.get<JO_OS_job_ID>(), os_job.get<JO_ja_task_ID>(),
                       pe_task_id_str ? pe_task_id_str : "");

               ptf_set_native_job_priority(job_elem, os_job, PTF_PRIORITY_TO_NATIVE_PRIORITY(priority));
            }
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Update job priority
 *
 * The funktion updates the priority of each process which belongs
 * to "job". The attribute JL_pri of "job" specifies the new priority.
 *
 * @param job the job
 */
static void
ptf_set_job_priority(const ocs::tom::ConstElement<ocs::tom::JL> job) {
   DENTER(TOP_LAYER);
   long pri = job.get<JL_pri>();

   for (const auto osjob : job.get<JL_OS_job_list>()) {
      ptf_set_native_job_priority(job, osjob, pri);
   }
   DRETURN_VOID;
}

/**
 * @brief Change job priority
 *
 * The function updates the priority of each process which belongs
 * to "job" and "osjob".
 *
 * @param job job
 * @param osjob one of the os jobs of "job"
 * @param pri new priority value
 */
static void
ptf_set_native_job_priority(const ocs::tom::ConstElement<ocs::tom::JL> job,
                            const ocs::tom::ConstElement<ocs::tom::JO> osjob, const long pri) {
   DENTER(TOP_LAYER);
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   ptf_setpriority_addgrpid(job, osjob, pri);
#endif
   DRETURN_VOID;
}

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

/**
 * @brief Change priority of processes
 *
 * This function is only available for the architecture SOLARIS,
 * LINUX, DARWIN and FREEBSD. All processes belonging to "job" and "osjob" will
 * get a new priority.
 * This function assumes the the "max" priority is smaller than the "min"
 * priority.
 *
 * @param job job
 * @param osjob one of the os jobs of "job"
 * @param pri new priority
 */
static void
ptf_setpriority_addgrpid(const ocs::tom::ConstElement<ocs::tom::JL> job,
                         const ocs::tom::ConstElement<ocs::tom::JO> osjob, const long pri) {
   DENTER(TOP_LAYER);

   /*
    * set the priority for each active process
    */
   for (const auto pid : osjob.get<JO_pid_list>()) {
      if (setpriority(PRIO_PROCESS, pid.get<JP_pid>(), pri) < 0 &&
          errno != ESRCH) {
         ERROR(MSG_PRIO_JOBXPIDYSETPRIORITYFAILURE_UUS, job.get<JL_job_ID>(), pid.get<JP_pid>(),
               strerror(errno));
      } else {
         DPRINTF("Changing Priority of process " sge_u32 " to %ld\n", pid.get<JP_pid>(), pri);
      }
   }
   DRETURN_VOID;
}

#endif

/**
 * @brief Look up the job for the supplied job_id and return it
 *
 * look up the job for the supplied job_id and return it
 *
 * @param job_id SGE job id
 *
 * @return the job, or an empty handle if the PTF does not know it
 */
static ocs::tom::Element<ocs::tom::JL>
ptf_get_job(const u_long job_id) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::tom::List<ocs::tom::JL>(ptf_jobs).find_first<JL_job_ID>(job_id));
}

/**
 * @brief Look up the job for the supplied OS job_id
 *
 * This functions tries to find a os job element (JO) with
 * "os_job_id" within the list of os jobs of "job_elem". If "job_elem"
 * is not provided the function will look up the whole "job_list".
 *
 * @param job_list List of all known jobs
 * @param os_job_id os job id (ash, job id, supplementary gid)
 * @param job_elem Pointer to a job handle - pointer to an empty handle => *job_elem will contain the
 *                 corresponding job when the function returns successfully - nullptr
 *
 * @return osjob or an empty handle if it was not found.
 */
static ocs::tom::Element<ocs::tom::JO>
ptf_get_job_osjob_by_osjobid(const ocs::tom::List<ocs::tom::JL> job_list, const osjobid_t os_job_id,
                             ocs::tom::Element<ocs::tom::JL> *job_elem) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JO> osjob;

   // @todo use find_first<JO_OS_job_ID>() instead of building a condition
   //       only possible, when we remove the non LINUX, SOLARIS etc. branch
   //       or make a uint64_t out of the 2 uint32_t values
#if defined(LINUX) || defined(SOLARIS) || defined(DARWIN) || defined(FREEBSD) || defined(NETBSD)
   const auto where = ocs::tom::field<JO_OS_job_ID>() == static_cast<uint32_t>(os_job_id);
#else
   const auto where = ocs::tom::field<JO_OS_job_ID>() == static_cast<uint32_t>(os_job_id & 0xffffffff) &&
                      ocs::tom::field<JO_OS_job_ID2>() == static_cast<uint32_t>(((u_osjobid_t) os_job_id) >> 32);
#endif

   if (!where) {
      CRITICAL(SFNMAX, MSG_WHERE_FAILEDTOBUILDWHERECONDITION);
      DRETURN(osjob);
   }

   if (job_elem != nullptr && *job_elem) {
      osjob = job_elem->get<JL_OS_job_list>().find_if(where);
   } else {
      ocs::tom::Element<ocs::tom::JL> job;
      for (const auto candidate : job_list) {
         osjob = candidate.get<JL_OS_job_list>().find_if(where);
         if (osjob) {
            job = candidate;
            break;
         }
      }
      if (job_elem != nullptr) {
         *job_elem = job;
      }
   }

   DRETURN(osjob);
}

/**
 * @brief Find the os job of an array task or of a PE task
 *
 * @param osjoblist the os jobs of one job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr for the array task itself
 *
 * @return the os job, or an empty handle if there is none
 */
static ocs::tom::Element<ocs::tom::JO>
ptf_get_osjob_by_ids(const ocs::tom::List<ocs::tom::JO> osjoblist, const uint32_t ja_task_id,
                     const char *pe_task_id) {
   DENTER(TOP_LAYER);

   for (const auto osjob : osjoblist) {
      if (osjob.get<JO_ja_task_ID>() == ja_task_id) {
         const char *osjob_pe_task_id = osjob.get<JO_task_id_str>();
         if (pe_task_id == nullptr) {
            if (osjob_pe_task_id == nullptr) {
               // we found the array task element (and not a possible pe task element with the same job/ja_task_id)
               DRETURN(osjob);
            }
         } else {
            // we have a pe task, check if the task id matches
            if (osjob_pe_task_id != nullptr && strcmp(pe_task_id, osjob_pe_task_id) == 0) {
               // found the pe task element
               DRETURN(osjob);
            }
         }
      }
   }

   DRETURN(ocs::tom::Element<ocs::tom::JO>());
}

/**
 * @brief Process a job received from the SGE scheduler
 *
 * This assumes that new jobs can be received after a job ticket
 * list has been sent.  The new jobs will have an associated number
 * of tickets which will be updated in the job_list maintained by
 * the PTF.
 *
 * @param os_job_id the operating system job id, or the additional group id
 * @param task_id_str the PE task, or nullptr for the array task itself
 * @param new_job the job, with the array task as first element of `JB_ja_tasks`
 * @param jataskid the array task
 * @param systemd_scope the systemd scope, or nullptr
 * @param usage_collection how much usage detail to collect
 *
 * @return the PTF job
 */
static ocs::tom::Element<ocs::tom::JL>
ptf_process_job(const osjobid_t os_job_id, const char *task_id_str, const ocs::tom::ConstElement<ocs::tom::JB> new_job,
                const uint32_t jataskid, const char *systemd_scope, const usage_collection_t usage_collection) {
   DENTER(TOP_LAYER);

   u_long job_id = new_job.get<JB_job_number>();
   double job_tickets = new_job.get<JB_ja_tasks>().front().get<JAT_tix>();
   bool interactive = new_job.get<JB_script_file>() == nullptr;

   /*
    * Add the job to the job list, if it does not already exist
    */

/*
 * cases:
 *
 * job == nullptr && osjobid == 0
 *    return
 *    job == nullptr && osjobid > 0
 *    add osjob job && osjobid > 0 search by osjobid if osjob
 *    found skip
 *    else
 *    add osjob job && osjobid == 0 skip
 */
   auto job = ptf_get_job(job_id);
   if (!job) {
      // the free form, as it creates ptf_jobs when there is none yet
      job = ocs::tom::emplace_back<JL_job_ID>(&ptf_jobs, job_id);
   }
   if (job) {
      auto osjob = ptf_get_osjob_by_ids(job.get<JL_OS_job_list>(), jataskid, task_id_str);
      if (!osjob) {
         osjob = job.emplace_back_in<JL_OS_job_list, JO_ja_task_ID>(jataskid);
         if (osjob) {
            if (task_id_str != nullptr) {
               osjob.set<JO_task_id_str>(task_id_str);
            }
            osjob.set<JO_usage_list>(ptf_build_usage_list("usagelist", usage_collection));
         }
      }
      // this function might get called first before we have the osjobid
      // in later calls then make sure to update osjobid and systemd_scope
      if (osjob) {
         ptf_set_osjobid(osjob, os_job_id);
         osjob.set<JO_usage_collection>(usage_collection);
         if (systemd_scope != nullptr) {
            osjob.set<JO_systemd_scope>(systemd_scope);
         }
      }

      /*
       * set number of tickets in job entry
       */
      job.set<JL_tickets>(static_cast<uint32_t>(std::max(job_tickets, 1.0)));

      /*
       * set interactive job flag
       */
      if (interactive) {
         job.set<JL_interactive>(1);
      }
   }

   DRETURN(job);
}

/**
 * @brief Get usage from PDC
 *
 * get the usage for all the jobs in the job ticket list and update
 * the job list
 * call data collector routine with a list of all the jobs in the job_list
 * for each job in the job_list
 *    update job.usage with data collector info
 *    update list of process IDs associated with job end
 * do
 */
static void ptf_get_usage_from_data_collector() {
#ifdef USE_DC
   DENTER(TOP_LAYER);

   struct psJob_s *jobs, *ojobs, *tmp_jobs;
   struct psProc_s *procs;
   uint64 jobcount;
   int proccount;
   const char *tid;
   int i, j;
   bool enable_mem_details = mconf_get_enable_mem_details();

   // in case of hybrid mode, we will not use PDC data for systemd provided usage (cpu, rss, maxrss)

   ojobs = jobs = psGetAllJobs();
   if (jobs) {
      jobcount = *(uint64 *) jobs;

      INCJOBPTR(jobs, sizeof(uint64));

      for (i = 0; i < (int)jobcount; i++) {
         double cpu_usage_value;

         /* look up job in job list */

         ocs::tom::Element<ocs::tom::JL> job;
         // Passing nullptr as systemd_scope:
         // Here we are in the data collector, there must be an add_grp_id / osjobid.
         const auto osjob = ptf_get_job_osjob_by_osjobid(ocs::tom::List<ocs::tom::JL>(ptf_jobs), jobs->jd_jid, &job);
         if (osjob) {
            // If the osjobid / addgrp == 0, we do not want to get usage from PDC.
            if (osjob.get<JO_OS_job_ID>() != 0) {
               u_long job_state = osjob.get<JO_state>();

               tmp_jobs = jobs;

               /* fill in job completion state */
               osjob.set<JO_state>(jobs->jd_refcnt ?
                         (job_state & ~JL_JOB_COMPLETE) : (job_state | JL_JOB_COMPLETE));

               /* fill in usage for job */
               const auto usage_collection =
                  static_cast<usage_collection_t>(osjob.get<JO_usage_collection>());
               auto usage_list = osjob.get<JO_usage_list>();
               if (!usage_list) {
                  osjob.set<JO_usage_list>(ptf_build_usage_list("usagelist", usage_collection));
                  usage_list = osjob.get<JO_usage_list>();
               }

               /* set CPU usage */
               if (ocs::common::use_pdc_for_usage_collection(usage_collection)) {
                  cpu_usage_value = jobs->jd_utime_c + jobs->jd_utime_a + jobs->jd_stime_c + jobs->jd_stime_a;
                  usage_list_max_double_usage(usage_list, USAGE_ATTR_CPU, cpu_usage_value, false);

                  /* set rss and maxrss usage */
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_RSS, jobs->jd_rss, false);
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_MAXRSS, jobs->jd_maxrss, false);
               }

               /* set mem usage (in GB seconds) */
               usage_list_set_double_usage(usage_list, USAGE_ATTR_MEM, jobs->jd_mem / 1048576.0, false);

               /* set I/O usage (in GB) */
               usage_list_set_double_usage(usage_list, USAGE_ATTR_IO, jobs->jd_chars / 1073741824.0, false);

               /* set I/O ops */
               usage_list_set_double_usage(usage_list, USAGE_ATTR_IOOPS, jobs->jd_ioops, false);

               /* set I/O wait time */
               usage_list_set_double_usage(usage_list, USAGE_ATTR_IOW, jobs->jd_iow, false);

               /* set vmem and maxvmem usage */
               usage_list_set_double_usage(usage_list, USAGE_ATTR_VMEM, jobs->jd_vmem, false);
               usage_list_set_double_usage(usage_list, USAGE_ATTR_MAXVMEM, jobs->jd_himem, false);

               if (enable_mem_details) {
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_PSS, jobs->jd_pss, false);
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_MAXPSS, jobs->jd_maxpss, false);
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_PMEM, jobs->jd_pmem, false);
                  usage_list_set_double_usage(usage_list, USAGE_ATTR_SMEM, jobs->jd_smem, false);
               }

               /* build new pid list */
               proccount = jobs->jd_proccount;
               INCJOBPTR(jobs, jobs->jd_length);

               if (proccount > 0) {
                  const auto oldpidlist = osjob.get<JO_pid_list>();
                  auto pidlist = ocs::tom::ListPtr<ocs::tom::JP>::create("pidlist");

                  procs = (struct psProc_s *) jobs;
                  for (j = 0; j < proccount; j++) {
                     if (procs->pd_state == 1) {
                        const auto pid = oldpidlist.find_first<JP_pid>(procs->pd_pid);
                        if (pid) {
                           pidlist->push_back(pid.clone());
                        } else {
                           auto new_pid = ocs::tom::ElementPtr<ocs::tom::JP>::create();

                           new_pid->set<JP_pid>(procs->pd_pid);
                           pidlist->push_back(std::move(new_pid));
                        }
                     }
                     INCPROCPTR(procs, procs->pd_length);
                  }

                  jobs = (struct psJob_s *)procs;
                  osjob.set<JO_pid_list>(std::move(pidlist));
               } else {
                  osjob.set<JO_pid_list>(nullptr);
               }

               tid = osjob.get<JO_task_id_str>();
               DPRINTF("JOB " sge_u32 "." sge_u32 ": %s: (cpu = %8.3lf / mem = "
                       UINT64_FMT " / io = " UINT64_FMT " / vmem = "
                       UINT64_FMT " / himem = " UINT64_FMT ")\n",
                       job.get<JL_job_ID>(),
                       osjob.get<JO_ja_task_ID>(), tid ? tid : "",
                       tmp_jobs->jd_utime_c + tmp_jobs->jd_utime_a +
                       tmp_jobs->jd_stime_c + tmp_jobs->jd_stime_a,
                       tmp_jobs->jd_mem, tmp_jobs->jd_chars,
                       tmp_jobs->jd_vmem, tmp_jobs->jd_himem);
            }
         } else {
            /* 
             * NOTE: Under what conditions would DC have a job
             * that the PTF doesn't know about? 
             */
            psIgnoreJob(jobs->jd_jid);

            proccount = jobs->jd_proccount;
            INCJOBPTR(jobs, jobs->jd_length);
            procs = (struct psProc_s *) jobs;
            for (j = 0; j < proccount; j++) {
               INCPROCPTR(procs, procs->pd_length);
            }
            jobs = (struct psJob_s *) procs;
         }
      }
      sge_free(&ojobs);

      for (const auto job : ocs::tom::List<ocs::tom::JL>(ptf_jobs)) {
         double usage_value, old_usage_value;
         double cpu_usage_value = 0;
         int active_jobs = 0;

         for (const auto osjob : job.get<JL_OS_job_list>()) {
            const auto usage = osjob.get<JO_usage_list>().find_first<UA_name>(USAGE_ATTR_CPU);
            if (usage) {
               cpu_usage_value += usage.get<UA_value>();
            }
            if (!(osjob.get<JO_state>() & JL_JOB_COMPLETE)) {
               active_jobs = 1;
            }
         }

         /* calculate JL_usage */
         usage_value = cpu_usage_value;
         old_usage_value = job.get<JL_old_usage_value>();
         job.set<JL_old_usage_value>(usage_value);
         job.set<JL_usage>(
                                 std::max(PTF_MIN_JOB_USAGE, job.get<JL_usage>() *
                                          PTF_USAGE_DECAY_FACTOR + (usage_value - old_usage_value)));
         job.set<JL_last_usage>(usage_value - old_usage_value);

         /* set job state */
         if (!active_jobs) {
            job.set<JL_state>(job.get<JL_state>() & JL_JOB_COMPLETE);
         }
      }
   }

#else


   DRETURN_VOID;

#endif /* USE_DC */
}

/**
 * @brief Pass 0 of the proportional share calculation: sum up tickets and last usage
 *
 * @param job the job
 * @param sum_of_job_tickets the sum of the tickets of all jobs, increased by the job's tickets
 * @param sum_of_last_usage the sum of the last usage of all jobs, increased by the job's last usage
 */
static void
ptf_calc_job_proportion_pass0(const ocs::tom::ConstElement<ocs::tom::JL> job, u_long *sum_of_job_tickets,
                              double *sum_of_last_usage) {
   DENTER(TOP_LAYER);
   *sum_of_job_tickets += job.get<JL_tickets>();
   *sum_of_last_usage += job.get<JL_last_usage>();
   DRETURN_VOID;
}


/**
 * @brief Pass 1 of the proportional share calculation: the share and the proportion of a job
 *
 * @param job the job; JL_share, JL_ticket_share and JL_proportion are set
 * @param sum_of_job_tickets the sum of the tickets of all jobs, not 0
 * @param sum_of_last_usage the sum of the last usage of all jobs, unused
 * @param sum_proportion the sum of the proportions of all jobs, increased by the job's proportion
 */
static void
ptf_calc_job_proportion_pass1(const ocs::tom::Element<ocs::tom::JL> job, const u_long sum_of_job_tickets,
                              const double sum_of_last_usage, double *sum_proportion) {
   DENTER(TOP_LAYER);

   double share, job_proportion;
   double u;

   share = ((double) job.get<JL_tickets>() / sum_of_job_tickets) *
      1000.0 + 1.0;
   job.set<JL_share>(share);
   job.set<JL_ticket_share>((double) job.get<JL_tickets>() / sum_of_job_tickets);

   /*
    * Calculate each jobs proportion value based on the job's tickets
    * and recent usage:
    * job.proportion = (job.tickets/sum_of_job_tickets)^2 / job.usage
    */
   u = std::max(job.get<JL_usage>(), PTF_MIN_JOB_USAGE);
   job_proportion = share * share / u;
   *sum_proportion += job_proportion;
   job.set<JL_proportion>(job_proportion);

   DRETURN_VOID;
}

/**
 * @brief Pass 2 of the proportional share calculation: the adjusted usage and adjusted proportion of a job
 *
 * @param job the job; JL_adjusted_usage and JL_adjusted_proportion are set
 * @param sum_of_job_tickets the sum of the tickets of all jobs, not 0
 * @param sum_proportion the sum of the proportions of all jobs
 * @param sum_adjusted_proportion the sum of the adjusted proportions, increased by the job's one
 * @param sum_last_interval_usage the usage of the last interval, unused
 */
static void
ptf_calc_job_proportion_pass2(const ocs::tom::Element<ocs::tom::JL> job, const u_long sum_of_job_tickets,
                              const double sum_proportion, double *sum_adjusted_proportion,
                              double *sum_last_interval_usage) {
   DENTER(TOP_LAYER);

   double job_current_proportion = 0;
   double compensate_proportion;
   double job_targetted_proportion, job_adjusted_usage, job_adjusted_proportion, share;

   job_targetted_proportion = (double) job.get<JL_tickets>() /
                               sum_of_job_tickets;

   if (sum_proportion > 0) {
      job_current_proportion = job.get<JL_proportion>() / sum_proportion;
   }

   compensate_proportion = PTF_COMPENSATION_FACTOR * job_targetted_proportion;
   if (job_current_proportion > compensate_proportion) {
      job_adjusted_usage = job.get<JL_usage>() * 
          (job_current_proportion / compensate_proportion);
   } else {
      job_adjusted_usage = job.get<JL_usage>();
   }
   job.set<JL_adjusted_usage>(job_adjusted_usage);

   /*
    * Recalculate proportions based on adjusted job usage
    */
   share = job.get<JL_share>();
   job_adjusted_usage = std::max(job_adjusted_usage, PTF_MIN_JOB_USAGE);
   job_adjusted_proportion = share * share / job_adjusted_usage;
   *sum_adjusted_proportion += job_adjusted_proportion;
   job.set<JL_adjusted_proportion>(job_adjusted_proportion);

   DRETURN_VOID;
}

/**
 * @brief Pass 3 of the proportional share calculation: the adjusted current proportion of a job
 *
 * @param job the job; JL_last_proportion and JL_adjusted_current_proportion are set
 * @param sum_adjusted_proportion the sum of the adjusted proportions of all jobs
 * @param sum_last_interval_usage the usage of the last interval, unused
 * @param min_share the smallest adjusted current proportion so far, updated
 * @param max_share the largest adjusted current proportion so far, updated
 * @param max_ticket_share the largest ticket share so far, updated
 */
static void
ptf_calc_job_proportion_pass3(const ocs::tom::Element<ocs::tom::JL> job, const double sum_adjusted_proportion,
                              const double sum_last_interval_usage, double *min_share, double *max_share,
                              double *max_ticket_share) {
   DENTER(TOP_LAYER);

   double job_adjusted_current_proportion = 0;

   if (sum_adjusted_proportion > 0)
      job_adjusted_current_proportion =
         job.get<JL_adjusted_proportion>() / sum_adjusted_proportion;

   job.set<JL_last_proportion>(
              job.get<JL_adjusted_current_proportion>());

   job.set<JL_adjusted_current_proportion>(
              job_adjusted_current_proportion);

   *max_share = std::max(*max_share, job_adjusted_current_proportion);
   *min_share = std::min(*min_share, job_adjusted_current_proportion);
   *max_ticket_share = std::max(*max_ticket_share, job.get<JL_ticket_share>());

#if 0
    DPRINTF(("XXXXXXX  minshare: %f, max_share: %f XXXXXXX\n", *min_share, 
             *max_share)); 
#endif

   DRETURN_VOID;
}

/**
 * @brief Set the O.S. scheduling parameters of all jobs
 *
 * This function assumes the the "max" priority is smaller than the "min"
 * priority.
 *
 * @param job_list the jobs; JL_timeslice and JL_pri are set and the priority is applied
 * @param min_share the smallest current priority of the jobs
 * @param max_share the largest current priority of the jobs
 * @param max_ticket_share the largest ticket share of the jobs
 */
static void
ptf_set_OS_scheduling_parameters(const ocs::tom::List<ocs::tom::JL> job_list, const double min_share,
                                 const double max_share, const double max_ticket_share) {
   DENTER(TOP_LAYER);

   static long pri_range, pri_min = -999, pri_max = -999;
   long pri_min_tmp, pri_max_tmp;
   long pri;
   
   
   pri_min_tmp = mconf_get_ptf_min_priority();
   if (pri_min_tmp == -999) {
      pri_min_tmp = PTF_MIN_PRIORITY;   
   }

   pri_max_tmp = mconf_get_ptf_max_priority();
   if (pri_max_tmp == -999) {
      pri_max_tmp = PTF_MAX_PRIORITY;   
   }

   /* 
    * For OS'es where we enforce the values of max and min priority verify
    * the the values are in the boundaries of PTF_OS_MAX_PRIORITY and
    * PTF_OS_MIN_PRIORITY.
    */
#if ENFORCE_PRI_RANGE
   pri_max_tmp = std::max(pri_max_tmp, PTF_OS_MAX_PRIORITY);
   pri_min_tmp = std::min(pri_min_tmp, PTF_OS_MIN_PRIORITY);
#endif 

   /*
    * Ensure that the max priority can't get a bigger value than
    * the min priority (otherwise the "range" gets wrongly calculated
    */         
   if (pri_max_tmp > pri_min_tmp) {
      pri_max_tmp = pri_min_tmp;
   }
      
   /* If the value has changed set pri_max/pri_min/pri_range and log */
   if (pri_max != pri_max_tmp || pri_min != pri_min_tmp) {
      uint32_t old_ll = log_state_get_log_level();
      
      pri_max = pri_max_tmp;
      pri_min = pri_min_tmp;
      pri_range = pri_min - pri_max;
   
      log_state_set_log_level(LOG_INFO);   
      INFO(MSG_PRIO_PTFMINMAX_II, (int) pri_max, (int) pri_min);
      log_state_set_log_level(old_ll);   
   }

   /* Set the priority for each job */
   for (const auto job : job_list) {

      pri = pri_max + (long)(pri_range * (1.0 -
                       job.get<JL_adjusted_current_proportion>()));

      /*
       * Note: Should calculate targetted proportion and if it is below
       * a certain % then set a background priority.  This is because
       * nice seems to always give at least some minimal % to all
       * processes.
       */

#ifdef notdef
      pri = pri_max + (pri_range * (ocs::tom::get<JL_curr_pri>(job) / max_share));
#endif

      if (max_share > 0) {
         pri = pri_max + (long)(pri_range * ((job.get<JL_curr_pri>() 
                                              - min_share) / max_share));
      } else {
         pri = pri_min;
      }

      if (pri_min > 50 && pri_max > 50) {
         if (max_ticket_share > 0) {
  	         job.set<JL_timeslice>(
                       std::max(static_cast<double>(pri_max), job.get<JL_ticket_share>() * pri_min / max_ticket_share));
         } else {
            job.set<JL_timeslice>(pri_min);
         }
      }
      job.set<JL_pri>(pri);
      ptf_set_job_priority(job);
   }

   DRETURN_VOID;
}


/** @brief Register a job the PTF should start adjusting
 *
 * @param os_job_id the operating system job id, or the additional group id
 * @param task_id_str the PE task, or nullptr for the job itself
 * @param new_job the job, with the array task as first element of `JB_ja_tasks`
 * @param jataskid the array task
 * @param systemd_scope the job's systemd scope, or nullptr when systemd is not in use
 * @param usage_collection how much usage detail to collect for it
 * @return 0 on success, one of the `PTF_ERROR_*` values otherwise
 */
int
ptf_job_started(const osjobid_t os_job_id, const char *task_id_str, const ocs::tom::ConstElement<ocs::tom::JB> new_job,
                const uint32_t jataskid, const char *systemd_scope, const usage_collection_t usage_collection) {
   DENTER(TOP_LAYER);

   /*
    * Add new job to job list
    */
   ptf_process_job(os_job_id, task_id_str, new_job, jataskid, systemd_scope, usage_collection);

   /*
    * Tell data collector to start collecting data for this job
    */
#ifdef USE_DC
   if (os_job_id > 0) {
      psWatchJob(os_job_id, usage_collection);
   }
#else


#endif

   DRETURN(0);
}

/** @brief Take a finished job out of the PTF and hand back its final usage
 *
 * The entry is kept until the usage has been collected, so that a job which
 * ends between two rounds does not lose what it consumed.
 *
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr for the job itself
 * @param[out] usage receives the final usage list, owned by the caller; set also when the os job is not found
 * @return 0 on success, one of the `PTF_ERROR_*` values otherwise
 */
int
ptf_job_complete(const uint32_t job_id, const uint32_t ja_task_id, const char *pe_task_id,
                 ocs::tom::ListPtr<ocs::tom::UA> &usage) {
   DENTER(TOP_LAYER);

   auto ptf_job = ptf_get_job(job_id);

   if (!ptf_job) {
      DRETURN(PTF_ERROR_JOB_NOT_FOUND);
   }

   /*
    * if job is not complete, go get latest job usage info
    */
   if (!(ptf_job.get<JL_state>() & JL_JOB_COMPLETE)) {
      // @todo This will get usage for all jobs, not just the one, might be expensive when many jobs finish,
      //       e.g. with short jobs on a big machine.
      //       And does it make sense at all? We get here when a job finished - all its processes / its systemd scope
      //       should have vanished by now.
      sge_switch2start_user();
      ptf_get_usage_from_data_collector();
      sge_switch2admin_user();

#if defined (OCS_WITH_SYSTEMD)
      if (ocs::execd::execd_use_systemd()) {
         ocs::execd::ptf_get_usage_from_systemd();
      }
#endif
   }

   /*
    * Get usage for completed job
    *    (If this for a sub-task then we return the sub-task usage)
    */
   usage = _ptf_get_job_usage(ptf_job, ja_task_id, pe_task_id);

   // search ptf osjob
   const auto osjobs = ptf_job.get<JL_OS_job_list>();
   const auto osjob = ptf_get_osjob_by_ids(osjobs, ja_task_id, pe_task_id);

   if (!osjob) {
      DRETURN(PTF_ERROR_JOB_NOT_FOUND);
   } 
   
   /*
    * Mark osjob as complete and see if all tracked osjobs are done
    * Tell data collector to stop collecting data for this job
    */
   osjob.set<JO_state>(osjob.get<JO_state>() | JL_JOB_DELETED);
#ifdef USE_DC
   psIgnoreJob(ptf_get_osjobid(osjob));
#endif

#ifndef USE_DC
#endif

   /*
    * Remove job/task from job/task list
    */

   DPRINTF("PTF: Removing job " sge_u32 "." sge_u32 ", petask %s\n",
           job_id, ja_task_id, pe_task_id == nullptr ? "none" : pe_task_id);
   osjobs.erase(osjob);

   if (osjobs.empty()) {
      DPRINTF("PTF: Removing job\n");
      ocs::tom::List<ocs::tom::JL>(ptf_jobs).erase(ptf_job);
   }

   DRETURN(0);
}


/** @brief Take a new set of ticket values from the scheduler
 *
 * Tickets are what the scheduler decided each job is entitled to; the next
 * round of ptf_adjust_job_priorities() works towards them.
 *
 * @param job_ticket_list the tickets, per job, with the array task as first element of `JB_ja_tasks`
 * @return 0 on success, one of the `PTF_ERROR_*` values otherwise
 */
int
ptf_process_job_ticket_list(const ocs::tom::List<ocs::tom::JB> job_ticket_list) {
   DENTER(TOP_LAYER);

    /*
     * Update the job entries in the job list with the number of
     * tickets from the job ticket list.  Reset the usage to the
     * minimum usage value.
     */
   for (const auto jte : job_ticket_list) {
      /*
       * set JB_script_file because we don't know if this is
       * an interactive job 
       */
      // @todo required?
      jte.set<JB_script_file>("dummy");

      // The job and os job should already exist.
      // If not it would not get created in ptf_process_job(), but probably later on
      // once the job is started.
      // @todo what about tightly integrated PE tasks? The ja_task then only is a SLAVE container,
      //       it doesn't have osjobid, systemd_scope, usage_collection.
      uint32_t job_id = jte.get<JB_job_number>();
      const auto jte_ja_task = jte.get<JB_ja_tasks>().front();
      uint32_t ja_task_id = jte_ja_task.get<JAT_task_number>();
      // the objects belong to the execd's job list
      ocs::tom::Element<ocs::tom::JB> job;
      ocs::tom::Element<ocs::tom::JAT> ja_task;
      if (execd_get_job_ja_task(job_id, ja_task_id, &job, &ja_task, false)) {
         osjobid_t osjobid{};
         const char *osjobid_str = ja_task.get<JAT_osjobid>();
         if (osjobid_str != nullptr) {
            osjobid = static_cast<osjobid_t>(std::stoi(osjobid_str));
         }
         const auto ptf_job = ptf_process_job(osjobid, nullptr,
            jte, ja_task_id, ja_task.get<JAT_systemd_scope>(),
            static_cast<usage_collection_t>(ja_task.get<JAT_usage_collection>()));
         if (ptf_job) {
            /* reset temporary usage and priority */
            ptf_job.set<JL_usage>(std::max(PTF_MIN_JOB_USAGE, ptf_job.get<JL_usage>() * 0.1));
            ptf_job.set<JL_curr_pri>(0);
         }
      } else {
         // this might be a valid situation, e.g., immediately after job start
      }
   }

   DRETURN(0);
}

/** @brief Refresh every registered job's usage from the data collector
 *
 * Called before the priorities are adjusted and before usage is reported to
 * the qmaster, so both work from the same reading.
 */
void ptf_update_job_usage() {
   DENTER(TOP_LAYER);

   // We always call ptf_get_usage_from_data_collector() to update the usage
   // even if we are running jobs via systemd, because we might have jobs
   // which were started with USAGE_COLLECTION being configured to PDC or HYBRID.
   // If all jobs have been started with systemd usage collection only, then
   // ptf_get_usage_from_data_collector() will not find any jobs to update.
   sge_switch2start_user();
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM3);
   ptf_get_usage_from_data_collector();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM3);
   sge_switch2admin_user();

   // Similar for systemd, we always call ptf_get_usage_from_systemd() if systemd is available at all.
   // Whether a job gets usage via systemd can be determined from the systemd scope stored in the array task
   // or pe task object.
#if defined (OCS_WITH_SYSTEMD)
   if (ocs::execd::execd_use_systemd()) {
      PROF_START_MEASUREMENT(SGE_PROF_CUSTOM2);
      ocs::execd::ptf_get_usage_from_systemd();
      PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM2);
   }
#endif

   DRETURN_VOID;
}


/** @brief One round of the feedback loop: nudge each job's nice value
 *
 * Compares what each job has consumed against what its tickets entitled it to
 * and moves its nice value towards the difference, damped by
 * #PTF_DIFF_DECAY_CONSTANT. Runs at most every #PTF_SCHEDULE_TIME seconds.
 *
 * @return 0 on success, one of the `PTF_ERROR_*` values otherwise
 */
int ptf_adjust_job_priorities() {
   DENTER(TOP_LAYER);
   static uint64_t next = 0;
   uint64_t now = sge_get_gmt64();
   u_long sum_of_job_tickets = 0;
   int num_procs = 0;
   double sum_proportion = 0;
   double sum_adjusted_proportion = 0;
   double min_share = 100.0;
   double max_share = 0;
   double max_ticket_share = 0;
   double sum_interval_usage = 0;
   double sum_of_last_usage = 0;

   if (now < next) {
      DRETURN(0);
   }

   const auto job_list = ocs::tom::List<ocs::tom::JL>(ptf_jobs);

   /*
    * Do pass 0 of calculating job proportional share of resources
    */
   for (const auto job : job_list) {
      ptf_calc_job_proportion_pass0(job, &sum_of_job_tickets,
                                    &sum_of_last_usage);
   }
   if (sum_of_job_tickets == 0) {
      sum_of_job_tickets = 1;
   }

   /*
    * Do pass 1 of calculating job proportional share of resources
    */
   for (const auto job : job_list) {
      ptf_calc_job_proportion_pass1(job, sum_of_job_tickets,
                                    sum_of_last_usage, &sum_proportion);
   }

   /*
    * Do pass 2 of calculating job proportional share of resources
    */
   for (const auto job : job_list) {
      ptf_calc_job_proportion_pass2(job, sum_of_job_tickets,
                                    sum_proportion, 
                                    &sum_adjusted_proportion,
                                    &sum_interval_usage);
   }

   /*
    * Do pass 3 of calculating job proportional share of resources
    */
   for (const auto job : job_list) {
      ptf_calc_job_proportion_pass3(job, sum_proportion,
                                    sum_interval_usage, &min_share,
                                    &max_share, &max_ticket_share);
   }

   max_share = 0;
   min_share = -1;

   for (const auto job : job_list) {
      double shr;

      /*
       * calculate share based on tickets only
       */
      shr = job.get<JL_share>();

      num_procs = 0;
      for (const auto osjob : job.get<JL_OS_job_list>()) {
         const auto pid_list = osjob.get<JO_pid_list>();
         if (pid_list) {
            num_procs += static_cast<int>(pid_list.size());
         }
      }
      num_procs = std::max(1, num_procs);

      /* 
       * NOTE: share algo only adjusts priority when a process runs 
       */
      job.set<JL_curr_pri>(job.get<JL_curr_pri>() 
                 + ((job.get<JL_adjusted_usage>() * num_procs) 
                    / (shr * shr)));

      max_share = std::max(max_share, job.get<JL_curr_pri>());
      if (min_share < 0) {
         min_share = job.get<JL_curr_pri>();
      } else {
         min_share = std::min(min_share, job.get<JL_curr_pri>());
      } 
   }

   /*
    * Set the O.S. scheduling parameters for the jobs
    */
   ptf_set_OS_scheduling_parameters(job_list, min_share, max_share,
                                    max_ticket_share);
   
   next = now + sge_gmt32_to_gmt64(PTF_SCHEDULE_TIME);

   DRETURN(0);
}

/** @brief The usage collected so far for one job
 * @param job_id the job
 * @param ja_task_id the array task
 * @param task_id the PE task, or nullptr for the job itself
 * @return the usage list, owned by the caller, or an empty handle when the job is not registered
 */
ocs::tom::ListPtr<ocs::tom::UA>
ptf_get_job_usage(const u_long job_id, const u_long ja_task_id, const char *task_id) {
   DENTER(TOP_LAYER);
   DRETURN(_ptf_get_job_usage(ptf_get_job(job_id), ja_task_id, task_id));
}

/**
 * @brief The usage of the os jobs of a job, summed up
 *
 * @param job the job, or an empty handle
 * @param ja_task_id the array task
 * @param task_id the PE task, "*" for all of them, or nullptr or "" for the array task itself
 *
 * @return the usage list, owned by the caller, or an empty handle when no os job matches
 */
static ocs::tom::ListPtr<ocs::tom::UA>
_ptf_get_job_usage(const ocs::tom::ConstElement<ocs::tom::JL> job, const u_long ja_task_id, const char *task_id) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::UA> job_usage;

   if (!job) {
      DRETURN(job_usage);
   }

   for (const auto osjob : job.get<JL_OS_job_list>()) {
      const char *task_id_str = osjob.get<JO_task_id_str>();

      if ((((!task_id || !task_id[0]) && !task_id_str) 
           || (task_id && !strcmp(task_id, "*")) 
           || (task_id && task_id_str && !strcmp(task_id, task_id_str)))
          && (osjob.get<JO_ja_task_ID>() == ja_task_id)) {

         if (job_usage) {
            for (const auto usrc : osjob.get<JO_usage_list>()) {
               const auto udst = job_usage->find_first<UA_name>(usrc.get<UA_name>());
               if (udst) {
                  udst.set<UA_value>(udst.get<UA_value>() + usrc.get<UA_value>());
               } else {
                  job_usage->push_back(usrc.clone());
               }
            }
         } else {
            job_usage = osjob.get<JO_usage_list>().clone();
         }
      }
   }

   DRETURN(job_usage);
}

/** @brief The usage collected so far for every registered job
 *
 * The job list holds the job id and the usage filled in. A separate job entry is returned for each tracked
 * sub-task.
 *
 * @param[out] job_usage_list receives one entry per job, reduced to `JB_job_number` and `JB_ja_tasks`; a list
 *                            held before is freed
 * @return 0 on success, one of the `PTF_ERROR_*` values otherwise
 */
int
ptf_get_usage(ocs::tom::ListPtr<ocs::tom::JB> &job_usage_list) {
   DENTER(TOP_LAYER);

   const auto what = ocs::tom::what<JB_job_number, JB_ja_tasks>();

   auto temp_usage_list = ocs::tom::ListPtr<ocs::tom::JB>::create("PtfJobUsageList");
   for (const auto job : ocs::tom::ConstList<ocs::tom::JL>(ptf_jobs)) {
      uint32_t job_id = job.get<JL_job_ID>();
      for (const auto osjob : job.get<JL_OS_job_list>()) {
         uint32_t ja_task_id = osjob.get<JO_ja_task_ID>();
         const char *pe_task_id = osjob.get<JO_task_id_str>();

         if (osjob.get<JO_state>() & JL_JOB_DELETED) {
            continue;
         }

         auto tmp_job = temp_usage_list->find_first<JB_job_number>(job_id);
         if (!tmp_job) {
            auto new_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
            new_job->set<JB_job_number>(job_id);
            tmp_job = temp_usage_list->push_back(std::move(new_job));
         }

         auto tmp_ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(tmp_job, nullptr, ja_task_id));
         if (!tmp_ja_task) {
            tmp_ja_task = tmp_job.emplace_back_in<JB_ja_tasks, JAT_task_number>(ja_task_id);
         }

         if (pe_task_id != nullptr) {
            auto tmp_pe_task = ocs::tom::Element<ocs::tom::PET>(ja_task_search_pe_task(tmp_ja_task, pe_task_id));
            if (!tmp_pe_task) {
               tmp_pe_task = tmp_ja_task.emplace_back_in<JAT_task_list, PET_id>(pe_task_id);
            }
            tmp_pe_task.set<PET_usage>(osjob.get<JO_usage_list>().clone());
         } else {
            tmp_ja_task.set<JAT_usage_list>(osjob.get<JO_usage_list>().clone());
         }
      } /* for each osjob */
   } /* for each ptf job */

   job_usage_list = temp_usage_list->select(what, "PtfJobUsageList");

   DRETURN(0);
}


/** @brief Initialize the Priority Translation Facility: the PTF's job list and the data collector
 * @return 0 on success, -1 if the data collector could not be started
 */
int ptf_init() {
   DENTER(TOP_LAYER);
   ocs::tom::install_model(ocs::tom::gcs_model());

   ptf_jobs = ocs::tom::ListPtr<ocs::tom::JL>::create("ptf_job_list").release();

   sge_switch2start_user();
   if (psStartCollector()) {
      sge_switch2admin_user();
      DRETURN(-1);
   }
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   if (getuid() == 0) {
      if (setpriority(PRIO_PROCESS, getpid(), PTF_MAX_PRIORITY) < 0) {
         ERROR(MSG_PRIO_SETPRIOFAILED_S, strerror(errno));
      }
   }
#endif
   sge_switch2admin_user();
   DRETURN(0);
}

/** @brief Begin adjusting priorities
 *
 * Idempotent; ptf_is_running() reports the state.
 */
void ptf_start() {
   DENTER(TOP_LAYER);
   if (!is_ptf_running) {
      ptf_init();
      is_ptf_running = 1;
   }
   DRETURN_VOID;
}

/** @brief Stop adjusting priorities and forget every registered job */
void ptf_stop() {
   DENTER(TOP_LAYER);
   if (is_ptf_running) {
      ptf_unregister_registered_jobs();
      psStopCollector();
      is_ptf_running = 0;
   }

   ocs::tom::ListPtr<ocs::tom::JL>::from_typed(std::exchange(ptf_jobs, nullptr)).reset();
   DRETURN_VOID;
}

/** @brief Write the registered jobs and their priorities to the log */
void ptf_show_registered_jobs() {
   DENTER(TOP_LAYER);

   for (const auto job_elem : ocs::tom::ConstList<ocs::tom::JL>(ptf_jobs)) {
      DPRINTF("\tjob id: " sge_u32 "\n", job_elem.get<JL_job_ID>());
      for (const auto os_job : job_elem.get<JL_OS_job_list>()) {
         const char *pe_task_id_str;
         uint32_t ja_task_id;

         pe_task_id_str = os_job.get<JO_task_id_str>();
         pe_task_id_str = pe_task_id_str ? pe_task_id_str : "<null>";
         ja_task_id = os_job.get<JO_ja_task_ID>();

         DPRINTF("\t\tosjobid: " sge_u32 " ja_task_id: " sge_u32 " petaskid: %s\n",
                 os_job.get<JO_OS_job_ID>(), ja_task_id, pe_task_id_str);
         for (const auto process : os_job.get<JO_pid_list>()) {
            uint32_t pid = process.get<JP_pid>();
            DPRINTF("\t\t\tpid: " sge_u32 "\n", pid);
         }
      }
   }
   DRETURN_VOID;
}

/** @brief Forget one job without collecting its usage
 * @param job_id the job
 * @param ja_task_id the array task
 */
void ptf_unregister_registered_job(uint32_t job_id, uint32_t ja_task_id ) {
   DENTER(TOP_LAYER);

   const auto job_list = ocs::tom::List<ocs::tom::JL>(ptf_jobs);
   auto job = job_list.front();
   while (job) {
      const auto next_job = job.next();

      if (job.get<JL_job_ID>() == job_id) {
         DPRINTF("PTF: found job id " sge_u32 "\n", job_id);
         const auto os_job_list = job.get<JL_OS_job_list>();
         auto os_job = os_job_list.front();
         while (os_job) {
            const auto next_os_job = os_job.next();
            if (os_job.get<JO_ja_task_ID>() == ja_task_id) {
               DPRINTF("PTF: found job task id " sge_u32 "\n", ja_task_id);
               psIgnoreJob(ptf_get_osjobid(os_job));
               DPRINTF("PTF: Notify PDC to remove data for osjobid " sge_u32 "\n",
                       os_job.get<JO_OS_job_ID>());
               os_job_list.erase(os_job);
            }
            os_job = next_os_job;
         }

         if (!os_job_list.front()) {
            DPRINTF("PTF: No more os_job_list entries, removing job\n");
            DPRINTF("PTF: Removing job " sge_u32 "\n", job.get<JL_job_ID>());
            job_list.erase(job);
         }
      }
      job = next_job;
   }
   DRETURN_VOID;
}

/** @brief Forget every registered job without collecting usage */
void ptf_unregister_registered_jobs() {
   DENTER(TOP_LAYER);

   for (const auto job : ocs::tom::ConstList<ocs::tom::JL>(ptf_jobs)) {
      for (const auto os_job : job.get<JL_OS_job_list>()) {
         psIgnoreJob(ptf_get_osjobid(os_job));
         DPRINTF("PTF: Notify PDC to remove data for osjobid " sge_u32 "\n", os_job.get<JO_OS_job_ID>());
      }
   }

   ocs::tom::ListPtr<ocs::tom::JL>::from_typed(std::exchange(ptf_jobs, nullptr)).reset();
   DPRINTF("PTF: All jobs unregistered from PTF\n");
   DRETURN_VOID;
}

/** @brief Is the PTF currently adjusting priorities?
 * @return non-zero when it is running
 */
int ptf_is_running() {
   return is_ptf_running;
}

/*--------------------------------------------------------------------
 * ptf_errstr - return PTF error string
 *--------------------------------------------------------------------*/

/** @brief The message belonging to a `PTF_ERROR_*` value
 * @param ptf_error_code the error code
 * @return its message
 */
const char *ptf_errstr(int ptf_error_code) {
   DENTER(TOP_LAYER);

   const char *errmsg = MSG_ERROR_UNKNOWNERRORCODE;

   switch (ptf_error_code) {
   case PTF_ERROR_NONE:
      errmsg = MSG_ERROR_NOERROROCCURED;
      break;

   case PTF_ERROR_INVALID_ARGUMENT:
      errmsg = MSG_ERROR_INVALIDARGUMENT;
      break;

   case PTF_ERROR_JOB_NOT_FOUND:
      errmsg = MSG_ERROR_JOBDOESNOTEXIST;
      break;

   default:
      break;
   }

   DRETURN(errmsg);
}


#endif
