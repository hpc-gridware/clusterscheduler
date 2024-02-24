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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <cerrno>

#include "comm/commlib.h"

#include "cull/cull.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"

#include "gdi/pack_job_delivery.h"
#include "gdi/sge_qexec.h"
#include "gdi/sge_gdi2.h"
#include "gdi/msg_gdilib.h"

#include "sgeobj/sge_pe_task.h"

#include "msg_common.h"

static lList *remote_task_list = nullptr;
static char lasterror[4096];

/* option flags for rcv_from_execd() */
#define OPT_SYNCHRON 1

#define LOCATE_RTASK(tid) lGetElemStrRW(remote_task_list, RT_tid, tid)

static int rcv_from_execd(int options, int tag);

const char *qexec_last_err() {
   return lasterror;
}

/****** gdi/sge/sge_qexecve() ************************************************
*  NAME
*     sge_qexecve() -- start a task in a tightly integrated par. job
*
*  SYNOPSIS
*     sge_tid_t sge_qexecve(const char *hostname, const char *queuename, 
*                           const char *cwd, const lList *environment
*                           const lList *path_aliases)
*
*  FUNCTION
*     Starts a task in a tightly integrated job.
*     Builds a job object describing the task, 
*     connects to the commd on the targeted execution host,
*     deliveres the job object and waits for an answer.
*     The answer from the execution daemon on the execution host
*     contains a task id that is returned to the caller of the function.
*
*  INPUTS
*     const char *hostname - name of the host on which to start the task
*     const lList *environment  - list containing environment variable 
*                            settings for the task that override the 
*                            default environment
*     const lList *path_aliases - optional a path alias list
*
*  RESULT
*     sge_tid_t - the task id, if the task can be executed,
*                 a value <= 0 indicates an error.
*
*  NOTES
*     MT-NOTE: sge_qexecve() is not MT safe
******************************************************************************/
sge_tid_t
sge_qexecve(const char *hostname, const char *queuename, const char *cwd, const lList *environment,
            const lList *path_aliases) {
   char myname[256];
   const char *s;
   int ret;
   lListElem *petrep;
   lListElem *rt;
   sge_pack_buffer pb;
   u_long32 jobid, jataskid;
   u_long32 dummymid = 0;
   const char *env_var_name = "SGE_TASK_ID";

   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      sprintf(lasterror, MSG_GDI_INVALIDPARAMETER_SS, "sge_qexecve", "hostname");
      DRETURN(nullptr);
   }

   /* resolve user */
   uid_t uid = getuid();
   if (sge_uid2user(uid, myname, sizeof(myname) - 1, MAX_NIS_RETRIES)) {
      sprintf(lasterror, MSG_GDI_RESOLVINGUIDTOUSERNAMEFAILED_IS, uid, strerror(errno));
      DRETURN(nullptr);
   }

   if ((s = getenv("JOB_ID")) == nullptr) {
      sprintf(lasterror, MSG_GDI_MISSINGINENVIRONMENT_S, "JOB_ID");
      DRETURN(nullptr);
   }

   if (sscanf(s, sge_uu32, &jobid) != 1) {
      sprintf(lasterror, MSG_GDI_STRINGISINVALID_SS, s, "JOB_ID");
      DRETURN(nullptr);
   }

   if ((s = getenv(env_var_name)) != nullptr) {
      if (strcmp(s, "undefined") == 0) {
         jataskid = 1;
      } else {
         if (sscanf(s, sge_uu32, &jataskid) != 1) {
            sprintf(lasterror, MSG_GDI_STRINGISINVALID_SS, s, env_var_name);
            DRETURN(nullptr);
         }
      }
   } else {
      sprintf(lasterror, MSG_GDI_MISSINGINENVIRONMENT_S, env_var_name);
      DRETURN(nullptr);
   }

   /* ---- build up pe task request structure (see gdilib/sge_petaskL.h) */
   petrep = lCreateElem(PETR_Type);

   lSetUlong(petrep, PETR_jobid, jobid);
   lSetUlong(petrep, PETR_jataskid, jataskid);
   lSetString(petrep, PETR_owner, myname);
   lSetUlong(petrep, PETR_submission_time, sge_get_gmt());

   if (cwd != nullptr) {
      lSetString(petrep, PETR_cwd, cwd);
   }

   if (environment != nullptr) {
      lSetList(petrep, PETR_environment, lCopyList("environment", environment));
   }

   if (path_aliases != nullptr) {
      lSetList(petrep, PETR_path_aliases, lCopyList("path_aliases", path_aliases));
   }


   if (queuename != nullptr) {
      lSetString(petrep, PETR_queuename, queuename);
   }

   if (init_packbuffer(&pb, 1024, 0) != PACK_SUCCESS) {
      lFreeElem(&petrep);
      sprintf(lasterror, SFN, MSG_GDI_OUTOFMEMORY);
      DRETURN(nullptr);
   }

   pack_job_delivery(&pb, petrep);

   ret = gdi2_send_message_pb(1, prognames[EXECD], 1, hostname,
                              TAG_JOB_EXECUTION, &pb, &dummymid);

   clear_packbuffer(&pb);

   lFreeElem(&petrep);

   if (ret != CL_RETVAL_OK) {
      sprintf(lasterror, MSG_GDI_SENDTASKTOEXECDFAILED_SS, hostname, cl_get_error_text(ret));
      DRETURN(nullptr);
   }

   /* add list into our remote task list */
   rt = lAddElemStr(&remote_task_list, RT_tid, "none", RT_Type);
   lSetHost(rt, RT_hostname, hostname);
   lSetUlong(rt, RT_state, RT_STATE_WAIT4ACK);

   rcv_from_execd(OPT_SYNCHRON, TAG_JOB_EXECUTION);

   auto tid = (sge_tid_t) lGetString(rt, RT_tid);

   if (strcmp(tid, "none") == 0) {
      tid = nullptr;
      sprintf(lasterror, MSG_GDI_EXECDONHOSTDIDNTACCEPTTASK_S, hostname);
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
int sge_qwaittid(sge_tid_t tid, int *status, int options) {
   lListElem *rt = nullptr;
   int ret, rcv_opt = 0;

   DENTER(TOP_LAYER);

   if (!(options & WNOHANG))
      rcv_opt |= OPT_SYNCHRON;

   if (tid != nullptr && !(rt = LOCATE_RTASK(tid))) {
      sprintf(lasterror, MSG_GDI_TASKNOTEXIST_S, tid);
      DRETURN(-1);
   }

   while ((rt && /* definite one searched */
           lGetUlong(rt, RT_state) != RT_STATE_EXITED && /* not exited */
           lGetUlong(rt, RT_state) == RT_STATE_WAIT4ACK) /* waiting for ack */
          || (!rt && /* anybody searched */
              !lGetElemUlong(remote_task_list, RT_state, RT_STATE_EXITED) && /* none exited */
              lGetElemUlong(remote_task_list, RT_state, RT_STATE_WAIT4ACK))) /* but one is waiting for ack */ {
      /* wait for incoming messeges about exited tasks */
      if ((ret = rcv_from_execd(rcv_opt, TAG_TASK_EXIT))) {
         DRETURN((ret < 0) ? -1 : 0);
      }
   }

   if (status)
      *status = (int)lGetUlong(rt, RT_status);
   lSetUlong(rt, RT_state, RT_STATE_WAITED);

   DRETURN(0);
}

/* return 
   0  reaped a task cleanly  
   1  no message (asynchronuous mode)
   -1 got an error
 
    NOTES
       MT-NOTE: rcv_from_execd() is not MT safe

*/
static int rcv_from_execd(int options, int tag) {
   int ret;
   char *msg = nullptr;
   u_long32 msg_len = 0;
   sge_pack_buffer pb;
   u_short from_id;
   char host[1024];

   lListElem *rt_rcv;
   u_long32 exit_status = 0;
   sge_tid_t tid = nullptr;

   DENTER(TOP_LAYER);

   host[0] = '\0';
   from_id = 1;
   do {
      /* FIX_CONST */
      ret = gdi2_receive_message((char *) prognames[EXECD], &from_id, host, &tag, &msg, &msg_len,
                                 (options & OPT_SYNCHRON) ? 1 : 0);

      if (ret != CL_RETVAL_OK && ret != CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
         sprintf(lasterror, MSG_GDI_MESSAGERECEIVEFAILED_SI, cl_get_error_text(ret), ret);
         DRETURN(-1);
      }
   } while (options & OPT_SYNCHRON && ret == CL_RETVAL_SYNC_RECEIVE_TIMEOUT);

   if (ret == CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
      DRETURN(1);
   }

   ret = init_packbuffer_from_buffer(&pb, msg, msg_len);
   if (ret != PACK_SUCCESS) {
      sprintf(lasterror, MSG_GDI_ERRORUNPACKINGGDIREQUEST_S, cull_pack_strerror(ret));
      DRETURN(-1);
   }

   switch (tag) {
      case TAG_TASK_EXIT:
         unpackstr(&pb, &tid);
         unpackint(&pb, &exit_status);
         break;
      case TAG_JOB_EXECUTION:
         unpackstr(&pb, &tid);
         break;
      default:
         break;
   }

   clear_packbuffer(&pb);

   switch (tag) {
      case TAG_TASK_EXIT:
         /* change state in exited task */
         if (!(rt_rcv = lGetElemStrRW(remote_task_list, RT_tid, tid))) {
            sprintf(lasterror, MSG_GDI_TASKNOTFOUND_S, tid);
            sge_free(&tid);
            DRETURN(-1);
         }

         lSetUlong(rt_rcv, RT_status, exit_status);
         lSetUlong(rt_rcv, RT_state, RT_STATE_EXITED);
         break;

      case TAG_JOB_EXECUTION:
         /* search task without taskid */
         if (!(rt_rcv = lGetElemStrRW(remote_task_list, RT_tid, "none"))) {
            sprintf(lasterror, MSG_GDI_TASKNOTFOUNDNOIDGIVEN_S, tid);
            DRETURN(-1);
         }
         lSetString(rt_rcv, RT_tid, tid);
         break;

      default:
         break;
   }

   sge_free(&tid);
   DRETURN(0);
}

