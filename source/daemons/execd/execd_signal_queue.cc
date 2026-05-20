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
#include <cerrno>
#include <cstring>
#include <fcntl.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_stdio.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#if defined(DARWIN)
#  include "uti/sge_uidgid.h"
#endif

#include "spool/classic/read_write_job.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_report.h"

#include "comm/commlib.h"

#include "symbols.h"
#include "job_report_execd.h"
#include "execd_signal_queue.h"
#include "sig_handlers.h"
#include "execd.h"
#include "dispatcher.h"
#include "reaper_execd.h"
#include "admin_mail.h"
#include "sge_report_execd.h"
#include "mail.h"
#include "get_path.h"
#include "sge.h"
#include "msg_execd.h"
#include "msg_daemons_common.h"

extern volatile int shut_me_down;

/**************************************************************************
 called from dispatcher

 counterpart in qmaster: c_qmod.c
 **************************************************************************/
int do_signal_queue(ocs::gdi::ClientServerBase::struct_msg_t *aMsg, sge_pack_buffer *apb)
{
   DENTER(TOP_LAYER);

   lListElem *jep;
   bool found = false;
   uint32_t jobid, signal, jataskid;
   char *qname = nullptr;

   if (unpackint(&(aMsg->buf), &jobid) != 0 ||
       unpackint(&(aMsg->buf), &jataskid) != 0 ||
       unpackstr(&(aMsg->buf), &qname) != 0 || /* mallocs qname !! */
       unpackint(&(aMsg->buf), &signal)) {     /* signal doesn't need to be packed */
      sge_free(&qname);
      DRETURN(1);
   }

   DPRINTF("===>DELIVER_SIGNAL: %s >%s< Job(s) " sge_u32"." sge_u32" \n", sge_sig2str(signal), qname? qname: "<nullptr>", jobid, jataskid);

   if (aMsg->tag == ocs::gdi::ClientServerBase::TAG_SIGJOB) { /* signal a job / task */
      pack_ack(apb, ACK_SIGJOB, jobid, jataskid, nullptr);

      found = signal_job(jobid, jataskid, signal) == 0;
   } else {            /* signal a queue */
      pack_ack(apb, ACK_SIGQUEUE, jobid, jataskid, qname);

      for_each_rw(jep, *ocs::DataStore::get_master_list(SGE_TYPE_JOB)) {
         lListElem *gdil_ep, *master_q;
         lListElem *jatep;
         const char *qnm;

         for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
            if (lGetUlong(jatep, JAT_status) == JSLAVE) {
               break;
            }

            /* iterate through all queues of a parallel job -
               this is done to ensure that signal delivery is also
               forwarded to the job in case the master queue keeps still active */
            for_each_rw (gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
               master_q = lGetObject(gdil_ep, JG_queue);
               if (master_q != nullptr) {
                  qnm =  lGetString(master_q, QU_full_name);
                  if (strcmp(qname, qnm) == 0) {
                     char tmpstr[SGE_PATH_MAX];

                     /* job signaling triggerd by a queue signal */
                     snprintf(tmpstr, sizeof(tmpstr), "%s (%s)", sge_sig2str(signal), qnm);
                     /* if the queue gets suspended and the job is already suspended
                        we do not deliver a signal */
                     if (signal == SGE_SIGSTOP) {
                        qinstance_state_set_manual_suspended(master_q, true);
                        if (!VALID(JSUSPENDED, lGetUlong(jatep, JAT_state))) {
                           if (lGetUlong(jep, JB_checkpoint_attr)& CHECKPOINT_SUSPEND) {
                              INFO(MSG_JOB_INITMIGRSUSPQ_U, lGetUlong(jep, JB_job_number));
                              signal = SGE_MIGRATE;
                           }
                           if (sge_execd_deliver_signal(signal, jep, jatep) == 0) {
                              sge_send_suspend_mail(signal,master_q, jep, jatep);
                           }
                        }
                     } else {
                        /* if the signal is a unsuspend and the job is suspended
                           we do not deliver a signal */
                        if (signal == SGE_SIGCONT) {
                           qinstance_state_set_manual_suspended(master_q, false);
                           if (!VALID(JSUSPENDED, lGetUlong(jatep, JAT_state))) {
                              if ( sge_execd_deliver_signal(signal, jep, jatep) == 0) {
                                 sge_send_suspend_mail(signal,master_q ,jep, jatep);
                              }
                           }
                        } else {
                           sge_execd_deliver_signal(signal, jep, jatep);
                        }
                     }
                     found = true;

                     if (!mconf_get_simulate_jobs()) {
                        job_write_spool_file(jep,
                           lGetUlong(lFirst(lGetList(jep, JB_ja_tasks)),
                           JAT_task_number), nullptr, SPOOL_WITHIN_EXECD);
                     }

                  }
               }
            }
         }
      }
      /*
      ** when a queue state has changed
      ** we release the block on sending admin mails
      */
      adm_mail_reset(BIT_ADM_QCHANGE);
   }

   // Found tells us if a job was signaled (either due to a job signal or a queue signal).
   // If it was a job signal, but we didn't find the job, then ack it to get rid of it in sge_qmaster.
   if (!found && aMsg->tag == ocs::gdi::ClientServerBase::TAG_SIGJOB) {
      lListElem *jr = get_job_report(jobid, jataskid, nullptr);

      if (jr == nullptr) {
         ERROR(MSG_SHEPHERD_MISSINGJOBXYINJOBREPORT_UU, jobid, jataskid);
         jr = add_job_report(jobid, jataskid, nullptr, nullptr);
      }
      if (jr != nullptr) {
         remove_acked_job_exit(jobid, jataskid, nullptr, jr);
      }
      job_unknown(jobid, jataskid, qname);
   }

   sge_free(&qname);

   DRETURN(0);
}

