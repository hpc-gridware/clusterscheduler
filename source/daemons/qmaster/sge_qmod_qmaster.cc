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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <climits>
#include <fnmatch.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_DataStore.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_pe_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "sge_qmod_qmaster.h"
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
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, u_long32 action,
                       u_long32 force, lList **answer, monitoring_t *monitor);

static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep,
                     u_long32 task_id, u_long32 action, u_long32 force, lList **answer, monitoring_t *monitor);

static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, u_long32 force, lList **answer,
                     int isoperator, int isowner, monitoring_t *monitor);

static int
qmod_queue_clean(const ocs::gdi::Packet *packet, lListElem *qep, u_long32 force, lList **answer,
                 int isoperator, int isowner, monitoring_t *monitor);

static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                 lList **answer, monitoring_t *monitor);

static void
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                   lList **answer, monitoring_t *monitor);

static void
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                    lList **answer, monitoring_t *monitor);

/*-------------------------------------------------------------------------*/

void
sge_gdi_qmod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   lList *alp = nullptr;
   const lListElem *dep;
   lListElem *jatask = nullptr, *job, *tmp_task;
   const lListElem *rn;
   bool found;
   u_long32 jobid;
   u_long32 start = 0, end = 0, step = 0;
   int alltasks;
   dstring cqueue_buffer = DSTRING_INIT;
   dstring hostname_buffer = DSTRING_INIT;
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   /*
   ** loop over the ids and change queue or job state and signal them
   ** if necessary
   */
   for_each_ep(dep, task->data_list) {
      lList *tmp_list = nullptr;
      lList *qref_list = nullptr;
      bool found_something = true;
      u_long32 id_action = lGetUlong(dep, ID_action);

      found = false;

      if ((id_action & JOB_DO_ACTION) == 0) {
         qref_list_add(&qref_list, nullptr, lGetString(dep, ID_str));
         qref_list_resolve_hostname(qref_list);
         qref_list_resolve(qref_list, nullptr, &tmp_list,
                           &found_something, master_cqueue_list,
                           master_hgroup_list,
                           true, true);
         if (found_something) {
            const lListElem *qref = nullptr;

            id_action = (id_action & (~QUEUE_DO_ACTION));

            for_each_ep(qref, tmp_list) {
               const char *full_name = nullptr;
               const char *cqueue_name = nullptr;
               const char *hostname = nullptr;
               const lListElem *cqueue = nullptr;
               lListElem *qinstance = nullptr;
               const lList *qinstance_list = nullptr;

               full_name = lGetString(qref, QR_name);
               if (!cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
                  continue;
               }
               cqueue_name = sge_dstring_get_string(&cqueue_buffer);
               hostname = sge_dstring_get_string(&hostname_buffer);
               cqueue = lGetElemStr(master_cqueue_list, CQ_name, cqueue_name);
               qinstance_list = lGetList(cqueue, CQ_qinstances);
               qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, hostname);

               sge_change_queue_state(packet, task, qinstance, id_action, lGetUlong(dep, ID_force), &alp, monitor);
               found = true;
            }
         }
         lFreeList(&qref_list);
         lFreeList(&tmp_list);
      }
      if (!found) {
         bool is_jobName_suport = false;
         u_long action = lGetUlong(dep, ID_action);
         if ((action & JOB_DO_ACTION) > 0 &&
             (action & QUEUE_DO_ACTION) == 0) {
            action = (action & (~JOB_DO_ACTION));
            is_jobName_suport = true;
         }

         /*
         ** We found no queue so look for a job. This only makes sense for
         ** suspend, unsuspend and reschedule
         */
         if (sge_strisint(lGetString(dep, ID_str)) &&
             (action == QI_DO_SUSPEND ||
              action == QI_DO_RESCHEDULE ||
              action == QI_DO_CLEARERROR ||
              action == QI_DO_UNSUSPEND)) {
            jobid = strtol(lGetString(dep, ID_str), nullptr, 10);

            rn = lFirst(lGetList(dep, ID_ja_structure));
            if (rn) {
               start = lGetUlong(rn, RN_min);
               if (start) {
                  end = lGetUlong(rn, RN_max);
                  step = lGetUlong(rn, RN_step);
                  if (!step)
                     step = 1;
                  alltasks = 0;
               } else {
                  start = 1;
                  end = (u_long32) LONG_MAX;
                  step = 1;
                  alltasks = 1;
               }
               if (start > end)
                  end = start;

            } else {
               alltasks = 1;
            }

            job = lGetElemUlongRW(master_job_list, JB_job_number, jobid);
            if (job) {
               jatask = lFirstRW(lGetList(job, JB_ja_tasks));

               while ((tmp_task = jatask)) {
                  u_long32 task_number;

                  jatask = lNextRW(tmp_task);
                  task_number = lGetUlong(tmp_task, JAT_task_number);
                  if ((task_number >= start && task_number <= end &&
                       ((task_number - start) % step) == 0) || alltasks) {
                     DPRINTF("Modify job: " sge_u32"." sge_u32"\n", jobid, task_number);

                     /* this specifies no queue, so lets probe for a job */
                     /* change state of job: */
                     sge_change_job_state(packet, task, job, tmp_task, 0, action, lGetUlong(dep, ID_force), &alp, monitor);
                     found = true;
                  }
               }

               /* create more precise GDI answers also for pending jobs/tasks and jobs/tasks in hold state
                  When the operation is to be applied on the whole job array but no task is enrolled so far
                  (i.e. not found) only one single GDI answer is created. Otherwise one message is created
                  per task */
               if (alltasks && job_is_array(job)) {
                  if (!found) {
                     sge_change_job_state(packet, task, job, nullptr, 0, action, lGetUlong(dep, ID_force), &alp, monitor);
                     found = true;
                  }
               } else {
                  const lListElem *range;
                  u_long32 min, max, step;
                  u_long32 taskid;

                  /* handle all pending tasks */
                  for_each_ep(range, lGetList(job, JB_ja_n_h_ids)) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32"." sge_u32"\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, lGetUlong(dep, ID_force), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in user hold */
                  for_each_ep(range, lGetList(job, JB_ja_u_h_ids)) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32"." sge_u32"\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, lGetUlong(dep, ID_force), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in system hold that are not in user hold */
                  for_each_ep(range, lGetList(job, JB_ja_s_h_ids)) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32"." sge_u32"\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, lGetUlong(dep, ID_force), &alp, monitor);
                           found = true;
                        }
                     }
                  }

                  /* handle all tasks in operator hold that are not in user hold or system hold */
                  for_each_ep(range, lGetList(job, JB_ja_o_h_ids)) {
                     range_get_all_ids(range, &min, &max, &step);
                     for (taskid = min; taskid <= max; taskid += step) {
                        if (range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), taskid) ||
                            range_list_is_id_within(lGetList(job, JB_ja_s_h_ids), taskid)) {
                           continue;
                        }
                        if ((taskid >= start && taskid <= end &&
                             ((taskid - start) % step) == 0) || alltasks) {
                           DPRINTF("Modify job: " sge_u32"." sge_u32"\n", jobid, taskid);
                           sge_change_job_state(packet, task, job, nullptr, taskid, action, lGetUlong(dep, ID_force), &alp, monitor);
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

            const char *job_name = lGetString(dep, ID_str);
            const lListElem *job;
            lListElem *mod = nullptr;
            for_each_ep(job, master_job_list) {
               if (!fnmatch(job_name, lGetString(job, JB_job_name), 0)) {
                  char job_id[40];
                  mod = lCopyElem(dep);
                  snprintf(job_id, sizeof(job_id), sge_u32, lGetUlong(job, JB_job_number));
                  lSetString(mod, ID_str, job_id);
                  lAppendElem(task->data_list, mod);
                  found = true;
               }
            }
         } else {
            /* job id invalid or action invalid for jobs */

         }
      }

      if (!found) {
         u_long action = lGetUlong(dep, ID_action);
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
            ERROR(MSG_QUEUE_INVALIDQORJOB_S, lGetString(dep, ID_str));
         } else {
            ERROR(MSG_QUEUE_INVALIDQ_S, lGetString(dep, ID_str));
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
sge_change_queue_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, u_long32 action,
                       u_long32 force, lList **answer, monitoring_t *monitor) {
   bool isoperator;
   bool isowner;
   int result = 0;
   const char *ehname = lGetHost(qep, QU_qhostname);
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   isowner = qinstance_check_owner(packet, qep, master_manager_list, master_operator_list);
   isoperator = manop_is_operator(packet, master_manager_list, master_operator_list);

   if (!isowner) {
      ERROR(MSG_QUEUE_NOCHANGEQPERMS_SS, packet->user, lGetString(qep, QU_full_name));
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
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
         INFO(MSG_LOG_QUNKNOWNQMODCMD_U, sge_u32c(action));
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
         cqueue_list_del_all_orphaned(master_cqueue_list, answer, lGetString(qep, QU_qname), ehname, packet->gdi_session);
         break;
      default:
         break;
   }

   DRETURN(result);
}

