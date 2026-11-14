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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Handling qmaster's request to start a job, as master or as slave host
 */
#include <cerrno>
#include <climits>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "uti/ocs_Systemd.h"
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
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "gdi/ocs_gdi_security.h"

#include "spool/classic/read_write_job.h"

#include "execd.h"
#include "ocs_execd_systemd.h"
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

/** @brief Handle qmaster's request to start a job on this host
 * @param aMsg the received message
 * @param apb the buffer the answer is packed into
 * @param from_qmaster whether the request came from qmaster rather than from a peer execd
 * @return 0 on success
 */
int do_job_exec(ocs::gdi::ClientServerBase::struct_msg_t *aMsg, sge_pack_buffer *apb, bool from_qmaster) {
   DENTER(TOP_LAYER);

   int ret = 1;

   /* if request comes from qmaster: start a job
    * else it is a request to start a pe task
    */
   if (from_qmaster) {
      lListElem *job, *ja_task;
      lList *answer_list = nullptr;


      if (!object_unpack_elem_verify(&answer_list, &(aMsg->buf), &job, JB_Type)) {
         answer_list_output(&answer_list);
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      }

      if (!job_verify_execd_job(job, &answer_list, component_get_qualified_hostname())) {
         const char *err_str = ocs::tom::get<AN_text>(lFirst(answer_list));
         ja_task = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(job)));

         /* set the job into error state */
         execd_job_start_failure(job, ja_task, nullptr, err_str, GFSTATE_JOB);

         /* error output to messages file, cleanup */
         answer_list_output(&answer_list);
         ERROR(MSG_EXECD_INVALIDJOBREQUEST_SS, aMsg->snd_name, aMsg->snd_host);
         lFreeElem(&job);
         DRETURN(0);
      }

      /* we expect one jatask to start per request */
      ocs::tom::Element<ocs::tom::JAT> ja_task_jat = lFirstRW(ocs::tom::get<JB_ja_tasks>(job));
      if (ja_task_jat != nullptr) {
         DPRINTF("new job %ld.%ld\n", (long) ocs::tom::get<JB_job_number>(job), (long) ocs::tom::get<JAT_task_number>(ja_task_jat));
         ret = handle_job(job, ja_task_jat, 0);
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

      DPRINTF("new pe task for job: %ld.%ld\n", (long) ocs::tom::get<PETR_jobid>(petrep), (long) ocs::tom::get<PETR_jataskid>(petrep));

      ret = handle_task(petrep, aMsg->snd_name, aMsg->snd_host, aMsg->snd_id, apb);

      lFreeElem(&petrep);
   }

   if (ret == 0) {
      jobs_to_start = 1;
   }

   DRETURN(0);
}

/** @brief Handle the request to prepare this host as a slave of a parallel job
 *
 * A slave host holds the job's slots and runs its PE tasks, but the master
 * host is the one that starts the job script.
 *
 * @param aMsg the received message
 * @return 0 on success
 */
int do_job_slave(ocs::gdi::ClientServerBase::struct_msg_t *aMsg) {
   DENTER(TOP_LAYER);

   int ret = 1;
   lListElem *jelem, *ja_task;
   lList *answer_list = nullptr;

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

   for_each_rw(ja_task, static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jelem))) {
      DPRINTF("Job: " sge_u32 " Task: " sge_u32 "\n", ocs::tom::get<JB_job_number>(jelem), ocs::tom::get<JAT_task_number>(ja_task));
      ret = handle_job(jelem, ja_task, 1);
   }

   if (ret)  {
      lFreeElem(&jelem);
   }

   DRETURN(0);
}

