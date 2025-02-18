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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cerrno>
#include <climits>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_hostname.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_report.h"

#include "gdi/sge_security.h"

#include "spool/classic/read_write_job.h"

#include "execd.h"
#include "reaper_execd.h"
#include "job_report_execd.h"
#include "execd_job_exec.h"
#include "execution_states.h"
#include "get_path.h"
#include "msg_common.h"
#include "msg_execd.h"

extern volatile int jobs_to_start;

static int handle_job(lListElem *jelem, lListElem *jatep, int slave);
static int handle_task(lListElem *petrep, char *commproc, char *host, u_short id, sge_pack_buffer *apb);

int do_job_exec(ocs::gdi::ClientServerBase::struct_msg_t *aMsg, sge_pack_buffer *apb)
{
   int ret = 1;
   u_long32 feature_set;
   const char *admin_user = bootstrap_get_admin_user();
   const char *progname = component_get_component_name();

   DENTER(TOP_LAYER);

   /* ------- featureset */
   if (unpackint(&(aMsg->buf), &feature_set)) {
      ERROR(SFNMAX, MSG_COM_UNPACKFEATURESET);
      DRETURN(0);
   }

   /* if request comes from qmaster: start a job
    * else it is a request to start a pe task
    */
   if (strcmp(aMsg->snd_name, prognames[QMASTER]) == 0) {
      lListElem *job, *ja_task;
      lList *answer_list = nullptr;

      if (!sge_security_verify_unique_identifier(true, admin_user, progname, 0,
                                            aMsg->snd_host, aMsg->snd_name, aMsg->snd_id)) {
         DRETURN(0);
      }
       
      if (!object_unpack_elem_verify(&answer_list, &(aMsg->buf), &job, JB_Type)) {
         answer_list_output(&answer_list);
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      }

      if (!job_verify_execd_job(job, &answer_list, component_get_qualified_hostname())) {
         const char *err_str = lGetString(lFirst(answer_list), AN_text);
         ja_task = lFirstRW(lGetList(job, JB_ja_tasks));

         /* set the job into error state */
         execd_job_start_failure(job, ja_task, nullptr, err_str, GFSTATE_JOB);

         /* error output to messages file, cleanup */
         answer_list_output(&answer_list);
         ERROR(MSG_EXECD_INVALIDJOBREQUEST_SS, aMsg->snd_name, aMsg->snd_host);
         lFreeElem(&job);
         DRETURN(0);
      }

      /* we expect one jatask to start per request */
      ja_task = lFirstRW(lGetList(job, JB_ja_tasks));
      if (ja_task != nullptr) {
         DPRINTF("new job %ld.%ld\n", (long) lGetUlong(job, JB_job_number), (long) lGetUlong(ja_task, JAT_task_number));
         ret = handle_job(job, ja_task, 0);
         if (ret != 0) {
            lFreeElem(&job);
         }
      }
   } else {
      /* start a pe task */ 
      lListElem *petrep;
      lList *answer_list = nullptr;

      if (!object_unpack_elem_verify(&answer_list, &(aMsg->buf), &petrep, PETR_Type)) {
         answer_list_output(&answer_list);
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      }

      if (!pe_task_verify_request(petrep, &answer_list)) {
         answer_list_output(&answer_list);
         ERROR(MSG_EXECD_INVALIDTASKREQUEST_SS, aMsg->snd_name, aMsg->snd_host);
         DRETURN(0);
      }

      DPRINTF("new pe task for job: %ld.%ld\n", (long) lGetUlong(petrep, PETR_jobid), (long) lGetUlong(petrep, PETR_jataskid));

      ret = handle_task(petrep, aMsg->snd_name, aMsg->snd_host, aMsg->snd_id, apb);

      lFreeElem(&petrep);
   }
   
   if (ret == 0) {
      jobs_to_start = 1;
   }

   DRETURN(0);
}

