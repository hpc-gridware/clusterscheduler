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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <sys/stat.h>
#include <cerrno>

#include "uti/ocs_TerminationManager.h"
#include "uti/ocs_cond.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"

#include "japi/japi.h"
#include "japi/japiP.h"

#include "gdi/ocs_gdi_security.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "sig_handlers.h"
#include "basis_types.h"
#include "usage.h"
#include "parse_job_cull.h"
#include "ocs_client_parse.h"
#include "ocs_client_job.h"
#include "parse_qsub.h"
#include "msg_clients_common.h"
#include "msg_qsub.h"
#include "msg_qmaster.h"


extern char **environ;
static pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t exit_cv = PTHREAD_COND_INITIALIZER;
static bool exited = false;

static char *get_bulk_jobid_string(long job_id, int start, int end, int step);
static void qsub_setup_sig_handlers();
static void qsub_terminate();
static void *sig_thread(void *dummy);
static int report_exit_status(int stat, const char *jobid);
static void error_handler(const char *message);

/************************************************************************/
int
main(int argc, const char **argv)
{
   DENTER_MAIN(TOP_LAYER, "qsub");
   lList *opts_cmdline = nullptr;
   lList *opts_defaults = nullptr;
   lList *opts_scriptfile = nullptr;
   lList *opts_all = nullptr;
   lListElem *job = nullptr;
   lList *alp = nullptr;
   int exit_status = 0;
   int just_verify;
   int tmp_ret;
   int is_immediate = 0;
   dstring session_key_out = DSTRING_INIT;
   dstring diag = DSTRING_INIT;
   dstring jobid = DSTRING_INIT;
   u_long32 start, end, step;
   u_long32 num_tasks;
   u_long32 count;
   int stat;
   char *jobid_string = nullptr;
   bool has_terse = false;
   drmaa_attr_values_t *jobids = nullptr;

   u_long32 prog_number = 0;
   u_long32 myuid = 0;
   const char *sge_root = nullptr;
   const char *cell_root = nullptr;
   const char *username = nullptr;
   const char *qualified_hostname = nullptr;
   const char *unqualified_hostname = nullptr;
   const char *mastername = nullptr;

   /* Set up the program information name */
   sge_setup_sig_handlers(QSUB);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   ocs::uti::condition_initialize(&exit_cv);

   DPRINTF("Initializing JAPI\n");

   if (japi_init(nullptr, nullptr, nullptr, QSUB, false, nullptr, &diag)
                                                      != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "\n");
      fprintf(stderr, MSG_QSUB_COULDNOTINITIALIZEENV_S,
              sge_dstring_get_string(&diag));
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   prog_number = component_get_component_id();
   myuid = component_get_uid();
   sge_root = bootstrap_get_sge_root();
   cell_root = bootstrap_get_cell_root();
   username = component_get_username();
   qualified_hostname = component_get_qualified_hostname();
   unqualified_hostname = component_get_unqualified_hostname();
   mastername = ocs::gdi::ClientBase::gdi_get_act_master_host(false);

   /*
    * read switches from the various defaults files
    */
   opt_list_append_opts_from_default_files(prog_number, cell_root, username, &opts_defaults, &alp, environ);
   tmp_ret = answer_list_print_err_warn(&alp, nullptr, nullptr, MSG_WARNING);
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }

   /*
    * append the commandline switches to the list
    */
   opt_list_append_opts_from_qsub_cmdline(prog_number, &opts_cmdline, &alp,
                                          argv + 1, environ);
   tmp_ret = answer_list_print_err_warn(&alp, nullptr, "qsub: ", MSG_QSUB_WARNING_S);
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }

   /*
    * show usage if -help was in commandline
    */
   if (opt_list_has_X(opts_cmdline, "-help")) {
      sge_usage(QSUB, stdout);
      sge_exit(0);
   }

   /*
    * Check if -terse is requested
    */
   if (opt_list_has_X(opts_cmdline, "-terse")) {
      has_terse = true;
   }

   /*
    * We will only read commandline options from scripfile if the script
    * itself should not be handled as binary
    */
   if (opt_list_is_X_true(opts_cmdline, "-b") ||
       (!opt_list_has_X(opts_cmdline, "-b") &&
        opt_list_is_X_true(opts_defaults, "-b"))) {
      DPRINTF("Skipping options from script due to -b option\n");
   } else {
      opt_list_append_opts_from_script(prog_number,
                                       &opts_scriptfile, &alp,
                                       opts_cmdline, environ);
      tmp_ret = answer_list_print_err_warn(&alp, nullptr, MSG_QSUB_COULDNOTREADSCRIPT_S, MSG_WARNING);
      if (tmp_ret > 0) {
         sge_exit(tmp_ret);
      }
   }

   /*
    * Merge all commandline options and interpret them
    */
   opt_list_merge_command_lines(&opts_all, &opts_defaults,
                                &opts_scriptfile, &opts_cmdline);

   opt_list_verify_scope(opts_all, &alp);
   tmp_ret = answer_list_print_err_warn(&alp, nullptr, nullptr, nullptr);
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }

   u_long32 sync_opt = SYNC_NO;
   alp = cull_parse_job_parameter(myuid, username, cell_root, unqualified_hostname, qualified_hostname, opts_all, &job, &sync_opt);

   job_set_command_line(job, argc, argv);

   if (sge_getenv("SGE_DEBUG_DUMP_JOB") != nullptr) {
      lWriteElemTo(job, stdout);
   }

   tmp_ret = answer_list_print_err_warn(&alp, nullptr, "qsub: ", MSG_WARNING);
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }

   if (set_sec_cred(sge_root, mastername, job, &alp) != 0) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   /* Check if job is immediate */
   is_immediate = (int)JOB_TYPE_IS_IMMEDIATE(lGetUlong(job, JB_type));
   DPRINTF("Job is%s immediate\n", is_immediate ? "" : " not");

   DPRINTF("Everything ok\n");

   if (lGetUlong(job, JB_verify)) {
      cull_show_job(job, 0, false);
      sge_exit(0);
   }

   if (is_immediate || sync_opt != SYNC_NO) {
      pthread_t sigt;

      qsub_setup_sig_handlers();

      if (pthread_create(&sigt, nullptr, sig_thread, (void *)nullptr) != 0) {
         fprintf(stderr, "\n");
         fprintf(stderr, MSG_QSUB_COULDNOTINITIALIZEENV_S,
                 " error preparing signal handling thread");
         fprintf(stderr, "\n");

         exit_status = 1;
         goto Error;
      }

      if (japi_enable_job_wait(username, unqualified_hostname, nullptr, &session_key_out, error_handler, &diag) ==
                                       DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE) {
         const char *msg = sge_dstring_get_string(&diag);
         fprintf(stderr, "\n");
         fprintf(stderr, MSG_QSUB_COULDNOTINITIALIZEENV_S,
                 msg?msg:" error starting event client thread");
         fprintf(stderr, "\n");

         exit_status = 1;
         goto Error;
      }
   }

   job_get_submit_task_ids(job, &start, &end, &step);
   num_tasks = (end - start) / step + 1;

   if (num_tasks > 1) {
      int error = japi_run_bulk_jobs(&jobids, &job, start, end, step, &diag);
      if (error != DRMAA_ERRNO_SUCCESS) {
         /* No active session here means that japi_enable_job_wait() was
          * interrupted by the signal handler, in which case we just break out
          * quietly. */
         if (error != DRMAA_ERRNO_NO_ACTIVE_SESSION) {
            fprintf(stderr, MSG_QSUB_COULDNOTRUNJOB_S,
                    sge_dstring_get_string(&diag));
            fprintf(stderr, "\n");
         }

         /* BUGFIX: Issuezilla #1013
          * To quickly fix this issue, I'm mapping the JAPI/DRMAA error code
          * back into a GDI error code.  This is the easy solution.  The
          * correct solution would be to address issue #859, presumably by
          * having JAPI reuse the GDI error codes instead of the JAPI error
          * codes. */
         if (error == DRMAA_ERRNO_TRY_LATER) {
            exit_status = STATUS_NOTOK_DOAGAIN;
         }
         else {
            exit_status = 1;
         }

         goto Error;
      }

      DPRINTF("job id is: %ld\n", jobids->it.ji.jobid);

      jobid_string = get_bulk_jobid_string((long)jobids->it.ji.jobid, start, end, step);
   }
   else if (num_tasks == 1) {
      int error = japi_run_job(&jobid, &job, &diag);

      if (error != DRMAA_ERRNO_SUCCESS) {
         if (error != DRMAA_ERRNO_NO_ACTIVE_SESSION) {
            fprintf(stderr, MSG_QSUB_COULDNOTRUNJOB_S,
                    sge_dstring_get_string(&diag));
            fprintf(stderr, "\n");
         }

         /* BUGFIX: Issuezilla #1013
          * To quickly fix this issue, I'm mapping the JAPI/DRMAA error code
          * back into a GDI error code.  This is the easy solution.  The
          * correct solution would be to address issue #859, presumably by
          * having JAPI reuse the GDI error codes instead of the DRMAA error
          * codes. */
         if (error == DRMAA_ERRNO_TRY_LATER) {
            exit_status = STATUS_NOTOK_DOAGAIN;
         }
         else {
            exit_status = 1;
         }

         goto Error;
      }

      jobid_string = strdup(sge_dstring_get_string(&jobid));
      DPRINTF("job id is: %s\n", jobid_string);

      sge_dstring_free(&jobid);
   }
   else {
      fprintf(stderr, MSG_QSUB_COULDNOTRUNJOB_S, "invalid task structure");
      fprintf(stderr, "\n");

      exit_status = 1;
      goto Error;
   }

   /* only success message is printed to stdout */

   just_verify = (lGetUlong(job, JB_verify_suitable_queues)==JUST_VERIFY ||
                  lGetUlong(job, JB_verify_suitable_queues)==POKE_VERIFY);
   DPRINTF("Just verifying job\n");

   if (!just_verify) {
      const char *output = sge_dstring_get_string(&diag);

      /* print the tersed output */
      if (has_terse) {
         printf("%s", jobid_string);
      } else if (output != nullptr) {
        printf("%s", output);
      } else {
        printf(MSG_QSUB_YOURJOBHASBEENSUBMITTED_SS, jobid_string, lGetString(job, JB_job_name));
      }
      printf("\n");
   } else {
      printf("%s\n", MSG_JOB_VERIFYFOUNDQ);
   }

   if ((sync_opt != SYNC_NO || is_immediate) && !just_verify) {
      int event;

      if (is_immediate) {
         fprintf(stderr, "%s\n", MSG_QSUB_WAITINGFORIMMEDIATEJOBTOBESCHEDULED);

         /* We only need to wait for the first task to be scheduled to be able
          * to say that the job is running. */
         tmp_ret = japi_wait(DRMAA_JOB_IDS_SESSION_ANY, &jobid, &stat,
                             DRMAA_TIMEOUT_WAIT_FOREVER, JAPI_JOB_START, &event,
                             nullptr, &diag);

         if ((tmp_ret == DRMAA_ERRNO_SUCCESS) && (event == JAPI_JOB_START)) {
            fprintf(stderr, "\n");
            fprintf(stderr, MSG_QSUB_YOURIMMEDIATEJOBXHASBEENSUCCESSFULLYSCHEDULED_S,
                  jobid_string);
            fprintf(stderr, "\n");
         }
         /* A job finish event here means that the job was rejected. */
         else if ((tmp_ret == DRMAA_ERRNO_SUCCESS) &&
                  (event == JAPI_JOB_FINISH)) {
            fprintf(stderr, "\n%s\n", MSG_QSUB_YOURQSUBREQUESTCOULDNOTBESCHEDULEDDTRYLATER);

            exit_status = 1;
            goto Error;
         }
         else {
         /* Since we told japi_wait to wait forever, we know that if it gets
          * a timeout, it's because it's been interrupted to exit, in which
          * case we don't complain.  Same for no active session. */
            if ((tmp_ret != DRMAA_ERRNO_EXIT_TIMEOUT) &&
                (tmp_ret != DRMAA_ERRNO_NO_ACTIVE_SESSION)) {
               fprintf(stderr, "\n");
               fprintf(stderr, MSG_QSUB_COULDNOTWAITFORJOB_S,
                       sge_dstring_get_string(&diag));
               fprintf(stderr, "\n");
            }

            exit_status = 1;
            goto Error;
         }
      }

      // We have to wait for certain job states
      if (sync_opt != SYNC_NO) {

         // JOB START: just wait for the first task to start
         if ((sync_opt & SYNC_JOB_START) == SYNC_JOB_START) {
            tmp_ret = japi_wait(DRMAA_JOB_IDS_SESSION_ANY, &jobid, &stat, DRMAA_TIMEOUT_WAIT_FOREVER, JAPI_JOB_START, &event, nullptr, &diag);

            if (tmp_ret != DRMAA_ERRNO_SUCCESS) {
               if (tmp_ret != DRMAA_ERRNO_EXIT_TIMEOUT && tmp_ret != DRMAA_ERRNO_NO_ACTIVE_SESSION) {
                  fprintf(stderr, "\n");
                  fprintf(stderr, MSG_QSUB_COULDNOTWAITFORJOB_S, sge_dstring_get_string(&diag));
                  fprintf(stderr, "\n");
               }

               exit_status = 1;
               goto Error;
            } else {
               printf(MSG_QSUB_JOBHASSTARTED_S, sge_dstring_get_string(&jobid));
               printf("\n");
            }
         }

         // JOB END: Now wait for the end of *all* tasks
         if ((sync_opt & SYNC_JOB_END) == SYNC_JOB_END) {
            for (count = 0; count < num_tasks; count++) {
               // Rather than using japi_synchronize on ALL for bulk jobs, we use japi_wait on ANY num_tasks times because with synchronize, we would
               // have to wait for all the tasks to finish before we know if any finished.
               // Since there's only one running job in the session, we can just  wait for ANY.
               tmp_ret = japi_wait(DRMAA_JOB_IDS_SESSION_ANY, &jobid, &stat, DRMAA_TIMEOUT_WAIT_FOREVER, JAPI_JOB_FINISH, &event, nullptr, &diag);
               if (tmp_ret != DRMAA_ERRNO_SUCCESS) {
                  if (tmp_ret != DRMAA_ERRNO_EXIT_TIMEOUT && tmp_ret != DRMAA_ERRNO_NO_ACTIVE_SESSION) {
                     fprintf(stderr, "\n");
                     fprintf(stderr, MSG_QSUB_COULDNOTWAITFORJOB_S, sge_dstring_get_string(&diag));
                     fprintf(stderr, "\n");
                  }

                  exit_status = 1;
                  goto Error;
               }

               // report how job finished
               if (exit_status == 0) {
                  // If the job is an array job, use the first non-zero exit code as the exit code for qsub.
                  exit_status = report_exit_status(stat, sge_dstring_get_string(&jobid));
               } else {
                  // If we've already found a non-zero exit code, just print the exit info for the task.
                  report_exit_status(stat, sge_dstring_get_string(&jobid));
               }
            }
         }
      }
   }