/*************************************************************************
 execds function to deliver a signal to the job. This cant be done direct,
 because there is the shepherd between.
 We do a signal mapping for the shepherd.

 sent to shepherd  |  signal to deliver to job
 ------------------|---------------------------
 SIGTTIN           | given in "signal"-file
 SIGUSR1           | SIGUSR1
 SIGUSR2           | SIGXCPU
 SIGCONT           | SIGCONT
 SIGWINCH          | SIGSTOP (not used anymore, use SIGTTIN to fwd SIGSTOP )
 SIGTSTP           | SIGKILL

 SIGTTIN forces the shepherd to look into the "signal" file. This file
 contains the number of the signal to send.

 The job will get a SIGUSR1 as notification for SIGSTOP and
                    SIGUSR2                     SIGKILL if
 The job has been submitted with -notify and the notification time of the
 queue is > 0.

 returns
   0 on success
   1 if job is supposed to be not in a healthy state and thus
     should be removed by the calling context
 *************************************************************************/
int sge_execd_deliver_signal(uint32_t sig, const lListElem *jep, lListElem *jatep)
{
   int queue_already_suspended;
   int getridofjob = 0;

   DENTER(TOP_LAYER);

   INFO(MSG_JOB_SIGNALTASK_UUS, lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number), sge_sig2str(sig));

   // for simulated jobs fill in the job report
   if (mconf_get_simulate_jobs()) {
      if (sig == SGE_SIGKILL) {
         simulated_job_exit(jep, jatep, sig);
      }

      DRETURN(0);
   }