int do_job_slave(ocs::gdi::ClientServerBase::struct_msg_t *aMsg)
{
   int ret = 1;
   lListElem *jelem, *ja_task;
   u_long32 feature_set;
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   /* ------- featureset */
   if (unpackint(&(aMsg->buf), &feature_set)) {
      ERROR(SFNMAX, MSG_COM_UNPACKFEATURESET);
      DRETURN(0);
   }

   /*
   ** the check if the request has admin/root credentials is done by
   ** the dispatcher in authorize_dpe()
   ** so no additional check needed here like in execd_job_exec()
   */

   /* ------- job */
   if (!object_unpack_elem_verify(&answer_list, &(aMsg->buf), &jelem, JB_Type)) {
      answer_list_output(&answer_list);
      ERROR(SFNMAX, MSG_COM_UNPACKJOB);
      DRETURN(0);
   }
   lFreeList(&answer_list);

   for_each_rw(ja_task, lGetList(jelem, JB_ja_tasks)) {
      DPRINTF("Job: %ld Task: %ld\n", (long) lGetUlong(jelem, JB_job_number), (long) lGetUlong(ja_task, JAT_task_number));
      ret = handle_job(jelem, ja_task, 1);
   }

   if (ret)  {
      lFreeElem(&jelem);
   } 

   DRETURN(0);
}

