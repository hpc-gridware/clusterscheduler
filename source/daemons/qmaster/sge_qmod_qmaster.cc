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
 * @brief Handling qmod: suspending, resuming, enabling and rescheduling
 */
#include <cstdlib>
#include <cstring>
#include <climits>
#include <fnmatch.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_pe_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "sge_qmod_qmaster.h"
#include "sge_reconnect_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_give_jobs.h"
#include "symbols.h"
#include "reschedule.h"
#include "sge_qinstance_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "tom/ocs_PackCore.h"


/*-------------------------------------------------------------------------*/
static void
signal_slave_jobs_in_queue(int how, ocs::tom::ConstElement<ocs::tom::QU> qep, monitoring_t *monitor);

static void
signal_slave_tasks_of_job(int how, ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatep,
                          monitoring_t *monitor);

static int
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::QU> qep,
                       uint32_t action, uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor);

static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::JB> jep,
                     ocs::tom::Element<ocs::tom::JAT> jatep, uint32_t task_id, uint32_t action, uint32_t force,
                     ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor);

static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::QU> qep,
                     uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, int isoperator, int isowner,
                     monitoring_t *monitor);

static int
qmod_queue_clean(const ocs::gdi::Packet *packet, ocs::tom::ConstElement<ocs::tom::QU> qep, uint32_t force,
                 ocs::tom::ListPtr<ocs::tom::AN> *answer, int isoperator, int isowner, monitoring_t *monitor);

static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::JB> jep,
                 ocs::tom::Element<ocs::tom::JAT> jatep, ocs::tom::Element<ocs::tom::QU> queueep, uint32_t force,
                 ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor);

static void
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::JB> jep,
                   ocs::tom::Element<ocs::tom::JAT> jatep, ocs::tom::Element<ocs::tom::QU> queueep, uint32_t force,
                   ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor);

static void
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::JB> jep,
                    ocs::tom::Element<ocs::tom::JAT> jatep, ocs::tom::Element<ocs::tom::QU> queueep, uint32_t force,
                    ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor);

/*-------------------------------------------------------------------------*/