/*
   DPRINTF(("(sig==SGE_MIGRATE) = %d (ckpt on suspend) = %d %d\n",
      (sig == SGE_MIGRATE), lGetUlong(jep, JB_checkpoint_attr)|CHECKPOINT_SUSPEND,
         lGetUlong(jep, JB_checkpoint_attr)));
*/
   /* Simply apply signal to all subtasks of the job
      except in case of SGE_MIGRATE when there is a
      ckpt env with "migrate on suspend" configured */
   queue_already_suspended = (lGetUlong(jatep, JAT_state)&JSUSPENDED);
   if (!(sig == SGE_MIGRATE
         && (lGetUlong(jep, JB_checkpoint_attr)|CHECKPOINT_SUSPEND))
         && !queue_already_suspended) {
      /* signal each pe task */
      for_each_ep_lv(petep, lGetList(jatep, JAT_task_list)) {
         if (sge_kill((int)lGetUlong(petep, PET_pid), sig,
                      lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number),
                      lGetString(petep, PET_id)) == -2) {
            getridofjob = 1;
         }
      }
   }

   if (lGetUlong(jatep, JAT_status) != JSLAVE) {
      if (sge_kill((int)lGetUlong(jatep, JAT_pid), sig, lGetUlong(jep, JB_job_number),
                        lGetUlong(jatep, JAT_task_number), nullptr) == -2) {
         getridofjob = 1;
      }
   }

   DRETURN(getridofjob);
}

/****** execd_signal_queue/sge_send_suspend_mail() *****************************
*  NAME
*     sge_send_suspend_mail() -- send suspend / condinue mail if enabled
*
*  SYNOPSIS
*     void sge_send_suspend_mail(uint32_t signal, lListElem *master_q,
*     lListElem *jep, lListElem *jatep)
*
*  FUNCTION
*     This function will send the suspend/continue mail to the job owner
*     or to the users defined with the -M option of qsub. The mail is
*     only sent when the user has specified the -m s flag.
*
*     The mail is sent when the execd is signaling the job with
*     SGE_SIGSTOP(suspend) / SGE_SIGCONT(continue)
*
*  INPUTS
*     uint32_t signal     - type of signal (SGE_SIGSTOP/SGE_SIGCONT)
*     lListElem *master_q - pointer to QU_Type  cull list element
*                           of job (not used)
*     lListElem *jep      - pointer to JB_Type  cull list element
*                           of job
*     lListElem *jatep    - pointer to JAT_Type cull list element
*                           of job
*******************************************************************************/
void sge_send_suspend_mail(uint32_t signal, lListElem *master_q, lListElem *jep, lListElem *jatep) {

   uint32_t mail_options;

   DENTER(TOP_LAYER);

   mail_options = lGetUlong(jep, JB_mail_options);

   /* only if mail at suspendsion is enabled */
   if (VALID(MAIL_AT_SUSPENSION, mail_options)) {
       const lList *mail_users      = nullptr;

       uint32_t jobid         = 0;
       uint32_t taskid        = 0;
       uint64_t job_sub_time  = 0;
       uint64_t job_exec_time = 0;

       char mail_subject[MAX_STRING_SIZE];
       char mail_body[MAX_STRING_SIZE];
       char job_sub_time_str[256];
       char job_exec_time_str[256];

       const char *job_name = nullptr;
       const char *job_master_queue = nullptr;
       const char *job_owner = nullptr;
       const char *mail_type = "unknown";

       dstring ds;
       char buffer[128];

       sge_dstring_init(&ds, buffer, sizeof(buffer));


       /* get values */
       if (jep != nullptr) {
          job_sub_time = lGetUlong64(jep, JB_submission_time);
          jobid        = lGetUlong(jep, JB_job_number);
          mail_users   = lGetList(jep, JB_mail_list);
          job_name     = lGetString(jep, JB_job_name);
          job_owner    = lGetString(jep, JB_owner);
        }

       if (jatep != nullptr) {
          job_exec_time    = lGetUlong64(jatep, JAT_start_time);
          taskid           = lGetUlong(jatep, JAT_task_number);
          job_master_queue = lGetString(jatep, JAT_master_queue);
       }

       /* check strings */
       if (job_name == nullptr) {
           job_name = MSG_MAIL_UNKNOWN_NAME;
       }
       if (job_master_queue == nullptr) {
           job_master_queue = MSG_MAIL_UNKNOWN_NAME;
       }
       if (job_owner == nullptr) {
           job_owner = MSG_MAIL_UNKNOWN_NAME;
       }


       /* make human readable time format */
       snprintf(job_sub_time_str, sizeof(job_sub_time_str), "%s", sge_ctime64(job_sub_time, &ds));
       snprintf(job_exec_time_str, sizeof(job_exec_time_str), "%s", sge_ctime64(job_exec_time, &ds));

       if (signal == SGE_SIGSTOP) {
          /* suspended */
          if (job_is_array(jep)) {
              snprintf(mail_subject, sizeof(mail_subject), MSG_MAIL_SUBJECT_JA_TASK_SUSP_UUS, jobid, taskid, job_name);
          } else {
              snprintf(mail_subject, sizeof(mail_subject), MSG_MAIL_SUBJECT_JOB_SUSP_US, jobid, job_name);
          }
          mail_type = MSG_MAIL_TYPE_SUSP;
       } else if (signal == SGE_SIGCONT ) {
          /* continued */
          if (job_is_array(jep)) {
              snprintf(mail_subject, sizeof(mail_subject), MSG_MAIL_SUBJECT_JA_TASK_CONT_UUS, jobid, taskid, job_name);
          } else {
              snprintf(mail_subject, sizeof(mail_subject), MSG_MAIL_SUBJECT_JOB_CONT_US, jobid, job_name);
          }
          mail_type = MSG_MAIL_TYPE_CONT;
       } else {
          DPRINTF("no suspend or continue signaling\n");
          DRETURN_VOID;
       }

       /* create mail body */
       snprintf(mail_body, sizeof(mail_body), MSG_MAIL_BODY_SSSSS, mail_subject, job_master_queue, job_owner, job_sub_time_str, job_exec_time_str);
       snprintf(mail_body, sizeof(mail_body), "\n");

       cull_mail(EXECD, mail_users, mail_subject, mail_body, mail_type );
   }
   DRETURN_VOID;
}