Error:
   sge_free(&jobid_string);
   lFreeList(&alp);
   lFreeList(&opts_all);

   if ((tmp_ret = japi_exit(JAPI_EXIT_NO_FLAG, &diag)) != DRMAA_ERRNO_SUCCESS) {
      if (tmp_ret != DRMAA_ERRNO_NO_ACTIVE_SESSION) {
         fprintf(stderr, "\n");
         fprintf(stderr, MSG_QSUB_COULDNOTFINALIZEENV_S, sge_dstring_get_string(&diag));
         fprintf(stderr, "\n");
      } else {
         /* We know that if we get a DRMAA_ERRNO_NO_ACTIVE_SESSION here, it's
          * because the signal handler thread called japi_exit().  We know this
          * because if the call to japi_init() fails, we just exit directly.
          * If the call to japi_init() succeeds, then we have an active session,
          * so coming here because of an error would not result in the
          * DRMAA_ERRNO_NO_ACTIVE_SESSION error. */
         DPRINTF("Sleeping for 15 seconds to wait for the exit to finish.\n");

         sge_mutex_lock("qsub_exit_mutex", __func__, __LINE__, &exit_mutex);

         while (!exited) {
            if (ocs::uti::condition_timedwait(&exit_cv, &exit_mutex, 15) == ETIMEDOUT) {
               DPRINTF("Exit has not finished after 15 seconds.  Exiting.\n");
               break;
            }
         }

         sge_mutex_unlock("qsub_exit_mutex", __func__, __LINE__, &exit_mutex);
      }
   }

   sge_prof_cleanup();

   /* This is an exit() instead of an sge_exit() because when the qmaster is
    * supended, sge_exit() hangs. */
   exit(exit_status);
   DRETURN(exit_status);
}