/** @brief Handle a `qmod` request: suspend, resume, enable, disable, reschedule
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 */
void
sge_gdi_qmod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   bool found;
   uint32_t jobid;
   uint32_t start = 0, end = 0, step = 0;
   int alltasks;
   dstring cqueue_buffer = DSTRING_INIT;
   dstring hostname_buffer = DSTRING_INIT;
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
   // the request data is structure checked in the listener (CS-3125)
   const ocs::tom::List<ocs::tom::ID> data_list(task->data_list);

   /*
   ** loop over the ids and change queue or job state and signal them
   ** if necessary
   ** the job name branch below appends to the list while it is walked, the walk then visits the new
   ** elements too, exactly as before
   */
   for (const auto dep : data_list) {
      bool found_something = true;
      uint32_t id_action = dep.get<ID_action>();

      found = false;

      // CS-2144: IJS session-reconnect path bypasses the qmod queue/job dispatch.
      // The client encodes the request in ID_str as "<job_id>:<client_host>:<client_port>".
      // On success an answer of STATUS_OK is added with text "RECONNECT_OK <token> <exec_host>",
      // which the qrsh -reconnect client parses to obtain the rendezvous parameters.
      if ((id_action & ~JOB_DO_ACTION) == QI_DO_RECONNECT) {
         const char *spec = dep.get<ID_str>();
         char spec_copy[1024];
         snprintf(spec_copy, sizeof(spec_copy), "%s", spec != nullptr ? spec : "");

         char *job_id_str = spec_copy;
         char *host_str = strchr(job_id_str, ':');
         char *port_str = nullptr;
         if (host_str != nullptr) {
            *host_str++ = '\0';
            port_str = strchr(host_str, ':');
            if (port_str != nullptr) {
               *port_str++ = '\0';
            }
         }

         if (host_str == nullptr || port_str == nullptr || *job_id_str == '\0' ||
             *host_str == '\0' || *port_str == '\0') {
            answer_list_add_sprintf(&alp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    "QI_DO_RECONNECT: malformed spec, expected <job_id>:<host>:<port>");
            continue;
         }

         uint32_t reconnect_job_id = (uint32_t)strtoul(job_id_str, nullptr, 10);
         int reconnect_port = (int)strtol(port_str, nullptr, 10);

         // CS-2206: the reconnecting client's TLS cert (PEM) rides in
         // ID_user_list as a single ST_name element (nullptr in non-TLS mode).
         const char *reconnect_cred = nullptr;
         ocs::tom::ConstElement<ocs::tom::ST> cred_ep = dep.get<ID_user_list>().front();
         if (cred_ep != nullptr) {
            reconnect_cred = cred_ep.get<ST_name>();
         }

         char token[128] = {0};
         char exec_host[CL_MAXHOSTNAMELEN] = {0};
         if (qmaster_handle_reconnect_request(reconnect_job_id, packet->user,
                                              host_str, reconnect_port,
                                              reconnect_cred,
                                              token, sizeof(token),
                                              exec_host, sizeof(exec_host),
                                              &alp) == 0) {
            answer_list_add_sprintf(&alp, STATUS_OK, ANSWER_QUALITY_INFO,
                                    "RECONNECT_OK %s %s", token, exec_host);
         }
         // qmaster_handle_reconnect_request already populated alp on failure.
         continue;
      }

      if ((id_action & JOB_DO_ACTION) == 0) {
         ocs::tom::ListPtr<ocs::tom::QR> qref_list;
         ocs::tom::ListPtr<ocs::tom::QR> tmp_list;

         qref_list_add(&qref_list, nullptr, dep.get<ID_str>());
         qref_list_resolve_hostname(qref_list.get());
         qref_list_resolve(qref_list.get(), nullptr, tmp_list,
                           &found_something, master_cqueue_list,
                           master_hgroup_list,
                           true, true);
         if (found_something) {
            id_action = (id_action & (~QUEUE_DO_ACTION));

            for (const auto qref : tmp_list.get()) {
               const char *full_name = nullptr;
               const char *cqueue_name = nullptr;
               const char *hostname = nullptr;

               full_name = qref.get<QR_name>();
               if (!cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
                  continue;
               }
               cqueue_name = sge_dstring_get_string(&cqueue_buffer);
               hostname = sge_dstring_get_string(&hostname_buffer);
               const ocs::tom::Element<ocs::tom::CQ> cqueue = master_cqueue_list.find_first<CQ_name>(cqueue_name);
               const ocs::tom::List<ocs::tom::QU> qinstance_list = cqueue.get<CQ_qinstances>();
               const auto qinstance = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(hostname));

               sge_change_queue_state(packet, task, qinstance, id_action, dep.get<ID_force>(), &alp, monitor);
               found = true;
            }
         }
      }
      if (!found) {
         bool is_jobName_suport = false;
         u_long action = dep.get<ID_action>();
         if ((action & JOB_DO_ACTION) > 0 &&
             (action & QUEUE_DO_ACTION) == 0) {
            action = (action & (~JOB_DO_ACTION));
            is_jobName_suport = true;
         }

         /*
         ** We found no queue so look for a job. This only makes sense for
         ** suspend, unsuspend, reschedule, and clear
         */
         if (sge_strisint(dep.get<ID_str>()) &&
             (action == QI_DO_SUSPEND ||
              action == QI_DO_RESCHEDULE ||
              action == QI_DO_CLEARERROR ||
              action == QI_DO_UNSUSPEND)) {
            jobid = strtol(dep.get<ID_str>(), nullptr, 10);

            ocs::tom::ConstElement<ocs::tom::RN> rn = dep.get<ID_ja_structure>().front();
            if (rn) {
               start = rn.get<RN_min>();
               if (start) {
                  end = rn.get<RN_max>();
                  step = rn.get<RN_step>();
                  if (!step)
                     step = 1;
                  alltasks = 0;
               } else {
                  start = 1;
                  end = (uint32_t) LONG_MAX;
                  step = 1;
                  alltasks = 1;
               }
               if (start > end)
                  end = start;

            } else {
               alltasks = 1;
            }

            // read access as master_list() gives it; the job found is changed
            ocs::tom::Element<ocs::tom::JB> job =
               ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true).find_first<JB_job_number>(jobid);
            if (job != nullptr) {
               ocs::tom::Element<ocs::tom::JAT> jatask = job.get<JB_ja_tasks>().front();
               ocs::tom::Element<ocs::tom::JAT> tmp_task;

               while ((tmp_task = jatask)) {
                  uint32_t task_number;

                  jatask = tmp_task.next();
                  task_number = tmp_task.get<JAT_task_number>();
                  if ((task_number >= start && task_number <= end &&
                       ((task_number - start) % step) == 0) || alltasks) {
                     DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, task_number);

                     /* this specifies no queue, so lets probe for a job */
                     /* change state of job: */
                     sge_change_job_state(packet, task, job, tmp_task, 0, action, dep.get<ID_force>(), &alp, monitor);
                     found = true;
                  }
               }

               /* create more precise GDI answers also for pending jobs/tasks and jobs/tasks in hold state
                  When the operation is to be applied on the whole job array but no task is enrolled so far
                  (i.e. not found) only one single GDI answer is created. Otherwise one message is created
                  per task */
               if (alltasks && job_is_array(job)) {
                  if (!found) {
                     sge_change_job_state(packet, task, job, nullptr, 0, action, dep.get<ID_force>(), &alp, monitor);
                     found = true;
                  }
               } else {
                  uint32_t min, max, step;
                  uint32_t taskid;

                  /* handle all pending tasks */
                  for (const auto range : job.get<JB_ja_n_h_ids>()) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, dep.get<ID_force>(), &alp,
                                                monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in user hold */
                  for (const auto range : job.get<JB_ja_u_h_ids>()) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, dep.get<ID_force>(), &alp,
                                                monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in system hold that are not in user hold */
                  for (const auto range : job.get<JB_ja_s_h_ids>()) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(job.get<JB_ja_u_h_ids>(), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, dep.get<ID_force>(), &alp,
                                                monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in operator hold that are not in user hold or system hold */
                  for (const auto range : job.get<JB_ja_o_h_ids>()) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(job.get<JB_ja_u_h_ids>(), taskid) ||
                            range_list_is_id_within(job.get<JB_ja_s_h_ids>(), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, dep.get<ID_force>(), &alp,
                                                monitor);
                           found = true;
                        }
                     }
                  }
               }
            }
         }
            /* job name or pattern was submitted */
         else if (is_jobName_suport && (
                 action == QI_DO_SUSPEND ||
                 action == QI_DO_RESCHEDULE ||
                 action == QI_DO_CLEARERROR ||
                 action == QI_DO_UNSUSPEND)) {

            const char *job_name = dep.get<ID_str>();
            for (const auto job : master_job_list) {
               if (!fnmatch(job_name, job.get<JB_job_name>(), 0)) {
                  char job_id[40];
                  auto mod = dep.clone();
                  snprintf(job_id, sizeof(job_id), sge_u32, job.get<JB_job_number>());
                  mod->set<ID_str>(job_id);
                  data_list.push_back(std::move(mod));
                  found = true;
               }
            }
         } else {
            /* job id invalid or action invalid for jobs */

         }
      }

      if (!found) {
         u_long action = dep.get<ID_action>();
/*
         if ((action & JOB_DO_ACTION)) {
            action = action - JOB_DO_ACTION;
         }
*/
         /*
         ** If the action is QI_DO_UNSUSPEND or QI_DO_SUSPEND,
         ** 'invalid queue or job' will be printed,
         ** otherwise 'invalid queue' will be printed, because these actions
         ** are not suitable for jobs.
         */
         if ((action & QUEUE_DO_ACTION) == 0 && (
                 (action & JOB_DO_ACTION) != 0 ||
                 (action & QI_DO_SUSPEND) != 0 ||
                 (action & QI_DO_UNSUSPEND) != 0 ||
                 (action & QI_DO_CLEAN) != 0)) {
            ERROR(MSG_QUEUE_INVALIDQORJOB_S, dep.get<ID_str>());
         } else {
            ERROR(MSG_QUEUE_INVALIDQ_S, dep.get<ID_str>());
         }
         answer_list_add(&alp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      }
   }

   sge_dstring_free(&cqueue_buffer);
   sge_dstring_free(&hostname_buffer);

   task->answer_list = std::move(alp);

   DRETURN_VOID;
}

