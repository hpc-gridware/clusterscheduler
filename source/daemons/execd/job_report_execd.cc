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
 * @brief The job reports waiting to be acknowledged by qmaster
 */
#include <cstdio>
#include <cstdlib>
#include <cfloat>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_hostname.h"
#include "uti/ocs_TerminationManager.h"

#include "tom/ocs_Types.h"
#include "tom/ocs_PackCore.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_qinstance.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ACK.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "job_report_execd.h"
#include "ocs_execd_systemd.h"
#include "reaper_execd.h"
#include "execd_signal_queue.h"
#include "load_avg.h"

#include "msg_common.h"
#include "msg_execd.h"
#include "sge_conf.h"

/** @brief The job reports waiting to be sent to qmaster (`JR`)
 *
 * A report stays in this list until qmaster acknowledges it, so that a report
 * lost to a restart or a network failure is sent again rather than dropped -
 * which is what keeps a finished job from being forgotten. The list lives as
 * long as the process and is read and written through non-owning handles.
 */
lList *jr_list = nullptr;
static bool flush_jr = false;
static int check_queue_limits = 0;

/** @brief Ask for the job reports to be sent at the next opportunity
 * @param value true to request a flush
 */
void sge_set_flush_jr_flag(bool value) {
   flush_jr = value;
}

/** @brief Is a job report flush pending?
 * @return true when one is
 */
bool sge_get_flush_jr_flag() {
   return flush_jr;
}

/**
 * @brief Mark one job report as worth sending immediately
 *
 * A state change the user is waiting on - a job starting or finishing -
 * should not wait for the next periodic report.
 *
 * @param jr the job report, or an empty handle
 */
void flush_job_report(const ocs::tom::Element<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   if (jr) {
      jr.set<JR_flush>(true);
      sge_set_flush_jr_flag(true);
   }

   DRETURN_VOID;
}

/**
 * @brief Start a job report for a job, task or PE task
 *
 * The report is appended to #jr_list, which is created with the first report.
 * A report that exists already for the task is not looked for.
 *
 * @param jobid the job
 * @param jataskid the array task
 * @param petaskid the PE task, or nullptr
 * @param jep the job, or an empty handle; when given, the report is initialized from it and from its array
 *        task and PE task
 * @return the new report
 */
ocs::tom::Element<ocs::tom::JR>
add_job_report(const uint32_t jobid, const uint32_t jataskid, const char *petaskid,
               const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);

   if (jr_list == nullptr) {
      // the list lives as long as the process
      jr_list = ocs::tom::ListPtr<ocs::tom::JR>::create("job report list").release();
   }
   const ocs::tom::List<ocs::tom::JR> job_reports(jr_list);

   auto new_jr = ocs::tom::ElementPtr<ocs::tom::JR>::create();

   new_jr->set<JR_job_number>(jobid);
   new_jr->set<JR_ja_task_number>(jataskid);
   if (petaskid != nullptr) {
      new_jr->set<JR_pe_task_id_str>(petaskid);
   }

   const auto jr = job_reports.push_back(std::move(new_jr));
   DPRINTF("adding job report for " sge_u32 "." sge_u32 "\n", jobid, jataskid);

   if (jep) {
      const ocs::tom::ConstElement<ocs::tom::JAT> jatep(job_search_task(jep, nullptr, jataskid));
      if (jatep) {
         ocs::tom::ConstElement<ocs::tom::PET> petep;
         if (petaskid != nullptr) {
            petep = ocs::tom::ConstElement<ocs::tom::PET>(ja_task_search_pe_task(jatep, petaskid));
         }
         job_report_init_from_job(jr, jep, jatep, petep);
      }
   }

   DRETURN(jr);
}

/**
 * @brief The pending report for a job, task or PE task
 *
 * Without a PE task the first report of the array task is found, which may
 * also be the report of one of its PE tasks.
 *
 * @param job_id the job
 * @param ja_task_id the array task
 * @param pe_task_id the PE task, or nullptr
 * @return the report, or an empty handle when there is none
 */