/**
 * @brief Forward a signal to a job via its shepherd (CS-2226).
 *
 * Two execd->shepherd transports exist:
 *
 *  - signal_pipe FIFO (per job, ordered, non-coalescing): the shepherd
 *    poll()s it and feeds each record into its existing signal ring
 *    buffer. Used for every signal that would otherwise go through the
 *    legacy single "signal" file - i.e. SIGSTOP/suspend and any arbitrary
 *    qsig signal (SIGTERM, SIGINT, SIGHUP, ...) - AND for SIGCONT/resume.
 *    Reason: the legacy "signal"-file + SIGTTIN relay coalesces. The file
 *    is truncate-rewritten and SIGTTIN is a non-queuing standard signal,
 *    so a fast suspend->resume (or any rapid burst of file-mediated
 *    signals) can collapse into a single delivery, leaving the job e.g.
 *    frozen forever (the CS-2226 bug; made deterministic by the slow
 *    systemd cgroup-v2 freeze_unit D-Bus call widening the race window).
 *
 *  - direct signal (out-of-band, unchanged): the dedicated control
 *    signals keep their own real-signal delivery to the shepherd and are
 *    deliberately NOT routed through the FIFO, so a wedged/full FIFO or a
 *    stuck shepherd poll loop can never make a job unkillable or block
 *    checkpoint/migration:
 *        SIGKILL -> shepherd SIGTSTP  (terminate)
 *        SIGXCPU -> shepherd SIGUSR2  (delivered to job as SIGXCPU)
 *        SIGTTOU                       (checkpoint / migration initiation)
 *        SIGUSR1                       (job notify)
 *
 * Shepherd-side mapping of what finally reaches the job (legacy table;
 * still applies because the FIFO record carries the same post-mapping
 * "sig" value the "signal" file would have held):
 *
 *      sent to shepherd     | shepherd delivers to the job
 *      ---------------------|------------------------------------------
 *      FIFO record / SIGTTIN| the signal number in the record/"signal"
 *      SIGTTOU              | checkpoint/migration (shepherd knows it)
 *      SIGUSR1              | SIGUSR1
 *      SIGUSR2              | SIGXCPU
 *      SIGCONT              | SIGCONT      (legacy fallback path only)
 *      SIGTSTP              | SIGKILL
 *
 * FIFO use is additionally gated on pe_task_id == nullptr: the FIFO is
 * per job-shepherd; per-pe-task signalling stays on the legacy path.
 *
 * Back-compat / fallback: if the FIFO is absent (ENOENT: old execd or
 * mkfifo failed) or has no reader (ENXIO: old-kernel shepherd that
 * dropped it / has not opened it yet) delivery falls through to the
 * legacy "signal"-file + SIGTTIN relay (or, for SIGCONT, the legacy
 * direct SIGCONT). On any other FIFO open error or a short write the
 * execd is never blocked: it falls through to the shepherd-liveness
 * check and the qmaster pending-signal/resend machinery retries.
 *
 * Shepherd gone / execd restarted: the shepherd holds the FIFO O_RDWR
 * for the whole job, so once it exits there is no reader and the execd's
 * O_WRONLY|O_NONBLOCK open returns ENXIO (POSIX; it does NOT block - that
 * is why O_NONBLOCK is mandatory here). ENXIO -> legacy path -> kill()
 * the now-dead shepherd -> ESRCH -> CheckShepherdStillRunning, i.e. a
 * harmless no-op (a suspend/resume for an already-exited job is moot).
 * qmaster stops resending once it receives the job's final exit report
 * after the execd is back. A record written in the brief instant while
 * the shepherd is still exiting may be discarded with the FIFO - that is
 * acceptable best-effort at end-of-job.
 *
 * @param pid         shepherd pid to signal.
 * @param sge_signal  SGE_SIG* signal to deliver to the job.
 * @param job_id      job id (active-job dir / diagnostics).
 * @param ja_task_id  array task id.
 * @param pe_task_id  pe-task id, or nullptr for the job shepherd.
 * @return  0 on success; -2 if the shepherd is gone; -1 on other errors.
 */