/** @brief Apply a `qmod` queue action to one queue instance
 *
 * Checks the permission (owner, or operator on an admin host), executes the
 * action and triggers the slotwise subordination and orphan cleanup that
 * follow from it.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param qep the queue instance, may be empty
 * @param action the QI_DO_* action
 * @param force whether the action is forced
 * @param answer receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @return 0 on success, -1 on error
 */
static int
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::QU> qep,
                       const uint32_t action, const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer,
                       monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int result = 0;
   bool isowner = qinstance_is_owner(packet, qep);
   bool isoperator = manop_is_operator(packet);

   if (!isowner && !isoperator) {
      ERROR(MSG_QUEUE_NOCHANGEQPERMS_SS, packet->user, qep.get<QU_full_name>());
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   // if we are not owner but operator (or manager), we need to be on an admin host
   if (!isowner) {
      bool is_admin_host = host_is_admin_host(packet->host);
      if (!is_admin_host) {
         ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }
   }

   if (qep == nullptr) {
      ERROR(MSG_NULLELEMENTPASSEDTO_S, __func__);
      DRETURN(-1);
   }

   switch (action) {
      case QI_DO_CLEARERROR:
      case QI_DO_ENABLE:
      case QI_DO_DISABLE:
      case QI_DO_SUSPEND:
      case QI_DO_UNSUSPEND:
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
         case QI_DO_SETERROR:
         case QI_DO_SETORPHANED:
         case QI_DO_CLEARORPHANED:
         case QI_DO_SETUNKNOWN:
         case QI_DO_CLEARUNKNOWN:
         case QI_DO_SETAMBIGUOUS:
         case QI_DO_CLEARAMBIGUOUS:
#endif
         result = qinstance_change_state_on_command(qep, answer, action, force ? true : false, packet->user, packet->host,
                                                    isoperator, isowner, monitor, packet->gdi_session) ? 0 : -1;
         break;
      case QI_DO_CLEAN:
         result = qmod_queue_clean(packet, qep, force, answer, isoperator, isowner, monitor);
         break;

      case QI_DO_RESCHEDULE:
         result = qmod_queue_weakclean(packet, task, qep, force, answer, isoperator, isowner, monitor);
         break;
      default:
         INFO(MSG_LOG_QUNKNOWNQMODCMD_U, action);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;
   }

   switch (action) {
      case QI_DO_SUSPEND:
         /* When the queue gets suspended, the other queues can possibly unsuspend
          * tasks that where suspended by slotwise subordination.
          * Therefore we call the function with "false" (=unsuspend)
          */
         do_slotwise_x_on_subordinate_check(qep, false, false, monitor);
         break;
      case QI_DO_UNSUSPEND:
         /* This queue gets unsuspended, possibly tasks in other queues have to
          * be suspended because of slotwise subordination.
          * Therefore we call the function with "true" (=suspend)
          */
         do_slotwise_x_on_subordinate_check(qep, true, false, monitor);
         break;
      case QI_DO_CLEAN:
      case QI_DO_RESCHEDULE:
         {
            const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
            cqueue_list_del_all_orphaned(master_cqueue_list, answer, qep.get<QU_qname>(),
                                         qep.get<QU_qhostname>(), packet->gdi_session);
         }
         break;
      default:
         break;
   }

   DRETURN(result);
}

/** @brief Apply a `qmod` job action to one job or array task
 *
 * Checks the permission (owner, or operator on an admin host) and executes the
 * action on the task, in the queue instance its master task runs in.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param jep the job
 * @param jatep the array task, or an empty handle for a task not enrolled yet
 * @param task_id the id of the not enrolled task, or 0
 * @param action the QI_DO_* action, or JSUSPENDED / JRUNNING
 * @param force whether the action is forced
 * @param answer receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @return 0 on success, -1 on error
 */
