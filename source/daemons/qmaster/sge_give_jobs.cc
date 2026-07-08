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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <signal.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"

#include "sched/debit.h"

#include "gdi/ocs_gdi_security.h"

#include "spool/sge_spooling.h"

#include "ocs_CategoryQmaster.h"
#include "ocs_FinishedJob.h"
#include "ocs_ReportingFileWriter.h"
#include "sge.h"
#include <cinttypes>
#include "execution_states.h"
#include "sge_subordinate_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_host_qmaster.h"
#include "evm/sge_event_master.h"
#include "evm/sge_queue_event_master.h"
#include "sge_cqueue_qmaster.h"
#include "reschedule.h"
#include "sge_task_depend.h"
#include "sge_qmaster_timed_event.h"
#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

#include <ocs_gdi_ClientServerBase.h>

static void
sge_clear_granted_resources(lListElem *jep, lListElem *ja_task, int incslots,
                            monitoring_t *monitor, uint64_t gdi_session);

static void
reduce_queue_limits(const lList *master_centry_list, const lListElem *gdil_ep, lListElem *qep,
                    const lListElem *job, const lListElem *ja_task, bool is_pe_job, bool is_first_gdil_entry);
static void
reduce_queue_limit(const lList *master_centry_list, lListElem *qep, const lListElem *job,
                   bool is_pe_job, bool is_first_gdil_entry, bool master_only, int nm, const char *rlimit_name);

static void
release_successor_jobs(const lListElem *jep, uint64_t gdi_session);

static void
release_successor_jobs_ad(const lListElem *jep, uint64_t gdi_session);

static void
release_successor_tasks_ad(lListElem *jep, uint32_t task_id, uint64_t gdi_session);

static int
send_slave_jobs(lListElem *jep, lListElem *jatep, monitoring_t *monitor, uint64_t gdi_session);


static int
send_slave_jobs_wc(lListElem *tmpjep, monitoring_t *monitor, uint64_t gdi_session);

static int
send_job(const char *rhost, lListElem *jep, lListElem *jatep, lListElem *hep, int master, uint64_t gdi_session);

static void
sge_bury_job(const char *sge_root, lListElem *jep, uint32_t jid, lListElem *ja_task, int spool_job, int no_events, uint64_t gdi_session);

static void
sge_job_finish_event(lListElem *jep, lListElem *jatep, lListElem *jr, int commit_flags, const char *diagnosis, uint64_t gdi_session);

static int
setCheckpointObj(lListElem *job);

static lListElem *
copyJob(lListElem *job, lListElem *ja_task);

static int queue_field[] = {QU_qhostname,
                            QU_qname,
                            QU_full_name,
                            QU_job_slots,
                            QU_priority,
                            QU_tmpdir,
                            QU_shell_start_mode,
                            QU_shell,
                            QU_tmpdir,
                            QU_prolog,
                            QU_epilog,
                            QU_starter_method,
                            QU_suspend_method,
                            QU_processors,
                            QU_resume_method,
                            QU_terminate_method,
                            QU_min_cpu_interval,
                            QU_notify,
                            QU_consumable_config_list,
                            QU_resource_utilization,
                            QU_state,
                            QU_s_rt,
                            QU_h_rt,
                            QU_s_cpu,
                            QU_h_cpu,
                            QU_s_fsize,
                            QU_h_fsize,
                            QU_s_data,
                            QU_h_data,
                            QU_s_stack,
                            QU_h_stack,
                            QU_s_core,
                            QU_h_core,
                            QU_s_rss,
                            QU_h_rss,
                            QU_s_vmem,
                            QU_h_vmem,
                            NoName};


/************************************************************************
 Master function to give job to the execd.

 We make asynchronous sends and implement a retry mechanism.
 Do everything to make sure the execd is prepared for receiving the job.
 ************************************************************************/
/* pe = is nullptr for serial jobs*/
int
sge_give_job(lListElem *jep, lListElem *jatep, const lListElem *master_qep, lListElem *hep, monitoring_t *monitor, uint64_t gdi_session) {
   const char *rhost;
   int ret = 0;
   int sent_slaves = 0;

   DENTER(TOP_LAYER);

   rhost = lGetHost(master_qep, QU_qhostname);
   DPRINTF("execd host: %s\n", rhost);

   switch (send_slave_jobs(jep, jatep, monitor, gdi_session)) {
      case -1:
         ret = -1;
         sent_slaves = 1;
         break;
      case 0:
         sent_slaves = 1;
         break;
      case 1:
         // Either it is a sequential/array job or a loosely integrated parallel job:
         // We may send the master task immediately.
         // Or it is a tightly integrated parallel job and all slave hosts have acknowledged the job start.
         // Then we may send the master task.
         sent_slaves = 0;
         break;
      default:
         DPRINTF("send_slave_jobs returned an unknown error code\n");
         // @todo sent_slaves is initialized to 0, so we will send the job!
         //       Can not really happen as send_slave_jobs() only returns -1, 0, or 1.
         ret = -1;
         break;
   }

   if (!sent_slaves) {
      /* wait till all slaves are acked */
      lSetUlong(jatep, JAT_next_pe_task_id, 1);
      ret = send_job(rhost, jep, jatep, hep, 1, gdi_session);
      MONITOR_MESSAGES_OUT(monitor);
   }

   DRETURN(ret);
}


/****** sge_give_jobs/send_slave_jobs() ****************************************
*  NAME
*     send_slave_jobs() -- send out slave tasks of a pe job
*
*  SYNOPSIS
*     static int send_slave_jobs(lListElem *jep, lListElem *jatep)
*
*  FUNCTION
*     It prepares the data for the sending out the pe slaves. Once that data
*     is created, it calles the actual send_slave_method.
*
*  INPUTS
*     lListElem *jep     - job structure
*     lListElem *jatep   - ja-taks (template, not the actual one)
*
*  RESULT
*     static int -  1 : no pe job
*                   0 : send slaves
*                  -1 : something went wrong
*
*  NOTES
*     MT-NOTE: send_slave_jobs() is not MT safe
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
static int
send_slave_jobs(lListElem *jep, lListElem *jatep, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   lListElem *tmpjep, *qep, *tmpjatep;
   int ret = 0;
   bool is_pe_jobs = false;
   lDescr *rdp = nullptr;
   lEnumeration *what;
   const lList *master_centry_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   // do we still have pe slave tasks to be delivered?
   for_each_rw_lv(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
      if (lGetUlong(gdil_ep, JG_tag_slave_job)) {
         lSetUlong(jatep, JAT_next_pe_task_id, 1);
         is_pe_jobs = true;
         break;
      }
   }
   if (!is_pe_jobs) { /* we do not have slaves... nothing to do */
      DRETURN(1);
   }

   /* prepare the data to be sent ... */

   /* create a copy of the job */
   if ((tmpjep = copyJob(jep, jatep)) == nullptr) {
      DRETURN(-1);
   }

   /* insert ckpt object if required **now**. it is only
   ** needed in the execd and we have no need to keep it
   ** in qmaster's permanent job list
   */
   if (setCheckpointObj(tmpjep) != 0) {
      lFreeElem(&tmpjep);
      DRETURN(-1);
   }

   /* add all queues referenced in gdil to qlp
    * (necessary for availability of ALL resource limits and tempdir in queue)
    * AND
    * copy all JG_processors from all queues to the JG_processors in gdil
    * (so the execd can decide on which processors (-sets) the job will be run).
    */
   what = lIntVector2What(QU_Type, queue_field);
   lReduceDescr(&rdp, QU_Type, what);

   tmpjatep = lFirstRW(lGetList(tmpjep, JB_ja_tasks));
   bool is_pe_job = lGetObject(tmpjatep, JAT_pe_object) != nullptr;
   bool is_first_gdil_entry = true;
   for_each_rw_lv (gdil_ep, lGetList(tmpjatep, JAT_granted_destin_identifier_list)) {
      const char *src_qname = lGetString(gdil_ep, JG_qname);
      const lListElem *src_qep = cqueue_list_locate_qinstance(master_cqueue_list, src_qname);

      if (src_qep == nullptr) {
         DRETURN(-1);
      }

      /* copy all JG_processors from all queues to gdil (which will be
       * sent to the execd).
       */
      lSetString(gdil_ep, JG_processors, lGetString(src_qep, QU_processors));

      // copy the referenced queue instance
      qep = lSelectElemDPack(src_qep, nullptr, rdp, what, false, nullptr);
      lSetObject(gdil_ep, JG_queue, qep);

      // build minimum of job request and queue resource limit
      reduce_queue_limits(master_centry_list, gdil_ep, qep, tmpjep, tmpjatep, is_pe_job, is_first_gdil_entry);

      is_first_gdil_entry = false;
   }

   lFreeWhat(&what);
   sge_free(&rdp);

   ret = send_slave_jobs_wc(tmpjep, monitor, gdi_session);

   lFreeElem(&tmpjep);

   DRETURN(ret);
}