/****** get_bulk_jobid_string() ************************************************
*  NAME
*     get_bulk_jobid_string() -- Turn the job id and parameters into a string
*
*  SYNOPSIS
*     char *get_bulk_jobid_string(long job_id, int start, int end, int step)
*
*  FUNCTION
*     Creates a string from the job id, start task, end task, and task step.
*     The return job id string must be freed by the caller.
*
*  INPUT
*     long job_id   - The job's id number
*     int start     - The number of the first task in the job
*     int end       - The number of the last task in the job
*     int step      - The increment between job task numbers
*
*  RESULT
*     static char * - The job id string
*
*  NOTES
*     MT-NOTES: get_bulk_jobid_string() is MT safe
*******************************************************************************/
static char *get_bulk_jobid_string(long job_id, int start, int end, int step)
{
   size_t jobid_str_size = sizeof(char) * 1024;
   char *jobid_str = sge_malloc(jobid_str_size);
   char *ret_str = nullptr;

   snprintf(jobid_str, jobid_str_size, "%ld.%d-%d:%d", job_id, start, end, step);
   ret_str = strdup(jobid_str);
   sge_free(&jobid_str);

   return ret_str;
}

/****** qsub_setup_sig_handlers() **********************************************
*  NAME
*     qsub_setup_sig_handlers() -- Set up the signal handlers
*
*  SYNOPSIS
*     void qsub_setup_sig_handlers()
*
*  FUNCTION
*     Blocks all signals so that the signal handler thread receives them.
*
*  NOTES
*     MT-NOTES: get_bulk_jobid_string() is MT safe
*******************************************************************************/
static void qsub_setup_sig_handlers()
{
   sigset_t sig_set;

   sigfillset(&sig_set);
   pthread_sigmask(SIG_BLOCK, &sig_set, nullptr);
}