static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::JB> jep,
                     const ocs::tom::Element<ocs::tom::JAT> jatep, uint32_t task_id, const uint32_t action,
                     const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t job_id = jep.get<JB_job_number>();

   // check the modifying users permissions
   // need to be either owner
   // or operator/manager on an admin host
   if (sge_strnullcmp(packet->user, jep.get<JB_owner>()) != 0) {

      if (!manop_is_operator(packet)) {
         ERROR(MSG_JOB_NOMODJOBPERMS_SU, packet->user, job_id);
         answer_list_add(answer, SGE_EVENT, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }

      // if we are an operator, then we also need to be on an admin host
      bool is_admin_host = host_is_admin_host(packet->host);
      if (!is_admin_host) {
         ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }
   }

   if (jatep == nullptr) {
      /* unenrolled tasks always are not-running pending/hold */
      if (task_id) {
         WARNING(MSG_QMODJOB_NOTENROLLED_UU, job_id, task_id);
      } else {
         WARNING(MSG_QMODJOB_NOTENROLLED_U, job_id);
      }
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      DRETURN(-1);
   }

   task_id = jatep.get<JAT_task_number>();

   /* CS-1908: under retention a JFINISHED ja_task lives on JB_ja_tasks until
    * the sweep prunes it. qmod actions (suspend/unsuspend/reschedule/
    * clearerror) target a running or errored task -- none of them make
    * sense against a finished one, and the running-side machinery below
    * silently no-ops when JAT_master_queue is empty (which it is once the
    * finish path has run). Emit the same INFO the mod handler uses so the
    * caller understands why nothing happened. */
   if (jatep.get<JAT_status>() == JFINISHED) {
      INFO(MSG_JOB_ALREADYFINISHED_NOMOD_U, job_id);
      answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      DRETURN(0);
   }

   ocs::tom::Element<ocs::tom::QU> queueep = nullptr;
   if (jatep.get<JAT_master_queue>()) {
      const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
      queueep = cqueue_list_locate_qinstance(master_cqueue_list, jatep.get<JAT_master_queue>());
   }

   switch (action) {
      case QI_DO_RESCHEDULE:
         qmod_job_reschedule(packet, task, jep, jatep, queueep, force, answer, monitor);
         break;

      case JSUSPENDED:
         qmod_job_suspend(packet, task, jep, jatep, queueep, force, answer, monitor);
         if (queueep != nullptr) {
            do_slotwise_x_on_subordinate_check(queueep, false, false, monitor);
         }
         break;

      case JRUNNING:
         qmod_job_unsuspend(packet, task, jep, jatep, queueep, force, answer, monitor);
         if (queueep != nullptr) {
            do_slotwise_x_on_subordinate_check(queueep, true, false, monitor);
         }
         break;

      case QI_DO_CLEARERROR:
         if (VALID(JERROR, jatep.get<JAT_state>())) {
            jatep.set<JAT_state>(jatep.get<JAT_state>() & ~JERROR);
            ja_task_message_trash_all_of_type_X(jatep, 1);

            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            job_id, task_id, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);

            if (job_is_array(jep)) {
               INFO(MSG_JOB_CLEARERRORTASK_SSUU, packet->user, packet->host, job_id, task_id);
            } else {
               INFO(MSG_JOB_CLEARERRORJOB_SSU, packet->user, packet->host, job_id);
            }
         } else {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_NOERRORSTATETASK_UU, job_id, task_id);
            } else {
               INFO(MSG_JOB_NOERRORSTATEJOB_UU, job_id);
            }
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;

      default:
         INFO(MSG_LOG_JOBUNKNOWNQMODCMD_U, action);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;
   }

   DRETURN(0);
}

/** @brief Reschedule all jobs of a queue instance (`qmod -rq`)
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param qep the queue instance
 * @param force whether the rescheduling is forced
 * @param answer receives messages for the caller
 * @param isoperator whether the user is an operator
 * @param isowner whether the user owns the queue
 * @param monitor for monitoring qmaster threads
 * @return 0 on success, -1 if the user may not do it
 */
static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::QU> qep,
                     const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, const int isoperator,
                     const int isowner, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   if (!isoperator && !isowner) {
      ERROR(MSG_QUEUE_NORESCHEDULEQPERMS_SS, packet->user, qep.get<QU_full_name>());
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   reschedule_jobs(qep, force, answer, monitor, true, packet->user, packet->host, packet->gdi_session);

   DRETURN(0);
}

/** @brief Clean a queue instance: every job in it is deleted
 *
 * The user will do this via qconf -cq <qname>.
 *
 * @param packet the client request
 * @param qep the queue instance
 * @param force unused
 * @param answer receives messages for the caller
 * @param isoperator whether the user is an operator
 * @param isowner unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success, -1 if the user may not do it
 */
static int
qmod_queue_clean(const ocs::gdi::Packet *packet, const ocs::tom::ConstElement<ocs::tom::QU> qep, const uint32_t force,
                 ocs::tom::ListPtr<ocs::tom::AN> *answer, const int isoperator, const int isowner,
                 monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *qname = nullptr;
   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();

   qname = qep.get<QU_full_name>();

   DPRINTF("cleaning queue >%s<\n", qname);

   // need to be operator/manager for qmod -r
   if (!isoperator) {
      ERROR(SFNMAX, MSG_QUEUE_NOCLEANQPERMS);
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   /* using sge_commit_job(j, COMMIT_ST_FINISHED_FAILED) q->job_list
      could get modified so we have to be careful when iterating through the job list */
   ocs::tom::Element<ocs::tom::JB> nextjep = master_job_list.front();
   ocs::tom::Element<ocs::tom::JB> jep;
   while ((jep = nextjep)) {
      nextjep = jep.next();

      ocs::tom::Element<ocs::tom::JAT> nexttep = jep.get<JB_ja_tasks>().front();
      ocs::tom::Element<ocs::tom::JAT> jatep;
      while ((jatep = nexttep)) {
         nexttep = jatep.next();

         if (jatep.find_first_in<JAT_granted_destin_identifier_list, JG_qname>(qname) != nullptr) {
            /* 3: JOB_FINISH reports aborted */
            sge_commit_job(jep, jatep, nullptr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT | COMMIT_NEVER_RAN,
                           monitor, packet->gdi_session);
         }
      }
   }
   INFO(MSG_QUEUE_PURGEQ_SSS, packet->user, packet->host, qname);
   answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(0);
}

/** @brief Reschedule one job or array task (`qmod -rj`)
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param jep the job
 * @param jatep the array task
 * @param queueep the queue instance of the master task, or an empty handle
 * @param force whether the rescheduling is forced
 * @param answer receives messages for the caller
 * @param monitor for monitoring qmaster threads
 */
static void
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::JB> jep,
                    const ocs::tom::Element<ocs::tom::JAT> jatep, const ocs::tom::Element<ocs::tom::QU> queueep,
                    const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   reschedule_job(jep, jatep, queueep, force, answer, monitor, true, packet->user, packet->host, packet->gdi_session);

   DRETURN_VOID;
}