/****** sge_give_jobs/send_slave_jobs_wc() *************************************
*  NAME
*     send_slave_jobs_wc() -- takes the prepared data end sends it out.
*
*  SYNOPSIS
*     static int send_slave_jobs_wc(lListElem *tmpjep,
*     lListElem *jatep, lListElem *pe, lList *qlp)
*
*  FUNCTION
*     This is a helper function of send_slave_jobs. It handles the actual send.
*
*  INPUTS
*     lListElem *tmpjep  - prepared job structure
*     lListElem *jatep   - ja-taks (most likely the templete)
*     lListElem *pe      - target pe
*     lList *qlp         - prepared queue instance list
*
*  RESULT
*     static int -  0 : everything is fine
*                  -1 : an error
*
*  NOTES
*     MT-NOTE: send_slave_jobs_wc() is not MT safe
*
*******************************************************************************/
static int
send_slave_jobs_wc(lListElem *jep, monitoring_t *monitor, uint64_t gdi_session) {
   lList *saved_gdil = nullptr;
   lList *gdil;
   lListElem *gdil_ep = nullptr;
   lListElem *next_gdil_ep = nullptr;
   lListElem *jatep = lFirstRW(lGetList(jep, JB_ja_tasks));
   int ret = 0;
   int failed = CL_RETVAL_OK;
   lList *master_ehost_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);

   const char *sge_root = bootstrap_get_sge_root();
   bool simulate_execd = mconf_get_simulate_execds();

   DENTER(TOP_LAYER);

   gdil = saved_gdil = lCreateList("", JG_Type);
   lXchgList(jatep, JAT_granted_destin_identifier_list, &saved_gdil);

   next_gdil_ep = lFirstRW(saved_gdil);
   while ((gdil_ep = next_gdil_ep)) {
      lListElem *hep;
      const char *hostname = nullptr;
      unsigned long last_heard_from;

      next_gdil_ep = lNextRW(gdil_ep);

      // when we are re-sending: skip the already acknowledged ones
      if (lGetUlong(gdil_ep, JG_tag_slave_job) == 0) {
         continue;
      }

      hostname = lGetHost(gdil_ep, JG_qhostname);
      hep = host_list_locate(master_ehost_list, hostname);
      if (hep == nullptr) {
         ret = -1;
         break;
      }

      if (!simulate_execd) {
         /* do ask_commproc() only if we are missing load reports */
         cl_commlib_get_last_message_time(cl_com_get_handle(to_cstr(QMASTER), 0),
                                          hostname, to_cstr(EXECD), 1, &last_heard_from);
         if (sge_gmt32_to_gmt64(last_heard_from + mconf_get_max_unheard()) <= sge_get_gmt64()) {

            ERROR(MSG_COM_CANT_DELIVER_UNHEARD_SSU, to_cstr(EXECD), hostname,  lGetUlong(jep, JB_job_number));
            sge_mark_unheard(hep, gdi_session);
            ret = -1;
            break;
         }
      }

      /*
      ** get credential for job
      */
      if (mconf_get_do_credentials()) {
         cache_sec_cred(sge_root, jep, hostname);
      }

      lDechainElem(saved_gdil, gdil_ep);
      lAppendElem(gdil, gdil_ep);

      /*
      ** security hook
      */
      tgt2cc(jep, hostname);
      {
         uint32_t dummymid = 0;
         sge_pack_buffer send_pb;

         init_packbuffer(&send_pb, 0);

         pack_job_delivery(&send_pb, jep);
         if (simulate_execd) {
            failed = CL_RETVAL_OK;
         } else {
            failed = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, to_cstr(EXECD), 1, hostname, ocs::gdi::ClientServerBase::TAG_SLAVE_ALLOW, &send_pb, &dummymid);
         }
         clear_packbuffer(&send_pb);
      }
      MONITOR_MESSAGES_OUT(monitor);
      /*
      ** security hook
      */
      tgtcclr(jep, hostname);

      if (failed != CL_RETVAL_OK) {
         /* we failed to send the job to the execd */
         ERROR(MSG_COM_SENDJOBTOHOST_US, lGetUlong(jep, JB_job_number), hostname);
         ERROR("commlib error: %s\n", cl_get_error_text(failed));
         sge_mark_unheard(hep, gdi_session);
         ret = -1;
         break;
      } else {
         DPRINTF("successfully sent slave job " sge_u32 " to host \"%s\"\n", lGetUlong(jep, JB_job_number), hostname);
      }
      lRemoveElem(gdil, &gdil_ep);
   }
   lFreeList(&saved_gdil);

   DRETURN(ret);
}

static int
send_job(const char *rhost, lListElem *jep, lListElem *jatep, lListElem *hep, int master, uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   int failed;
   sge_pack_buffer pb;
   lListElem *tmpjep, *qep, *tmpjatep = nullptr;
   unsigned long last_heard_from;
   const char *sge_root = bootstrap_get_sge_root();
   const char *myprogname = component_get_component_name();
   bool simulate_execd = mconf_get_simulate_execds();
   lDescr *rdp = nullptr;
   lEnumeration *what;
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   if (!simulate_execd) {
      cl_commlib_get_last_message_time(cl_com_get_handle(myprogname, 0), rhost, to_cstr(EXECD), 1,
                                       &last_heard_from);
      uint64_t now = sge_get_gmt64();
      if (sge_gmt32_to_gmt64(last_heard_from + mconf_get_max_unheard()) <= now) {
         ERROR(MSG_COM_CANT_DELIVER_UNHEARD_SSU, to_cstr(EXECD), rhost,  lGetUlong(jep, JB_job_number));
         sge_mark_unheard(hep, gdi_session);
         DRETURN(-1);
      }
   }
   // @todo: we do lots of things *with* simulate_execd, why? Just create the timed event.

   if ((tmpjep = copyJob(jep, jatep)) == nullptr) {
      DRETURN(-1);
   } else {
      tmpjatep = lFirstRW(lGetList(tmpjep, JB_ja_tasks));
   }

   /* load script into job structure for sending to execd */
   /*
   ** if exec_file is not set, then this is an interactive job
   */
   if (master && lGetString(tmpjep, JB_exec_file) && !JOB_TYPE_IS_BINARY(lGetUlong(jep, JB_type))) {
      if (!spool_read_script(nullptr, lGetUlong(tmpjep, JB_job_number), tmpjep)) {
         lFreeElem(&tmpjep);
         DRETURN(-1);
      }
   }

   /* if a granted resource list is defined int must be passed through, too */
   if (lGetList(jatep, JAT_granted_resources_list)) {
      const lList *grl = lGetList(jatep, JAT_granted_resources_list);
      lSetList(tmpjatep, JAT_granted_resources_list, lCopyList(nullptr, grl));
   }

   /* insert ckpt object if required **now**. it is only
   ** needed in the execd and we have no need to keep it
   ** in qmaster's permanent job list
   */
   if (setCheckpointObj(tmpjep) != 0) {
      lFreeElem(&tmpjep);
      DRETURN(-1);
   }

   /* add all queues referenced in gdil to qlp
    * (necessary for availability of ALL resource limits and tempdir in queue)
    * AND
    * copy all JG_processors from all queues to the JG_processors in gdil
    * (so the execd can decide on which processors (-sets) the job will be run).
    */
   what = lIntVector2What(QU_Type, queue_field);
   lReduceDescr(&rdp, QU_Type, what);

   // @todo CS-1273 YES: We need to deliver the whole GDIL to the master execd (e.g. for building the PE_HOSTFILE)
   //               BUT: Do we need to do the reduce_job_limit for all queues? Wouldn't only the master queue be enough?
   // @todo CS-1273 What happens on the master host of a tightly integrated job?
   //               We first get the SLAVE container, then the master task.
   //               So it already has the job, what does it do with the job sent with the master task?
   //               ==> The data we send here for the master task is simply thrown away, so no need to build it.
   bool is_pe_job = lGetObject(tmpjatep, JAT_pe_object) != nullptr;
   bool is_first_gdil_entry = true;
   for_each_rw_lv(gdil_ep, lGetList(tmpjatep, JAT_granted_destin_identifier_list)) {
      const char *src_qname = lGetString(gdil_ep, JG_qname);
      const lListElem *src_qep = cqueue_list_locate_qinstance(master_cqueue_list, src_qname);

      if (src_qep == nullptr) {
         DRETURN(-1);
      }

      /* copy all JG_processors from all queues to gdil (which will be
       * sent to the execd).
       */
      lSetString(gdil_ep, JG_processors, lGetString(src_qep, QU_processors));

      // copy the referenced queue instance
      qep = lSelectElemDPack(src_qep, nullptr, rdp, what, false, nullptr);
      lSetObject(gdil_ep, JG_queue, qep);

      // build minimum of job request and queue resource limit
      reduce_queue_limits(master_centry_list, gdil_ep, qep, tmpjep, tmpjatep, is_pe_job, is_first_gdil_entry);

      is_first_gdil_entry = false;
   }

   lFreeWhat(&what);
   sge_free(&rdp);

   /*
   ** get credential for job
   */
   if (mconf_get_do_credentials()) {
      cache_sec_cred(sge_root, tmpjep, rhost);
   }

   /*
   ** remove some data not used at execd side
   */
   lSetList(tmpjep, JB_ja_template, nullptr);

   if (init_packbuffer(&pb, 0) != PACK_SUCCESS) {
      lFreeElem(&tmpjep);
      DRETURN(-1);
   }

   pack_job_delivery(&pb, tmpjep);
   lFreeElem(&tmpjep);

   /*
   ** security hook
   */
   tgt2cc(jep, rhost);

   if (simulate_execd) {
      failed = CL_RETVAL_OK;
   } else {
      uint32_t dummymid = 0;
      failed = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, to_cstr(EXECD), 1, rhost,
                                                           master ? ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION : ocs::gdi::ClientServerBase::TAG_SLAVE_ALLOW, &pb, &dummymid);
   }

   /*
   ** security hook
   */
   tgtcclr(jep, rhost);

   clear_packbuffer(&pb);

   /* We will get an acknowledge from execd. But we wait in the main loop so
      that we can handle some requests until the acknowledge arrives.
      This is to make the qmaster non blocking */

   if (failed != CL_RETVAL_OK) {
      /* we failed sending the job to the execd */
      ERROR(MSG_COM_SENDJOBTOHOST_US, lGetUlong(jep, JB_job_number), rhost);
      ERROR("commlib error: %s\n", cl_get_error_text(failed));
      sge_mark_unheard(hep, gdi_session);
      DRETURN(-1);
   } else {
      DPRINTF("successfully sent %sjob " sge_u32 " to host \"%s\"\n", master ? "" : "SLAVE ",
              lGetUlong(jep, JB_job_number), rhost);
   }

   DRETURN(0);
}

