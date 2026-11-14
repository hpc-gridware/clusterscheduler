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


/*-------------------------------------------------------------------------*/
static void
signal_slave_jobs_in_queue(int how, lListElem *jep, monitoring_t *monitor);

static void
signal_slave_tasks_of_job(int how, lListElem *jep, lListElem *jatep, monitoring_t *monitor);

static int
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, uint32_t action,
                       uint32_t force, lList **answer, monitoring_t *monitor);

static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep,
                     uint32_t task_id, uint32_t action, uint32_t force, lList **answer, monitoring_t *monitor);

static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, uint32_t force, lList **answer,
                     int isoperator, int isowner, monitoring_t *monitor);

static int
qmod_queue_clean(const ocs::gdi::Packet *packet, lListElem *qep, uint32_t force, lList **answer,
                 int isoperator, int isowner, monitoring_t *monitor);

static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                 lList **answer, monitoring_t *monitor);

static void
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                   lList **answer, monitoring_t *monitor);

static void
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                    lList **answer, monitoring_t *monitor);

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
   lList *alp = nullptr;
   lListElem *jatask = nullptr, *tmp_task;
   const lListElem *rn;
   bool found;
   uint32_t jobid;
   uint32_t start = 0, end = 0, step = 0;
   int alltasks;
   dstring cqueue_buffer = DSTRING_INIT;
   dstring hostname_buffer = DSTRING_INIT;
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   /*
   ** loop over the ids and change queue or job state and signal them
   ** if necessary
   */
   for_each_ep_lv(dep, task->data_list) {
      lList *tmp_list = nullptr;
      lList *qref_list = nullptr;
      bool found_something = true;
      uint32_t id_action = ocs::tom::get<ID_action>(dep);

      found = false;

      // CS-2144: IJS session-reconnect path bypasses the qmod queue/job dispatch.
      // The client encodes the request in ID_str as "<job_id>:<client_host>:<client_port>".
      // On success an answer of STATUS_OK is added with text "RECONNECT_OK <token> <exec_host>",
      // which the qrsh -reconnect client parses to obtain the rendezvous parameters.
      if ((id_action & ~JOB_DO_ACTION) == QI_DO_RECONNECT) {
         const char *spec = ocs::tom::get<ID_str>(dep);
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
         const lListElem *cred_ep = lFirst(static_cast<const lList *>(ocs::tom::get<ID_user_list>(dep)));
         if (cred_ep != nullptr) {
            reconnect_cred = ocs::tom::get<ST_name>(cred_ep);
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
         qref_list_add(&qref_list, nullptr, ocs::tom::get<ID_str>(dep));
         qref_list_resolve_hostname(qref_list);
         qref_list_resolve(qref_list, nullptr, &tmp_list,
                           &found_something, master_cqueue_list,
                           master_hgroup_list,
                           true, true);
         if (found_something) {
            id_action = (id_action & (~QUEUE_DO_ACTION));

            for_each_ep_lv(qref, tmp_list) {
               const char *full_name = nullptr;
               const char *cqueue_name = nullptr;
               const char *hostname = nullptr;
               const lListElem *cqueue = nullptr;
               lListElem *qinstance = nullptr;
               const lList *qinstance_list = nullptr;

               full_name = ocs::tom::get<QR_name>(qref);
               if (!cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
                  continue;
               }
               cqueue_name = sge_dstring_get_string(&cqueue_buffer);
               hostname = sge_dstring_get_string(&hostname_buffer);
               cqueue = ocs::tom::find_first<CQ_name>(master_cqueue_list, cqueue_name);
               qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
               qinstance = ocs::tom::find_first_rw<QU_qhostname>(qinstance_list, hostname);

               sge_change_queue_state(packet, task, qinstance, id_action, ocs::tom::get<ID_force>(dep), &alp, monitor);
               found = true;
            }
         }
         lFreeList(&qref_list);
         lFreeList(&tmp_list);
      }
      if (!found) {
         bool is_jobName_suport = false;
         u_long action = ocs::tom::get<ID_action>(dep);
         if ((action & JOB_DO_ACTION) > 0 &&
             (action & QUEUE_DO_ACTION) == 0) {
            action = (action & (~JOB_DO_ACTION));
            is_jobName_suport = true;
         }

         /*
         ** We found no queue so look for a job. This only makes sense for
         ** suspend, unsuspend, reschedule, and clear
         */
         if (sge_strisint(ocs::tom::get<ID_str>(dep)) &&
             (action == QI_DO_SUSPEND ||
              action == QI_DO_RESCHEDULE ||
              action == QI_DO_CLEARERROR ||
              action == QI_DO_UNSUSPEND)) {
            jobid = strtol(ocs::tom::get<ID_str>(dep), nullptr, 10);

            rn = lFirst(static_cast<const lList *>(ocs::tom::get<ID_ja_structure>(dep)));
            if (rn) {
               start = ocs::tom::get<RN_min>(rn);
               if (start) {
                  end = ocs::tom::get<RN_max>(rn);
                  step = ocs::tom::get<RN_step>(rn);
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

            lListElem *job = ocs::tom::find_first_rw<JB_job_number>(master_job_list, jobid);
            if (job != nullptr) {
               jatask = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(job)));

               while ((tmp_task = jatask)) {
                  uint32_t task_number;

                  jatask = lNextRW(tmp_task);
                  task_number = ocs::tom::get<JAT_task_number>(tmp_task);
                  if ((task_number >= start && task_number <= end &&
                       ((task_number - start) % step) == 0) || alltasks) {
                     DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, task_number);

                     /* this specifies no queue, so lets probe for a job */
                     /* change state of job: */
                     sge_change_job_state(packet, task, job, tmp_task, 0, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
                     found = true;
                  }
               }

               /* create more precise GDI answers also for pending jobs/tasks and jobs/tasks in hold state
                  When the operation is to be applied on the whole job array but no task is enrolled so far
                  (i.e. not found) only one single GDI answer is created. Otherwise one message is created
                  per task */
               if (alltasks && job_is_array(job)) {
                  if (!found) {
                     sge_change_job_state(packet, task, job, nullptr, 0, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
                     found = true;
                  }
               } else {
                  const lListElem *range;
                  uint32_t min, max, step;
                  uint32_t taskid;

                  /* handle all pending tasks */
                  for_each_ep(range, static_cast<const lList *>(ocs::tom::get<JB_ja_n_h_ids>(job))) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in user hold */
                  for_each_ep(range, static_cast<const lList *>(ocs::tom::get<JB_ja_u_h_ids>(job))) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in system hold that are not in user hold */
                  for_each_ep(range, static_cast<const lList *>(ocs::tom::get<JB_ja_s_h_ids>(job))) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(ocs::tom::get<JB_ja_u_h_ids>(job), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in operator hold that are not in user hold or system hold */
                  for_each_ep(range, static_cast<const lList *>(ocs::tom::get<JB_ja_o_h_ids>(job))) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(ocs::tom::get<JB_ja_u_h_ids>(job), taskid) ||
                            range_list_is_id_within(ocs::tom::get<JB_ja_s_h_ids>(job), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32 "." sge_u32 "\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, ocs::tom::get<ID_force>(dep), &alp, monitor);
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

            const char *job_name = ocs::tom::get<ID_str>(dep);
            const lListElem *job;
            lListElem *mod = nullptr;
            for_each_ep(job, master_job_list) {
               if (!fnmatch(job_name, ocs::tom::get<JB_job_name>(job), 0)) {
                  char job_id[40];
                  mod = lCopyElem(dep);
                  snprintf(job_id, sizeof(job_id), sge_u32, ocs::tom::get<JB_job_number>(job));
                  ocs::tom::set<ID_str>(mod, job_id);
                  lAppendElem(task->data_list, mod);
                  found = true;
               }
            }
         } else {
            /* job id invalid or action invalid for jobs */

         }
      }

      if (!found) {
         u_long action = ocs::tom::get<ID_action>(dep);
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
            ERROR(MSG_QUEUE_INVALIDQORJOB_S, ocs::tom::get<ID_str>(dep));
         } else {
            ERROR(MSG_QUEUE_INVALIDQ_S, ocs::tom::get<ID_str>(dep));
         }
         answer_list_add(&alp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      }
   }

   sge_dstring_free(&cqueue_buffer);
   sge_dstring_free(&hostname_buffer);

   task->answer_list = alp;

   DRETURN_VOID;
}

static int
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, uint32_t action,
                       uint32_t force, lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int result = 0;
   bool isowner = qinstance_is_owner(packet, qep);
   bool isoperator = manop_is_operator(packet);

   if (!isowner && !isoperator) {
      ERROR(MSG_QUEUE_NOCHANGEQPERMS_SS, packet->user, ocs::tom::get<QU_full_name>(qep));
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
            lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
            cqueue_list_del_all_orphaned(master_cqueue_list, answer, ocs::tom::get<QU_qname>(qep),
                                         ocs::tom::get<QU_qhostname>(qep), packet->gdi_session);
         }
         break;
      default:
         break;
   }

   DRETURN(result);
}

static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep,
                     uint32_t task_id, uint32_t action, uint32_t force, lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t job_id = ocs::tom::get<JB_job_number>(jep);

   // check the modifying users permissions
   // need to be either owner
   // or operator/manager on an admin host
   if (sge_strnullcmp(packet->user, ocs::tom::get<JB_owner>(jep)) != 0) {

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

   task_id = ocs::tom::get<JAT_task_number>(jatep);

   /* CS-1908: under retention a JFINISHED ja_task lives on JB_ja_tasks until
    * the sweep prunes it. qmod actions (suspend/unsuspend/reschedule/
    * clearerror) target a running or errored task -- none of them make
    * sense against a finished one, and the running-side machinery below
    * silently no-ops when JAT_master_queue is empty (which it is once the
    * finish path has run). Emit the same INFO the mod handler uses so the
    * caller understands why nothing happened. */
   if (ocs::tom::get<JAT_status>(jatep) == JFINISHED) {
      INFO(MSG_JOB_ALREADYFINISHED_NOMOD_U, job_id);
      answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      DRETURN(0);
   }

   lListElem *queueep;
   if (ocs::tom::get<JAT_master_queue>(jatep)) {
      const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
      queueep = cqueue_list_locate_qinstance(master_cqueue_list, ocs::tom::get<JAT_master_queue>(jatep));
   } else {
      queueep = nullptr;
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
         if (VALID(JERROR, ocs::tom::get<JAT_state>(jatep))) {
            ocs::tom::set<JAT_state>(jatep, ocs::tom::get<JAT_state>(jatep) & ~JERROR);
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

/****
 **** qmod_queue_weakclean (static)
 ****/
static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, uint32_t force, lList **answer,
                     int isoperator, int isowner, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   if (!isoperator && !isowner) {
      ERROR(MSG_QUEUE_NORESCHEDULEQPERMS_SS, packet->user, ocs::tom::get<QU_full_name>(qep));
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   reschedule_jobs(qep, force, answer, monitor, true, packet->user, packet->host, packet->gdi_session);

   DRETURN(0);
}

/****
 **** qmod_queue_clean (static)
 ****
 **** cleans the specified queue (every job will be deleted)
 **** The user will do this via qconf -cq <qname>
 ****/
static int
qmod_queue_clean(const ocs::gdi::Packet *packet, lListElem *qep, uint32_t force, lList **answer,
                 int isoperator, int isowner, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   lListElem *nextjep, *jep;
   const char *qname = nullptr;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   qname = ocs::tom::get<QU_full_name>(qep);

   DPRINTF("cleaning queue >%s<\n", qname);

   // need to be operator/manager for qmod -r
   if (!isoperator) {
      ERROR(SFNMAX, MSG_QUEUE_NOCLEANQPERMS);
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   /* using sge_commit_job(j, COMMIT_ST_FINISHED_FAILED) q->job_list
      could get modified so we have to be careful when iterating through the job list */
   nextjep = lFirstRW(master_job_list);
   while ((jep = nextjep)) {
      lListElem *jatep, *nexttep;
      nextjep = lNextRW(jep);

      nexttep = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jep)));
      while ((jatep = nexttep)) {
         nexttep = lNextRW(jatep);

         if (static_cast<const lListElem *>(ocs::tom::find_first_in<JAT_granted_destin_identifier_list, JG_qname>(jatep, qname)) != nullptr) {
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

/****
 **** qmod_job_reschedule (static)
 ****/
static void
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task,  lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                    lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   reschedule_job(jep, jatep, queueep, force, answer, monitor, true, packet->user, packet->host, packet->gdi_session);

   DRETURN_VOID;
}

/****
 **** qmod_job_suspend (static)
 ****/
static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                 lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   uint32_t state = 0;
   uint32_t jataskid = 0;
   uint32_t jobid = 0;
   bool migrate_on_suspend = false;

   uint64_t now = sge_get_gmt64();

   jobid = ocs::tom::get<JB_job_number>(jep);
   jataskid = ocs::tom::get<JAT_task_number>(jatep);

   /* determine whether we actually migrate upon suspend */
   if (ocs::tom::get<JB_checkpoint_attr>(jep) & CHECKPOINT_SUSPEND)
      migrate_on_suspend = true;

   if (VALID(JSUSPENDED, ocs::tom::get<JAT_state>(jatep))) {
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
      state = ocs::tom::get<JAT_state>(jatep);
      CLEARBIT(JRUNNING, state);
      SETBIT(JSUSPENDED, state);
      ocs::tom::set<JAT_state>(jatep, state);
      if (migrate_on_suspend) {
         ocs::tom::set<JAT_stop_initiate_time>(jatep, now);
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

         state = ocs::tom::get<JAT_state>(jatep);
         CLEARBIT(JRUNNING, state);
         SETBIT(JSUSPENDED, state);
         ocs::tom::set<JAT_state>(jatep, state);
         if (migrate_on_suspend) {
            ocs::tom::set<JAT_stop_initiate_time>(jatep, now);
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

            state = ocs::tom::get<JAT_state>(jatep);
            CLEARBIT(JRUNNING, state);
            SETBIT(JSUSPENDED, state);
            ocs::tom::set<JAT_state>(jatep, state);
            if (migrate_on_suspend) {
               ocs::tom::set<JAT_stop_initiate_time>(jatep, now);
            }
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            jobid, jataskid, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
         }
      }
      ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_SUSPENDED, packet->user, packet->host, nullptr, jep, jatep, nullptr, nullptr);
   }
   DRETURN_VOID;
}

/****
 **** qmod_job_unsuspend (static)
 ****/
static void
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, uint32_t force,
                   lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   uint32_t state = 0;
   uint32_t jobid, jataskid;

   uint64_t now = sge_get_gmt64();

   jobid = ocs::tom::get<JB_job_number>(jep);
   jataskid = ocs::tom::get<JAT_task_number>(jatep);

   /* admin suspend may not override suspend from threshold */
   if (VALID(JSUSPENDED_ON_THRESHOLD, ocs::tom::get<JAT_state>(jatep))) {
      if (VALID(JSUSPENDED, ocs::tom::get<JAT_state>(jatep))) {
         if (job_is_array(jep)) {
            INFO(MSG_JOB_RMADMSUSPENDTASK_SSUU, packet->user, packet->host, jobid, jataskid);
         } else {
            INFO(MSG_JOB_RMADMSUSPENDJOB_SSU, packet->user, packet->host, jobid);
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

         state = ocs::tom::get<JAT_state>(jatep);
         CLEARBIT(JSUSPENDED, state);
         ocs::tom::set<JAT_state>(jatep, state);
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host, nullptr, jep, jatep, nullptr, nullptr);
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

   if (VALID(JRUNNING, ocs::tom::get<JAT_state>(jatep))) {
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

         state = ocs::tom::get<JAT_state>(jatep);
         SETBIT(JRUNNING, state);
         CLEARBIT(JSUSPENDED, state);
         ocs::tom::set<JAT_state>(jatep, state);
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

            state = ocs::tom::get<JAT_state>(jatep);
            SETBIT(JRUNNING, state);
            CLEARBIT(JSUSPENDED, state);
            ocs::tom::set<JAT_state>(jatep, state);
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            jobid, jataskid, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
         }
      }
   }
   ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host, nullptr, jep, jatep, nullptr, nullptr);
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

   const lListElem *cqueue, *jep, *jatep;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   /* J O B */
   for_each_ep(jep, master_job_list) {
      for_each_ep(jatep, static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jep))) {
         uint64_t when = ocs::tom::get<JAT_pending_signal_delivery_time>(jatep);

         if (ocs::tom::get<JAT_pending_signal>(jatep) && (when > 0)) {
            uint32_t key1 = ocs::tom::get<JB_job_number>(jep);
            uint32_t key2 = ocs::tom::get<JAT_task_number>(jatep);
            te_event_t ev = nullptr;

            ev = te_new_event(when, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, key1, key2, nullptr);
            te_add_event(ev);
            te_free_event(&ev);
         }
      }
   }

   /* Q U E U E */
   for_each_ep(cqueue, master_cqueue_list) {
      const lList *qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
      const lListElem *qinstance;

      for_each_ep(qinstance, qinstance_list) {
         uint64_t when = ocs::tom::get<QU_pending_signal_delivery_time>(qinstance);

         if (ocs::tom::get<QU_pending_signal>(qinstance) && when > 0) {
            const char *str_key = ocs::tom::get<QU_full_name>(qinstance);
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

   lListElem *qep, *jep, *jatep;
   uint32_t jobid = te_get_first_numeric_key(anEvent);
   uint32_t jataskid = te_get_second_numeric_key(anEvent);
   const char *queue = te_get_alphanumeric_key(anEvent);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   if (queue == nullptr) {
      if (!(jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, jobid)) ||
          !(jatep = job_search_task(jep, nullptr, jataskid))) {
         ERROR(MSG_EVE_RESENTSIGNALTASK_UU, jobid, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if ((qep = cqueue_list_locate_qinstance(master_cqueue_list, ocs::tom::get<JAT_master_queue>(jatep)))) {
         sge_signal_queue(ocs::tom::get<JAT_pending_signal>(jatep), qep, jep, jatep, monitor);
      }
   } else {
      if (!(qep = cqueue_list_locate_qinstance(master_cqueue_list, queue))) {
         ERROR(MSG_EVE_RESENTSIGNALQ_S, queue);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         sge_free(&queue);
         DRETURN_VOID;
      }

      sge_signal_queue(ocs::tom::get<QU_pending_signal>(qep), qep, nullptr, nullptr, monitor);
   }

   sge_free(&queue);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

static void
sge_propagate_queue_suspension(const char *qnm, int how) {
   DENTER(TOP_LAYER);
   const lListElem *jep;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DPRINTF("searching for all jobs in queue %s due to %s\n", qnm, sge_sig2str(how));
   for_each_ep(jep, master_job_list) {
      for_each_rw_lv (jatep, ocs::tom::get<JB_ja_tasks>(jep)) {
         if (ocs::tom::find_first<JG_qname>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep), qnm)) {
            uint32_t jstate;
            DPRINTF("found " sge_u32"." sge_u32"\n", ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep));
            jstate = ocs::tom::get<JAT_state>(jatep);
            if (how == SGE_SIGSTOP)
               jstate |= JSUSPENDED_ON_SUBORDINATE;
            else
               jstate &= ~JSUSPENDED_ON_SUBORDINATE;
            ocs::tom::set<JAT_state>(jatep, jstate);
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
 * @param jep the job, or nullptr to signal every job in the queue
 * @param jatep the array task, or nullptr
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
sge_signal_queue(int how, lListElem *qep, lListElem *jep, lListElem *jatep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int i;
   sge_pack_buffer pb;
   int sent = 0;

   uint64_t now = sge_get_gmt64();

   DEBUG("queue_signal: %d, queue: %s, job: " sge_u32 ", jatask: " sge_u32, how,
      (qep != nullptr ? ocs::tom::get<QU_full_name>(qep) : "none"),
      jep != nullptr ? ocs::tom::get<JB_job_number>(jep) : 0,
      jatep != nullptr ? ocs::tom::get<JAT_task_number>(jatep) : 0);

   if (!jep && (how == SGE_SIGSTOP || how == SGE_SIGCONT)) {
      sge_propagate_queue_suspension(ocs::tom::get<QU_full_name>(qep), how);
   }

   /* don't try to signal unheard queues */
   if (!qinstance_state_is_unknown(qep)) {
      const char *hnm = ocs::tom::get<QU_qhostname>(qep);
      const char *pnm = to_cstr(EXECD);

      if ((i = init_packbuffer(&pb, 256)) == PACK_SUCCESS) {
         /* identifier for acknowledgement */
         if (jep) {
            /*
             * Due to IZ 1619: pack signal only if
             *    job is a non-parallel job
             *    or all slaves of the parallel job have been acknowledged
             */
            if (!ocs::tom::get<JAT_master_queue>(jatep) ||
                is_pe_master_task_send(jatep)) {
               /* TAG_SIGJOB */
               packint(&pb, ocs::tom::get<JB_job_number>(jep));
               packint(&pb, ocs::tom::get<JAT_task_number>(jatep));
               packstr(&pb, nullptr);
               packint(&pb, how);
            }
         } else {
            /* TAG_SIGQUEUE */
            packint(&pb, 0);
            packint(&pb, 0);
            packstr(&pb, ocs::tom::get<QU_full_name>(qep));
            packint(&pb, how);
         }

         if (mconf_get_simulate_execds()) {
            // @todo no need to do the packing above, best split the function in two
            // @todo check if the job is already in deletion?
            i = CL_RETVAL_OK;
            if (jep && how == SGE_SIGKILL)
               trigger_job_resend(sge_get_gmt64(), nullptr, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep), 1);
         } else {
            if (pb_filled(&pb)) {
               uint32_t dummy = 0;
               i = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, pnm, 1, hnm,
                                                               jep ? ocs::gdi::ClientServerBase::TAG_SIGJOB : ocs::gdi::ClientServerBase::TAG_SIGQUEUE, &pb, &dummy);
            }
         }

         MONITOR_MESSAGES_OUT(monitor);
         clear_packbuffer(&pb);
      } else {
         i = CL_RETVAL_MALLOC;  /* an error */
      }

      if (i != CL_RETVAL_OK) {
         ERROR(MSG_COM_NOUPDATEQSTATE_IS, how, ocs::tom::get<QU_full_name>(qep));
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
              ocs::tom::get<JB_job_number>(jep), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              static_cast<const char *>(ocs::tom::get<QU_qhostname>(qep)));
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, ocs::tom::get<JB_job_number>(jep),
                               ocs::tom::get<JAT_task_number>(jatep), nullptr);

      if (!mconf_get_simulate_execds()) {
         ocs::tom::set<JAT_pending_signal>(jatep, how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT,
                           ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatep), nullptr);
         te_add_event(ev);
         te_free_event(&ev);
         ocs::tom::set<JAT_pending_signal_delivery_time>(jatep, next_delivery_time);
      }
   } else {
      te_event_t ev = nullptr;

      DPRINTF("QUEUE %s: %s signal %s (retry after " sge_u64" microseconds) host %s\n",
              ocs::tom::get<QU_full_name>(qep), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              static_cast<const char *>(ocs::tom::get<QU_qhostname>(qep)));
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, 0, 0, ocs::tom::get<QU_full_name>(qep));

      if (!mconf_get_simulate_execds()) {
         ocs::tom::set<QU_pending_signal>(qep, how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0,
                           ocs::tom::get<QU_full_name>(qep));
         te_add_event(ev);
         te_free_event(&ev);
         ocs::tom::set<QU_pending_signal_delivery_time>(qep, next_delivery_time);
      }
   }

   if (!jep) {/* signalling a queue ? - handle slave jobs in this queue */
      signal_slave_jobs_in_queue(how, qep, monitor);
   } else {/* is this the master queue of this job to signal ? - then decide whether slave tasks also
           must get signalled */
      if (!strcmp(ocs::tom::get<JG_qname>(lFirst(static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)))), ocs::tom::get<QU_full_name>(qep))) {
         signal_slave_tasks_of_job(how, jep, jatep, monitor);
      }
   }

   DRETURN(0);
} /* sge_signal_queue() */

