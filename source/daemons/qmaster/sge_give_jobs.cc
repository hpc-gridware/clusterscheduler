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

/** @file
 * @brief Handing a job to an execution host, and burying it when it ends
 */
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
#include "cull/ocs_cull_descr.h"

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
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JR.h"

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
/** @brief Send a job to the execution host that was granted it
 * @param jep the job (`JB_Type`)
 * @param jatep the array task
 * @param master_qep the master queue instance
 * @param hep the execution host
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 * @return 0 on success
 */
int
sge_give_job(lListElem *jep, lListElem *jatep, const lListElem *master_qep, lListElem *hep, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const char *rhost;
   int ret = 0;
   int sent_slaves = 0;

   rhost = ocs::tom::get<QU_qhostname>(master_qep);
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
      ocs::tom::set<JAT_next_pe_task_id>(jatep, 1);
      ret = send_job(rhost, jep, jatep, hep, 1, gdi_session);
      MONITOR_MESSAGES_OUT(monitor);
   }

   DRETURN(ret);
}


/**
 * @brief Send out slave tasks of a pe job
 *
 * It prepares the data for the sending out the pe slaves. Once that data
 * is created, it calles the actual send_slave_method.
 *
 * @param jep job structure
 * @param jatep ja-taks (template, not the actual one)
 *
 * @return 1 : no pe job 0 : send slaves -1 : something went wrong
 *
 * @note MT-NOTE: send_slave_jobs() is not MT safe
 */
static int
send_slave_jobs(lListElem *jep, lListElem *jatep, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   lListElem *tmpjep, *qep;
   int ret = 0;
   bool is_pe_jobs = false;
   const lDescr *rdp = nullptr;
   lEnumeration *what;
   const lList *master_centry_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   // do we still have pe slave tasks to be delivered?
   for (ocs::tom::Element<ocs::tom::JG> gdil_ep : ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) {
      if (ocs::tom::get<JG_tag_slave_job>(gdil_ep)) {
         ocs::tom::set<JAT_next_pe_task_id>(jatep, 1);
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

   ocs::tom::Element<ocs::tom::JAT> tmpjatep = lFirstRW(ocs::tom::get<JB_ja_tasks>(tmpjep));
   bool is_pe_job = static_cast<lObject>(ocs::tom::get<JAT_pe_object>(tmpjatep)) != nullptr;
   bool is_first_gdil_entry = true;
   for (ocs::tom::Element<ocs::tom::JG> gdil_ep : ocs::tom::get<JAT_granted_destin_identifier_list>(tmpjatep)) {
      const char *src_qname = ocs::tom::get<JG_qname>(gdil_ep);
      const lListElem *src_qep = cqueue_list_locate_qinstance(master_cqueue_list, src_qname);

      if (src_qep == nullptr) {
         DRETURN(-1);
      }

      /* copy all JG_processors from all queues to gdil (which will be
       * sent to the execd).
       */
      ocs::tom::set<JG_processors>(gdil_ep, ocs::tom::get<QU_processors>(src_qep));

      // copy the referenced queue instance
      qep = lSelectElemDPack(src_qep, nullptr, rdp, what, false, nullptr);
      ocs::tom::set<JG_queue>(gdil_ep, qep);

      // build minimum of job request and queue resource limit
      reduce_queue_limits(master_centry_list, gdil_ep, qep, tmpjep, tmpjatep, is_pe_job, is_first_gdil_entry);

      is_first_gdil_entry = false;
   }

   lFreeWhat(&what);
   ocs_cull_descr_release(&rdp);

   ret = send_slave_jobs_wc(tmpjep, monitor, gdi_session);

   lFreeElem(&tmpjep);

   DRETURN(ret);
}

/**
 * @brief Takes the prepared data end sends it out
 *
 * This is a helper function of send_slave_jobs. It handles the actual send.
 *
 * @param tmpjep prepared job structure
 * @param jatep ja-taks (most likely the templete)
 * @param pe target pe
 * @param qlp prepared queue instance list
 *
 * @return 0 : everything is fine -1 : an error
 *
 * @note MT-NOTE: send_slave_jobs_wc() is not MT safe
 */
static int
send_slave_jobs_wc(lListElem *jep, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   lList *saved_gdil = nullptr;
   lList *gdil;
   lListElem *gdil_ep = nullptr;
   lListElem *next_gdil_ep = nullptr;
   ocs::tom::Element<ocs::tom::JAT> jatep = lFirstRW(ocs::tom::get<JB_ja_tasks>(jep));
   int ret = 0;
   int failed = CL_RETVAL_OK;
   lList *master_ehost_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);

   bool simulate_execd = mconf_get_simulate_execds();

   gdil = saved_gdil = lCreateList("", JG_Type);
   ocs::tom::exchange<JAT_granted_destin_identifier_list>(jatep, &saved_gdil);

   next_gdil_ep = lFirstRW(saved_gdil);
   while ((gdil_ep = next_gdil_ep)) {
      lListElem *hep;
      const char *hostname = nullptr;
      unsigned long last_heard_from;

      next_gdil_ep = lNextRW(gdil_ep);

      // when we are re-sending: skip the already acknowledged ones
      if (ocs::tom::get<JG_tag_slave_job>(gdil_ep) == 0) {
         continue;
      }

      hostname = ocs::tom::get<JG_qhostname>(gdil_ep);
      hep = host_list_locate(master_ehost_list, hostname);
      if (hep == nullptr) {
         ret = -1;
         break;
      }

      if (!simulate_execd) {
         /* do ask_commproc() only if we are missing load reports */
         cl_commlib_get_last_message_time(cl_com_get_handle(to_cstr(QMASTER), 0),
                                          hostname, to_cstr(EXECD), 1, &last_heard_from);
         if (sge_gmt32_to_gmt64(last_heard_from) + host_unheard_timeout(hep) <= sge_get_gmt64()) {

            ERROR(MSG_COM_CANT_DELIVER_UNHEARD_SSU, to_cstr(EXECD), hostname,  ocs::tom::get<JB_job_number>(jep));
            sge_mark_unheard(hep, gdi_session);
            ret = -1;
            break;
         }
      }

      lDechainElem(saved_gdil, gdil_ep);
      lAppendElem(gdil, gdil_ep);

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
      if (failed != CL_RETVAL_OK) {
         /* we failed to send the job to the execd */
         ERROR(MSG_COM_SENDJOBTOHOST_US, ocs::tom::get<JB_job_number>(jep), hostname);
         ERROR("commlib error: %s\n", cl_get_error_text(failed));
         sge_mark_unheard(hep, gdi_session);
         ret = -1;
         break;
      } else {
         DPRINTF("successfully sent slave job " sge_u32 " to host \"%s\"\n", ocs::tom::get<JB_job_number>(jep), hostname);
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
   const char *myprogname = component_get_component_name();
   bool simulate_execd = mconf_get_simulate_execds();
   const lDescr *rdp = nullptr;
   lEnumeration *what;
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   if (!simulate_execd) {
      cl_commlib_get_last_message_time(cl_com_get_handle(myprogname, 0), rhost, to_cstr(EXECD), 1,
                                       &last_heard_from);
      uint64_t now = sge_get_gmt64();
      if (sge_gmt32_to_gmt64(last_heard_from) + host_unheard_timeout(hep) <= now) {
         ERROR(MSG_COM_CANT_DELIVER_UNHEARD_SSU, to_cstr(EXECD), rhost,  ocs::tom::get<JB_job_number>(jep));
         sge_mark_unheard(hep, gdi_session);
         DRETURN(-1);
      }
   }
   // @todo: we do lots of things *with* simulate_execd, why? Just create the timed event.

   if ((tmpjep = copyJob(jep, jatep)) == nullptr) {
      DRETURN(-1);
   } else {
      tmpjatep = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(tmpjep)));
   }

   /* load script into job structure for sending to execd */
   /*
   ** if exec_file is not set, then this is an interactive job
   */
   if (master && ocs::tom::get<JB_exec_file>(tmpjep) && !JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(jep))) {
      if (!spool_read_script(nullptr, ocs::tom::get<JB_job_number>(tmpjep), tmpjep)) {
         lFreeElem(&tmpjep);
         DRETURN(-1);
      }
   }

   /* if a granted resource list is defined int must be passed through, too */
   if (ocs::tom::get<JAT_granted_resources_list>(jatep)) {
      ocs::tom::ConstList<ocs::tom::GRU> grl = ocs::tom::get<JAT_granted_resources_list>(jatep);
      ocs::tom::set<JAT_granted_resources_list>(tmpjatep, lCopyList(nullptr, grl));
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
   bool is_pe_job = static_cast<lObject>(ocs::tom::get<JAT_pe_object>(tmpjatep)) != nullptr;
   bool is_first_gdil_entry = true;
   for (ocs::tom::Element<ocs::tom::JG> gdil_ep : ocs::tom::get<JAT_granted_destin_identifier_list>(tmpjatep)) {
      const char *src_qname = ocs::tom::get<JG_qname>(gdil_ep);
      const lListElem *src_qep = cqueue_list_locate_qinstance(master_cqueue_list, src_qname);

      if (src_qep == nullptr) {
         DRETURN(-1);
      }

      /* copy all JG_processors from all queues to gdil (which will be
       * sent to the execd).
       */
      ocs::tom::set<JG_processors>(gdil_ep, ocs::tom::get<QU_processors>(src_qep));

      // copy the referenced queue instance
      qep = lSelectElemDPack(src_qep, nullptr, rdp, what, false, nullptr);
      ocs::tom::set<JG_queue>(gdil_ep, qep);

      // build minimum of job request and queue resource limit
      reduce_queue_limits(master_centry_list, gdil_ep, qep, tmpjep, tmpjatep, is_pe_job, is_first_gdil_entry);

      is_first_gdil_entry = false;
   }

   lFreeWhat(&what);
   ocs_cull_descr_release(&rdp);

   /*
   ** remove some data not used at execd side
   */
   ocs::tom::set<JB_ja_template>(tmpjep, nullptr);

   if (init_packbuffer(&pb, 0) != PACK_SUCCESS) {
      lFreeElem(&tmpjep);
      DRETURN(-1);
   }

   pack_job_delivery(&pb, tmpjep);
   lFreeElem(&tmpjep);

   if (simulate_execd) {
      failed = CL_RETVAL_OK;
   } else {
      uint32_t dummymid = 0;
      failed = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, to_cstr(EXECD), 1, rhost,
                                                           master ? ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION : ocs::gdi::ClientServerBase::TAG_SLAVE_ALLOW, &pb, &dummymid);
   }

   clear_packbuffer(&pb);

   /* We will get an acknowledge from execd. But we wait in the main loop so
      that we can handle some requests until the acknowledge arrives.
      This is to make the qmaster non blocking */

   if (failed != CL_RETVAL_OK) {
      /* we failed sending the job to the execd */
      ERROR(MSG_COM_SENDJOBTOHOST_US, ocs::tom::get<JB_job_number>(jep), rhost);
      ERROR("commlib error: %s\n", cl_get_error_text(failed));
      sge_mark_unheard(hep, gdi_session);
      DRETURN(-1);
   } else {
      DPRINTF("successfully sent %sjob " sge_u32 " to host \"%s\"\n", master ? "" : "SLAVE ",
              ocs::tom::get<JB_job_number>(jep), rhost);
   }

   DRETURN(0);
}