static int handle_job(lListElem *jelem, lListElem *jatep, int slave) {
   lListElem *jep; 
   dstring err_str = DSTRING_INIT;
   u_long32 jobid, jataskid;
   int general = GFSTATE_QUEUE;
   lListElem *qep, *gdil_ep;
   const char *qnm;
   const void *iterator = nullptr;
   bool report_job_error = true;   /* send job report on error? */

   DENTER(TOP_LAYER);

   DPRINTF("got %s job " sge_u32"\n", slave ?"slave ":"", lGetUlong(jelem, JB_job_number));

   jobid = lGetUlong(jelem, JB_job_number);
   jataskid = lGetUlong(jatep, JAT_task_number);

   /* 
    * Sometimes the rescheduled job from qmaster which is 
    * sent synchronuously arrives earlier than the ack 
    * reporting that qmaster got job exit message from run before 
    * 
    * We can ignore this job because job is resend by qmaster.
    */
   jep = lGetElemUlongFirstRW(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), JB_job_number, jobid, &iterator);
   while (jep != nullptr) {
      if (job_search_task(jep, nullptr, jataskid) != nullptr) {
         DPRINTF("Job " sge_u32"." sge_u32" is already running - skip the new one\n", jobid, jataskid);
         goto Ignore;   /* don't set queue in error state */
      }

      jep = lGetElemUlongNextRW(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), JB_job_number, jobid, &iterator);
   }

   /* initialize state - prevent slaves from getting started */
   lSetUlong(jatep, JAT_status, slave?JSLAVE:JIDLE);

   /* now we have a queue and a job filled */
   DPRINTF("===>JOB_EXECUTION: >" sge_u32"." sge_u32"< with " sge_u32" tickets\n", jobid, jataskid, (u_long32)lGetDouble(jatep, JAT_tix));

   /* initialize job */
   for_each_rw (gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
      lList *answer_list = nullptr;
      int slots;

      if (!(qep=lGetObject(gdil_ep, JG_queue))) {
         qnm = lGetString(gdil_ep, JG_qname);
         sge_dstring_sprintf(&err_str, MSG_JOB_MISSINGQINGDIL_SU, qnm, lGetUlong(jelem, JB_job_number));
         goto Error;
      }

      /* 
       * Verify the queue list sent with the job start order.
       * If it is incorrect, we reject the job start.
       * We do not send a job report for this job - this would trigger
       * rescheduling in qmaster ...
       */
      if (!qinstance_verify(qep, &answer_list)) {
         answer_list_output(&answer_list);
         report_job_error = false;
         goto Error;
      }
      lFreeList(&answer_list);

      /* clear any queue state that might be set from qmaster */
      lSetUlong(qep, QU_state, 0);

      /* store number of slots we got in this queue for this job */
      slots = lGetUlong(gdil_ep, JG_slots);
      lSetUlong(qep, QU_job_slots, slots);
      qinstance_set_slots_used(qep, 0);
   }

   /* ------- optionally pe */
   if (lGetString(jatep, JAT_granted_pe)) {
      lListElem *pelem = lGetObject(jatep, JAT_pe_object);

      if (pelem != nullptr && lGetBool(pelem, PE_control_slaves) && !lGetBool(pelem, PE_job_is_first_task)) {
         int slots;
         lListElem *mq = lGetObject(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), 
                                    JG_queue);
         slots = lGetUlong(mq, QU_job_slots) + 1;
         DPRINTF("Increasing job slots in master queue \"%s\" to %d because job is not first task\n",
                 lGetString(mq, QU_qname), slots);
         lSetUlong(mq, QU_job_slots, slots);
      }
   }

   if (sge_make_ja_task_active_dir(jelem, jatep, &err_str) == nullptr) {
      goto Error;
   }

   if (!JOB_TYPE_IS_BINARY(lGetUlong(jelem, JB_type))) {
      /* interactive jobs and slave jobs do not have a script file */
      if (!slave && lGetString(jelem, JB_script_file)) {
         int nwritten;
         u_long32 job_id = lGetUlong(jelem, JB_job_number);

         if (!mconf_get_simulate_jobs()) {
            /*
             * Is another array task of the same job already here?
             * In this case it is not necessary to spool the jobscript.
             *
             * But it is not enough, just to look for another array task. we have
             * check wether there is another master task of the same job running
             * on this host. This is important in case of array pe-jobs.
             */
             if (count_master_tasks(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), job_id) == 0) {
               int fd;

               /* We are root. Make the scriptfile readable for the jobs submitter,
                  so shepherd can open (execute) it after changing to the user. */
               fd = SGE_OPEN3(lGetString(jelem, JB_exec_file), O_CREAT | O_WRONLY, 0755);
               if (fd < 0) {
                  sge_dstring_sprintf(&err_str, MSG_ERRORWRITINGFILE_SS, 
                                      lGetString(jelem, JB_exec_file), 
                                      strerror(errno));
                  goto Error;
               }

               nwritten = sge_writenbytes(fd, lGetString(jelem, JB_script_ptr), lGetUlong(jelem, JB_script_size));
               if (nwritten != (int)lGetUlong(jelem, JB_script_size)) {
                  DPRINTF("errno: %d\n", errno);
                  sge_dstring_sprintf(&err_str, MSG_EXECD_NOWRITESCRIPT_SIUS, 
                                      lGetString(jelem, JB_exec_file), nwritten, 
                                      lGetUlong(jelem, JB_script_size),
                                      strerror(errno));
                  close(fd);
                  goto Error;
               }      
               close(fd);
            }
            lSetString(jelem, JB_script_ptr, nullptr);
         }
      }
   }
      
   /* 
   ** security hook
   **
   ** Execute command to store the client's DCE or Kerberos credentials.
   ** This also creates a forwardable credential for the user.
   */
   if (mconf_get_do_credentials()) {
      const char *sge_root = bootstrap_get_sge_root();
      const char *unqualified_hostname = component_get_unqualified_hostname();

      if (store_sec_cred2(sge_root, unqualified_hostname, jelem, mconf_get_do_authentication(), &general, &err_str) != 0) {
         goto Error;
      }   
   }

#ifdef KERBEROS
   kerb_job(jelem, de);
#endif

   if (!mconf_get_simulate_jobs()) {
      lSetUlong(jelem, JB_script_size, 0);
      if (job_write_spool_file(jelem, jataskid, nullptr, SPOOL_WITHIN_EXECD)) {
         /* SGE_EVENT is written by job_write_spool_file() */
         sge_dstring_copy_string(&err_str, SGE_EVENT);
         goto Error;
      }
   }

   { 
      lListElem *report = add_job_report(jobid, jataskid, nullptr, jelem);

      flush_job_report(report);
   }   

   /* check if job has queue limits and increase global flag if necessary */
   modify_queue_limits_flag_for_job(component_get_qualified_hostname(), jelem, true);

   /* put into job list */
   lAppendElem(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB), jelem);

   DRETURN(0);

Error:
   if (report_job_error) {
      execd_job_start_failure(jelem, jatep, nullptr, sge_dstring_get_string(&err_str), general);
   }

   sge_dstring_free(&err_str);

