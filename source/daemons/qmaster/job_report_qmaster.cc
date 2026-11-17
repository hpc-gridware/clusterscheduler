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
 * @brief Reconciling an execution host's job reports with what qmaster believes
 */
#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ack.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_REP.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Sort.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_report_execd.h"
#include "execution_states.h"
#include "job_report_qmaster.h"
#include "job_exit.h"
#include "evm/sge_event_master.h"
#include "sge_job_qmaster.h"
#include "sge_give_jobs.h"
#include "reschedule.h"
#include "sge_persistence_qmaster.h"
#include "msg_daemons_common.h"
#include "msg_qmaster.h"

/*
 * When the master task of a tightly integrated job exits
 * we wait for the slave execds to report job finish.
 * Do not wait forever, but only n report intervals.
 * Could be made configurable in qmaster_params.
 */
/** @brief How long a finished master task waits for its slave tasks before it gives up */
#define MAX_MASTER_TASK_FINISH_BEFORE_EXIT 20

static const char *status2str(uint32_t status);

static bool
job_report_is_from_earlier_run(ocs::tom::ConstElement<ocs::tom::JAT> jatep, const char *rhost,
                               const char **current_host);

/** @brief Is a task in one of the states that mean it is on its way, or running?
 *
 * `JWRITTEN` and `JWAITING4OSJID` count as running because the job is already
 * committed to a host even though it has not started yet.
 *
 * @param state the task state
 */
#define is_running(state) (state==JWRITTEN || state==JRUNNING|| state==JWAITING4OSJID)

static const char *
status2str(uint32_t status) {
   const char *s;

   switch (status) {
      case JTRANSFERING:
         s = "JTRANSFERING";
         break;
      case JRUNNING:
         s = "JRUNNING";
         break;
      case JFINISHED:
         s = "JFINISHED";
         break;
      case JIDLE:
         s = "JIDLE";
         break;
      default:
         s = "<unknown>";
         break;
   }

   return s;
}

/**
 * @brief Does a report of the master task come from an earlier run of the task?
 *
 * After qmod -r or the rescheduling of the jobs of an unknown host, the reschedule unknown list
 * of the host of the earlier run makes sure that this run is killed and that its reports are not
 * processed. If the entry is missing, the reports of the earlier run arrive like the reports of
 * the current run. A report of the master task can only belong to the current run if the task
 * runs, or is being delivered, with its master task on the reporting host.
 *
 * A report from the master host of the current run cannot be told apart from a report of an
 * earlier run on the same host.
 *
 * @param jatep the array task (JAT_Type)
 * @param rhost the host which sent the report
 * @param current_host returns the master host of the current run, or MSG_OBJ_NOTRUNNING
 * @return true if the task is pending, or runs with its master task on another host
 */