ocs::tom::Element<ocs::tom::JR>
get_job_report(const uint32_t job_id, const uint32_t ja_task_id, const char *pe_task_id) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JR> ret;
   for (const auto jr : ocs::tom::List<ocs::tom::JR>(jr_list).find_all<JR_job_number>(job_id)) {
      if (jr.get<JR_ja_task_number>() == ja_task_id) {
         if (pe_task_id == nullptr) {
            ret = jr;
            break;
         } else {
            if (sge_strnullcmp(pe_task_id, jr.get<JR_pe_task_id_str>()) == 0) {
               ret = jr;
               break;
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Drop one job report, once qmaster has acknowledged it
 * @param jr the report, an element of #jr_list; the handle dangles afterwards
 */
void del_job_report(const ocs::tom::Element<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   ocs::tom::List<ocs::tom::JR>(jr_list).erase(jr);

   DRETURN_VOID;
}

/**
 * @brief Drop every report belonging to one job task
 * @param jobid the job
 * @param jataskid the array task
 */
void cleanup_job_report(const uint32_t jobid, const uint32_t jataskid) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::JR> job_reports(jr_list);
   const void *iterator = nullptr;

   /* get rid of job reports for all slave tasks */
   // the next report is fetched before the current one is removed
   auto jr_next = ocs::tom::find_first_rw<JR_job_number>(job_reports, jobid, &iterator);
   while (jr_next) {
      const auto jr = jr_next;
      jr_next = ocs::tom::find_next_rw<JR_job_number>(job_reports, jobid, &iterator);
      if (jr.get<JR_ja_task_number>() == jataskid) {
         const char *s = jr.get<JR_pe_task_id_str>();

         DPRINTF("!!!! removing jobreport for " sge_u32 "." sge_u32 " task %s !!!!\n", jobid, jataskid,
                 s?s:"master");
         job_reports.erase(jr);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Add one usage figure to a job report
 *
 * Adds the attribute 'name' to the usage list of a job report, or updates it.
 * If there is no 'val_as_str', or it cannot be converted into a number, 'val'
 * is used as the value for UA_value.
 *
 * @param jr the report, or an empty handle
 * @param name the usage attribute
 * @param val_as_str its value as text, for the attributes reported that way, or nullptr
 * @param val its value as a number
 * @return 0 on success, -1 on error
 */
int add_usage(const ocs::tom::Element<ocs::tom::JR> jr, const char *name, const char *val_as_str, double val) {
   DENTER(TOP_LAYER);


   if (!jr || !name) {
      DRETURN(-1);
   }

   /* check if we already have a usage value with this name */
   ocs::tom::Element<ocs::tom::UA> usage = jr.find_first_in_rw<JR_usage, UA_name>(name);
   if (!usage) {
      if (!(usage = jr.emplace_back_in<JR_usage, UA_name>(name))) {
         DRETURN(-1);
      }
   }

   if (val_as_str) {
      char *p;
      double parsed = strtod(val_as_str, &p);
      if (p==val_as_str) {
         ERROR(MSG_PARSE_USAGEATTR_SSU, val_as_str, name, jr.get<JR_job_number>()); /* use default value */
         usage.set<UA_value>(val);
         DRETURN(-1);
      }
      val = parsed;
   }

   usage.set<UA_value>(val);

   DRETURN(0);
}

/**
 * @brief Update job report value from usage list
 *
 * Updates a single usage value in a job report from a usage list.
 *
 * @param jr the job report to update
 * @param usage_list the usage, or an empty handle
 * @param name the name of the usage attribute to update
 */
static void
job_report_update_value_from_usage_list(const ocs::tom::Element<ocs::tom::JR> jr,
                                        const ocs::tom::ConstList<ocs::tom::UA> usage_list, const char *name) {
   DENTER(TOP_LAYER);

   const auto usage = usage_list.find_first<UA_name>(name);
   if (usage) {
      add_usage(jr, name, nullptr, usage.get<UA_value>());
   }

   DRETURN_VOID;
}

/**
 * @brief Update job report from usage list
 *
 * Update all default usage values in a job report from a usage list.
 *
 * @param jr the job report to update
 * @param usage_list the usage, or an empty handle
 */
void
job_report_update_from_usage_list(const ocs::tom::Element<ocs::tom::JR> jr,
                                  const ocs::tom::ConstList<ocs::tom::UA> usage_list) {
   DENTER(TOP_LAYER);

   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_CPU);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_MEM);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_IO);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_IOOPS);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_IOW);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_VMEM);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_MAXVMEM);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_RSS);
   job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_MAXRSS);
   if (mconf_get_enable_mem_details()) {
      job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_PSS);
      job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_MAXPSS);
      job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_PMEM);
      job_report_update_value_from_usage_list(jr, usage_list, USAGE_ATTR_SMEM);
   }

   DRETURN_VOID;
}