int sge_kill(int pid, uint32_t sge_signal, uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id)
{
   int sig;
   int direct_signal;   /* deliver per signal or per file */
   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   DENTER(TOP_LAYER);

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   if (!pid) {
      DPRINTF("sge_kill won't kill pid 0!\n");
      DRETURN(-1);
   }

   /* mapping from SGE_SIG... which is equal on all platforms to the platform
      specific SIG... */
   sig = sge_unmap_signal(sge_signal);

   /*  now do the mapping for the shepherd

       sent to shepherd  |  shepherd signal to deliver to job
       ------------------|---------------------------
       SIGTTIN           | given in "signal"-file
       SIGTTOU           | SIGTTOU (initiate migration  - shepherd knows the signal)
       SIGUSR1           | SIGUSR1
       SIGUSR2           | SIGXCPU
       SIGCONT           | SIGCONT
       SIGWINCH          | SIGSTOP (not used anymore, use SIGTTIN to fwd SIGSTOP )
       SIGTSTP           | SIGKILL
   */
   direct_signal = 1;

   switch (sig) {
   case SIGKILL:
      sig = SIGTSTP;
      break;
#if defined(SIGXCPU)
   case SIGXCPU:
      sig = SIGUSR2;
      break;
#endif
   case SIGTTOU:
   case SIGUSR1:
   case SIGCONT:
      break;
   /* We now fwd SIGSTOP using file(SIGTTIN) rather than SIGWINCH, refer CR6623174 */
   default:
      direct_signal = 0;        /* communication has to be done via file */
   }

   /* CS-2226: route every file-mediated signal (SIGSTOP/suspend + any
    * arbitrary signal: SIGTERM, SIGINT, ...) and SIGCONT/resume
    * through the per-job, non-coalescing "signal_pipe" FIFO. The
    * predicate excludes the dedicated direct control signals
    * (SIGKILL->SIGTSTP, SIGXCPU->SIGUSR2, SIGTTOU, SIGUSR1), which keep
    * their out-of-band direct delivery so a wedged FIFO can never make a
    * job unkillable or block checkpoint/migration. FIFO is per
    * job-shepherd (pe_task_id == nullptr). The record carries the same
    * post-mapping "sig" value the legacy "signal" file would have held,
    * so the shepherd dispatches it identically. ENOENT/ENXIO (no FIFO /
    * no reader: old execd or old-kernel shepherd) or any write problem
    * -> fall through to the legacy "signal"+SIGTTIN / direct path below;
    * the execd is never blocked (qmaster resends). */
   if (pe_task_id == nullptr && (!direct_signal || sge_signal == SGE_SIGCONT)) {
      dstring ff = DSTRING_INIT;

      sge_get_active_job_file_path(&ff, job_id, ja_task_id, nullptr, "signal_pipe");
      int ffd = open(sge_dstring_get_string(&ff), O_WRONLY | O_NONBLOCK);
      if (ffd >= 0) {
         static unsigned long sr_seq = 0;
         char rec[64];
         int rl = snprintf(rec, sizeof(rec), "%lu %d\n", ++sr_seq, sig);
         ssize_t wn = write(ffd, rec, (size_t)rl);
         close(ffd);
         sge_dstring_free(&ff);
         if (wn == (ssize_t)rl) {
            DPRINTF("queued %s via signal_pipe FIFO for job " SFN "\n", sge_sig2str(sge_signal),
                    job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
            DRETURN(0);
         }
         /* short/failed write (EAGAIN/EPIPE/...): do not block the execd -
          * let the shepherd-liveness check + qmaster resend handle it. */
         DPRINTF("signal_pipe FIFO write for job " SFN " failed - falling back\n",
                 job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
         goto CheckShepherdStillRunning;
      } else if (errno != ENOENT && errno != ENXIO) {
         /* unexpected open error: don't block - shepherd-liveness check +
          * qmaster resend handle it. */
         sge_dstring_free(&ff);
         goto CheckShepherdStillRunning;
      }
      /* ENOENT (no FIFO) or ENXIO (no reader: old-kernel shepherd dropped
       * it / not opened yet) -> legacy "signal"+SIGTTIN / direct path. */
      sge_dstring_free(&ff);
   }

   DPRINTF("signalling job/task " SFN ", pid " pid_t_fmt " with %d\n",
           job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), pid, sig);
   if (!direct_signal) {
      dstring fname = DSTRING_INIT;
      FILE *fp;

      sge_get_active_job_file_path(&fname, job_id, ja_task_id, pe_task_id, "signal");
      if (!(fp = fopen(sge_dstring_get_string(&fname), "w"))) {
         ERROR(MSG_EXECD_WRITESIGNALFILE_S, sge_dstring_get_string(&fname));
         sge_dstring_free(&fname);
         goto CheckShepherdStillRunning;
      }

      fprintf(fp, "%d\n", sig);
      sge_dstring_free(&fname);
      FCLOSE(fp);
   }

#if defined(DARWIN)
   sge_switch2start_user();
#endif
   if (kill(pid, direct_signal?sig:SIGTTIN)) {
#if defined(DARWIN)
      sge_switch2admin_user();
#endif
      if (errno == ESRCH)
         goto CheckShepherdStillRunning;
      DRETURN(-1);
   }

   DRETURN(0);

FCLOSE_ERROR:
CheckShepherdStillRunning:
   {
      dstring path = DSTRING_INIT;
      SGE_STRUCT_STAT statbuf;

      sge_get_active_job_file_path(&path, job_id, ja_task_id, pe_task_id, nullptr);

      if (!SGE_STAT(sge_dstring_get_string(&path), &statbuf) && S_ISDIR(statbuf.st_mode)) {
         sge_sig_handler_dead_children = 1; /* maybe we've lost a SIGCHLD */
         sge_dstring_free(&path);
         DRETURN(0);
      } else {
         WARNING(MSG_JOB_DELIVERSIGNAL_ISSIS, sig, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring), sge_sig2str(sge_signal), pid, strerror(errno)); sge_dstring_free(&path);
         DRETURN(-2);
      }
   }
}