static int handle_job(lListElem *jelem, lListElem *jatep, int slave) {
   DENTER(TOP_LAYER);

   lListElem *jep;
   dstring err_str = DSTRING_INIT;
   uint32_t jobid, jataskid;
   int general = GFSTATE_QUEUE;
   lListElem *qep, *gdil_ep;
   const char *qnm;
   const void *iterator = nullptr;
   bool report_job_error = true;   /* send job report on error? */

   DPRINTF("got %s job " sge_u32 "\n", slave ?"slave ":"", ocs::tom::get<JB_job_number>(jelem));

   jobid = ocs::tom::get<JB_job_number>(jelem);
   jataskid = ocs::tom::get<JAT_task_number>(jatep);

   /*
    * Sometimes the rescheduled job from qmaster which is
    * sent synchronuously arrives earlier than the ack
    * reporting that qmaster got job exit message from run before
    *
    * We can ignore this job because job is resend by qmaster.
    */
   jep = ocs::tom::find_first_rw<JB_job_number>(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), jobid, &iterator);
   while (jep != nullptr) {
      if (job_search_task(jep, nullptr, jataskid) != nullptr) {
         // qmaster resends the job when it did not get the acknowledgement of a delivery in
         // time, then the job is already here. It can also be the slave container of a
         // previous run of a rescheduled tightly integrated job, which is still waiting for
         // its pe tasks to exit. The delivery is dropped either way, so say so: a job whose
         // start keeps being dropped hangs in the transferring state.
         WARNING(MSG_EXECD_JOBALREADYKNOWN_UU, jobid, jataskid);
         goto Ignore;   /* don't set queue in error state */
      }

      jep = ocs::tom::find_next_rw<JB_job_number>(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), jobid, &iterator);
   }

   /* initialize state - prevent slaves from getting started */
   ocs::tom::set<JAT_status>(jatep, slave?JSLAVE:JIDLE);

   /* now we have a queue and a job filled */
   DPRINTF("===>JOB_EXECUTION: >" sge_u32 "." sge_u32 "< with " sge_u32 " tickets\n", jobid, jataskid, static_cast<uint32_t>(ocs::tom::get<JAT_tix>(jatep)));

   /* initialize job */
   for_each_rw (gdil_ep, static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep))) {
      lList *answer_list = nullptr;
      int slots;

      if (!(qep=ocs::tom::get<JG_queue>(gdil_ep))) {
         qnm = ocs::tom::get<JG_qname>(gdil_ep);
         sge_dstring_sprintf(&err_str, MSG_JOB_MISSINGQINGDIL_SU, qnm, ocs::tom::get<JB_job_number>(jelem));
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
      ocs::tom::set<QU_state>(qep, 0);

      /* store number of slots we got in this queue for this job */
      slots = ocs::tom::get<JG_slots>(gdil_ep);
      ocs::tom::set<QU_job_slots>(qep, slots);
      qinstance_set_slots_used(qep, 0);
   }

   /* ------- optionally pe */
   if (ocs::tom::get<JAT_granted_pe>(jatep)) {
      ocs::tom::Element<ocs::tom::PE> pelem = ocs::tom::get<JAT_pe_object>(jatep);

      if (pelem != nullptr && ocs::tom::get<PE_control_slaves>(pelem) && !ocs::tom::get<PE_job_is_first_task>(pelem)) {
         int slots;
         lListElem *mq = ocs::tom::get<JG_queue>(lFirstRW(static_cast<lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep))));
         slots = ocs::tom::get<QU_job_slots>(mq) + 1;
         DPRINTF("Increasing job slots in master queue \"%s\" to %d because job is not first task\n",
                 ocs::tom::get<QU_qname>(mq), slots);
         ocs::tom::set<QU_job_slots>(mq, slots);
      }
   }

   if (sge_make_ja_task_active_dir(jelem, jatep, &err_str) == nullptr) {
      goto Error;
   }

   if (!JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(jelem))) {
      /* interactive jobs and slave jobs do not have a script file */
      if (!slave && ocs::tom::get<JB_script_file>(jelem)) {
         int nwritten;
         uint32_t job_id = ocs::tom::get<JB_job_number>(jelem);

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
               fd = SGE_OPEN3(ocs::tom::get<JB_exec_file>(jelem), O_CREAT | O_WRONLY, 0755);
               if (fd < 0) {
                  sge_dstring_sprintf(&err_str, MSG_ERRORWRITINGFILE_SS,
                                      ocs::tom::get<JB_exec_file>(jelem),
                                      strerror(errno));
                  goto Error;
               }

               nwritten = sge_writenbytes(fd, ocs::tom::get<JB_script_ptr>(jelem), ocs::tom::get<JB_script_size>(jelem));
               if (nwritten != (int)ocs::tom::get<JB_script_size>(jelem)) {
                  DPRINTF("errno: %d\n", errno);
                  sge_dstring_sprintf(&err_str, MSG_EXECD_NOWRITESCRIPT_SIUS,
                                      ocs::tom::get<JB_exec_file>(jelem), nwritten,
                                      ocs::tom::get<JB_script_size>(jelem),
                                      strerror(errno));
                  close(fd);
                  goto Error;
               }
               close(fd);
            }
            ocs::tom::set<JB_script_ptr>(jelem, nullptr);
         }
      }
   }