static bool
job_report_is_from_earlier_run(const ocs::tom::ConstElement<ocs::tom::JAT> jatep, const char *rhost,
                               const char **current_host) {
   DENTER(TOP_LAYER);

   bool ret = false;
   *current_host = MSG_OBJ_NOTRUNNING;
   uint32_t status = ocs::tom::get<JAT_status>(jatep);
   if (status == JIDLE) {
      ret = true;
   } else if (status == JRUNNING || status == JTRANSFERING) {
      const auto master_gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front();
      const char *master_host = nullptr;
      if (master_gdil) {
         master_host = ocs::tom::get<JG_qhostname>(master_gdil);
      }
      if (master_host != nullptr) {
         *current_host = master_host;
         ret = sge_hostcmp(master_host, rhost) != 0;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Record in a ja task who ended one of its tasks
 *
 * When sge_qmaster deletes a job because one of its tasks failed, the one who ended that task
 * (e.g. oom-killer@<host> or execd@<host>, see JR_deleted_by) is the one who ended the job.
 * It is stored in the ja task, so that the accounting records of the job's remaining tasks
 * show it as well. A value which is already stored, e.g. by qdel, is kept.
 *
 * @param jatep the ja task of the job
 * @param jr the job report of the failed task
 */
static void
job_report_take_over_deleted_by(const ocs::tom::Element<ocs::tom::JAT> jatep,
                                const ocs::tom::ConstElement<ocs::tom::JR> jr) {
   DENTER(TOP_LAYER);

   const char *deleted_by = ocs::tom::get<JR_deleted_by>(jr);
   if (deleted_by != nullptr && ja_task_get_deleted_by(jatep) == nullptr) {
      ja_task_set_deleted_by(jatep, deleted_by);
   }

   DRETURN_VOID;
}

/*
 * Protocol between qmaster and execd for tightly integrated parallel jobs:
 * ========================================================================
 * 
 * Job delivery: 
 *    - all slave hosts are notified about the job
 *    - once all slave hosts acknowledged having got the job,
 *      the master task is delivered
 *
 * During job runtime
 *    - master host reports master task usage
 *    - slave tasks get started, slave exec hosts report usage
 *      and slave task exit
 *
 * Job finish:
 *    - master tasks reports finish
 *    - all slave hosts are tagged (in ja_task gdil)
 *    - all slave hosts are notified about job finish (ACK_SIGNAL_SLAVE)
 *      triggeres kill of still running tasks and sending of a final report
 *    - master task report is acknowledged with ACK_JOB_REPORT_RESEND,
 *      triggering flushed resend of the master task finish report
 *    - if slave hosts still report the job running, resend the ACK_SIGNAL_SLAVE
 *    - when slave host sends final report, it is untagged (in ja_task gdil)
 *    - when qmaster gets resent master task final report
 *      - and all slaves are done, treat the job as finished
 *      - not all slaves are done, again trigger resend of master task report
 *    - waiting for all slave hosts to finish is terminated when the master task 
 *      report got received more than MAX_MASTER_TASK_FINISH_BEFORE_EXIT times.
 *      In this case (which happens for example when a slave host is down),
 *      the job is treated as finished.
 */

/**
 * @brief Process a job report from execd
 *
 * Process 'report' containing a job report list from
 * 'commproc' at 'rhost'.
 * The 'pb' may get used to collect requests that will be
 * generated in this process. The caller should reply it
 * to the sender of this job report list if 'pb' remains
 * not empty.
 *
 * @param report the job report
 * @param hep the host for which we got the job report
 * @param rhost name of the remote host having sent the report
 * @param commproc the commproc name
 * @param pb pack buffer for sending answers (ACK)
 * @param monitor monitor
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: process_job_report() is MT safe
 */
void process_job_report(const ocs::tom::Element<ocs::tom::REP> report, const ocs::tom::Element<ocs::tom::EH> hep,
                        char *rhost, [[maybe_unused]] char *commproc, sge_pack_buffer *pb, monitoring_t *monitor,
                        const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const ocs::tom::ConstList<ocs::tom::PE> master_pe_list(*ocs::DataStore::get_master_list(SGE_TYPE_PE));
   // REP_list names no sub-type in the model; a job report holds JR elements, unpacked with their
   // descriptor - from_typed keeps the old tolerance for the data of an execd
   const auto jrl = ocs::tom::List<ocs::tom::JR>::from_typed(ocs::tom::get<REP_list>(report));

   DPRINTF("received job report with %d elements:\n", static_cast<int>(jrl.size()));

   /*
   ** first process job reports of sub tasks to ensure this we put all these 
   ** job reports to the top of the 'jrl' list this is necessary to ensure 
   ** slave tasks get accounted on a shm machine 
   */
   // @todo CS-1026 Improve performance by moving sorting of job reports from the master to the execd's
   if (jrl) {
      jrl.sort<ocs::tom::desc<JR_pe_task_id_str>>();
   }

   // buffer for job / ja-task / pe-task ID string (used in debug and logging messages
   char job_id_buffer[MAX_STRING_SIZE];
   dstring job_id_dstring;
   const char *job_id_string;
   sge_dstring_init(&job_id_dstring, job_id_buffer, sizeof(job_id_buffer));

   /*
   ** now check all job reports found in step 1 are 
   ** removed from job report list
   */
   for (const auto jr : jrl) {
      const char *queue_name;
      uint32_t status = 0;
      int fret;

      // find job/task/pe-task ID and state in report
      uint32_t jobid = ocs::tom::get<JR_job_number>(jr);
      uint32_t jataskid = ocs::tom::get<JR_ja_task_number>(jr);
      const char *pe_task_id_str = ocs::tom::get<JR_pe_task_id_str>(jr);
      uint32_t rstate = ocs::tom::get<JR_state>(jr);

      // find the corresponding objects
      const auto jep = ocs::tom::List<ocs::tom::JB>(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB, true))
                          .find_first<JB_job_number>(jobid);
      ocs::tom::Element<ocs::tom::JAT> jatep = nullptr;
      ocs::tom::Element<ocs::tom::PET> petask = nullptr;
      bool petask_already_existed = false;
      if (jep) {
         jatep = ocs::tom::get<JB_ja_tasks>(jep).find_first<JAT_task_number>(jataskid);

         if (jatep) {
            status = ocs::tom::get<JAT_status>(jatep);

            if (pe_task_id_str != nullptr) {
               petask = ocs::tom::find_first_in_rw<JAT_task_list, PET_id>(jatep, pe_task_id_str);
               if (petask) {
                  petask_already_existed = true;
               }
            }
         }
      }

      /* build a job_id_string once - we need it in many places */
      job_id_string = job_get_id_string(jobid, jataskid, pe_task_id_str, &job_id_dstring);

      /* handle protocol to execd for all jobs which are
         already finished and maybe rescheduled */
      /* RU: */
      fret = skip_restarted_job(hep, jr, jobid, jataskid);
      if (fret > 0) {
         if (fret == 2) {
            pack_ack(pb, ACK_SIGNAL_JOB, jobid, jataskid, nullptr);
         } else if (fret == 3) {
            pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
            // the old run of a task rescheduled with qmod -r has exited, the new run may start
            // (the exit of a pe task on the master host does not end the old run)
            if (jep && jatep && pe_task_id_str == nullptr) {
               reschedule_clear_deferred_start(jep, jatep, gdi_session);
            }
         }
         continue;
      }


      if ((queue_name = ocs::tom::get<JR_queue_name>(jr)) == nullptr) {
         queue_name = MSG_OBJ_UNKNOWNQ;
      }

      // the report of the master task of an earlier run must not be taken as a report of the
      // current run, e.g. as its arrival, as its usage or as its end: kill the earlier run and
      // acknowledge its exit, its exit lets the new run of a task rescheduled with qmod -r start
      const char *current_host = nullptr;
      if (jatep && pe_task_id_str == nullptr
          && (is_running(rstate) || rstate == JEXITING)
          && job_report_is_from_earlier_run(jatep, rhost, &current_host)) {
         if (rstate == JEXITING) {
            WARNING(MSG_JOB_REPORTEXITOLDRUN_SUUSS, rhost, jobid, jataskid, queue_name,
                    current_host);
            pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, nullptr);
            if (jep) {
               reschedule_clear_deferred_start(jep, jatep, gdi_session);
            }
         } else {
            WARNING(MSG_JOB_REPORTRUNOLDRUN_SUUSS, rhost, jobid, jataskid, queue_name,
                    current_host);
            pack_ack(pb, ACK_SIGNAL_JOB, jobid, jataskid, nullptr);
         }
         continue;
      }

      switch (rstate) {
         case JWRITTEN:
         case JRUNNING:
         case JWAITING4OSJID:
            if (jep && jatep) {
               lList *answer_list = nullptr;

               switch (status) {
                  case JTRANSFERING:
                  case JRUNNING:
                     /*
                      * If a ja_task was deleted while the execd was down, we'll
                      * get a "job running" report when the execd starts up again.
                      * The ja_task will be deleted by a timer triggered event
                      * (TYPE_SIGNAL_RESEND_EVENT), but this can take up to one
                      * minute - better send a kill signal immediately.
                      */
                     if (ISSET(ocs::tom::get<JAT_state>(jatep), JDELETED)) {
                        DPRINTF("Received report from " sge_u32"." sge_u32
                                " which is already in \"deleted\" state. "
                                "==> send kill signal\n", jobid, jataskid);

                        pack_ack(pb, ACK_SIGNAL_JOB, jobid, jataskid, nullptr);
                     }

                     if (pe_task_id_str == nullptr) {

                        /* store unscaled usage directly in job */
                        ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<JAT_usage_list>(jatep));

                        /* update jobs scaled usage list */
                        ocs::tom::set<JAT_scaled_usage_list>(jatep,
                                 ocs::tom::get<JAT_usage_list>(jatep).clone("scaled"));
                        scale_usage(ocs::tom::get<EH_usage_scaling_list>(hep),
                                    ocs::tom::get<JAT_previous_usage_list>(jatep),
                                    ocs::tom::get<JAT_scaled_usage_list>(jatep));

                        if (status == JTRANSFERING) { /* got async ack for this job */
                           DPRINTF("--- transfering job " SFN " is running\n", job_id_string);
                           sge_commit_job(jep, jatep, jr, COMMIT_ST_ARRIVED, COMMIT_DEFAULT,
                                          monitor, gdi_session); /* implicitly sending usage to schedd */
                           cancel_job_resend(jobid, jataskid);
                        } else {
                           /* need to generate a job event for new usage
                            * the timestamp should better come from report object
                            */
                           /* jatask usage is not spooled (?) */
                           sge_add_list_event(0, sgeE_JOB_USAGE, jobid, jataskid, nullptr, nullptr,
                                              ocs::tom::get<JB_session>(jep), ocs::tom::get<JAT_scaled_usage_list>(jatep), gdi_session);
                        }
                     } else {
                        /* do we expect a pe task report from this host? */
                        if (ja_task_is_tightly_integrated(jatep, master_pe_list) &&
                            ocs::tom::find_first<JG_qhostname>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep), rhost)) {

                           /* is the task already known (object was created earlier)? */
                           if (!petask) {
                              /* here qmaster hears the first time about this task
                                 and thus adds it to the task list of the appropriate job */
                              DPRINTF("--- task (#%d) " SFN " -> running\n",
                                      static_cast<int>(ocs::tom::get<JAT_task_list>(jatep).size()), job_id_string);
                              petask = ocs::tom::emplace_back_in<JAT_task_list, PET_id>(jatep, pe_task_id_str);
                              ocs::tom::set<PET_status>(petask, JRUNNING);
                              // @todo CS-1026: This should be delivered from execd as part of the report
                              ocs::tom::set<PET_start_time>(petask, sge_get_gmt64());
                              ocs::tom::set<PET_granted_destin_identifier_list>(petask, nullptr);
                              if (const auto ep = ocs::tom::emplace_back_in<PET_granted_destin_identifier_list,
                                                                            JG_qhostname>(petask, rhost);
                                  ep) {
                                 ocs::tom::set<JG_qname>(ep, queue_name);
                              }
                           }

                           /* store unscaled usage directly in sub-task */
                           ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<PET_usage>(petask));

                           /* update task's scaled usage list */
                           ocs::tom::set<PET_scaled_usage>(petask, ocs::tom::get<PET_usage>(petask).clone("scaled"));

                           scale_usage(ocs::tom::get<EH_usage_scaling_list>(hep),
                                       ocs::tom::get<PET_previous_usage>(petask),
                                       ocs::tom::get<PET_scaled_usage>(petask));

                           /* notify scheduler of task usage event */
                           if (!petask_already_existed) {
                              sge_event_spool(&answer_list, 0, sgeE_PETASK_ADD,
                                              jobid, jataskid, pe_task_id_str, nullptr,
                                              ocs::tom::get<JB_session>(jep),
                                              jep, jatep, petask, true, true, gdi_session);
                           } else {
                              sge_add_list_event(0, sgeE_JOB_USAGE, jobid, jataskid, pe_task_id_str,
                                                 nullptr, ocs::tom::get<JB_session>(jep),
                                                 ocs::tom::get<PET_scaled_usage>(petask), gdi_session);
                           }
                           answer_list_output(&answer_list);
                        } else if (ocs::tom::get<JAT_status>(jatep) != JFINISHED) {
                           const char *shouldbe_queue_name;
                           const char *shouldbe_host_name;

                           if (const auto jg = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front(); !jg) {
                              shouldbe_queue_name = MSG_OBJ_NOTRUNNING;
                              shouldbe_host_name = MSG_OBJ_NOTRUNNING;
                           } else {
                              if ((shouldbe_queue_name = ocs::tom::get<JG_qname>(jg)) == nullptr) {
                                 shouldbe_queue_name = MSG_OBJ_UNKNOWN;
                              }
                              if ((shouldbe_host_name = ocs::tom::get<JG_qhostname>(jg)) == nullptr) {
                                 shouldbe_host_name = MSG_OBJ_UNKNOWN;
                              }
                           }
                           /* should never happen */
                           ERROR(MSG_JOB_REPORTEXITQ_SUUSSSSS, rhost, jobid, jataskid, pe_task_id_str ? pe_task_id_str : MSG_MASTER, queue_name, shouldbe_queue_name, shouldbe_host_name, status2str(ocs::tom::get<JAT_status>(jatep)));
                        }
                     }

                     /*
                      * online_usage record: emit a JSONL snapshot of the scaled
                      * usage on every job report from execd, gated by the
                      * reporting_params=online_usage list. Honour the PE's
                      * accounting_summary flag with the same convention as
                      * sge_write_rusage / create_acct_records:
                      *   - pe_task JR + accounting_summary TRUE  -> skip
                      *     (aggregated record will be written on the master JR)
                      *   - pe_task JR + accounting_summary FALSE -> per-pe_task
                      *   - master  JR                            -> ja_task,
                      *     summing pe_task usage when accounting_summary TRUE
                      */
                     if (ocs::ReportingFileWriter::is_online_usage_required()) {
                        const bool do_accounting_summary =
                                pe_do_accounting_summary(ocs::tom::get<JAT_pe_object>(jatep));
                        if (pe_task_id_str != nullptr) {
                           if (petask && !do_accounting_summary) {
                              ocs::ReportingFileWriter::create_online_usage_records(nullptr, jr, jep, jatep,
                                                                                    petask, false);
                           }
                        } else {
                           const ocs::tom::ConstElement<ocs::tom::PET> no_pe_task;
                           ocs::ReportingFileWriter::create_online_usage_records(nullptr, jr, jep, jatep,
                                                                                 no_pe_task, do_accounting_summary);
                        }
                     }

                     /*
                      * once a day write an intermediate usage record to the
                      * reporting file to have correct daily usage reporting with
                      * long running jobs
                      */
                     if (ocs::ReportingFileWriter::is_intermediate_acct_required(jep, jatep, petask)) {
                        /* write intermediate usage */
                        ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, true);

                        /* this action has changed the ja_task/pe_task - spool */
                        if (pe_task_id_str != nullptr) {
                           /* JG: TODO we would need a PETASK_MOD event here!
                            * for spooling only, the ADD event is OK
                            */
                           sge_event_spool(&answer_list, 0, sgeE_PETASK_ADD,
                                           jobid, jataskid, pe_task_id_str, nullptr,
                                           ocs::tom::get<JB_session>(jep),
                                           jep, jatep, petask, false, true, gdi_session);
                        } else {
                           sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD,
                                           jobid, jataskid, nullptr, nullptr,
                                           ocs::tom::get<JB_session>(jep),
                                           jep, jatep, nullptr, true, true, gdi_session);
                        }
                        answer_list_output(&answer_list);
                     }
                     break;
                  default:
                     ERROR(MSG_JOB_REPORTRUNQ_SUUSSU, rhost, jobid, jataskid, pe_task_id_str ? pe_task_id_str : "master", queue_name, status);
                     break;
               }
            } else {
               /* execd reports a running job that is unknown */
               /* signal this job to kill it at execd
                  this can be caused by a qdel -f while
                  execd was unreachable or by deletion of
                  the job in qmasters spool dir + qmaster
                  restart
                  retry is triggered if execd reports
                  this job again as running
               */
               ERROR(MSG_JOB_REPORTRUNFALSE_SUUSS, rhost, jobid, jataskid, pe_task_id_str ? pe_task_id_str : MSG_MASTER, queue_name);
               pack_ack(pb, ACK_SIGNAL_JOB, jobid, jataskid, nullptr);
            }
            break;

         case JSLAVE:
            /*
             * Job report from an execd running slave tasks of a
             * tightly integrated parallel job:
             * - the first report cancels resending of slave job delivery
             * - once all slave jobs have been delivered, the master task is started
             * - during the job runtime, slave job reports are sent in the load report interval
             * - when the master task has finished, a slave job report will be acknowledged
             *   with ACK_SIGNAL_SLAVE
             * - the final slave job report contains a usage list with at least a (dummy)
             *   exit_status - when we receive such a final slave job report, we trigger
             *   exit of the job
             */
            if (!jep || !jatep) {
               DPRINTF("send cleanup request for slave job " SFN "\n", job_id_string);
               pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
            } else {
               /* must be ack (or later slave job report) for slave job */

               /* we lookup the first gdil for the reporting host
                * there might be multiple gdil for one host if the pe job spawns multiple queues
                * but only the first one is tagged
                */
               const auto first_at_host = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)
                                             .find_first<JG_qhostname>(ocs::tom::Host(rhost));
               if (first_at_host) {
                  /* If the job is being delivered, we first deliver it to the slave hosts,
                   * once all slave hosts have been notified, we start the master task.
                   * During this time the job is in status JTRANSFERING.
                   */
                  if (ocs::tom::get<JAT_status>(jatep) == JTRANSFERING) {
                     /* if tag is still 1, this is the ACK for slave notification */
                     if (ocs::tom::get<JG_tag_slave_job>(first_at_host) != 0) {
                        DPRINTF("slave job " SFN " arrived on host %s\n", job_id_string, rhost);
                        ocs::tom::set<JG_tag_slave_job>(first_at_host, 0);

                        /* should trigger a fast delivery of the job to master execd
                         * script but only when all other slaves have also arrived
                         */
                        if (is_pe_master_task_send(jatep)) {
                           /* triggers direct job delivery to master execd */
                           const auto master_gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front();
                           ocs::tom::set<JAT_master_queue>(jatep, ocs::tom::get<JG_qname>(master_gdil));

                           DPRINTF("trigger retry of job delivery to master execd\n");
                           ocs::tom::set<JAT_start_time>(jatep, 0);
                           cancel_job_resend(jobid, jataskid);
                           trigger_job_resend(sge_get_gmt64(), nullptr, jobid, jataskid, 0);
                        }
                     }
                  } else {
                     /* This is a slave execd report while the job is running.
                      * When the master task has finished, the hosts gdil is tagged ==> we send a ACK_SIGNAL_SLAVE
                      * When the slave report contains JR_usage with exit_status, we are done, untag gdil, pack_job_exit
                      */
                     if (ocs::tom::get<JG_tag_slave_job>(first_at_host) != 0) {
                        if (ocs::tom::get<JR_usage>(jr).find_first<UA_name>("exit_status")) {
                           /* the job is done in this slave exec host */
                           ocs::tom::set<JG_tag_slave_job>(first_at_host, 0);
                           DPRINTF("slave job " SFN " exited on host %s\n", job_id_string, rhost);
                           pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
                        } else {
                           DPRINTF("trigger exit of slave job " SFN " on host %s\n", job_id_string, rhost);
                           pack_ack(pb, ACK_SIGNAL_SLAVE, jobid, jataskid, pe_task_id_str);
                        }
                     }
                  }
               } else {
                  /* clear state with regards to slave controlled container
                   * we got a slave report, but GDIL doesn't contain an object for this host!
                   * The job got rescheduled, e.g. with qmod -r, or while the reporting host
                   * was down (reschedule unknown).
                   *
                   * The reschedule unknown list entry of the host keeps the scheduler from
                   * placing the job there again. It may only be removed when the slave
                   * container is gone: pe tasks of the previous run can still be running in
                   * it, and the execd drops a new delivery of the job as long as it knows the
                   * job (CS-3067). So first make the execd kill the remaining pe tasks and send
                   * the final slave report, as at the end of a job. Only the final report,
                   * which contains an exit_status, releases the host.
                   */
                  if (ocs::tom::find_first_in<JR_usage, UA_name>(jr, "exit_status")) {
                     const ocs::tom::ConstList<ocs::tom::EH> ehosts(
                        *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST));
                     const ocs::tom::Element<ocs::tom::EH> host(host_list_locate(ehosts, rhost));
                     update_reschedule_unknown_list_for_job(host, jobid, jataskid);

                     DPRINTF("RU: CLEANUP FOR SLAVE JOB " SFN " on host " SFN "\n", job_id_string,
                             rhost);

                     /* clean up */
                     pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
                  } else {
                     DPRINTF("RU: TRIGGER EXIT OF SLAVE JOB " SFN " on host " SFN "\n",
                             job_id_string, rhost);
                     pack_ack(pb, ACK_SIGNAL_SLAVE, jobid, jataskid, pe_task_id_str);
                  }
               }
            }
            break;
         case JEXITING: {
            int skip_job_exit = 0;

            if (!jep || !jatep || (jep && status == JFINISHED)) {
               /* must be retry of execds job exit
                * or job was deleted using "qdel -f"
                * while execd was down or ...
                */
               DPRINTF("exiting job " SFN " does not exist\n", job_id_string);
            } else {
               /* job exited */
               if (pe_task_id_str == nullptr) {
                  /* store unscaled usage directly in job */
                  ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<JAT_usage_list>(jatep));

                  /* update jobs scaled usage list */
                  ocs::tom::set<JAT_scaled_usage_list>(jatep, ocs::tom::get<JAT_usage_list>(jatep).clone("scaled"));
                  scale_usage(ocs::tom::get<EH_usage_scaling_list>(hep),
                              ocs::tom::get<JAT_previous_usage_list>(jatep),
                              ocs::tom::get<JAT_scaled_usage_list>(jatep));

                  /* additional handling for tightly integrated parallel jobs */
                  if (ja_task_is_tightly_integrated(jatep, master_pe_list)) {
                     /* when we get the first job finish report from the master task of a
                      * tightly integrated parallel job,
                      * we tag all gdil entries (including the master gdil_ep)
                      * first report is when the master task gdil_ep is not yet tagged!
                      */
                     const auto master_gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front();
                     uint32_t master_tag = ocs::tom::get<JG_tag_slave_job>(master_gdil);
                     if (master_tag == 0) {
                        ack_all_slaves(jobid, jataskid, jatep, ACK_SIGNAL_SLAVE);
                        tag_all_host_gdil(jatep);
                        skip_job_exit = 1;
                        pack_ack(pb, ACK_JOB_REPORT_RESEND, jobid, jataskid, pe_task_id_str);
                     } else {
                        /*
                         * skip sge_job_exit() and pack_job_exit() in case there
                         * are still running tasks, since execd resends job exit
                         * we do not wait forever - skip waiting after n master task finish reports
                         */
                        if (master_tag < MAX_MASTER_TASK_FINISH_BEFORE_EXIT) {
                           if (!all_slave_jobs_finished(jatep)) {
                              skip_job_exit = 1;
                              master_tag++;
                              ocs::tom::set<JG_tag_slave_job>(master_gdil, master_tag);
                              pack_ack(pb, ACK_JOB_REPORT_RESEND, jobid, jataskid, pe_task_id_str);
                           }
                        } else {
                           WARNING(MSG_JOB_NOTALLSLAVEEND_S, job_id_string);
                        }
                     }
                  }

                  switch (status) {
                     case JRUNNING:
                     case JTRANSFERING:
                        if (!skip_job_exit) {
                           DPRINTF("--- running job " SFN " is exiting, previous status: " SFN "\n",
                                   job_id_string, (status == JTRANSFERING) ? "transfering" : "running");

                           sge_job_exit(jr, jep, jatep, monitor, gdi_session);
                        } else {
                           uint32_t failed = ocs::tom::get<JR_failed>(jr);

                           if (failed == SSTATE_FAILURE_AFTER_JOB && !ocs::tom::get<JB_checkpoint_name>(jep)) {
                              if (!ISSET(ocs::tom::get<JAT_state>(jatep), JDELETED)) {
                                 // the job is deleted because the master task failed - what ended
                                 // the master task ended the pe tasks as well
                                 job_report_take_over_deleted_by(jatep, jr);
                                 job_mark_job_as_deleted(jep, jatep);
                                 ERROR(MSG_JOB_MASTERTASKFAILED_S, job_id_string);
                              }
                           }
                        }
                        break;
                     case JFINISHED:
                        /* must be retry */
                        skip_job_exit = 1;
                        break;
                     default:
                        ERROR(MSG_JOB_REPORTEXITJ_UUU, jobid, jataskid, status);
                        break;
                  }
               } else {
                  if (ja_task_is_tightly_integrated(jatep, master_pe_list) &&
                      ocs::tom::find_first<JG_qhostname>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep), rhost)) {
                     /*
                      * here we get usage of tasks that ran on slave/master execd's
                      * we store the pe task id of finished pe tasks in the ja task
                      * to prevent multiple handling of pe task finish in case
                      * execd resends job report.
                      */

                     if (ja_task_add_finished_pe_task(jatep, pe_task_id_str)) {
                        bool known_pe_task = true; /* did this pe task show up
                                                   earlier (USAGE report) */

                        if (!petask) {
                           known_pe_task = false;
                           petask = ocs::tom::emplace_back_in<JAT_task_list, PET_id>(jatep, pe_task_id_str);
                           ocs::tom::set<PET_status>(petask, JRUNNING);
                        }

                        /* store unscaled usage directly in sub-task */
                        /* lXchgList(jr, JR_usage, lGetListRef(task, JB_usage_list)); */
                        /* copy list because we need to keep usage in jr for sge_log_dusage() */
                        ocs::tom::set<PET_usage>(petask, ocs::tom::get<JR_usage>(jr).clone());

                        /* update task's scaled usage list */
                        ocs::tom::set<PET_scaled_usage>(petask, ocs::tom::get<PET_usage>(petask).clone("scaled"));
                        scale_usage(ocs::tom::get<EH_usage_scaling_list>(hep),
                                    ocs::tom::get<PET_previous_usage>(petask),
                                    ocs::tom::get<PET_scaled_usage>(petask));


                        if (ocs::tom::get<PET_status>(petask) == JRUNNING ||
                            ocs::tom::get<PET_status>(petask) == JTRANSFERING) {
                           uint32_t failed = ocs::tom::get<JR_failed>(jr);

                           DPRINTF("--- petask " SFN " -> final usage\n", job_id_string);
                           ocs::tom::set<PET_status>(petask, JFINISHED);

                           ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);

                           /* add tasks (scaled) usage to past usage container */
                           {
                              auto container =
                                 ocs::tom::find_first_in_rw<JAT_task_list, PET_id>(jatep, PE_TASK_PAST_USAGE_CONTAINER);
                              if (!container) {
                                 lList *answer_list = nullptr;
                                 container = pe_task_sum_past_usage_list(ocs::tom::get<JAT_task_list>(jatep), petask);
                                 /* usage container will be spooled */
                                 sge_event_spool(&answer_list, 0, sgeE_PETASK_ADD,
                                                 jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER, nullptr,
                                                 ocs::tom::get<JB_session>(jep),
                                                 jep, jatep, container, true, true, gdi_session);
                                 answer_list_output(&answer_list);
                              } else {
                                 lList *answer_list = nullptr;

                                 pe_task_sum_past_usage(container, petask);
                                 /* create list event for the USAGE_CONTAINER */
                                 sge_add_list_event(0, sgeE_JOB_USAGE, jobid, jataskid,
                                                    PE_TASK_PAST_USAGE_CONTAINER, nullptr,
                                                    ocs::tom::get<JB_session>(jep),
                                                    ocs::tom::get<PET_scaled_usage>(container), gdi_session);
                                 /* usage container will be spooled */
                                 /* JG: TODO: it is not really a sgeE_PETASK_ADD,
                                  * but a sgeE_PETASK_MOD. We don't have this event
                                  * yet. For spooling only, the add event will do
                                  */
                                 sge_event_spool(&answer_list, 0, sgeE_PETASK_ADD,
                                                 jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER, nullptr,
                                                 ocs::tom::get<JB_session>(jep),
                                                 jep, jatep, container, false, true, gdi_session);
                                 answer_list_output(&answer_list);
                              }
                           }

                           /* remove pe task from job/jatask */
                           if (known_pe_task) {
                              lList *answer_list = nullptr;
                              sge_event_spool(&answer_list, 0, sgeE_PETASK_DEL,
                                              jobid, jataskid, pe_task_id_str,
                                              nullptr, nullptr, nullptr, nullptr, nullptr,
                                              true, true, gdi_session);
                              answer_list_output(&answer_list);
                           }
                           ocs::tom::get<JAT_task_list>(jatep).erase(petask);
                           petask = nullptr;

                           /* get rid of this job in case a task died from XCPU/XFSZ or
                              exited with a core dump */
                           ocs::tom::ConstElement<ocs::tom::UA> ep;
                           if (failed == SSTATE_FAILURE_AFTER_JOB
                               && (ep = ocs::tom::get<JR_usage>(jr).find_first<UA_name>("signal"))) {
                              uint32_t sge_signo = (uint32_t) ocs::tom::get<UA_value>(ep);

                              switch (sge_signo) {
                                 case SGE_SIGXFSZ:
                                    INFO(MSG_JOB_FILESIZEEXCEED_SSUU, pe_task_id_str, rhost, jobid, jataskid);
                                    break;
                                 case SGE_SIGXCPU:
                                    INFO(MSG_JOB_CPULIMEXCEED_SSUU, pe_task_id_str, rhost, jobid, jataskid);
                                    break;
                                 default:
                                    INFO(MSG_JOB_DIEDTHROUGHSIG_SSUUS, pe_task_id_str, rhost, jobid, jataskid, sge_sig2str( sge_signo));
                                    break;
                              }
                           } else if (failed == 0) {
                              INFO(MSG_JOB_TASKFINISHED_SSUU, pe_task_id_str, rhost, jobid, jataskid);
                           } else {
                              INFO(MSG_JOB_TASKFAILED_SSUUU, pe_task_id_str, rhost, jobid, jataskid, failed);
                           }

                           if (failed == SSTATE_FAILURE_AFTER_JOB && !ocs::tom::get<JB_checkpoint_name>(jep)) {
                              if (!ISSET(ocs::tom::get<JAT_state>(jatep), JDELETED)) {
                                 // the job is deleted because the pe task failed - what ended the
                                 // pe task (e.g. oom-killer@<host>) ended the job as well
                                 job_report_take_over_deleted_by(jatep, jr);
                                 job_mark_job_as_deleted(jep, jatep);
                                 ERROR(MSG_JOB_JOBTASKFAILED_S, job_id_string);
                              }
                           }
                        }
                     }
                  } else if (status != JFINISHED) {
                     const char *shouldbe_queue_name;
                     const char *shouldbe_host_name;

                     if (const auto jg = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep).front(); !jg) {
                        shouldbe_queue_name = MSG_OBJ_NOTRUNNING;
                        shouldbe_host_name = MSG_OBJ_NOTRUNNING;
                     } else {
                        if ((shouldbe_queue_name = ocs::tom::get<JG_qname>(jg)) == nullptr) {
                           shouldbe_queue_name = MSG_OBJ_UNKNOWN;
                        }
                        if ((shouldbe_host_name = ocs::tom::get<JG_qhostname>(jg)) == nullptr) {
                           shouldbe_host_name = MSG_OBJ_UNKNOWN;
                        }
                     }
                     /* should never happen */
                     ERROR(MSG_JOB_REPORTEXITQ_SUUSSSSS, rhost, jobid, jataskid, pe_task_id_str ? pe_task_id_str : MSG_MASTER, queue_name, shouldbe_queue_name, shouldbe_host_name, status2str(ocs::tom::get<JAT_status>(jatep)));
                  }
               }
            }
            /* pack ack to enable execd cleaning up */
            if (!skip_job_exit) {
               pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
            }
         }
            break;
         default:
            ERROR(MSG_EXECD_UNKNOWNJ_SUUSUS, rhost, jobid, jataskid, pe_task_id_str ? pe_task_id_str : MSG_MASTER, rstate, queue_name);


            pack_ack(pb, ACK_JOB_EXIT, jobid, jataskid, pe_task_id_str);
            break;
      }
   }


   DRETURN_VOID;
}