/* in case we have to signal a queue
   in which slave tasks are running
   we have to notify the master execd
   where the master task of this job is running
*/
static void
signal_slave_jobs_in_queue(int how, lListElem *qep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const lList *gdil_lp;
   lListElem *mq;
   lListElem *jep, *jatep;
   const char *qname, *mqname, *pe_name;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);

   qname = ocs::tom::get<QU_full_name>(qep);
   /* test whether there are parallel jobs
      with a slave slot in this queue
      if so then signal this job */
   for_each_rw (jep, master_job_list) {
      for_each_rw (jatep, static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jep))) {

         /* skip sequential and not running jobs */
         if (lGetNumberOfElem(gdil_lp = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) <= 1)
            continue;

         /* signalling of not "slave controlled" parallel jobs will not work
            since they are not known to the appropriate execd - we should
            omit signalling in this case to prevent waste of communication bandwidth */
         if (!(pe_name = ocs::tom::get<JAT_granted_pe>(jatep)) || !pe_list_locate(master_pe_list, pe_name))
            continue;

         if (static_cast<const lListElem *>(ocs::tom::find_first<JG_qname>(gdil_lp, qname)) != nullptr) {

            /* search master queue - needed for signalling of a job */
            if ((mq = cqueue_list_locate_qinstance(master_cqueue_list,
                                                   mqname = ocs::tom::get<JG_qname>(lFirst(gdil_lp))))) {
               DPRINTF("found slave job " sge_u32" in queue %s master queue is %s\n",
                       ocs::tom::get<JB_job_number>(jep), qname, mqname);
               sge_signal_queue(how, mq, jep, jatep, monitor);
            } else {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, mqname, ocs::tom::get<JB_job_number>(jep));
            }
         }
      }
   }

   DRETURN_VOID;
}