static int
sge_change_job_state(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep,
                     u_long32 task_id, u_long32 action, u_long32 force, lList **answer, monitoring_t *monitor) {
   lListElem *queueep;
   u_long32 job_id;

   DENTER(TOP_LAYER);
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   job_id = lGetUlong(jep, JB_job_number);

   /* check the modifying users permissions */
   if (strcmp(packet->user, lGetString(jep, JB_owner)) && !manop_is_operator(packet, master_manager_list, master_operator_list)) {
      ERROR(MSG_JOB_NOMODJOBPERMS_SU, packet->user, sge_u32c(job_id));
      answer_list_add(answer, SGE_EVENT, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   if (!jatep) {
      /* unenrolled tasks always are not-running pending/hold */
      if (task_id) {
         WARNING(MSG_QMODJOB_NOTENROLLED_UU, sge_u32c(job_id), sge_u32c(task_id));
      } else {
         WARNING(MSG_QMODJOB_NOTENROLLED_U, sge_u32c(job_id));
      }
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      DRETURN(-1);
   }

   task_id = lGetUlong(jatep, JAT_task_number);

   if (lGetString(jatep, JAT_master_queue)) {
      queueep = cqueue_list_locate_qinstance(master_cqueue_list, lGetString(jatep, JAT_master_queue));
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
         if (VALID(JERROR, lGetUlong(jatep, JAT_state))) {
            lSetUlong(jatep, JAT_state, lGetUlong(jatep, JAT_state) & ~JERROR);
            ja_task_message_trash_all_of_type_X(jatep, 1);
/* lWriteElemTo(jatep, stderr); */
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            job_id, task_id, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
            if (job_is_array(jep)) {
               INFO(MSG_JOB_CLEARERRORTASK_SSUU, packet->user, packet->host, sge_u32c(job_id), sge_u32c(task_id));
            } else {
               INFO(MSG_JOB_CLEARERRORJOB_SSU, packet->user, packet->host, sge_u32c(job_id));
            }
         } else {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_NOERRORSTATETASK_UU, sge_u32c(job_id), sge_u32c(task_id));
            } else {
               INFO(MSG_JOB_NOERRORSTATEJOB_UU, sge_u32c(job_id));
            }
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;

      default:
         INFO(MSG_LOG_JOBUNKNOWNQMODCMD_U, sge_u32c(action));
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;
   }

   DRETURN(0);
}

/****
 **** qmod_queue_weakclean (static)
 ****/
static int
qmod_queue_weakclean(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *qep, u_long32 force, lList **answer,
                     int isoperator, int isowner, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   if (!isoperator && !isowner) {
      ERROR(MSG_QUEUE_NORESCHEDULEQPERMS_SS, packet->user, lGetString(qep, QU_full_name));
      answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   reschedule_jobs(qep, force, answer, monitor, true, packet->gdi_session);

   DRETURN(0);
}

/****
 **** qmod_queue_clean (static)
 ****
 **** cleans the specified queue (every job will be deleted)
 **** The user will do this via qconf -cq <qname>
 ****/
static int
qmod_queue_clean(const ocs::gdi::Packet *packet, lListElem *qep, u_long32 force, lList **answer,
                 int isoperator, int isowner, monitoring_t *monitor) {
   lListElem *nextjep, *jep;
   const char *qname = nullptr;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   qname = lGetString(qep, QU_full_name);

   DPRINTF("cleaning queue >%s<\n", qname);

   if (!manop_is_manager(packet, master_manager_list)) {
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

      nexttep = lFirstRW(lGetList(jep, JB_ja_tasks));
      while ((jatep = nexttep)) {
         nexttep = lNextRW(jatep);

         if (lGetSubStr(jatep, JG_qname, qname, JAT_granted_destin_identifier_list) != nullptr) {
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
qmod_job_reschedule(ocs::gdi::Packet *packet, ocs::gdi::Task *task,  lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                    lList **answer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   reschedule_job(jep, jatep, queueep, force, answer, monitor, true, packet->gdi_session);

   DRETURN_VOID;
}

/****
 **** qmod_job_suspend (static)
 ****/
static void
qmod_job_suspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                 lList **answer, monitoring_t *monitor) {
   int i;
   u_long32 state = 0;
   u_long32 jataskid = 0;
   u_long32 jobid = 0;
   bool migrate_on_suspend = false;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();

   jobid = lGetUlong(jep, JB_job_number);
   jataskid = lGetUlong(jatep, JAT_task_number);

   /* determine whether we actually migrate upon suspend */
   if (lGetUlong(jep, JB_checkpoint_attr) & CHECKPOINT_SUSPEND)
      migrate_on_suspend = true;

   if (VALID(JSUSPENDED, lGetUlong(jatep, JAT_state))) {
      /* this job is already suspended or lives in a suspended queue */
      if (force && queueep) {
         /* here force means to send the suspend signal again
            this can only be done if we know the queue this job
            runs in */
         if (sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatep, monitor)) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOFORCESUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_NOFORCESUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_FORCESUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_FORCESUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else {
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_ALREADYSUSPENDED_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
         } else {
            WARNING(MSG_JOB_ALREADYSUSPENDED_SU, packet->user, sge_u32c(jobid));
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      }

      /*
      ** may be the queue is suspended, than the job might not be
      */
      state = lGetUlong(jatep, JAT_state);
      CLEARBIT(JRUNNING, state);
      SETBIT(JSUSPENDED, state);
      lSetUlong(jatep, JAT_state, state);
      if (migrate_on_suspend) {
         lSetUlong64(jatep, JAT_stop_initiate_time, now);
      }

      sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                      jobid, jataskid, nullptr, nullptr, nullptr,
                      jep, jatep, nullptr, true, true, packet->gdi_session);
   } else {   /* job wasn't suspended yet */
      if (queueep) {
         if ((i = sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatep, monitor))) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOSUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_NOSUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else
         i = 1;

      if (force) {
         /* set jobs state to suspend in all cases */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_FORCESUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               INFO(MSG_JOB_FORCESUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }

         state = lGetUlong(jatep, JAT_state);
         CLEARBIT(JRUNNING, state);
         SETBIT(JSUSPENDED, state);
         lSetUlong(jatep, JAT_state, state);
         if (migrate_on_suspend) {
            lSetUlong64(jatep, JAT_stop_initiate_time, now);
         }
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
      } else {
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_SUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               INFO(MSG_JOB_SUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

            state = lGetUlong(jatep, JAT_state);
            CLEARBIT(JRUNNING, state);
            SETBIT(JSUSPENDED, state);
            lSetUlong(jatep, JAT_state, state);
            if (migrate_on_suspend) {
               lSetUlong64(jatep, JAT_stop_initiate_time, now);
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
qmod_job_unsuspend(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lListElem *jatep, lListElem *queueep, u_long32 force,
                   lList **answer, monitoring_t *monitor) {
   int i;
   u_long32 state = 0;
   u_long32 jobid, jataskid;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();

   jobid = lGetUlong(jep, JB_job_number);
   jataskid = lGetUlong(jatep, JAT_task_number);

   /* admin suspend may not override suspend from threshold */
   if (VALID(JSUSPENDED_ON_THRESHOLD, lGetUlong(jatep, JAT_state))) {
      if (VALID(JSUSPENDED, lGetUlong(jatep, JAT_state))) {
         if (job_is_array(jep)) {
            INFO(MSG_JOB_RMADMSUSPENDTASK_SSUU, packet->user, packet->host, sge_u32c(jobid), sge_u32c(jataskid));
         } else {
            INFO(MSG_JOB_RMADMSUSPENDJOB_SSU, packet->user, packet->host, sge_u32c(jobid));
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

         state = lGetUlong(jatep, JAT_state);
         CLEARBIT(JSUSPENDED, state);
         lSetUlong(jatep, JAT_state, state);
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host, nullptr, jep, jatep, nullptr, nullptr);
         DRETURN_VOID;
      } else {
         /* guess admin tries to remove threshold suspension by qmon -us <jobid> */
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_NOADMSUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
         } else {
            WARNING(MSG_JOB_NOADMSUSPENDJOB_SU, packet->user, sge_u32c(jobid));
         }
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         DRETURN_VOID;
      }
   }

   if (VALID(JRUNNING, lGetUlong(jatep, JAT_state))) {
      /* this job is already running */
      if (force && queueep) {
         /*
         ** here force means to send the cont signal again
         ** this can only be done if we know the queue this job
         ** runs in
         */
         if (sge_signal_queue(SGE_SIGCONT, queueep, jep, jatep, monitor)) {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_NOFORCEENABLETASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_NOFORCEENABLEJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            if (job_is_array(jep)) {
               WARNING(MSG_JOB_FORCEENABLETASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_FORCEENABLEJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else {
         if (job_is_array(jep)) {
            WARNING(MSG_JOB_ALREADYUNSUSPENDED_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
         } else {
            WARNING(MSG_JOB_ALREADYUNSUSPENDED_SU, packet->user, sge_u32c(jobid));
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
               WARNING(MSG_JOB_NOUNSUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               WARNING(MSG_JOB_NOUNSUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
      } else
         i = 1;

      if (force) {
         /* set jobs state to suspend in all cases */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_FORCEUNSUSPTASK_SSUU, packet->user, packet->host, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               INFO(MSG_JOB_FORCEUNSUSPJOB_SSU, packet->user, packet->host, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);
         }

         state = lGetUlong(jatep, JAT_state);
         SETBIT(JRUNNING, state);
         CLEARBIT(JSUSPENDED, state);
         lSetUlong(jatep, JAT_state, state);
         sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                         jobid, jataskid, nullptr, nullptr, nullptr,
                         jep, jatep, nullptr, true, true, packet->gdi_session);
      } else {
         /* set job state only if communication works */
         if (!i) {
            if (job_is_array(jep)) {
               INFO(MSG_JOB_UNSUSPENDTASK_SUU, packet->user, sge_u32c(jobid), sge_u32c(jataskid));
            } else {
               INFO(MSG_JOB_UNSUSPENDJOB_SU, packet->user, sge_u32c(jobid));
            }
            answer_list_add(answer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);

            state = lGetUlong(jatep, JAT_state);
            SETBIT(JRUNNING, state);
            CLEARBIT(JSUSPENDED, state);
            lSetUlong(jatep, JAT_state, state);
            sge_event_spool(answer, 0, sgeE_JATASK_MOD,
                            jobid, jataskid, nullptr, nullptr, nullptr,
                            jep, jatep, nullptr, true, true, packet->gdi_session);
         }
      }
   }
   ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_UNSUSPENDED, packet->user, packet->host, nullptr, jep, jatep, nullptr, nullptr);
   DRETURN_VOID;
}


void
rebuild_signal_events() {
   const lListElem *cqueue, *jep, *jatep;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   /* J O B */
   for_each_ep(jep, master_job_list) {
      for_each_ep(jatep, lGetList(jep, JB_ja_tasks)) {
         u_long64 when = lGetUlong64(jatep, JAT_pending_signal_delivery_time);

         if (lGetUlong(jatep, JAT_pending_signal) && (when > 0)) {
            u_long32 key1 = lGetUlong(jep, JB_job_number);
            u_long32 key2 = lGetUlong(jatep, JAT_task_number);
            te_event_t ev = nullptr;

            ev = te_new_event(when, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, key1, key2, nullptr);
            te_add_event(ev);
            te_free_event(&ev);
         }
      }
   }

   /* Q U E U E */
   for_each_ep(cqueue, master_cqueue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      const lListElem *qinstance;

      for_each_ep(qinstance, qinstance_list) {
         u_long64 when = lGetUlong64(qinstance, QU_pending_signal_delivery_time);

         if (lGetUlong(qinstance, QU_pending_signal) && when > 0) {
            const char *str_key = lGetString(qinstance, QU_full_name);
            te_event_t ev = nullptr;

            ev = te_new_event(when, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, str_key);
            te_add_event(ev);
            te_free_event(&ev);
         }
      }
   }

   DRETURN_VOID;
} /* rebuild_signal_events() */

/* this function is called by our timer mechanism for resending signals */
void
resend_signal_event(te_event_t anEvent, monitoring_t *monitor) {
   lListElem *qep, *jep, *jatep;
   u_long32 jobid = te_get_first_numeric_key(anEvent);
   u_long32 jataskid = te_get_second_numeric_key(anEvent);
   const char *queue = te_get_alphanumeric_key(anEvent);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   if (queue == nullptr) {
      if (!(jep = lGetElemUlongRW(master_job_list, JB_job_number, jobid)) ||
          !(jatep = job_search_task(jep, nullptr, jataskid))) {
         ERROR(MSG_EVE_RESENTSIGNALTASK_UU, sge_u32c(jobid), sge_u32c(jataskid));
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if ((qep = cqueue_list_locate_qinstance(master_cqueue_list, lGetString(jatep, JAT_master_queue)))) {
         sge_signal_queue(lGetUlong(jatep, JAT_pending_signal), qep, jep, jatep, monitor);
      }
   } else {
      if (!(qep = cqueue_list_locate_qinstance(master_cqueue_list, queue))) {
         ERROR(MSG_EVE_RESENTSIGNALQ_S, queue);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         sge_free(&queue);
         DRETURN_VOID;
      }

      sge_signal_queue(lGetUlong(qep, QU_pending_signal), qep, nullptr, nullptr, monitor);
   }

   sge_free(&queue);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

static void
sge_propagate_queue_suspension(const char *qnm, int how) {
   const lListElem *jep;
   lListElem *jatep;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   DPRINTF("searching for all jobs in queue %s due to %s\n", qnm, sge_sig2str(how));
   for_each_ep(jep, master_job_list) {
      for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
         if (lGetElemStr(lGetList(jatep, JAT_granted_destin_identifier_list), JG_qname, qnm)) {
            u_long32 jstate;
            DPRINTF("found " sge_u32"." sge_u32"\n", lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number));
            jstate = lGetUlong(jatep, JAT_state);
            if (how == SGE_SIGSTOP)
               jstate |= JSUSPENDED_ON_SUBORDINATE;
            else
               jstate &= ~JSUSPENDED_ON_SUBORDINATE;
            lSetUlong(jatep, JAT_state, jstate);
         }
      }
   }

   DRETURN_VOID;
}

/************************************************************************
 This is called by the qmaster to:
 - send a signal to all jobs in a queue (job_number == 0);
 - send a signal to one job
 ************************************************************************/
int
sge_signal_queue(int how, lListElem *qep, lListElem *jep, lListElem *jatep, monitoring_t *monitor) {
   int i;
   sge_pack_buffer pb;
   int sent = 0;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();

   DEBUG("queue_signal: %d, queue: %s, job: " sge_uu32 ", jatask: " sge_uu32, how,
      (qep != nullptr ? lGetString(qep, QU_full_name) : "none"),
      jep != nullptr ? lGetUlong(jep, JB_job_number) : 0,
      jatep != nullptr ? lGetUlong(jatep, JAT_task_number) : 0);

   if (!jep && (how == SGE_SIGSTOP || how == SGE_SIGCONT)) {
      sge_propagate_queue_suspension(lGetString(qep, QU_full_name), how);
   }

   /* don't try to signal unheard queues */
   if (!qinstance_state_is_unknown(qep)) {
      const char *hnm, *pnm;

      pnm = prognames[EXECD];
      hnm = lGetHost(qep, QU_qhostname);

      if ((i = init_packbuffer(&pb, 256, 0)) == PACK_SUCCESS) {
         /* identifier for acknowledgement */
         if (jep) {
            /*
             * Due to IZ 1619: pack signal only if
             *    job is a non-parallel job
             *    or all slaves of the parallel job have been acknowledged
             */
            if (!lGetString(jatep, JAT_master_queue) ||
                is_pe_master_task_send(jatep)) {
               /* TAG_SIGJOB */
               packint(&pb, lGetUlong(jep, JB_job_number));
               packint(&pb, lGetUlong(jatep, JAT_task_number));
               packstr(&pb, nullptr);
               packint(&pb, how);
            }
         } else {
            /* TAG_SIGQUEUE */
            packint(&pb, 0);
            packint(&pb, 0);
            packstr(&pb, lGetString(qep, QU_full_name));
            packint(&pb, how);
         }

         if (mconf_get_simulate_execds()) {
            // @todo no need to do the packing above, best split the function in two
            // @todo check if the job is already in deletion?
            i = CL_RETVAL_OK;
            if (jep && how == SGE_SIGKILL)
               trigger_job_resend(sge_get_gmt64(), nullptr, lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number), 1);
         } else {
            if (pb_filled(&pb)) {
               u_long32 dummy = 0;
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
         ERROR(MSG_COM_NOUPDATEQSTATE_IS, how, lGetString(qep, QU_full_name));
         DRETURN(i);
      }
      sent = 1;
   }

   u_long64 next_delivery_time = now + sge_gmt32_to_gmt64(60);

   /* If this is a operation on one job we enter the signal request in the
      job structure. If the operation is not acknowledged in time we can do
      further steps */
   if (jep) {
      te_event_t ev = nullptr;

      DPRINTF("JOB " sge_u32": %s signal %s (retry after " sge_u64" microseconds) host: %s\n",
              lGetUlong(jep, JB_job_number), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              lGetHost(qep, QU_qhostname));
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, lGetUlong(jep, JB_job_number),
                               lGetUlong(jatep, JAT_task_number), nullptr);

      if (!mconf_get_simulate_execds()) {
         lSetUlong(jatep, JAT_pending_signal, how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT,
                           lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number), nullptr);
         te_add_event(ev);
         te_free_event(&ev);
         lSetUlong64(jatep, JAT_pending_signal_delivery_time, next_delivery_time);
      }
   } else {
      te_event_t ev = nullptr;

      DPRINTF("QUEUE %s: %s signal %s (retry after " sge_u64" microseconds) host %s\n",
              lGetString(qep, QU_full_name), sent ? "sent" : "queued", sge_sig2str(how), next_delivery_time - now,
              lGetHost(qep, QU_qhostname));
      te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, 0, 0, lGetString(qep, QU_full_name));

      if (!mconf_get_simulate_execds()) {
         lSetUlong(qep, QU_pending_signal, how);
         ev = te_new_event(next_delivery_time, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0,
                           lGetString(qep, QU_full_name));
         te_add_event(ev);
         te_free_event(&ev);
         lSetUlong64(qep, QU_pending_signal_delivery_time, next_delivery_time);
      }
   }

   if (!jep) {/* signalling a queue ? - handle slave jobs in this queue */
      signal_slave_jobs_in_queue(how, qep, monitor);
   } else {/* is this the master queue of this job to signal ? - then decide whether slave tasks also
           must get signalled */
      if (!strcmp(lGetString(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)),
                             JG_qname), lGetString(qep, QU_full_name))) {
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
   const lList *gdil_lp;
   lListElem *mq;
   lListElem *jep, *jatep;
   const char *qname, *mqname, *pe_name;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);

   DENTER(TOP_LAYER);

   qname = lGetString(qep, QU_full_name);
   /* test whether there are parallel jobs
      with a slave slot in this queue
      if so then signal this job */
   for_each_rw (jep, master_job_list) {
      for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {

         /* skip sequential and not running jobs */
         if (lGetNumberOfElem(gdil_lp = lGetList(jatep, JAT_granted_destin_identifier_list)) <= 1)
            continue;

         /* signalling of not "slave controlled" parallel jobs will not work
            since they are not known to the appropriate execd - we should
            omit signalling in this case to prevent waste of communication bandwidth */
         if (!(pe_name = lGetString(jatep, JAT_granted_pe)) || !pe_list_locate(master_pe_list, pe_name))
            continue;

         if (lGetElemStr(gdil_lp, JG_qname, qname) != nullptr) {

            /* search master queue - needed for signalling of a job */
            if ((mq = cqueue_list_locate_qinstance(master_cqueue_list,
                                                   mqname = lGetString(lFirst(gdil_lp), JG_qname)))) {
               DPRINTF("found slave job " sge_u32" in queue %s master queue is %s\n",
                       lGetUlong(jep, JB_job_number), qname, mqname);
               sge_signal_queue(how, mq, jep, jatep, monitor);
            } else {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, mqname, sge_u32c(lGetUlong(jep, JB_job_number)));
            }
         }
      }
   }

   DRETURN_VOID;
}

static void
signal_slave_tasks_of_job(int how, lListElem *jep, lListElem *jatep, monitoring_t *monitor) {
   const lList *gdil_lp;
   lListElem *mq, *pe;
   const lListElem *gdil_ep;
   const char *qname, *pe_name;
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   /* do not signal slave tasks in case of checkpointing jobs with
      STOP/CONT when suspending means migration */
   if ((how == SGE_SIGCONT || how == SGE_SIGSTOP) &&
       ISSET(lGetUlong(jep, JB_checkpoint_attr), CHECKPOINT_SUSPEND)) {
      DPRINTF("omit signaling - checkpoint script does action for whole job\n");
      return;
   }

   /* forward signal to slave exec hosts
      in case of slave controlled jobs */
   if (!((lGetNumberOfElem(gdil_lp = lGetList(jatep, JAT_granted_destin_identifier_list))) <= 1 ||
         !(pe_name = lGetString(jatep, JAT_granted_pe)) || !(pe = pe_list_locate(master_pe_list, pe_name)) ||
         !lGetBool(pe, PE_control_slaves)))
      for (gdil_ep = lNext(lFirst(gdil_lp)); gdil_ep; gdil_ep = lNext(gdil_ep))
         if ((mq = cqueue_list_locate_qinstance(master_cqueue_list, qname = lGetString(gdil_ep, JG_qname)))) {
            DPRINTF("found slave job " sge_u32" in queue %s\n", lGetUlong(jep, JB_job_number), qname);
            sge_signal_queue(how, mq, jep, jatep, monitor);
         }

   DRETURN_VOID;
}

