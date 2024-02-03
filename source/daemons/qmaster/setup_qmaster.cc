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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <sys/resource.h>

#include "uti/sge_rmon.h"
#include "uti/sge_log.h"
#include "uti/sge_prog.h"
#include "uti/sge_unistd.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_os.h"
#include "uti/sge_hostname.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_spool.h"
#include "uti/config_file.h"
#include "uti/sge_time.h"

#include "evm/sge_event_master.h"

#include "spool/sge_spooling.h"

#include "sgeobj/parse.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_utility.h"

#include "gdi/qm_name.h"
#include "gdi/sge_gdi2.h"

#include "sched/debit.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_resource_quota_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "sge_host_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_task_depend.h"
#include "sched_conf_qmaster.h"
#include "configuration_qmaster.h"
#include "lock.h"
#include "usage.h"
#include "shutdown.h"
#include "sge_give_jobs.h"
#include "sge_resource_quota_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_qinstance_qmaster.h"

#include "msg_daemons_common.h"
#include "msg_qmaster.h"
#include "msg_common.h"

struct cmplx_tmp {
   char *name;
   char *shortcut;
   u_long32 valtype;
   u_long32 relop;
   u_long32 consumable;
   char *valdefault;
   u_long32 requestable;
   char *urgency_weight;
};

static void
process_cmdline(char **);

static lList *
parse_cmdline_qmaster(char **, lList **);

static lList *
parse_qmaster(lList **, u_long32 *);

static void
qmaster_init(sge_gdi_ctx_class_t *ctx, char **);

static void
communication_setup(sge_gdi_ctx_class_t *ctx);

static bool
is_qmaster_already_running(const char *qmaster_spool_dir);

static void
qmaster_lock_and_shutdown(void **ctx_ref, int);

static int
setup_qmaster(sge_gdi_ctx_class_t *ctx);

static int
remove_invalid_job_references(bool job_spooling, int user);

static int
debit_all_jobs_from_qs(void);

static void
init_categories(void);


/****** qmaster/setup_qmaster/sge_setup_qmaster() ******************************
*  NAME
*     sge_setup_qmaster() -- setup qmaster 
*
*  SYNOPSIS
*     int sge_setup_qmaster(char* anArgv[]) 
*
*  FUNCTION
*     Process commandline arguments. Remove qmaster lock file. Write qmaster
*     host to the 'act_qmaster' file. Initialize qmaster and reporting. Write
*     qmaster PID file.  
*
*     NOTE: Before this function is invoked, qmaster must become admin user.
*
*  INPUTS
*     char* anArgv[] - commandline argument vector 
*
*  RESULT
*     0 - success 
*
*  NOTES
*     MT-NOTE: sge_setup_qmaster() is NOT MT safe! 
*     MT-NOTE:
*     MT-NOTE: This function must be called exclusively, with the qmaster main
*     MT-NOTE: thread being the *only* active thread. In other words, do not
*     MT-NOTE: invoke this function after any additional thread (directly or
*     MT-NOTE: indirectly) has been created.
*
*     Do *not* write the qmaster pid file, before 'qmaster_init()' did return
*     successfully. Otherwise, if we do have a running qmaster and a second
*     qmaster is started (illegally) on the same host, the second qmaster will
*     overwrite the pid of the qmaster started first. The second qmaster will
*     detect it's insubordinate doing and terminate itself, thus leaving behind
*     a useless pid.
*
*******************************************************************************/
int
sge_setup_qmaster(sge_gdi_ctx_class_t *ctx, char *anArgv[]) {
   char err_str[1024];
   const char *qualified_hostname = uti_state_get_qualified_hostname();
   const char *act_qmaster_file = bootstrap_get_act_qmaster_file();

   DENTER(TOP_LAYER);

   umask(022); /* this needs a better solution */

   process_cmdline(anArgv);

   INFO((SGE_EVENT, SFNMAX, MSG_STARTUP_BEGINWITHSTARTUP));

   qmaster_unlock(QMASTER_LOCK_FILE);

   if (write_qm_name(qualified_hostname, act_qmaster_file, err_str)) {
      ERROR((SGE_EVENT, "%s\n", err_str));
      SGE_EXIT(nullptr, 1);
   }

   qmaster_init(ctx, anArgv);

   sge_write_pid(QMASTER_PID_FILE);

   DRETURN(0);
} /* sge_setup_qmaster() */

/****** qmaster/setup_qmaster/sge_qmaster_thread_init() ************************
*  NAME
*     sge_qmaster_thread_init() -- Initialize a qmaster thread.
*
*  SYNOPSIS
*     int sge_qmaster_thread_init(bool switch_to_admin_user) 
*
*  FUNCTION
*     Subsume functions which need to be called immediately after thread
*     startup. This function does make sure that the thread local data
*     structures do contain reasonable values.
*
*  INPUTS
*     bool switch_to_admin_user - become admin user if set to true
*
*  RESULT
*     0 - success 
*
*  NOTES
*     MT-NOTE: sge_qmaster_thread_init() is MT safe 
*     MT-NOTE:
*     MT-NOTE: sge_qmaster_thread_init() should be invoked at the beginning
*     MT-NOTE: of a thread function.
*
*******************************************************************************/
int
sge_qmaster_thread_init(sge_gdi_ctx_class_t **ctx_ref, u_long32 prog_id, u_long32 thread_id,
                        bool switch_to_admin_user) {
   const char *admin_user = nullptr;
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   lInit(nmv);

   if (sge_setup2(ctx_ref, prog_id, thread_id, &alp, true) != AE_OK) {
      answer_list_output(&alp);
      SGE_EXIT((void **) ctx_ref, 1);
   }
   reresolve_qualified_hostname();
   DEBUG((SGE_EVENT, "%s: qualified hostname \"%s\"\n", __func__, uti_state_get_qualified_hostname()));
   admin_user = bootstrap_get_admin_user();

   if (switch_to_admin_user == true) {
      char str[MAX_STRING_SIZE];
      if (sge_set_admin_username(admin_user, str) == -1) {
         CRITICAL((SGE_EVENT, SFNMAX, str));
         SGE_EXIT((void **) ctx_ref, 1);
      }

      if (sge_switch2admin_user()) {
         CRITICAL((SGE_EVENT, SFNMAX, MSG_ERROR_CANTSWITCHTOADMINUSER));
         SGE_EXIT((void **) ctx_ref, 1);
      }
   }

   DRETURN(0);
} /* sge_qmaster_thread_init() */