/** @brief Suspend one job or array task (`qmod -sj`)
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param jep the job
 * @param jatep the array task
 * @param queueep the queue instance of the master task, or an empty handle
 * @param force whether to set the state even if the signal cannot be delivered
 * @param answer receives messages for the caller
 * @param monitor for monitoring qmaster threads
 */
static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::JB> jep,
                 const ocs::tom::Element<ocs::tom::JAT> jatep, const ocs::tom::Element<ocs::tom::QU> queueep,
                 const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   uint32_t state = 0;
   uint32_t jataskid = 0;
   uint32_t jobid = 0;
   bool migrate_on_suspend = false;

   uint64_t now = sge_get_gmt64();

   jobid = jep.get<JB_job_number>();
   jataskid = jatep.get<JAT_task_number>();

   /* determine whether we actually migrate upon suspend */
   if (jep.get<JB_checkpoint_attr>() & CHECKPOINT_SUSPEND)
      migrate_on_suspend = true;

   if (VALID(JSUSPENDED, jatep.get<JAT_state>())) {
      /* this job is already suspended or lives in a suspended queue */
      if (force && queueep) {
         /* here force means to send the suspend signal again
            this can only be done if we know the queue this job
            runs in */
         if (sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatep, monitor)) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOFORCESUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_NOFORCESUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_FORCESUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_FORCESUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else {
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_ALREADYSUSPENDED_SUU, packet->user, jobid, jataskid);
         } else {
            WARNING(MSG_JOB_ALREADYSUSPENDED_SU, packet->user, jobid);
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      }

      /*
      ** may be the queue is suspended, than the job might not be
      */
      state = jatep.get<JAT_state>();
      CLEARBIT(JRUNNING, state);
      SETBIT(JSUSPENDED, state);
      jatep.set<JAT_state>(state);
      if (migrate_on_suspend) {
         jatep.set<JAT_stop_initiate_time>(now);
      }

      sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                      jobid, jataskid, nullptr, nullptr, nullptr,
                      jep, jatep, nullptr, true, true, packet->gdi_session);
   } else {   /* job wasn't suspended yet */
      if (queueep) {
         if ((i = sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatep, monitor))) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOSUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_NOSUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else
         i = 1;

      if (force) {
         /* set jobs state to suspend in all cases */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_FORCESUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               INFO(MSG_JOB_FORCESUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }

         state = jatep.get<JAT_state>();
         CLEARBIT(JRUNNING, state);
         SETBIT(JSUSPENDED, state);
         jatep.set<JAT_state>(state);
         if (migrate_on_suspend) {
            jatep.set<JAT_stop_initiate_time>(now);
         }
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
      } else {
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_SUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               INFO(MSG_JOB_SUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

            state = jatep.get<JAT_state>();
            CLEARBIT(JRUNNING, state);
            SETBIT(JSUSPENDED, state);
            jatep.set<JAT_state>(state);
            if (migrate_on_suspend) {
               jatep.set<JAT_stop_initiate_time>(now);
            }
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            jobid, jataskid, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
         }
      }
      ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_SUSPENDED, packet->user, packet->host,
                                             ocs::tom::ConstElement<ocs::tom::JR>(), jep, jatep,
                                             ocs::tom::ConstElement<ocs::tom::PET>(), nullptr);
   }
   DRETURN_VOID;
}

/** @brief Unsuspend one job or array task (`qmod -usj`)
 *
 * An admin unsuspend may not override a suspension by threshold.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param jep the job
 * @param jatep the array task
 * @param queueep the queue instance of the master task, or an empty handle
 * @param force whether to set the state even if the signal cannot be delivered
 * @param answer receives messages for the caller
 * @param monitor for monitoring qmaster threads
 */
