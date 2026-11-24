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
 * @brief Rescheduling the jobs of a host that stopped answering
 */
#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RU.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "tom/ocs_List.h"

#include "sge.h"
#include <cinttypes>
#include "job_exit.h"
#include "execution_states.h"
#include "mail.h"
#include "symbols.h"
#include "reschedule.h"
#include "configuration_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_give_jobs.h"
#include "msg_qmaster.h"

/** @brief Extra delay added to every reschedule-unknown timeout
 *
 * Set by the testsuite so a takeover can be forced to happen sooner or later
 * than the configured timeout; 0 in normal operation.
 */
uint64_t add_time = 0;

static uint32_t
reschedule_unknown_timeout(ocs::tom::Element<ocs::tom::EH> hep);

static void
update_reschedule_unknown_timeout(ocs::tom::Element<ocs::tom::EH> host);

static ocs::tom::Element<ocs::tom::RU>
get_from_reschedule_unknown_list(ocs::tom::Element<ocs::tom::EH> host, uint32_t job_number,
                                 uint32_t task_number);

static bool
is_master_host_entry(ocs::tom::ConstElement<ocs::tom::RU> ruep);

static bool
has_master_host_entry(ocs::tom::ConstList<ocs::tom::RU> rulp, uint32_t job_number, uint32_t task_number);

static void
clear_deferred_start_of_task(uint32_t job_number, uint32_t task_number, uint64_t gdi_session);

/**
 * @brief Event handler to reschedule jobs
 *
 * This function initiates the automatic rescheduling for certain
 * jobs running on a specific host. These jobs will be put back into
 * the list of pending jobs.
 * The function is triggered by TYPE_RESCHEDULE_UNKNOWN_EVENT's.
 * TYPE_RESCHEDULE_UNKNOWN_EVENT's occure when the configured
 * "reschedule_unknown" timout value rundown. The clock tiggering
 * this event handler will be wind up with a call of
 * reschedule_unknown_trigger() in following situations:
 *    - a execution host went in unknown state due to missing
 *      load reports
 *       * execd was shut down (qconf -ke)
 *       * execd died (kill or coredump)
 *       * network or host problems (machine crashed, cable problem ...)
 *    - qmaster startup (all execution hosts are in unknown state)
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 *
 * @note MT-NOTE: reschedule_unknown_event() is NOT MT safe
 */
void reschedule_unknown_event(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list; /* AN */
   ocs::tom::Element<ocs::tom::EH> hep;
   uint64_t timeout = sge_gmt32_to_gmt64(te_get_first_numeric_key(anEvent));
   uint64_t new_timeout;
   char *hostname = te_get_alphanumeric_key(anEvent);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const auto master_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

   /*
    * is the automatic rescheduling disabled
    */
   if (mconf_get_disable_reschedule()) {
      DTRACE;
      goto Error;
   }

   /*
    * locate the host object which went in unknown-state
    */
   if (!(hep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(ocs::DataStore::master_list<SGE_TYPE_EXECHOST>(),
                                                                hostname)))) {
      DTRACE;
      goto Error;
   }

   /*
    * Did someone change the timeout value?
    */
   new_timeout = sge_gmt32_to_gmt64(reschedule_unknown_timeout(hep));
   if (new_timeout == 0) {
      INFO(MSG_RU_CANCELED_S, hostname);
      DTRACE;
      goto Error;
   } else if (new_timeout + add_time > timeout) {
      te_event_t ev = nullptr;
      uint64_t delta = new_timeout + add_time;
      uint64_t when = sge_get_gmt64() + delta - timeout;
      ev = te_new_event(when, TYPE_RESCHEDULE_UNKNOWN_EVENT, ONE_TIME_EVENT, delta, 0, hostname);
      te_add_event(ev);
      te_free_event(&ev);
      DTRACE;
      goto Error;
   }

   /*
    * Check if host is still in unknown state
    */
   for (const auto qep : master_list) {
      const auto qinstance = qep.get<CQ_qinstances>().find_first<QU_qhostname>(ocs::tom::Host(hostname));

      if (qinstance) {
         if (!qinstance_state_is_unknown(qinstance)) {
            DTRACE;
            goto Error;
         }
      }
   }

   /*
    * Find all jobs currently running on the host which went into
    * unknown state and append the jobids/taskids into
    * a sublist of the exechost object
    */
   reschedule_jobs(hep, 0, &answer_list, monitor, false, nullptr, nullptr, ocs::SessionManager::GDI_SESSION_NONE);
   answer_list.reset();

   Error:
   sge_free(&hostname);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
   DRETURN_VOID;
}

/**
 * @brief Reschedule jobs junning in host/queue
 *
 * The function is able to reschedule jobs running on a certain host
 * or in a specific queue. Please note that not all jobs will be
 * rescheduled. reschedule_job() containes more information which
 * jobs are applied.
 *
 * The signature stays raw: @p ep is of one of two types, and the caller outside this module passes raw
 * elements.
 *
 * @param ep host or queue (EH or QU)
 * @param force force the rescheduling of certain jobs (boolean)
 * @param answer answer list (AN)
 * @param is_manual indicator for manual (e.g. 'qmod -rj') or automatic (e.g. execd goes down) rescheduling.
 * @param monitor for monitoring qmaster threads
 * @param caller_user the requesting user
 * @param caller_host the requesting host
 * @param gdi_session the session the change belongs to
 *
 * @return 0 on success; 1 if one of the parameters was invalid
 */
