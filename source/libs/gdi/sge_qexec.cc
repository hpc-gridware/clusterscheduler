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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Starting and controlling remote tasks of a parallel job
 */
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <cerrno>

#include "comm/commlib.h"

#include "cull/cull.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_stdlib.h"

#include "gdi/sge_qexec.h"
#include "gdi/msg_gdilib.h"

#include "sgeobj/sge_pe_task.h"
#include "sgeobj/tom/ocs_tom_RT.h"
#include "tom/ocs_Element.h"

#include "msg_common.h"

#include <ocs_gdi_ClientServerBase.h>

static lList *remote_task_list = nullptr;
static char lasterror[4096];

static int rcv_from_execd(int options, ocs::gdi::ClientServerBase::ClientServerBaseTag tag);

/// `rcv_from_execd()` option: block until a message arrives
#define OPT_SYNCHRON 1

/**
 * @brief The reason the last qexec call failed
 * @return the message; static storage, do not free
 */
/**
 * @brief The reason the last qexec call failed
 * @return the message; static storage, do not free
 */
const char *qexec_last_err() {
   return lasterror;
}

/**
 * @brief Start a task in a tightly integrated par. job
 *
 * Starts a task in a tightly integrated job.
 * Builds a job object describing the task,
 * connects to the commd on the targeted execution host,
 * deliveres the job object and waits for an answer.
 * The answer from the execution daemon on the execution host
 * contains a task id that is returned to the caller of the function.
 *
 * @param hostname name of the host on which to start the task
 * @param queuename queue to start the task in, or nullptr to let qmaster choose
 * @param cwd working directory for the task, or nullptr for the job's
 * @param environment list containing environment variable settings for the task that override the default environment
 * @param path_aliases optional a path alias list
 * @param cert the security credential to pass to the execution host, or nullptr
 *
 * @return the task id, if the task can be executed, a value <= 0 indicates an error.
 *
 * @note MT-NOTE: sge_qexecve() is not MT safe
 */
sge_tid_t
sge_qexecve(const char *hostname, const char *queuename, const char *cwd, const lList *environment,
            const lList *path_aliases, const char *cert) {
   DENTER(TOP_LAYER);

   char myname[256];
   const char *s;
   int ret;
   lListElem *petrep;
   sge_pack_buffer pb;
   uint32_t jobid, jataskid;
   uint32_t dummymid = 0;
   const char *env_var_name = "SGE_TASK_ID";

   if (hostname == nullptr) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_INVALIDPARAMETER_SS, "sge_qexecve", "hostname");
      DRETURN(nullptr);
   }

   /* resolve user */
   uid_t uid = getuid();
   if (sge_uid2user(uid, myname, sizeof(myname) - 1, MAX_NIS_RETRIES)) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_RESOLVINGUIDTOUSERNAMEFAILED_IS, uid, strerror(errno));
      DRETURN(nullptr);
   }

   if ((s = getenv("JOB_ID")) == nullptr) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_MISSINGINENVIRONMENT_S, "JOB_ID");
      DRETURN(nullptr);
   }

   if (sscanf(s, sge_u32, &jobid) != 1) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_STRINGISINVALID_SS, s, "JOB_ID");
      DRETURN(nullptr);
   }

   if ((s = getenv(env_var_name)) != nullptr) {
      if (strcmp(s, "undefined") == 0) {
         jataskid = 1;
      } else {
         if (sscanf(s, sge_u32, &jataskid) != 1) {
            snprintf(lasterror, sizeof(lasterror), MSG_GDI_STRINGISINVALID_SS, s, env_var_name);
            DRETURN(nullptr);
         }
      }
   } else {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_MISSINGINENVIRONMENT_S, env_var_name);
      DRETURN(nullptr);
   }

   /* ---- build up pe task request structure (see gdilib/sge_petaskL.h) */
   petrep = lCreateElem(PETR_Type);

   ocs::tom::set<PETR_jobid>(petrep, jobid);
   ocs::tom::set<PETR_jataskid>(petrep, jataskid);
   ocs::tom::set<PETR_owner>(petrep, myname);
   ocs::tom::set<PETR_submission_time>(petrep, sge_get_gmt64());

   if (cwd != nullptr) {
      ocs::tom::set<PETR_cwd>(petrep, cwd);
   }

   if (environment != nullptr) {
      ocs::tom::set<PETR_environment>(petrep, lCopyList("environment", environment));
   }

   if (path_aliases != nullptr) {
      ocs::tom::set<PETR_path_aliases>(petrep, lCopyList("path_aliases", path_aliases));
   }