static void
sge_job_resend_event_handler_sim_job_end(uint32_t jobid, uint32_t jataskid, lListElem *jep, lListElem *jatep,
                                         monitoring_t *monitor, uint64_t now, int runtime, bool deleted, uint64_t gdi_session) {
   lListElem *ue, *jr = lCreateElem(JR_Type);
   lSetUlong(jr, JR_job_number, jobid);
   lSetUlong(jr, JR_ja_task_number, jataskid);

   const lListElem *ep;
   if ((ep = lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)))) {
      lSetString(jr, JR_queue_name, lGetString(ep, JG_qname));
      if (deleted) {
         lSetUlong(jr, JR_wait_status, SGE_SET_WEXITSTATUS(SGE_WSIGNALED_BIT, SIGKILL)); // was killed
         ue = lAddSubStr(jr, UA_name, "exit_status", JR_usage, UA_Type);
         lSetDouble(ue, UA_value, 137); // 128 + SIGKILL
         lSetUlong(jr, JR_failed, SSTATE_FAILURE_AFTER_JOB);
      } else {
         lSetUlong(jr, JR_wait_status, SGE_SET_WEXITSTATUS(SGE_WEXITED_BIT, 0)); // did exit(0)
      }

      ue = lAddSubStr(jr, UA_name, "submission_time", JR_usage, UA_Type);
      lSetDouble(ue, UA_value, lGetUlong64(jep, JB_submission_time));

      uint64_t start_time = lGetUlong64(jatep, JAT_start_time);
      ue = lAddSubStr(jr, UA_name, "start_time", JR_usage, UA_Type);
      lSetDouble(ue, UA_value, start_time);

      ue = lAddSubStr(jr, UA_name, "end_time", JR_usage, UA_Type);
      lSetDouble(ue, UA_value, now);

      ue = lAddSubStr(jr, UA_name, "ru_wallclock", JR_usage, UA_Type);
      lSetDouble(ue, UA_value, sge_gmt64_to_gmt32(now - start_time));

      ue = lAddSubStr(jr, UA_name, "wallclock", JR_usage, UA_Type);
      lSetDouble(ue, UA_value, sge_gmt64_to_gmt32_double(now - start_time));

      lXchgList(jr, JR_usage, lGetListRef(jatep, JAT_scaled_usage_list));
      ocs::ReportingFileWriter::create_acct_records(nullptr, jr, jep, jatep, false);
      sge_commit_job(jep, jatep, jr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT, monitor, gdi_session);
   }
   lFreeElem(&jr);
}

static int
sge_job_resend_event_handler_sim_job_runtime(uint64_t now, const lListElem *job, const lListElem *ja_task) {
   const lListElem *argv1;
   int runtime = 3;

   argv1 = lFirst(lGetList(job, JB_job_args));
   if (argv1 != nullptr) {
      runtime = atoi(lGetString(argv1, ST_name));
   }

   // We might get here due to a restart of sge_qmaster.
   // Do not re-start with the whole simulated runtime but consider the already passed time.
   uint64_t start_time = lGetUlong64(ja_task, JAT_start_time);
   if (start_time > 0) {
      int passed = sge_gmt64_to_gmt32(now - start_time);
      runtime -= passed;
      if (runtime < 0) {
         runtime = 0;
      }
   }

   return runtime;
}