int
reschedule_jobs(ocs::tom::ElementData *ep, uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer,
                monitoring_t *monitor, bool is_manual, const char *caller_user, const char *caller_host,
                uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   int ret = 1;

   /*
    * if ep is of type EH than we will reschedule all jobs
    * running on that host. if it is of type QU than we will
    * only reschedule the jobs for that queue
    */
   if (object_has_type(ep, ocs::tom::EH::descriptor.e) || object_has_type(ep, ocs::tom::QU::descriptor.e)) {
      /*
       * Find all jobs currently running on the host/queue
       * append the jobids/taskids into a sublist of the exechost object
       *
       * Cache the next pointer before processing: with ENABLE_RESCHEDULE_KILL
       * reschedule_job() -> sge_commit_job(COMMIT_ST_FINISHED_FAILED_EE) now
       * buries the job synchronously (CS-1239 consolidation), which removes
       * the current jep from this very list. A plain range-for would
       * dereference next through the freed element.
       */
      auto next_jep = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>().front();
      while (next_jep) {
         const auto jep = next_jep;
         next_jep = jep.next();
         reschedule_job(jep, nullptr, ep, force, answer, monitor, is_manual, caller_user, caller_host, gdi_session);
      }
      ret = 0;
   }

   DRETURN(ret);
}

/**
 * @brief Reschedule array tasks or jobs
 *
 * This function is able to reschedule:
 *    (a) a job with all its array tasks running anywhere
 *       (jatep == nullptr && ep == nullptr)
 *    (b) one array task running anywhere
 *       (jatep != nullptr && ep == nullptr)
 *    (c) all tasks of a job running on a certain host/queue
 *       (jatep == nullptr && ep != nullptr)
 *    (d) one array task running on a certain host/queue
 *       (jatep != nullptr && ep != nullptr)
 * Additionally to the conditions above jobs/tasks will only be
 * rescheduled if they fulfill following requirements:
 *    (1) not pending
 *    (2) restartable ("rerun" of the queue is "true" or "qsub -r y")
 *    (3) not interactive (qsh, qlogin, qrsh)
 *    (4) ckpt job and "when" of ckpt-obj containes "r" flag
 *    (5) was not deleted previously (qdel)
 * It is possible to force the rescheduling of jobs/tasks not fulfilling
 * condition (2), (4) and (5) if the force parameter is 1.
 *
 * The signature stays raw: @p ep is of one of two types, and the caller outside this module passes raw
 * elements.
 *
 * @param raw_jep job (JB)
 * @param raw_jatep array task (JAT or nullptr)
 * @param ep host or queue (EH or QU or nullptr)
 * @param force force rescheduling (boolean)
 * @param answer answer list (AN)
 * @param is_manual indicator for manual (e.g. 'qmod -rj') or automatic (e.g. execd goes down) rescheduling
 * @param monitor for monitoring qmaster threads
 * @param caller_user the requesting user
 * @param caller_host the requesting host
 * @param gdi_session the session the change belongs to
 *
 * @return 0 on success
 */