static void
sge_job_resend_event_handler_sim_job_end(uint32_t jobid, uint32_t jataskid, lListElem *jep, lListElem *jatep,
                                         monitoring_t *monitor, uint64_t now, int runtime, bool deleted, uint64_t gdi_session) {
   auto jr = ocs::tom::ElementPtr<ocs::tom::JR>::create();
   ocs::tom::set<JR_job_number>(jr, jobid);
   ocs::tom::set<JR_ja_task_number>(jr, jataskid);

   ocs::tom::ConstElement<ocs::tom::JG> ep;
   if ((ep = lFirst(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)))) {
      ocs::tom::set<JR_queue_name>(jr, ocs::tom::get<JG_qname>(ep));
      if (deleted) {
         ocs::tom::set<JR_wait_status>(jr, SGE_SET_WEXITSTATUS(SGE_WSIGNALED_BIT, SIGKILL)); // was killed
         ocs::tom::Element<ocs::tom::UA> ue = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "exit_status");
         ocs::tom::set<UA_value>(ue, 137); // 128 + SIGKILL
         ocs::tom::set<JR_failed>(jr, SSTATE_FAILURE_AFTER_JOB);
      } else {
         ocs::tom::set<JR_wait_status>(jr, SGE_SET_WEXITSTATUS(SGE_WEXITED_BIT, 0)); // did exit(0)
      }

      ocs::tom::Element<ocs::tom::UA> ue = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "submission_time");
      ocs::tom::set<UA_value>(ue, ocs::tom::get<JB_submission_time>(jep));

      uint64_t start_time = ocs::tom::get<JAT_start_time>(jatep);
      ocs::tom::Element<ocs::tom::UA> ue_ua = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "start_time");
      ocs::tom::set<UA_value>(ue_ua, start_time);

      ocs::tom::Element<ocs::tom::UA> ue_ua_2 = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "end_time");
      ocs::tom::set<UA_value>(ue_ua_2, now);

      ocs::tom::Element<ocs::tom::UA> ue_ua_3 = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "ru_wallclock");
      ocs::tom::set<UA_value>(ue_ua_3, sge_gmt64_to_gmt32(now - start_time));

      ocs::tom::Element<ocs::tom::UA> ue_ua_4 = ocs::tom::emplace_back_in<JR_usage, UA_name>(jr, "wallclock");
      ocs::tom::set<UA_value>(ue_ua_4, sge_gmt64_to_gmt32_double(now - start_time));

      ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<JAT_scaled_usage_list>(jatep));
      ocs::ReportingFileWriter::create_acct_records(nullptr, *jr, jep, jatep, false);
      sge_commit_job(jep, jatep, *jr, COMMIT_ST_FINISHED_FAILED_EE, COMMIT_DEFAULT, monitor, gdi_session);
   }
}