/**
 * @brief Handle a qmaster acknowledgement of the reports it received
 *
 * These requests are triggered by our job report list that is sent
 * periodically. They are responses of qmaster in different cases, but they
 * get sent as one message, to save communication.
 *
 * This is what lets a report be dropped from #jr_list; anything not
 * acknowledged is sent again.
 *
 * @param aMsg the received message
 * @return 0, the typical dispatcher service function return value
 */
int do_ack(ocs::gdi::ClientServerBase::struct_msg_t *aMsg) {
   DENTER(TOP_LAYER);

   uint32_t jobid, jataskid;
   ocs::tom::Element<ocs::tom::JR> jr;
   const char *pe_task_id_str;

   DPRINTF("------- GOT ACK'S ---------\n");

   /* we get a bunch of ack's */
   while (ocs::tom::buffer_has_unread(&(aMsg->buf))) {
      ocs::tom::ElementPtr<ocs::tom::ACK> ack;
      try {
         ack = ocs::tom::ElementPtr<ocs::tom::ACK>::unpack(aMsg->buf);
      } catch (const ocs::tom::PackException &) {
         // the bytes cut off or no memory, where cull_unpack_elem() returned an error
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      } catch (const ocs::tom::FieldException &) {
         // an element of another type than ACK: refused like a message that cannot be unpacked
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      }

      switch (ack->get<ACK_type>()) {

         case ACK_JOB_EXIT:
/*
**          This is the answer of qmaster if we report a job as exiting
**          - job gets removed from job report list and from job list
**          - job gets cleaned from file system
**          - retry is triggered by next job report sent to qmaster
**            containing this job as "exiting"
*/
            jobid = ack->get<ACK_id>();
            jataskid = ack->get<ACK_id2>();
            pe_task_id_str = ack->get<ACK_str>();

            DPRINTF("remove exiting job " sge_u32 "." sge_u32 " %s\n",
                    jobid, jataskid, pe_task_id_str?pe_task_id_str:"");

#if defined(OCS_WITH_SYSTEMD)
            if (pe_task_id_str == nullptr) {
               // remove the tight pe job slave container (if any) job from systemd
               ocs::execd::execd_delete_tight_pe_slice(jobid, jataskid, pe_task_id_str);
            }
#endif

            if ((jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
               remove_acked_job_exit(jobid, jataskid, pe_task_id_str, jr);
            } else {
               DPRINTF("acknowledged job " sge_u32 "." sge_u32" not found\n", jobid, jataskid);
            }

            break;

         case ACK_JOB_REPORT_RESEND:
/*
**          sent by qmaster instead of ACK_JOB_EXIT to make execd
**          resend the job report
*/
            jobid = ack->get<ACK_id>();
            jataskid = ack->get<ACK_id2>();
            pe_task_id_str = ack->get<ACK_str>();

            DPRINTF("resending report for exiting job " sge_u32 "/" sge_u32"/%s\n", jobid, jataskid, pe_task_id_str?pe_task_id_str:"");

            if ((jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
               flush_job_report(jr);
            } else {
               DPRINTF("job requested to resend " sge_u32 "." sge_u32" not found\n", jobid, jataskid);
            }

            break;

         case ACK_SIGNAL_JOB:
/*
**          This is the answer of qmaster
**          if we report a job as running
**          while qmaster does not know
**          this job
**          - no "unknown job" is added
**            to the job report
**          - retry is triggered by next
**            job report sent to qmaster
**            containing this job as
**            "running"
*/
            {
               uint32_t signo  = SGE_SIGKILL;

               jobid = ack->get<ACK_id>();
               jataskid = ack->get<ACK_id2>();

               if (signal_job(jobid, jataskid, signo)) {
                  auto jr = get_job_report(jobid, jataskid, nullptr);
                  if (!jr) {
                     jr = add_job_report(jobid, jataskid, nullptr, nullptr);
                  }
                  if (jr) {
                     remove_acked_job_exit(jobid, jataskid, nullptr, jr);
                  }
                  job_unknown(jobid, jataskid, nullptr);
               }
            }
            break;

         case ACK_LOAD_REPORT:
            execd_merge_load_report(ack->get<ACK_id>());
            break;

/*
 * This is the answer of qmaster
 * when we report a slave job,
 * and the master task of this slave job has finished.
 * qmaster expects us to send a final slave report
 * (having JR_usage with at least a pseudo exit_status)
 * once all pe_tasks have finished.
 */
         case ACK_SIGNAL_SLAVE:
            jobid = ack->get<ACK_id>();
            jataskid = ack->get<ACK_id2>();

            execd_slave_job_exit(jobid, jataskid);
            break;

         default:
            ERROR(MSG_COM_ACK_UNKNOWN1_S, aMsg->snd_host);
            break;
      }

      ack.reset();
      /*
       * delete job's spooling directory may take some time
       * (NFS directory case). We have to trigger communication
       * to be sure not to get communication timeouts when we have
       * to delete lot's of jobs at once. ( The trigger is done
       * NOT synchron which means that the commlib will return
       * when there is nothing to do
       */
      // @todo doesn't sge_execd use the multithreaded commlib? Why then call cl_commlib_trigger?
      //       with synchron 0 it does nothing
      cl_commlib_trigger(cl_com_get_handle("execd", 1) ,0);
   }

   DRETURN(0);
}

/**
 * @brief Count a limit of a queue instance in or out of the queue limits to be checked
 *
 * This function checks if the queue limits for a job are set and updates the
 * global check_queue_limits counter accordingly.
 *
 * @tparam F The limit field of the queue instance (e.g., QU_h_cpu).
 * @param queue The queue instance to check.
 * @param type The type of limit to check (e.g., TYPE_TIM, TYPE_MEM).
 * @param increase If true, increments the check_queue_limits counter; otherwise, decrements it.
 *
 * @return true if a limit was found and processed, false otherwise.
 */
template <auto F>
static bool
count_queue_limits(const ocs::tom::ConstElement<ocs::tom::QU> queue, const ocs::CEntry::Type type,
                   const bool increase) {
   DENTER(TOP_LAYER);

   // check_queue_limits is a global variable that is used to determine
   // whether we need to check queue limits or not.
   // @todo: store this once in the ja_task?
   double lim{};
   bool found_limit = false;
   parse_ulong_val(&lim, nullptr, type, queue.get<F>(), nullptr, 0);
   if (lim != DBL_MAX) {
      if (increase) {
         check_queue_limits++;
      } else {
         check_queue_limits--;
      }
      found_limit = true;
   }

   DRETURN(found_limit);
}

/**
 * @brief Count a job in or out of the queues whose limits have to be watched
 *
 * Only queues holding a job with a resource limit need to be checked each
 * interval, so the daemon keeps a count rather than scanning all of them.
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param jep the job
 * @param increase true when the job starts, false when it ends
 */
void modify_queue_limits_flag_for_job(const char *qualified_hostname, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                                      const bool increase) {
   DENTER(TOP_LAYER);

   for (const auto jatep : jep.get<JB_ja_tasks>()) {
      const auto gdil = jatep.get<JAT_granted_destin_identifier_list>();
      for (const auto gdil_ep : gdil.find_all<JG_qhostname>(ocs::tom::Host(qualified_hostname))) {
         const ocs::tom::ConstElement<ocs::tom::QU> q = gdil_ep.get_as<JG_queue, ocs::tom::QU>();
         if (!q) {
            // this should never happen, but if it does, we have to skip this gdil_ep
            CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, "gdil_ep->JG_queue");
#if defined (ENABLE_DEBUG_CHECKS)
            ocs::TerminationManager::trigger_abort();
#endif
            continue;
         }

         if (count_queue_limits<QU_s_cpu>(q, ocs::CEntry::Type::TIME, increase) ||
             count_queue_limits<QU_h_cpu>(q, ocs::CEntry::Type::TIME, increase) ||
             count_queue_limits<QU_s_rss>(q, ocs::CEntry::Type::MEM, increase) ||
             count_queue_limits<QU_h_rss>(q, ocs::CEntry::Type::MEM, increase) ||
             count_queue_limits<QU_s_vmem>(q, ocs::CEntry::Type::MEM, increase) ||
             count_queue_limits<QU_h_vmem>(q, ocs::CEntry::Type::MEM, increase)) {
            break;
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Whether any running job is subject to a queue resource limit
 * @return non-zero when the limits have to be enforced this interval
 */
bool check_for_queue_limits() {
   bool ret = false;

   if (check_queue_limits != 0) {
      ret = true;
   }

   return ret;
}