void
sge_job_resend_event_handler(te_event_t anEvent, monitoring_t *monitor) {
   lListElem *jep, *jatep;
   const lListElem *ep;
   lListElem *hep = nullptr;
   const lListElem *mqep;
   const char *qnm, *hnm;
   uint32_t jobid = te_get_first_numeric_key(anEvent);
   uint32_t jataskid = te_get_second_numeric_key(anEvent);
   lList **master_job_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);

   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   jep = lGetElemUlongRW(*master_job_list, JB_job_number, jobid);
   jatep = job_search_task(jep, nullptr, jataskid);
   uint64_t now = sge_get_gmt64();

   if (jep == nullptr || jatep == nullptr) {
      WARNING(MSG_COM_RESENDUNKNOWNJOB_UU, jobid, jataskid);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      DRETURN_VOID;
   }

   if (mconf_get_simulate_execds()) {
      int runtime = sge_job_resend_event_handler_sim_job_runtime(now, jep, jatep);

      if (ISSET(lGetUlong(jatep, JAT_state), JDELETED)) {
         // job has been deleted
         // delete other timers for this job/jatask
         te_delete_one_time_event(TYPE_JOB_RESEND_EVENT, jobid, jataskid, nullptr);
         // create usage
         sge_job_resend_event_handler_sim_job_end(jobid, jataskid, jep, jatep, monitor, now, runtime, true, ocs::SessionManager::GDI_SESSION_NONE);
      } else {
         if (lGetUlong(jatep, JAT_status) == JTRANSFERING) {
            // transition transferring to running state and set a timer for job end
            sge_commit_job(jep, jatep, nullptr, COMMIT_ST_ARRIVED, COMMIT_DEFAULT, monitor, ocs::SessionManager::GDI_SESSION_NONE);
            trigger_job_resend(now, hep, jobid, jataskid, runtime);
         } else { /* must be JRUNNING */
            // job end, create usage
            sge_job_resend_event_handler_sim_job_end(jobid, jataskid, jep, jatep, monitor, now, runtime, false, ocs::SessionManager::GDI_SESSION_NONE);
         }
      }

      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

      DRETURN_VOID;
   }

   /* check whether a slave execd allowance has to be retransmitted */
   lList *jatasks = lGetListRW(jep, JB_ja_tasks);

   if (lGetUlong(jatep, JAT_status) == JTRANSFERING) {

      ep = lFirst(lGetList(jatep, JAT_granted_destin_identifier_list));

      if (!ep || !(qnm = lGetString(ep, JG_qname)) || !(hnm = lGetHost(ep, JG_qhostname))) {
         ERROR(MSG_JOB_UNKNOWNGDIL4TJ_UU, jobid, jataskid);
         lDelElemUlong(master_job_list, JB_job_number, jobid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      mqep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);

      if (mqep == nullptr) {
         ERROR(MSG_JOB_NOQUEUE4TJ_SUU, qnm, jobid, jataskid);
         lDelElemUlong(&jatasks, JAT_task_number, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if (!(hnm = lGetHost(mqep, QU_qhostname)) || !(hep = host_list_locate(master_ehost_list, hnm))) {
         ERROR(MSG_JOB_NOHOST4TJ_SUU, hnm, jobid, jataskid);
         lDelElemUlong(&jatasks, JAT_task_number, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if (qinstance_state_is_unknown(mqep)) {
         trigger_job_resend(now, hep, jobid, jataskid, 0);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID; /* try later again */
      }

      if (lGetUlong64(jatep, JAT_start_time) > 0) {
         WARNING(MSG_JOB_DELIVER2Q_UUS, jobid, jataskid, lGetString(jatep, JAT_master_queue));
      }

      /* send job to execd */
      sge_give_job(jep, jatep, mqep, hep, monitor, ocs::SessionManager::GDI_SESSION_NONE);

      /* reset timer */
      lSetUlong64(jatep, JAT_start_time, now);

      /* initialize resending of job if not acknowledged by execd */
      trigger_job_resend(now, hep, lGetUlong(jep, JB_job_number),
                         lGetUlong(jatep, JAT_task_number), 0);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

void
cancel_job_resend(uint32_t jid, uint32_t ja_task_id) {
   DENTER(TOP_LAYER);

   DPRINTF("CANCEL JOB RESEND " sge_u32 "/" sge_u32 "\n", jid, ja_task_id);
   te_delete_one_time_event(TYPE_JOB_RESEND_EVENT, jid, ja_task_id, "job-resend_event");

   DRETURN_VOID;
}

/*
 * if hep equals to nullptr, resend is triggered immediately
 */
void
trigger_job_resend(uint64_t now, lListElem *hep, uint32_t jid, uint32_t ja_task_id, int delta) {
   DENTER(TOP_LAYER);

   uint32_t seconds;
   te_event_t ev = nullptr;

   if (mconf_get_simulate_execds()) {
      seconds = delta;
   } else {
      seconds = hep ? MAX(load_report_interval(hep), MAX_JOB_DELIVER_TIME) : 0;
   }
   DPRINTF("TRIGGER JOB RESEND " sge_u32 "/" sge_u32 " in %d seconds\n", jid, ja_task_id, seconds);

   uint64_t when = now + sge_gmt32_to_gmt64(seconds);
   ev = te_new_event(when, TYPE_JOB_RESEND_EVENT, ONE_TIME_EVENT, jid, ja_task_id, "job-resend_event");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/**
 * @brief Recreates timed events for all running jobs in execd simulation mode.
 *
 * When execd simulation is enabled, this function iterates through all jobs and their
 * array tasks in the master job list. For each running task, it calculates the remaining
 * runtime and triggers a job resend event. This is used during qmaster restart
 * to restore the timing state of simulated job executions.
 *
 * @note Only active when mconf_get_simulate_execds() returns true.
 *
 * @see trigger_job_resend()
 * @see sge_job_resend_event_handler()
 */
void
create_timed_events_for_simulated_jobs() {
   DENTER(TOP_LAYER);

   if (mconf_get_simulate_execds()) {
      uint64_t now = sge_get_gmt64();
      lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
      for_each_ep_lv(job, master_job_list) {
         for_each_ep_lv(ja_task, lGetList(job, JB_ja_tasks)) {
            if (ja_task_is_running(ja_task)) {
               uint32_t jobid = lGetUlong(job, JB_job_number);
               uint32_t jataskid = lGetUlong(ja_task, JAT_task_number);
               int runtime = sge_job_resend_event_handler_sim_job_runtime(now, job, ja_task);
               trigger_job_resend(now, nullptr, jobid, jataskid, runtime);
            }
         }
      }
   }

   DRETURN_VOID;
}

/****** sge_give_jobs/sge_commit_job() *****************************************
*  NAME
*     sge_commit_job() -- Do job state transitions
*
*  SYNOPSIS
*     void sge_commit_job(lListElem *jep, lListElem *jatep, lListElem *jr,
*     sge_commit_mode_t mode, sge_commit_flags_t commit_flags)
*
*  FUNCTION
*     sge_commit_job() implements job state transitions. The mode parameter
*     selects which transition:
*
*       COMMIT_ST_SENT             Dispatch order from schedd: send the job
*                                  asynchronously to execd and mark JTRANSFERING.
*       COMMIT_ST_ARRIVED          Job report from execd: mark JRUNNING.
*       COMMIT_ST_FINISHED_FAILED  Tear down an unenrolled array task that
*                                  never ran (callers pair this with
*                                  COMMIT_UNENROLLED_TASK). Log, fire the
*                                  DRMAA finish event, bury. No usage to book,
*                                  no host/queue debits to release.
*       COMMIT_ST_FINISHED_FAILED_EE  GEEE finish: clear host/queue debits,
*                                  emit JOB_FINAL_USAGE events, book usage into
*                                  UU_/PR_/UPP_, bury. Pre-CS-1239 the booking
*                                  was done by the scheduler via the ORT_remove_job
*                                  order and a follow-up COMMIT_ST_DEBITED_EE
*                                  call; CS-1239 folded both into this mode.
*       COMMIT_ST_NO_RESOURCES     Schedd's ORT_remove_immediate_job order: bury
*                                  an interactive job that could not be scheduled.
*       COMMIT_ST_RESCHEDULED /
*         _USER_RESCHEDULED /
*         _FAILED_AND_ERROR        Reset the ja_task back to JIDLE/JQUEUED for
*                                  another scheduling pass; preserve previous usage.
*       COMMIT_ST_DELIVERY_FAILED  Same as RESCHEDULED, but sge_clear_granted_
*                                  resources() must not increase free slots.
*
*     sge_commit_job() spools the affected ja_task / job and emits the matching
*     mirror events. For the finish modes it also burys the ja_task / job (and
*     potentially the whole job, if this was the last ja_task).
*
*  INPUTS
*     lListElem *jep                  - the job
*     lListElem *jatep                - the array task
*     lListElem *jr                   - the job report (may be nullptr)
*     sge_commit_mode_t mode          - the 'mode' - actually the state transition
*     sge_commit_flags_t commit_flags - additional flags for parametrizing
*
*  SEE ALSO
*     See sge_commit_mode_t typedef for documentation on mode.
*     See sge_commit_flags_t typedef for documentation on commit_flags.
*******************************************************************************/
void
sge_commit_job(lListElem *jep, lListElem *jatep, lListElem *jr, sge_commit_mode_t mode,
               int commit_flags, monitoring_t *monitor, uint64_t gdi_session) {
   lListElem *global_host_ep;
   lUlong jobid, jataskid;
   int spool_job = !(commit_flags & COMMIT_NO_SPOOLING);
   int no_events = (commit_flags & COMMIT_NO_EVENTS);
   uint64_t now = sge_get_gmt64();
   const char *session;
   lList *answer_list = nullptr;
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_exechost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);

   /* need hostname for job_log */
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *sge_root = bootstrap_get_sge_root();
   uint64_t task_wallclock = std::numeric_limits<uint64_t>::max();
   bool compute_qwallclock = false;
   uint32_t state = 0;

   DENTER(TOP_LAYER);

   jobid = lGetUlong(jep, JB_job_number);
   jataskid = jatep != nullptr ? lGetUlong(jatep, JAT_task_number) : 0;
   session = lGetString(jep, JB_session);

   const char *pe_name = nullptr;
   const lListElem *pe = nullptr;
   if (jatep != nullptr) {
      pe_name = lGetString(jatep, JAT_granted_pe);
      pe = lGetObject(jatep, JAT_pe_object);
   }

   switch (mode) {
      case COMMIT_ST_SENT: {
         bool master_task = true;
         const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);

         lSetUlong(jatep, JAT_state, JRUNNING);
         lSetUlong(jatep, JAT_status, JTRANSFERING);

         if (!job_get_wallclock_limit(&task_wallclock, jep)) {
            compute_qwallclock = true;
         }

         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_SENT, MSG_QMASTER, qualified_hostname, jr, jep, jatep, nullptr,
                                  MSG_LOG_SENT2EXECD);

         global_host_ep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);
         bool do_per_global_host_booking = true;

         uint32_t ar_id = lGetUlong(jep, JB_ar);
         lListElem *ar = nullptr;
         lListElem *ar_global_host = nullptr;
         if (ar_id != 0) {
            ar = lGetElemUlongRW(master_ar_list, AR_id, ar_id);
            if (ar == nullptr) {
               ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, ar_id, jobid);
               DRETURN_VOID;
            } else {
               ar_global_host = lGetSubHostRW(ar, EH_name, SGE_GLOBAL_NAME, AR_reserved_hosts);
            }
         }

         const char *last_hostname = nullptr;
         const lList *granted_resources_list = lGetList(jatep, JAT_granted_resources_list);
         const lList *gdil = lGetList(jatep, JAT_granted_destin_identifier_list);
         for_each_rw_lv(gdil_ep, gdil) {
            const char *queue_name = lGetString(gdil_ep, JG_qname);
            lListElem *queue = nullptr;

            if ((queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name)) == nullptr) {
               ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, queue_name, jobid);
            } else {
               const char *queue_hostname = lGetHost(queue, QU_qhostname);
               bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);
               const char *limit = nullptr;
               int tmp_slot = lGetUlong(gdil_ep, JG_slots);
               lListElem *host;

               if (compute_qwallclock) {
                  limit = lGetString(queue, QU_h_rt);
                  if (strcasecmp(limit, INFINITY_STR) != 0) {
                     uint32_t clock_val;
                     parse_ulong_val(nullptr, &clock_val, ocs::CEntry::Type::TIME, limit, nullptr, 0);
                     task_wallclock = std::min(task_wallclock, sge_gmt32_to_gmt64(clock_val));
                  } else {
                     task_wallclock = std::min(task_wallclock, std::numeric_limits<uint64_t>::max());
                  }

                  limit = lGetString(queue, QU_s_rt);
                  if (strcasecmp(limit, INFINITY_STR) != 0) {
                     uint32_t clock_val;
                     parse_ulong_val(nullptr, &clock_val, ocs::CEntry::Type::TIME, limit, nullptr, 0);
                     task_wallclock = std::min(task_wallclock, sge_gmt32_to_gmt64(clock_val));
                  } else {
                     task_wallclock = std::min(task_wallclock, std::numeric_limits<uint64_t>::max());
                  }
               }

               /* debit consumable resources */
               if (debit_host_consumable(jep, jatep, granted_resources_list, pe, global_host_ep, master_centry_list, tmp_slot, master_task,
                                         do_per_global_host_booking,
                                         nullptr) > 0) {
                  /* this info is not spooled */
                  sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                                "global", nullptr, nullptr, global_host_ep, gdi_session);
                  ocs::ReportingFileWriter::create_host_consumable_records(&answer_list, global_host_ep, jep, now);
                  answer_list_output(&answer_list);
               }
               host = host_list_locate(master_exechost_list, queue_hostname);
               if (debit_host_consumable(jep, jatep, granted_resources_list, pe, host, master_centry_list, tmp_slot, master_task,
                                         do_per_host_booking, nullptr) > 0) {
                  /* this info is not spooled */
                  sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                                queue_hostname, nullptr, nullptr, host, gdi_session);
                  ocs::ReportingFileWriter::create_host_consumable_records(&answer_list, host, jep, now);
                  answer_list_output(&answer_list);
               }
               qinstance_debit_consumable(queue, jep, pe, master_centry_list, tmp_slot, master_task,
                                          do_per_host_booking, nullptr);
               ocs::ReportingFileWriter::create_queue_consumable_records(&answer_list, host, queue, jep, now);
               answer_list_output(&answer_list);
               /* this info is not spooled */
               qinstance_add_event(queue, sgeE_QINSTANCE_MOD, gdi_session);

               if (ar_id == 0) {
                  /* debit resource quota set */
                  for_each_rw_lv(rqs, master_rqs_list) {
                     if (rqs_debit_consumable(rqs, jep, gdil_ep, pe, master_centry_list,
                                              master_userset_list, master_hgroup_list, tmp_slot, master_task, do_per_host_booking) > 0) {
                        /* this info is not spooled */
                        sge_add_event(0, sgeE_RQS_MOD, 0, 0, lGetString(rqs, RQS_name), nullptr, nullptr, rqs, gdi_session);
                     }
                  }
               } else if (ar != nullptr) {
                  /* debit in advance reservation */
                  lListElem *queue = lGetSubStrRW(ar, QU_full_name, queue_name, AR_reserved_queues);
                  int bookings = 0;
                  bookings += qinstance_debit_consumable(queue, jep, pe, master_centry_list, tmp_slot, master_task,
                                                 do_per_host_booking, nullptr) > 0;
                  if (ar_global_host != nullptr) {
                     bookings += debit_host_consumable(jep, jatep, granted_resources_list, pe, ar_global_host, master_centry_list, tmp_slot, master_task, do_per_global_host_booking, nullptr) > 0;
                  }
                  lListElem *host = lGetSubHostRW(ar, EH_name, queue_hostname, AR_reserved_hosts);
                  if (host != nullptr) {
                     bookings += debit_host_consumable(jep, jatep, granted_resources_list, pe, host, master_centry_list, tmp_slot, master_task, do_per_host_booking, nullptr) > 0;
                  }
                  if (bookings > 0) {
                     /* this info is not spooled */
                     sge_add_event(0, sgeE_AR_MOD, ar_id, 0, nullptr, nullptr, nullptr, ar, gdi_session);
                  }
               }
            }
            master_task = false;
            do_per_global_host_booking = false;
         }

         lSetUlong64(jatep, JAT_wallclock_limit, task_wallclock);

         /*
          * Would be nice if we could use a more accurate start time that could be reported
          * by execd. However this would constrain time synchronization between qmaster and
          * execd host .. sigh!
          */
         lSetUlong64(jatep, JAT_start_time, now);
         sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid, nullptr, nullptr, session,
                         jep, jatep, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
         break;
      }
      case COMMIT_ST_ARRIVED: {
         // change state to running
         lSetUlong(jatep, JAT_status, JRUNNING);

         // spool job log
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_DELIVERED, MSG_QMASTER, qualified_hostname,
                                  jr, jep, jatep, nullptr, MSG_LOG_DELIVERED);

         // spool and notify others about the state change
         sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid, nullptr, nullptr, session,
                   jep, jatep, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
         break;
      }
      case COMMIT_ST_RESCHEDULED:
      case COMMIT_ST_USER_RESCHEDULED:
      case COMMIT_ST_FAILED_AND_ERROR:
         WARNING(MSG_JOB_RESCHEDULE_UU, jobid, jataskid);

         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_RESTART, MSG_QMASTER, qualified_hostname, jr, jep, jatep, nullptr,
                                  SGE_EVENT);
         /* JG: TODO: no accounting record created? Or somewhere else? */
         /* add a reschedule unknown list entry to all slave
            hosts where a part of that job ran */
         {
            if (pe_name != nullptr) {
               if (pe != nullptr && lGetBool(pe, PE_control_slaves)) {
                  bool is_master = true;

                  for_each_ep_lv(granted_queue, lGetList(jatep, JAT_granted_destin_identifier_list)) {
                     if (!is_master) {
                        lListElem *host = host_list_locate(master_exechost_list, lGetHost(granted_queue, JG_qhostname));

                        add_to_reschedule_unknown_list(host, jobid, jataskid, RESCHEDULE_HANDLE_JR_WAIT, gdi_session);

                        DPRINTF("RU: sge_commit_job: granted_queue %s job " sge_u32 "." sge_u32 "\n",
                                lGetString(granted_queue, JG_qname), jobid, jataskid);
                     }
                     is_master = false;
                  }
               }
            } else {
               const lList *granted_list = lGetList(jatep, JAT_granted_destin_identifier_list);
               const lListElem *granted_queue = lFirst(granted_list);
               lListElem *host = host_list_locate(master_exechost_list, lGetHost(granted_queue, JG_qhostname));

               add_to_reschedule_unknown_list(host, jobid, jataskid, RESCHEDULE_SKIP_JR, gdi_session);
            }
         }

         /*
          * Preserve any potential deferred startup request
          * in JAT_state across re-initialization.
          */
         lSetUlong(jatep, JAT_status, JIDLE);
         state = lGetUlong(jatep, JAT_state);
         state &= JDEFERRED_REQ;
         if (mode == COMMIT_ST_RESCHEDULED || mode == COMMIT_ST_USER_RESCHEDULED) {
            state |= JQUEUED | JWAITING;
         } else {
            state |= JQUEUED | JWAITING | JERROR;
         }
         lSetUlong(jatep, JAT_state, state);

         /*
          * Prepare usage lists for the next run
          */
         lSetList(jatep, JAT_previous_usage_list, lCopyList("name", lGetList(jatep, JAT_scaled_usage_list)));
         lSetList(jatep, JAT_scaled_usage_list, nullptr);
         lSetList(jatep, JAT_reported_usage_list, nullptr);

         /*
          * Submit time will be set to the current timestamp in case a user
          * reschedules his own jobs. This prevents users from "stealing" cpu
          * resources. As a result of that the rescheduled job will be put
          * at the end of the pending job list instead of the top.
          *
          * Insert at the end of the pending job list can be disabled by
          * two qmaster params. OLD_RESCHEDULE_BEHAVIOR disables it for non-array jobs
          * whereas OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB disables it for array job tasks.
          */
         if (mode == COMMIT_ST_USER_RESCHEDULED) {
            bool is_array_job = job_is_array(jep);

            if ((!is_array_job && !mconf_get_old_reschedule_behavior()) ||
                (is_array_job && !mconf_get_old_reschedule_behavior_array_job())) {
               lSetUlong64(jep, JB_submission_time, now);
               sge_event_spool(&answer_list, now, sgeE_JOB_MOD, jobid, jataskid,
                               nullptr, nullptr, session, jep, jatep, nullptr, true, true, gdi_session);
            }
         }

         /* sum up the usage of all pe tasks not yet deleted (e.g. tasks from
          * exec host in unknown state). Then remove all pe tasks except the
          * past usage container
          */
         {
            lList *pe_task_list = lGetListRW(jatep, JAT_task_list);

            if (pe_task_list != nullptr) {
               const lListElem *existing_container = lGetElemStr(pe_task_list, PET_id, PE_TASK_PAST_USAGE_CONTAINER);
               lListElem *container = pe_task_sum_past_usage_all(pe_task_list);
               lListElem *pe_task = nullptr;
               lListElem *next = nullptr;

               /* the usage container is not spooled */
               if (existing_container == nullptr) {
                  sge_add_event(now, sgeE_PETASK_ADD, jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER,
                                nullptr, session, container, gdi_session);
               } else {
                  sge_add_list_event(now, sgeE_JOB_USAGE, jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER,
                                     nullptr, session, lGetListRW(container, PET_scaled_usage), gdi_session);
               }

               next = lFirstRW(pe_task_list);
               while ((pe_task = next) != nullptr) {
                  next = lNextRW(next);
                  if (pe_task != container) {
                     lRemoveElem(pe_task_list, &pe_task);
                  }
               }
            }
         }

         sge_clear_granted_resources(jep, jatep, 1, monitor, gdi_session);
         ja_task_clear_finished_pe_tasks(jatep);
         sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid, nullptr, nullptr, session,
                         jep, jatep, nullptr, true, true, gdi_session);

         answer_list_output(&answer_list);
         break;

      case COMMIT_ST_FINISHED_FAILED:
         /* "Tear down an unenrolled array task" - in current callers this mode is
          * always paired with COMMIT_UNENROLLED_TASK, i.e. the ja_task only existed
          * as a pending range entry, was never enrolled, never ran. So there are
          * no host/queue debits to release and no UU_/PR_ usage to book - we just
          * log, fire the DRMAA finish event, and bury. */
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_FINISHED, MSG_QMASTER, qualified_hostname,
                                                   jr, jep, jatep, nullptr, MSG_LOG_EXITED);
         remove_from_reschedule_unknown_lists(jobid, jataskid, gdi_session);
         sge_job_finish_event(jep, jatep, jr, commit_flags, nullptr, gdi_session);
         sge_bury_job(sge_root, jep, jobid, jatep, spool_job, no_events, gdi_session);
         break;
      case COMMIT_ST_FINISHED_FAILED_EE:
         /* CS-1239 consolidation: the legacy two-step (FINISHED_FAILED_EE then,
          * after the scheduler debited usage via ORT_remove_job, DEBITED_EE) is
          * collapsed into one step. The order roundtrip is gone, so the booking
          * and the bury happen here, right after the JFINISHED transition.
          * Eliminates one reporting-log entry, one sgeE_JATASK_MOD event, one
          * jatep spool write that the next instruction would delete, and the
          * duplicated script-delete + successor-release pair (sge_bury_job
          * does both). */
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_FINISHED, MSG_QMASTER, qualified_hostname,
                                                   jr, jep, jatep, nullptr, MSG_LOG_EXITED);
         remove_from_reschedule_unknown_lists(jobid, jataskid, gdi_session);

         lSetUlong(jatep, JAT_status, JFINISHED);

         sge_job_finish_event(jep, jatep, jr, commit_flags, nullptr, gdi_session);

         /* possibly release successor tasks if there are any array dependencies on this task */
         if (jataskid) {
            release_successor_tasks_ad(jep, jataskid, gdi_session);
         }
         sge_clear_granted_resources(jep, jatep, 1, monitor, gdi_session);
         for_each_rw_lv(petask, lGetList(jatep, JAT_task_list)) {
            sge_add_list_event(now, sgeE_JOB_FINAL_USAGE, jobid, jataskid,
                               lGetString(petask, PET_id), nullptr, session,
                               lGetListRW(petask, PET_scaled_usage), gdi_session);
         }
         sge_add_list_event(now, sgeE_JOB_FINAL_USAGE, jobid, jataskid, nullptr, nullptr, session,
                            lGetListRW(jatep, JAT_scaled_usage_list), gdi_session);

         /* Book the finished job's usage into UU_/PR_/UPP_ (was the second
          * chained call from callers). No events or spool here - those are
          * deferred to the TET sharetree-spool handler. */
         sge_book_finished_job_usage(jep, jatep, monitor, gdi_session);

         /* Bury the ja_task / job. sge_bury_job opens its own spool transaction
          * around its writes (spool_delete_script + spool_delete_object) and
          * emits sgeE_JATASK_DEL or sgeE_JOB_DEL. No outer transaction here
          * because BerkeleyDB transactions do not nest. */
         sge_bury_job(sge_root, jep, jobid, jatep, spool_job, no_events, gdi_session);
         break;

      case COMMIT_ST_NO_RESOURCES: /* triggered by ORT_remove_immediate_job */
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_DELETED, MSG_SCHEDD, qualified_hostname,
                                                   jr, jep, jatep, nullptr, MSG_LOG_DELIMMEDIATE);
         sge_job_finish_event(jep, jatep, jr, commit_flags, nullptr, gdi_session);
         sge_bury_job(sge_root, jep, jobid, jatep, spool_job, no_events, gdi_session);
         break;

      case COMMIT_ST_DELIVERY_FAILED:
         /* The same as case COMMIT_ST_RESCHEDULED except sge_clear_granted_resources() may not increase free slots. */
         WARNING(MSG_JOB_RESCHEDULE_UU, jobid, jataskid);
         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_RESTART, MSG_QMASTER, qualified_hostname, jr, jep, jatep, nullptr,
                                  SGE_EVENT);
         lSetUlong(jatep, JAT_status, JIDLE);
         lSetUlong(jatep, JAT_state, JQUEUED | JWAITING);
         sge_clear_granted_resources(jep, jatep, 0, monitor, gdi_session);
         // @todo: Verify: When delivery has been tried, hasn't the JATASK been spooled yet with state JTRANSFERRING?

         sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid,
                         nullptr, nullptr, session, jep, jatep, nullptr, true, false, gdi_session);
         answer_list_output(&answer_list);
         break;
   }

   DRETURN_VOID;
}

