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
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>

#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_Category.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/cull/sge_message_SME_L.h"
#include "sgeobj/cull/sge_message_MES_L.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_jsv_script.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/ocs_DataStore.h"

#include "sched/sge_job_schedd.h"
#include "sched/schedd_message.h"
#include "sched/sge_schedd_text.h"
#include "sched/sge_complex_schedd.h"

#include "cull/cull_list.h"

#include "spool/sge_spooling.h"

#include "ocs_CategoryQmaster.h"
#include "ocs_ReportingFileWriter.h"
#include "sge_task_depend.h"
#include "sge_persistence_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_job_verify.h"
#include "symbols.h"
#include "mail.h"
#include "sge_cqueue_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_qmod_qmaster.h"
#include "evm/sge_event_master.h"
#include "msg_common.h"
#include "msg_qmaster.h"

#include "ocs_gdi_ClientServerBase.h"


/****** qmaster/job/spooling ***************************************************
*
*  NAME
*     job spooling -- when are jobs/ja_tasks/pe_tasks spooled?
*
*  FUNCTION
*     Spooling of jobs is done when
*        - a new job is added
*        - a job is modified (qalter)
*        - a ja_task has been created
*        - the jobs ja_tasks are partly deleted (not all tasks)
*        - a job leaves qmaster (all tasks finished)
*
*     Spooling of ja_tasks is done when
*        - a ja_task is created (as result of schedd start order)
*        - a ja_task is sent to execd
*        - a ja_task has been received (ack) by an execd
*        - a ja_task is rescheduled
*        - ja_task delivery to execd failed (reschedule)
*        - the ja_task is marked as deleted
*        - jobs are notified about exec host shutdown
*        - for long running ja_tasks, the reported usage is spooled once a day
*        - a ja_task is (un)suspended on threshold
*        - a job is (un)suspended (qmod)
*        - a job error state is cleared
*
*     Spooling of pe_tasks is done when
*        - a new pe_task has been reported from execd
*        - for long running pe_tasks, the reported usage is spooled once a day
*        - for finished pe_tasks, usage is summed up in a container pe_task.
*          This container is spooled whenever usage is summed up.
*        - a pe_task is deleted
*
*******************************************************************************/

typedef struct {
   u_long32 job_number;
   bool changed;
   pthread_mutex_t job_number_mutex;
} job_number_t;

job_number_t job_number_control = {0, false, PTHREAD_MUTEX_INITIALIZER};

static int
mod_task_attributes(const ocs::gdi::Packet *packet, lListElem *job, lListElem *new_ja_task, lListElem *tep, lList **alpp,
                    int *trigger, int is_array, int is_task_enrolled);

static int
mod_job_attributes(const ocs::gdi::Packet *packet, lListElem *new_job, lListElem *jep, lList **alpp, int *trigger);

void
set_context(lList *jbctx, lListElem *job);

static u_long32
guess_highest_job_number();

static bool
contains_dependency_cycles(const lListElem *new_job, u_long32 job_number, lList **alpp);

static int
verify_job_list_filter(const ocs::gdi::Packet *packet, lList **alpp, int all_users_flag, int all_jobs_flag, int jid_flag, int user_list_flag);

static void
empty_job_list_filter(const ocs::gdi::Packet *packet, lList **alpp, int was_modify, int user_list_flag, lList *user_list, int jid_flag,
                      const char *jobid, int all_users_flag, int all_jobs_flag, int is_array, u_long32 start, u_long32 end, u_long32 step);

static void
get_rid_of_schedd_job_messages(u_long32 job_number);

static bool
is_changes_consumables(lList **alpp, const lList *new_lp, const lList *old_lp);

static void
job_list_filter(lList *user_list, const char *jobid, lCondition **job_filter);

static int
sge_delete_all_tasks_of_job(const ocs::gdi::Packet *packet, lList **alpp,
                            lListElem *job, u_long32 *r_start, u_long32 *r_end, u_long32 *step,
                            const lList *ja_structure, int *alltasks, u_long32 *deleted_tasks, u_long64 start_time,
                            monitoring_t *monitor, int forced, bool *deletion_time_reached);


/* when this character is modified, it has also be modified
   the JOB_NAME_DEL in clients/qalter/qalter.c
   */
static const char JOB_NAME_DEL = ':';

/*-------------------------------------------------------------------------*/
/* jepp is set to nullptr, if the job was successfully added                   */
/*                                                                         */
/* MT-Note: it is thread safe. It is using the global lock to secure the   */
/*          none safe functions                                            */
/*-------------------------------------------------------------------------*/
int
sge_gdi_add_job(lListElem **jep, lList **alpp, lList **lpp,
                ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                monitoring_t *monitor) {
   int ret;
   bool lret;
   u_long32 start;
   u_long32 end;
   u_long32 step;
   cl_thread_settings_t *tc = cl_thread_get_thread_config();
   lList **master_job_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   lList **master_suser_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_SUSER);

   DENTER(TOP_LAYER);

   // JSV verification if enabled
   if (jsv_is_enabled(tc->thread_name)) {

      // job verification so that data that is passed to JSV is correct
      ret = sge_job_verify_adjust(*jep, alpp, lpp, packet, task, monitor);
      if (ret != STATUS_OK) {
         DRETURN(ret);
      }

      // JSV verification with threshold handling
      struct timeval start_time{};
      struct timeval end_time{};
      gettimeofday(&start_time, nullptr);
      lret = jsv_do_verify(tc->thread_name, jep, alpp, true);
      gettimeofday(&end_time, nullptr);
      int jsv_threshold = mconf_get_jsv_threshold();
      if (((end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000) > jsv_threshold || jsv_threshold == 0) {
         INFO(MSG_JSV_THRESHOLD_UU, lGetUlong(*jep, JB_job_number), static_cast<u_long32>((end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000));
      }
      if (!lret) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   // final job verification
   ret = sge_job_verify_adjust(*jep, alpp, lpp, packet, task, monitor);
   if (ret != STATUS_OK) {
      DRETURN(ret);
   }

   // open a spooling transaction
   spool_transaction(alpp, spool_get_default_context(), STC_begin);

   // write script to file separately,  we do not want to hold it in memory
   if (lGetString(*jep, JB_script_file) && !JOB_TYPE_IS_BINARY(lGetUlong(*jep, JB_type))) {
      if (!spool_write_script(alpp, lGetUlong(*jep, JB_job_number), *jep)) {
         spool_transaction(alpp, spool_get_default_context(), STC_rollback);
         ERROR(MSG_JOB_NOWRITE_US, lGetUlong(*jep, JB_job_number), strerror(errno));
         answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EDISK);
      }
   }
   lSetString(*jep, JB_script_ptr, nullptr);
   lSetUlong(*jep, JB_script_size, 0);

   // create or assign a category
   lList** master_category_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
   const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
   lret = ocs::CategoryQmaster::attach_job(master_category_list, *jep,
                                                 master_userset_list, master_project_list, master_rqs_list,
                                                 true, packet->gdi_session);

   // Handle job spooling and event
   if (!sge_event_spool(alpp, 0, sgeE_JOB_ADD,
                        lGetUlong(*jep, JB_job_number), 0, nullptr, nullptr, nullptr,
                        *jep, nullptr, nullptr, true, true, packet->gdi_session)) {
      spool_transaction(alpp, spool_get_default_context(), STC_rollback);
      ERROR(MSG_JOB_NOWRITE_U, lGetUlong(*jep, JB_job_number));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      if ((lGetString(*jep, JB_exec_file) != nullptr)) {
         unlink(lGetString(*jep, JB_exec_file));
         lSetString(*jep, JB_exec_file, nullptr);
      }
      DRETURN(STATUS_EDISK);
   }

   // close the spooling transaction
   spool_transaction(alpp, spool_get_default_context(), STC_commit);

   if (!job_is_array(*jep)) {
      DPRINTF("Added Job " sge_u32"\n", lGetUlong(*jep, JB_job_number));
   } else {
      job_get_submit_task_ids(*jep, &start, &end, &step);
      DPRINTF("Added JobArray " sge_u32"." sge_u32"-" sge_u32":" sge_u32"\n", lGetUlong(*jep, JB_job_number), start, end, step);
   }

   /* add into job list */
   if (job_list_add_job(master_job_list, "master job list", lCopyElem(*jep), 0)) {
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /** increase user counter */
   suser_increase_job_counter(suser_list_add(master_suser_list, nullptr, packet->user));

   /* JG: TODO: error handling:
    * if job can't be spooled, no event is sent (in sge_event_spool)
    * if job can't be added to master list, it remains spooled
    * make checks earlier
    */

   /*
   ** immediate jobs trigger scheduling immediately
   */
   if (JOB_TYPE_IS_IMMEDIATE(lGetUlong(*jep, JB_type))) {
      sge_deliver_events_immediately(EV_ID_SCHEDD);
   }

   if (!job_is_array(*jep)) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_SUBMITJOB_US,
               lGetUlong(*jep, JB_job_number), lGetString(*jep, JB_job_name));
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_SUBMITJOBARRAY_UUUUS, lGetUlong(*jep, JB_job_number),
               start, end, step, lGetString(*jep, JB_job_name));
   }
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   /* do job logging */
   ocs::ReportingFileWriter::create_new_job_records(nullptr, *jep);
   ocs::ReportingFileWriter::create_job_logs(nullptr, lGetUlong64(*jep, JB_submission_time),
                                             JL_PENDING, packet->user, packet->host, nullptr,
                                             *jep, nullptr, nullptr, MSG_LOG_NEWJOB);

   /*
   **  add element to return list if necessary
   */
   if (lpp) {
      if (!*lpp) {
         *lpp = lCreateList("Job Return", JB_Type);
      }
      lAppendElem(*lpp, lCopyElem(*jep));
   }

   DRETURN(STATUS_OK);
}


/**
 * ocs::gdi::Client::sge_gdi_delete_job
 *    - called in sge_c_gdi_del (possibly multiple times, e.g. qdel 1,2 will trigger 2 calls)
 * @param[in] idep ID_Type element, can contain job ids, job names (with patterns), a user list (with patterns)
 * @param[out] alpp to return messages (INFO, WARNING, ERROR) to the caller
 * @param[in] ruser the user who executed qdel
 * @param[in] rhost the host on which qdel was executed
 * @param[in] sub_command sub command being part of the request (SGE_GDI_ALL_JOBS, SGE_GDI_ALL_USERS)
 * @param[in] monitor for monitoring qmaster threads
 */