/****** qmaster/setup_qmaster/sge_setup_job_resend() ***************************
*  NAME
*     sge_setup_job_resend() -- Setup job resend events.
*
*  SYNOPSIS
*     void sge_setup_job_resend(void) 
*
*  FUNCTION
*     Register a job resend event for each job or array task which does have a
*     'JTRANSFERING' status.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_setup_job_resend() is not MT safe 
*
*******************************************************************************/
void
sge_setup_job_resend(void) {
   const lListElem *job = nullptr;

   DENTER(TOP_LAYER);

   job = lFirst(*object_type_get_master_list(SGE_TYPE_JOB));

   while (nullptr != job) {
      const lListElem *task;
      u_long32 job_num;

      job_num = lGetUlong(job, JB_job_number);
      task = lFirst(lGetList(job, JB_ja_tasks));
      while (nullptr != task) {
         if (lGetUlong(task, JAT_status) == JTRANSFERING) {
            const lListElem *granted_queue, *qinstance;
            lListElem *host;
            const char *qname;
            u_long32 task_num, when;
            te_event_t ev;

            task_num = lGetUlong(task, JAT_task_number);
            granted_queue = lFirst(lGetList(task, JAT_granted_destin_identifier_list));
            qname = lGetString(granted_queue, JG_qname);
            qinstance = cqueue_list_locate_qinstance(*object_type_get_master_list(SGE_TYPE_CQUEUE), qname);
            host = host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), lGetHost(qinstance, QU_qhostname));
            when = lGetUlong(task, JAT_start_time);
            when += MAX(load_report_interval(host), MAX_JOB_DELIVER_TIME);
            ev = te_new_event((time_t) when, TYPE_JOB_RESEND_EVENT, ONE_TIME_EVENT, job_num, task_num,
                              "job-resend_event");
            te_add_event(ev);
            te_free_event(&ev);

            DPRINTF(("Did add job resend for "sge_u32"/"sge_u32" at %d\n", job_num, task_num, when));
         }

         task = lNext(task);
      }

      job = lNext(job);
   }

   DRETURN_VOID;
} /* sge_setup_job_resend() */

/****** setup_qmaster/sge_process_qmaster_cmdline() ****************************
*  NAME
*     sge_process_qmaster_cmdline() -- global available function for qmaster
*
*  SYNOPSIS
*     void sge_process_qmaster_cmdline(char**anArgv) 
*
*  FUNCTION
*     This function simply calls the static function process_cmdline()
*
*  INPUTS
*     char**anArgv - command line arguments from main()
*
*  NOTES
*     MT-NOTE: sge_process_qmaster_cmdline() is NOT MT safe 
*
*  SEE ALSO
*     qmaster/setup_qmaster/process_cmdline()
*******************************************************************************/
void
sge_process_qmaster_cmdline(char **anArgv) {
   process_cmdline(anArgv);
}

/****** qmaster/setup_qmaster/process_cmdline() ********************************
*  NAME
*     process_cmdline() -- Handle command line arguments 
*
*  SYNOPSIS
*     static void process_cmdline(char **anArgv) 
*
*  FUNCTION
*     Handle command line arguments. Parse argument vector and handle options.
*
*  INPUTS
*     char **anArgv - pointer to agrument vector 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: process_cmdline() is NOT MT safe. 
*
*******************************************************************************/
static void
process_cmdline(char **anArgv) {
   lList *alp, *pcmdline;
   const lListElem *aep;
   u_long32 help = 0;

   DENTER(TOP_LAYER);

   alp = pcmdline = nullptr;

   alp = parse_cmdline_qmaster(&anArgv[1], &pcmdline);
   if (alp) {
      /*
      ** high level parsing error! show answer list
      */
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      SGE_EXIT(nullptr, 1);
   }

   alp = parse_qmaster(&pcmdline, &help);
   if (alp) {
      /*
      ** low level parsing error! show answer list
      */
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      SGE_EXIT(nullptr, 1);
   }

   if (help) {
      /* user wanted to see help. we can exit */
      lFreeList(&pcmdline);
      SGE_EXIT(nullptr, 0);
   }

   DRETURN_VOID;
} /* process_cmdline */

/****** qmaster/setup_qmaster/parse_cmdline_qmaster() **************************
*  NAME
*     parse_cmdline_qmaster() -- Parse command line arguments
*
*  SYNOPSIS
*     static lList* parse_cmdline_qmaster(char **argv, lList **ppcmdline) 
*
*  FUNCTION
*     Decompose argument vector. Handle options and option arguments. 
*
*  INPUTS
*     char **argv       - pointer to argument vector 
*     lList **ppcmdline - pointer to lList pointer which does contain the 
*                         command line arguments upon return. 
*
*  RESULT
*     lList* - pointer to answer list 
*
*  NOTES
*     MT-NOTE: parse_cmdline_qmaster() is MT safe. 
*
*******************************************************************************/
static lList *
parse_cmdline_qmaster(char **argv, lList **ppcmdline) {
   char **sp;
   char **rp;
   stringT str;
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   rp = argv;
   while (*(sp = rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, &alp)) != sp)
         continue;

      /* oops */
      sprintf(str, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      printf("%s\n", *sp);
      sge_usage(QMASTER, stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(alp);
   }

   DRETURN(alp);
} /* parse_cmdline_qmaster() */

