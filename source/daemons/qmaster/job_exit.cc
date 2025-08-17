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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_answer.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_give_jobs.h"
#include "execution_states.h"
#include "symbols.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_job_enforce_limit.h"
#include "msg_common.h"
#include "msg_qmaster.h"

/************************************************************************
 Master routine for job exit

 We need a rusage struct filled.
 In normal cases this is done by the execd, sending this structure
 to notify master about job finish.

 In case of an error noticed by the master which needs the job to be 
 removed we can fill this structure by hand. We need:

 rusage->job_number
 rusage->qname to clean up the queue (if we didn't find it we nevertheless
               clean up the job

 for functions regarding rusage see sge_rusage.c
 ************************************************************************/
void
sge_job_exit(lListElem *jr, lListElem *jep, lListElem *jatep, monitoring_t *monitor, u_long64 gdi_session) {
   DENTER(TOP_LAYER);
   lListElem *queueep = nullptr;
   const char *err_str = nullptr;
   const char *qname = nullptr;
   const char *hostname = MSG_OBJ_UNKNOWNHOST;
   u_long32 jobid, jataskid;
   const lListElem *hep = nullptr;
   u_long32 failed, general_failure;
   lList *saved_gdil;

   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   lList *master_ar_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_AR);
   lList *master_ehost_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);

   /* JG: TODO: we'd prefer some more precise timestamp, e.g. from jr */
   u_long64 timestamp = sge_get_gmt64();

   qname = lGetString(jr, JR_queue_name);
   if (qname == nullptr) {
      qname = (char *) MSG_OBJ_UNKNOWNQ;
   }

   err_str = lGetString(jr, JR_err_str);
   if (err_str == nullptr) {
      err_str = MSG_UNKNOWNREASON;
   }

   jobid = lGetUlong(jr, JR_job_number);
   jataskid = lGetUlong(jr, JR_ja_task_number);
   failed = lGetUlong(jr, JR_failed);
   general_failure = lGetUlong(jr, JR_general_failure);

   cancel_job_resend(jobid, jataskid);

   /* This only has a meaning for Hibernator jobs. The job pid must
    * be saved accross restarts, since jobs get their old pid
    */
   lSetUlong(jatep, JAT_pvm_ckpt_pid, lGetUlong(jr, JR_job_pid));

   DPRINTF("reaping job " sge_u32 "." sge_u32 " in queue >%s< job_pid " sge_u32 "\n",
           jobid, jataskid, qname, lGetUlong(jatep, JAT_pvm_ckpt_pid));

   queueep = cqueue_list_locate_qinstance(master_cqueue_list, qname);
   if (queueep == nullptr) {
      ERROR(MSG_JOB_WRITEJFINISH_S, qname);
   }

   sge_job_remove_enforce_limit_trigger(jobid, jataskid);

   /* retrieve hostname for later use */
   if (queueep != nullptr) {
      hostname = lGetHost(queueep, QU_qhostname);
   }

   if (failed) {        /* a problem occurred */
      WARNING(MSG_JOB_FAILEDONHOST_UUSSSS, jobid, jataskid, hostname, general_failure ? MSG_GENERAL : "", get_sstate_description(failed), err_str);
   } else {
      INFO(MSG_JOB_JFINISH_UUS, jobid, jataskid, hostname);
   }

   /*-------------------------------------------------*/

   /* test if this job is in state JRUNNING or JTRANSFERING */
   if (lGetUlong(jatep, JAT_status) != JRUNNING &&
       lGetUlong(jatep, JAT_status) != JTRANSFERING) {
      ERROR(MSG_JOB_JEXITNOTRUN_UU, lGetUlong(jep, JB_job_number), jataskid);
      DRETURN_VOID;
   }

   saved_gdil = lCopyList("cpy", lGetList(jatep, JAT_granted_destin_identifier_list));

   /*
    * case 1: job being trashed because 
    *    --> failed starting interactive job
    *    --> job was deleted
    *    --> a failed batch job that explicitely shall not enter error state
    */
   if (((lGetUlong(jatep, JAT_state) & JDELETED) == JDELETED) || (failed && !lGetString(jep, JB_exec_file)) ||
       (failed && general_failure == GFSTATE_JOB && JOB_TYPE_IS_NO_ERROR(lGetUlong(jep, JB_type)))) {
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      /* JG: TODO: we need more information in the log message */
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_DELETED, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JREMOVED);

      sge_commit_job(jep, jatep, jr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);

      if (lGetUlong(jep, JB_ar) != 0 && (lGetUlong(jatep, JAT_state) & JDELETED) == JDELETED) {
         /* get AR and remove it if no other jobs are debited */
         lListElem *ar = ar_list_locate(master_ar_list, lGetUlong(jep, JB_ar));

         if (ar != nullptr && lGetUlong(ar, AR_state) == AR_DELETED) {
            const lListElem *ar_queue;
            u_long32 ar_id = lGetUlong(ar, AR_id);

            for_each_ep(ar_queue, lGetList(ar, AR_reserved_queues)) {
               if (qinstance_slots_used(ar_queue) != 0) {
                  break;
               }
            }
            if (ar_queue == nullptr) {
               /* no jobs registered in advance reservation */
               ar_do_reservation(ar, false, gdi_session);

               ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_DELETED, "AR deleted", timestamp);
               ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, timestamp);

               lRemoveElem(master_ar_list, &ar);

               sge_event_spool(nullptr, 0, sgeE_AR_DEL, ar_id, 0, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, true, true, gdi_session);
            }
         }
      }
   }
      /*
       * case 2: set job in error state
       *    --> owner requested wrong
       *        -e/-o/-S/-cwd
       *    --> user did not exist at the execution machine
       *    --> application controlled job error
       */
   else if ((failed && general_failure == GFSTATE_JOB)) {
      DPRINTF("set job " sge_u32 "." sge_u32 " in ERROR state\n", lGetUlong(jep, JB_job_number), jataskid);
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      /* JG: TODO: we need more information in the log message */
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_ERROR, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JERRORSET);
      lSetUlong64(jatep, JAT_start_time, 0);
      ja_task_message_add(jatep, 1, err_str);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_FAILED_AND_ERROR, COMMIT_DEFAULT, monitor, gdi_session);
   }
      /*
       * case 3: job being rescheduled because it wasnt even started
       *                            or because it was a general error
       */
   else if (((failed && (failed <= SSTATE_BEFORE_JOB)) ||
             general_failure)) {
      /* JG: TODO: we need more information in the log message */
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_RESTART, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JNOSTARTRESCHEDULE);
      ja_task_message_add(jatep, 1, err_str);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_RESCHEDULED, COMMIT_DEFAULT, monitor, gdi_session);
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      lSetUlong64(jatep, JAT_start_time, 0);
   }
      /*
       * case 4: job being rescheduled because rerun specified or ckpt job
       */
   else if (((failed == ESSTATE_NO_EXITSTATUS) || failed == ESSTATE_DIED_THRU_SIGNAL) &&
            ((lGetUlong(jep, JB_restart) == 1 || (lGetUlong(jep, JB_checkpoint_attr) & ~NO_CHECKPOINT)) ||
             (!lGetUlong(jep, JB_restart) && (queueep != nullptr && lGetBool(queueep, QU_rerun))))) {
      lSetUlong(jatep, JAT_job_restarted, MAX(lGetUlong(jatep, JAT_job_restarted), lGetUlong(jr, JR_ckpt_arena)));
      lSetString(jatep, JAT_osjobid, lGetString(jr, JR_osjobid));
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      /* JG: TODO: we need more information in the log message */
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_RESTART, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JRERUNRESCHEDULE);
      lSetUlong64(jatep, JAT_start_time, 0);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_RESCHEDULED, COMMIT_DEFAULT, monitor, gdi_session);
   }
      /*
       * case 5: job being rescheduled because it was interrupted and a checkpoint exists
       */
   else if (failed == SSTATE_MIGRATE) {
      DTRACE;
      /* job_restarted == 2 means a checkpoint in the ckpt arena */
      lSetUlong(jatep, JAT_job_restarted, MAX(lGetUlong(jatep, JAT_job_restarted), lGetUlong(jr, JR_ckpt_arena)));
      lSetString(jatep, JAT_osjobid, lGetString(jr, JR_osjobid));
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_MIGRATE, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JCKPTRESCHEDULE);
      lSetUlong64(jatep, JAT_start_time, 0);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_RESCHEDULED, COMMIT_DEFAULT, monitor, gdi_session);
   }
      /*
       * case 6: job being rescheduled because of exit 99 
       *                            or because of a rerun e.g. triggered by qmod -r <jobid>
       */
   else if (failed == SSTATE_AGAIN) {
      lSetUlong(jatep, JAT_job_restarted, MAX(lGetUlong(jatep, JAT_job_restarted), lGetUlong(jr, JR_ckpt_arena)));
      lSetString(jatep, JAT_osjobid, lGetString(jr, JR_osjobid));
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_RESTART, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_JNORESRESCHEDULE);
      lSetUlong64(jatep, JAT_start_time, 0);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_USER_RESCHEDULED, COMMIT_DEFAULT, monitor, gdi_session);
   }
      /*
       * case 7: job finished 
       */
   else {
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      ocs::ReportingFileWriter::create_job_logs(nullptr, timestamp, JL_FINISHED, MSG_EXECD, hostname, jr, jep, jatep, nullptr,
                               MSG_LOG_EXITED);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT, monitor, gdi_session);
   }

   if (queueep != nullptr) {
      bool found_host = false;
      lList *answer_list = nullptr;
      /*
      ** in this case we have to halt all queues on this host
      */
      if (general_failure && general_failure == GFSTATE_HOST) {
         hep = host_list_locate(master_ehost_list, lGetHost(queueep, QU_qhostname));
         if (hep != nullptr) {
            const lListElem *cqueue = nullptr;
            const char *host = lGetHost(hep, EH_name);
            dstring error = DSTRING_INIT;

            found_host = true;

            for_each_ep(cqueue, *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE)) {
               const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
               lListElem *qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, host);

               if (qinstance != nullptr) {

                  sge_qmaster_qinstance_state_set_error(qinstance, true, gdi_session);

                  sge_dstring_sprintf(&error, MSG_LOG_QERRORBYJOBHOST_SUS, lGetString(qinstance, QU_qname), jobid, host);
                  qinstance_message_add(qinstance, QI_ERROR, sge_dstring_get_string(&error)); 
                  ERROR(SFNMAX, sge_dstring_get_string(&error));
                  sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, lGetString(qinstance, QU_qname),
                                  lGetHost(qinstance, QU_qhostname), nullptr, qinstance, nullptr, nullptr, true, true, gdi_session);
               }
            }
            sge_dstring_free(&error);
         }
      }
      /*
      ** to be sure this queue is halted even if the host 
      ** is not found in the next statement
      */
      if (general_failure && general_failure != GFSTATE_JOB && !found_host) {
         dstring error = DSTRING_INIT;

         sge_dstring_sprintf(&error, MSG_LOG_QERRORBYJOBHOST_SUS,
                             lGetString(queueep, QU_qname), jobid, hostname);

         /* general error -> this queue cant run any job */
         sge_qmaster_qinstance_state_set_error(queueep, true, gdi_session);
         qinstance_message_add(queueep, QI_ERROR, sge_dstring_get_string(&error));
         ERROR(SFNMAX, sge_dstring_get_string(&error));
         sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, lGetString(queueep, QU_qname),
                         lGetHost(queueep, QU_qhostname), nullptr, queueep, nullptr, nullptr, true, true, gdi_session);
         sge_dstring_free(&error);
      }

      gdil_del_all_orphaned(saved_gdil, &answer_list, gdi_session);
      answer_list_output(&answer_list);
   }

   lFreeList(&saved_gdil);
   DRETURN_VOID;
}