static int
sge_job_resend_event_handler_sim_job_runtime(uint64_t now, const lListElem *job, const lListElem *ja_task) {
   int runtime = 3;

   ocs::tom::ConstElement<ocs::tom::ST> argv1 = lFirst(ocs::tom::get<JB_job_args>(job));
   if (argv1 != nullptr) {
      runtime = atoi(ocs::tom::get<ST_name>(argv1));
   }

   // We might get here due to a restart of sge_qmaster.
   // Do not re-start with the whole simulated runtime but consider the already passed time.
   uint64_t start_time = ocs::tom::get<JAT_start_time>(ja_task);
   if (start_time > 0) {
      int passed = sge_gmt64_to_gmt32(now - start_time);
      runtime -= passed;
      if (runtime < 0) {
         runtime = 0;
      }
   }

   return runtime;
}

/** @brief Redeliver a job whose execution host never confirmed it
 *
 * The handler for #TYPE_JOB_RESEND_EVENT.
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 */
void
sge_job_resend_event_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   lListElem *jatep;
   lListElem *hep = nullptr;
   const lListElem *mqep;
   const char *qnm, *hnm;
   uint32_t jobid = te_get_first_numeric_key(anEvent);
   uint32_t jataskid = te_get_second_numeric_key(anEvent);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   lList **master_job_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);

   ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(*master_job_list, jobid);
   jatep = job_search_task(jep, nullptr, jataskid);
   uint64_t now = sge_get_gmt64();

   if (jep == nullptr || jatep == nullptr) {
      WARNING(MSG_COM_RESENDUNKNOWNJOB_UU, jobid, jataskid);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      DRETURN_VOID;
   }

   if (mconf_get_simulate_execds()) {
      int runtime = sge_job_resend_event_handler_sim_job_runtime(now, jep, jatep);

      if (ISSET(ocs::tom::get<JAT_state>(jatep), JDELETED)) {
         // job has been deleted
         // delete other timers for this job/jatask
         te_delete_one_time_event(TYPE_JOB_RESEND_EVENT, jobid, jataskid, nullptr);
         // create usage
         sge_job_resend_event_handler_sim_job_end(jobid, jataskid, jep, jatep, monitor, now, runtime, true, ocs::SessionManager::GDI_SESSION_NONE);
      } else {
         if (ocs::tom::get<JAT_status>(jatep) == JTRANSFERING) {
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
   lList *jatasks = ocs::tom::get<JB_ja_tasks>(jep);

   if (ocs::tom::get<JAT_status>(jatep) == JTRANSFERING) {

      ocs::tom::ConstElement<ocs::tom::JG> ep = lFirst(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep));

      if (!ep || !(qnm = ocs::tom::get<JG_qname>(ep)) || !(hnm = ocs::tom::get<JG_qhostname>(ep))) {
         ERROR(MSG_JOB_UNKNOWNGDIL4TJ_UU, jobid, jataskid);
         ocs::tom::erase<JB_job_number>(master_job_list, jobid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      mqep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);

      if (mqep == nullptr) {
         ERROR(MSG_JOB_NOQUEUE4TJ_SUU, qnm, jobid, jataskid);
         ocs::tom::erase<JAT_task_number>(&jatasks, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if (!(hnm = ocs::tom::get<QU_qhostname>(mqep)) || !(hep = host_list_locate(master_ehost_list, hnm))) {
         ERROR(MSG_JOB_NOHOST4TJ_SUU, hnm, jobid, jataskid);
         ocs::tom::erase<JAT_task_number>(&jatasks, jataskid);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID;
      }

      if (qinstance_state_is_unknown(mqep)) {
         trigger_job_resend(now, hep, jobid, jataskid, 0);
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         DRETURN_VOID; /* try later again */
      }

      if (ocs::tom::get<JAT_start_time>(jatep) > 0) {
         WARNING(MSG_JOB_DELIVER2Q_UUS, jobid, jataskid, ocs::tom::get<JAT_master_queue>(jatep));
      }

      /* send job to execd */
      sge_give_job(jep, jatep, mqep, hep, monitor, ocs::SessionManager::GDI_SESSION_NONE);

      /* reset timer */
      ocs::tom::set<JAT_start_time>(jatep, now);

      /* initialize resending of job if not acknowledged by execd */
      trigger_job_resend(now, hep, ocs::tom::get<JB_job_number>(jep),
                         ocs::tom::get<JAT_task_number>(jatep), 0);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

/** @brief Stop resending a job to its execution host
 *
 * Called once the host confirms the job, or once the job is gone.
 *
 * @param jid the job
 * @param ja_task_id the array task
 */
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
/** @brief Arm a timer to deliver a job again if it is still unconfirmed
 *
 * Delivery is not acknowledged synchronously, so an unconfirmed job is simply
 * sent again after @p delta seconds until the host accepts it or the job is
 * cancelled.
 *
 * @param now the current time
 * @param hep the execution host, or nullptr when it is not known yet
 * @param jid the job
 * @param ja_task_id the array task
 * @param delta seconds to wait before the next attempt
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
         for (ocs::tom::ConstElement<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            if (ja_task_is_running(ja_task)) {
               uint32_t jobid = ocs::tom::get<JB_job_number>(job);
               uint32_t jataskid = ocs::tom::get<JAT_task_number>(ja_task);
               int runtime = sge_job_resend_event_handler_sim_job_runtime(now, job, ja_task);
               trigger_job_resend(now, nullptr, jobid, jataskid, runtime);
            }
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Do job state transitions
 *
 * sge_commit_job() implements job state transitions. The mode parameter
 * selects which transition:
 *   COMMIT_ST_SENT             Dispatch order from schedd: send the job
 *                              asynchronously to execd and mark JTRANSFERING.
 *   COMMIT_ST_ARRIVED          Job report from execd: mark JRUNNING.
 *   COMMIT_ST_FINISHED_FAILED  Tear down an unenrolled array task that
 *                              never ran (callers pair this with
 *                              COMMIT_UNENROLLED_TASK). Log, fire the
 *                              DRMAA finish event, bury. No usage to book,
 *                              no host/queue debits to release.
 *   COMMIT_ST_FINISHED_FAILED_EE  GEEE finish: clear host/queue debits,
 *                              emit JOB_FINAL_USAGE events, book usage into
 *                              UU_/PR_/UPP_, bury. Pre-CS-1239 the booking
 *                              was done by the scheduler via the ORT_remove_job
 *                              order and a follow-up COMMIT_ST_DEBITED_EE
 *                              call; CS-1239 folded both into this mode.
 *   COMMIT_ST_NO_RESOURCES     Schedd's ORT_remove_immediate_job order: bury
 *                              an interactive job that could not be scheduled.
 *   COMMIT_ST_RESCHEDULED /
 *     _USER_RESCHEDULED /
 *     _FAILED_AND_ERROR        Reset the ja_task back to JIDLE/JQUEUED for
 *                              another scheduling pass; preserve previous usage.
 *   COMMIT_ST_DELIVERY_FAILED  Same as RESCHEDULED, but sge_clear_granted_
 *                              resources() must not increase free slots.
 * sge_commit_job() spools the affected ja_task / job and emits the matching
 * mirror events. For the finish modes it also burys the ja_task / job (and
 * potentially the whole job, if this was the last ja_task).
 *
 * @param jep the job
 * @param jatep the array task
 * @param jr the job report (may be nullptr)
 * @param mode the 'mode' - actually the state transition
 * @param commit_flags additional flags for parametrizing, a `COMMIT_*` bitmask
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 */
void
sge_commit_job(lListElem *jep, lListElem *jatep, lListElem *jr, sge_commit_mode_t mode,
               int commit_flags, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

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

   jobid = ocs::tom::get<JB_job_number>(jep);
   jataskid = jatep != nullptr ? ocs::tom::get<JAT_task_number>(jatep) : 0;
   session = ocs::tom::get<JB_session>(jep);

   const char *pe_name = nullptr;
   ocs::tom::ConstElement<ocs::tom::PE> pe = nullptr;
   if (jatep != nullptr) {
      pe_name = ocs::tom::get<JAT_granted_pe>(jatep);
      pe = ocs::tom::get<JAT_pe_object>(jatep);
   }

   switch (mode) {
      case COMMIT_ST_SENT: {
         bool master_task = true;
         const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);

         ocs::tom::set<JAT_state>(jatep, JRUNNING);
         ocs::tom::set<JAT_status>(jatep, JTRANSFERING);

         if (!job_get_wallclock_limit(&task_wallclock, jep)) {
            compute_qwallclock = true;
         }

         ocs::ReportingFileWriter::create_job_logs(nullptr, now, JL_SENT, MSG_QMASTER, qualified_hostname, jr, jep, jatep, nullptr,
                                  MSG_LOG_SENT2EXECD);

         global_host_ep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);
         bool do_per_global_host_booking = true;

         uint32_t ar_id = ocs::tom::get<JB_ar>(jep);
         ocs::tom::Element<ocs::tom::AR> ar = nullptr;
         lListElem *ar_global_host = nullptr;
         if (ar_id != 0) {
            ar = ocs::tom::find_first_rw<AR_id>(master_ar_list, ar_id);
            if (ar == nullptr) {
               ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, ar_id, jobid);
               DRETURN_VOID;
            } else {
               ar_global_host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, SGE_GLOBAL_NAME);
            }
         }

         const char *last_hostname = nullptr;
         ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(jatep);
         ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);
         for_each_rw_lv(gdil_ep, gdil) {
            const char *queue_name = ocs::tom::get<JG_qname>(gdil_ep);
            lListElem *queue = nullptr;

            if ((queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name)) == nullptr) {
               ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, queue_name, jobid);
            } else {
               const char *queue_hostname = ocs::tom::get<QU_qhostname>(queue);
               bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);
               const char *limit = nullptr;
               int tmp_slot = ocs::tom::get<JG_slots>(gdil_ep);
               lListElem *host;

               if (compute_qwallclock) {
                  limit = ocs::tom::get<QU_h_rt>(queue);
                  if (strcasecmp(limit, INFINITY_STR) != 0) {
                     uint32_t clock_val;
                     parse_ulong_val(nullptr, &clock_val, ocs::CEntry::Type::TIME, limit, nullptr, 0);
                     task_wallclock = std::min(task_wallclock, sge_gmt32_to_gmt64(clock_val));
                  } else {
                     task_wallclock = std::min(task_wallclock, std::numeric_limits<uint64_t>::max());
                  }

                  limit = ocs::tom::get<QU_s_rt>(queue);
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
                        sge_add_event(0, sgeE_RQS_MOD, 0, 0, ocs::tom::get<RQS_name>(rqs), nullptr, nullptr, rqs, gdi_session);
                     }
                  }
               } else if (ar != nullptr) {
                  /* debit in advance reservation */
                  lListElem *queue = ocs::tom::find_first_in_rw<AR_reserved_queues, QU_full_name>(ar, queue_name);
                  int bookings = 0;
                  bookings += qinstance_debit_consumable(queue, jep, pe, master_centry_list, tmp_slot, master_task,
                                                 do_per_host_booking, nullptr) > 0;
                  if (ar_global_host != nullptr) {
                     bookings += debit_host_consumable(jep, jatep, granted_resources_list, pe, ar_global_host, master_centry_list, tmp_slot, master_task, do_per_global_host_booking, nullptr) > 0;
                  }
                  lListElem *host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, queue_hostname);
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

         ocs::tom::set<JAT_wallclock_limit>(jatep, task_wallclock);

         /*
          * Would be nice if we could use a more accurate start time that could be reported
          * by execd. However this would constrain time synchronization between qmaster and
          * execd host .. sigh!
          */
         ocs::tom::set<JAT_start_time>(jatep, now);
         sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid, nullptr, nullptr, session,
                         jep, jatep, nullptr, true, true, gdi_session);
         answer_list_output(&answer_list);
         break;
      }
      case COMMIT_ST_ARRIVED: {
         // change state to running
         ocs::tom::set<JAT_status>(jatep, JRUNNING);

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
               if (pe != nullptr && ocs::tom::get<PE_control_slaves>(pe)) {
                  bool is_master = true;

                  for (ocs::tom::ConstElement<ocs::tom::JG> granted_queue : ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) {
                     if (!is_master) {
                        lListElem *host = host_list_locate(master_exechost_list, ocs::tom::get<JG_qhostname>(granted_queue));

                        add_to_reschedule_unknown_list(host, jobid, jataskid, RESCHEDULE_HANDLE_JR_WAIT, gdi_session);

                        DPRINTF("RU: sge_commit_job: granted_queue %s job " sge_u32 "." sge_u32 "\n",
                                ocs::tom::get<JG_qname>(granted_queue), jobid, jataskid);
                     }
                     is_master = false;
                  }
               }
            } else {
               ocs::tom::ConstList<ocs::tom::JG> granted_list = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);
               ocs::tom::ConstElement<ocs::tom::JG> granted_queue = lFirst(granted_list);
               lListElem *host = host_list_locate(master_exechost_list, ocs::tom::get<JG_qhostname>(granted_queue));

               add_to_reschedule_unknown_list(host, jobid, jataskid, RESCHEDULE_SKIP_JR, gdi_session);
            }
         }

         /*
          * Preserve any potential deferred startup request
          * in JAT_state across re-initialization.
          */
         ocs::tom::set<JAT_status>(jatep, JIDLE);
         state = ocs::tom::get<JAT_state>(jatep);
         state &= JDEFERRED_REQ;
         if (mode == COMMIT_ST_RESCHEDULED || mode == COMMIT_ST_USER_RESCHEDULED) {
            state |= JQUEUED | JWAITING;
         } else {
            state |= JQUEUED | JWAITING | JERROR;
         }
         ocs::tom::set<JAT_state>(jatep, state);

         /*
          * Prepare usage lists for the next run
          */
         ocs::tom::set<JAT_previous_usage_list>(jatep, lCopyList("name", ocs::tom::get<JAT_scaled_usage_list>(jatep)));
         ocs::tom::set<JAT_scaled_usage_list>(jatep, nullptr);
         ocs::tom::set<JAT_reported_usage_list>(jatep, nullptr);

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
               ocs::tom::set<JB_submission_time>(jep, now);
               sge_event_spool(&answer_list, now, sgeE_JOB_MOD, jobid, jataskid,
                               nullptr, nullptr, session, jep, jatep, nullptr, true, true, gdi_session);
            }
         }

         /* sum up the usage of all pe tasks not yet deleted (e.g. tasks from
          * exec host in unknown state). Then remove all pe tasks except the
          * past usage container
          */
         {
            ocs::tom::List<ocs::tom::PET> pe_task_list = ocs::tom::get<JAT_task_list>(jatep);

            if (pe_task_list != nullptr) {
               const lListElem *existing_container = ocs::tom::find_first<PET_id>(pe_task_list, PE_TASK_PAST_USAGE_CONTAINER);
               lListElem *container = pe_task_sum_past_usage_all(pe_task_list);
               lListElem *pe_task = nullptr;
               lListElem *next = nullptr;

               /* the usage container is not spooled */
               if (existing_container == nullptr) {
                  sge_add_event(now, sgeE_PETASK_ADD, jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER,
                                nullptr, session, container, gdi_session);
               } else {
                  sge_add_list_event(now, sgeE_JOB_USAGE, jobid, jataskid, PE_TASK_PAST_USAGE_CONTAINER,
                                     nullptr, session, ocs::tom::get<PET_scaled_usage>(container), gdi_session);
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

         ocs::tom::set<JAT_status>(jatep, JFINISHED);

         sge_job_finish_event(jep, jatep, jr, commit_flags, nullptr, gdi_session);

         /* possibly release successor tasks if there are any array dependencies on this task */
         if (jataskid) {
            release_successor_tasks_ad(jep, jataskid, gdi_session);
         }
         sge_clear_granted_resources(jep, jatep, 1, monitor, gdi_session);
         for (ocs::tom::Element<ocs::tom::PET> petask : ocs::tom::get<JAT_task_list>(jatep)) {
            sge_add_list_event(now, sgeE_JOB_FINAL_USAGE, jobid, jataskid,
                               ocs::tom::get<PET_id>(petask), nullptr, session,
                               ocs::tom::get<PET_scaled_usage>(petask), gdi_session);
         }
         sge_add_list_event(now, sgeE_JOB_FINAL_USAGE, jobid, jataskid, nullptr, nullptr, session,
                            ocs::tom::get<JAT_scaled_usage_list>(jatep), gdi_session);

         /* Book the finished job's usage into UU_/PR_/UPP_ (was the second
          * chained call from callers). No events or spool here - those are
          * deferred to the TET sharetree-spool handler. */
         sge_book_finished_job_usage(jep, jatep, monitor, gdi_session);

         /* CS-1908 retention: keep the finished ja_task on master_job_list for
          * a configurable window instead of burying it inline. Gate on either
          * retention-semantics tunable being non-zero; when both are 0 the
          * feature is off and we bury as pre-CS-1908.
          *
          * Under retention we:
          *  (1) propagate execd's end_time from JR_usage onto master's
          *      JAT_end_time (KTD2 correction, closes the pre-existing TODO
          *      at sge_ja_task.cc:482). Retention's age comparison reads
          *      JAT_end_time.
          *  (2) fold all finished pe_tasks into the PE_TASK_PAST_USAGE_CONTAINER
          *      on JAT_task_list and remove the individual pe_task elements.
          *      Per-pe_task detail would balloon memory for tight-PE workloads;
          *      the aggregate container carries the usage a qstat -j reader
          *      needs and per-pe_task detail lives in accounting (KTD in origin).
          *  (3) run sge_finish_ja_task -- the U3 finish-side (signal-resend
          *      removal, per-JAT + whole-JOB successor release, security
          *      hooks, suser_unregister_job, category detach).
          *  (4) sge_event_spool(sgeE_JATASK_MOD, ...) -- one call = spool
          *      write + mirror event (KTD5).
          *  (5) skip sge_bury_ja_task -- the U5 retention sweep prunes it
          *      when the time OR count limit bites. */
         if (mconf_get_finished_jobs_keep_time() > 0 || mconf_get_finished_jobs_max() > 0) {
            /* (1) stamp master JAT_end_time. Prefer execd's shepherd-recorded
             * end_time from JR_usage (accurate to the moment the child exited),
             * fall back to sge_get_gmt64() when the field is absent -- happens
             * on paths where jr is nullptr (SUSPENDED-on-cancel, NEVER_RAN
             * cases) or where execd delivered a JR with no usage list at all
             * (some interactive/qrsh teardowns write no usage file, so
             * reaper_execd's convert_attribute never runs). Under retention
             * the sweep handler compares finished[i].end_time > 0 && ... +
             * keep_time_delta < curr_time; leaving JAT_end_time at 0 makes
             * time-based pruning silently no-op while count-based pruning
             * still works, which is exactly the symptom pattern. */
            uint64_t end_time = usage_list_get_ulong64_usage(ocs::tom::get<JR_usage>(jr), "end_time", 0);
            if (end_time == 0) {
               end_time = sge_get_gmt64();
            }
            ocs::tom::set<JAT_end_time>(jatep, end_time);

            // (2) fold finished pe_tasks into PAST_USAGE_CONTAINER, remove pe_tasks
            ocs::tom::List<ocs::tom::PET> pe_task_list = ocs::tom::get<JAT_task_list>(jatep);
            if (pe_task_list != nullptr) {
               pe_task_sum_past_usage_all(pe_task_list);
               lListElem *pe_task = lFirstRW(pe_task_list);
               while (pe_task != nullptr) {
                  lListElem *next_pe_task = lNextRW(pe_task);
                  if (strcmp(ocs::tom::get<PET_id>(pe_task), PE_TASK_PAST_USAGE_CONTAINER) != 0) {
                     lRemoveElem(pe_task_list, &pe_task);
                  }
                  pe_task = next_pe_task;
               }
            }

            // (3) finish-side (U3): signal-resend removal, successor release,
            //     security hooks, suser_unregister_job, category detach
            sge_finish_ja_task(sge_root, jep, jobid, jatep, no_events, gdi_session);

            // (4) spool the modification + emit sgeE_JATASK_MOD (KTD5)
            lList *answer_list = nullptr;
            sge_event_spool(&answer_list, now, sgeE_JATASK_MOD, jobid, jataskid,
                            nullptr, nullptr, session, jep, jatep, nullptr,
                            true, true, gdi_session);
            answer_list_output(&answer_list);

            // (5) bury deferred to U5 retention sweep
         } else {
            /* Retention off: bury the ja_task / job inline as pre-CS-1908.
             * sge_bury_job opens its own spool transaction around its writes
             * (spool_delete_script + spool_delete_object) and emits
             * sgeE_JATASK_DEL or sgeE_JOB_DEL. No outer transaction here
             * because BerkeleyDB transactions do not nest. */
            sge_bury_job(sge_root, jep, jobid, jatep, spool_job, no_events, gdi_session);
         }
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
         ocs::tom::set<JAT_status>(jatep, JIDLE);
         ocs::tom::set<JAT_state>(jatep, JQUEUED | JWAITING);
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
   DENTER(TOP_LAYER);

   bool release_jr = false;

   if (!jr) {
      jr = lCreateElem(JR_Type);
      ocs::tom::set<JR_job_number>(jr, ocs::tom::get<JB_job_number>(jep));
      ocs::tom::set<JR_ja_task_number>(jr, ocs::tom::get<JAT_task_number>(jatep));
      ocs::tom::set<JR_wait_status>(jr, SGE_NEVERRAN_BIT);
      release_jr = true;
   } else {
      /* JR_usage gets cleared in the process of cleaning up a finished job.
       * It's contents get put into JAT_usage_list and eventually added to
       * JAT_usage_list.  In the JAPI, however, it would be ugly to have
       * to go picking through the master job list to find the accounting data.
       * So instead we pick through it here and stick it back in JR_usage. */

      ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<JAT_usage_list>(jatep));

      if (commit_flags & COMMIT_NEVER_RAN) {
         ocs::tom::set<JR_wait_status>(jr, SGE_NEVERRAN_BIT);
      }
   }

   if (diagnosis != nullptr) {
      ocs::tom::set<JR_err_str>(jr, diagnosis);
   } else if (!ocs::tom::get<JR_err_str>(jr)) {
      if (SGE_GET_NEVERRAN(ocs::tom::get<JR_wait_status>(jr))) {
         ocs::tom::set<JR_err_str>(jr, "Job never ran");
      } else {
         ocs::tom::set<JR_err_str>(jr, "Unknown job finish condition");
      }
   }

   sge_add_event(0, sgeE_JOB_FINISH, ocs::tom::get<JB_job_number>(jep),
                 ocs::tom::get<JAT_task_number>(jatep), nullptr, nullptr,
                 ocs::tom::get<JB_session>(jep), jr, gdi_session);

   if (release_jr) {
      lFreeElem(&jr);
   } else {
      ocs::tom::exchange<JR_usage>(jr, ocs::tom::list_ref<JAT_usage_list>(jatep));
   }

   DRETURN_VOID;
}

static void
release_successor_jobs(const lListElem *jep, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JRE> jid;
   uint32_t job_ident;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   for_each_ep(jid, ocs::tom::get<JB_jid_successor_list>(jep)) {
      ocs::tom::Element<ocs::tom::JB> suc_jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, ocs::tom::get<JRE_job_number>(jid));
      if (suc_jep) {
         /* if we don't find it by job id we try it with the name */
         job_ident = ocs::tom::get<JB_job_number>(jep);
         if (!ocs::tom::erase_in<JB_jid_predecessor_list, JRE_job_number>(suc_jep, job_ident)) {

            DPRINTF("no reference " sge_u32 " and %s to job " sge_u32 " in predecessor list of job " sge_u32"\n",
                    job_ident, ocs::tom::get<JB_job_name>(jep), ocs::tom::get<JB_job_number>(suc_jep), ocs::tom::get<JB_job_number>(jep));
         } else {
            if (ocs::tom::get<JB_jid_predecessor_list>(suc_jep)) {
               DPRINTF("removed job " sge_u32 "'s dependance from exiting job " sge_u32 "\n",
                       ocs::tom::get<JB_job_number>(suc_jep), ocs::tom::get<JB_job_number>(jep));
            } else {
               DPRINTF("job " sge_u32 "'s job exit triggers start of job " sge_u32 "\n",
                       ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JB_job_number>(suc_jep));
            }
            sge_add_job_event(sgeE_JOB_MOD, suc_jep, nullptr, gdi_session);
         }
      }
   }

   DRETURN_VOID;
}