#if defined (OCS_WITH_SYSTEMD)
   // Store the slice name before the ja_task is spooled below. The slice of a
   // tightly integrated job is removed by execd_delete_tight_pe_slice(), which
   // reads the name back from JAT_systemd_slice. If the execd goes down while
   // the job is still known and the job ends in the meantime, the restarted
   // execd only has what was spooled - a name set after the write would be lost
   // and the slice would stay behind forever. The master task path does the same
   // (exec_job.cc), where the store is followed by the spool in
   // execd_ck_to_do.cc.
   if (slave) {
      bool enable_systemd = ocs::execd::execd_use_systemd();
      if (enable_systemd) {
         ocs::execd::execd_store_tight_pe_slice(jelem, jatep);
      }
   }
#endif

   if (!mconf_get_simulate_jobs()) {
      ocs::tom::set<JB_script_size>(jelem, 0);
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

/**
 * @brief Set queue to use for task
 *
 * Extend the task structure of task `jatask` by a
 * JAT_granted_destin_identifier list, which contains
 * the queue `qname` and uses one slot.
 *
 * @param qualified_hostname name of host
 * @param qname name of queue to set
 * @param pe_task task structure
 *
 * @return the new created JAT_granted_destin_identifier list
 */
static lList *job_set_queue_info_in_task(const char *qualified_hostname, const char *qname, lListElem *petep) {
   DENTER(TOP_LAYER);


   ocs::tom::Element<ocs::tom::JG> jge = ocs::tom::emplace_back_in<PET_granted_destin_identifier_list, JG_qname>(petep, qname);
   ocs::tom::set<JG_qhostname>(jge, qualified_hostname);
   ocs::tom::set<JG_slots>(jge, 1);
   DPRINTF("selected queue %s for task\n", qname);

   DRETURN(ocs::tom::get<PET_granted_destin_identifier_list>(petep));
}

/**
 * @brief Find Q with already exited task
 *
 * tries to find a pe task in the job (array task) `jatep` that has
 * already exited, but which is not yet cleaned up by the execd.
 * On exit of a qrsh pe task, the shepherd creates a file
 * "shepherd_about_to_exit" in the active_jobs directory of the pe task.
 * This function tries to find this file, if it exists, the slot of
 * the exited task can be reused before being freed by the execd.
 * The check is done for all running tasks of the job, if one is found,
 * the corresponding queue is set to be used by the new task.
 *
 * @param jatep the actual job (substructure job array task)
 * @param jatask the new pe task
 * @param jobid the jobid of the job
 * @param jataskid the task id of the job array task
 *
 * @return on success, the JAT_granted_destin_identifier list of the new pe task else nullptr
 *
 * @see #job_set_queue_info_in_task
 */
static lList *job_get_queue_with_task_about_to_exit(lListElem *jep,
                                                    lListElem *jatep,
                                                    lListElem *petep,
                                                    const char *qualified_hostname,
                                                    const char *queuename) {
   DENTER(TOP_LAYER);

   for (ocs::tom::ConstElement<ocs::tom::PET> petask : ocs::tom::get<JAT_task_list>(jatep)) {
      ocs::tom::ConstElement<ocs::tom::JG> pe_task_queue = lFirst(ocs::tom::get<PET_granted_destin_identifier_list>(petask));
      if (pe_task_queue != nullptr) {
         /* if a certain queue is requested, skip non matching tasks */
         if (queuename != nullptr && strcmp(queuename, ocs::tom::get<JG_qname>(pe_task_queue)) != 0) {
            continue;
         } else {
            dstring shepherd_about_to_exit = DSTRING_INIT;
            SGE_STRUCT_STAT stat_buffer;
            uint32_t jobid;
            uint32_t jataskid;
            const char *petaskid = nullptr;

            jobid = ocs::tom::get<JB_job_number>(jep);
            jataskid = ocs::tom::get<JAT_task_number>(jatep);
            petaskid = ocs::tom::get<PET_id>(petask);

            sge_get_active_job_file_path(&shepherd_about_to_exit,
                                         jobid, jataskid, petaskid,
                                         "shepherd_about_to_exit");
            DPRINTF("checking for file %s\n", sge_dstring_get_string(&shepherd_about_to_exit));

            if (SGE_STAT(sge_dstring_get_string(&shepherd_about_to_exit), &stat_buffer) == 0) {
               lList *jat_gdil = job_set_queue_info_in_task(qualified_hostname,
                                       ocs::tom::get<JG_qname>(pe_task_queue), petep);
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

/**
 * @brief Find a queue suited for task execution
 *
 * Search for a queue, that
 *    - may be used by job in which new task shall be started
 *    - resides on the host of this execd
 *    - has free slots for the job in which new task shall be started
 * If a suited queue is found, it is set to be used by the new task.
 *
 * @param jatep the actual job (substructure job array task)
 * @param petep the new pe task
 * @param qualified_hostname qualfied hostname
 * @param queuename optional: request a certain queue
 *
 * @return on success, the JAT_granted_destin_identifier list of the new pe task else nullptr
 *
 * @see #job_set_queue_info_in_task
 */
static lList *
job_get_queue_for_task(uint32_t job_id, uint32_t ja_task_id, lListElem *jatep, lListElem *petep,
                       const char *qualified_hostname, const char *queuename) {
   DENTER(TOP_LAYER);

   bool on_master_host = ocs::tom::get<JAT_status>(jatep) == JSLAVE ? false : true;
   ocs::tom::ConstElement<ocs::tom::PE> pe = ocs::tom::get<JAT_pe_object>(jatep);
   if (pe == nullptr) {
      // can not really happen, we start a task for a tightly integrated parallel job
      DRETURN(nullptr);
   }

   // if we are on the master host and master_forks_slaves is set, not a single qrsh -inherit is allowed
   if (on_master_host && ocs::tom::get<PE_master_forks_slaves>(pe)) {
      WARNING(MSG_REJECT_TASK_MASTER_FORKS_UU, job_id, ja_task_id);
      DRETURN(nullptr);
   }

   ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);

   // in case of daemon_forks_slaves we have to check if we may start more tasks
   if (ocs::tom::get<PE_daemon_forks_slaves>(pe)) {
      int max_slots;
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
      const lListElem *next_gdil_ep = ocs::tom::find_first<JG_qhostname>(gdil, qualified_hostname, &iterator);
      const lListElem *gdil_ep;
      while ((gdil_ep = next_gdil_ep) != nullptr) {
         next_gdil_ep = ocs::tom::find_next<JG_qhostname>(gdil, qualified_hostname, &iterator);

         const lListElem *queue = ocs::tom::get<JG_queue>(gdil_ep);
         if (queue != nullptr) {
            total_slots_used += qinstance_slots_used(queue);
            if (total_slots_used >= max_slots) {
               // all possible tasks are already running
               WARNING(MSG_REJECT_TASK_NOT_ENOUGH_SLOTS_UUII, job_id, ja_task_id, max_slots, total_slots_used);
               DRETURN(nullptr);
            }
         }
      }
   }

   // if we get here we may still start tasks
   // select a queue
   const void *iterator = nullptr;
   const lListElem *next_gdil_ep = ocs::tom::find_first<JG_qhostname>(gdil, qualified_hostname, &iterator);
   const lListElem *gdil_ep;
   while ((gdil_ep = next_gdil_ep) != nullptr) {
      next_gdil_ep = ocs::tom::find_next<JG_qhostname>(gdil, qualified_hostname, &iterator);

      // if a specific queue is requested, skip non matching queues
      if (queuename != nullptr && sge_strnullcmp(queuename, ocs::tom::get<JG_qname>(gdil_ep)) != 0) {
         continue;
      }

      const lListElem *queue = ocs::tom::get<JG_queue>(gdil_ep);
      if (queue != nullptr) {
         /* Queue must have free slots */
         if (qinstance_slots_used(queue) < (int)ocs::tom::get<QU_job_slots>(queue)) {
            lList *jat_gdil = job_set_queue_info_in_task(qualified_hostname, ocs::tom::get<JG_qname>(gdil_ep), petep);
            if (jat_gdil == nullptr) {
               WARNING(MSG_REJECT_TASK_OUT_OF_MEMORY_UU, job_id, ja_task_id);
            }
            DRETURN(jat_gdil);
         } else {
            WARNING(MSG_REJECT_TASK_SLOTS_IN_USE_UUIUS, job_id, ja_task_id, qinstance_slots_used(queue),
                    ocs::tom::get<QU_job_slots>(queue), ocs::tom::get<QU_qname>(queue));
         }
      }
   }

   // if we get here we didn't find a queue instance with free slots
   WARNING(MSG_REJECT_TASK_NO_FREE_SLOT_FOUND_UU, job_id, ja_task_id);
   DRETURN(nullptr);
}


static int handle_task(lListElem *petrep, char *commproc, char *host, u_short id, sge_pack_buffer *apb) {
   DENTER(TOP_LAYER);

   uint32_t jobid, jataskid;
   lListElem *jep   = nullptr;
   ocs::tom::Element<ocs::tom::PE> pe    = nullptr;
   lListElem *jatep = nullptr;
   lListElem *petep = nullptr;
   const char *requested_queue;
   char new_task_id[1024];
   lList *gdil = nullptr;
   int tid = 0;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *unqualified_hostname = component_get_unqualified_hostname();


   jobid    = ocs::tom::get<PETR_jobid>(petrep);
   jataskid = ocs::tom::get<PETR_jataskid>(petrep);

   if (!execd_get_job_ja_task(jobid, jataskid, &jep, &jatep, false)) {
      goto Error;
   }

/*
 * Verify that it is actually the job owner starting a pe task: compare the
 * pe task request owner against the job owner.
 */
   if (strcmp(ocs::tom::get<JB_owner>(jep), ocs::tom::get<PETR_owner>(petrep)) != 0) {
      WARNING(MSG_DENIED_PETASKREQUEST_WRONG_USER_SS, ocs::tom::get<PETR_owner>(petrep), ocs::tom::get<JB_owner>(jep));
      goto Error;
   }

   /* do not accept the task if job is not parallel or 'control_slaves' is not active */
   if (!(pe=ocs::tom::get<JAT_pe_object>(jatep)) || !ocs::tom::get<PE_control_slaves>(pe)) {
      ERROR(MSG_JOB_TASKNOSUITABLEJOB_U, jobid);
      goto Error;
   }

   /* do not accept the task if job is in deletion */
   if (ocs::tom::get<JAT_state>(jatep) & JDELETED) {
      DPRINTF("received task exec request while job is in deletion or exiting\n");
      goto Error;
   }

   /*
    * The environment of a parallel task is not filtered any more (Issue GE-3761): the task
    * runs as the job owner. The shepherd drops the loader and interpreter variables for the
    * procedures which run as another user, see dangerous_env_vars[] in builtin_starter.cc
    * (CS-2764).
    */

   /* generate unique task id by combining consecutive number 1-max(uint32_t) */
   tid = std::max(1u, ocs::tom::get<JAT_next_pe_task_id>(jatep));
   snprintf(new_task_id, sizeof(new_task_id), "%d.%s", tid, unqualified_hostname);
   DPRINTF("using pe_task_id_str %s for job " sge_u32 "." sge_u32"\n", new_task_id, jobid, jataskid);
   petep = lCreateElem(PET_Type);
   ocs::tom::set<PET_id>(petep, new_task_id);

   /* set taskid for next task to be started */
   ocs::tom::set<JAT_next_pe_task_id>(jatep, tid + 1);

   ocs::tom::set<PET_name>(petep, "petask");
   ocs::tom::set<PET_submission_time>(petep, ocs::tom::get<PETR_submission_time>(petrep));
   ocs::tom::set<PET_cwd>(petep, ocs::tom::get<PETR_cwd>(petrep));
   ocs::tom::set<PET_environment>(petep,
            lCopyList("petask environment", ocs::tom::get<PETR_environment>(petrep)));
   ocs::tom::set<PET_path_aliases>(petep,
            lCopyList("petask path_aliases", ocs::tom::get<PETR_path_aliases>(petrep)));
#if defined(OCS_WITH_OPENSSL)
   ocs::tom::set<PET_cred>(petep, ocs::tom::get<PETR_cred>(petrep));
#endif

   requested_queue = ocs::tom::get<PETR_queuename>(petrep);

   DPRINTF("got task (" sge_u32 "/%s) from (%s/%s/%d) %s queue selection\n",
           jobid, new_task_id,
           commproc, host, id, requested_queue != nullptr ? "with" : "without");

   gdil = job_get_queue_for_task(jobid, jataskid, jatep, petep, qualified_hostname, requested_queue);

   if (gdil == nullptr) { /* ran through list without finding matching queue */
      gdil = job_get_queue_with_task_about_to_exit(jep, jatep, petep, qualified_hostname, requested_queue);

      if (gdil == nullptr) {  /* also no already exited task found -> no way to start new task */
         ERROR(MSG_JOB_NOFREEQ_USSS, jobid, ocs::tom::get<PETR_owner>(petrep), host, qualified_hostname);
         lFreeElem(&petep);
         goto Error;
      }
   }

   /* put task into task_list of slave/master job */
   if (static_cast<const lList *>(ocs::tom::get<JAT_task_list>(jatep)) == nullptr) {
      ocs::tom::set<JAT_task_list>(jatep, lCreateList("task_list", PET_Type));
   }
   /* put task into task_list of slave/master job */
   lAppendElem(ocs::tom::get<JAT_task_list>(jatep), petep);

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

      add_usage(jr, "submission_time", nullptr, ocs::tom::get<PET_submission_time>(petep));

      /* if we are not interested in online usage per task, suppress sending of this job report */
      if (mconf_get_sharetree_reserved_usage() && ocs::tom::get<PE_accounting_summary>(pe)) {
         ocs::tom::set<JR_no_send>(jr, true);
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

/**
 * @brief Verify a job entering execd
 *
 * Verifies a job object entering execd.
 * Does generic tests by calling job_verify, like verifying the cull
 * structure, and makes sure a number of job attributes are set
 * correctly.
 *
 * @param job the job to verify
 * @param answer_list answer list to pass back error messages
 * @param qualified_hostname this host, as the cluster knows it
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: job_verify_execd_job() is MT safe
 *
 * @bug The function is far from being complete.
 *      Currently, only the CULL structure is verified, not the contents.
 *
 * @see `job_verify()`
 */
bool job_verify_execd_job(const lListElem *job, lList **answer_list, const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   bool ret = true;

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
      const lListElem *ckpt = ocs::tom::get<JB_checkpoint_object>(job);
      if (ckpt != nullptr) {
         if (ckpt_validate(ckpt, answer_list) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* for job execution, we need exactly one ja task */
   if (ret) {
      ocs::tom::ConstList<ocs::tom::JAT> ja_tasks = ocs::tom::get<JB_ja_tasks>(job);

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
      uint32_t job_id = ocs::tom::get<JB_job_number>(job);
      uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(lFirst(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(job))));

      sge_get_path(qualified_hostname,
                   ocs::tom::get<JB_stdin_path_list>(job), "",
                   ocs::tom::get<JB_owner>(job),
                   ocs::tom::get<JB_job_name>(job),
                   job_id,
                   job_is_array(job) ? ja_task_id : 0,
                   SGE_STDIN, stdin_path, SGE_PATH_MAX);
      sge_get_path(qualified_hostname,
                   ocs::tom::get<JB_stdout_path_list>(job), "",
                   ocs::tom::get<JB_owner>(job),
                   ocs::tom::get<JB_job_name>(job),
                   job_id,
                   job_is_array(job) ? ja_task_id : 0,
                   SGE_STDOUT, stdout_path, SGE_PATH_MAX);
      sge_get_path(qualified_hostname,
                   ocs::tom::get<JB_stderr_path_list>(job), "",
                   ocs::tom::get<JB_owner>(job),
                   ocs::tom::get<JB_job_name>(job),
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