Ignore:   
   DRETURN(-1);  
}

/****** execd/job/job_set_queue_info_in_task() ********************************
*  NAME
*     job_set_queue_info_in_task() -- set queue to use for task
*
*  SYNOPSIS
*     static lList *job_set_queue_info_in_task(char *qname, lListElem *pe_task);
*
*  FUNCTION
*     Extend the task structure of task <jatask> by a 
*     JAT_granted_destin_identifier list, which contains 
*     the queue <qname> and uses one slot.
*
*  INPUTS
*     qualified_hostname - name of host
*     qname   - name of queue to set
*     pe_task - task structure
*
*  RESULT
*     the new created JAT_granted_destin_identifier list
******************************************************************************/
static lList *job_set_queue_info_in_task(const char *qualified_hostname, const char *qname, lListElem *petep)
{
   lListElem *jge;

   DENTER(TOP_LAYER);

   jge = lAddSubStr(petep, JG_qname, qname, 
                    PET_granted_destin_identifier_list, JG_Type);
   lSetHost(jge, JG_qhostname, qualified_hostname);
   lSetUlong(jge, JG_slots, 1);
   DPRINTF("selected queue %s for task\n", qname);

   DRETURN(lGetListRW(petep, PET_granted_destin_identifier_list));
}

/****** execd/job/job_get_queue_with_task_about_to_exit() *********************
*  NAME
*     job_get_queue_with_task_about_to_exit -- find Q with already exited task
*
*  SYNOPSIS
*     static lList *job_get_queue_with_task_about_to_exit(lListElem *jatep, 
*                                                         lListElem *jatask,
*                                                         u_long32 jobid,
*                                                         u_long32 jataskid);
*
*  FUNCTION
*     tries to find a pe task in the job (array task) <jatep> that has
*     already exited, but which is not yet cleaned up by the execd.
*
*     On exit of a qrsh pe task, the shepherd creates a file 
*     "shepherd_about_to_exit" in the active_jobs directory of the pe task.
*     This function tries to find this file, if it exists, the slot of 
*     the exited task can be reused before being freed by the execd.
*
*     The check is done for all running tasks of the job, if one is found, 
*     the corresponding queue is set to be used by the new task.
*
*  INPUTS
*     jatep    - the actual job (substructure job array task)
*     jatask   - the new pe task
*     jobid    - the jobid of the job
*     jataskid - the task id of the job array task
*
*  RESULT
*     on success, the JAT_granted_destin_identifier list of the new pe task
*     else nullptr
*
*  SEE ALSO
*     execd/job/job_set_queue_info_in_task()
******************************************************************************/
static lList *job_get_queue_with_task_about_to_exit(lListElem *jep,
                                                    lListElem *jatep, 
                                                    lListElem *petep,
                                                    const char *qualified_hostname,
                                                    const char *queuename)
{
   const lListElem *petask;
   
   DENTER(TOP_LAYER);
   
   for_each_ep(petask, lGetList(jatep, JAT_task_list)) {
      const lListElem *pe_task_queue = lFirst(lGetList(petask, PET_granted_destin_identifier_list));
      if (pe_task_queue != nullptr) {
         /* if a certain queue is requested, skip non matching tasks */
         if (queuename != nullptr && strcmp(queuename, lGetString(pe_task_queue, JG_qname)) != 0) {
            continue;
         } else {
            dstring shepherd_about_to_exit = DSTRING_INIT;
            SGE_STRUCT_STAT stat_buffer;
            u_long32 jobid;
            u_long32 jataskid;
            const char *petaskid = nullptr;
           
            jobid = lGetUlong(jep, JB_job_number);
            jataskid = lGetUlong(jatep, JAT_task_number);
            petaskid = lGetString(petask, PET_id);
            
            sge_get_active_job_file_path(&shepherd_about_to_exit,
                                         jobid, jataskid, petaskid,
                                         "shepherd_about_to_exit");
            DPRINTF("checking for file %s\n", sge_dstring_get_string(&shepherd_about_to_exit));

            if (SGE_STAT(sge_dstring_get_string(&shepherd_about_to_exit), &stat_buffer) == 0) {
               lList *jat_gdil = job_set_queue_info_in_task(qualified_hostname, 
                                       lGetString(pe_task_queue, JG_qname), petep);
               DPRINTF("task %s of job %d.%d already exited, using its slot for new task\n", petaskid, jobid, jataskid);
               sge_dstring_free(&shepherd_about_to_exit);         
               DRETURN(jat_gdil); 
            }
            sge_dstring_free(&shepherd_about_to_exit);         
         }
      }   
   }

   DRETURN(nullptr);
}