static void
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::JB> jep,
                   const ocs::tom::Element<ocs::tom::JAT> jatep, const ocs::tom::Element<ocs::tom::QU> queueep,
                   const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   uint32_t state = 0;
   uint32_t jobid, jataskid;

   uint64_t now = sge_get_gmt64();

   jobid = jep.get<JB_job_number>();
   jataskid = jatep.get<JAT_task_number>();

   /* admin suspend may not override suspend from threshold */
   if (VALID(JSUSPENDED_ON_THRESHOLD, jatep.get<JAT_state>())) {
      if (VALID(JSUSPENDED, jatep.get<JAT_state>())) {
         if (job_is_array(jep)) {
            INFO(MSG_JOB_RMADMSUSPENDTASK_SSUU, packet->user, packet->host, jobid, jataskid);
         } else {
            INFO(MSG_JOB_RMADMSUSPENDJOB_SSU, packet->user, packet->host, jobid);
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

         state = jatep.get<JAT_state>();
         CLEARBIT(JSUSPENDED, state);
         jatep.set<JAT_state>(state);
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host,
                                             ocs::tom::ConstElement<ocs::tom::JR>(), jep, jatep,
                                             ocs::tom::ConstElement<ocs::tom::PET>(), nullptr);
         DRETURN_VOID;
      } else {
         /* guess admin tries to remove threshold suspension by qmon -us <jobid> */
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_NOADMSUSPENDTASK_SUU, packet->user, jobid, jataskid);
         } else {
            WARNING(MSG_JOB_NOADMSUSPENDJOB_SU, packet->user, jobid);
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         DRETURN_VOID;
      }
   }

   if (VALID(JRUNNING, jatep.get<JAT_state>())) {
      /* this job is already running */
      if (force && queueep) {
         /*
         ** here force means to send the cont signal again
         ** this can only be done if we know the queue this job
         ** runs in
         */
         if (sge_signal_queue(SGE_SIGCONT, queueep, jep, jatep, monitor)) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOFORCEENABLETASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_NOFORCEENABLEJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_FORCEENABLETASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_FORCEENABLEJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else {
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_ALREADYUNSUSPENDED_SUU, packet->user, jobid, jataskid);
         } else {
            WARNING(MSG_JOB_ALREADYUNSUSPENDED_SU, packet->user, jobid);
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      }
      /*
      ** job is already running, so no job information has to be changed
      */
   } else {   /* job wasn't suspended till now */
      if (queueep) {
         if ((i = sge_signal_queue(SGE_SIGCONT, queueep, jep, jatep, monitor))) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOUNSUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               WARNING(MSG_JOB_NOUNSUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else
         i = 1;

      if (force) {
         /* set jobs state to suspend in all cases */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_FORCEUNSUSPTASK_SSUU, packet->user, packet->host, jobid, jataskid);
            } else {
               INFO(MSG_JOB_FORCEUNSUSPJOB_SSU, packet->user, packet->host, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);
         }

         state = jatep.get<JAT_state>();
         SETBIT(JRUNNING, state);
         CLEARBIT(JSUSPENDED, state);
         jatep.set<JAT_state>(state);
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
      } else {
         /* set job state only if communication works */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_UNSUSPENDTASK_SUU, packet->user, jobid, jataskid);
            } else {
               INFO(MSG_JOB_UNSUSPENDJOB_SU, packet->user, jobid);
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);

            state = jatep.get<JAT_state>();
            SETBIT(JRUNNING, state);
            CLEARBIT(JSUSPENDED, state);
            jatep.set<JAT_state>(state);
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            jobid, jataskid, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
         }
      }
   }
   ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host,
                                             ocs::tom::ConstElement<ocs::tom::JR>(), jep, jatep,
                                             ocs::tom::ConstElement<ocs::tom::PET>(), nullptr);
   DRETURN_VOID;
}


/** @brief Re-arm the signal resend timers after a qmaster restart
 *
 * The timed events themselves live only in memory, but the state they were
 * created from is spooled: `JAT_pending_signal` /
 * `JAT_pending_signal_delivery_time` on array tasks, and the `QU_pending_signal`
 * pair on queue instances. Without this, a signal that was in flight across a
 * restart would never be repeated and the job or queue would sit in its old
 * state forever.
 *
 * Called once during qmaster startup, after the spooled lists have been read.
 */
void
rebuild_signal_events() {
   DENTER(TOP_LAYER);

   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

   /* J O B */
   for (const auto jep : master_job_list) {
      for (const auto jatep : jep.get<JB_ja_tasks>()) {
         uint64_t when = jatep.get<JAT_pending_signal_delivery_time>();

         if (jatep.get<JAT_pending_signal>() && (when > 0)) {
            uint32_t key1 = jep.get<JB_job_number>();
            uint32_t key2 = jatep.get<JAT_task_number>();
            te_event_t ev = nullptr;

            ev = te_new_event(when, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, key1, key2, nullptr);
            te_add_event(ev);
            te_free_event(&ev);
         }
      }
   }

   /* Q U E U E */
   for (const auto cqueue : master_cqueue_list) {
      for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
         uint64_t when = qinstance.get<QU_pending_signal_delivery_time>();

         if (qinstance.get<QU_pending_signal>() && when > 0) {
            const char *str_key = qinstance.get<QU_full_name>();
            te_event_t ev = nullptr;

            ev = te_new_event(when, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, str_key);
            te_add_event(ev);
            te_free_event(&ev);
         }
      }
   }

   DRETURN_VOID;
} /* rebuild_signal_events() */