/****** qsub_terminate() *******************************************************
*  NAME
*     qsub_terminate() -- Terminates qsub
*
*  SYNOPSIS
*     void qsub_terminate()
*
*  FUNCTION
*     Prints out messages that qsub is ending and exits JAPI.
*
*  NOTES
*     MT-NOTES: qsub_terminate() is MT safe
*******************************************************************************/
static void qsub_terminate()
{
   dstring diag = DSTRING_INIT;
   int tmp_ret;

   fprintf(stderr, "\n%s\n", MSG_QSUB_INTERRUPTED);
   fprintf(stderr, "%s\n", MSG_QSUB_TERMINATING);

   tmp_ret = japi_exit(JAPI_EXIT_KILL_PENDING, &diag);

   /* No active session here means that the main thread beat us to exiting,
      in which case, we just quietly give up and go away. */
   if ((tmp_ret != DRMAA_ERRNO_SUCCESS) &&
       (tmp_ret != DRMAA_ERRNO_NO_ACTIVE_SESSION)) {
      fprintf(stderr, "\n");
      fprintf(stderr, MSG_QSUB_COULDNOTFINALIZEENV_S,
              sge_dstring_get_string(&diag));
      fprintf(stderr, "\n");
   }

   sge_dstring_free(&diag);

   sge_mutex_lock("qsub_exit_mutex", "qsub_terminate", __LINE__, &exit_mutex);
   exited = true;
   pthread_cond_signal(&exit_cv);
   sge_mutex_unlock("qsub_exit_mutex", "qsub_terminate", __LINE__, &exit_mutex);
}