/*
 * Job finish information required by DRMAA
 * uint32_t jobid
 * uint32_t taskid
 * uint32_t status
 *    SGE_WEXITED   flag
 *    SGE_WSIGNALED flag
 *    SGE_WCOREDUMP flag
 *    SGE_WABORTED  flag (= COMMIT_NEVER_RAN)
 *    exit status  (8 bit)
 *    signal       (8 bit)
 * lList *rusage;
 *
 * Additional job finish information provided by JAPI
 * char *failed_reason
 */
static void
sge_job_finish_event(lListElem *jep, lListElem *jatep, lListElem *jr, int commit_flags, const char *diagnosis, uint64_t gdi_session) {
   bool release_jr = false;

   DENTER(TOP_LAYER);

   if (!jr) {
      jr = lCreateElem(JR_Type);
      lSetUlong(jr, JR_job_number, lGetUlong(jep, JB_job_number));
      lSetUlong(jr, JR_ja_task_number, lGetUlong(jatep, JAT_task_number));
      lSetUlong(jr, JR_wait_status, SGE_NEVERRAN_BIT);
      release_jr = true;
   } else {
      /* JR_usage gets cleared in the process of cleaning up a finished job.
       * It's contents get put into JAT_usage_list and eventually added to
       * JAT_usage_list.  In the JAPI, however, it would be ugly to have
       * to go picking through the master job list to find the accounting data.
       * So instead we pick through it here and stick it back in JR_usage. */

      lXchgList(jr, JR_usage, lGetListRef(jatep, JAT_usage_list));

      if (commit_flags & COMMIT_NEVER_RAN) {
         lSetUlong(jr, JR_wait_status, SGE_NEVERRAN_BIT);
      }
   }

   if (diagnosis != nullptr) {
      lSetString(jr, JR_err_str, diagnosis);
   } else if (!lGetString(jr, JR_err_str)) {
      if (SGE_GET_NEVERRAN(lGetUlong(jr, JR_wait_status))) {
         lSetString(jr, JR_err_str, "Job never ran");
      } else {
         lSetString(jr, JR_err_str, "Unknown job finish condition");
      }
   }

   sge_add_event(0, sgeE_JOB_FINISH, lGetUlong(jep, JB_job_number),
                 lGetUlong(jatep, JAT_task_number), nullptr, nullptr,
                 lGetString(jep, JB_session), jr, gdi_session);

   if (release_jr) {
      lFreeElem(&jr);
   } else {
      lXchgList(jr, JR_usage, lGetListRef(jatep, JAT_usage_list));
   }

   DRETURN_VOID;
}