/** @brief Send a signal again to a job or queue that has not reacted
 *
 * This function is called by our timer mechanism for resending signals: the
 * handler for #TYPE_SIGNAL_RESEND_EVENT. A signal is not acknowledged
 * synchronously, so it is repeated until the state actually changes.
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 */
void
resend_signal_event(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t jobid = te_get_first_numeric_key(anEvent);
   uint32_t jataskid = te_get_second_numeric_key(anEvent);
   const char *queue = te_get_alphanumeric_key(anEvent);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   if (queue == nullptr) {
      // read access as master_list() gives it; the job found is changed
      ocs::tom::Element<ocs::tom::JB> jep =
         ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true).find_first<JB_job_number>(jobid);
      ocs::tom::Element<ocs::tom::JAT> jatep = nullptr;
      if (!jep || !(jatep = job_search_task(jep, nullptr, jataskid))) {
         ERROR(MSG_EVE_RESENTSIGNALTASK_UU, jobid, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      const ocs::tom::Element<ocs::tom::QU> qep =
              cqueue_list_locate_qinstance(master_cqueue_list, jatep.get<JAT_master_queue>());
      if (qep) {
         sge_signal_queue(jatep.get<JAT_pending_signal>(), qep, jep, jatep, monitor);
      }
   } else {
      const ocs::tom::Element<ocs::tom::QU> qep = cqueue_list_locate_qinstance(master_cqueue_list, queue);
      if (!qep) {
         ERROR(MSG_EVE_RESENTSIGNALQ_S, queue);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         sge_free(&queue);
         DRETURN_VOID;
      }

      sge_signal_queue(qep.get<QU_pending_signal>(), qep, ocs::tom::Element<ocs::tom::JB>(),
                       ocs::tom::Element<ocs::tom::JAT>(), monitor);
   }

   sge_free(&queue);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

/** @brief Mark every task in a queue instance as (un)suspended on subordinate
 *
 * @param qnm the full name of the queue instance
 * @param how SGE_SIGSTOP sets JSUSPENDED_ON_SUBORDINATE, every other signal clears it
 */
static void
sge_propagate_queue_suspension(const char *qnm, const int how) {
   DENTER(TOP_LAYER);
   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();

   DPRINTF("searching for all jobs in queue %s due to %s\n", qnm, sge_sig2str(how));
   for (const auto jep : master_job_list) {
      for (const auto jatep : jep.get<JB_ja_tasks>()) {
         if (jatep.get<JAT_granted_destin_identifier_list>().find_first<JG_qname>(qnm)) {
            uint32_t jstate;
            DPRINTF("found " sge_u32"." sge_u32"\n", jep.get<JB_job_number>(), jatep.get<JAT_task_number>());
            jstate = jatep.get<JAT_state>();
            if (how == SGE_SIGSTOP)
               jstate |= JSUSPENDED_ON_SUBORDINATE;
            else
               jstate &= ~JSUSPENDED_ON_SUBORDINATE;
            jatep.set<JAT_state>(jstate);
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Signal a queue instance, or one job in it
 *
 * This is called by the qmaster to:
 * - send a signal to all jobs in a queue (job_number == 0);
 * - send a signal to one job
 *
 * @param how the signal to send
 * @param qep the queue instance
 * @param jep the job, or an empty handle to signal every job in the queue
 * @param jatep the array task, or an empty handle
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
sge_signal_queue(const int how, const ocs::tom::Element<ocs::tom::QU> qep, const ocs::tom::Element<ocs::tom::JB> jep,
                 const ocs::tom::Element<ocs::tom::JAT> jatep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   sge_pack_buffer pb;
   int sent = 0;

   uint64_t now = sge_get_gmt64();

   DEBUG("queue_signal: %d, queue: %s, job: " sge_u32 ", jatask: " sge_u32, how,
      (qep != nullptr ? qep.get<QU_full_name>() : "none"),
      jep != nullptr ? jep.get<JB_job_number>() : 0,
      jatep != nullptr ? jatep.get<JAT_task_number>() : 0);

   if (!jep && (how == SGE_SIGSTOP || how == SGE_SIGCONT)) {
      sge_propagate_queue_suspension(qep.get<QU_full_name>(), how);
   }

   /* don't try to signal unheard queues */
   if (!qinstance_state_is_unknown(qep)) {
      const char *hnm = qep.get<QU_qhostname>();
      const char *pnm = to_cstr(EXECD);

      if ((i = ocs::tom::try_init_buffer(&pb, 256)) == PACK_SUCCESS) {
         /* identifier for acknowledgement */
         if (jep) {
            /*
             * Due to IZ 1619: pack signal only if
             *    job is a non-parallel job
             *    or all slaves of the parallel job have been acknowledged
             */
            if (!jatep.get<JAT_master_queue>() ||
                is_pe_master_task_send(jatep)) {
               /* TAG_SIGJOB */
               ocs::tom::try_put_uint32(&pb, jep.get<JB_job_number>());
               ocs::tom::try_put_uint32(&pb, jatep.get<JAT_task_number>());
               ocs::tom::try_put_str(&pb, nullptr);
               ocs::tom::try_put_uint32(&pb, how);
            }
         } else {
            /* TAG_SIGQUEUE */
            ocs::tom::try_put_uint32(&pb, 0);
            ocs::tom::try_put_uint32(&pb, 0);
            ocs::tom::try_put_str(&pb, qep.get<QU_full_name>());
            ocs::tom::try_put_uint32(&pb, how);
         }

         if (mconf_get_simulate_execds()) {
            // @todo no need to do the packing above, best split the function in two
            // @todo check if the job is already in deletion?
            i = CL_RETVAL_OK;
            if (jep && how == SGE_SIGKILL)
               trigger_job_resend(sge_get_gmt64(), nullptr, jep.get<JB_job_number>(), jatep.get<JAT_task_number>(), 1);
         } else {
            if (ocs::tom::buffer_filled(&pb)) {
               uint32_t dummy = 0;
               i = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, pnm, 1, hnm,
                                                               jep ? ocs::gdi::ClientServerBase::TAG_SIGJOB : ocs::gdi::ClientServerBase::TAG_SIGQUEUE, &pb, &dummy);
            }
         }

         MONITOR_MESSAGES_OUT(monitor);
         ocs::tom::clear_buffer(&pb);
      } else {
         i = CL_RETVAL_MALLOC;  /* an error */
      }

      if (i != CL_RETVAL_OK) {
         ERROR(MSG_COM_NOUPDATEQSTATE_IS, how, qep.get<QU_full_name>());
         DRETURN(i);
      }
      sent = 1;
   }

   uint64_t next_delivery_time = now + sge_gmt32_to_gmt64(60);

   /* If this is a operation on one job we enter the signal request in the
      job structure. If the operation is not acknowledged in time we can do
      further steps */
   if (jep) {
      te_event_t ev = nullptr;

      DPRINTF("JOB " sge_u32 ": %s signal %s (retry after " sge_u64 " microseconds) host: %s\n",
              jep.get<JB_job_number>(), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              qep.get<QU_qhostname>().c_str());
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, jep.get<JB_job_number>(),
                               jatep.get<JAT_task_number>(), nullptr);

      if (!mconf_get_simulate_execds()) {
         jatep.set<JAT_pending_signal>(how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT,
                           jep.get<JB_job_number>(), jatep.get<JAT_task_number>(), nullptr);
         te_add_event(ev);
         te_free_event(&ev);
         jatep.set<JAT_pending_signal_delivery_time>(next_delivery_time);
      }
   } else {
      te_event_t ev = nullptr;

      DPRINTF("QUEUE %s: %s signal %s (retry after " sge_u64" microseconds) host %s\n",
              qep.get<QU_full_name>(), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              qep.get<QU_qhostname>().c_str());
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, 0, 0, qep.get<QU_full_name>());

      if (!mconf_get_simulate_execds()) {
         qep.set<QU_pending_signal>(how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0,
                           qep.get<QU_full_name>());
         te_add_event(ev);
         te_free_event(&ev);
         qep.set<QU_pending_signal_delivery_time>(next_delivery_time);
      }
   }

   if (!jep) {/* signalling a queue ? - handle slave jobs in this queue */
      signal_slave_jobs_in_queue(how, qep, monitor);
   } else {/* is this the master queue of this job to signal ? - then decide whether slave tasks also
           must get signalled */
      if (!strcmp(jatep.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>(),
                  qep.get<QU_full_name>())) {
         signal_slave_tasks_of_job(how, jep, jatep, monitor);
      }
   }

   DRETURN(0);
} /* sge_signal_queue() */

/** @brief Forward a queue signal to the parallel jobs having a slave task in the queue
 *
 * In case we have to signal a queue in which slave tasks are running we have
 * to notify the master execd where the master task of this job is running.
 *
 * @param how the signal to send
 * @param qep the queue instance
 * @param monitor for monitoring qmaster threads
 */
static void
signal_slave_jobs_in_queue(const int how, const ocs::tom::ConstElement<ocs::tom::QU> qep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::JG> gdil_lp;
   const char *qname, *mqname, *pe_name;
   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();

   qname = qep.get<QU_full_name>();
   /* test whether there are parallel jobs
      with a slave slot in this queue
      if so then signal this job */
   for (const auto jep : master_job_list) {
      for (const auto jatep : jep.get<JB_ja_tasks>()) {

         /* skip sequential and not running jobs */
         if ((gdil_lp = jatep.get<JAT_granted_destin_identifier_list>()).size() <= 1)
            continue;

         /* signalling of not "slave controlled" parallel jobs will not work
            since they are not known to the appropriate execd - we should
            omit signalling in this case to prevent waste of communication bandwidth */
         if (!(pe_name = jatep.get<JAT_granted_pe>()) || !master_pe_list.find_first<PE_name>(pe_name))
            continue;

         if (gdil_lp.find_first<JG_qname>(qname) != nullptr) {

            /* search master queue - needed for signalling of a job */
            mqname = gdil_lp.front().get<JG_qname>();
            const ocs::tom::Element<ocs::tom::QU> mq = cqueue_list_locate_qinstance(master_cqueue_list, mqname);
            if (mq) {
               DPRINTF("found slave job " sge_u32" in queue %s master queue is %s\n",
                       jep.get<JB_job_number>(), qname, mqname);
               sge_signal_queue(how, mq, jep, jatep, monitor);
            } else {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, mqname, jep.get<JB_job_number>());
            }
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Forward a job signal to the slave tasks of a slave controlled parallel job
 *
 * @param how the signal to send
 * @param jep the job
 * @param jatep the array task
 * @param monitor for monitoring qmaster threads
 */
static void
signal_slave_tasks_of_job(const int how, const ocs::tom::Element<ocs::tom::JB> jep,
                          const ocs::tom::Element<ocs::tom::JAT> jatep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::JG> gdil_lp;
   ocs::tom::ConstElement<ocs::tom::PE> pe;
   const char *qname, *pe_name;
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   /* do not signal slave tasks in case of checkpointing jobs with
      STOP/CONT when suspending means migration */
   if ((how == SGE_SIGCONT || how == SGE_SIGSTOP) &&
       ISSET(jep.get<JB_checkpoint_attr>(), CHECKPOINT_SUSPEND)) {
      DPRINTF("omit signaling - checkpoint script does action for whole job\n");
      DRETURN_VOID;
   }

   /* forward signal to slave exec hosts
      in case of slave controlled jobs */
   if (!((gdil_lp = jatep.get<JAT_granted_destin_identifier_list>()).size() <= 1 ||
         !(pe_name = jatep.get<JAT_granted_pe>()) || !(pe = master_pe_list.find_first<PE_name>(pe_name)) ||
         !pe.get<PE_control_slaves>())) {
      for (auto gdil_ep = gdil_lp.front().next(); gdil_ep; gdil_ep = gdil_ep.next()) {
         qname = gdil_ep.get<JG_qname>();
         const ocs::tom::Element<ocs::tom::QU> mq = cqueue_list_locate_qinstance(master_cqueue_list, qname);
         if (mq) {
            DPRINTF("found slave job " sge_u32" in queue %s\n", jep.get<JB_job_number>(), qname);
            sge_signal_queue(how, mq, jep, jatep, monitor);
         }
      }
   }

   DRETURN_VOID;
}