/****** qmaster/setup_qmaster/parse_qmaster() **********************************
*  NAME
*     parse_qmaster() -- Process options. 
*
*  SYNOPSIS
*     static lList* parse_qmaster(lList **ppcmdline, u_long32 *help) 
*
*  FUNCTION
*     Process options 
*
*  INPUTS
*     lList **ppcmdline - list of options
*     u_long32 *help    - flag is set upon return if help has been requested
*
*  RESULT
*     lList* - answer list 
*
*  NOTES
*     MT-NOTE: parse_qmaster() is not MT safe. 
*
*******************************************************************************/
static lList *
parse_qmaster(lList **ppcmdline, u_long32 *help) {
   lList *alp = nullptr;
   int usageshowed = 0;

   DENTER(TOP_LAYER);

   /* Loop over all options. Only valid options can be in the 
      ppcmdline list.
   */
   while (lGetNumberOfElem(*ppcmdline)) {
      /* -help */
      if (parse_flag(ppcmdline, "-help", &alp, help)) {
         usageshowed = 1;
         sge_usage(QMASTER, stdout);
         break;
      }
   }

   if (lGetNumberOfElem(*ppcmdline)) {
      if (!usageshowed) {
         sge_usage(QMASTER, stderr);
      }
      answer_list_add(&alp, MSG_PARSE_TOOMANYOPTIONS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(alp);
   }
   DRETURN(alp);
} /* parse_qmaster() */

/****** qmaster/setup_qmaster/qmaster_init() ***********************************
*  NAME
*     qmaster_init() -- Initialize qmaster 
*
*  SYNOPSIS
*     static void qmaster_init(char **anArgv) 
*
*  FUNCTION
*     Initialize qmaster. Do general setup and communication setup. 
*
*  INPUTS
*     char **anArgv - process argument vector 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: qmaster_init() is NOT MT safe. 
*
*******************************************************************************/
static void
qmaster_init(sge_gdi_ctx_class_t *ctx, char **anArgv) {

   DENTER(TOP_LAYER);

   if (setup_qmaster(ctx)) {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_STARTUP_SETUPFAILED));
      SGE_EXIT(nullptr, 1);
   }

   uti_state_set_exit_func(qmaster_lock_and_shutdown);

   communication_setup(ctx);

   starting_up(); /* write startup info message to message file */

   DRETURN_VOID;
} /* qmaster_init() */

/****** qmaster/setup_qmaster/communication_setup() ****************************
*  NAME
*     communication_setup() -- set up communication
*
*  SYNOPSIS
*     static void communication_setup(void) 
*
*  FUNCTION
*     Initialize qmaster communication. 
*
*     This function will fail, if the configured qmaster port is already in
*     use.
*
*     This could happen if either qmaster has been terminated shortly before
*     and the operating system did not get around to free the port or there
*     is a qmaster already running.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: communication_setup() is NOT MT safe 
*
*******************************************************************************/
static void
communication_setup(sge_gdi_ctx_class_t *ctx) {
   cl_com_handle_t *com_handle = nullptr;
   char *qmaster_params = nullptr;
   struct rlimit qmaster_rlimits;

   const char *qualified_hostname = uti_state_get_qualified_hostname();
   u_long32 qmaster_port = bootstrap_get_sge_qmaster_port();
   const char *qmaster_spool_dir = bootstrap_get_qmaster_spool_dir();

   DENTER(TOP_LAYER);

   DEBUG((SGE_EVENT, "my resolved hostname name is: \"%s\"\n", qualified_hostname));

   com_handle = cl_com_get_handle(prognames[QMASTER], 1);

   if (com_handle == nullptr) {
      ERROR((SGE_EVENT, "port "sge_u32" already bound\n", qmaster_port));

      if (is_qmaster_already_running(qmaster_spool_dir) == true) {
         char *host = nullptr;
         int res = -1;

         res = cl_com_gethostname(&host, nullptr, nullptr, nullptr);

         CRITICAL((SGE_EVENT, MSG_QMASTER_FOUNDRUNNINGQMASTERONHOSTXNOTSTARTING_S, ((CL_RETVAL_OK == res) ? host
                                                                                                          : "unknown")));

         if (CL_RETVAL_OK == res) {
            sge_free(&host);
         }
      }

      SGE_EXIT(nullptr, 1);
   }

   if (com_handle) {
      unsigned long max_connections = 0;
      u_long32 old_ll = 0;

      /* 
       * re-check file descriptor limits for qmaster 
       */
      getrlimit(RLIMIT_NOFILE, &qmaster_rlimits);

      /* save old debug log level and set log level to INFO */
      old_ll = log_state_get_log_level();

      /* enable max connection close mode */
      cl_com_set_max_connection_close_mode(com_handle, CL_ON_MAX_COUNT_CLOSE_AUTOCLOSE_CLIENTS);

      cl_com_get_max_connections(com_handle, &max_connections);

      /* add local host to allowed host list */
      cl_com_add_allowed_host(com_handle, com_handle->local->comp_host);

      /* check dynamic event client count */
      mconf_set_max_dynamic_event_clients(sge_set_max_dynamic_event_clients(mconf_get_max_dynamic_event_clients()));

      /* log startup info into qmaster messages file */
      log_state_set_log_level(LOG_INFO);
      INFO((SGE_EVENT, MSG_QMASTER_FD_HARD_LIMIT_SETTINGS_U, sge_u32c(qmaster_rlimits.rlim_max)));
      INFO((SGE_EVENT, MSG_QMASTER_FD_SOFT_LIMIT_SETTINGS_U, sge_u32c(qmaster_rlimits.rlim_cur)));
      INFO((SGE_EVENT, MSG_QMASTER_MAX_FILE_DESCRIPTORS_LIMIT_U, sge_u32c(max_connections)));
      INFO((SGE_EVENT, MSG_QMASTER_MAX_EVC_LIMIT_U, sge_u32c(mconf_get_max_dynamic_event_clients())));
      log_state_set_log_level(old_ll);
   }

   cl_commlib_set_connection_param(cl_com_get_handle(prognames[QMASTER], 1), HEARD_FROM_TIMEOUT,
                                   mconf_get_max_unheard());

   /* fetching qmaster_params and begin to parse */
   qmaster_params = mconf_get_qmaster_params();

   /* updating the commlib paramterlist with new or changed parameters */
   cl_com_update_parameter_list(qmaster_params);
   DPRINTF(("received qmaster_params are: %s\n", qmaster_params));
   sge_free(&qmaster_params);

   /* now enable qmaster communication */
   cl_commlib_set_global_param(CL_COMMLIB_DELAYED_LISTEN, false);

   DRETURN_VOID;
} /* communication_setup() */