/*------------------------------------------------------------

NAME

   signal_job()

DESCRIPTION

   Tries to signal the job.

RETURN

   0 job was found and was signaled
   1 job was not found you better get rid of it to prevent
     infinite pingpong effects
   ------------------------------------------------------------ */
int signal_job(uint32_t jobid, uint32_t jataskid, uint32_t signal)
{
   lListElem *jep;
   uint32_t state;
   lListElem *master_q;
   lListElem *jatep = nullptr;
   int getridofjob = 0;

   int suspend_change = 0;
   int send_mail = 0;

   DENTER(TOP_LAYER);

   /* search appropriate array task and job */
   if (!execd_get_job_ja_task(jobid, jataskid, &jep, &jatep, false)) {
      DRETURN(1);
   }

   master_q = lGetObject(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), JG_queue);

   DPRINTF("sending %s to job " sge_u32 "." sge_u32 "\n", sge_sig2str(signal), jobid, jataskid);
   if (signal == SGE_SIGCONT) {
      state = lGetUlong(jatep, JAT_state);
      if (ISSET(state, JSUSPENDED)) {
         suspend_change = 1;
      }
      CLEARBIT(JSUSPENDED, state);
      SETBIT(JRUNNING, state);
      lSetUlong(jatep, JAT_state, state);

      /* If the one of the queues is suspended
         and we unsuspend the job.
         The Job should stay sleeping */

      if (!qinstance_state_is_manual_suspended(master_q)) {
         getridofjob = sge_execd_deliver_signal(signal, jep, jatep);
         if ((!getridofjob) && (suspend_change == 1) ) {
            send_mail = 1;
         }
      } else {
         DPRINTF("Queue is suspended -> do nothing\n");
      }
   } else {
      if ((signal == SGE_SIGSTOP) && (lGetUlong(jep, JB_checkpoint_attr) & CHECKPOINT_SUSPEND)) {
         INFO(MSG_JOB_INITMIGRSUSPJ_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number));
         signal = SGE_MIGRATE;
         getridofjob = sge_execd_deliver_signal(signal, jep, jatep);
      } else if (signal == SGE_SIGSTOP) {
         state = lGetUlong(jatep, JAT_state);
         if (!ISSET(state, JSUSPENDED)) {
            suspend_change = 1;
         }
         SETBIT(JSUSPENDED, state);
         CLEARBIT(JRUNNING, state);
         lSetUlong(jatep, JAT_state, state);

         /* if this is a stop signal for a job
            which is in at least ONE queue
            which is already stopped we
            do not deliver the signal */

         getridofjob = sge_execd_deliver_signal(signal, jep, jatep);
         if ((!getridofjob) && (suspend_change == 1)) {
            if (!qinstance_state_is_manual_suspended(master_q)) {
               send_mail = 2;
            }
         }
      } else {
         if (signal == SGE_SIGKILL) {
            /*
             * At this point job termination is triggered and JDELETED state is set.
             * Based on that state it should be possible to provide better diagnosis
             * information if the job dies due to a signal (Issue: #483). Possibly
             * SGE_SIGXCPU should be treated similarly.
             * ja_task_message_add(jatep, 1, err_str) could be used to intermediately
             * store job termination reason until it gets reaped later on.
             */
            state = lGetUlong(jatep, JAT_state);
            SETBIT(JDELETED, state);
            lSetUlong(jatep, JAT_state, state);
            DPRINTF("SIGKILL of job " sge_u32 "\n", jobid);
         }
         getridofjob = sge_execd_deliver_signal(signal, jep, jatep);
      }
   }

   /* now save this job/queue so we are up to date on restart */
   if (!getridofjob) {
      if (!mconf_get_simulate_jobs()) {
         job_write_spool_file(jep, jataskid, nullptr, SPOOL_WITHIN_EXECD);
         // set_enforce_cleanup_old_jobs();
      }
      /* write mail */
      if (send_mail == 1) {
         sge_send_suspend_mail(SGE_SIGCONT,master_q, jep, jatep);
      }
      if (send_mail == 2) {
         sge_send_suspend_mail(SGE_SIGSTOP,master_q, jep, jatep);
      }

   } else {
      DPRINTF("Job  " sge_u32 "." sge_u32" is no longer running\n", jobid, jataskid);
   }
   DRETURN(getridofjob);
}