/****** execd/job/job_get_queue_for_task() ************************************
*  NAME
*     job_get_queue_for_task() -- find a queue suited for task execution
*
*  SYNOPSIS
*     static lList *job_get_queue_for_task(lListElem  *jatep,
*                                          lListElem  *jatask,
*                                          const char *queuename);
*
*  FUNCTION
*     Search for a queue, that 
*        - may be used by job in which new task shall be started
*        - resides on the host of this execd
*        - has free slots for the job in which new task shall be started
*     If a suited queue is found, it is set to be used by the new task.
*
*  INPUTS
*     jatep     - the actual job (substructure job array task)
*     petep     - the new pe task
*     qualified_hostname - qualfied hostname
*     queuename - optional: request a certain queue
*
*  RESULT
*     on success, the JAT_granted_destin_identifier list of the new pe task
*     else nullptr
*
*  SEE ALSO
*     execd/job/job_set_queue_info_in_task()
******************************************************************************/
static lList *
job_get_queue_for_task(lListElem *jatep, lListElem *petep, 
                       const char *qualified_hostname, const char *queuename) 
{
   DENTER(TOP_LAYER);

   bool on_master_host = lGetUlong(jatep, JAT_status) == JSLAVE ? false : true;
   const lListElem *pe = lGetObject(jatep, JAT_pe_object);
   if (pe == nullptr) {
      // can not really happen, we start a task for a tightly integrated parallel job
      DRETURN(nullptr);
   }

   // if we are on the master host and master_forks_slaves is set, not a single qrsh -inherit is allowed
   if (on_master_host && lGetBool(pe, PE_master_forks_slaves)) {
      DRETURN(nullptr);
   }

   const lList *gdil = lGetList(jatep, JAT_granted_destin_identifier_list);

   // in case of daemon_forks_slaves we have to check if we may start more tasks
   if (lGetBool(pe, PE_daemon_forks_slaves)) {
      int max_slots = INT_MAX;
      if (on_master_host) {
         // on the master host we can have the job script itself + one daemon starting slave tasks
         max_slots = 2;
      } else {
         // on a slave host we can only have the daemon starting slave task
         max_slots = 1;
      }

      // we can have multiple queue instances on the host
      // count how many slots are used in total
      // and check if we may still start more
      int total_slots_used = 0;
      const void *iterator = nullptr;
      const lListElem *next_gdil_ep = lGetElemHostFirst(gdil, JG_qhostname, qualified_hostname, &iterator);
      const lListElem *gdil_ep;
      while ((gdil_ep = next_gdil_ep) != nullptr) {
         next_gdil_ep = lGetElemHostNext(gdil, JG_qhostname, qualified_hostname, &iterator);

         const lListElem *queue = lGetObject(gdil_ep, JG_queue);
         if (queue != nullptr) {
            total_slots_used += qinstance_slots_used(queue);
            if (total_slots_used >= max_slots) {
               // all possible tasks are already running
               DRETURN(nullptr);
            }
         }
      }
   }

   // if we get here we may still start tasks
   // select a queue
   const void *iterator = nullptr;
   const lListElem *next_gdil_ep = lGetElemHostFirst(gdil, JG_qhostname, qualified_hostname, &iterator);
   const lListElem *gdil_ep;
   while ((gdil_ep = next_gdil_ep) != nullptr) {
      next_gdil_ep = lGetElemHostNext(gdil, JG_qhostname, qualified_hostname, &iterator);

      // if a specific queue is requested, skip non matching queues
      if (queuename != nullptr && sge_strnullcmp(queuename, lGetString(gdil_ep, JG_qname)) != 0) {
         continue;
      } 

      const lListElem *queue = lGetObject(gdil_ep, JG_queue);
      if (queue != nullptr) {
         /* Queue must have free slots */
         if (qinstance_slots_used(queue) < (int)lGetUlong(queue, QU_job_slots)) {
            lList *jat_gdil = job_set_queue_info_in_task(qualified_hostname, lGetString(gdil_ep, JG_qname), petep);
            DRETURN(jat_gdil); 
         } 
      }
   }

   // if we get here we didn't find a queue instance with free slots
   DRETURN(nullptr);
}