/****** qmaster/setup_qmaster/is_qmaster_already_running() *********************
*  NAME
*     is_qmaster_already_running() -- is qmaster already running 
*
*  SYNOPSIS
*     static bool is_qmaster_already_running(void) 
*
*  FUNCTION
*     Check, whether there is running qmaster already.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     true  - running qmaster detected. 
*     false - otherwise
*
*  NOTES
*     MT-NOTE: is_qmaster_already_running() is not MT safe 
*
*  BUGS
*     This function will only work, if the PID found in the qmaster PID file
*     either does belong to a running qmaster or no process at all.
*
*     Of course PID's will be reused. This is, however, not a problem because
*     of the very specifc situation in which this function is called.
*
*******************************************************************************/
static bool
is_qmaster_already_running(const char *qmaster_spool_dir) {
   enum {
      NULL_SIGNAL = 0
   };

   bool res = true;
   char pidfile[SGE_PATH_MAX] = {'\0'};
   pid_t pid = 0;

   DENTER(TOP_LAYER);

   sprintf(pidfile, "%s/%s", qmaster_spool_dir, QMASTER_PID_FILE);

   if ((pid = sge_readpid(pidfile)) == 0) {
      DRETURN(false);
   }

   res = (kill(pid, NULL_SIGNAL) == 0) ? true : false;

   DRETURN(res);
} /* is_qmaster_already_running() */


static void
sge_propagate_queue_suspension(lListElem *jep, dstring *cqueue_name, dstring *host_domain) {
   const lListElem *gdil_ep, *cq, *qi;
   lListElem *jatep;

   for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
      u_long32 jstate = lGetUlong(jatep, JAT_state);
      bool is_suspended = false;

      for_each_ep(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {

         if (!cqueue_name_split(lGetString(gdil_ep, JG_qname), cqueue_name, host_domain, nullptr, nullptr)) {
            continue;
         }

         if (!(cq = lGetElemStr(*object_type_get_master_list(SGE_TYPE_CQUEUE), CQ_name,
                                sge_dstring_get_string(cqueue_name))) ||
             !(qi = lGetElemHost(lGetList(cq, CQ_qinstances), QU_qhostname, sge_dstring_get_string(host_domain))))
            continue;

         if (qinstance_state_is_manual_suspended(qi) ||
             qinstance_state_is_susp_on_sub(qi) ||
             qinstance_state_is_cal_suspended(qi)) {
            is_suspended = true;
            break;
         }
      }

      if (is_suspended)
         jstate |= JSUSPENDED_ON_SUBORDINATE;
      else
         jstate &= ~JSUSPENDED_ON_SUBORDINATE;
      lSetUlong(jatep, JAT_state, jstate);
   }
}

/****** qmaster/setup_qmaster/qmaster_lock_and_shutdown() ***************************
*  NAME
*     qmaster_lock_and_shutdown() -- Acquire qmaster lock file and shutdown 
*
*  SYNOPSIS
*     static void qmaster_lock_and_shutdown(int anExitValue) 
*
*  FUNCTION
*     qmaster exit function. This version MUST NOT be used, if the current
*     working   directory is NOT the spool directory. Other components do rely
*     on finding the lock file in the spool directory.
*
*  INPUTS
*     int anExitValue - exit value 
*
*  RESULT
*     void - none
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: qmaster_lock_and_shutdown() is MT safe 
*
*******************************************************************************/
static void
qmaster_lock_and_shutdown(void **ctx_ref, int anExitValue) {
   DENTER(TOP_LAYER);

   if (anExitValue == 0) {
      if (qmaster_lock(QMASTER_LOCK_FILE) == -1) {
         CRITICAL((SGE_EVENT, SFNMAX, MSG_QMASTER_LOCKFILE_ALREADY_EXISTS));
      }
   }
   sge_gdi2_shutdown(ctx_ref);

   DRETURN_VOID;
} /* qmaster_lock_and_shutdown() */


static int
setup_qmaster(sge_gdi_ctx_class_t *ctx) {
   lListElem *jep, *ep, *tmpqep;
   static bool first = true;
   const lListElem *spooling_context = nullptr;
   lList *answer_list = nullptr;
   time_t time_start, time_end;
   monitoring_t monitor;
   const char *qualified_hostname = nullptr;

   bool job_spooling = bootstrap_get_job_spooling();
   DENTER(TOP_LAYER);

   if (first) {
      first = false;
   } else {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_SETUP_SETUPMAYBECALLEDONLYATSTARTUP));
      DRETURN(-1);
   }

   /* register our error function for use in replace_params() */
   config_errfunc = set_error;

   /*
    * Initialize Master lists and hash tables, if necessary 
    */

/** This is part is making the scheduler a
  * lot slower that it was before. This was an enhancement introduced
  * in cvs revision 1.35
  * It might be added again, when hte hashing problem on large job lists
  * with only a view owners is solved.
  */
#if 0
   if (*object_type_get_master_list(SGE_TYPE_JOB) == nullptr) {
      *object_type_get_master_list(SGE_TYPE_JOB) = lCreateList("Master_Job_List", JB_Type);
   }
   cull_hash_new(*object_type_get_master_list(SGE_TYPE_JOB), JB_owner, 0);