static void
release_successor_jobs(const lListElem *jep, uint64_t gdi_session) {
   const lListElem *jid;
   lListElem *suc_jep;
   uint32_t job_ident;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   for_each_ep(jid, lGetList(jep, JB_jid_successor_list)) {
      suc_jep = lGetElemUlongRW(master_job_list, JB_job_number, lGetUlong(jid, JRE_job_number));
      if (suc_jep) {
         /* if we don't find it by job id we try it with the name */
         job_ident = lGetUlong(jep, JB_job_number);
         if (!lDelSubUlong(suc_jep, JRE_job_number, job_ident, JB_jid_predecessor_list)) {

            DPRINTF("no reference " sge_u32 " and %s to job " sge_u32 " in predecessor list of job " sge_u32"\n",
                    job_ident, lGetString(jep, JB_job_name), lGetUlong(suc_jep, JB_job_number), lGetUlong(jep, JB_job_number));
         } else {
            if (lGetList(suc_jep, JB_jid_predecessor_list)) {
               DPRINTF("removed job " sge_u32 "'s dependance from exiting job " sge_u32 "\n",
                       lGetUlong(suc_jep, JB_job_number), lGetUlong(jep, JB_job_number));
            } else {
               DPRINTF("job " sge_u32 "'s job exit triggers start of job " sge_u32 "\n",
                       lGetUlong(jep, JB_job_number), lGetUlong(suc_jep, JB_job_number));
            }
            sge_add_job_event(sgeE_JOB_MOD, suc_jep, nullptr, gdi_session);
         }
      }
   }

   DRETURN_VOID;
}

static void
release_successor_jobs_ad(const lListElem *jep, uint64_t gdi_session) {
   const lListElem *jid;
   lListElem *suc_jep;
   uint32_t job_ident;
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   for_each_ep(jid, lGetList(jep, JB_ja_ad_successor_list)) {
      suc_jep = lGetElemUlongRW(master_job_list, JB_job_number, lGetUlong(jid, JRE_job_number));
      if (suc_jep) {
         int Modified = 0;
         /* if we don't find it by job id we try it with the name */
         job_ident = lGetUlong(jep, JB_job_number);
         if (!lDelSubUlong(suc_jep, JRE_job_number, job_ident, JB_ja_ad_predecessor_list)) {
            DPRINTF("no reference " sge_u32 " and %s to job " sge_u32 " in array predecessor list of job " sge_u32"\n",
                    job_ident, lGetString(jep, JB_job_name),
                    lGetUlong(suc_jep, JB_job_number), lGetUlong(jep, JB_job_number));
         } else {
            if (lGetList(suc_jep, JB_ja_ad_predecessor_list)) {
               DPRINTF("removed job " sge_u32 "'s array dependance from exiting job " sge_u32 "\n",
                       lGetUlong(suc_jep, JB_job_number), lGetUlong(jep, JB_job_number));
            } else {
               DPRINTF("job " sge_u32 "'s job exit triggers release of array tasks in job " sge_u32 "\n",
                       lGetUlong(jep, JB_job_number), lGetUlong(suc_jep, JB_job_number));
            }
            Modified = 1;
         }
         /* cascade unlink and flush ops into a single job mod event */
         if (Modified) {
            /* flush task dependency state for empty predecessors list */
            sge_task_depend_flush(suc_jep, nullptr, gdi_session);
            sge_add_job_event(sgeE_JOB_MOD, suc_jep, nullptr, gdi_session);
         }
      }
   }

   DRETURN_VOID;
}

/*****************************************************************************
 Efficiently release array dependencies based on a completed task of job jep
 *****************************************************************************/
static void
release_successor_tasks_ad(lListElem *jep, uint32_t task_id, uint64_t gdi_session) {
   const lListElem *jid;
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   /* every successor job of this job might have tasks to be released */
   for_each_ep(jid, lGetList(jep, JB_ja_ad_successor_list)) {
      uint32_t job_ident = lGetUlong(jid, JRE_job_number);
      lListElem *suc_range = nullptr;
      lListElem *suc_jep = nullptr;
      int Modified = 0;
      uint32_t bmin, bmax, sb;

      /* JA: should be doing something if this job cannot be located? */
      suc_jep = lGetElemUlongRW(master_job_list, JB_job_number, job_ident);
      if (!suc_jep) continue;

      /* infer reverse dependencies from the completed predecessor task to this successor */
      if (sge_task_depend_get_range(&suc_range, nullptr, suc_jep, jep, task_id)) {
         lFreeElem(&suc_range);
         continue;
      }

      /* fetch successor job dependency range ids */
      range_get_all_ids(suc_range, &bmin, &bmax, &sb);

      /* recalculate task dependency info for this range of tasks */
      for (; bmin <= bmax; bmin += sb) {
         /* returns true if suc_jep was modified */
         if (sge_task_depend_update(suc_jep, nullptr, bmin, gdi_session))
            Modified = 1;
      }

      /* for immediate scheduler reaction event emitting code is required */
      if (Modified)
         sge_add_job_event(sgeE_JOB_MOD, suc_jep, nullptr, gdi_session);

      /* cleanup */
      lFreeElem(&suc_range);
   }

   DRETURN_VOID;
}

/*****************************************************************************
 Clear granted resources for job.
 job->granted_destin_identifier_list->str0 contains the granted queue
 *****************************************************************************/
/****
 **** if incslots is 1, QU_job_slots_used is decreased by the number of
 **** used slots of this job.
 **** if it is 0, QU_job_slots_used is untouched.
 ****/
static void
sge_clear_granted_resources(lListElem *job, lListElem *ja_task, int incslots, monitoring_t *monitor, uint64_t gdi_session) {
   int pe_slots = 0;
   uint32_t job_id = lGetUlong(job, JB_job_number);
   uint32_t ja_task_id = lGetUlong(ja_task, JAT_task_number);
   const lList *gdi_list = lGetList(ja_task, JAT_granted_destin_identifier_list);
   lList *answer_list = nullptr;
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_exechost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   lListElem *rqs = nullptr;
   lListElem *global_host_ep = nullptr;
   bool master_task = true;

   DENTER(TOP_LAYER);

   if (!job_is_ja_task_defined(job, ja_task_id) ||
       !job_is_enrolled(job, ja_task_id)) {
      DRETURN_VOID;
   }

   uint64_t now = sge_get_gmt64();

   /* unsuspend queues on subordinate */
   cqueue_list_x_on_subordinate_gdil(master_cqueue_list, false, gdi_list, monitor, gdi_session);

   global_host_ep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);
   bool do_per_global_host_booking = true;

   const char *pe_name = lGetString(ja_task, JAT_granted_pe);
   lListElem *pe = lGetObject(ja_task, JAT_pe_object);

   uint32_t ar_id = lGetUlong(job, JB_ar);
   lListElem *ar = nullptr;
   lListElem *ar_global_host = nullptr;
   if (ar_id != 0) {
      ar = lGetElemUlongRW(master_ar_list, AR_id, ar_id);
      if (ar == nullptr) {
         ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, ar_id, job_id);
         DRETURN_VOID;
      } else {
         ar_global_host = lGetSubHostRW(ar, EH_name, SGE_GLOBAL_NAME, AR_reserved_hosts);
      }
   }

   /* free granted resources of the queue */
   const char *last_hostname = nullptr;
   const lListElem *gdil_ep;
   for_each_ep(gdil_ep, gdi_list) {
      const char *queue_name = lGetString(gdil_ep, JG_qname);
      lListElem *queue = nullptr;

      if ((queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name)) == nullptr) {
         ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, queue_name, job_id);
      } else {
         const char *queue_hostname = lGetHost(queue, QU_qhostname);
         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);
         int tmp_slot = lGetUlong(gdil_ep, JG_slots);

         /* increase free slots */
         pe_slots += tmp_slot;
         if (incslots) {
            lListElem *host;
            const lList *granted_resources_list = lGetList(ja_task, JAT_granted_resources_list);

            /* undebit consumable resources */
            if (debit_host_consumable(job, ja_task, granted_resources_list, pe, global_host_ep, master_centry_list, -tmp_slot, master_task,
                                      do_per_global_host_booking, nullptr) > 0) {
               /* this info is not spooled */
               sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0, SGE_GLOBAL_NAME, nullptr, nullptr, global_host_ep, gdi_session);
               ocs::ReportingFileWriter::create_host_consumable_records(&answer_list, global_host_ep, job, now);
               answer_list_output(&answer_list);
            }
            host = host_list_locate(master_exechost_list, queue_hostname);
            if (debit_host_consumable(job, ja_task, granted_resources_list, pe, host, master_centry_list, -tmp_slot, master_task,
                                      do_per_host_booking, nullptr) > 0) {
               /* this info is not spooled */
               sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0, queue_hostname, nullptr, nullptr, host, gdi_session);
               ocs::ReportingFileWriter::create_host_consumable_records(&answer_list, host, job, now);
               answer_list_output(&answer_list);
            }
            qinstance_debit_consumable(queue, job, pe, master_centry_list, -tmp_slot, master_task,
                                       do_per_host_booking, nullptr);
            ocs::ReportingFileWriter::create_queue_consumable_records(&answer_list, host, queue, job, now);
            /* this info is not spooled */
            qinstance_add_event(queue, sgeE_QINSTANCE_MOD, gdi_session);

            if (ar_id == 0) {
               /* undebit resource quota set */
               for_each_rw(rqs, master_rqs_list) {
                  DPRINTF("undebiting rqs %s\n", lGetString(rqs, RQS_name));
                  if (rqs_debit_consumable(rqs, job, gdil_ep, pe,
                                           master_centry_list, master_userset_list, master_hgroup_list,
                                           -tmp_slot, master_task, do_per_host_booking) > 0) {
                     /* this info is not spooled */
                     sge_add_event(0, sgeE_RQS_MOD, 0, 0,
                                   lGetString(rqs, RQS_name), nullptr, nullptr, rqs, gdi_session);
                  }
               }
            } else {
               /* undebit in advance reservation */
               lListElem *queue = lGetSubStrRW(ar, QU_full_name, queue_name, AR_reserved_queues);
               int bookings = 0;
               bookings += qinstance_debit_consumable(queue, job, pe, master_centry_list, -tmp_slot, master_task,
                                              do_per_host_booking, nullptr) > 0;
               if (ar_global_host != nullptr) {
                  bookings += debit_host_consumable(job, ja_task, granted_resources_list, pe, ar_global_host, master_centry_list, -tmp_slot, master_task, do_per_global_host_booking, nullptr) > 0;
               }
               lListElem *host = lGetSubHostRW(ar, EH_name, queue_hostname, AR_reserved_hosts);
               if (host != nullptr) {
                  bookings += debit_host_consumable(job, ja_task, granted_resources_list, pe, host, master_centry_list, -tmp_slot, master_task, do_per_host_booking, nullptr) > 0;
               }

               if (bookings > 0) {
                  /* this info is not spooled */
                  sge_add_event(0, sgeE_AR_MOD, ar_id, 0, nullptr, nullptr, nullptr, ar, gdi_session);
               }
            }
         }
      }
      master_task = false;
      do_per_global_host_booking = false;
   }

   /* free granted resources of the parallel environment */
   if (pe_name != nullptr) {
      lListElem *master_pe = pe_list_locate(master_pe_list, pe_name);
      if (master_pe == nullptr) {
         ERROR(MSG_OBJ_UNABLE2FINDPE_S, pe_name);
      } else {
         if (incslots) {
            pe_debit_slots(master_pe, -pe_slots, job_id);
            /* this info is not spooled */
            sge_add_event(0, sgeE_PE_MOD, 0, 0, pe_name, nullptr, nullptr, master_pe, gdi_session);
         }
      }
      lSetString(ja_task, JAT_granted_pe, nullptr);
      lSetObject(ja_task, JAT_pe_object, nullptr);
   }

   /* forget about old tasks */
   lSetList(ja_task, JAT_task_list, nullptr);

   /* remove granted_destin_identifier_list */
   lSetList(ja_task, JAT_granted_destin_identifier_list, nullptr);
   lSetString(ja_task, JAT_master_queue, nullptr);

   DRETURN_VOID;
}