static void
release_successor_jobs_ad(const lListElem *jep, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JRE> jid;
   uint32_t job_ident;
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   for_each_ep(jid, ocs::tom::get<JB_ja_ad_successor_list>(jep)) {
      ocs::tom::Element<ocs::tom::JB> suc_jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, ocs::tom::get<JRE_job_number>(jid));
      if (suc_jep) {
         int Modified = 0;
         /* if we don't find it by job id we try it with the name */
         job_ident = ocs::tom::get<JB_job_number>(jep);
         if (!ocs::tom::erase_in<JB_ja_ad_predecessor_list, JRE_job_number>(suc_jep, job_ident)) {
            DPRINTF("no reference " sge_u32 " and %s to job " sge_u32 " in array predecessor list of job " sge_u32"\n",
                    job_ident, ocs::tom::get<JB_job_name>(jep),
                    ocs::tom::get<JB_job_number>(suc_jep), ocs::tom::get<JB_job_number>(jep));
         } else {
            if (ocs::tom::get<JB_ja_ad_predecessor_list>(suc_jep)) {
               DPRINTF("removed job " sge_u32 "'s array dependance from exiting job " sge_u32 "\n",
                       ocs::tom::get<JB_job_number>(suc_jep), ocs::tom::get<JB_job_number>(jep));
            } else {
               DPRINTF("job " sge_u32 "'s job exit triggers release of array tasks in job " sge_u32 "\n",
                       ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JB_job_number>(suc_jep));
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
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JRE> jid;
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);

   /* every successor job of this job might have tasks to be released */
   for_each_ep(jid, ocs::tom::get<JB_ja_ad_successor_list>(jep)) {
      uint32_t job_ident = ocs::tom::get<JRE_job_number>(jid);
      lListElem *suc_range = nullptr;
      lListElem *suc_jep = nullptr;
      int Modified = 0;
      uint32_t bmin, bmax, sb;

      /* JA: should be doing something if this job cannot be located? */
      suc_jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_ident);
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
   DENTER(TOP_LAYER);

   int pe_slots = 0;
   uint32_t job_id = ocs::tom::get<JB_job_number>(job);
   uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   ocs::tom::ConstList<ocs::tom::JG> gdi_list = ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task);
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

   if (!job_is_ja_task_defined(job, ja_task_id) ||
       !job_is_enrolled(job, ja_task_id)) {
      DRETURN_VOID;
   }

   uint64_t now = sge_get_gmt64();

   /* unsuspend queues on subordinate */
   cqueue_list_x_on_subordinate_gdil(master_cqueue_list, false, gdi_list, monitor, gdi_session);

   global_host_ep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);
   bool do_per_global_host_booking = true;

   const char *pe_name = ocs::tom::get<JAT_granted_pe>(ja_task);
   lListElem *pe = ocs::tom::get<JAT_pe_object>(ja_task);

   uint32_t ar_id = ocs::tom::get<JB_ar>(job);
   ocs::tom::Element<ocs::tom::AR> ar = nullptr;
   lListElem *ar_global_host = nullptr;
   if (ar_id != 0) {
      ar = ocs::tom::find_first_rw<AR_id>(master_ar_list, ar_id);
      if (ar == nullptr) {
         ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, ar_id, job_id);
         DRETURN_VOID;
      } else {
         ar_global_host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, SGE_GLOBAL_NAME);
      }
   }

   /* free granted resources of the queue */
   const char *last_hostname = nullptr;
   ocs::tom::ConstElement<ocs::tom::JG> gdil_ep;
   for_each_ep(gdil_ep, gdi_list) {
      const char *queue_name = ocs::tom::get<JG_qname>(gdil_ep);
      lListElem *queue = nullptr;

      if ((queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name)) == nullptr) {
         ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, queue_name, job_id);
      } else {
         const char *queue_hostname = ocs::tom::get<QU_qhostname>(queue);
         bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);
         int tmp_slot = ocs::tom::get<JG_slots>(gdil_ep);

         /* increase free slots */
         pe_slots += tmp_slot;
         if (incslots) {
            lListElem *host;
            ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(ja_task);

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
                  DPRINTF("undebiting rqs %s\n", ocs::tom::get<RQS_name>(rqs));
                  if (rqs_debit_consumable(rqs, job, gdil_ep, pe,
                                           master_centry_list, master_userset_list, master_hgroup_list,
                                           -tmp_slot, master_task, do_per_host_booking) > 0) {
                     /* this info is not spooled */
                     sge_add_event(0, sgeE_RQS_MOD, 0, 0,
                                   ocs::tom::get<RQS_name>(rqs), nullptr, nullptr, rqs, gdi_session);
                  }
               }
            } else {
               /* undebit in advance reservation */
               lListElem *queue = ocs::tom::find_first_in_rw<AR_reserved_queues, QU_full_name>(ar, queue_name);
               int bookings = 0;
               bookings += qinstance_debit_consumable(queue, job, pe, master_centry_list, -tmp_slot, master_task,
                                              do_per_host_booking, nullptr) > 0;
               if (ar_global_host != nullptr) {
                  bookings += debit_host_consumable(job, ja_task, granted_resources_list, pe, ar_global_host, master_centry_list, -tmp_slot, master_task, do_per_global_host_booking, nullptr) > 0;
               }
               lListElem *host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(ar, queue_hostname);
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
      ocs::tom::set<JAT_granted_pe>(ja_task, nullptr);
      ocs::tom::set<JAT_pe_object>(ja_task, nullptr);
   }

   /* forget about old tasks */
   ocs::tom::set<JAT_task_list>(ja_task, nullptr);

   /* remove granted_destin_identifier_list */
   ocs::tom::set<JAT_granted_destin_identifier_list>(ja_task, nullptr);
   ocs::tom::set<JAT_master_queue>(ja_task, nullptr);

   DRETURN_VOID;
}