#endif
   if (*object_type_get_master_list(SGE_TYPE_SUSER) == nullptr) {
      *object_type_get_master_list_rw(SGE_TYPE_SUSER) = lCreateList("Master_SUser_List", SU_Type);
   }

   if (!sge_initialize_persistence(ctx, &answer_list)) {
      answer_list_output(&answer_list);
      DRETURN(-1);
   } else {
      answer_list_output(&answer_list);
      spooling_context = spool_get_default_context();
   }

   if (sge_read_configuration(ctx, spooling_context, object_type_get_master_list_rw(SGE_TYPE_CONFIG), answer_list) !=
       0) {
      DRETURN(-1);
   }

   mconf_set_new_config(true);

   /* get aliased hostname from commd */
   reresolve_qualified_hostname();
   qualified_hostname = uti_state_get_qualified_hostname();
   DEBUG((SGE_EVENT, "uti_state_get_qualified_hostname() returned \"%s\"\n", qualified_hostname));

   /*
   ** read in all objects and check for correctness
   */
   DPRINTF(("Complex Attributes----------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_CENTRY), SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);

   /*
    * for release 6.2u5 the "job to core"- binding feature has been added 
    * that needs some additional complex entries. We check here if those
    * entries exist and create them silently if they are not there. Only
    * this prevents from creating a update procedure.
    *
    * TODO: As soon as there is a release where an update procedure is 
    *       available we should put that code there and remove it here.
    */
   {
      struct cmplx_tmp new_complexes[] = {
              {"m_thread",         "thread", 1, CMPLXLE_OP, CONSUMABLE_NO, "0",  REQU_YES, "0"},
              {"m_core",           "core",   1, CMPLXLE_OP, CONSUMABLE_NO, "0",  REQU_YES, "0"},
              {"m_socket",         "socket", 1, CMPLXLE_OP, CONSUMABLE_NO, "0",  REQU_YES, "0"},
              {"m_topology",       "topo",   9, CMPLXEQ_OP, CONSUMABLE_NO, nullptr, REQU_YES, "0"},
              {"m_topology_inuse", "utopo",  9, CMPLXEQ_OP, CONSUMABLE_NO, nullptr, REQU_YES, "0"},
              {nullptr, nullptr,                   0, 0,          0,             nullptr, 0,        0}
      };
      int i;

      for (i = 0; new_complexes[i].name != nullptr; i++) {
         lList *centry_list = *object_type_get_master_list_rw(SGE_TYPE_CENTRY);
         const lListElem *entry_long = lGetElemStr(centry_list, CE_name, new_complexes[i].name);
         const lListElem *entry_short = lGetElemStr(centry_list, CE_shortcut, new_complexes[i].shortcut);

         if (entry_long == nullptr && entry_short == nullptr) {
            lListElem *new_centry = lCreateElem(CE_Type);

            lSetString(new_centry, CE_name, new_complexes[i].name);
            lSetString(new_centry, CE_shortcut, new_complexes[i].shortcut);
            lSetString(new_centry, CE_defaultval, new_complexes[i].valdefault);
            lSetString(new_centry, CE_urgency_weight, new_complexes[i].urgency_weight);
            lSetUlong(new_centry, CE_valtype, new_complexes[i].valtype);
            lSetUlong(new_centry, CE_relop, new_complexes[i].relop);
            lSetUlong(new_centry, CE_consumable, new_complexes[i].consumable);
            lSetUlong(new_centry, CE_requestable, new_complexes[i].requestable);

            /* append and spool the object */
            lAppendElem(centry_list, new_centry);
            spool_write_object(nullptr, spool_get_default_context(), new_centry,
                               lGetString(new_centry, CE_name), SGE_TYPE_CENTRY, false);

         }
      }
   }

   DPRINTF(("host_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_EXECHOST),
                   SGE_TYPE_EXECHOST);
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_ADMINHOST),
                   SGE_TYPE_ADMINHOST);
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_SUBMITHOST),
                   SGE_TYPE_SUBMITHOST);
   answer_list_output(&answer_list);

   if (!host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), SGE_TEMPLATE_NAME)) {
      /* add an exec host "template" */
      if (sge_add_host_of_type(ctx, SGE_TEMPLATE_NAME, SGE_EH_LIST, &monitor))
         ERROR((SGE_EVENT, SFNMAX, MSG_CONFIG_ADDINGHOSTTEMPLATETOEXECHOSTLIST));
   }

   /* add host "global" to Master_Exechost_List as an exec host */
   if (!host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), SGE_GLOBAL_NAME)) {
      /* add an exec host "global" */
      if (sge_add_host_of_type(ctx, SGE_GLOBAL_NAME, SGE_EH_LIST, &monitor))
         ERROR((SGE_EVENT, SFNMAX, MSG_CONFIG_ADDINGHOSTGLOBALTOEXECHOSTLIST));
   }

   /* add qmaster host to Master_Adminhost_List as an administrativ host */
   if (!host_list_locate(*object_type_get_master_list(SGE_TYPE_ADMINHOST), qualified_hostname)) {
      if (sge_add_host_of_type(ctx, qualified_hostname, SGE_AH_LIST, &monitor)) {
         DRETURN(-1);
      }
   }

   DPRINTF(("manager_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_MANAGER), SGE_TYPE_MANAGER);
   answer_list_output(&answer_list);
   const lList *master_manager_list = *object_type_get_master_list(SGE_TYPE_MANAGER);
   if (!manop_is_manager("root", master_manager_list)) {
      ep = lAddElemStr(object_type_get_master_list_rw(SGE_TYPE_MANAGER), UM_name, "root", UM_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_MANAGER, job_spooling)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, SFNMAX, MSG_CONFIG_CANTWRITEMANAGERLIST));
         DRETURN(-1);
      }
   }
   for_each_rw(ep, *object_type_get_master_list(SGE_TYPE_MANAGER)) {
      DPRINTF(("%s\n", lGetString(ep, UM_name)));
   }

   DPRINTF(("host group definitions-----------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_HGROUP), SGE_TYPE_HGROUP);
   answer_list_output(&answer_list);

   DPRINTF(("operator_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_OPERATOR),
                   SGE_TYPE_OPERATOR);
   answer_list_output(&answer_list);
   const lList *master_operator_list = *object_type_get_master_list(SGE_TYPE_OPERATOR);
   if (!manop_is_operator("root", master_manager_list, master_operator_list)) {
      ep = lAddElemStr(object_type_get_master_list_rw(SGE_TYPE_OPERATOR), UO_name, "root", UO_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_OPERATOR, job_spooling)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, SFNMAX, MSG_CONFIG_CANTWRITEOPERATORLIST));
         DRETURN(-1);
      }
   }
   for_each_rw(ep, *object_type_get_master_list(SGE_TYPE_OPERATOR)) {
      DPRINTF(("%s\n", lGetString(ep, UO_name)));
   }


   DPRINTF(("userset_list------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_USERSET), SGE_TYPE_USERSET);
   answer_list_output(&answer_list);

   DPRINTF(("calendar list ------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_CALENDAR),
                   SGE_TYPE_CALENDAR);
   answer_list_output(&answer_list);

   DPRINTF(("resource quota list -----------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_RQS), SGE_TYPE_RQS);
   answer_list_output(&answer_list);

   DPRINTF(("cluster_queue_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_CQUEUE), SGE_TYPE_CQUEUE);
   answer_list_output(&answer_list);
   cqueue_list_set_unknown_state(*(object_type_get_master_list_rw(SGE_TYPE_CQUEUE)), nullptr, false, true);

   /*
    * Initialize cached values for each qinstance:
    *    - fullname
    *    - suspend_on_subordinate
    */
   for_each_rw(tmpqep, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      const lList *qinstance_list = lGetList(tmpqep, CQ_qinstances);
      lListElem *qinstance = nullptr;
      const lList *aso_list = nullptr;
      lListElem *aso = nullptr;

      /*
       * Update cluster queue configuration from pre-6.2u5 to 6.2u5, i.e. from
       * cluster queues without slotwise suspend on subordinate to such with 
       * slotwise ssos.
       */
      aso_list = lGetList(tmpqep, CQ_subordinate_list);
      if (aso_list != nullptr) {
         /* This cluster queue has a list of subordinate lists (possibly one
          * for each host).*/
         for_each_rw (aso, aso_list) {
            const lListElem *elem;
            int pos;
            const lList *so_list = lGetList(aso, ASOLIST_value);

            /* Every element of the ASOLIST should have a SOLIST, but we
             * check it to be sure.
             */
            if (so_list != nullptr) {
               /* In each subordinate list, all elements must have the same
                * SO_slots_sum value, so it's enough to look in the first
                * element.
                */
               elem = lFirst(so_list);
               pos = lGetPosViaElem(elem, SO_slots_sum, SGE_NO_ABORT);
               if (pos == -1) {
                  /* In the subordinate list of the cluster queue there is no
                   * SO_slots_sum field yet, so we have to create a new
                   * subordinate list, copy the values from the old one and
                   * initialize the new fields, remove the old subordinate list
                   * from the cluster queue and add the new one instead.
                   */
                  lList *new_so_list = nullptr;
                  lListElem *so = nullptr;
                  lListElem *new_so = nullptr;
                  const char *so_list_name = nullptr;

                  so_list_name = lGetListName(so_list);
                  new_so_list = lCreateList(so_list_name, SO_Type);

                  for_each_rw (so, so_list) {
                     new_so = lCreateElem(SO_Type);
                     lSetString(new_so, SO_name, lGetString(so, SO_name));
                     lSetUlong(new_so, SO_threshold, lGetUlong(so, SO_threshold));
                     lSetUlong(new_so, SO_slots_sum, 0);
                     lSetUlong(new_so, SO_seq_no, 0);
                     lSetUlong(new_so, SO_action, 0);
                     lAppendElem(new_so_list, new_so);
                  }
                  lSetList(aso, ASOLIST_value, new_so_list);
               }
            }
         }
      }
      for_each_rw(qinstance, qinstance_list) {
         qinstance_set_full_name(qinstance);
         sge_qmaster_qinstance_state_set_susp_on_sub(qinstance, false);
      }
   }

   DPRINTF(("pe_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_PE), SGE_TYPE_PE);
   answer_list_output(&answer_list);

   DPRINTF(("ckpt_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_CKPT), SGE_TYPE_CKPT);
   answer_list_output(&answer_list);

   DPRINTF(("advance reservation list -----------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_AR), SGE_TYPE_AR);
   answer_list_output(&answer_list);

   /* initialize cached advance reservations structures */
   {
      lListElem *ar;
      for_each_rw(ar, *object_type_get_master_list(SGE_TYPE_AR)) {
         ar_initialize_reserved_queue_list(ar);
      }
   }

   DPRINTF(("job_list-----------------------------------\n"));
   /* measure time needed to read job database */
   time_start = time(0);
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_JOB), SGE_TYPE_JOB);
   time_end = time(0);
   answer_list_output(&answer_list);

   {
      u_long32 saved_logginglevel = log_state_get_log_level();
      log_state_set_log_level(LOG_INFO);
      INFO((SGE_EVENT, MSG_QMASTER_READ_JDB_WITH_X_ENTR_IN_Y_SECS_UU,
              sge_u32c(lGetNumberOfElem(*object_type_get_master_list(SGE_TYPE_JOB))),
              sge_u32c(time_end - time_start)));
      log_state_set_log_level(saved_logginglevel);
   }

   {
      dstring cqueue_name = DSTRING_INIT;
      dstring host_domain = DSTRING_INIT;

      for_each_rw(jep, *object_type_get_master_list(SGE_TYPE_JOB)) {

         DPRINTF(("JOB "sge_u32" PRIORITY %d\n", lGetUlong(jep, JB_job_number),
                 (int) lGetUlong(jep, JB_priority) - BASE_PRIORITY));

         /* doing this operation we need the complete job list read in */
         job_suc_pre(jep);

         /* also do this for array dependency predecessors */
         job_suc_pre_ad(jep);

         /* array successor jobs need to have their cache rebuilt. this will
            do nothing spectacular if the AD reqest list for this job is empty. */
         sge_task_depend_init(jep, &answer_list);

         centry_list_fill_request(lGetListRW(jep, JB_hard_resource_list),
                                  nullptr, *object_type_get_master_list(SGE_TYPE_CENTRY), false, true, false);

         /* need to update JSUSPENDED_ON_SUBORDINATE since task spooling is not 
            triggered upon queue un/-suspension */
         sge_propagate_queue_suspension(jep, &cqueue_name, &host_domain);
      }
      sge_dstring_free(&cqueue_name);
      sge_dstring_free(&host_domain);
   }

   if (!bootstrap_get_job_spooling()) {
      lList *answer_list = nullptr;
      dstring buffer = DSTRING_INIT;

      INFO((SGE_EVENT, "job spooling is disabled - removing spooled jobs"));

      bootstrap_set_job_spooling(true);

      for_each_rw(jep, *object_type_get_master_list(SGE_TYPE_JOB)) {
         u_long32 job_id = lGetUlong(jep, JB_job_number);
         sge_dstring_clear(&buffer);

         if (lGetString(jep, JB_exec_file) != nullptr) {
            if (spool_read_script(&answer_list, job_id, jep) == true) {
               spool_delete_script(&answer_list, job_id, jep);
            } else {
               printf("could not read in script file\n");
            }
         }
         spool_delete_object(&answer_list, spool_get_default_context(),
                             SGE_TYPE_JOB,
                             job_get_key(job_id, 0, nullptr, &buffer),
                             job_spooling);
      }
      answer_list_output(&answer_list);
      sge_dstring_free(&buffer);
      bootstrap_set_job_spooling(true);
   }

   /* 
      if the job is in state running 
      we have to register each slot 
      in a queue, in the resource quota sets
      and in the parallel 
      environment if the job is a 
      parallel one
   */
   debit_all_jobs_from_qs();
   debit_all_jobs_from_pes(*object_type_get_master_list_rw(SGE_TYPE_PE));

   /*
    * Initialize cached values for each qinstance:
    *    - update suspend on subordinate state according to running jobs
    *    - update cached QI values.
    */
   const lList *master_hgroup_list = *object_type_get_master_list(SGE_TYPE_HGROUP);
   lList *master_cqueue_list = *object_type_get_master_list_rw(SGE_TYPE_CQUEUE);
   for_each_rw(tmpqep, *object_type_get_master_list(SGE_TYPE_CQUEUE)) {
      cqueue_mod_qinstances(ctx, tmpqep, nullptr, tmpqep, true, false, &monitor, master_hgroup_list, master_cqueue_list);
   }

   /* rebuild signal resend events */
   rebuild_signal_events();


   DPRINTF(("user list-----------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_USER), SGE_TYPE_USER);
   answer_list_output(&answer_list);

   remove_invalid_job_references(job_spooling, 1);

   DPRINTF(("project list-----------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_PROJECT), SGE_TYPE_PROJECT);
   answer_list_output(&answer_list);

   remove_invalid_job_references(job_spooling, 0);

   DPRINTF(("scheduler config -----------------------------------\n"));

   sge_read_sched_configuration(ctx, spooling_context, &answer_list);
   answer_list_output(&answer_list);

   DPRINTF(("share tree list-----------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list_rw(SGE_TYPE_SHARETREE),
                   SGE_TYPE_SHARETREE);
   answer_list_output(&answer_list);
   ep = lFirstRW(*object_type_get_master_list(SGE_TYPE_SHARETREE));
   if (ep) {
      lList *alp = nullptr;
      lList *found = nullptr;
      check_sharetree(&alp, ep, *object_type_get_master_list(SGE_TYPE_USER),
                      *object_type_get_master_list(SGE_TYPE_PROJECT), nullptr, &found);
      lFreeList(&found);
      lFreeList(&alp);
   }


   init_categories();

   DRETURN(0);
}

/****** setup_qmaster/remove_invalid_job_references() **************************
*  NAME
*     remove_invalid_job_references() -- ??? 
*
*  SYNOPSIS
*     static int remove_invalid_job_references(bool job_spooling, int user
*
*  FUNCTION
*   get rid of still debited per job usage contained 
*   in user or project object if the job is no longer existing
*
*  INPUTS
*     bool job_spooling
*     int user                        - work on users
*
*  RESULT
*     static int -  always 0
*
*  NOTES
*     MT-NOTE: remove_invalid_job_references() is not MT safe 
*
*******************************************************************************/
static int
remove_invalid_job_references(bool job_spooling, int user) {
   const lListElem *up;
   lListElem *upu, *next;
   u_long32 jobid;
   int object_key;
   const lList *object_list;
   sge_object_type object_type;
   const char *object_name;
   int debited_job_usage_key;

   DENTER(TOP_LAYER);

   if (user == 0) {
      object_key = PR_name;
      object_list = *object_type_get_master_list(SGE_TYPE_PROJECT);
      object_type = SGE_TYPE_PROJECT;
      object_name = MSG_OBJ_PRJ;
      debited_job_usage_key = PR_debited_job_usage;
   } else {
      object_key = UU_name;
      object_list = *object_type_get_master_list(SGE_TYPE_USER);
      object_type = SGE_TYPE_USER;
      object_name = MSG_OBJ_USER;
      debited_job_usage_key = UU_debited_job_usage;
   }

   for_each_ep(up, object_list) {
      int spool_me = 0;
      next = lFirstRW(lGetList(up, debited_job_usage_key));
      while ((upu = next)) {
         next = lNextRW(upu);

         jobid = lGetUlong(upu, UPU_job_number);
         if (!lGetElemUlong(*object_type_get_master_list(SGE_TYPE_JOB), JB_job_number, jobid)) {
            lRemoveElem(lGetListRW(up, debited_job_usage_key), &upu);
            WARNING((SGE_EVENT, "removing reference to no longer existing job "sge_u32" of %s "SFQ"\n",
                    jobid, object_name, lGetString(up, object_key)));
            spool_me = 1;
         }
      }

      if (spool_me) {
         lList *answer_list = nullptr;
         spool_write_object(&answer_list, spool_get_default_context(), up,
                            lGetString(up, object_key), object_type, job_spooling);
         answer_list_output(&answer_list);
      }
   }

   DRETURN(0);
}

static int debit_all_jobs_from_qs() {
   const lListElem *gdi;
   u_long32 slots;
   const char *queue_name;
   lListElem *next_jep = nullptr;
   lListElem *jep = nullptr;
   lListElem *qep = nullptr;
   lListElem *next_jatep = nullptr;
   lListElem *jatep = nullptr;
   int ret = 0;
   const lList *master_centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *object_type_get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_ar_list = *object_type_get_master_list(SGE_TYPE_AR);
   const lList *master_rqs_list = *object_type_get_master_list(SGE_TYPE_RQS);

   DENTER(TOP_LAYER);

   next_jep = lFirstRW(*object_type_get_master_list(SGE_TYPE_JOB));
   while ((jep = next_jep)) {

      /* may be we have to delete this job */
      next_jep = lNextRW(jep);

      next_jatep = lFirstRW(lGetList(jep, JB_ja_tasks));
      while ((jatep = next_jatep)) {
         bool master_task = true;
         next_jatep = lNextRW(jatep);

         /* don't look at states - we only trust in 
            "granted destin. ident. list" */

         for_each_ep(gdi, lGetList(jatep, JAT_granted_destin_identifier_list)) {
            u_long32 ar_id = lGetUlong(jep, JB_ar);
            const lListElem *ar = nullptr;

            queue_name = lGetString(gdi, JG_qname);
            slots = lGetUlong(gdi, JG_slots);

            if (!(qep = cqueue_list_locate_qinstance(master_cqueue_list, queue_name))) {
               ERROR((SGE_EVENT, MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU,
                       queue_name, sge_u32c(lGetUlong(jep, JB_job_number))));
               lRemoveElem(lGetListRW(jep, JB_ja_tasks), &jatep);
            } else if (ar_id != 0 && (ar = lGetElemUlong(master_ar_list, AR_id, ar_id)) == nullptr) {
               ERROR((SGE_EVENT, MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU,
                       sge_u32c(ar_id), sge_u32c(lGetUlong(jep, JB_job_number))));
               lRemoveElem(lGetListRW(jep, JB_ja_tasks), &jatep);
            } else {
               /* debit in all layers */
               lListElem *rqs = nullptr;
               debit_host_consumable(jep, host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST),
                                                           "global"), master_centry_list, slots, master_task, nullptr);
               debit_host_consumable(jep, host_list_locate(
                                             *object_type_get_master_list(SGE_TYPE_EXECHOST), lGetHost(qep, QU_qhostname)),
                                     master_centry_list, slots, master_task, nullptr);
               qinstance_debit_consumable(qep, jep, master_centry_list, slots, master_task, nullptr);
               for_each_rw (rqs, master_rqs_list) {
                  rqs_debit_consumable(rqs, jep, gdi, lGetString(jatep, JAT_granted_pe), master_centry_list,
                                       *object_type_get_master_list(SGE_TYPE_USERSET),
                                       *object_type_get_master_list(SGE_TYPE_HGROUP), slots, master_task);
               }
               if (ar != nullptr) {
                  lListElem *queue = lGetSubStr(ar, QU_full_name, lGetString(gdi, JG_qname), AR_reserved_queues);
                  if (queue != nullptr) {
                     qinstance_debit_consumable(queue, jep, master_centry_list, slots, master_task, nullptr);
                  } else {
                     ERROR((SGE_EVENT, "job "sge_U32CFormat" runs in queue "SFQ" not reserved by AR "sge_U32CFormat,
                             sge_u32c(lGetUlong(jep, JB_job_number)), lGetString(gdi, JG_qname), sge_u32c(ar_id)));
                  }
               }
            }
            master_task = false;
         }
      }
   }

   DRETURN(ret);
}

/****** setup_qmaster/init_categories() ****************************************
*  NAME
*     init_categories() -- Initialize usersets/projects wrts categories
*
*  SYNOPSIS
*     static void init_categories(void)
*
*  FUNCTION
*     Initialize usersets/projects wrts categories.
*
*  NOTES
*     MT-NOTE: init_categories() is not MT safe
*******************************************************************************/
static void init_categories(void) {
   const lListElem *cq, *pe, *hep, *ep;
   lListElem *acl, *prj;
   const lListElem *rqs;
   lList *u_list = nullptr, *p_list = nullptr;
   const lList *master_project_list = *object_type_get_master_list(SGE_TYPE_PROJECT);
   const lList *master_userset_list = *object_type_get_master_list(SGE_TYPE_USERSET);
   bool all_projects = false;
   bool all_usersets = false;

   /*
    * collect a list of references to usersets/projects used in
    * the resource quota sets
    */
   for_each_ep(rqs, *object_type_get_master_list(SGE_TYPE_RQS)) {
      if (!all_projects && !rqs_diff_projects(rqs, nullptr, &p_list, nullptr, master_project_list)) {
         all_projects = true;
      }
      if (!all_usersets && !rqs_diff_usersets(rqs, nullptr, &u_list, nullptr, master_userset_list)) {
         all_usersets = true;
      }
      if (all_usersets && all_projects) {
         break;
      }
   }

   /*
    * collect list of references to usersets/projects used as ACL
    * with queue_conf(5), host_conf(5) and sge_pe(5)
    */
   for_each_ep(cq, *object_type_get_master_list(SGE_TYPE_CQUEUE)) {
      cqueue_diff_projects(cq, nullptr, &p_list, nullptr);
      cqueue_diff_usersets(cq, nullptr, &u_list, nullptr);
   }

   for_each_ep(pe, *object_type_get_master_list(SGE_TYPE_PE)) {
      pe_diff_usersets(pe, nullptr, &u_list, nullptr);
   }

   for_each_ep(hep, *object_type_get_master_list(SGE_TYPE_EXECHOST)) {
      host_diff_projects(hep, nullptr, &p_list, nullptr);
      host_diff_usersets(hep, nullptr, &u_list, nullptr);
   }

   /*
    * now set categories flag with usersets/projects used as ACL
    */
   for_each_ep(ep, p_list)if ((prj = prj_list_locate(master_project_list, lGetString(ep, PR_name))))
         lSetBool(prj, PR_consider_with_categories, true);

   for_each_ep(ep, u_list)if ((acl = userset_list_locate(master_userset_list, lGetString(ep, US_name))))
         lSetBool(acl, US_consider_with_categories, true);

   lFreeList(&p_list);
   lFreeList(&u_list);
}