static const char *
get_requested_limit(const lListElem *job, const char *limit_name,
                    bool is_pe_job, bool is_first_gdil_entry, bool master_only) {
   const char *ret = nullptr;

   // if we have requested the limit globally, then use this value
   const lListElem *res = lGetElemStr(job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL), CE_name, limit_name);
   if (res != nullptr) {
      ret = lGetString(res, CE_stringval);
   } else {
      if (is_pe_job) {
         // it is a tightly integrated pe job, we might have master or slave requests
         if (is_first_gdil_entry) {
            // if this is the first gdil entry (master task)
            // we might have only a master task in this gdil (1 slot, job_is_first_task true) => use only master request
            // or a master task and slave tasks => use maximum of master and slave request
            if (master_only) {
               res = lGetElemStr(job_get_hard_resource_list(job, JRS_SCOPE_MASTER), CE_name, limit_name);
               if (res != nullptr) {
                  ret = lGetString(res, CE_stringval);
               }
            } else {
               // we need to compare the possibly existing master and slave request as double in order to compare them
               double master_request = std::numeric_limits<double>::max();
               double slave_request = std::numeric_limits<double>::max();
               const lListElem *master_res = lGetElemStr(job_get_hard_resource_list(job, JRS_SCOPE_MASTER), CE_name, limit_name);
               if (master_res != nullptr) {
                  master_request = lGetDouble(master_res, CE_doubleval);
               }
               const lListElem *slave_res = lGetElemStr(job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), CE_name, limit_name);
               if (slave_res != nullptr) {
                  slave_request = lGetDouble(slave_res, CE_doubleval);
               }

               // pick the maximum
               if (master_request >= slave_request) {
                  if (master_res != nullptr) {
                     ret = lGetString(master_res, CE_stringval);
                  }
               } else {
                  if (slave_res != nullptr) {
                     ret = lGetString(slave_res, CE_stringval);
                  }
               }
            }
         } else {
            // this is a slave host/queue, we might have a slave request
            res = lGetElemStr(job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), CE_name, limit_name);
            if (res != nullptr) {
               ret = lGetString(res, CE_stringval);
            }
         }
      }
   }

   return ret;
}

/* what we do is:
      if there is a hard request for this rlimit then we replace
      the queue's value by the job request

   no need to compare both values - this is schedd's job
*/

static void
reduce_queue_limit(const lList *master_centry_list, lListElem *qep, const lListElem *job,
                   bool is_pe_job, bool is_first_gdil_entry, bool master_only, int nm, const char *rlimit_name) {
   DENTER(BASIS_LAYER);

   const char *request = get_requested_limit(job, rlimit_name, is_pe_job, is_first_gdil_entry, master_only);
   if (request != nullptr) {
      // we know: if the job was scheduled, the job request must have been <= the queue limit,
      // therefore, we can just set the queue limit to the job request
      DPRINTF("job reduces queue limit: %s = %s (was %s)\n", rlimit_name, request, lGetString(qep, nm));
      lSetString(qep, nm, request);
   } else { /* enforce default request if set, but only if the consumable is */
      lListElem *dcep; /*really used to manage resources of this queue, host or globally */
      if ((dcep = centry_list_locate(master_centry_list, rlimit_name)) && lGetUlong(dcep, CE_consumable)) {
         const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
         if (lGetSubStr(qep, CE_name, rlimit_name, QU_consumable_config_list) ||
             lGetSubStr(host_list_locate(master_ehost_list, lGetHost(qep, QU_qhostname)), CE_name, rlimit_name,
                        EH_consumable_config_list) ||
             lGetSubStr(host_list_locate(master_ehost_list, SGE_GLOBAL_NAME), CE_name, rlimit_name,
                        EH_consumable_config_list)) {
            /* managed at queue level, managed at host level or managed at global level */
            lSetString(qep, nm, lGetString(dcep, CE_defaultval));
         }
      }
   }

   DRETURN_VOID;
}

static void
reduce_queue_limits(const lList *master_centry_list, const lListElem *gdil_ep, lListElem *qep, const lListElem *job, const lListElem *ja_task,
                    bool is_pe_job, bool is_first_gdil_entry) {
   bool only_master_task = true;
   if (is_pe_job && is_first_gdil_entry) {
      // if it is a tightly integrated pe job and we are looking at the first gdil_ep
      // we have the master task and optionally slave tasks
      // we want to know if we have only the master task in this gdil_ep
      // this is not the case when
      //    - we have more than one slot in the queue
      //    - pe setting job_is_first_task is false
      const lListElem *pe = lGetObject(ja_task, JAT_pe_object);
      if (lGetUlong(gdil_ep, JG_slots) > 1 ||
         (pe != nullptr && !lGetBool(pe, PE_job_is_first_task))) {
         only_master_task = false;
      }
   }
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_cpu, "s_cpu");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_cpu, "h_cpu");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_core, "s_core");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_core, "h_core");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_data, "s_data");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_data, "h_data");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_stack, "s_stack");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_stack, "h_stack");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_rss, "s_rss");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_rss, "h_rss");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_fsize, "s_fsize");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_fsize, "h_fsize");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_vmem, "s_vmem");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_vmem, "h_vmem");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_s_rt, "s_rt");
   reduce_queue_limit(master_centry_list, qep, job, is_pe_job, is_first_gdil_entry, only_master_task, QU_h_rt, "h_rt");
}

/*-------------------------------------------------------------------------*/
/* CS-1908: sge_bury_job SPLIT into a finish-side pair for retention.       */
/*                                                                         */
/* Before CS-1908 the finish-side actions (signal-resend removal, successor */
/* release, security hook, suser unregister, category detach) and the      */
/* bury-side actions (script unlink, spool delete, sgeE_*_DEL emit, list   */
/* removal) all fired in a single sge_bury_job call at the moment a JAT    */
/* reached its end. Under retention these phases separate in time by up to */
/* finished_jobs_keep_time seconds, so they separate in code too:          */
/*                                                                         */
/*   sge_finish_ja_task -- finish-side. Runs at JAT_end_time whether or    */
/*                         not retention holds the JAT. Idempotent-safe.   */
/*                                                                         */
/*   sge_bury_ja_task   -- bury-side. Runs at prune time from the U5       */
/*                         retention sweep, or immediately after the       */
/*                         finish-side when retention is off.              */
/*                                                                         */
/*   sge_bury_job       -- legacy wrapper. Calls finish-side then bury-    */
/*                         side back-to-back so pre-CS-1908 callers see    */
/*                         identical behaviour.                            */
/*-------------------------------------------------------------------------*/

/** @brief CS-1908 finish-side: actions that must run at JAT_end_time,
 * whether or not retention holds the JAT on master_job_list. Runs from
 * the legacy sge_bury_job wrapper today; under retention U4 calls this
 * directly  and defers sge_bury_ja_task to the U5 retention sweep. */