int
sge_gdi_del_job(const ocs::gdi::Packet *packet, ocs::gdi::Task *task,  lListElem *idep, lList **alpp,
                ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_command, monitoring_t *monitor) {
   int all_jobs_flag;
   int all_users_flag;
   int jid_flag;
   int user_list_flag = false;
   const char *jid_str;
   lCondition *job_where = nullptr;
   lList *user_list = nullptr;
   int njobs = 0;
   u_long32 deleted_tasks = 0;
   u_long32 r_start = 0;
   u_long32 r_end = 0;
   u_long32 step = 0;
   int alltasks = 1;
   lListElem *nxt, *job = nullptr;
   bool forced = false;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   if (idep == nullptr) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (lGetPosViaElem(idep, ID_force, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(idep, ID_force) == 1) {
         forced = true;
      }
   } else {
      CRITICAL(MSG_NMNOTINELEMENT_S, "ID_force");
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* first lets make sure they have permission if a force is involved */
   if (!mconf_get_enable_forced_qdel()) {/* Flag ENABLE_FORCED_QDEL in qmaster_params */
      if (forced && !manop_is_manager(packet, master_manager_list)) {
         ERROR(MSG_JOB_FORCEDDELETEPERMS_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* sub-commands */
   all_jobs_flag = ((sub_command & ocs::gdi::SubCommand::SGE_GDI_ALL_JOBS) != 0);
   all_users_flag = ((sub_command & ocs::gdi::SubCommand::SGE_GDI_ALL_USERS) != 0);

   /* Did we get a user list or something else ? */
   if (lGetPosViaElem(idep, ID_user_list, SGE_NO_ABORT) >= 0) {
      user_list = lGetListRW(idep, ID_user_list);
   } else {
      CRITICAL(MSG_NMNOTINELEMENT_S, "ID_user_list");
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* Did we get a user list? */
   if (user_list && lGetNumberOfElem(user_list) > 0) {
      const lListElem *user = nullptr;
      for_each_ep(user, user_list) {
         if (strcmp(lGetString(user, ST_name), "*") == 0) {
            all_users_flag = true;
         }
      }
      if (!all_users_flag) {
         user_list_flag = true;
      }
   }

   jid_str = lGetString(idep, ID_str);

   /* Did we get a valid jobid? */
   if (!all_jobs_flag && (jid_str != nullptr) && (strcmp(jid_str, "0") != 0)) {
      jid_flag = 1;
   } else {
      jid_flag = 0;
   }

   /* no user is set, thought only work on the jobs for the current user, if
      a job name is specified. We do not care for users, if we work on jid*/
   if (!all_users_flag && !user_list_flag && (jid_str != nullptr) &&
       !isdigit(jid_str[0])) {
      lListElem *current_user = lCreateElem(ST_Type);

      if (user_list == nullptr) {
         user_list = lCreateList("user list", ST_Type);
         lSetList(idep, ID_user_list, user_list);
      }
      lSetString(current_user, ST_name, packet->user);
      lAppendElem(user_list, current_user);
      user_list_flag = true;
   }

   if (verify_job_list_filter(packet, alpp, all_users_flag, all_jobs_flag, jid_flag, user_list_flag)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   job_list_filter(user_list_flag ? user_list : nullptr,
                   jid_flag ? jid_str : nullptr, &job_where);

   u_long64 start_time = sge_get_gmt64();

   /* See CS-415 speed up job deletion by reverse order
    * going over the job list in reverse order significantly lowers the probability of running into
    * a problematic behavior of mass job deletion (e.g. qdel "*"):
    * - the first bunch of jobs gets deleted,
    *   then deletion pauses after a certain time period and qdel has to repeat the request
    * - in the meantime scheduler sees free slots and starts the next pending jobs
    * - exactly these jobs are deleted next
    * - this repeats over and over again and drastically slows down the deletion (and might cause other issues as well)
    */
//#define DEL_JOB_REVERSE_ORDER
#if defined(DEL_JOB_REVERSE_ORDER)
   nxt = lLastRW(master_job_list);
#else
   nxt = lFirstRW(master_job_list);
#endif
   while ((job = nxt)) {
      u_long32 job_number = 0;
      bool deletion_time_reached = false;

#if defined(DEL_JOB_REVERSE_ORDER)
      nxt = lPrevRW(job);
#else
      nxt = lNextRW(job);
#endif

      if ((job_where != nullptr) && !lCompare(job, job_where)) {
         continue;
      }
      job_number = lGetUlong(job, JB_job_number);

      /* Does user have privileges to delete the job/task? */
      if (job_check_owner(packet, job_number, master_job_list, master_manager_list, master_operator_list)) {
         ERROR(MSG_DELETEPERMS_SSU, packet->user, SGE_OBJ_JOB, job_number);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR);
         njobs++;
         /* continue with next job */
         continue;
      }

      njobs += sge_delete_all_tasks_of_job(packet, alpp, job, &r_start, &r_end, &step,
                                           lGetList(idep, ID_ja_structure),
                                           &alltasks, &deleted_tasks, start_time, monitor, forced,
                                           &deletion_time_reached);

      if (deletion_time_reached) {
         lFreeWhere(&job_where);
         DRETURN(STATUS_OK);
      }
   }

   lFreeWhere(&job_where);

   if (!njobs && !deleted_tasks) {
      empty_job_list_filter(packet, alpp, 0, user_list_flag, user_list, jid_flag,
                            jid_flag ? lGetString(idep, ID_str) : "0", all_users_flag, all_jobs_flag,
                            alltasks == 0 ? 1 : 0, r_start, r_end, step);
      DRETURN(STATUS_EEXIST);
   }

   if (forced) {
      /* remove all orphaned queue intances, which are empty. */
      cqueue_list_del_all_orphaned(master_cqueue_list, alpp, nullptr, nullptr, packet->gdi_session);
   }

   DRETURN(STATUS_OK);
}

/****** sge_job_qmaster/is_pe_master_task_send() *******************************
*  NAME
*     is_pe_master_task_send() -- figures out, if all slaves have been notified
*
*  SYNOPSIS
*     bool is_pe_master_task_send(lListElem *jatep) 
*
*  FUNCTION
*     In case of tightly integrated pe jobs the slave execds have to be notified first.
*     Once all execds acknowledged the slave notification, the master can be sent.
*     This function figures out, if all slaves have acknowledged notification.
*
*  INPUTS
*     lListElem *jatep - ja task in question
*
*  RESULT
*     bool - true, if all slaves have acknowledged slave notification
*
*  NOTES
*     MT-NOTE: is_pe_master_task_send() is MT safe 
*
*******************************************************************************/
bool
is_pe_master_task_send(lListElem *jatep) {
   bool all_slaves_arrived = true;
   const lListElem *gdil_ep = nullptr;

   for_each_ep(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
      if (lGetUlong(gdil_ep, JG_tag_slave_job) != 0) {
         all_slaves_arrived = false;
         break;
      }
   }

   return all_slaves_arrived;
}

/****** sge_job_qmaster/all_slave_jobs_finished() ******************************
*  NAME
*     all_slave_jobs_finished() -- have all slave jobs finished?
*
*  SYNOPSIS
*     bool all_slave_jobs_finished(lListElem *jatep)
*
*  FUNCTION
*     Figures out if all slave jobs of a tightly integrated parallel
*     job have finished.
*     The first gdil element of a host is tagged as long as the execd
*     on this host didn't report the slave job finish.
*
*  INPUTS
*     lListElem *jatep - the ja task of the running pe job
*
*  RESULT
*     bool - true if all slave jobs finished, else false
*
*  NOTES
*     MT-NOTE: all_slave_jobs_finished() is MT safe
*     TODO: can be merged with is_pe_master_task_send
*
*  SEE ALSO
*     sge_job_qmaster/is_pe_master_task_send()
*******************************************************************************/
bool
all_slave_jobs_finished(lListElem *jatep) {
   bool all_slaves_finished = true;
   const lList *gdil = nullptr;
   const lListElem *gdil_ep = nullptr;

   /* Search gdil for tagged entries.
    * Tagged means, the slave execd did not yet report job finish.
    * The first entry of gdil is the master task - ignore it.
    * Only the first gdil_ep of a host is tagged.
    */
   gdil = lGetList(jatep, JAT_granted_destin_identifier_list);
   for_each_ep(gdil_ep, gdil) {
      const char *host = lGetHost(gdil_ep, JG_qhostname);
      const lListElem *first_at_host = lGetElemHost(gdil, JG_qhostname, host);
      if (gdil_ep == first_at_host && gdil_ep != lFirst(gdil)) {
         if (lGetUlong(gdil_ep, JG_tag_slave_job) == 1) {
            all_slaves_finished = false;
            break;
         }
      }
   }

   return all_slaves_finished;
}

/****** sge_job_qmaster/tag_all_host_gdil() ************************************
*  NAME
*     tag_all_host_gdil() -- tag all hosts of a parallel job
*
*  SYNOPSIS
*     void tag_all_host_gdil(lListElem *jatep)
*
*  FUNCTION
*     Sets a tag for all hosts of a tightly integrated parallel job.
*
*  INPUTS
*     lListElem *jatep - the ja task of the parallel job
*
*  NOTES
*     MT-NOTE: tag_all_host_gdil() is MT safe 
*******************************************************************************/
void
tag_all_host_gdil(lListElem *jatep) {
   const lList *gdil = lGetList(jatep, JAT_granted_destin_identifier_list);
   lListElem *gdil_ep;

   for_each_rw (gdil_ep, gdil) {
      const char *host = lGetHost(gdil_ep, JG_qhostname);
      /* only the first gdil_ep for a host is tagged
       * if the pe job spawns multiple queues there might be multiple entries per host
       */
      const lListElem *first_at_host = lGetElemHost(gdil, JG_qhostname, host);
      if (gdil_ep == first_at_host) {
         lSetUlong(gdil_ep, JG_tag_slave_job, 1);
      }
   }
}

/****** sge_job_qmaster/ack_all_slaves() ***************************************
*  NAME
*     ack_all_slaves() -- send an ACK to all slave hosts
*
*  SYNOPSIS
*     void 
*     ack_all_slaves(sge_gdi_ctx_class_t *ctx,
*                    u_long32 job_id, u_long32 ja_task_id,
*                    const lListElem *ja_task, u_long32 type) 
*
*  FUNCTION
*     Sends an acknowledge message for a tighly integrated job to all
*     slave exec hosts.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - gdi context
*     u_long32 job_id          - job id
*     u_long32 ja_task_id      - job array task id
*     const lListElem *ja_task - the job array task
*     u_long32 type            - which ACK to send, e.g. ACK_SIGNAL_SLAVE
*
*  NOTES
*     MT-NOTE: ack_all_slaves() is MT safe 
*******************************************************************************/
void
ack_all_slaves(u_long32 job_id, u_long32 ja_task_id, const lListElem *ja_task,
               u_long32 type) {
   const lList *gdil = lGetList(ja_task, JAT_granted_destin_identifier_list);
   const lListElem *gdil_ep;

   for_each_ep(gdil_ep, gdil) {
      const char *host = lGetHost(gdil_ep, JG_qhostname);
      /* we only signal a slave host once
       * if the pe job spawns multiple queues there might be multiple entries per host
       */
      const lListElem *first_at_host = lGetElemHost(gdil, JG_qhostname, host);
      if (gdil_ep == first_at_host) {
         u_long32 dummymid = 0;
         sge_pack_buffer pb;

         init_packbuffer(&pb, 256); /* this is more than sufficient */
         pack_ack(&pb, type, job_id, ja_task_id, nullptr);
         /*
          * Send the ack to slave exec host. No special error handling done.
          * In a huge parallel job, it is not unusual for a slave host being
          * down, we don't want to see tons of error messages in the messages
          * file. The caller of this functions has to handle this situation.
          */
         ocs::gdi::ClientServerBase::gdi_send_message_pb(0, prognames[EXECD], 1, host, ocs::gdi::ClientServerBase::TAG_ACK_REQUEST, &pb, &dummymid);
         clear_packbuffer(&pb);
      }
   }
}

static void
empty_job_list_filter(const ocs::gdi::Packet *packet, lList **alpp, int was_modify, int user_list_flag, lList *user_list, int jid_flag,
                      const char *jobid, int all_users_flag, int all_jobs_flag, int is_array,
                      u_long32 start, u_long32 end, u_long32 step) {
   DENTER(TOP_LAYER);

   if (all_users_flag) {
      ERROR(SFNMAX, MSG_SGETEXT_THEREARENOJOBS);
   } else if (user_list_flag) {
      dstring user_list_string = DSTRING_INIT;

      sge_dstring_sprintf(&user_list_string, "");

      if (lGetNumberOfElem(user_list) > 0) {
         const lListElem *user;
         bool first = true;
         int umax = 20;

         for_each_ep(user, user_list) {
            if (!first) {
               sge_dstring_append(&user_list_string, ",");
            } else {
               first = false;
            }
            if (umax == 0) {
               sge_dstring_append(&user_list_string, "...");
               break;
            }
            sge_dstring_append(&user_list_string, lGetString(user, ST_name));
            umax--;
         }
      }
      if (jid_flag) {
         if (is_array) {
            if (start == end) {
               ERROR(MSG_SGETEXT_DOESNOTEXISTTASK_SUS, jobid, start, sge_dstring_get_string(&user_list_string));
            } else {
               ERROR(MSG_SGETEXT_DOESNOTEXISTTASKRANGE_SUUUS, jobid, start, end, step, sge_dstring_get_string(&user_list_string));
            }
         } else {
            ERROR(MSG_SGETEXT_DEL_JOB_SS, jobid, sge_dstring_get_string(&user_list_string));
         }
      } else {
         ERROR(MSG_SGETEXT_THEREARENOXFORUSERS_SS, SGE_OBJ_JOB, sge_dstring_get_string(&user_list_string));
      }

      sge_dstring_free(&user_list_string);

   } else if (all_jobs_flag) {
      ERROR(MSG_SGETEXT_THEREARENOXFORUSERS_SS, SGE_OBJ_JOB, packet->user);
   } else if (jid_flag) {
      /* should not be possible */
      if (is_array) {
         if (start == end) {
            ERROR(MSG_SGETEXT_DOESNOTEXISTTASK_SU, jobid, start);
         } else {
            ERROR(MSG_SGETEXT_DOESNOTEXISTTASKRANGE_SUUU, jobid, start, end, step);
         }
      } else {
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, SGE_OBJ_JOB, jobid);
      }
   } else {
      /* Should not be possible */
      ERROR(SFNMAX, was_modify ? MSG_SGETEXT_NOJOBSMODIFIED : MSG_SGETEXT_NOJOBSDELETED);
   }

   answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
   DRETURN_VOID;
}

/****** sge_job_qmaster/job_list_filter() **************************************
*  NAME
*     job_list_filter() -- Build filter for the joblist
*
*  SYNOPSIS
*     static void job_list_filter(lList *user_list, const char* jobid, char 
*     *ruser, bool all_users_flag, lCondition **job_filter, lCondition 
*     **user_filter) 
*
*  FUNCTION
*     Builds two where filters: one for users and one for jobs. 
*
*  INPUTS
*     lList *user_list         - user list or nullptr if no user exists
*     const char* jobid        - a job id or a job name or a pattern
*     lCondition **job_filter  - pointer to the target filter. If a where
*                                does exist, it will be extended by the new ones
*
*  RESULT
*     static void - 
*
*  NOTES
*     MT-NOTE: job_list_filter() is MT safe 
*
*******************************************************************************/
static void
job_list_filter(lList *user_list, const char *jobid, lCondition **job_filter) {
   lCondition *new_where = nullptr;

   DENTER(TOP_LAYER);

   if (job_filter == nullptr) {
      ERROR("job_list_filter() got no filters");
      DRETURN_VOID;
   }

   if (user_list != nullptr) {
      const lListElem *user;

      DPRINTF("Add all users given in userlist to filter\n");
      for_each_ep(user, user_list) {
         const char *user_name = lGetString(user, ST_name);
         if (sge_is_pattern(user_name)) {
            new_where = lWhere("%T(%I p= %s)", JB_Type, JB_owner, user_name);
         } else {
            new_where = lWhere("%T(%I == %s)", JB_Type, JB_owner, user_name);
         }
         if (!*job_filter) {
            *job_filter = new_where;
         } else {
            *job_filter = lOrWhere(*job_filter, new_where);
         }
      }
   }

   if (jobid != nullptr) {
      DPRINTF("Add jid %s to filter\n", jobid);
      if (isdigit(jobid[0])) {
         new_where = lWhere("%T(%I==%u)", JB_Type, JB_job_number, atol(jobid));
      } else {
         new_where = lWhere("%T(%I p= %s)", JB_Type, JB_job_name, jobid);
      }
      if (!*job_filter) {
         *job_filter = new_where;
      } else {
         *job_filter = lAndWhere(*job_filter, new_where);
      }
   }

   DRETURN_VOID;
}

/*
   qalter -uall               => all_users_flag = true
   qalter ... <jid> ...       => jid_flag = true
   qalter -u <username> ...   => user_list_flag = true
   qalter ... all             => all_jobs_flag = true

   1) all_users_flag && all_jobs_flag     => all jobs of all users (requires
                                             manager pevileges)
   2) all_users_flag && jid_flag          => not valid
   3) all_users_flag                      => all jobs of all users (requires
                                             manager pevileges)
   4) user_list_flag && all_jobs_flag     => all jobs of all users given in 
                                             <user_list>
   5) user_list_flag && jid_flag          => not valid
   6) user_list_flag                      => all jobs of all users given in 
                                             <user_list>
   7) all_jobs_flag                       => all jobs of current user
   8) jid_flag                            => <jid>
   9) all_users_flag && user_list_flag    => not valid
*/

static int
verify_job_list_filter(const ocs::gdi::Packet *packet, lList **alpp, int all_users_flag, int all_jobs_flag, int jid_flag, int user_list_flag) {
   DENTER(TOP_LAYER);
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   /* Reject incorrect requests */
   if (!all_users_flag && !all_jobs_flag && !jid_flag && !user_list_flag) {
      ERROR(MSG_SGETEXT_SPECIFYUSERORID_S, SGE_OBJ_JOB);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* case 9 */

   if (all_users_flag && user_list_flag) {
      ERROR(SFNMAX, MSG_SGETEXT_SPECIFYONEORALLUSER);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

#if 0
   /* case 2,5 */
   if ((all_users_flag || user_list_flag) && jid_flag) {
      ERROR(SFNMAX, MSG_SGETEXT_NOTALLOWEDTOSPECUSERANDJID);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
#endif

   /* case 1,3: Only manager can modify all jobs of all users */
   if (all_users_flag && !jid_flag && !manop_is_manager(packet, master_manager_list)) {
      ERROR(MSG_SGETEXT_MUST_BE_MGR_TO_SS, packet->user, "modify all jobs");
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   DRETURN(0);
}

static void get_rid_of_schedd_job_messages(u_long32 job_number) {
   const lListElem *sme = nullptr;
   lListElem *mes = nullptr;
   lListElem *next = nullptr;
   lList *mes_list = nullptr;
   lList *master_job_schedd_info_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB_SCHEDD_INFO);


   DENTER(TOP_LAYER);
   if (master_job_schedd_info_list != nullptr) {
      sme = lFirst(master_job_schedd_info_list);
      mes_list = lGetListRW(sme, SME_message_list);

      /*
      ** remove all messages for job_number
      */
      next = lFirstRW(mes_list);
      while ((mes = next)) {
         lListElem *job_ulng;
         next = lNextRW(mes);

         if ((job_ulng = lGetElemUlongRW(lGetList(mes, MES_job_number_list), ULNG_value, job_number)) != nullptr) {
            /* 
            ** more than one job in list for this message => remove job id
            ** else => remove whole message 
            */
            if (lGetNumberOfElem(lGetList(mes, MES_job_number_list)) > 1) {
               lRemoveElem(lGetListRW(mes, MES_job_number_list), &job_ulng);
               DPRINTF("Removed jobid " sge_u32" from list of scheduler messages\n", job_number);
            } else {
               lRemoveElem(mes_list, &mes);
               DPRINTF("Removed message from list of scheduler messages " sge_u32 "\n", job_number);
            }
         }
      }
   }
   DRETURN_VOID;
}

void job_ja_task_send_abort_mail(const lListElem *job,
                                 const lListElem *ja_task,
                                 const char *ruser,
                                 const char *rhost,
                                 const char *err_str) {
   dstring subject = DSTRING_INIT;
   dstring body = DSTRING_INIT;
   const lList *users = nullptr;
   u_long32 job_id;
   u_long32 ja_task_id;
   const char *job_name = nullptr;
   int send_abort_mail = 0;

   ja_task_id = lGetUlong(ja_task, JAT_task_number);
   job_name = lGetString(job, JB_job_name);
   job_id = lGetUlong(job, JB_job_number);
   users = lGetList(job, JB_mail_list);
   send_abort_mail = VALID(MAIL_AT_ABORT, lGetUlong(job, JB_mail_options))
                     && !(lGetUlong(ja_task, JAT_state) & JDELETED);

   if (send_abort_mail) {
      if (job_is_array(job)) {
         sge_dstring_sprintf(&subject, MSG_MAIL_TASKKILLEDSUBJ_UUS,
                             job_id, ja_task_id, job_name);
         sge_dstring_sprintf(&body, MSG_MAIL_TASKKILLEDBODY_UUSSS,
                             job_id, ja_task_id, job_name, ruser, rhost);
      } else {
         sge_dstring_sprintf(&subject, MSG_MAIL_JOBKILLEDSUBJ_US,
                             job_id, job_name);
         sge_dstring_sprintf(&body, MSG_MAIL_JOBKILLEDBODY_USSS,
                             job_id, job_name, ruser, rhost);
      }
      if (err_str != nullptr) {
         sge_dstring_append(&body, "\n");
         sge_dstring_append(&body, MSG_MAIL_BECAUSE);
         sge_dstring_append(&body, err_str);
      }
      cull_mail(QMASTER, users, sge_dstring_get_string(&subject),
                sge_dstring_get_string(&body), "job abortion");
   }

   sge_dstring_free(&subject);
   sge_dstring_free(&body);
}

void get_rid_of_job_due_to_qdel(lListElem *j,
                                lListElem *t,
                                lList **answer_list,
                                const char *ruser,
                                int force,
                                monitoring_t *monitor, u_long64 gdi_session) {
   u_long32 job_number, task_number;
   lListElem *qep = nullptr;
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   job_number = lGetUlong(j, JB_job_number);
   task_number = lGetUlong(t, JAT_task_number);
   qep = cqueue_list_locate_qinstance(master_cqueue_list, lGetString(t, JAT_master_queue));
   if (!qep) {
      ERROR(MSG_JOB_UNABLE2FINDQOFJOB_S, lGetString(t, JAT_master_queue));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
   }
   if (sge_signal_queue(SGE_SIGKILL, qep, j, t, monitor)) {
      if (force) {
         /* 3: JOB_FINISH reports aborted */
         sge_commit_job(j, t, nullptr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
         cancel_job_resend(job_number, task_number);
         j = nullptr;

         if (job_is_array(j)) {
            ERROR(MSG_JOB_FORCEDDELTASK_SUU, ruser, job_number, task_number);
         } else {
            ERROR(MSG_JOB_FORCEDDELJOB_SU, ruser, job_number);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      } else {
         ERROR(MSG_COM_NOSYNCEXECD_SU, ruser, job_number);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      }
   } else {
      if (force) {
         u_long64 now = sge_get_gmt64();
         const char *qualified_hostname = component_get_qualified_hostname();
         lListElem *dummy_jr = lCreateElem(JR_Type);

         if (job_is_array(j)) {
            ERROR(MSG_JOB_FORCEDDELTASK_SUU, ruser, job_number, task_number);
         } else {
            ERROR(MSG_JOB_FORCEDDELJOB_SU, ruser, job_number);
         }

         job_report_init_from_job_with_usage(dummy_jr, j, t, nullptr, now);
         ocs::ReportingFileWriter::create_acct_records(nullptr, dummy_jr, j, t, false);
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_DELETED, MSG_SCHEDD, qualified_hostname, nullptr, j, t, nullptr,
                                  MSG_LOG_DELFORCED);
         sge_commit_job(j, t, nullptr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
         cancel_job_resend(job_number, task_number);
         lFreeElem(&dummy_jr);
         j = nullptr;
      } else {
         /*
          * the job gets registered for deletion:
          * 0. send signal to execd
          * 1. JB_pending_signal = SGE_SIGKILL
          * 2. ACK from execd resets JB_pending_signal to 0
          *    Here we need a state for the job displaying its
          *    pending deletion
          * 3. execd signals shepherd and reaps job after job exit
          * 4. execd informs master of job exits and job is
          *    deleted from master lists
          */

         if (job_is_array(j)) {
            INFO(MSG_JOB_REGDELTASK_SUU, ruser, job_number, task_number);
         } else {
            INFO(MSG_JOB_REGDELX_SSU, ruser, SGE_OBJ_JOB, job_number);
         }
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }
   job_mark_job_as_deleted(j, t);
   DRETURN_VOID;
}

void job_mark_job_as_deleted(lListElem *j, lListElem *t) {
   DENTER(TOP_LAYER);
   if (j != nullptr && t != nullptr) {

      u_long32 state = lGetUlong(t, JAT_state);
      SETBIT(JDELETED, state);
      lSetUlong(t, JAT_state, state);
      lSetUlong64(t, JAT_stop_initiate_time, sge_get_gmt64());

      lList *answer_list = nullptr;
      sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, lGetUlong(j, JB_job_number),
                      lGetUlong(t, JAT_task_number), nullptr, nullptr, nullptr,
                      j, t, nullptr, true, true, 0);
      answer_list_output(&answer_list);
   }

   DRETURN_VOID;
}

/*-------------------------------------------------------------------------*/
/* ocs::gdi::Client::sge_gdi_modify_job                                                    */
/*    called in sge_c_gdi_mod                                              */
/*-------------------------------------------------------------------------*/

/* 
   this is our strategy:

   do common checks and search old job
   make a copy of the old job (this will be the new job)
   modify new job using reduced job as instruction
      on error: dispose new job
   store new job to disc
      on error: dispose new job
   create events
   replace old job by new job
*/

/* actions to be done after successful 
saving to disk of a modified job */
enum {
   MOD_EVENT = 1,
   PRIO_EVENT = 2,
   RECHAIN_JID_HOLD = 4,
   RECHAIN_JA_AD_HOLD = 8,
   VERIFY_EVENT = 16
};

int
sge_gdi_mod_job(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *jep, lList **alpp, int sub_command) {
   lListElem *nxt, *jobep = nullptr;   /* pointer to old job */
   int job_id_pos;
   int user_list_pos;
   int job_name_pos;
   lCondition *job_where = nullptr;
   int user_list_flag;
   int njobs = 0, ret, jid_flag;
   int all_jobs_flag;
   int all_users_flag;
   bool job_name_flag = false;
   char *job_mod_name = nullptr;
   const char *job_name = nullptr;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   lList **master_job_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   if (jep == nullptr) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* sub-commands */
   all_jobs_flag = ((sub_command & ocs::gdi::SubCommand::SGE_GDI_ALL_JOBS) > 0);
   all_users_flag = ((sub_command & ocs::gdi::SubCommand::SGE_GDI_ALL_USERS) > 0);

   /* Did we get a user list? */
   if (((user_list_pos = lGetPosViaElem(jep, JB_user_list, SGE_NO_ABORT)) >= 0)
       && lGetNumberOfElem(lGetPosList(jep, user_list_pos)) > 0) {
      user_list_flag = 1;
   } else {
      user_list_flag = 0;
   }

   job_name_pos = lGetPosViaElem(jep, JB_job_name, SGE_NO_ABORT);
   if (job_name_pos >= 0) {
      job_name = lGetPosString(jep, job_name_pos);
   }

   /* Did we get a job - with a jobid? */
   if (
           (((job_id_pos = lGetPosViaElem(jep, JB_job_number, SGE_NO_ABORT)) >= 0) &&
            lGetPosUlong(jep, job_id_pos) > 0) ||
           ((job_name != nullptr) &&
            (job_name_flag = (job_name[0] == JOB_NAME_DEL) ? true : false))
           ) {
      jid_flag = 1;
   } else {
      jid_flag = 0;
   }

   if ((ret = verify_job_list_filter(packet, alpp, all_users_flag, all_jobs_flag,
                                     jid_flag, user_list_flag))) {
      DRETURN(ret);
   }

   if (jsv_is_modify_rejected(alpp, jep)) {
      DRETURN(ret);
   }

   {
      const char *job_id_str = nullptr;
      char job_id[40];
      if (!job_name_flag) {
         snprintf(job_id, sizeof(job_id), sge_u32, lGetPosUlong(jep, job_id_pos));
         job_id_str = job_id;
      } else {
         /* format: <delimiter>old_name<delimiter>new_name */
         char *del_pos = nullptr;
         job_id_str = lGetPosString(jep, job_name_pos);
         job_id_str++;
         del_pos = strchr((char *) job_id_str, JOB_NAME_DEL);
         *del_pos = '\0';
         del_pos++;

         job_mod_name = sge_strdup(nullptr, job_id_str);
         job_id_str = job_mod_name;

         if (strlen(del_pos) > 0) {
            lSetPosString(jep, job_name_pos, del_pos);
         } else {
            lSetPosString(jep, job_name_pos, nullptr);
         }
      }

      job_list_filter(user_list_flag ? lGetPosList(jep, user_list_pos) : nullptr,
                      jid_flag ? job_id_str : nullptr, &job_where);
   }

   nxt = lFirstRW(*master_job_list);
   while ((jobep = nxt)) {
      u_long32 jobid = 0;
      lListElem *new_job;        /* new job */
      lList *tmp_alp = nullptr;
      lListElem *jatep;
      int trigger = 0;
      nxt = lNextRW(jobep);

      if ((job_where != nullptr) && !lCompare(jobep, job_where)) {
         continue;
      }

      /* ignore modify requests if all job tasks are already JFINISHED 
         and no task id remains in not yet ran task id lists */
      if (job_get_not_enrolled_ja_tasks(jobep) == 0) {
         const lListElem *ja_task;
         bool all_finished = true;
         for_each_ep(ja_task, lGetList(jobep, JB_ja_tasks)) {
            if (lGetUlong(ja_task, JAT_status) != JFINISHED) {
               all_finished = false;
               break;
            }
         }
         if (all_finished) {
            continue;
         }
      }

      njobs++;
      jobid = lGetUlong(jobep, JB_job_number);

      /* general check whether ruser is allowed to modify this job */
      if (strcmp(packet->user, lGetString(jobep, JB_owner))
          && !manop_is_operator(packet, master_manager_list, master_operator_list)
          && !manop_is_manager(packet, master_manager_list)) {
         ERROR(MSG_SGETEXT_MUST_BE_JOB_OWN_TO_SUS, packet->user, jobid, MSG_JOB_CHANGEATTR);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR);
         lFreeWhere(&job_where);
         sge_free(&job_mod_name);
         DRETURN(STATUS_ENOTOWNER);
      }

      /* operate on a cull copy of the job */
      new_job = lCopyElem(jobep);
      if (mod_job_attributes(packet, new_job, jep, &tmp_alp, &trigger)) {
         if (*alpp == nullptr) {
            *alpp = lCreateList("answer", AN_Type);
         }
         lAddList(*alpp, &tmp_alp);
         lFreeElem(&new_job);
         lFreeWhere(&job_where);
         sge_free(&job_mod_name);
         DRETURN(STATUS_EUNKNOWN);
      }

      if (!(trigger & VERIFY_EVENT)) {
         bool dbret;
         lList *answer_list = nullptr;

         // @todo CS-1156: Why not also for PRIO_EVENT, RECHAIN_JID_HOLD and RECHAIN_JA_AD_HOLD
         if (trigger & MOD_EVENT) {
            lSetUlong(new_job, JB_version, lGetUlong(new_job, JB_version) + 1);
         }

         // @todo CS-1155: add a transaction?
         // open a spooling transaction
         spool_transaction(alpp, spool_get_default_context(), STC_begin);


         // if the job changed then check if also the category changed and trigger required events
         if ((trigger & (PRIO_EVENT | MOD_EVENT)) > 0) {
            // build the category string
            const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
            const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
            const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
            dstring category_str = DSTRING_INIT;
            ocs::Category::build_string(&category_str, new_job, master_userset_list, master_project_list, master_rqs_list);
            const char *cat_str = sge_dstring_get_string(&category_str);

            // check if the category string changed
            lList **master_category_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);
            u_long32 old_category_id = lGetUlong(new_job, JB_category_id);
            const lListElem *old_category = lGetElemUlong(*master_category_list, CT_id, old_category_id);
            const char *old_cat_str = lGetString(old_category, CT_str);

            // check if the category string changed and trigger corresponding events
            if (strcmp(old_cat_str, cat_str) != 0) {
               // remove the job from the category
               ocs::CategoryQmaster::detach_job(master_category_list, new_job, true, packet->gdi_session);

               // add the job to the new category
               ocs::CategoryQmaster::attach_job(master_category_list, new_job,
                                                master_userset_list, master_project_list, master_rqs_list,
                                                true, packet->gdi_session);
            }
         }

         /* all job modifications to be saved on disk must be made in new_job */
         dstring buffer = DSTRING_INIT;
         dbret = spool_write_object(&answer_list, spool_get_default_context(), new_job,
                                    job_get_key(jobid, 0, nullptr, &buffer), SGE_TYPE_JOB, true);
         answer_list_output(&answer_list);
         if (!dbret) {
            ERROR(MSG_JOB_NOALTERNOWRITE_U, jobid);

            // @todo CS-1155: add a transaction?
            spool_transaction(alpp, spool_get_default_context(), STC_rollback);

            answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
            sge_dstring_free(&buffer);
            lFreeList(&tmp_alp);
            lFreeElem(&new_job);
            lFreeWhere(&job_where);
            sge_free(&job_mod_name);
            DRETURN(STATUS_EDISK);
         }
         sge_dstring_free(&buffer);

         /* all elems in tmp_alp need to be appended to alpp */
         if (!*alpp) {
            *alpp = lCreateList("answer", AN_Type);
         }
         lAddList(*alpp, &tmp_alp);

         if (trigger & MOD_EVENT) {
            sge_add_job_event(sgeE_JOB_MOD, new_job, nullptr, packet->gdi_session);
            for_each_rw(jatep, lGetList(new_job, JB_ja_tasks)) {
               sge_add_jatask_event(sgeE_JATASK_MOD, new_job, jatep, packet->gdi_session);
            }
         }
         if (trigger & PRIO_EVENT) {
            sge_add_job_event(sgeE_JOB_MOD, new_job, nullptr, packet->gdi_session);
         }

         /* remove all existing trigger links - 
            this has to be done using the old 
            jid_predecessor_list */

         if (trigger & RECHAIN_JID_HOLD) {
            lListElem *suc_jobep, *jid;
            for_each_rw(jid, lGetList(jobep, JB_jid_predecessor_list)) {
               u_long32 pre_ident = lGetUlong(jid, JRE_job_number);

               DPRINTF(" JOB #" sge_u32": P: " sge_u32"\n", jobid, pre_ident);

               if ((suc_jobep = lGetElemUlongRW(*master_job_list, JB_job_number, pre_ident))) {
                  lListElem *temp_job = lGetElemUlongRW(lGetList(suc_jobep, JB_jid_successor_list), JRE_job_number, jobid);
                  DPRINTF("  JOB " sge_u32 " removed from trigger list of job " sge_u32 "\n", jobid, pre_ident);
                  lRemoveElem(lGetListRW(suc_jobep, JB_jid_successor_list), &temp_job);
               }
            }
         }

         if (trigger & RECHAIN_JA_AD_HOLD) {
            lListElem *suc_jobep, *jid;
            for_each_rw(jid, lGetList(jobep, JB_ja_ad_predecessor_list)) {
               u_long32 pre_ident = lGetUlong(jid, JRE_job_number);

               DPRINTF(" JOB #" sge_u32 ": P: " sge_u32"\n", jobid, pre_ident);

               if ((suc_jobep = lGetElemUlongRW(*master_job_list, JB_job_number, pre_ident))) {
                  lListElem *temp_job = lGetElemUlongRW(lGetList(suc_jobep, JB_ja_ad_successor_list), JRE_job_number,
                                                        jobid);
                  DPRINTF("  JOB " sge_u32 " removed from trigger list of job " sge_u32 "\n", jobid, pre_ident);
                  lRemoveElem(lGetListRW(suc_jobep, JB_ja_ad_successor_list), &temp_job);
               }
            }
         }

         /* write data back into job list  */
         {
            lListElem *prev = lPrevRW(jobep);

            lRemoveElem(*master_job_list, &jobep);
            lInsertElem(*master_job_list, prev, new_job);
         }
         /* no need to spool these mods */
         if (trigger & RECHAIN_JID_HOLD) {
            job_suc_pre(new_job);
         }
         if (trigger & RECHAIN_JA_AD_HOLD) {
            job_suc_pre_ad(new_job);
         }

         // @todo CS-1155: add a transaction?
         // close the spooling transaction
         spool_transaction(alpp, spool_get_default_context(), STC_commit);

         INFO(MSG_SGETEXT_MODIFIEDINLIST_SSUS, packet->user, packet->host, jobid, MSG_JOB_JOB);
      }
   }
   lFreeWhere(&job_where);

   if (!njobs) {
      const char *job_id_str = nullptr;
      char job_id[40];
      if (!job_name_flag) {
         snprintf(job_id, sizeof(job_id), sge_u32, lGetPosUlong(jep, job_id_pos));
         job_id_str = job_id;
      } else {
         job_id_str = job_mod_name;
      }

      empty_job_list_filter(packet, alpp, 1, user_list_flag,
                            user_list_flag ? lGetPosList(jep, user_list_pos) : nullptr,
                            jid_flag, jid_flag ? job_id_str : "0",
                            all_users_flag, all_jobs_flag, 0, 0, 0, 0);
      sge_free(&job_mod_name);
      DRETURN(STATUS_EEXIST);
   }

   sge_free(&job_mod_name);
   DRETURN(STATUS_OK);
}

void sge_add_job_event(ev_event type, lListElem *jep, lListElem *jatask, u_long64 gdi_request) {
   DENTER(TOP_LAYER);
   sge_add_event(0, type, lGetUlong(jep, JB_job_number),
                 jatask ? lGetUlong(jatask, JAT_task_number) : 0,
                 nullptr, nullptr, lGetString(jep, JB_session), jep, gdi_request);
   DRETURN_VOID;
}

void sge_add_jatask_event(ev_event type, lListElem *jep, lListElem *jatask, u_long64 gdi_request) {
   DENTER(TOP_LAYER);
   sge_add_event(0, type, lGetUlong(jep, JB_job_number),
                 lGetUlong(jatask, JAT_task_number),
                 nullptr, nullptr, lGetString(jep, JB_session), jatask, gdi_request);
   DRETURN_VOID;
}

/* 
   build up jid hold links for a job 
   no need to spool them or to send
   events to update schedd data 
*/
static void job_suc_pre_doit(lListElem *jep, bool array_deps) {
   lListElem *parent_jep;
   const lListElem *task;
   const lListElem *prep;
   int pre_nm, suc_nm;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   if (array_deps) {
      pre_nm = JB_ja_ad_predecessor_list;
      suc_nm = JB_ja_ad_successor_list;
   } else {
      pre_nm = JB_jid_predecessor_list;
      suc_nm = JB_jid_successor_list;
   }

   /*
    * here we check whether every job
    * in the predecessor list has exited
    */
   prep = lFirst(lGetList(jep, pre_nm));
   while (prep) {
      u_long32 pre_ident = lGetUlong(prep, JRE_job_number);

      parent_jep = lGetElemUlongRW(master_job_list, JB_job_number, pre_ident);
      if (parent_jep) {
         bool Exited = true;
         const lListElem *ja_task;

         if (lGetList(parent_jep, JB_ja_n_h_ids) != nullptr ||
             lGetList(parent_jep, JB_ja_u_h_ids) != nullptr ||
             lGetList(parent_jep, JB_ja_o_h_ids) != nullptr ||
             lGetList(parent_jep, JB_ja_a_h_ids) != nullptr ||
             lGetList(parent_jep, JB_ja_s_h_ids) != nullptr) {
            Exited = false;
         }
         if (Exited) {
            for_each_ep(ja_task, lGetList(parent_jep, JB_ja_tasks)) {
               if (lGetUlong(ja_task, JAT_status) != JFINISHED) {
                  Exited = false;
                  break;
               }
               for_each_ep(task, lGetList(ja_task, JAT_task_list)) {
                  /* skip the pseudo pe task used for summing up the usage
                   * of already finished tasks
                   */
                  if (strcmp(lGetString(task, PET_id), PE_TASK_PAST_USAGE_CONTAINER) != 0) {
                     if (lGetUlong(task, PET_status) != JFINISHED) {
                        /* at least one running pe task exists */
                        Exited = false;
                        break;
                     }
                  }
               }
               if (!Exited) {
                  break;
               }
            }
         }
         if (!Exited) {
            DPRINTF("adding jid " sge_u32" into successor list of job " sge_u32"\n", lGetUlong(jep, JB_job_number), pre_ident);

            /* add jid to successor_list of parent job */
            lAddSubUlong(parent_jep, JRE_job_number, lGetUlong(jep, JB_job_number), suc_nm, JRE_Type);

            prep = lNext(prep);
         } else {
            DPRINTF("job " sge_u32 " from predecessor list already exited - ignoring it\n", pre_ident);

            prep = lNext(prep);
            lDelSubUlong(jep, JRE_job_number, pre_ident, pre_nm);
         }
      } else {
         DPRINTF("predecessor job " sge_u32 " does not exist\n", pre_ident);
         prep = lNext(prep);
         lDelSubUlong(jep, JRE_job_number, pre_ident, pre_nm);
      }
   }

   DRETURN_VOID;
}

/****** sge_job_qmaster/job_suc_pre() ******************************************
*  NAME
*     job_suc_pre() -- build job depencency hold links
*
*  SYNOPSIS
*     void job_suc_pre(lListElem *jep) 
*
*  FUNCTION
*     Builds the hold links for job dependencies,
*     both in the dependent job, as well in its predecessor jobs.
*
*  INPUTS
*     lListElem *jep - the dependent job
*
*  NOTES
*     MT-NOTE: job_suc_pre() is MT safe if the caller holds the global lock
*
*     no need to spool the dependency links or to send events to 
*     update event clients - this is done in the calling functions
*
*  SEE ALSO
*     sge_job_qmaster/job_suc_pre_doit()
*     sge_job_qmaster/job_suc_pre_ad()
*******************************************************************************/
void job_suc_pre(lListElem *jep) {
   DENTER(TOP_LAYER);
   job_suc_pre_doit(jep, false);
   DRETURN_VOID;
}

/****** sge_job_qmaster/job_suc_pre_ad() ******************************************
*  NAME
*     job_suc_pre_ad() -- build job array task depencency hold links
*
*  SYNOPSIS
*     void job_suc_pre_ad(lListElem *jep) 
*
*  FUNCTION
*     Builds the hold links for job array task dependencies,
*     both in the dependent job, as well in its predecessor jobs.
*
*  INPUTS
*     lListElem *jep - the dependent job
*
*  NOTES
*     MT-NOTE: job_suc_pre_ad() is MT safe if the caller holds the global lock
*
*     no need to spool the dependency links or to send events to 
*     update event clients - this is done in the calling functions
*
*  SEE ALSO
*     sge_job_qmaster/job_suc_pre_doit()
*     sge_job_qmaster/job_suc_pre()
*******************************************************************************/
void job_suc_pre_ad(lListElem *jep) {
   DENTER(TOP_LAYER);
   job_suc_pre_doit(jep, true);
   DRETURN_VOID;
}

/* handle all per task attributes which are changeable 
   from outside using gdi requests 

   job - the job
   new_ja_task - new task structure DST; may be nullptr for not enrolled tasks
                 (not dispatched)
   tep  - reduced task element SRC
*/
static int
mod_task_attributes(const ocs::gdi::Packet *packet, lListElem *job, lListElem *new_ja_task,
                    lListElem *tep, lList **alpp, int *trigger, int is_array, int is_task_enrolled) {
   u_long32 jobid = lGetUlong(job, JB_job_number);
   u_long32 jataskid = lGetUlong(new_ja_task, JAT_task_number);
   int pos;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);

   DENTER(TOP_LAYER);

   if (is_task_enrolled) {

      /* --- JAT_fshare */
      if ((pos = lGetPosViaElem(tep, JAT_fshare, SGE_NO_ABORT)) >= 0) {
         u_long32 uval;

         /* need to be operator */
         if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
            ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, MSG_JOB_CHANGESHAREFUNC);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOOPR);
         }
         uval = lGetPosUlong(tep, pos);
         if (uval != lGetUlong(new_ja_task, JAT_fshare)) {
            lSetUlong(new_ja_task, JAT_fshare, uval);
            DPRINTF("JAT_fshare = " sge_u32 "\n", uval);
            *trigger |= MOD_EVENT;
         }

         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_SETSHAREFUNC_SSUUU,
                  packet->user, packet->host, jobid, jataskid, uval);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }

   }

   /* --- JAT_hold */
   if ((pos = lGetPosViaElem(tep, JAT_hold, SGE_NO_ABORT)) >= 0) {
      u_long32 op_code_and_hold = lGetPosUlong(tep, pos);
      u_long32 op_code = op_code_and_hold & ~MINUS_H_TGT_ALL;
      u_long32 target = op_code_and_hold & MINUS_H_TGT_ALL;
      int is_sub_op_code = (op_code == MINUS_H_CMD_SUB);
      u_long32 old_hold = job_get_hold_state(job, jataskid);
      u_long32 new_hold;

#if 0
      DPRINTF("******** jo_id = %d\n", jobid );
      DPRINTF("******** task_id = %d\n", jataskid );
      
      DPRINTF("********** op_code_and_hold = %x\n", op_code_and_hold );
      DPRINTF("******************* op_code = %x\n", op_code );
      DPRINTF("*************is_sub_op_code = %x\n", is_sub_op_code);
      DPRINTF("****************** old_hold = %x\n", old_hold);
      DPRINTF("******************** target = %x\n", target );
      DPRINTF("******* MINUS_H_TGT_SYSTEM  = %x\n", MINUS_H_TGT_SYSTEM );
      DPRINTF("***** MINUS_H_TGT_OPERATOR  = %x\n", MINUS_H_TGT_OPERATOR );
      DPRINTF("********* MINUS_H_TGT_USER  = %x\n", MINUS_H_TGT_USER);
#endif

      if (!is_task_enrolled) {
         new_ja_task = job_get_ja_task_template_pending(job, jataskid);
      }

      switch (op_code) {
         case MINUS_H_CMD_SUB:
            new_hold = old_hold & ~target;
            break;
         case MINUS_H_CMD_ADD:
            new_hold = old_hold | target;
            break;
         case MINUS_H_CMD_SET:
            new_hold = target;
            break;
         default:
            new_hold = old_hold;
            break;
      }

      if (new_hold != old_hold) {
         if ((target & MINUS_H_TGT_SYSTEM) == MINUS_H_TGT_SYSTEM) {
            if (!manop_is_manager(packet, master_manager_list)) {
               u_long32 new_mask = op_code_and_hold & ~MINUS_H_TGT_SYSTEM;
               lSetPosUlong(tep, pos, new_mask);
               ERROR(MSG_SGETEXT_MUST_BE_MGR_TO_SS, packet->user, is_sub_op_code ? MSG_JOB_RMHOLDMNG : MSG_JOB_SETHOLDMNG);
               answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_ENOOPR);
            }
         }

         if ((target & MINUS_H_TGT_OPERATOR) == MINUS_H_TGT_OPERATOR) {
            if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
               u_long32 new_mask = op_code_and_hold & ~MINUS_H_TGT_OPERATOR;
               lSetPosUlong(tep, pos, new_mask);

               ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, is_sub_op_code ? MSG_JOB_RMHOLDOP : MSG_JOB_SETHOLDOP);
               answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_ENOOPR);
            }
         }


         if ((target & MINUS_H_TGT_USER) == MINUS_H_TGT_USER) {
            if (strcmp(packet->user, lGetString(job, JB_owner)) &&
                !manop_is_operator(packet, master_manager_list, master_operator_list)) {
               u_long32 new_mask = op_code_and_hold & ~MINUS_H_TGT_USER;
               lSetPosUlong(tep, pos, new_mask);
               ERROR(MSG_SGETEXT_MUST_BE_JOB_OWN_TO_SUS, packet->user, jobid, is_sub_op_code ? MSG_JOB_RMHOLDUSER : MSG_JOB_SETHOLDUSER);
               answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_ENOOPR);
            }
         }
      }

      job_set_hold_state(job, nullptr, jataskid, new_hold);
      *trigger |= MOD_EVENT;

      if (new_hold != old_hold) {
         if (is_array) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JATASK_SUU, MSG_JOB_HOLD, jobid, jataskid);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_HOLD, jobid);
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
   }

   DRETURN(0);
}