#if defined(OCS_WITH_OPENSSL)
   if (cert != nullptr) {
      ocs::tom::set<PETR_cred>(petrep, cert);
   }
#endif

   if (queuename != nullptr) {
      ocs::tom::set<PETR_queuename>(petrep, queuename);
   }

   /* CS-2375: the qrsh -inherit task delivery to the execd is a single
    * synchronous send. When a massively parallel tightly-integrated job starts
    * many tasks at once, the freshly (auto-)opened connection to the master-host
    * execd may not have completed its handshake by the time the send fires, so
    * cl_commlib_append_message_to_connection() rejects it with
    * CL_RETVAL_SEND_ERROR (connection not yet in CL_COM_WORK) or
    * CL_RETVAL_CONNECTION_NOT_FOUND. Both are *pre-transmit* failures - the task
    * was never queued, so retrying cannot double-start it. Retry a few times,
    * driving the (single-threaded client) commlib between attempts so the
    * connection can progress to CL_COM_WORK. */
   {
      constexpr int task_send_max_attempts = 10;
      cl_com_handle_t *execd_handle = nullptr;

      for (int attempt = 0; attempt < task_send_max_attempts; attempt++) {
         if (init_packbuffer(&pb, 1024) != PACK_SUCCESS) {
            lFreeElem(&petrep);
            snprintf(lasterror, sizeof(lasterror), SFN, MSG_GDI_OUTOFMEMORY);
            DRETURN(nullptr);
         }

         /* gdi_send_message_pb() consumes (frees) the packed buffer on every
          * attempt, so re-pack the task delivery for each try. */
         pack_job_delivery(&pb, petrep);

         ret = ocs::gdi::ClientServerBase::gdi_send_message_pb(1, to_cstr(EXECD), 1, hostname,
                                                           ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION, &pb, &dummymid);

         clear_packbuffer(&pb);

         if (ret == CL_RETVAL_OK ||
             (ret != CL_RETVAL_SEND_ERROR && ret != CL_RETVAL_CONNECTION_NOT_FOUND)) {
            break;   /* delivered, or a non-transient error that retrying won't help */
         }

         /* transient: let the opening connection make progress, then back off */
         if (execd_handle == nullptr) {
            execd_handle = cl_com_get_handle("execd_handle", 0);
         }
         if (execd_handle != nullptr) {
            cl_commlib_trigger(execd_handle, 1);
         }
         sge_usleep(100000);   /* 100 ms */
      }

      lFreeElem(&petrep);

      if (ret != CL_RETVAL_OK) {
         snprintf(lasterror, sizeof(lasterror), MSG_GDI_SENDTASKTOEXECDFAILED_SS, hostname, cl_get_error_text(ret));
         DRETURN(nullptr);
      }
   }

   /* add list into our remote task list */
   ocs::tom::Element<ocs::tom::RT> rt = ocs::tom::emplace_back<RT_tid>(&remote_task_list, "none");
   ocs::tom::set<RT_hostname>(rt, hostname);
   ocs::tom::set<RT_state>(rt, RT_STATE_WAIT4ACK);

   rcv_from_execd(OPT_SYNCHRON, ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION);

   auto tid = (sge_tid_t) ocs::tom::get<RT_tid>(rt);

   if (strcmp(tid, "none") == 0) {
      tid = nullptr;
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_EXECDONHOSTDIDNTACCEPTTASK_S, hostname);
   }

   /* now close message to execd */
   cl_commlib_shutdown_handle(cl_com_get_handle("execd_handle", 0), false);

   DRETURN(tid);
}

/*
 *
 *  NOTES
 *     MT-NOTE: sge_qwaittid() is not MT safe
 *
 */
/**
 * @brief Wait for a remote task started with #sge_qexecve
 *
 * @param tid the task to wait for, or nullptr for any
 * @param[out] status receives the task's exit status
 * @param options 0 to block, or `WNOHANG` to return immediately
 * @return the id of the task that ended, or nullptr when none had
 */