void
sge_finish_ja_task(const char *sge_root, lListElem *job, uint32_t job_id, lListElem *ja_task, int no_events, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   if (job == nullptr) {
      lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
      job = lGetElemUlongRW(master_job_list, JB_job_number, job_id);
   }

   const uint32_t ja_task_id = lGetUlong(ja_task, JAT_task_number);

   /* Per-JAT cleanup: fires unconditionally on every JAT finish, whether or
    * not this is the last JAT of the JB. */

   // kill any pending resend for this specific JAT
   te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, job_id, ja_task_id, nullptr);

   /* Array-dependency (JB_ja_ad_*) predecessor-list cleanup. Idempotent:
    * lDelSubUlong removes the predecessor entry on the first successful call,
    * subsequent JATs of the same predecessor find nothing to remove and skip
    * the flush. */
   release_successor_jobs_ad(job, gdi_session);

   /* Per-JAT successor-task release (array dependencies keyed on this task).
    * sge_task_depend_get_range reads only the JB dependency structure, not
    * the JAT itself, so timing this at finish is safe. */
   release_successor_tasks_ad(job, ja_task_id, gdi_session);

   /* Whole-JOB done detection: fire JOB-level cleanup when the JB has no
    * more work pending. Two ways this can be true:
    *   (1) This is the only remaining JAT on JB_ja_tasks (pre-CS-1908
    *       semantic: the bury-side has removed the earlier JATs one by one
    *       so this is the last). Preserved for the wrapper flow AND for the
    *       COMMIT_ST_FINISHED_FAILED / COMMIT_ST_NO_RESOURCES paths (KTD8)
    *       where JAT_status is not necessarily JFINISHED.
    *   (2) All enrolled JATs are JFINISHED and no unenrolled tasks remain
    *       (CS-1908 retention semantic: JATs stay on JB_ja_tasks with the
    *       JFINISHED marker; the whole array is done when every remaining
    *       JAT is JFINISHED and no pending tasks are left).
    * Under retention U4 sets JFINISHED on the current JAT BEFORE calling
    * finish-side, mirroring the pre-CS-1908 ordering at sge_give_jobs.cc
    * :1261 → :1287, so (2) sees the current JAT as JFINISHED already. */
   bool job_done = (job_get_ja_tasks(job) == 1);
   if (!job_done && job_get_not_enrolled_ja_tasks(job) == 0) {
      job_done = true;
      for_each_ep_lv(iter_jat, lGetList(job, JB_ja_tasks)) {
         if (lGetUlong(iter_jat, JAT_status) != JFINISHED) {
            job_done = false;
            break;
         }
      }
   }

   if (job_done) {
      release_successor_jobs(job, gdi_session);

      // security hook
      delete_credentials(sge_root, job);

      // release the SUSER slot the JB held
      const lList *master_suser_list = *ocs::DataStore::get_master_list(SGE_TYPE_SUSER);
      suser_unregister_job(job, master_suser_list);

      // update the category. A retained-JFINISHED JAT no longer occupies a
      // category slot from the scheduler's perspective; the R16a audit
      // records this decision.
      lList **master_category_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);
      ocs::CategoryQmaster::detach_job(master_category_list, job, !no_events, gdi_session);
   }

   DRETURN_VOID;
}

/** @brief CS-1908 bury-side: remove the JAT from master_job_list, unlink its
 * spool entry, emit the DEL event. When this is the last JAT on the JB, also
 * unlink the job script and exec file and remove the whole JB. Runs from the
 * legacy sge_bury_job wrapper today; under retention U5's sweep calls this
 * directly. Idempotent on already-removed JATs is not guaranteed — callers
 * must ensure the JAT is on the list when invoking. */
void
sge_bury_ja_task(const char *sge_root, lListElem *job, uint32_t job_id, lListElem *ja_task, int spool_job, int no_events, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   if (job == nullptr) {
      job = lGetElemUlongRW(master_job_list, JB_job_number, job_id);
   }

   const uint32_t ja_task_id = lGetUlong(ja_task, JAT_task_number);
   if (const bool remove_job = (!ja_task || job_get_ja_tasks(job) == 1); remove_job) {
      /*
       * do not try to remove script file for interactive jobs
       */
      {
         lList *answer_list = nullptr;
         dstring buffer = DSTRING_INIT;

         spool_transaction(&answer_list, spool_get_default_context(), STC_begin);

         if ((lGetString(job, JB_exec_file) != nullptr)) {
            PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
            if (!JOB_TYPE_IS_BINARY(lGetUlong(job, JB_type))) {
               spool_delete_script(&answer_list, lGetUlong(job, JB_job_number), job);
            }
            lSetString(job, JB_exec_file, nullptr);
            PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
         }

         spool_delete_object(&answer_list, spool_get_default_context(), SGE_TYPE_JOB,
                             job_get_key(job_id, 0, nullptr, &buffer), true);
         answer_list_output(&answer_list);
         sge_dstring_free(&buffer);

         spool_transaction(&answer_list, spool_get_default_context(), STC_commit);
      }

      // send events
      if (!no_events) {
         sge_add_event(0, sgeE_JOB_DEL, job_id, ja_task_id,
                       nullptr, nullptr, lGetString(job, JB_session), nullptr, gdi_session);

      }

      // final job removal
      lRemoveElem(master_job_list, &job);
   } else {
      int is_enrolled = job_is_enrolled(job, ja_task_id);

      if (!no_events) {
         sge_add_event(0, sgeE_JATASK_DEL, job_id, ja_task_id,
                       nullptr, nullptr, lGetString(job, JB_session), nullptr, gdi_session);
      }

      /*
       * remove the task
       */
      if (is_enrolled) {
         lList *answer_list = nullptr;
         dstring buffer = DSTRING_INIT;
         spool_delete_object(&answer_list, spool_get_default_context(),
                             SGE_TYPE_JOB, job_get_key(job_id, ja_task_id, nullptr, &buffer), true);
         answer_list_output(&answer_list);
         sge_dstring_free(&buffer);
         lRemoveElem(lGetListRW(job, JB_ja_tasks), &ja_task);
      } else {
         job_delete_not_enrolled_ja_task(job, nullptr, ja_task_id);
         if (spool_job) {
            lList *answer_list = nullptr;
            dstring buffer = DSTRING_INIT;
            spool_write_object(&answer_list, spool_get_default_context(), job,
                               job_get_key(job_id, ja_task_id, nullptr, &buffer), SGE_TYPE_JOB, true);
            answer_list_output(&answer_list);
            sge_dstring_free(&buffer);
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Legacy wrapper. Existing callers of sge_bury_job see the finish-side
 * and bury-side run back-to-back, which is identical to pre-CS-1908 behaviour.
 * New callers under retention (U4 finish path, U5 sweep) call the two split
 * functions independently. */
static void
sge_bury_job(const char *sge_root, lListElem *job, uint32_t job_id, lListElem *ja_task, int spool_job, int no_events, uint64_t gdi_session) {
   sge_finish_ja_task(sge_root, job, job_id, ja_task, no_events, gdi_session);
   sge_bury_ja_task(sge_root, job, job_id, ja_task, spool_job, no_events, gdi_session);
}

/****** sge_give_jobs/copyJob() ************************************************
*  NAME
*     copyJob() -- copy the job with only the specified ja task
*
*  SYNOPSIS
*     static lListElem* copyJob(lListElem *job, lListElem *ja_task)
*
*  FUNCTION
*     copy the job with only the specified ja task
*
*  INPUTS
*     lListElem *job     - job structure
*     lListElem *ja_task - ja-task (template)
*
*  RESULT
*     static lListElem* -  nullptr, or new job structure
*
*  NOTES
*     MT-NOTE: copyJob() is MT safe
*
*******************************************************************************/
static lListElem *
copyJob(lListElem *job, lListElem *ja_task) {
   lListElem *job_copy = nullptr;
   const lListElem *ja_task_copy = nullptr;
   lList *tmp_ja_task_list = nullptr;

   DENTER(TOP_LAYER);

   /* create a copy of the job */
   lXchgList(job, JB_ja_tasks, &tmp_ja_task_list);
   job_copy = lCopyElem(job);
   lXchgList(job, JB_ja_tasks, &tmp_ja_task_list);

   if (job_copy != nullptr) { /* not enough memory */

      ja_task_copy = lGetElemUlong(lGetList(job, JB_ja_tasks), JAT_task_number,
                                   lGetUlong(ja_task, JAT_task_number));

      tmp_ja_task_list = lCreateList("cp_ja_task_list", lGetElemDescr(ja_task_copy));
      lAppendElem(tmp_ja_task_list, lCopyElem(ja_task_copy));
      lSetList(job_copy, JB_ja_tasks, tmp_ja_task_list);

      if (lGetNumberOfElem(tmp_ja_task_list) == 0) { /* not enough memory */
         lFreeElem(&job_copy); /* this also frees tmp_ja_task_list */
      }
   }

   DRETURN(job_copy);
}


/****** sge_give_jobs/setCheckpointObj() ***************************************
*  NAME
*     setCheckpointObj() -- sets the job checkpointing name
*
*  SYNOPSIS
*     static int setCheckpointObj(lListElem *job)
*
*  FUNCTION
*     sets the job checkpointing name
*
*  INPUTS
*     lListElem *job - job to modify
*
*  RESULT
*     static int -  0 : everything went find
*                  -1 : something went wrong
*
*  NOTES
*     MT-NOTE: setCheckpointObj() is not MT safe
*
*******************************************************************************/
static int
setCheckpointObj(lListElem *job) {
   const lListElem *ckpt = nullptr;
   lListElem *tmp_ckpt = nullptr;
   const char *ckpt_name = nullptr;
   int ret = 0;
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);

   DENTER(TOP_LAYER);

   if ((ckpt_name = lGetString(job, JB_checkpoint_name))) {
      ckpt = ckpt_list_locate(master_ckpt_list, ckpt_name);
      if (!ckpt) {
         ERROR(MSG_OBJ_UNABLE2FINDCKPT_S, ckpt_name);
         ret = -1;
      }

         /* get the necessary memory */
      else if (!(tmp_ckpt = lCopyElem(ckpt))) {
         ERROR(MSG_OBJ_UNABLE2CREATECKPT_SU, ckpt_name, lGetUlong(job, JB_job_number));
         ret = -1;
      } else {
         /* everything's in place. stuff it in */
         lSetObject(job, JB_checkpoint_object, tmp_ckpt);
      }
   }
   DRETURN(ret);
}

bool
gdil_del_all_orphaned(const lList *gdil_list, lList **alpp, uint64_t gdi_session) {
   bool ret = true;
   const lListElem *gdil_ep;
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

   dstring cqueue_name = DSTRING_INIT;
   dstring host_name = DSTRING_INIT;
   bool has_hostname, has_domain;
   for_each_ep(gdil_ep, gdil_list) {
      cqueue_name_split(lGetString(gdil_ep, JG_qname), &cqueue_name, &host_name, &has_hostname, &has_domain);
      ret &= cqueue_list_del_all_orphaned(master_cqueue_list, alpp, sge_dstring_get_string(&cqueue_name),
                                          sge_dstring_get_string(&host_name), gdi_session);
   }
   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_name);

   return ret;
}