int reschedule_job(ocs::tom::ElementData *raw_jep, ocs::tom::ElementData *raw_jatep, ocs::tom::ElementData *ep,
                   const uint32_t force, ocs::tom::ListPtr<ocs::tom::AN> *answer, monitoring_t *monitor,
                   const bool is_manual, const char *caller_user, const char *caller_host, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   // the callers pass a job and array task of the master job list, checked here once
   const ocs::tom::Element<ocs::tom::JB> jep(raw_jep);
   const ocs::tom::Element<ocs::tom::JAT> jatep(raw_jatep);
   ocs::tom::Element<ocs::tom::QU> qep;
   ocs::tom::Element<ocs::tom::EH> hep;
   ocs::tom::Element<ocs::tom::JAT> next_jatep;
   char mail_ids[256];
   char mail_type[256];
   uint32_t job_number;
   uint32_t job_now;
   const char *hostname;
   int ret = 0;

   job_number = jep.get<JB_job_number>();

   /*
    * if jatep is nullptr then reschedule all tasks of this job
    */
   if (jatep) {
      next_jatep = jatep;
   } else {
      next_jatep = jep.get<JB_ja_tasks>().front();
   }

   while (next_jatep) {
      const auto this_jatep = next_jatep;
      ocs::tom::Element<ocs::tom::EH> host;
      uint32_t task_number;
      uint32_t found;

      if (jatep) {
         next_jatep = nullptr;
      } else {
         next_jatep = this_jatep.next();
      }

      ocs::tom::ConstList<ocs::tom::JG> granted_qs = this_jatep.get<JAT_granted_destin_identifier_list>();
      const auto first_granted_queue = granted_qs.front();

      /*
       * Jobs which have no granted queue can not be rescheduled (skip pending jobs)
       */
      if (!first_granted_queue) {
         continue;
      }

      task_number = this_jatep.get<JAT_task_number>();

      if (job_is_array(jep)) {
         snprintf(mail_ids, sizeof(mail_ids), sge_u32"." sge_u32, job_number, task_number);
         sge_strlcpy(mail_type, MSG_RU_TYPEJOBARRAY, sizeof(mail_type));
      } else {
         snprintf(mail_ids, sizeof(mail_ids), sge_u32, job_number);
         sge_strlcpy(mail_type, MSG_RU_TYPEJOB, sizeof(mail_type));
      }

      /*
       * if ep is of type EH than we will reschedule all tasks
       * running on that host. if it is of type QU than we will
       * only reschedule the tasks for that queue. if it is nullptr than we will
       * reschedule all tasks of that job
       */
      if (ep && object_has_type(ep, ocs::tom::EH::descriptor.e)) {
         hep = ocs::tom::Element<ocs::tom::EH>::from_typed(ep);   // object_has_type() checked it
         qep = nullptr;
         hostname = hep.get<EH_name>();
      } else if (ep && object_has_type(ep, ocs::tom::QU::descriptor.e)) {
         qep = ocs::tom::Element<ocs::tom::QU>::from_typed(ep);   // object_has_type() checked it
         hostname = qep.get<QU_qhostname>();
         hep = ocs::tom::Element<ocs::tom::EH>(
            host_list_locate(ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>(), hostname));
      } else {
         qep = nullptr;
         hep = nullptr;
         hostname = nullptr;
      }

      /*
       * We will skip current job if it is a
       *    - non-PE job and it is not running in queue/host
       *    - PE job where the master task is not running in queue/host (RESCHEDULE_SLAVE is false)
       *    - PE job where none of the tasks is running in queue/host (RESCHEDULE_SLAVE is true)
       */
      {
         const char *the_queuename = (qep ? qep.get<QU_full_name>() : nullptr);
         const char *the_hostname = (hep ? static_cast<const char *>(hep.get<EH_name>()) : nullptr);

         /*
          * If RESCHEDULE_SLAVE is set then
          *    we have to check each granted queue because PE jobs should be
          *    rescheduled also if only one PE slave task is running in that queue/host
          * otherwise
          *    PE jobs will only be rescheduled when the master task is effected
          */
         if (mconf_get_enable_reschedule_slave()) {
            bool one_matched = false;

            for (const auto granted_q : granted_qs) {
               if ((qep && strcmp(granted_q.get<JG_qname>(), the_queuename) == 0) ||
                   (hep && sge_hostcmp(granted_q.get<JG_qhostname>(), the_hostname) == 0)) {
                  one_matched = true;
                  break;
               }
            }
            if (!one_matched) {
               continue;
            }
         } else {
            if ((qep && strcmp(first_granted_queue.get<JG_qname>(), the_queuename)) ||
                (hep && sge_hostcmp(first_granted_queue.get<JG_qhostname>(), the_hostname))) {
               continue;
            }
         }
      }

      /*
       * Is this job tagged as restartable?
       * If the forced flag is set we will also reschedule this job!
       */
      if (!force && jep.get<JB_restart>() == 2) {
         if (mconf_get_enable_reschedule_kill()) {
            INFO(MSG_RU_REAPING_NOT_RESTARTABLE_SS, mail_type, mail_ids);
            sge_commit_job(jep, this_jatep, nullptr, COMMIT_ST_FINISHED_FAILED_EE,
                           COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
            continue;
         } else {
            INFO(MSG_RU_NOT_RESTARTABLE_SS, mail_type, mail_ids);
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         }
         continue;
      }

      /*
       * qsh, qlogin, qrsh, qrlogin-jobs won't be rescheduled automatically
       * (immediate jobs (qsub -now y ...) will be rescheduled)
       */
      job_now = jep.get<JB_type>();
      if (JOB_TYPE_IS_QSH(job_now) || JOB_TYPE_IS_QLOGIN(job_now)
          || JOB_TYPE_IS_QRSH(job_now) || JOB_TYPE_IS_QRLOGIN(job_now)) {
         INFO(MSG_RU_INTERACTIVEJOB_SSS, mail_ids, mail_type, mail_type);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         continue;
      }

      /*
       * ckpt-jobs will only be rescheduled when the "when" attribute
       * contains an appropriate flag or when the forced flag is set
       */
      if (!force && jep.get<JB_checkpoint_name>()) {
         const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
         const auto ckpt_ep = master_ckpt_list.find_first<CK_name>(jep.get<JB_checkpoint_name>());
         if (ckpt_ep) {
            uint32_t flags;

            flags = jep.get<JB_checkpoint_attr>();
            if (!flags) {
               flags = sge_parse_checkpoint_attr(ckpt_ep.get<CK_when>());
            }
            if (!(flags & CHECKPOINT_AT_AUTO_RES)) {
               INFO(MSG_RU_CKPTNOTVALID_SSS, mail_ids, ckpt_ep.get<CK_name>(), mail_type);
               answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
               continue;
            }
         } else {
            INFO(MSG_RU_CKPTEXIST_SS, mail_ids, ckpt_ep.get<CK_name>());
            answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
            continue;
         }
      }

      /*
       * Jobs which were registered for deletion will
       * not be automatically rescheduled (exception: forced flag)
       */
      if (!force && (this_jatep.get<JAT_state>() & JDELETED)) {
         INFO(MSG_RU_INDELETEDSTATE_SS, mail_type, mail_ids);
         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         continue;
      }

      /*
       * If the user did not use the -r flag during submit
       * we have to check the queue default (exception: forced flag)
       */
      if (!force && jep.get<JB_restart>() == 0) {
         ocs::tom::ConstElement<ocs::tom::QU> queue;

         if (qep && !strcmp(first_granted_queue.get<JG_qname>(), qep.get<QU_full_name>())) {
            queue = qep;
         } else {
            queue = cqueue_list_locate_qinstance(
                    ocs::DataStore::master_list<SGE_TYPE_CQUEUE>(),
                    first_granted_queue.get<JG_qname>());
         }
         if (!queue || !queue.get<QU_rerun>()) {
            if (mconf_get_enable_reschedule_kill()) {
               INFO(MSG_RU_REAPING_NOT_RESTARTABLE_SS, mail_type, mail_ids);
               sge_commit_job(jep, this_jatep, nullptr, COMMIT_ST_FINISHED_FAILED_EE,
                              COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
               continue;

            } else {
               INFO(MSG_RU_NORERUNQUEUE_SSS, mail_type, mail_ids, queue ? "none" : queue.get<QU_full_name>());
               answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
               continue;
            }
         }
      }

      /*
       * Is this task already contained in the list?
       * Append it if necessary
       */
      if (hep && !sge_hostcmp(first_granted_queue.get<JG_qhostname>(), hep.get<EH_name>())) {
         host = hep;
      } else {
         host = ocs::tom::Element<ocs::tom::EH>(host_list_locate(ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>(),
                                                                 first_granted_queue.get<JG_qhostname>()));
         hostname = first_granted_queue.get<JG_qhostname>();
      }
      if (get_from_reschedule_unknown_list(host, job_number, task_number)) {
         found = 1;
      } else {
         found = 0;
      }

      if (!found) {
         add_to_reschedule_unknown_list(host, job_number, task_number, 0, gdi_session);
         ret = 0;
      }

      /*
       * Mails and messages
       */
      if (!found) {
         uint32_t mail_options;
         char mail_action[256];

         mail_options = jep.get<JB_mail_options>();
         if (force) {
            sge_strlcpy(mail_action, MSG_RU_FORCEDR, sizeof(mail_action));
         } else {
            sge_strlcpy(mail_action, MSG_RU_PUSHEDR, sizeof(mail_action));
         }
         if (VALID(MAIL_AT_ABORT, mail_options)) {
            char mail_subject[1024];
            char mail_body[1024];

            ocs::tom::ConstList<ocs::tom::MR> mail_users = jep.get<JB_mail_list>();
            snprintf(mail_subject, sizeof(mail_subject), MSG_RU_MAILSUB_SS, mail_type, mail_ids);
            snprintf(mail_body, sizeof(mail_body), MSG_RU_MAILBODY_SSSS, mail_type, mail_ids, hostname, mail_action, mail_type);
            mail_to_users(QMASTER, mail_users, mail_subject, mail_body, MSG_RU_MAILTYPE);
         }

         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_RU_MSGFILEINFO, mail_action, mail_type, mail_ids, hostname);

         answer_list_add(answer, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      }

      /*
       * Trigger the rescheduling of this task
       */
      if (!found) {
          /* JR */
         uint32_t state = this_jatep.get<JAT_state>();

         this_jatep.set<JAT_job_restarted>(1);

         if (is_manual && host && host.get<EH_lt_heard_from>() != 0) {
            // The new run must not start before the old run is gone. The old run is killed with
            // the next report of the job from its host. A host which is unknown does not report,
            // waiting for it would block the job, e.g. after qmod -r of the jobs of a host which
            // went down.
            SETBIT(JDEFERRED_REQ, state);
         } else {
            /*
             * Paranoia ...
             */
            CLEARBIT(JDEFERRED_REQ, state);
         }
         this_jatep.set<JAT_state>(state);

         auto pseudo_jr = ocs::tom::ElementPtr<ocs::tom::JR>::create();
         {
            pseudo_jr->set<JR_job_number>(job_number);
            pseudo_jr->set<JR_ja_task_number>(task_number);
            pseudo_jr->set<JR_failed>(SSTATE_AGAIN);
            pseudo_jr->set<JR_err_str>((char *) MSG_RU_JR_ERRSTR);
            pseudo_jr->set<JR_queue_name>(first_granted_queue.get<JG_qname>());

            // Identify who terminated this run for the accounting record:
            //   - manual qmod -r: <caller_user>@<caller_host>
            //   - automatic (reschedule_unknown / host failure): qmaster@<master-host>
            // Setting it on the pseudo_jr (and not on JAT_joker) confines the marker to this
            // single acct write so it does not leak into the next run of the same ja_task.
            {
               DSTRING_STATIC(dstr_deleted_by, MAX_STRING_SIZE);
               if (caller_user != nullptr && caller_host != nullptr) {
                  sge_dstring_sprintf(&dstr_deleted_by, "%s@%s", caller_user, caller_host);
               } else {
                  sge_dstring_sprintf(&dstr_deleted_by, "qmaster@%s", component_get_qualified_hostname());
               }
               pseudo_jr->set<JR_deleted_by>(sge_dstring_get_string(&dstr_deleted_by));
            }

            sge_job_exit(*pseudo_jr, jep, this_jatep, monitor, gdi_session);
            pseudo_jr.reset();
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Add a job/task
 *
 * This function adds a job/task into the reschedule_unknown list of
 * a host. Jobs contained in this list won't be rescheduled back to
 * that hosts until it is sure that this job/task is not running
 * on that host anymore.
 * This mechanism makes it possible to reschedule jobs running on hosts
 * which are in an undefined state (no reports arrive the master).
 *
 * @param host host (EH) where the jok/task was running
 * @param job_number job id
 * @param task_number task id
 * @param state state
 * @param gdi_session the session the change belongs to
 *
 * @return the element added into the reschedule_unknown_list, an empty handle without a host
 */
ocs::tom::Element<ocs::tom::RU>
add_to_reschedule_unknown_list(const ocs::tom::Element<ocs::tom::EH> host, const uint32_t job_number,
                               const uint32_t task_number, const uint32_t state, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::RU> ruep = nullptr;
   if (host) {
      ruep = host.emplace_back_in<EH_reschedule_unknown_list, RU_job_number>(job_number);

      DPRINTF("RU: ADDED " sge_u32"." sge_u32" to EH_reschedule_unknown_list of host " SFN "\n",
              job_number, task_number, host.get<EH_name>().c_str());

      ruep.set<RU_task_number>(task_number);
      ruep.set<RU_state>(state);
      {
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;
         sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                         0, 0, host.get<EH_name>(), nullptr, nullptr, host, nullptr, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
      }
   }

   DRETURN(ruep);
}

/**
 * @brief Find an entry in a sublist
 *
 * This function tries to find an entry in the reschedule_unknown_list
 * of a host.
 *
 * @param host host (EH)
 * @param job_number job id
 * @param task_number task id
 *
 * @return the entry, or an empty handle
 */
static ocs::tom::Element<ocs::tom::RU>
get_from_reschedule_unknown_list(const ocs::tom::Element<ocs::tom::EH> host, const uint32_t job_number,
                                 const uint32_t task_number) {
   DENTER(TOP_LAYER);
   for (const auto ruep : host.get<EH_reschedule_unknown_list>()) {
      if (job_number == ruep.get<RU_job_number>()
          && task_number == ruep.get<RU_task_number>()) {
         DRETURN(ruep);
      }
   }
   DRETURN(nullptr);
}

/**
 * @brief Remove the reschedule unknown list entries of a host which are done
 *
 * Called after a report message of the host was processed. Removes the entries in state
 * - RESCHEDULE_HANDLE_JR_REMOVE: the final report of the slave task arrived
 * - RESCHEDULE_SKIP_JR_REMOVE: no report of the job arrived. This proves that the job does not
 *   run on the host any more only if the message contained the reports of all jobs on the host.
 *   A message with only the reports which the execd flushed after job events does not, removing
 *   the entry then would lose the kill of the old run of a rescheduled job (CS-3076).
 *
 * @param host host (EH)
 * @param all_jobs_reported true if the message contained a complete job report
 * @param gdi_session the session the change belongs to
 */
void
delete_from_reschedule_unknown_list(const ocs::tom::Element<ocs::tom::EH> host, const bool all_jobs_reported,
                                    const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool changed = false;
   ocs::tom::List<ocs::tom::RU> rulp = host.get<EH_reschedule_unknown_list>();
   if (rulp) {
      auto next = rulp.front();
      while (next) {
         const auto thiz = next;
         uint32_t state = thiz.get<RU_state>();

         next = thiz.next();

         if ((state == RESCHEDULE_SKIP_JR_REMOVE && all_jobs_reported)
             || state == RESCHEDULE_HANDLE_JR_REMOVE) {
            uint32_t job_number = thiz.get<RU_job_number>();
            uint32_t task_number = thiz.get<RU_task_number>();

            DPRINTF("RU: REMOVED " sge_u32"." sge_u32" FROM RU LIST\n", job_number, task_number);
            rulp.erase(thiz);
            changed = true;

            // the old run of the task is gone from this host, its new run may start
            if (state == RESCHEDULE_SKIP_JR_REMOVE
                && !has_master_host_entry(rulp, job_number, task_number)) {
               clear_deferred_start_of_task(job_number, task_number, gdi_session);
            }
         }
      }
      if (changed) {
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;

         sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                         0, 0, host.get<EH_name>(), nullptr, nullptr,
                         host, nullptr, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
      }
   }
   DRETURN_VOID;
}

/** @brief Allow the new run of a task rescheduled with qmod -r to start
 *
 * qmod -r sets JDEFERRED_REQ in JAT_state of the task: the scheduler does not dispatch the new run
 * before the old run is gone. The function clears the flag, spools the task and sends an event.
 *
 * @param jep the job (JB)
 * @param jatep the array task (JAT)
 * @param gdi_session the GDI session
 */
void
reschedule_clear_deferred_start(const ocs::tom::Element<ocs::tom::JB> jep, const ocs::tom::Element<ocs::tom::JAT> jatep,
                                const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   uint32_t state = jatep.get<JAT_state>();
   if (state & JDEFERRED_REQ) {
      uint32_t job_number = jep.get<JB_job_number>();
      uint32_t task_number = jatep.get<JAT_task_number>();

      DPRINTF("RU: deferred start of " sge_u32 "." sge_u32 " ends\n", job_number, task_number);
      CLEARBIT(JDEFERRED_REQ, state);
      jatep.set<JAT_state>(state);

      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr,
                      nullptr, jep, jatep, nullptr, true, true, gdi_session);
      answer_list_output(&answer_list);
   }

   DRETURN_VOID;
}

/** @brief Allow the new runs of the tasks whose old run was on a host to start
 *
 * Called when a host becomes unknown or is deleted. An unknown host does not report, the kill of
 * the old run of a task rescheduled with qmod -r is not sent before the host is back. Waiting for
 * it would block the task.
 *
 * The signature stays raw while the caller outside this module passes a raw element.
 *
 * @param raw_host the host (EH)
 * @param gdi_session the GDI session
 */
void
reschedule_clear_deferred_starts_of_host(const ocs::tom::ElementData *raw_host, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   // the caller passes a host of the master list, checked here once
   const ocs::tom::ConstElement<ocs::tom::EH> host(raw_host);
   for (const auto ruep : host.get<EH_reschedule_unknown_list>()) {
      if (is_master_host_entry(ruep)) {
         uint32_t job_number = ruep.get<RU_job_number>();
         uint32_t task_number = ruep.get<RU_task_number>();
         clear_deferred_start_of_task(job_number, task_number, gdi_session);
      }
   }

   DRETURN_VOID;
}

/** @brief Was a reschedule unknown list entry added for the old run of the master task?
 *
 * The entries for the host of the master task (or of a sequential job) start in one of the
 * RESCHEDULE_SKIP_JR* states, the entries for the slave hosts of a parallel job in one of the
 * RESCHEDULE_HANDLE_JR* states.
 *
 * @param ruep the entry (RU)
 * @return true if the entry is one for the host of the master task
 */
static bool
is_master_host_entry(const ocs::tom::ConstElement<ocs::tom::RU> ruep) {
   DENTER(TOP_LAYER);
   uint32_t state = ruep.get<RU_state>();
   bool ret = state == RESCHEDULE_SKIP_JR_REMOVE || state == RESCHEDULE_SKIP_JR_SEND_ACK
              || state == RESCHEDULE_SKIP_JR;
   DRETURN(ret);
}

/** @brief Has a reschedule unknown list an entry for the old run of the master task of a task?
 *
 * A sequential job gets two entries on its host, one from the reschedule and one from the commit of
 * the rescheduled task.
 *
 * @param rulp the reschedule unknown list of a host (RU)
 * @param job_number the job id
 * @param task_number the array task id
 * @return true if the list has such an entry
 */
static bool
has_master_host_entry(const ocs::tom::ConstList<ocs::tom::RU> rulp, const uint32_t job_number,
                      const uint32_t task_number) {
   DENTER(TOP_LAYER);
   for (const auto ruep : rulp) {
      if (ruep.get<RU_job_number>() == job_number
          && ruep.get<RU_task_number>() == task_number && is_master_host_entry(ruep)) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/** @brief Allow the new run of a task rescheduled with qmod -r to start
 *
 * @param job_number the job id
 * @param task_number the array task id
 * @param gdi_session the GDI session
 */
static void
clear_deferred_start_of_task(uint32_t job_number, uint32_t task_number, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const auto jep = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true)
                       .find_first<JB_job_number>(job_number);
   if (jep) {
      const auto jatep = jep.get<JB_ja_tasks>().find_first<JAT_task_number>(task_number);
      if (jatep) {
         reschedule_clear_deferred_start(jep, jatep, gdi_session);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Check entries in sublist
 *
 * This function checks and changes the state field of the elements
 * contained in the reschedule_unknown_list of a host. The state field
 * containes information about the current protocol state between the
 * master and a execution daemon for a job/task.
 *
 * @param host host (EH)
 * @param gdi_session the session the change belongs to
 */
void
update_reschedule_unknown_list(const ocs::tom::Element<ocs::tom::EH> host, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   if (host) {
      bool changed = false;
      for (const auto ruep : host.get<EH_reschedule_unknown_list>()) {
         uint32_t state = ruep.get<RU_state>();

         if (state == RESCHEDULE_SKIP_JR_SEND_ACK) {
            ruep.set<RU_state>(RESCHEDULE_SKIP_JR_REMOVE);
            changed = true;
         } else if (state == RESCHEDULE_SKIP_JR) {
            ruep.set<RU_state>(RESCHEDULE_SKIP_JR_SEND_ACK);
            changed = true;
         }
      }

      if (changed) {
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;
         sge_event_spool(nullptr, 0, sgeE_EXECHOST_MOD, 0, 0, host.get<EH_name>(), nullptr, nullptr,
                         host, nullptr, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief What should we do with a job report?
 *
 * This function is used within the master daemon at the place where
 * job reports arrive from the execd's. The function returns an integer
 * which indicates what to do with a job report and which steps are
 * necessary to interfere in the protocol between master and execd.
 * In following situation it is necessary to interfere in the protocol:
 * (1) job A was scheduled to host X
 * (2) All queues of host X went into unknown state because
 *     of network problems.
 * (3) automatic rescheduling mechanism decided to put the job A
 *     back into the list of pending jobs
 * (4) job A was scheduled to host Y
 * (5) host X came back and sends reports for job A
 * => now we have to ignore all reports from host X
 * => kill the old instance of job A
 * => make sure that no old stuff remains
 *
 * @param host host (EH)
 * @param job_report job report (JR)
 * @param job_number job id
 * @param task_number array task id
 *
 * @return what should we do? 0 => process the job report >0 => skip the job report 2 -> try to kill the job 3 -> send an ack to execd (job will be removed from filesystem)
 */
uint32_t
skip_restarted_job(const ocs::tom::Element<ocs::tom::EH> host, const ocs::tom::ConstElement<ocs::tom::JR> job_report,
                   const uint32_t job_number, const uint32_t task_number) {
   DENTER(TOP_LAYER);

   uint32_t ret = 0;
   for (const auto ruep : host.get<EH_reschedule_unknown_list>()) {
      if (ruep.get<RU_job_number>() == job_number
          && ruep.get<RU_task_number>() == task_number) {
         uint32_t state = ruep.get<RU_state>();

         if (state == RESCHEDULE_SKIP_JR_REMOVE) {
            ruep.set<RU_state>(RESCHEDULE_SKIP_JR);
            ret = 2;
         } else if (state == RESCHEDULE_SKIP_JR_SEND_ACK
                    || state == RESCHEDULE_SKIP_JR) {
            if (job_report.get<JR_state>() == JEXITING) {
               ret = 3;
            } else {
               ret = 1;
               DPRINTF("RU: GOT JR\n");
            }
         }
         break;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Check and change state
 *
 * This function is used to keep the state field up to date which is
 * contained in the reschedule_unknown_list entries. Only entries added for
 * parallel jobs will be changed.
 *
 * @param host host (EH)
 * @param job_number job id
 * @param task_number task id
 */
void
update_reschedule_unknown_list_for_job(const ocs::tom::Element<ocs::tom::EH> host, const uint32_t job_number,
                                       const uint32_t task_number) {
   DENTER(TOP_LAYER);

   if (host) {
      for (const auto ruep : host.get<EH_reschedule_unknown_list>()) {
         if (ruep.get<RU_job_number>() == job_number
             && ruep.get<RU_task_number>() == task_number) {
            if (ruep.get<RU_state>() == RESCHEDULE_HANDLE_JR_WAIT) {

               ruep.set<RU_state>(RESCHEDULE_HANDLE_JR_REMOVE);
               DPRINTF("RU: DECREMENTED PE-TAG OF " sge_u32 "." sge_u32"\n", job_number, task_number);
            }
            break;
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Change cached timeout value
 *
 * This functions changes all reschedule unknown values cached within
 * the exec host objects. 'config_name' may either be 'global' or
 * the name of a local configuration.
 *
 * @param config_name configuration name
 */
void
update_reschedule_unknown_timout_values(const char *config_name) {
   DENTER(TOP_LAYER);

   const auto master_exechost_list = ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>();

   if (strcmp(SGE_GLOBAL_NAME, config_name) == 0) {
      const ocs::tom::Element<ocs::tom::EH> global_exechost_elem(
         host_list_locate(master_exechost_list, SGE_GLOBAL_NAME));
      const ocs::tom::Element<ocs::tom::EH> template_exechost_elem(
         host_list_locate(master_exechost_list, SGE_TEMPLATE_NAME));

      for (const auto host : master_exechost_list) {
         if ((host != global_exechost_elem) && (host != template_exechost_elem)) {
            update_reschedule_unknown_timeout(host);
         }
      }
   } else if (strcmp(SGE_TEMPLATE_NAME, config_name) != 0) {
      update_reschedule_unknown_timeout(
         ocs::tom::Element<ocs::tom::EH>(host_list_locate(master_exechost_list, config_name)));
   }

   DRETURN_VOID;
}

/**
 * @brief Cache the timeout value in host
 *
 * The Function copies the timout value of the global/local configuration
 * for a certain host within the exec host object.
 *
 * @param host exec host (EH)
 */
static void
update_reschedule_unknown_timeout(const ocs::tom::Element<ocs::tom::EH> host) {
   DENTER(TOP_LAYER);

   if (host) {
      const char *hostname = host.get<EH_name>();
      uint32_t timeout = host.get<EH_reschedule_unknown>();

      auto conf_entry = ocs_get_configuration_entry_by_name(hostname, "reschedule_unknown");
      if (conf_entry) {
         const char *value = conf_entry->get<CF_value>();

         if (parse_ulong_val(nullptr, &timeout, ocs::CEntry::Type::TIME, value, nullptr, 0) == 0) {
            ERROR(MSG_OBJ_RESCHEDULEUNKN_SS, hostname, value);
            timeout = 0;
         }

         conf_entry.reset();
      } else {
         timeout = 0;
      }

      DPRINTF("%s: reschedule_unknown timeout for host " SFN " is " sge_u32 "\n", __func__, hostname, timeout);

      host.set<EH_reschedule_unknown>(timeout);
   } else {
      DPRINTF("%s: host == nullptr\n", __func__);
   }

   DRETURN_VOID;
}

/**
 * @brief Return the time to wait before resch
 *
 * This function returns the time to wait before rescheduling of
 * jobs running in hep will be initiated.
 *
 * @param hep host (EH)
 *
 * @return time in seconds
 */
static uint32_t
reschedule_unknown_timeout(const ocs::tom::Element<ocs::tom::EH> hep) {
   DENTER(TOP_LAYER);

   static int not_init = 1;
   uint32_t timeout = 0;
   const char *host = nullptr;

   host = hep.get<EH_name>();
   timeout = hep.get<EH_reschedule_unknown>();

   /* cache reschedule_unknown parameter in execd host to prevent host name resolving */
   if (mconf_is_new_config() || not_init) {
      if (auto conf_entry = ocs_get_configuration_entry_by_name(host, "reschedule_unknown"); conf_entry) {
         if (parse_ulong_val(nullptr, &timeout, ocs::CEntry::Type::TIME, conf_entry->get<CF_value>(), nullptr,
                             0) == 0) {
            ERROR(MSG_OBJ_RESCHEDULEUNKN_SS, host, conf_entry->get<CF_value>());
            timeout = 0;
         }
      }

      DPRINTF("%s: reschedule_unknown timeout for host %s is " sge_u32"\n", __func__, host, timeout);

      hep.set<EH_reschedule_unknown>(timeout);

      not_init = 0;
   }

   DRETURN(timeout);
}


/**
 * @brief Wind up timer for auto rescheduling
 *
 * This function winds up a timer used to trigger the automatic
 * rescheduling mechanism for a certain host. The initial timout value
 * will be the "reschedule_unknown" value in the global/local configuration
 * plus some time added with the reschedule_add_additional_time() function
 *
 * The signature stays raw while the caller outside this module passes a raw element.
 *
 * @param raw_hep host EH
 */
void
reschedule_unknown_trigger(ocs::tom::ElementData *raw_hep) {
   DENTER(TOP_LAYER);

   // the caller passes a host of the master list, checked here once
   const ocs::tom::Element<ocs::tom::EH> hep(raw_hep);

   uint64_t timeout;

   timeout = sge_gmt32_to_gmt64(reschedule_unknown_timeout(hep));

   if (timeout) {
      const char *host = hep.get<EH_name>();
      uint64_t when = sge_get_gmt64() + timeout + add_time;
      te_event_t ev = nullptr;

      DPRINTF("RU: Autorescheduling enabled for host " SFN ". (" sge_u64 " µsec)\n", host, timeout + add_time);

      ev = te_new_event(when, TYPE_RESCHEDULE_UNKNOWN_EVENT, ONE_TIME_EVENT, timeout, 0, host);
      te_add_event(ev);
      te_free_event(&ev);
   }
   DRETURN_VOID;
}

/**
 * @brief Set additional time to wait before r
 *
 * This function sets a time value which will be added to the
 * "reschedule_unknown" time. The master will wait this time after
 * a host went into unknown state before it initiates rescheduling of jobs
 *
 * @param time time in seconds
 */
void
reschedule_add_additional_time(uint64_t time) {
   DENTER(TOP_LAYER);
   add_time = time;
   DRETURN_VOID;
}

/** @brief Forget that one job on one host was waiting to be rescheduled
 * @param host the execution host
 * @param job_number the job
 * @param task_number the array task
 * @param gdi_session the session the change belongs to
 */
void
remove_from_reschedule_unknown_list(const ocs::tom::Element<ocs::tom::EH> host, const uint32_t job_number,
                                    const uint32_t task_number, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   if (host) {
      ocs::tom::List<ocs::tom::RU> unknown_list = host.get<EH_reschedule_unknown_list>();
      ocs::tom::Element<ocs::tom::RU> elem;

      for (const auto ruep : unknown_list) {
         if (ruep.get<RU_job_number>() == job_number &&
             ruep.get<RU_task_number>() == task_number) {
            elem = ruep;
            break;
         }
      }

      if (elem) {
         unknown_list.erase(elem);
         sge_event_spool(nullptr, 0, sgeE_EXECHOST_MOD,
                         0, 0, host.get<EH_name>(), nullptr, nullptr,
                         host, nullptr, nullptr, true, true, gdi_session);
      }
   }

   DRETURN_VOID;
}

/** @brief Forget a job on every host that was waiting to reschedule it
 * @param job_number the job
 * @param task_number the array task
 * @param gdi_session the session the change belongs to
 */
void
remove_from_reschedule_unknown_lists(uint32_t job_number, uint32_t task_number, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   for (const auto host : ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>()) {
      remove_from_reschedule_unknown_list(host, job_number, task_number, gdi_session);
   }

   DRETURN_VOID;
}