int sge_qwaittid(sge_tid_t tid, int *status, int options) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::RT> rt = nullptr;
   int ret, rcv_opt = 0;

   if (!(options & WNOHANG))
      rcv_opt |= OPT_SYNCHRON;

   if (tid != nullptr && !(rt = ocs::tom::find_first_rw<RT_tid>(remote_task_list, tid))) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_TASKNOTEXIST_S, tid);
      DRETURN(-1);
   }

   while ((rt && /* definite one searched */
           ocs::tom::get<RT_state>(rt) != RT_STATE_EXITED && /* not exited */
           ocs::tom::get<RT_state>(rt) == RT_STATE_WAIT4ACK) /* waiting for ack */
          || (!rt && /* anybody searched */
              !ocs::tom::find_first<RT_state>(remote_task_list, RT_STATE_EXITED) && /* none exited */
              ocs::tom::find_first<RT_state>(remote_task_list, RT_STATE_WAIT4ACK))) /* but one is waiting for ack */ {
      /* wait for incoming messeges about exited tasks */
      if ((ret = rcv_from_execd(rcv_opt, ocs::gdi::ClientServerBase::TAG_TASK_EXIT))) {
         DRETURN((ret < 0) ? -1 : 0);
      }
   }

   if (status)
      *status = (int)ocs::tom::get<RT_status>(rt);
   ocs::tom::set<RT_state>(rt, RT_STATE_WAITED);

   DRETURN(0);
}

/* return 
   0  reaped a task cleanly  
   1  no message (asynchronuous mode)
   -1 got an error
 
    NOTES
       MT-NOTE: rcv_from_execd() is not MT safe

*/
static int rcv_from_execd(int options, ocs::gdi::ClientServerBase::ClientServerBaseTag tag) {
   DENTER(TOP_LAYER);

   int ret;
   char *msg = nullptr;
   uint32_t msg_len = 0;
   sge_pack_buffer pb;
   u_short from_id;
   char host[1024];

   ocs::tom::Element<ocs::tom::RT> rt_rcv;
   uint32_t exit_status = 0;
   sge_tid_t tid = nullptr;

   host[0] = '\0';
   from_id = 1;
   do {
      char component_name[1024];
      strcpy(component_name, to_cstr(EXECD));

      ret = ocs::gdi::ClientServerBase::gdi_receive_message(component_name, &from_id, host,
                                                        &tag, &msg, &msg_len, (options & OPT_SYNCHRON) ? 1 : 0);

      if (ret != CL_RETVAL_OK && ret != CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
         snprintf(lasterror, sizeof(lasterror), MSG_GDI_MESSAGERECEIVEFAILED_SI, cl_get_error_text(ret), ret);
         DRETURN(-1);
      }
   } while (options & OPT_SYNCHRON && ret == CL_RETVAL_SYNC_RECEIVE_TIMEOUT);

   if (ret == CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
      DRETURN(1);
   }

   ret = init_packbuffer_from_buffer(&pb, msg, msg_len);
   if (ret != PACK_SUCCESS) {
      snprintf(lasterror, sizeof(lasterror), MSG_GDI_ERRORUNPACKINGGDIREQUEST_SSUS, host, to_cstr(EXECD), from_id, cull_pack_strerror(ret));
      DRETURN(-1);
   }

   switch (tag) {
      case ocs::gdi::ClientServerBase::TAG_TASK_EXIT:
         unpackstr(&pb, &tid);
         unpackint(&pb, &exit_status);
         break;
      case ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION:
         unpackstr(&pb, &tid);
         break;
      default:
         break;
   }

   clear_packbuffer(&pb);

   switch (tag) {
      case ocs::gdi::ClientServerBase::TAG_TASK_EXIT:
         /* change state in exited task */
         if (!(rt_rcv = ocs::tom::find_first_rw<RT_tid>(remote_task_list, tid))) {
            snprintf(lasterror, sizeof(lasterror), MSG_GDI_TASKNOTFOUND_S, tid);
            sge_free(&tid);
            DRETURN(-1);
         }

         ocs::tom::set<RT_status>(rt_rcv, exit_status);
         ocs::tom::set<RT_state>(rt_rcv, RT_STATE_EXITED);
         break;

      case ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION:
         /* search task without taskid */
         if (!(rt_rcv = ocs::tom::find_first_rw<RT_tid>(remote_task_list, "none"))) {
            snprintf(lasterror, sizeof(lasterror), MSG_GDI_TASKNOTFOUNDNOIDGIVEN_S, tid);
            DRETURN(-1);
         }
         ocs::tom::set<RT_tid>(rt_rcv, tid);
         break;

      default:
         break;
   }

   sge_free(&tid);
   DRETURN(0);
}