static void
signal_slave_tasks_of_job(int how, lListElem *jep, lListElem *jatep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const lList *gdil_lp;
   lListElem *mq, *pe;
   const lListElem *gdil_ep;
   const char *qname, *pe_name;
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   /* do not signal slave tasks in case of checkpointing jobs with
      STOP/CONT when suspending means migration */
   if ((how == SGE_SIGCONT || how == SGE_SIGSTOP) &&
       ISSET(ocs::tom::get<JB_checkpoint_attr>(jep), CHECKPOINT_SUSPEND)) {
      DPRINTF("omit signaling - checkpoint script does action for whole job\n");
      return;
   }

   /* forward signal to slave exec hosts
      in case of slave controlled jobs */
   if (!((lGetNumberOfElem(gdil_lp = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep))) <= 1 ||
         !(pe_name = ocs::tom::get<JAT_granted_pe>(jatep)) || !(pe = pe_list_locate(master_pe_list, pe_name)) ||
         !ocs::tom::get<PE_control_slaves>(pe)))
      for (gdil_ep = lNext(lFirst(gdil_lp)); gdil_ep; gdil_ep = lNext(gdil_ep))
         if ((mq = cqueue_list_locate_qinstance(master_cqueue_list, qname = ocs::tom::get<JG_qname>(gdil_ep)))) {
            DPRINTF("found slave job " sge_u32" in queue %s\n", ocs::tom::get<JB_job_number>(jep), qname);
            sge_signal_queue(how, mq, jep, jatep, monitor);
         }

   DRETURN_VOID;
}