static int handle_task(lListElem *petrep, char *commproc, char *host, u_short id, sge_pack_buffer *apb)
{
   u_long32 jobid, jataskid;
   lListElem *jep   = nullptr;
   lListElem *pe    = nullptr;
   lListElem *jatep = nullptr;
   lListElem *petep = nullptr;
   const char *requested_queue;
   char new_task_id[1024];
   lList *gdil = nullptr;
   int tid = 0;
   const char *progname = component_get_component_name();
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *unqualified_hostname = component_get_unqualified_hostname();

   DENTER(TOP_LAYER);

#ifdef KERBEROS
   if (krb_verify_user(de->host, de->commproc, de->id,
                       lGetString(petrep, PETR_owner)) < 0) {
      ERROR(MSG_SEC_KRB_CRED_SSSI, lGetString(petrep, PETR_owner), de->host, de->commproc, de->id);
      goto Error;
   }
#endif /* KERBEROS */

   jobid    = lGetUlong(petrep, PETR_jobid);
   jataskid = lGetUlong(petrep, PETR_jataskid);

   if (!execd_get_job_ja_task(jobid, jataskid, &jep, &jatep, false)) {
      goto Error;
   }

/*
 * Verify that it is actually the job owner starting a pe task:
 * - in CSP mode, we can check against the user certificate
 * - in general we can compare the pe task request owner against the job owner
 */
   if (!sge_security_verify_unique_identifier(false, 
                                         lGetString(jep, JB_owner), progname, 0,
                                         host, commproc, id)) {
      /* Error message is generated in sge_security_verify_unique_identifier */
      goto Error;
   }
   if (strcmp(lGetString(jep, JB_owner), lGetString(petrep, PETR_owner)) != 0) {
      WARNING(MSG_DENIED_PETASKREQUEST_WRONG_USER_SS, lGetString(petrep, PETR_owner), lGetString(jep, JB_owner));
      goto Error;
   }

   /* do not accept the task if job is not parallel or 'control_slaves' is not active */
   if (!(pe=lGetObject(jatep, JAT_pe_object)) || !lGetBool(pe, PE_control_slaves)) {
      ERROR(MSG_JOB_TASKNOSUITABLEJOB_U, jobid);
      goto Error;
   }

   /* do not accept the task if job is in deletion */
   if (lGetUlong(jatep, JAT_state) & JDELETED) {
      DPRINTF("received task exec request while job is in deletion or exiting\n");
      goto Error;
   }

   /* filter the environment, see issue GE-3761 */
   {
      lList *env_list = lGetListRW(petrep, PETR_environment);
      if (env_list != nullptr) {
         lList *answer_list = nullptr;
         var_list_filter_env_list(env_list, &answer_list);
         answer_list_output(&answer_list);
      }
   }

   /* generate unique task id by combining consecutive number 1-max(u_long32) */
   tid = MAX(1, lGetUlong(jatep, JAT_next_pe_task_id));
   snprintf(new_task_id, sizeof(new_task_id), "%d.%s", tid, unqualified_hostname);
   DPRINTF("using pe_task_id_str %s for job " sge_u32"." sge_u32"\n", new_task_id, jobid, jataskid);
   petep = lCreateElem(PET_Type);
   lSetString(petep, PET_id, new_task_id);

   /* set taskid for next task to be started */
   lSetUlong(jatep, JAT_next_pe_task_id, tid + 1);

   lSetString(petep, PET_name, "petask");
   lSetUlong64(petep, PET_submission_time, lGetUlong64(petrep, PETR_submission_time));
   lSetString(petep, PET_cwd, lGetString(petrep, PETR_cwd));
   lSetList(petep, PET_environment, 
            lCopyList("petask environment", lGetList(petrep, PETR_environment)));
   lSetList(petep, PET_path_aliases, 
            lCopyList("petask path_aliases", lGetList(petrep, PETR_path_aliases)));

   requested_queue = lGetString(petrep, PETR_queuename);

   DPRINTF("got task (" sge_u32"/%s) from (%s/%s/%d) %s queue selection\n",
           lGetUlong(jep, JB_job_number), new_task_id,
           commproc, host, id,
           requested_queue != nullptr ? "with" : "without");

   gdil = job_get_queue_for_task(jatep, petep, qualified_hostname, requested_queue);
         
   if (gdil == nullptr) { /* ran through list without finding matching queue */
      gdil = job_get_queue_with_task_about_to_exit(jep, jatep, petep, qualified_hostname, requested_queue);

      if (gdil == nullptr) {  /* also no already exited task found -> no way to start new task */
         ERROR(MSG_JOB_NOFREEQ_USSS, jobid, lGetString(petrep, PETR_owner), host, qualified_hostname);
         lFreeElem(&petep);
         goto Error;
      }
   }
         
   /* put task into task_list of slave/master job */ 
   if (lGetList(jatep, JAT_task_list) == nullptr) {
      lSetList(jatep, JAT_task_list, lCreateList("task_list", PET_Type));
   }
   /* put task into task_list of slave/master job */ 
   lAppendElem(lGetListRW(jatep, JAT_task_list), petep);

   if (!mconf_get_simulate_jobs()) {
      if (job_write_spool_file(jep, jataskid, nullptr, SPOOL_WITHIN_EXECD)) {
         dstring err_str = DSTRING_INIT;
         sge_dstring_copy_string(&err_str, SGE_EVENT);
         execd_job_start_failure(jep, jatep, petep, sge_dstring_get_string(&err_str), 1);
         sge_dstring_free(&err_str);
         goto Error;
      }
   }
   
   /* 
    * At this time we are sure that we have the task on disk.
    * Now add a new "running" element for this job to the job 
    * report which is used as ACK for this job send request.
    * Add the submission time for the task here.
    */
   {
      lListElem *jr = add_job_report(jobid, jataskid, new_task_id, jep);

      add_usage(jr, "submission_time", nullptr, lGetUlong64(petep, PET_submission_time));
      
      /* if we are not interested in online usage per task, suppress sending of this job report */
      if (mconf_get_sharetree_reserved_usage() && lGetBool(pe, PE_accounting_summary)) {
         lSetBool(jr, JR_no_send, true);
      }
   }

   /* for debugging: never start job but report a failure */
   if (getenv("FAILURE_BEFORE_START")) {
      execd_job_start_failure(jep, jatep, petep, "FAILURE_BEFORE_START", 0);
   }   

   if (sge_make_pe_task_active_dir(jep, jatep, petep, nullptr) == nullptr) {
     goto Error;
   }

   /* put task into task_list of slave/master job */ 
   /* send ack to sender of task */
   if (tid) {
      DPRINTF("sending tid %s\n", new_task_id);
      packstr(apb, new_task_id);
   }

   DRETURN(0);

Error:
   /* JG: TODO: The petep might have got appended to the ja_task's JAT_task_list.
    *           We should better dechain and destroy it.
    *           See issue GE-3461.
    */
   /* send nack to sender of task */
   DPRINTF("sending nack\n");
   packstr(apb, "none");    

   DRETURN(-1);
}