/****** sge_job/is_changes_consumables() ******************************************
*  NAME
*     is_changes_consumables() -- detect changes with consumable resource request
*
*  SYNOPSIS
*     static bool is_changes_consumables(lList* new_lp, lList* old_lp) 
*
*  INPUTS
*     lList** alpp - answer list pointer pointer
*     lList*  new_lp  - jobs new JB_hard_resource_list
*     lList*  old_lp  - jobs old JB_hard_resource_list
*
*  RESULT
*     bool      - false, nothing changed
*
*  MT-NOTE:  is thread safe (works only on parsed in variables)
*
*******************************************************************************/

static bool is_changes_consumables(lList **alpp, const lList *new_lp, const lList *old_lp) {
   const lListElem *new_entry = nullptr;
   const lListElem *old_entry = nullptr;
   const char *name = nullptr;

   DENTER(TOP_LAYER);

   /* ensure all old resource requests implying consumables 
      debitation are still contained in new resource request list */
   for_each_ep(old_entry, old_lp) {

      /* ignore non-consumables */
      if (!lGetUlong(old_entry, CE_consumable)) {
         continue;
      }
      name = lGetString(old_entry, CE_name);

      /* search it in new hard resource list */
      if (lGetElemStr(new_lp, CE_name, name) == nullptr) {
         ERROR(MSG_JOB_MOD_MISSINGRUNNINGJOBCONSUMABLE_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(true);
      }
   }

   /* ensure all new resource requests implying consumable 
      debitation were also contained in old resource request list
      AND have not changed the requested amount */
   for_each_ep(new_entry, new_lp) {

      /* ignore non-consumables */
      if (!lGetUlong(new_entry, CE_consumable)) {
         continue;
      }
      name = lGetString(new_entry, CE_name);

      /* search it in old hard resource list */
      if ((old_entry = lGetElemStr(old_lp, CE_name, name)) == nullptr) {
         ERROR(MSG_JOB_MOD_ADDEDRUNNINGJOBCONSUMABLE_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(true);
      }

      /* compare request in old_entry with new_entry */
      DPRINTF("request: \"%s\" old: %f new: %f\n", name,
              lGetDouble(old_entry, CE_doubleval),
              lGetDouble(new_entry, CE_doubleval));

      if (lGetDouble(old_entry, CE_doubleval) !=
          lGetDouble(new_entry, CE_doubleval)) {
         ERROR(MSG_JOB_MOD_CHANGEDRUNNINGJOBCONSUMABLE_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(true);
      }
   }

   DRETURN(false);
}

/****** sge_job/deny_soft_consumables() ****************************************
*  NAME
*     deny_soft_consumables() -- Deny soft consumables
*
*  SYNOPSIS
*     static int deny_soft_consumables(lList **alpp, lList *srl)
*
*  FUNCTION
*     Find out if consumables are requested and deny them.
*
*  INPUTS
*     lList** alpp                    - answer list pointer pointer
*     lList *srl                      - jobs JB_soft_resource_list
*     const lList *master_centry_list - the master centry list
*
*  RESULT
*     static int - 0 request can pass
*                !=0 consumables requested soft
*
*******************************************************************************/
int deny_soft_consumables(lList **alpp, const lList *srl, const lList *master_centry_list) {
   const lListElem *entry, *dcep;
   const char *name;

   DENTER(TOP_LAYER);

   /* ensure no consumables are requested in JB_soft_resource_list */
   for_each_ep(entry, srl) {
      name = lGetString(entry, CE_name);

      if (!(dcep = centry_list_locate(master_centry_list, name))) {
         ERROR(MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }

      /* ignore non-consumables */
      if (lGetUlong(dcep, CE_consumable)) {
         ERROR(MSG_JOB_MOD_SOFTREQCONSUMABLE_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

static int
mod_job_attributes(const ocs::gdi::Packet *packet, lListElem *new_job, lListElem *jep, lList **alpp, int *trigger) {
   int pos;
   int is_running = 0, may_not_be_running = 0;
   u_long32 uval;
   u_long32 jobid = lGetUlong(new_job, JB_job_number);

   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);

   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);

   DENTER(TOP_LAYER);

   /* is job running ? */
   {
      const lListElem *ja_task;
      for_each_ep(ja_task, lGetList(new_job, JB_ja_tasks)) {
         if (lGetUlong(ja_task, JAT_status) & JTRANSFERING ||
             lGetUlong(ja_task, JAT_status) & JRUNNING) {
            is_running = 1;
         }
      }
   }

   /* 
    * ---- JB_ja_tasks
    *      Do we have per task change request? 
    */
   if ((pos = lGetPosViaElem(jep, JB_ja_tasks, SGE_NO_ABORT)) >= 0) {
      lList *ja_task_list = lGetPosList(jep, pos);
      lListElem *ja_task = lFirstRW(ja_task_list);
      int new_job_is_array = job_is_array(new_job);
      u_long32 jep_ja_task_number = lGetNumberOfElem(ja_task_list);

      /* 
       * Is it a valid per task request:
       *    - at least one task element 
       *    - task id field 
       *    - multi tasks requests are only valid for array jobs 
       */
      if (!ja_task) {
         ERROR(MSG_SGETEXT_NEEDONEELEMENT_SS, lNm2Str(JB_ja_tasks), __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      if ((pos = lGetPosViaElem(ja_task, JAT_task_number, SGE_NO_ABORT)) < 0) {
         ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(JAT_task_number), __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      if (!new_job_is_array && jep_ja_task_number > 1) {
         ERROR(MSG_JOB_NOJOBARRAY_U, jobid);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }

      /* 
       * Visit tasks
       */
      if (ja_task_list != nullptr) {
         const lListElem *first = lFirst(ja_task_list);
         u_long32 handle_all_tasks = !lGetUlong(first, JAT_task_number);

         if (handle_all_tasks) {
            int list_id[] = {JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_o_h_ids,
                             JB_ja_s_h_ids, -1};
            lListElem *dst_ja_task = nullptr;
            int i = -1;

            /*
             * Visit all unenrolled tasks
             */
            while (list_id[++i] != -1) {
               lList *range_list =
                       lCopyList("task_id_range", lGetList(new_job, list_id[i]));
               const lListElem *range = nullptr;
               u_long32 id;

               for_each_ep(range, range_list) {
                  for (id = lGetUlong(range, RN_min);
                       id <= lGetUlong(range, RN_max);
                       id += lGetUlong(range, RN_step)) {

                     dst_ja_task =
                             job_get_ja_task_template_pending(new_job, id);

                     mod_task_attributes(packet, new_job, dst_ja_task, ja_task,
                                         alpp, trigger, job_is_array(new_job), 0);
                  }
               }
               lFreeList(&range_list);
            }
            /*
             * Visit enrolled tasks
             */
            for_each_rw (dst_ja_task, lGetList(new_job, JB_ja_tasks)) {
               mod_task_attributes(packet, new_job, dst_ja_task, ja_task, alpp,
                                   trigger, job_is_array(new_job), 1);
            }
         } else {
            for_each_rw (ja_task, ja_task_list) {
               u_long32 ja_task_id = lGetUlong(ja_task, JAT_task_number);
               int is_defined = job_is_ja_task_defined(new_job, ja_task_id);

               if (is_defined) {
                  lListElem *dst_ja_task = nullptr;
                  int is_enrolled = 1;

                  dst_ja_task = job_search_task(new_job, nullptr, ja_task_id);
                  if (dst_ja_task == nullptr) {
                     is_enrolled = 0;
                     dst_ja_task =
                             job_get_ja_task_template_pending(new_job,
                                                              ja_task_id);
                  }
                  mod_task_attributes(packet, new_job, dst_ja_task, ja_task, alpp,
                                      trigger, job_is_array(new_job), is_enrolled);
               } else { ; /* Ignore silently */
               }
            }
         }
      }
   }


   /* ---- JB_override_tickets 
           A attribute that must be allowed to 
           be changed when job is running
   */
   if ((pos = lGetPosViaElem(jep, JB_override_tickets, SGE_NO_ABORT)) >= 0) {
      uval = lGetPosUlong(jep, pos);

      /* need to be operator */
      if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
         ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, MSG_JOB_CHANGEOVERRIDETICKS);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ENOOPR);
      }

      /* ok, do it */
      if (uval != lGetUlong(new_job, JB_override_tickets)) {
         lSetUlong(new_job, JB_override_tickets, uval);
         *trigger |= MOD_EVENT;
      }

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_SETOVERRIDETICKS_SSUU, packet->user, packet->host, jobid, uval);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_priority */
   if ((pos = lGetPosViaElem(jep, JB_priority, SGE_NO_ABORT)) >= 0) {
      u_long32 old_priority;
      uval = lGetPosUlong(jep, pos);
      if (uval > (old_priority = lGetUlong(new_job, JB_priority))) {
         /* need to be at least operator */
         if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
            ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, MSG_JOB_PRIOINC);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOOPR);
         }
      }
      /* ok, do it */
      if (uval != old_priority) {
         *trigger |= MOD_EVENT;
      }

      job_normalize_priority(new_job, uval);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_PRIOSET_SSUI, packet->user, packet->host, jobid, ((int) (uval)) - BASE_PRIORITY);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_jobshare */
   if ((pos = lGetPosViaElem(jep, JB_jobshare, SGE_NO_ABORT)) >= 0) {
      u_long32 old_jobshare;
      uval = lGetPosUlong(jep, pos);
      if (uval != (old_jobshare = lGetUlong(new_job, JB_jobshare))) {
         /* need to be owner or at least operator */
         if (strcmp(packet->user, lGetString(new_job, JB_owner)) &&
             !manop_is_operator(packet, master_manager_list, master_operator_list)) {
            ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, MSG_JOB_CHANGEJOBSHARE);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOOPR);
         }
      }
      /* ok, do it */
      if (uval != old_jobshare)
         *trigger |= PRIO_EVENT;

      lSetUlong(new_job, JB_jobshare, uval);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_JOBSHARESET_SSUU, packet->user, packet->host, jobid, uval);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_ar */
   if ((pos = lGetPosViaElem(jep, JB_ar, SGE_NO_ABORT)) >= 0) {
      u_long32 ar_id = lGetUlong(new_job, JB_ar);
      uval = lGetPosUlong(jep, pos);
      if (uval != ar_id) {
         /* need to be owner or at least operator */
         if (strcmp(packet->user, lGetString(new_job, JB_owner)) &&
             !manop_is_operator(packet, master_manager_list, master_operator_list)) {
            ERROR(MSG_SGETEXT_MUST_BE_OPR_TO_SS, packet->user, MSG_JOB_CHANGEJOBAR);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOOPR, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOOPR);
         }
         *trigger |= PRIO_EVENT;
         may_not_be_running = 1;
      }
      /* ok, do it */
      lSetUlong(new_job, JB_ar, uval);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_JOBARSET_SSUU, packet->user, packet->host, jobid, uval);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }


   /* ---- JB_deadline */
   /* If it is a deadline job the user has to be a deadline user */
   if ((pos = lGetPosViaElem(jep, JB_deadline, SGE_NO_ABORT)) >= 0) {
      if (!user_is_deadline_user(packet, master_userset_list)) {
         ERROR(MSG_JOB_NODEADLINEUSER_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      } else {
         lSetUlong64(new_job, JB_deadline, lGetUlong64(jep, JB_deadline));
         *trigger |= MOD_EVENT;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_DEADLINETIME, jobid);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
   }


   /* ---- JB_execution_time */
   if ((pos = lGetPosViaElem(jep, JB_execution_time, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_execution_time\n");
      lSetUlong64(new_job, JB_execution_time, lGetUlong64(jep, JB_execution_time));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_STARTTIME, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_account */
   if ((pos = lGetPosViaElem(jep, JB_account, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_account\n");
      if (verify_str_key(alpp, lGetString(jep, JB_account), MAX_VERIFY_STRING,
                         "account string", QSUB_TABLE) != STATUS_OK) {
         DRETURN(STATUS_EUNKNOWN);
      }
      lSetString(new_job, JB_account, lGetString(jep, JB_account));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_ACCOUNT, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_cwd */
   if ((pos = lGetPosViaElem(jep, JB_cwd, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_cwd\n");
      lSetString(new_job, JB_cwd, lGetString(jep, JB_cwd));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_WD, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_checkpoint_name */
   if ((pos = lGetPosViaElem(jep, JB_checkpoint_name, SGE_NO_ABORT)) >= 0) {
      const char *ckpt_name;

      DPRINTF("got new JB_checkpoint_name\n");
      ckpt_name = lGetString(jep, JB_checkpoint_name);
      if (ckpt_name && !ckpt_list_locate(master_ckpt_list, ckpt_name)) {
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CKPT, ckpt_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      lSetString(new_job, JB_checkpoint_name, ckpt_name);
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_OBJ_CKPT, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_stderr_path_list */
   if ((pos = lGetPosViaElem(jep, JB_stderr_path_list, SGE_NO_ABORT)) >= 0) {
      int status;
      DPRINTF("got new JB_stderr_path_list\n");

      if ((status = job_resolve_host_for_path_list(jep, alpp, JB_stderr_path_list)) != STATUS_OK) {
         DRETURN(status);
      }
      lSetList(new_job, JB_stderr_path_list, lCopyList("", lGetList(jep, JB_stderr_path_list)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_STDERRPATHLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_stdin_path_list */
   if ((pos = lGetPosViaElem(jep, JB_stdin_path_list, SGE_NO_ABORT)) >= 0) {
      int status;
      DPRINTF("got new JB_stdin_path_list\n");

      if ((status = job_resolve_host_for_path_list(jep, alpp, JB_stdin_path_list)) != STATUS_OK) {
         DRETURN(status);
      }
      lSetList(new_job, JB_stdin_path_list, lCopyList("", lGetList(jep, JB_stdin_path_list)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_STDINPATHLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_reserve */
   if ((pos = lGetPosViaElem(jep, JB_reserve, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_reserve\n");
      lSetBool(new_job, JB_reserve, lGetBool(jep, JB_reserve));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_RESERVE, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_merge_stderr */
   if ((pos = lGetPosViaElem(jep, JB_merge_stderr, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_merge_stderr\n");
      lSetBool(new_job, JB_merge_stderr, lGetBool(jep, JB_merge_stderr));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_MERGEOUTPUT, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_pe - need to do it *before* JB_request_set_list */
   if ((pos = lGetPosViaElem(jep, JB_pe, SGE_NO_ABORT)) >= 0) {
      const char *pe_name;

      DPRINTF("got new JB_pe\n");
      pe_name = lGetString(jep, JB_pe);
      if (pe_name && !pe_list_find_matching(master_pe_list, pe_name)) {
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_PE, pe_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      lSetString(new_job, JB_pe, pe_name);
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_OBJ_PE, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_pe_range */
   if ((pos = lGetPosViaElem(jep, JB_pe_range, SGE_NO_ABORT)) >= 0 && lGetList(jep, JB_pe_range)) {
      lList *pe_range;
      const char *pe_name;
      DPRINTF("got new JB_pe_range\n");

      /* reject PE ranges change requests for jobs without PE request */
      if (!(pe_name = lGetString(new_job, JB_pe))) {
         ERROR(SFNMAX, MSG_JOB_PERANGE_ONLY_FOR_PARALLEL);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }

      pe_range = lCopyList("", lGetList(jep, JB_pe_range));
      if (object_verify_pe_range(alpp, pe_name, pe_range, SGE_OBJ_JOB) != STATUS_OK) {
         lFreeList(&pe_range);
         DRETURN(STATUS_EUNKNOWN);
      }
      lSetList(new_job, JB_pe_range, pe_range);

      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_SLOTRANGE, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   // ---- JB_request_set_list

   // need to loop over all JB_request_set_list elements and check the entries
   if ((pos = lGetPosViaElem(jep, JB_request_set_list, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_request_set_list\n");
      lListElem *jrs;
      for_each_rw(jrs, lGetList(jep, JB_request_set_list)) {
         u_long32 scope = lGetUlong(jrs, JRS_scope);
         DPRINTF("request set of scope " sge_u32 "\n", scope);

         // ---- JRS_hard_resource_list
         lList *resource_list = lGetListRW(jrs, JRS_hard_resource_list);
         if (resource_list != nullptr) {
            DPRINTF("got new JRS_hard_resource_list\n");
            if (centry_list_fill_request(resource_list, alpp, master_centry_list, false, true, false)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            if (compress_ressources(alpp, resource_list, SGE_OBJ_JOB)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            if (!centry_list_is_correct(resource_list, alpp)) {
               DRETURN(STATUS_EUNKNOWN);
            }

            /* to prevent inconsistent consumable management:
             * - deny resource requests changes on consumables for running jobs (IZ #251)
             * - a better solution would be to store for each running job the amount of resources
             *   @todo we have this now in the JAT_granted_resources_list
             */
            const lList *old_resource_list = job_get_hard_resource_list(new_job, scope);
            bool is_changed = is_changes_consumables(alpp, resource_list, old_resource_list);
            if (is_running && is_changed) {
               DRETURN(STATUS_EUNKNOWN);
            }

            job_set_hard_resource_list(new_job, lCopyList(nullptr, resource_list), scope);
            *trigger |= MOD_EVENT;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_HARDRESOURCELIST, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }


         // ---- JRS_soft_resource_list
         resource_list = lGetListRW(jrs, JRS_soft_resource_list);
         if (resource_list != nullptr) {
            DPRINTF("got new JRS_soft_resource_list\n");
            if (centry_list_fill_request(resource_list, alpp, master_centry_list, false, true, false)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            if (compress_ressources(alpp, resource_list, SGE_OBJ_JOB)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            if (deny_soft_consumables(alpp, resource_list, master_centry_list)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            if (!centry_list_is_correct(resource_list, alpp)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            job_set_soft_resource_list(new_job, lCopyList(nullptr, resource_list), scope);
            *trigger |= MOD_EVENT;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_SOFTRESOURCELIST, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }


         // JRS_hard_queue_list
         lList *queue_list = lGetListRW(jrs, JRS_hard_queue_list);
         if (queue_list != nullptr) {
            DPRINTF("got new JRS_hard_queue_list\n");
            if (!qref_list_is_valid(queue_list, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            job_set_hard_queue_list(new_job, lCopyList(nullptr, queue_list), scope);
            *trigger |= MOD_EVENT;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_HARDQLIST, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }


         // JRS_soft_queue_list
         queue_list = lGetListRW(jrs, JRS_soft_queue_list);
         if (queue_list != nullptr) {
            DPRINTF("got new JRS_soft_queue_list\n");
            if (!qref_list_is_valid(queue_list, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
               DRETURN(STATUS_EUNKNOWN);
            }
            job_set_soft_queue_list(new_job, lCopyList(nullptr, queue_list), scope);
            *trigger |= MOD_EVENT;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_SOFTQLIST, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }
      } // foreach scope
      if (!job_verify_adjust_request_set(alpp, new_job, master_centry_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   } // JB_request_set_list

   /* ---- JB_mail_options */
   if ((pos = lGetPosViaElem(jep, JB_mail_options, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_mail_options\n");
      lSetUlong(new_job, JB_mail_options, lGetUlong(jep, JB_mail_options));
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_MAILOPTIONS, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_mail_list */
   if ((pos = lGetPosViaElem(jep, JB_mail_list, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_mail_list\n");
      lSetList(new_job, JB_mail_list,
               lCopyList("", lGetList(jep, JB_mail_list)));
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_MAILLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_job_name */
   if ((pos = lGetPosViaElem(jep, JB_job_name, SGE_NO_ABORT)) >= 0 && lGetString(jep, JB_job_name)) {
/*      u_long32 succ_jid;*/
      const char *new_name = lGetString(jep, JB_job_name);

      DPRINTF("got new JB_job_name\n");

      /* preform checks only if job name _really_ changes */
      if (strcmp(new_name, lGetString(new_job, JB_job_name))) {
         char job_descr[100];
         const char *job_name;

         snprintf(job_descr, sizeof(job_descr), "job " sge_u32, jobid);
         job_name = lGetString(new_job, JB_job_name);
         lSetString(new_job, JB_job_name, new_name);
         if (object_verify_name(new_job, alpp, JB_job_name)) {
            lSetString(new_job, JB_job_name, job_name);
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_JOBNAME, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_jid_predecessor_list */
   if ((pos = lGetPosViaElem(jep, JB_jid_request_list, SGE_NO_ABORT)) >= 0 &&
       lGetList(jep, JB_jid_request_list)) {
      lList *new_pre_list = nullptr;
      lList *exited_pre_list = nullptr;
      lListElem *pre, *exited, *nxt, *job;

      lList *req_list = nullptr, *pred_list = nullptr;

      if (lGetPosViaElem(jep, JB_ja_tasks, SGE_NO_ABORT) != -1) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPTIONONLEONJOBS_U, jobid);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

         DRETURN(STATUS_EUNKNOWN);
      }

      DPRINTF("got new JB_jid_predecessor_list\n");

      if (lGetNumberOfElem(lGetList(jep, JB_jid_request_list)) > 0)
         req_list = lCopyList("requested_jid_list", lGetList(jep, JB_jid_request_list));

      lXchgList(new_job, JB_jid_request_list, &req_list);
      lXchgList(new_job, JB_jid_predecessor_list, &pred_list);

      if (job_verify_predecessors(new_job, alpp)) {
         lXchgList(new_job, JB_jid_request_list, &req_list);
         lXchgList(new_job, JB_jid_predecessor_list, &pred_list);
         lFreeList(&req_list);
         lFreeList(&pred_list);
         DRETURN(STATUS_EUNKNOWN);
      }

      lFreeList(&req_list);
      lFreeList(&pred_list);

      new_pre_list = lGetListRW(new_job, JB_jid_predecessor_list);

      /* remove jobid's of all no longer existing jobs from this
         new job - this must be done before event is sent to schedd */
      nxt = lFirstRW(new_pre_list);
      while ((pre = nxt)) {
         int move_to_exited = 0;
         u_long32 pre_ident = lGetUlong(pre, JRE_job_number);

         nxt = lNextRW(pre);
         DPRINTF("jid: " sge_u32 "\n", pre_ident);

         job = lGetElemUlongRW(master_job_list, JB_job_number, pre_ident);

         /* in SGE jobs are exited when they dont exist */
         if (!job) {
            move_to_exited = 1;
         }

         if (move_to_exited) {
            if (!exited_pre_list)
               exited_pre_list = lCreateList("exited list", JRE_Type);
            exited = lDechainElem(new_pre_list, pre);
            lAppendElem(exited_pre_list, exited);
         }
      }

      if (!lGetNumberOfElem(new_pre_list)) {
         lSetList(new_job, JB_jid_predecessor_list, nullptr);
         new_pre_list = nullptr;
      } else if (contains_dependency_cycles(new_job, lGetUlong(new_job, JB_job_number), alpp)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      *trigger |= (RECHAIN_JID_HOLD | MOD_EVENT);

      /* added primarily for own debugging purposes */
      {
         char str_predec[256], str_exited[256];
         const char *delis[] = {nullptr, ",", ""};

         int fields[] = {JRE_job_number, 0};
         uni_print_list(nullptr, str_predec, sizeof(str_predec) - 1, new_pre_list, fields, delis, 0);
         uni_print_list(nullptr, str_exited, sizeof(str_exited) - 1, exited_pre_list, fields, delis, 0);
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_HOLDLISTMOD_USS, jobid, str_predec, str_exited);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
      lFreeList(&exited_pre_list);
   }

   /* ---- JB_ja_ad_predecessor_list */
   if ((pos = lGetPosViaElem(jep, JB_ja_ad_request_list, SGE_NO_ABORT)) >= 0 &&
       lGetList(jep, JB_ja_ad_request_list)) {
      lList *new_pre_list = nullptr, *exited_pre_list = nullptr;
      lListElem *pre, *exited, *nxt, *job;

      lList *req_list = nullptr, *pred_list = nullptr;

      if (lGetPosViaElem(jep, JB_ja_tasks, SGE_NO_ABORT) != -1) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPTIONONLEONJOBS_U, jobid);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

         DRETURN(STATUS_EUNKNOWN);
      }

      DPRINTF("got new JB_ja_ad_predecessor_list\n");

      if (lGetNumberOfElem(lGetList(jep, JB_ja_ad_request_list)) > 0)
         req_list = lCopyList("requested_ja_ad_list", lGetList(jep, JB_ja_ad_request_list));

      lXchgList(new_job, JB_ja_ad_request_list, &req_list);
      lXchgList(new_job, JB_ja_ad_predecessor_list, &pred_list);

      if (job_verify_predecessors_ad(new_job, alpp, packet->gdi_session)) {
         lXchgList(new_job, JB_ja_ad_request_list, &req_list);
         lXchgList(new_job, JB_ja_ad_predecessor_list, &pred_list);
         lFreeList(&req_list);
         lFreeList(&pred_list);
         DRETURN(STATUS_EUNKNOWN);
      }

      lFreeList(&req_list);
      lFreeList(&pred_list);

      new_pre_list = lGetListRW(new_job, JB_ja_ad_predecessor_list);

      /* remove jobid's of all no longer existing jobs from this
         new job - this must be done before event is sent to schedd */
      nxt = lFirstRW(new_pre_list);
      while ((pre = nxt)) {
         int move_to_exited = 0;
         u_long32 pre_ident = lGetUlong(pre, JRE_job_number);

         nxt = lNextRW(pre);
         DPRINTF("jid: " sge_u32"\n", pre_ident);

         job = lGetElemUlongRW(master_job_list, JB_job_number, pre_ident);

         /* in SGE jobs are exited when they dont exist */
         if (!job) {
            move_to_exited = 1;
         }

         if (move_to_exited) {
            if (!exited_pre_list)
               exited_pre_list = lCreateList("exited list", JRE_Type);
            exited = lDechainElem(new_pre_list, pre);
            lAppendElem(exited_pre_list, exited);
         }
      }

      if (!lGetNumberOfElem(new_pre_list)) {
         lSetList(new_job, JB_ja_ad_predecessor_list, nullptr);
         new_pre_list = nullptr;
      } else if (contains_dependency_cycles(new_job, lGetUlong(new_job, JB_job_number), alpp)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      *trigger |= (RECHAIN_JA_AD_HOLD | MOD_EVENT);

      /* added primarily for debugging purposes */
      {
         char str_predec[256], str_exited[256];
         const char *delis[] = {nullptr, ",", ""};

         int fields[] = {JRE_job_number, 0};
         uni_print_list(nullptr, str_predec, sizeof(str_predec) - 1, new_pre_list, fields, delis, 0);
         uni_print_list(nullptr, str_exited, sizeof(str_exited) - 1, exited_pre_list, fields, delis, 0);
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_HOLDARRAYLISTMOD_USS, jobid, str_predec, str_exited);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
      lFreeList(&exited_pre_list);
   }

   /* ---- JB_notify */
   if ((pos = lGetPosViaElem(jep, JB_notify, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_notify\n");
      lSetBool(new_job, JB_notify, lGetBool(jep, JB_notify));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_NOTIFYBEHAVIOUR, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_stdout_path_list */
   if ((pos = lGetPosViaElem(jep, JB_stdout_path_list, SGE_NO_ABORT)) >= 0) {
      int status;
      DPRINTF("got new JB_stdout_path_list?\n");

      if ((status = job_resolve_host_for_path_list(jep, alpp, JB_stdout_path_list)) != STATUS_OK) {
         DRETURN(status);
      }
      lSetList(new_job, JB_stdout_path_list, lCopyList("", lGetList(jep, JB_stdout_path_list)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_STDOUTPATHLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_project */
   if ((pos = lGetPosViaElem(jep, JB_project, SGE_NO_ABORT)) >= 0) {
      int ret;
      int changed;
      const char *new_project = lGetString(jep, JB_project);
      const char *old_project = lGetString(new_job, JB_project);

      if (old_project == nullptr && new_project == nullptr) {
         changed = 0;
      } else {
         if (old_project == nullptr || new_project == nullptr) {
            changed = 1;
         } else {
            changed = strcmp(old_project, new_project);
         }
      }
      if (changed) {
         DPRINTF("got new JB_project\n");

         ret = job_verify_project(jep, alpp, lGetString(new_job, JB_owner), lGetString(new_job, JB_group), lGetList(new_job, JB_grp_list));
         if (ret != STATUS_OK) {
            DRETURN(ret);
         }

         lSetString(new_job, JB_project, new_project);
         may_not_be_running = 1;
         *trigger |= MOD_EVENT;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_PROJECT, jobid);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
   }

   /* ---- JB_binding */
   if ((pos = lGetPosViaElem(jep, JB_binding, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_binding\n");

      lSetList(new_job, JB_binding,
               lCopyList("", lGetList(jep, JB_binding)));

      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_BINDING, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_restart */
   if ((pos = lGetPosViaElem(jep, JB_restart, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_restart\n");
      lSetUlong(new_job, JB_restart, lGetUlong(jep, JB_restart));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_RESTARTBEHAVIOR, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_shell_list */
   if ((pos = lGetPosViaElem(jep, JB_shell_list, SGE_NO_ABORT)) >= 0) {
      int status;
      DPRINTF("got new JB_shell_list\n");

      if ((status = job_resolve_host_for_path_list(jep, alpp, JB_shell_list)) != STATUS_OK) {
         DRETURN(status);
      }
      lSetList(new_job, JB_shell_list, lCopyList("", lGetList(jep, JB_shell_list)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_SHELLLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_env_list */
   if ((pos = lGetPosViaElem(jep, JB_env_list, SGE_NO_ABORT)) >= 0) {
      lList *prefix_vars = nullptr;
      lList *tmp_var_list = nullptr;

      DPRINTF("got new JB_env_list\n");

      /* check for qsh without DISPLAY set */
      if (JOB_TYPE_IS_QSH(lGetUlong(new_job, JB_type))) {
         int ret = job_check_qsh_display(jep, alpp, false);
         if (ret != STATUS_OK) {
            DRETURN(ret);
         }
      }

      /* save existing prefix env vars from being overwritten
         TODO: can we rule out that after that step a prefix 
               env var appears two times in the env var list ? */
      tmp_var_list = lGetListRW(new_job, JB_env_list);
      var_list_split_prefix_vars(&tmp_var_list, &prefix_vars, VAR_PREFIX);
      lSetList(new_job, JB_env_list, lCopyList("", lGetList(jep, JB_env_list)));
      lAddList(lGetListRW(new_job, JB_env_list), &prefix_vars);
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_ENVLIST, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

      /* remove potentially dangerous environment variables */
      var_list_filter_env_list(lGetListRW(new_job, JB_env_list), alpp);
   }

   /* ---- JB_qs_args */
   if ((pos = lGetPosViaElem(jep, JB_qs_args, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_qs_args\n");
      lSetList(new_job, JB_qs_args, lCopyList("", lGetList(jep, JB_qs_args)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_QSARGS, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_job_args */
   if ((pos = lGetPosViaElem(jep, JB_job_args, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_job_args\n");
      lSetList(new_job, JB_job_args, lCopyList("", lGetList(jep, JB_job_args)));
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_SCRIPTARGS, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_verify_suitable_queues */
   if ((pos = lGetPosViaElem(jep, JB_verify_suitable_queues, SGE_NO_ABORT)) >= 0) {
      int ret;
      lSetUlong(new_job, JB_verify_suitable_queues,
                lGetUlong(jep, JB_verify_suitable_queues));
      ret = verify_suitable_queues(alpp, new_job, trigger, true);
      if (lGetUlong(new_job, JB_verify_suitable_queues) == JUST_VERIFY ||
          lGetUlong(new_job, JB_verify_suitable_queues) == POKE_VERIFY || ret != 0) {
         DRETURN(ret);
      }
   }

   /* ---- JB_context */
   if ((pos = lGetPosViaElem(jep, JB_context, SGE_NO_ABORT)) >= 0) {
      DPRINTF("got new JB_context\n");
      set_context(lGetListRW(jep, JB_context), new_job);
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_CONTEXT, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* ---- JB_ja_task_concurrency */
   if ((pos = lGetPosViaElem(jep, JB_ja_task_concurrency, SGE_NO_ABORT)) >= 0) {
      u_long32 task_concurrency;
      DPRINTF("got new JB_ja_task_concurrency\n");
      task_concurrency = lGetUlong(jep, JB_ja_task_concurrency);

      if (task_concurrency > 0 && !job_is_array(new_job)) {
         ERROR(MSG_PARSE_INVALIDOPTIONARGUMENTX_S, MSG_JOB_TASK_CONCURRENCY);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }

      lSetUlong(new_job, JB_ja_task_concurrency, task_concurrency);
      *trigger |= MOD_EVENT;
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_MOD_JOBS_SU, MSG_JOB_TASK_CONCURRENCY, task_concurrency);
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /* deny certain modifications of running jobs */
   if (may_not_be_running && is_running) {
      ERROR(MSG_SGETEXT_CANT_MOD_RUNNING_JOBS_U, jobid);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   DRETURN(0);
}

/****** sge_job_qmaster/contains_dependency_cycles() ***************************
*  NAME
*     contains_dependency_cycles() -- detects cycles in the job dependencies 
*
*  SYNOPSIS
*     static bool contains_dependency_cycles(const lListElem * new_job, 
*     u_long32 job_number, lList **alpp) 
*
*  FUNCTION
*     This function follows the deep search allgorithm, to look for cycles
*     in the job dependency list. It stops, when the first cycle is found. It
*     only performes the cycle check for a given job and not for all jobs in 
*     the system.
*
*  INPUTS
*     const lListElem * new_job - job, which dependency have to be evaludated 
*     u_long32 job_number       - job number, of the first job 
*     lList **alpp              - answer list 
*
*  RESULT
*     static bool - true, if there is a dependency cycle
*
*  MT-NOTE
*     Is not thread save. Reads from the global Job-List 
*
*******************************************************************************/
static bool contains_dependency_cycles(const lListElem *new_job, u_long32 job_number, lList **alpp) {
   bool is_cycle = false;
   const lList *predecessor_list = lGetList(new_job, JB_jid_predecessor_list);
   const lList *predecessor_list_ad = lGetList(new_job, JB_ja_ad_predecessor_list);
   const lListElem *pre_elem = nullptr;
   u_long32 pre_nr;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   for_each_ep(pre_elem, predecessor_list) {
      pre_nr = lGetUlong(pre_elem, JRE_job_number);
      if (pre_nr == job_number) {
         u_long32 temp = lGetUlong(new_job, JB_job_number);
         ERROR(MSG_JOB_DEPENDENCY_CYCLE_UU, job_number, temp);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

         is_cycle = true;
      } else {
         is_cycle = contains_dependency_cycles(lGetElemUlong(master_job_list, JB_job_number, pre_nr), job_number, alpp);
      }
      if (is_cycle)
         break;
   }

   for_each_ep(pre_elem, predecessor_list_ad) {
      pre_nr = lGetUlong(pre_elem, JRE_job_number);
      if (pre_nr == job_number) {
         u_long32 temp = lGetUlong(new_job, JB_job_number);
         ERROR(MSG_JOB_DEPENDENCY_CYCLE_UU, job_number, temp);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

         is_cycle = true;
      } else {
         is_cycle = contains_dependency_cycles(lGetElemUlong(master_job_list, JB_job_number, pre_nr), job_number, alpp);
      }
      if (is_cycle)
         break;
   }

   DRETURN(is_cycle);
}

/****** qmaster/job/job_verify_predecessors() *********************************
*  NAME
*     job_verify_predecessors() -- verify -hold_jid list of a job
*
*  SYNOPSIS
*     static int job_verify_predecessors(const lListElem *job,
*                                        lList **alpp,  
*                                        lList *predecessors) 
*
*  FUNCTION
*     These checks are done:
*       #1 Ensure the job will not become it's own predecessor
*       #2 resolve job names and regulare expressions. The
*          job ids will be stored in JB_jid_predecessor_list
*
*  INPUTS
*     const lListElem *job - JB_Type element (JB_job_number may be 0 if
*                            not yet know (at submit time)
*     lList **alpp         - the answer list
*
*  RESULT
*     int - returns != 0 if there is a problem with predecessors
******************************************************************************/
int job_verify_predecessors(lListElem *job, lList **alpp) {
   u_long32 jobid = lGetUlong(job, JB_job_number);
   const lList *predecessors_req = nullptr;
   lList *predecessors_id = nullptr;
   const lListElem *pre;
   lListElem *pre_temp;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   predecessors_req = lGetList(job, JB_jid_request_list);
   predecessors_id = lCreateList("job_predecessors", JRE_Type);
   if (!predecessors_id) {
      ERROR(SFNMAX, MSG_JOB_MOD_JOBDEPENDENCY_MEMORY);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   for_each_ep(pre, predecessors_req) {
      const char *pre_ident = lGetString(pre, JRE_job_name);

      if (isdigit(pre_ident[0])) {
         if (strchr(pre_ident, '.')) {
            lFreeList(&predecessors_id);
            DPRINTF("a job cannot wait for a task to finish\n");
            ERROR(MSG_JOB_MOD_UNKNOWNJOBTOWAITFOR_S, pre_ident);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         if (SGE_STRTOU_LONG32(pre_ident) == jobid) {
            lFreeList(&predecessors_id);
            DPRINTF("got my own jobid in JRE_job_name\n");
            ERROR(MSG_JOB_MOD_GOTOWNJOBIDINHOLDJIDOPTION_U, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         pre_temp = lCreateElem(JRE_Type);
         if (pre_temp) {
            lSetUlong(pre_temp, JRE_job_number, atoi(pre_ident));
            lAppendElem(predecessors_id, pre_temp);
         }

      } else {
         const lListElem *user_job = nullptr;         /* JB_Type */
         const lListElem *next_user_job = nullptr;    /* JB_Type */
         const void *user_iterator = nullptr;
         const char *owner = lGetString(job, JB_owner);

         next_user_job = lGetElemStrFirst(master_job_list, JB_owner, owner, &user_iterator);
         while ((user_job = next_user_job)) {
            const char *job_name = lGetString(user_job, JB_job_name);
            int result = string_base_cmp(TYPE_RESTR, pre_ident, job_name);

            if (!result) {
               if (lGetUlong(user_job, JB_job_number) != jobid) {
                  pre_temp = lCreateElem(JRE_Type);
                  if (pre_temp) {
                     lSetUlong(pre_temp, JRE_job_number, lGetUlong(user_job, JB_job_number));
                     lAppendElem(predecessors_id, pre_temp);
                  }
               }
            }

            next_user_job = lGetElemStrNext(master_job_list, JB_owner, owner, &user_iterator);
         }

         /* if no matching job has been found we have to assume 
            the job finished already */
      }
   }
   if (lGetNumberOfElem(predecessors_id) == 0) {
      lFreeList(&predecessors_id);
   }

   lSetList(job, JB_jid_predecessor_list, predecessors_id);

   DRETURN(0);
}

/****** qmaster/job/job_verify_predecessors_ad() *********************************
*  NAME
*     job_verify_predecessors_ad() -- verify -hold_jid_ad list of a job
*
*  SYNOPSIS
*     static int job_verify_predecessors_ad(lListElem *job, lList **alpp) 
*
*  FUNCTION
*     These checks are done:
*       #1 Ensure the job will not become it's own predecessor
*       #2 Resolve job names and regulare expressions. The
*          job ids will be stored in JB_ja_ad_predecessor_list
*       #3 Ensure the jobs in the predecessor list are equivalent array jobs
*       #4 Update JB_ja_a_h_ids and JB_ja_a_n_ids according to the 
           predecessors list
*
*  INPUTS
*     lListElem *job - JB_Type element (JB_job_number may be 0 if
*                            not yet know (at submit time)
*     lList **alpp   - the answer list
*
*  RESULT
*     int - returns != 0 if there is a problem with predecessors
******************************************************************************/
int job_verify_predecessors_ad(lListElem *job, lList **alpp, u_long64 gdi_session) {
   u_long32 jobid = lGetUlong(job, JB_job_number);
   const lList *predecessors_req = nullptr;
   lList *predecessors_id = nullptr;
   const lListElem *pre;
   lListElem *pre_temp;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   predecessors_req = lGetList(job, JB_ja_ad_request_list);
   predecessors_id = lCreateList("job_predecessors_ad", JRE_Type);
   if (!predecessors_id) {
      ERROR(SFNMAX, MSG_JOB_MOD_JOBDEPENDENCY_MEMORY);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* only verify -hold_jid_ad option if predecessors are requested */
   if (lGetNumberOfElem(predecessors_req) > 0) {
      /* verify -t option was used to create this job */
      if (!job_is_array(job)) {
         lFreeList(&predecessors_id);
         DPRINTF("could not create array dependence for non-array job\n");
         ERROR(SFNMAX, MSG_JOB_MOD_CANONLYSPECIFYHOLDJIDADWITHADOPT);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   for_each_ep(pre, predecessors_req) {
      const char *pre_ident = lGetString(pre, JRE_job_name);
      if (isdigit(pre_ident[0])) {
         if (strchr(pre_ident, '.')) {
            lFreeList(&predecessors_id);
            DPRINTF("a job cannot wait for a task to finish\n");
            ERROR(MSG_JOB_MOD_UNKNOWNJOBTOWAITFOR_S, pre_ident);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         if (SGE_STRTOU_LONG32(pre_ident) == jobid) {
            lFreeList(&predecessors_id);
            DPRINTF("got my own jobid in JRE_job_name\n");
            ERROR(MSG_JOB_MOD_GOTOWNJOBIDINHOLDJIDOPTION_U, jobid);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         pre_temp = lCreateElem(JRE_Type);
         if (pre_temp) {
            lSetUlong(pre_temp, JRE_job_number, atoi(pre_ident));
            lAppendElem(predecessors_id, pre_temp);
         }
      } else {
         const lListElem *user_job = nullptr;         /* JB_Type */
         const lListElem *next_user_job = nullptr;    /* JB_Type */
         const void *user_iterator = nullptr;
         const char *owner = lGetString(job, JB_owner);

         next_user_job = lGetElemStrFirst(master_job_list, JB_owner, owner, &user_iterator);

         while ((user_job = next_user_job)) {
            const char *job_name = lGetString(user_job, JB_job_name);
            int result = string_base_cmp(TYPE_RESTR, pre_ident, job_name);
            if (!result) {
               if (lGetUlong(user_job, JB_job_number) != jobid) {
                  pre_temp = lCreateElem(JRE_Type);
                  if (pre_temp) {
                     lSetUlong(pre_temp, JRE_job_number, lGetUlong(user_job, JB_job_number));
                     lAppendElem(predecessors_id, pre_temp);
                  }
               }
            }

            next_user_job = lGetElemStrNext(master_job_list, JB_owner, owner, &user_iterator);
         }

         /* if no matching job has been found we have to assume 
            the job finished already */
      }
   }

   /* to prevent iterating over task ids when no predecessors are matched */
   if (lGetNumberOfElem(predecessors_id) == 0) {
      lFreeList(&predecessors_id);
      lSetList(job, JB_ja_ad_predecessor_list, predecessors_id);
      /* flush task dependency state for empty predecessors list */
      sge_task_depend_flush(job, alpp, gdi_session);
      DRETURN(0);
   }

   /* verify the predecessor list before we try to calculate dependency info */
   for_each_ep(pre, predecessors_id) {
      /* locate the job id in the master list, if not found we can't do much here */
      const lListElem *pred_job = lGetElemUlong(master_job_list, JB_job_number, lGetUlong(pre, JRE_job_number));
      if (!pred_job) continue;
      /* verify this job is an array job */
      if (!job_is_array(pred_job)) {
         lFreeList(&predecessors_id);
         DPRINTF("could not create array dependence on non-array job\n");
         ERROR(SFNMAX, MSG_JOB_MOD_CANONLYSPECIFYHOLDJIDADWITHADOPT);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      /* verify this job has the same range of dependent sub-tasks */
      if (!sge_task_depend_is_same_range(pred_job, job)) {
         lFreeList(&predecessors_id);
         DPRINTF("could not create array dependence for jobs with different sub-task range\n");
         ERROR(SFNMAX, MSG_JOB_MOD_ARRAYJOBMUSTHAVESAMERANGEWITHADOPT);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* this obviously needs to be done before we call the update function */
   lSetList(job, JB_ja_ad_predecessor_list, predecessors_id);

   /* recalculate dependence information for each task of this job */
   sge_task_depend_init(job, alpp, gdi_session);

   DRETURN(0);
}

u_long32
sge_get_job_number(monitoring_t *monitor) {
   u_long32 job_nr;
   bool is_store_job = false;

   DENTER(TOP_LAYER);

   sge_mutex_lock("job_number_mutex", "sge_get_job_number", __LINE__, &job_number_control.job_number_mutex);

   if (job_number_control.job_number >= MAX_SEQNUM) {
      DPRINTF("highest job number MAX_SEQNUM " sge_u32 " reached, starting over with 1\n", MAX_SEQNUM);
      job_number_control.job_number = 0;
      is_store_job = true;
      /*
       * We need to sleep at least for one second to make sure
       * that _this_ job's submission time is unique given the
       * current time resolution of one second. This will allow
       * scheduler proper sorting in case of job ID rollover.
       */
      sge_usleep(1000000);
   }
   job_number_control.job_number++;
   job_number_control.changed = true;
   job_nr = job_number_control.job_number;

   sge_mutex_unlock("job_number_mutex", "sge_get_job_number", __LINE__, &job_number_control.job_number_mutex);

   if (is_store_job) {
      sge_store_job_number(nullptr, monitor);
   }

   DRETURN(job_nr);
}

void sge_init_job_number() {
   FILE *fp = nullptr;
   u_long32 job_nr = 0;
   u_long32 guess_job_nr;

   DENTER(TOP_LAYER);

   if ((fp = fopen(SEQ_NUM_FILE, "r"))) {
      if (fscanf(fp, sge_u32, &job_nr) != 1) {
         ERROR(MSG_NOSEQNRREAD_SSS, SGE_OBJ_JOB, SEQ_NUM_FILE, strerror(errno));
      }
      FCLOSE(fp);
      FCLOSE_ERROR:
      fp = nullptr;
   } else {
      WARNING(MSG_NOSEQFILEOPEN_SSS, SGE_OBJ_JOB, SEQ_NUM_FILE, strerror(errno));
   }

   guess_job_nr = guess_highest_job_number();
   job_nr = MAX(job_nr, guess_job_nr);

   sge_mutex_lock("job_number_mutex", "sge_init_job_number", __LINE__,
                  &job_number_control.job_number_mutex);
   job_number_control.job_number = job_nr;
   job_number_control.changed = true;
   sge_mutex_unlock("job_number_mutex", "sge_init_job_number", __LINE__,
                    &job_number_control.job_number_mutex);

   DRETURN_VOID;
}

void sge_store_job_number(te_event_t anEvent, monitoring_t *monitor) {
   u_long32 job_nr = 0;
   bool changed = false;

   DENTER(TOP_LAYER);

   sge_mutex_lock("job_number_mutex", "sge_store_job_number", __LINE__,
                  &job_number_control.job_number_mutex);
   if (job_number_control.changed) {
      job_nr = job_number_control.job_number;
      job_number_control.changed = false;
      changed = true;
   }
   sge_mutex_unlock("job_number_mutex", "sge_store_job_number", __LINE__,
                    &job_number_control.job_number_mutex);

   /* here we got a race condition that can (very unlikely)
      cause concurrent writing of the sequence number file  */
   if (changed) {
      FILE *fp = fopen(SEQ_NUM_FILE, "w");

      if (fp == nullptr) {
         ERROR(MSG_NOSEQFILECREATE_SSS, SGE_OBJ_JOB, SEQ_NUM_FILE, strerror(errno));
      } else {
         FPRINTF((fp, sge_u32"\n", job_nr));
         FCLOSE(fp);
      }
   }
   DRETURN_VOID;

   FPRINTF_ERROR:
   FCLOSE_ERROR:
   ERROR(MSG_NOSEQFILECLOSE_SSS, SGE_OBJ_JOB, SEQ_NUM_FILE, strerror(errno));
   DRETURN_VOID;
}

static u_long32 guess_highest_job_number() {
   const lListElem *jep;
   u_long32 maxid = 0;
   int pos;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   /* this function is called during qmaster startup and not while it is running,
      we do not need to monitor this lock */

   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   jep = lFirst(master_job_list);
   if (jep) {
      pos = lGetPosViaElem(jep, JB_job_number, SGE_NO_ABORT);

      for_each_ep(jep, master_job_list) {
         maxid = MAX(maxid, lGetPosUlong(jep, pos));
      }
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   DRETURN(maxid);
}

/* all modifications are done now verify schedulability */
int verify_suitable_queues(lList **alpp, lListElem *jep, int *trigger, bool is_modify) {
   int verify_mode = lGetUlong(jep, JB_verify_suitable_queues);
   bool verify_only = (verify_mode == JUST_VERIFY || verify_mode == POKE_VERIFY) ? true : false;
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   lList *master_ar_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_AR);
   lList *master_ehost_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);
   lList *master_rqs_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_RQS);

   DENTER(TOP_LAYER);

   if (verify_mode == SKIP_VERIFY) {
      DPRINTF("skip expensive verification of schedulability\n");
      DRETURN(0);
   }

   if (verify_only) {
      answer_list_remove_quality(*alpp, ANSWER_QUALITY_INFO);
   }

   /* can happen only from qalter -w ... */
   if (is_modify && job_count_pending_tasks(jep, false) == 0) {
      /* since we can rule out a finished jobs it can be running only */
      answer_list_add(alpp, MSG_JOB_VERIFYRUNNING, STATUS_OK, ANSWER_QUALITY_INFO);
      DRETURN(0);
   }

   {
      const lListElem *cqueue;
      lList *talp = nullptr;
      int try_it = 1;
      const char *ckpt_name;
      const lList *job_hard_queue_list = job_get_hard_queue_list(jep);
      const char *pe_name = lGetString(jep, JB_pe);

      sge_assignment_t a = SGE_ASSIGNMENT_INIT;

      assignment_init(&a, jep, nullptr, nullptr);
      assignment_init_ar(&a, master_ar_list);

      DPRINTF("verify schedulability = %c\n", OPTION_VERIFY_STR[verify_mode]);

      /* checkpointing */
      if ((ckpt_name = lGetString(jep, JB_checkpoint_name)))
         if (!(a.ckpt = ckpt_list_locate(master_ckpt_list, ckpt_name)))
            try_it = 0;

      /* parallel */
      if (try_it) {
         const lList *ar_granted_slots = nullptr;

         if (a.ar_id != 0) {
            if (a.ar != nullptr) {
               ar_granted_slots = lGetList(a.ar, AR_granted_slots);
            }
         }

         a.host_list = master_ehost_list;
         a.centry_list = master_centry_list;
         a.acl_list = master_userset_list;
         a.hgrp_list = master_hgroup_list;
         if (lGetUlong(jep, JB_ar) == 0) {
            a.rqs_list = master_rqs_list;
         } else {
            a.ar_list = master_ar_list;
         }
         a.gep = host_list_locate(master_ehost_list, SGE_GLOBAL_NAME);
         a.start = DISPATCH_TIME_NOW;
         /* in reservation scheduling mode a non-zero duration always must be defined */
         job_get_duration(&a.duration, jep);
         a.is_job_verify = true;

         if (verify_only) {
            a.monitor_alpp = &talp;
         }

         /* we will assume this time as start time for now assignments */
         a.now = sge_get_gmt64();

         /*
          * Current scheduler code expects all queue instances in a plain list. We use
          * a copy of all queue instances that needs to be free'd explicitely after
          * deciding about assignment. This is because assignment_release() sees
          * queue_list only as a list pointer.
          */
         a.queue_list = lCreateList("", QU_Type);

         /* imagine qs is empty */
         if (verify_mode != POKE_VERIFY) {
            sconf_set_qs_state(QS_STATE_EMPTY);
         }

         for_each_ep(cqueue, master_cqueue_list) {
            const char *cqname = lGetString(cqueue, CQ_name);

            /* we sort out explicit -q -l requests on queues */
            if (cqueue_match_static(cqname, &a) != DISPATCH_OK) {
               continue;
            }

            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
            const lListElem *qinstance;
            for_each_ep(qinstance, qinstance_list) {
               /*
                * When we are in POKE_VERIFY mode (qalter -w p) we may work on
                * a reduced set of queues (the actual status of the cluster).
                * Filter the queues that cannot be used, i.e (ALARM/SUS/DIS/UNK/ERR)
                *
                * Otherwise (qsub -w e|w|v) we assume an empty cluster with all
                * resources available.
                */
               if (verify_mode == POKE_VERIFY) {
                  u_long32 qi_state = lGetUlong(qinstance, QU_state);
                  if ((qi_state & QI_CAL_SUSPENDED) ||
                      (qi_state & QI_CAL_DISABLED) ||
                      (qi_state & QI_SUSPENDED) ||
                      (qi_state & QI_SUSPENDED_ON_SUBORDINATE) ||
                      (qi_state & QI_ERROR) ||
                      (qi_state & QI_UNKNOWN) ||
                      (qi_state & QI_DISABLED) ||
                      (qi_state & QI_AMBIGUOUS)
                          ) {
                     schedd_mes_add(a.monitor_alpp, a.monitor_next_run, a.job_id,
                                    SCHEDD_INFO_QUEUENOTAVAIL_, lGetString(qinstance, QU_full_name));
                     continue;
                  }
               }

               /* we only have to consider requested queues or hosts*/
               if (job_hard_queue_list != nullptr) {
                  if (qref_list_cq_rejected(job_hard_queue_list, cqname,
                                            lGetHost(qinstance, QU_qhostname), a.hgrp_list)) {
                     schedd_mes_add(a.monitor_alpp, a.monitor_next_run, a.job_id, SCHEDD_INFO_NOTINHARDQUEUELST_S,
                                    lGetString(qinstance, QU_full_name));
                     continue;
                  }
               }

               if (ar_granted_slots != nullptr) {
                  if (lGetElemStr(ar_granted_slots, JG_qname, lGetString(qinstance, QU_full_name)) == nullptr) {
                     continue;
                  }
               }

               /* we only have to consider queues containing the requested pe */
               if (pe_name != nullptr) {
                  bool found = false;
                  const lListElem *pe_ref;

                  for_each_ep(pe_ref, lGetList(qinstance, QU_pe_list)) {
                     if (pe_name_is_matching(lGetString(pe_ref, ST_name), pe_name)) {
                        found = true;
                        break;
                     }
                  }
                  if (!found) {
                     continue;
                  }

               }

               /* add this QIs to the probable list of QIs for the job */
               lAppendElem(a.queue_list, lCopyElem(qinstance));
            }
         }

         // in case of running in an AR, we swap resources to the AR state
         sge_ar_swap_resource_lists(a);

         /* check if this job can be actually scheduled */
         if (pe_name) {
            sge_select_parallel_environment(&a, master_pe_list);
         } else {
            sge_sequential_assignment(&a);
         }

         // in case of running in an AR, we swap resources back to the normal state
         sge_ar_swap_resource_lists(a);

         /* stop dreaming */
         if (verify_mode != POKE_VERIFY) {
            sconf_set_qs_state(QS_STATE_FULL);
         }

         lFreeList(&(a.queue_list));
      }


      /* consequences */
      if (!try_it || !a.slots) {
         /* copy error msgs from talp into alpp */
         if (verify_only) {
            if (!*alpp) {
               *alpp = lCreateList("answer", AN_Type);
            }
            lAddList(*alpp, &talp);
         } else {
            lFreeList(&talp);
         }
         if (verify_only) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NOSUITABLEQ_S, MSG_JOB_VERIFYVERIFY);
         } else if (verify_mode == ERROR_VERIFY) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NOSUITABLEQ_S, MSG_JOB_VERIFYERROR);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NOSUITABLEQ_S, MSG_JOB_VERIFYWARN);
         }

         if (verify_mode == WARNING_VERIFY) {
            answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         }

         if (verify_mode == ERROR_VERIFY) {
            answer_list_remove_quality(*alpp, ANSWER_QUALITY_INFO);
         }

         if (verify_mode != WARNING_VERIFY) {
            assignment_release(&a);
            DRETURN(STATUS_ESEMANTIC);
         }
      }
      lFreeList(&talp);
      a.monitor_alpp = nullptr;

      if (verify_only) {
         if (trigger) {
            *trigger |= VERIFY_EVENT;
         }
         if (!a.pe) {
            answer_list_add(alpp, MSG_JOB_VERIFYFOUNDQ, STATUS_OK, ANSWER_QUALITY_INFO);
         } else {
            answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_JOB_VERIFYFOUNDSLOTS_I, a.slots);
         }
         assignment_release(&a);
         DRETURN(STATUS_ESEMANTIC);
      }
      assignment_release(&a);
   }

   DRETURN(0);
}

int sge_gdi_copy_job(lListElem *jep, lList **alpp, lList **lpp,
                     ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   u_long32 seek_jid;
   int ret;
   const lListElem *old_jep;
   lListElem *new_jep;
   int dummy_trigger = 0;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   if (!jep) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* seek job */
   seek_jid = lGetUlong(jep, JB_job_number);
   DPRINTF("SEEK jobid " sge_u32 " for COPY operation\n", seek_jid);

   if (!(old_jep = lGetElemUlong(master_job_list, JB_job_number, seek_jid))) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SU, SGE_OBJ_JOB, seek_jid);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ensure copy is allowed */
   if (strcmp(packet->user, lGetString(old_jep, JB_owner)) && !manop_is_manager(packet, master_manager_list)) {
      ERROR(MSG_JOB_NORESUBPERMS_SSS, packet->user, packet->host, lGetString(old_jep, JB_owner));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   new_jep = lCopyElem(old_jep);

   /* read script from old job and reuse it */
   if (lGetString(new_jep, JB_exec_file) && !JOB_TYPE_IS_BINARY(lGetUlong(new_jep, JB_type))) {
      if (!spool_read_script(alpp, seek_jid, new_jep)) {
         lFreeElem(&new_jep);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   job_initialize_id_lists(new_jep, nullptr);

   /* override settings of old job with new settings of jep */
   if (mod_job_attributes(packet, new_jep, jep, alpp, &dummy_trigger)) {
      lFreeElem(&new_jep);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* reset the job number. we will get a new one when we add the job */
   lSetUlong(new_jep, JB_job_number, 0);

   /* call add() method */
   ret = sge_gdi_add_job(&new_jep, alpp, lpp, packet, task, monitor);

   lFreeElem(&new_jep);

   DRETURN(ret);
}

/****** sge_job_qmaster/spool_write_script() ***********************************
*  NAME
*     spool_write_script() -- Write job script
*
*  SYNOPSIS
*     bool spool_write_script(lList **answer_list,u_long32 jobid, lListElem *jep) 
*
*  FUNCTION
*     The function stores the script of a '-b n' job into a file.
*
*  INPUTS
*     lList **answer_list
*     u_long32 jobid - job id (needed for Dtrace only)
*     lListElem *jep - the job
*
*  RESULT
*     static bool - true on success
*
*  NOTES
*     MT-NOTE: spool_write_script() is MT safe 
*
*  SEE ALSO
*     spool_delete_script()
*     spool_read_script()
*******************************************************************************/
bool spool_write_script(lList **answer_list, u_long32 jobid, const lListElem *jep) {
   bool ret = true;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   /* The whole job object is needed for spooling classic */
   ret = spool_write_object(answer_list, spool_get_default_context(),
                            jep, jobscript_get_key(jep, &buffer),
                            SGE_TYPE_JOBSCRIPT, true);
   PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   sge_dstring_free(&buffer);

   DRETURN(ret);
}

/****** sge_job_qmaster/spool_read_script() **************************************
*  NAME
*     spool_read_script() -- Read job script
*
*  SYNOPSIS
*     bool spool_read_script(lList **answer_list, u_long32 jobid, lListElem *jep) 
*
*  FUNCTION
*     The function reads the script of a '-b n' job from file.
*
*  INPUTS
*     lList **answer_list
*     u_long32 jobid - job id (needed for Dtrace only)
*     lListElem *jep - the job
*
*  RESULT
*     bool - true on success
*
*  NOTES
*     MT-NOTE: spool_read_script() is MT safe 
*
*  SEE ALSO
*     spool_write_script()
*     spool_delete_script()
*******************************************************************************/
bool spool_read_script(lList **answer_list, u_long32 jobid, lListElem *jep) {
   bool ret = true;
   dstring buffer = DSTRING_INIT;
   lListElem *script_el = nullptr;
   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   script_el = spool_read_object(answer_list, spool_get_default_context(),
                                 SGE_TYPE_JOBSCRIPT, jobscript_get_key(jep, &buffer));
   PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   /* The spooled out structure must be restored */
   if (script_el != nullptr) {
      char *script = (char *) lGetString(script_el, STU_name);
      char *dummy = nullptr;
      int len = strlen(script);
      lXchgString(jep, JB_script_ptr, &script);
      lXchgString(script_el, STU_name, &dummy);
      lFreeElem(&script_el);
      lSetUlong(jep, JB_script_size, len);
   } else {
      ret = false;
   }
   sge_dstring_free(&buffer);
   DRETURN(ret);
}

/****** sge_job_qmaster/spool_delete_script() ************************************
*  NAME
*     spool_delete_script() -- Delete job script
*
*  SYNOPSIS
*     bool spool_delete_script(lList **answer_list, u_long32 jobid, lListElem *jep) 
*
*  FUNCTION
*     The function removes the file where the script of a '-b n' job is stored.
*
*  INPUTS
*     lList **answer_list
*     u_long32 jobid - job id (needed for Dtrace only)
*     lListElem *jep - the job
*
*  RESULT
*     bool - true on success
*
*  NOTES
*     MT-NOTE: spool_delete_script() is MT safe 
*
*  SEE ALSO
*     spool_write_script()
*     spool_read_script()
*******************************************************************************/
bool spool_delete_script(lList **answer_list, u_long32 jobid, lListElem *jep) {
   bool ret = true;
   dstring buffer = DSTRING_INIT;
   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   ret = spool_delete_object(answer_list, spool_get_default_context(),
                             SGE_TYPE_JOBSCRIPT, jobscript_get_key(jep, &buffer), true);
   PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   sge_dstring_free(&buffer);
   lSetString(jep, JB_exec_file, nullptr);
   lSetString(jep, JB_script_file, nullptr);
   lSetUlong(jep, JB_script_size, 0);
   DRETURN(ret);
}

static int sge_delete_all_tasks_of_job(const ocs::gdi::Packet *packet, lList **alpp,
                                       lListElem *job, u_long32 *r_start, u_long32 *r_end, u_long32 *step,
                                       const lList *ja_structure, int *alltasks, u_long32 *deleted_tasks,
                                       u_long64 start_time, monitoring_t *monitor, int forced,
                                       bool *deletion_time_reached) {
   int njobs = 0;
   const lListElem *rn;
   char *dupped_session = nullptr;
   int deleted_unenrolled_tasks;
   u_long32 task_number = 0;
   u_long32 existing_tasks;
   lList *range_list = nullptr;        /* RN_Type */
   u_long32 job_number = lGetUlong(job, JB_job_number);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   u_long64 max_job_deletion_time = sge_gmt32_to_gmt64(mconf_get_max_job_deletion_time());
   bool get_enable_forced_qdel_if_unknown = mconf_get_enable_forced_qdel_if_unknown();
   bool simulate_execds = mconf_get_simulate_execds();

   DENTER(TOP_LAYER);

   /* In certain cases sge_commit_job() free's the job structure passed.
    * The session information is needed after sge_commit_job() so we make 
    * a copy of the job session before calling sge_commit_job(). This copy
    * must be free'd!
    */
   if (lGetString(job, JB_session)) {
      dupped_session = strdup(lGetString(job, JB_session));
   }

   /*
    * Repeat until all requested taskid ranges are handled
    */
   rn = lFirst(ja_structure);
   do {
      int showmessage = 0;
      u_long32 enrolled_start = 0;
      u_long32 enrolled_end = 0;
      u_long32 unenrolled_start = 0;
      u_long32 unenrolled_end = 0;

      /*
       * delete tasks or the whole job?
       * if ja_structure not empty delete specified tasks
       * otherwise delete whole job
       */
      unenrolled_start = job_get_smallest_unenrolled_task_id(job);
      unenrolled_end = job_get_biggest_unenrolled_task_id(job);
      enrolled_start = job_get_smallest_enrolled_task_id(job);
      enrolled_end = job_get_biggest_enrolled_task_id(job);

      if (rn) {
         *r_start = lGetUlong(rn, RN_min);
         *r_end = lGetUlong(rn, RN_max);

         *step = lGetUlong(rn, RN_step);
         if (!(*step)) {
            *step = 1;
         }

         if (*r_start > unenrolled_start) {
            unenrolled_start = (*r_start);
         } else {
            u_long32 temp_start;

            /* we have to figure out the first task we can delete and we do want     */
            /* to start with the first existing task. For that, we compute:          */
            /*                                                                       */
            /* - the delta between the requested task id and the first existing one  */
            /* - we devide the delta by the step size, to get the number of steps we */
            /*   need ot get there.                                                  */
            /* - the number of steps multiplied by the step size + the start value   */
            /*   will get us the first task, or a very close. If we just right befor */
            /*   it, we add another step to get there.                               */
            temp_start = ((unenrolled_start - (*r_start)) / (*step)) * (*step) + (*r_start);

            if (temp_start < unenrolled_start) {
               unenrolled_start = temp_start + (*step);
            } else {
               unenrolled_start = temp_start;
            }
         }

         unenrolled_end = MIN(*r_end, unenrolled_end);


         if ((*r_start) > enrolled_start) {
            enrolled_start = *r_start;
         } else {
            u_long32 temp_start;

            temp_start = ((enrolled_start - *r_start) / (*step)) * (*step) + (*r_start);

            if (temp_start < enrolled_start) {
               enrolled_start = temp_start + (*step);
            } else {
               enrolled_start = temp_start;
            }
         }

         enrolled_end = MIN(*r_end, enrolled_end);

         *alltasks = 0;
      } else {
         *step = 1;
         *alltasks = 1;
      }

      DPRINTF("Request: alltasks = %d, start = %d, end = %d, step = %d\n",
              *alltasks, *r_start, *r_end, *step);
      DPRINTF("unenrolled ----> start = %d, end = %d, step = %d\n",
              unenrolled_start, unenrolled_end, *step);
      DPRINTF("enrolled   ----> start = %d, end = %d, step = %d\n",
              enrolled_start, enrolled_end, *step);

      showmessage = 0;

      /*
       * Delete all unenrolled pending tasks
       */
      deleted_unenrolled_tasks = 0;
      *deleted_tasks = 0;
      existing_tasks = job_get_ja_tasks(job);

      if (*alltasks == 1 && existing_tasks == 0) {
         /*
          * This job has no pending and no running tasks.
          */
         lListElem *tmp_task = job_get_ja_task_template_pending(job, task_number);

         sge_commit_job(job, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED,
                        COMMIT_UNENROLLED_TASK | COMMIT_NEVER_RAN, monitor, packet->gdi_session);

         INFO(MSG_JOB_DELETEX_SSU, packet->user, SGE_OBJ_JOB, job_number);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         njobs++;
         continue;
      }

      for (task_number = unenrolled_start;
           task_number <= unenrolled_end;
           task_number += *step) {
         bool is_defined;

         is_defined = job_is_ja_task_defined(job, task_number);

         if (is_defined && !job_is_enrolled(job, task_number)) {
            lListElem *tmp_task = job_get_ja_task_template_pending(job, task_number);

            (*deleted_tasks)++;

            ocs::ReportingFileWriter::create_job_logs(nullptr, sge_get_gmt64(), JL_DELETED,
                                     packet->user, packet->host, nullptr, job, tmp_task,
                                     nullptr, MSG_LOG_DELETED);
            sge_commit_job(job, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED,
                           COMMIT_NO_SPOOLING | COMMIT_UNENROLLED_TASK | COMMIT_NEVER_RAN, monitor, packet->gdi_session);
            deleted_unenrolled_tasks = 1;
            showmessage = 1;
            if (!*alltasks && showmessage) {
               range_list_insert_id(&range_list, nullptr, task_number);
            }
         }
      }

      if (deleted_unenrolled_tasks) {
         if (existing_tasks > *deleted_tasks) {
            dstring buffer = DSTRING_INIT;
            /* write only the common part - pass only the jobid, no jatask or petask id */
            lList *answer_list = nullptr;
            spool_write_object(&answer_list, spool_get_default_context(),
                               job, job_get_job_key(job_number, &buffer),
                               SGE_TYPE_JOB, true);
            answer_list_output(&answer_list);
            sge_dstring_free(&buffer);
         }
      }

      /*
       * Delete enrolled ja tasks
       */
      if (existing_tasks > *deleted_tasks) {
         for (task_number = enrolled_start;
              task_number <= enrolled_end;
              task_number += *step) {
            int spool_job = 1;
            int is_defined = job_is_ja_task_defined(job, task_number);

            if (is_defined) {
               lListElem *tmp_task = lGetElemUlongRW(lGetList(job, JB_ja_tasks), JAT_task_number, task_number);

               if (tmp_task == nullptr) {
                  /* ja task does not exist anymore - ignore silently */
                  continue;
               }

               njobs++;

               /*
                * if ENABLE_FORCED_QDEL_IF_UNKNOWN is enabled and a user send a qdel (not forced)
                * then we will handle it as a forced request if the job (master-task) is running
                * on a host in unknown state.
                */
               if (get_enable_forced_qdel_if_unknown) {
                  const lList *gdil = lGetList(tmp_task, JAT_granted_destin_identifier_list);
                  const lListElem *gdil_ep = lFirst(gdil);

                  if (gdil_ep != nullptr) {
                     lListElem *qinstance = cqueue_list_locate_qinstance(master_cqueue_list,
                                                                         lGetString(gdil_ep, JG_qname));

                     if (qinstance != nullptr) {
                        /*
                         * is the queue (=host) in unknown state, then make the qdel request a forced request
                         * otherwise check if we have a tight pe job. in that case a pe slave tasks will be tagged
                         * where the corresponding execution node can't be contacted so that a regular
                         * qdel will delete the job (code part of the qmaster<->execd protocol)
                         */
                        if (qinstance_state_is_unknown(qinstance)) {
                           INFO(MSG_JOB_CHGFORCED_UU, job_number, task_number);
                           forced = true;
                        } else {
                           if (job_is_tight_parallel(job, master_pe_list)) {
                              const lList *pe_task_list = lGetList(tmp_task, JAT_task_list);
                              lListElem *pe_task;

                              for_each_rw(pe_task, pe_task_list) {
                                 gdil = lGetList(pe_task, PET_granted_destin_identifier_list);
                                 gdil_ep = lFirst(gdil);

                                 if (gdil_ep != nullptr) {
                                    qinstance = cqueue_list_locate_qinstance(master_cqueue_list,
                                                                             lGetString(gdil_ep, JG_qname));

                                    if (qinstance != nullptr && qinstance_state_is_unknown(qinstance)) {
                                       lSetBool(pe_task, PET_do_contact, false);
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }

               /* 
                * if task is already in status deleted and was signaled
                * only recently and deletion is not forced, do nothing
                */
               if ((lGetUlong(tmp_task, JAT_status) & JFINISHED) ||
                   (simulate_execds && lGetUlong(tmp_task, JAT_state) & JDELETED) ||
                   (lGetUlong(tmp_task, JAT_state) & JDELETED &&
                    lGetUlong64(tmp_task, JAT_pending_signal_delivery_time) > sge_get_gmt64() &&
                    !forced)) {
                  INFO(MSG_JOB_ALREADYDELETED_U, job_number);
                  answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                  continue;
               }

               /* If job has large array of tasks, and time to delete the array
                * of jobs is greater than MAX_JOB_DELETION_TIME, break out of
                * qdel and delete remaining jobs later
                */
               if ((njobs > 0 || (*deleted_tasks) > 0) && ((sge_get_gmt64() - start_time) > max_job_deletion_time)) {
                  INFO(MSG_JOB_DISCONTTASKTRANS_SUU, packet->user, job_number, task_number);
                  answer_list_add(alpp, SGE_EVENT, STATUS_OK_DOAGAIN, ANSWER_QUALITY_INFO);
                  *deletion_time_reached = true;
                  sge_free(&dupped_session);
                  lFreeList(&range_list);
                  DRETURN(njobs);
               }

               ocs::ReportingFileWriter::create_job_logs(nullptr, sge_get_gmt64(), JL_DELETED,
                                                         packet->user, packet->host, nullptr, job,
                                                         tmp_task, nullptr, MSG_LOG_DELETED);

               if (lGetString(tmp_task, JAT_master_queue) && is_pe_master_task_send(tmp_task)) {
                  job_ja_task_send_abort_mail(job, tmp_task, packet->user, packet->host, nullptr);
                  get_rid_of_job_due_to_qdel(job, tmp_task, alpp, packet->user, forced, monitor, packet->gdi_session);
               } else {
                  // @todo: spool_job = 1 = COMMIT_NO_SPOOLING = 0x0001
                  sge_commit_job(job, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED_EE, spool_job | COMMIT_NEVER_RAN,
                                 monitor, packet->gdi_session);
                  showmessage = 1;
                  if (!*alltasks && showmessage) {
                     range_list_insert_id(&range_list, nullptr, task_number);
                  }
               }
            } else { ; /* Task did never exist! - Ignore silently */
            }
         }
      }

      if (range_list && showmessage) {
         if (range_list_get_number_of_ids(range_list) > 1) {
            dstring tid_string = DSTRING_INIT;

            range_list_sort_uniq_compress(range_list, nullptr, true);
            range_list_print_to_string(range_list, &tid_string, false, false, false);
            INFO(MSG_JOB_DELETETASKS_SSU, packet->user, sge_dstring_get_string(&tid_string), job_number);
            sge_dstring_free(&tid_string);
         } else {
            u_long32 task_id = range_list_get_first_id(range_list, nullptr);

            INFO(MSG_JOB_DELETETASK_SUU, packet->user, job_number, task_id);
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }

      if ((*alltasks) && showmessage) {
         get_rid_of_schedd_job_messages(job_number);
         INFO(MSG_JOB_DELETEX_SSU, packet->user, SGE_OBJ_JOB, job_number);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }

      if ((njobs > 0 || (*deleted_tasks) > 0) && ((sge_get_gmt64() - start_time) > max_job_deletion_time)) {
         INFO(MSG_JOB_DISCONTINUEDTRANS_SU, packet->user, job_number);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK_DOAGAIN, ANSWER_QUALITY_INFO);
         *deletion_time_reached = true;
         sge_free(&dupped_session);
         lFreeList(&range_list);
         DRETURN(njobs);
      }

      rn = lNext(rn);
   } while (rn != nullptr);

   /* free task id range list of this iteration */
   lFreeList(&range_list);
   sge_free(&dupped_session);

   DRETURN(njobs);
}

/****** sge_job_qmaster/job_verify_project() ***********************************
*  NAME
*     job_verify_project() -- verify the JB_project of a job
*
*  SYNOPSIS
*     static int 
*     job_verify_project(const lListElem *job, lList **alpp,
*                        const char *user, const char *group) 
*
*  FUNCTION
*     Does verifications on the JB_project field of a job.
*     Is called when qmaster gets a new job (sge_gdi_add_job),
*     or when a job is modified (sge_gdi_mod_job).
*     Verifies the project:
*        - global config projects/xprojects
*        - enforce_project
*        - exists in project list
*
*  INPUTS
*     const lListElem *job - the job containing JB_project
*     lList **alpp         - answer list to pass back error messages
*     const char *user     - job owner / job submitter
*     const char *group    - job owners group
*
*  RESULT
*     static int - STATUS_OK on success, else STATUS_* error code
*
*  NOTES
*     MT-NOTE: job_verify_project() is MT safe, if caller holds the global lock.
*******************************************************************************/
int
job_verify_project(const lListElem *job, lList **alpp,
                   const char *user, const char *group, const lList *grp_list) {
   int ret = STATUS_OK;
   const char *project = lGetString(job, JB_project);
   lList *projects = mconf_get_projects();
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

   DENTER(TOP_LAYER);

   /* job requests a project, verify existance, access rights, ... */
   if (project != nullptr) {
      /* make sure the requested project exists at all */
      const lListElem *pep = prj_list_locate(master_project_list, project);
      if (pep == nullptr) {
         ERROR(MSG_JOB_PRJUNKNOWN_S, project);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }

      if (ret == STATUS_OK) {
         /* ensure user belongs to this project */
         if (!sge_has_access_(user, group, grp_list, lGetList(pep, PR_acl), lGetList(pep, PR_xacl), master_userset_list)) {
            ERROR(MSG_SGETEXT_NO_ACCESS2PRJ4USER_SS, project, user);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }

      if (ret == STATUS_OK) {
         /* verify project can use SGE cluster according to global config projects/xprojects */
         lList *xprojects = mconf_get_xprojects();

         if ((xprojects != nullptr && prj_list_locate(xprojects, project)) ||
             (projects != nullptr && prj_list_locate(projects, project) == nullptr)) {
            ERROR(MSG_JOB_PRJNOSUBMITPERMS_S, project);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }

         lFreeList(&xprojects);
      }
   } else {
      /* 
       * job does not request a project - verify project enforcement
       * 
       * when we have a projects list in the global config,
       * every job *must* request a project
       */
      if (lGetNumberOfElem(projects) > 0) {
         ERROR(SFNMAX, MSG_JOB_PRJREQUIRED);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }

      if (ret == STATUS_OK) {
         char *enforce_project = mconf_get_enforce_project();

         if (enforce_project != nullptr && strcasecmp(enforce_project, "true") == 0) {
            ERROR(SFNMAX, MSG_SGETEXT_NO_PROJECT);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
         sge_free(&enforce_project);
      }
   }

   lFreeList(&projects);
   DRETURN(ret);
}

/*
* NOTES
*     MT-NOTE: reporting_get_job_log_name() is MT-safe
*/
const char *
get_job_log_name(const job_log_t type) {
   const char *ret;

   switch (type) {
      case JL_UNKNOWN:
         ret = MSG_JOBLOG_ACTION_UNKNOWN;
         break;
      case JL_PENDING:
         ret = MSG_JOBLOG_ACTION_PENDING;
         break;
      case JL_SENT:
         ret = MSG_JOBLOG_ACTION_SENT;
         break;
      case JL_RESENT:
         ret = MSG_JOBLOG_ACTION_RESENT;
         break;
      case JL_DELIVERED:
         ret = MSG_JOBLOG_ACTION_DELIVERED;
         break;
      case JL_RUNNING:
         ret = MSG_JOBLOG_ACTION_RUNNING;
         break;
      case JL_SUSPENDED:
         ret = MSG_JOBLOG_ACTION_SUSPENDED;
         break;
      case JL_UNSUSPENDED:
         ret = MSG_JOBLOG_ACTION_UNSUSPENDED;
         break;
      case JL_HELD:
         ret = MSG_JOBLOG_ACTION_HELD;
         break;
      case JL_RELEASED:
         ret = MSG_JOBLOG_ACTION_RELEASED;
         break;
      case JL_RESTART:
         ret = MSG_JOBLOG_ACTION_RESTART;
         break;
      case JL_MIGRATE:
         ret = MSG_JOBLOG_ACTION_MIGRATE;
         break;
      case JL_DELETED:
         ret = MSG_JOBLOG_ACTION_DELETED;
         break;
      case JL_FINISHED:
         ret = MSG_JOBLOG_ACTION_FINISHED;
         break;
      case JL_ERROR:
         ret = MSG_JOBLOG_ACTION_ERROR;
         break;
      default:
         ret = "!!!! unknown job state !!!!";
         break;
   }

   return ret;
}