static const char *
get_requested_limit(const lListElem *job, const char *limit_name,
                    bool is_pe_job, bool is_first_gdil_entry, bool master_only) {
   const char *ret = nullptr;

   // if we have requested the limit globally, then use this value
   ocs::tom::ConstElement<ocs::tom::CE> res = ocs::tom::find_first<CE_name>(job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL), limit_name);
   if (res != nullptr) {
      ret = ocs::tom::get<CE_stringval>(res);
   } else {
      if (is_pe_job) {
         // it is a tightly integrated pe job, we might have master or slave requests
         if (is_first_gdil_entry) {
            // if this is the first gdil entry (master task)
            // we might have only a master task in this gdil (1 slot, job_is_first_task true) => use only master request
            // or a master task and slave tasks => use maximum of master and slave request
            if (master_only) {
               ocs::tom::ConstElement<ocs::tom::CE> res = ocs::tom::find_first<CE_name>(job_get_hard_resource_list(job, JRS_SCOPE_MASTER), limit_name);
               if (res != nullptr) {
                  ret = ocs::tom::get<CE_stringval>(res);
               }
            } else {
               // we need to compare the possibly existing master and slave request as double in order to compare them
               double master_request = std::numeric_limits<double>::max();
               double slave_request = std::numeric_limits<double>::max();
               ocs::tom::ConstElement<ocs::tom::CE> master_res = ocs::tom::find_first<CE_name>(job_get_hard_resource_list(job, JRS_SCOPE_MASTER), limit_name);
               if (master_res != nullptr) {
                  master_request = ocs::tom::get<CE_doubleval>(master_res);
               }
               ocs::tom::ConstElement<ocs::tom::CE> slave_res = ocs::tom::find_first<CE_name>(job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), limit_name);
               if (slave_res != nullptr) {
                  slave_request = ocs::tom::get<CE_doubleval>(slave_res);
               }

               // pick the maximum
               if (master_request >= slave_request) {
                  if (master_res != nullptr) {
                     ret = ocs::tom::get<CE_stringval>(master_res);
                  }
               } else {
                  if (slave_res != nullptr) {
                     ret = ocs::tom::get<CE_stringval>(slave_res);
                  }
               }
            }
         } else {
            // this is a slave host/queue, we might have a slave request
            ocs::tom::ConstElement<ocs::tom::CE> res = ocs::tom::find_first<CE_name>(job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), limit_name);
            if (res != nullptr) {
               ret = ocs::tom::get<CE_stringval>(res);
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
      if ((dcep = centry_list_locate(master_centry_list, rlimit_name)) && ocs::tom::get<CE_consumable>(dcep)) {
         const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
         if (ocs::tom::find_first_in<QU_consumable_config_list, CE_name>(qep, rlimit_name) ||
             ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(host_list_locate(master_ehost_list, ocs::tom::get<QU_qhostname>(qep)), rlimit_name) ||
             ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(host_list_locate(master_ehost_list, SGE_GLOBAL_NAME), rlimit_name)) {
            /* managed at queue level, managed at host level or managed at global level */
            lSetString(qep, nm, ocs::tom::get<CE_defaultval>(dcep));
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
      ocs::tom::ConstElement<ocs::tom::PE> pe = ocs::tom::get<JAT_pe_object>(ja_task);
      if (ocs::tom::get<JG_slots>(gdil_ep) > 1 ||
         (pe != nullptr && !ocs::tom::get<PE_job_is_first_task>(pe))) {
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
/** @brief Book a finished task's usage and write its accounting record
 *
 * The first half of what used to be `sge_bury_job()`; the task itself survives
 * until sge_bury_ja_task().
 *
 * @param sge_root the cluster root, for the accounting file
 * @param job the job (`JB_Type`)
 * @param job_id the job id
 * @param ja_task the array task
 * @param no_events whether to skip announcing the change
 * @param gdi_session the session the change belongs to
 */
void
sge_finish_ja_task(const char *sge_root, lListElem *job, uint32_t job_id, lListElem *ja_task, int no_events, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   if (job == nullptr) {
      lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
      job = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_id);
   }

   const uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);

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
      for (ocs::tom::ConstElement<ocs::tom::JAT> iter_jat : ocs::tom::get<JB_ja_tasks>(job)) {
         if (ocs::tom::get<JAT_status>(iter_jat) != JFINISHED) {
            job_done = false;
            break;
         }
      }
   }

   if (job_done) {
      release_successor_jobs(job, gdi_session);

      // release the SUSER slot the JB held
      const lList *master_suser_list = *ocs::DataStore::get_master_list(SGE_TYPE_SUSER);
      suser_unregister_job(job, master_suser_list);

      /* CS-1908: category detach is deferred to sge_bury_ja_task's remove_job
       * block. The invariant "JB is on master_job_list implies its category is
       * attached (JB_category_id != 0 and the CT element exists)" must hold
       * for the whole retention window, otherwise the scheduler thread's
       * refresh_cat_data_in_job trips its SGE_ASSERT. So the detach fires
       * only at the actual JB removal from master_job_list, not here. */
   }

   DRETURN_VOID;
}

/** @brief CS-1908 bury-side: remove the JAT from master_job_list, unlink its
 * spool entry, emit the DEL event. When this is the last JAT on the JB, also
 * unlink the job script and exec file and remove the whole JB. Runs from the
 * legacy sge_bury_job wrapper today; under retention U5's sweep calls this
 * directly. Idempotent on already-removed JATs is not guaranteed — callers
 * must ensure the JAT is on the list when invoking. */
/** @brief Remove a finished task from qmaster
 *
 * The second half. With finished-job retention this runs long after
 * sge_finish_ja_task(), from the retention sweep, so that `qstat -s f` can
 * still report the task in the meantime.
 *
 * @param sge_root the cluster root
 * @param job the job (`JB_Type`)
 * @param job_id the job id
 * @param ja_task the array task
 * @param spool_job whether the job has to be spooled afterwards
 * @param no_events whether to skip announcing the change
 * @param gdi_session the session the change belongs to
 */
void
sge_bury_ja_task(const char *sge_root, lListElem *job, uint32_t job_id, lListElem *ja_task, int spool_job, int no_events, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   if (job == nullptr) {
      job = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_id);
   }

   const uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   if (const bool remove_job = (!ja_task || job_get_ja_tasks(job) == 1); remove_job) {
      /*
       * do not try to remove script file for interactive jobs
       */
      {
         lList *answer_list = nullptr;
         dstring buffer = DSTRING_INIT;

         spool_transaction(&answer_list, spool_get_default_context(), STC_begin);

         if ((ocs::tom::get<JB_exec_file>(job) != nullptr)) {
            PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
            if (!JOB_TYPE_IS_BINARY(ocs::tom::get<JB_type>(job))) {
               spool_delete_script(&answer_list, ocs::tom::get<JB_job_number>(job), job);
            }
            ocs::tom::set<JB_exec_file>(job, nullptr);
            PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
         }

         spool_delete_object(&answer_list, spool_get_default_context(), SGE_TYPE_JOB,
                             job_get_key(job_id, 0, nullptr, &buffer));
         answer_list_output(&answer_list);
         sge_dstring_free(&buffer);

         spool_transaction(&answer_list, spool_get_default_context(), STC_commit);
      }

      // send events
      if (!no_events) {
         sge_add_event(0, sgeE_JOB_DEL, job_id, ja_task_id,
                       nullptr, nullptr, ocs::tom::get<JB_session>(job), nullptr, gdi_session);

      }

      /* CS-1908: detach from category refcount at the same moment the JB
       * leaves master_job_list, so the scheduler thread's
       * refresh_cat_data_in_job never observes a JB with a stale
       * JB_category_id. Under the legacy sge_bury_job wrapper this
       * runs back-to-back with the finish-side steps, matching pre-CS-1908
       * behaviour; under retention it fires only at prune time from the
       * U5 sweep, so retained-JFINISHED JBs keep their category slot for
       * the full retention window. */
      lList **master_category_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);
      ocs::CategoryQmaster::detach_job(master_category_list, job, !no_events, gdi_session);

      // final job removal
      lRemoveElem(master_job_list, &job);
   } else {
      int is_enrolled = job_is_enrolled(job, ja_task_id);

      if (!no_events) {
         sge_add_event(0, sgeE_JATASK_DEL, job_id, ja_task_id,
                       nullptr, nullptr, ocs::tom::get<JB_session>(job), nullptr, gdi_session);
      }

      /*
       * remove the task
       */
      if (is_enrolled) {
         lList *answer_list = nullptr;
         dstring buffer = DSTRING_INIT;
         spool_delete_object(&answer_list, spool_get_default_context(),
                             SGE_TYPE_JOB, job_get_key(job_id, ja_task_id, nullptr, &buffer));
         answer_list_output(&answer_list);
         sge_dstring_free(&buffer);
         lRemoveElem(ocs::tom::get<JB_ja_tasks>(job), &ja_task);
      } else {
         job_delete_not_enrolled_ja_task(job, nullptr, ja_task_id);
         if (spool_job) {
            lList *answer_list = nullptr;
            dstring buffer = DSTRING_INIT;
            spool_write_object(&answer_list, spool_get_default_context(), job,
                               job_get_key(job_id, ja_task_id, nullptr, &buffer), SGE_TYPE_JOB);
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

/**
 * @brief Copy the job with only the specified ja task
 *
 * copy the job with only the specified ja task
 *
 * @param job job structure
 * @param ja_task ja-task (template)
 *
 * @return nullptr, or new job structure
 *
 * @note MT-NOTE: copyJob() is MT safe
 */
static lListElem *
copyJob(lListElem *job, lListElem *ja_task) {
   DENTER(TOP_LAYER);

   lListElem *job_copy = nullptr;
   const lListElem *ja_task_copy = nullptr;
   lList *tmp_ja_task_list = nullptr;

   /* create a copy of the job */
   ocs::tom::exchange<JB_ja_tasks>(job, &tmp_ja_task_list);
   job_copy = lCopyElem(job);
   ocs::tom::exchange<JB_ja_tasks>(job, &tmp_ja_task_list);

   if (job_copy != nullptr) { /* not enough memory */

      ja_task_copy = ocs::tom::find_first<JAT_task_number>(ocs::tom::get<JB_ja_tasks>(job),
                                   ocs::tom::get<JAT_task_number>(ja_task));

      tmp_ja_task_list = lCreateList("cp_ja_task_list", lGetElemDescr(ja_task_copy));
      lAppendElem(tmp_ja_task_list, lCopyElem(ja_task_copy));
      ocs::tom::set<JB_ja_tasks>(job_copy, tmp_ja_task_list);

      if (lGetNumberOfElem(tmp_ja_task_list) == 0) { /* not enough memory */
         lFreeElem(&job_copy); /* this also frees tmp_ja_task_list */
      }
   }

   DRETURN(job_copy);
}


/**
 * @brief Sets the job checkpointing name
 *
 * sets the job checkpointing name
 *
 * @param job job to modify
 *
 * @return 0 : everything went find -1 : something went wrong
 *
 * @note MT-NOTE: setCheckpointObj() is not MT safe
 */
static int
setCheckpointObj(lListElem *job) {
   DENTER(TOP_LAYER);

   const lListElem *ckpt = nullptr;
   lListElem *tmp_ckpt = nullptr;
   const char *ckpt_name = nullptr;
   int ret = 0;
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);

   if ((ckpt_name = ocs::tom::get<JB_checkpoint_name>(job))) {
      ckpt = ckpt_list_locate(master_ckpt_list, ckpt_name);
      if (!ckpt) {
         ERROR(MSG_OBJ_UNABLE2FINDCKPT_S, ckpt_name);
         ret = -1;
      }

         /* get the necessary memory */
      else if (!(tmp_ckpt = lCopyElem(ckpt))) {
         ERROR(MSG_OBJ_UNABLE2CREATECKPT_SU, ckpt_name, ocs::tom::get<JB_job_number>(job));
         ret = -1;
      } else {
         /* everything's in place. stuff it in */
         ocs::tom::set<JB_checkpoint_object>(job, tmp_ckpt);
      }
   }
   DRETURN(ret);
}

/** @brief Release the queue instances a granted list points at that no longer exist
 *
 * A job can outlive the queue instance it was granted - the queue may be
 * deleted or reconfigured while the job runs - and the slots would otherwise
 * stay booked against something that is gone.
 *
 * @param gdil_list the granted destination list
 * @param alpp receives messages for the caller
 * @param gdi_session the session the change belongs to
 * @return true when anything was released
 */
bool
gdil_del_all_orphaned(const lList *gdil_list, lList **alpp, uint64_t gdi_session) {
   bool ret = true;
   const lListElem *gdil_ep;
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

   dstring cqueue_name = DSTRING_INIT;
   dstring host_name = DSTRING_INIT;
   bool has_hostname, has_domain;
   for_each_ep(gdil_ep, gdil_list) {
      cqueue_name_split(ocs::tom::get<JG_qname>(gdil_ep), &cqueue_name, &host_name, &has_hostname, &has_domain);
      ret &= cqueue_list_del_all_orphaned(master_cqueue_list, alpp, sge_dstring_get_string(&cqueue_name),
                                          sge_dstring_get_string(&host_name), gdi_session);
   }
   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_name);

   return ret;
}