/****** execd/job/job_verify_execd_job() *****************************************
*  NAME
*     job_verify_execd_job() -- verify a job entering execd
*
*  SYNOPSIS
*     bool 
*     job_verify_execd_job(const lListElem *job, lList **answer_list) 
*
*  FUNCTION
*     Verifies a job object entering execd.
*     Does generic tests by calling job_verify, like verifying the cull
*     structure, and makes sure a number of job attributes are set
*     correctly.
*
*  INPUTS
*     const lListElem *job - the job to verify
*     lList **answer_list  - answer list to pass back error messages
*
*  RESULT
*     bool - true on success,
*            false on error with error message in answer_list
*
*  NOTES
*     MT-NOTE: job_verify_execd_job() is MT safe 
*
*  BUGS
*     The function is far from being complete.
*     Currently, only the CULL structure is verified, not the contents.
*
*  SEE ALSO
*     sge_job/job_verify()
*******************************************************************************/
bool
job_verify_execd_job(const lListElem *job, lList **answer_list, const char *qualified_hostname)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   ret = job_verify(job, answer_list, false);

   /* 
    * A job entering execd must have some additional properties:
    *    - correct state
    *    - JB_job_number > 0
    *    - JB_job_name != nullptr
    *    - JB_exec_file etc. ???
    *    - JB_submission_time, JB_execution_time??
    *    - JB_owner != nullptr
    *    - JB_cwd != nullptr??
    */

   if (ret) {
      ret = object_verify_ulong_not_null(job, answer_list, JB_job_number);
   }

   if (ret) {
      ret = object_verify_string_not_null(job, answer_list, JB_job_name);
   }

   if (ret) {
      ret = object_verify_string_not_null(job, answer_list, JB_owner);
   }

   if (ret) {
      const lListElem *ckpt = lGetObject(job, JB_checkpoint_object);
      if (ckpt != nullptr) {
         if (ckpt_validate(ckpt, answer_list) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* for job execution, we need exactly one ja task */
   if (ret) {
      const lList *ja_tasks = lGetList(job, JB_ja_tasks);

      if (ja_tasks == nullptr || lGetNumberOfElem(ja_tasks) != 1) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                           MSG_JOB_INVALIDJATASK_REQUEST);
         ret = false;
      }

      /* verify the ja task structure */
      if (ret) {
         ret = ja_task_verify_execd_job(lFirst(ja_tasks), answer_list);
      }
   }

   /* 
    * JB_stdout_path_list, JB_stderr_path_list, JB_stdin_path_list
    * validate, stdin may not be the same as stdout or stderr
    * except, when it is "/dev/null".
    */
   if (ret) {
      char stdin_path[SGE_PATH_MAX];
      char stdout_path[SGE_PATH_MAX];
      char stderr_path[SGE_PATH_MAX];
      u_long32 job_id = lGetUlong(job, JB_job_number);
      u_long32 ja_task_id = lGetUlong(lFirst(lGetList(job, JB_ja_tasks)), JAT_task_number);

      sge_get_path(qualified_hostname,
                   lGetList(job, JB_stdin_path_list), "",
                   lGetString(job, JB_owner), 
                   lGetString(job, JB_job_name),
                   job_id,
                   job_is_array(job) ? ja_task_id : 0,
                   SGE_STDIN, stdin_path, SGE_PATH_MAX);
      sge_get_path(qualified_hostname,
                   lGetList(job, JB_stdout_path_list), "", 
                   lGetString(job, JB_owner), 
                   lGetString(job, JB_job_name),
                   job_id,
                   job_is_array(job) ? ja_task_id : 0,
                   SGE_STDOUT, stdout_path, SGE_PATH_MAX);
      sge_get_path(qualified_hostname,
                   lGetList(job, JB_stderr_path_list), "",
                   lGetString(job, JB_owner), 
                   lGetString(job, JB_job_name),
                   job_id,
                   job_is_array(job) ? ja_task_id : 0,
                   SGE_STDERR, stderr_path, SGE_PATH_MAX);
      if (strcmp(stdin_path, "/dev/null") != 0) {
         if (strcmp(stdin_path, stdout_path) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_JOB_SAMEPATHSFORINPUTANDOUTPUT_SSS, stdin_path, "stdout", stdout_path);
            ret = false;
         } else if (strcmp(stdin_path, stderr_path) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_JOB_SAMEPATHSFORINPUTANDOUTPUT_SSS, stdin_path, "stderr", stderr_path);
            ret = false;
         }
      }
   }

   DRETURN(ret);
}