/****** sig_thread() ***********************************************************
*  NAME
*     sig_thread() -- Signal handler thread
*
*  SYNOPSIS
*     void *sig_thread(void *dummy)
*
*  FUNCTION
*     Waits for a SIGINT or SIGTERM and then calls qsub_terminate().
*
*  INPUT
*     void *dummy - Unused
*
*  RESULT
*     static void * - Always nullptr
*
*  NOTES
*     MT-NOTES: sig_thread() is MT safe
*******************************************************************************/
static void *sig_thread(void *dummy)
{
   int sig;
   sigset_t signal_set;
   dstring diag = DSTRING_INIT;

   sigemptyset(&signal_set);
   sigaddset(&signal_set, SIGINT);
   sigaddset(&signal_set, SIGTERM);

   /* Set up this thread so that when japi_exit() gets called, the GDI is
    * ready for use. */
   japi_init_mt(&diag);

   /* We don't care about sigwait's return(error) code because our response
    * to an error would be the same thing we're doing anyway: shutting down. */
   sigwait(&signal_set, &sig);

   qsub_terminate();

   return (void *)nullptr;
}

/****** report_exit_status() ***************************************************
*  NAME
*     report_exit_status() -- Prints a job's exit status
*
*  SYNOPSIS
*     static int report_exit_status(int stat, const char *jobid)
*
*  FUNCTION
*     Prints a job's exit status to stdout.
*
*  INPUT
*     int stat          - The job's exit status
*     const char *jobid - The job id string
*
*  RESULT
*     static int        - The exit code of the job
*
*  NOTES
*     MT-NOTES: report_exit_status() is MT safe
*******************************************************************************/
static int report_exit_status(int stat, const char *jobid)
{
   int aborted, exited, signaled;
   int exit_status = 0;

   japi_wifaborted(&aborted, stat, nullptr);

   if (aborted) {
      printf(MSG_QSUB_JOBNEVERRAN_S, jobid);
   } else {
      japi_wifexited(&exited, stat, nullptr);
      if (exited) {
         japi_wexitstatus(&exit_status, stat, nullptr);
         printf(MSG_QSUB_JOBEXITED_SI, jobid, exit_status);
      } else {
         japi_wifsignaled(&signaled, stat, nullptr);

         if (signaled) {
            dstring termsig = DSTRING_INIT;
            japi_wtermsig(&termsig, stat, nullptr);
            printf(MSG_QSUB_JOBRECEIVEDSIGNAL_SS, jobid,
                    sge_dstring_get_string(&termsig));
            sge_dstring_free(&termsig);
         } else {
            printf(MSG_QSUB_JOBFINISHUNCLEAR_S, jobid);
         }

         exit_status = 1;
      }
   }
   printf("\n");

   return exit_status;
}

/****** error_handler() ********************************************************
*  NAME
*     error_handler() -- Prints JAPI error messages
*
*  SYNOPSIS
*     static void error_handler(const char *message)
*
*  FUNCTION
*     Prints error messages from JAPI event client thread to stderr
*
*  INPUT
*     const char *message - The message to print
*
*  NOTES
*     MT-NOTES: error_handler() is MT safe
*******************************************************************************/
static void error_handler(const char *message)
{
   fprintf(stderr, "%s", message != nullptr ? message : "nullptr japi message");
}
