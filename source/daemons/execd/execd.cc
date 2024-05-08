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
#include <cstdlib>

#include "uti/sge_log.h"
#include "uti/sge_monitor.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_profiling.h"

#include "gdi/sge_gdi.h"
#include "gdi/oge_gdi_client.h"

#include "sgeobj/oge_DataStore.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_conf.h"

#include "spool/classic/read_write_job.h"

#include "sge_load_sensor.h"
#include "dispatcher.h"
#include "load_avg.h"
#include "reaper_execd.h"
#include "setup_execd.h"
#include "shutdown.h"
#include "sig_handlers.h"
#include "usage.h"
#include "execd.h"
#include "sge.h"
#include "msg_common.h"
#include "msg_execd.h"

#ifdef COMPILE_DC
#   include "ptf.h"
#endif

#if defined(SOLARIS)
#   include "sge_smf.h"
#   include "sge_string.h"
#endif

#if defined(LINUX)
#  include "sge_proc.h"

#endif



volatile int jobs_to_start = 1;

/* only used when running as SGE execd */
volatile int waiting4osjid = 1;

/* Store the directory the execd runs in when in normal operation.
 * avoid calling getcwd, cause this catches zombies on sun and is a !?GRML call!
 */
char execd_spool_dir[SGE_PATH_MAX];

static void execd_exit_func(int i);
static void parse_cmdline_execd(char **argv);
static lList *sge_parse_cmdline_execd(char **argv, lList **ppcmdline);
static lList *sge_parse_execd(lList **ppcmdline, lList **ppreflist, u_long32 *help);

int main(int argc, char *argv[]);
static u_long32 last_qmaster_registration_time = 0;


u_long32 get_last_qmaster_register_time() {
   return last_qmaster_registration_time;
}

/****** execd/sge_execd_application_status() ***********************************
*  NAME
*     sge_execd_application_status() -- commlib status callback function
*
*  SYNOPSIS
*     unsigned long sge_execd_application_status(char** info_message) 
*
*  FUNCTION
*      This is the implementation of the commlib application status callback
*      function. This function is called from the commlib when a connected
*      client wants to get a SIRM (Status Information Response Message).
*      The standard client for this action is the qping command.
*
*      The callback function is set with cl_com_set_status_func() after
*      commlib initalization.
*
*      The function is called by a commlib function which may not run in the
*      context of the execd application. This means no execd specific
*      functions should be called (e.g. locking of global variables).
*
*      status 0:  no errors
*      status 1:  dispatcher has reached warning timeout
*      status 2:  dispatcher has reached error timeout
*      status 3:  dispatcher alive timeout struct not initalized
*
*  INPUTS
*     char** info_message - pointer to an char* inside commlib.
*                           info message must be malloced, commlib will
*                           free this memory. 
*  RESULT
*     unsigned long status - status of application
*
*  NOTES
*     This function is MT save
*******************************************************************************/
unsigned long sge_execd_application_status(char** info_message) 
{
   return sge_monitor_status(info_message, 0);
}

int sge_execd_register_at_qmaster_try_forever() {
   DENTER(TOP_LAYER);

   int ret = 0;

   // here we have to wait for qmaster registration
   while (sge_execd_register_at_qmaster(false) != 0) {
      if (sge_get_com_error_flag(EXECD, SGE_COM_ACCESS_DENIED, true)) {
         // This is no error
         DPRINTF("*****  got SGE_COM_ACCESS_DENIED from qmaster  *****\n");
      }
      if (sge_get_com_error_flag(EXECD, SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         ret = SGE_COM_ENDPOINT_NOT_UNIQUE;
         break;
      }
      if (shut_me_down != 0) {
         break;
      }
      sleep(30);
      execd_reread_act_qmaster();
   }

   DRETURN(ret);
}


/*-------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   int ret;
   int my_pid;
   int ret_val;
   int printed_points = 0;
   int max_enroll_tries;
   static char tmp_err_file_name[SGE_PATH_MAX];
   time_t next_prof_output = 0;
   int execd_exit_state;
   lList **master_job_list;
   lList *alp = nullptr;

   DENTER_MAIN(TOP_LAYER, "execd");

   lInit(nmv);

#if defined(LINUX)
   gen_procList ();
#endif

   set_thread_name(pthread_self(),"Execd Thread");

   prof_set_level_name(SGE_PROF_CUSTOM1, "Execd Thread", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM2, "Execd Dispatch", nullptr);

#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */

   /* This needs a better solution */
   umask(022);
      
   /* Initialize path for temporary logging until we chdir to spool */
   my_pid = getpid();
   snprintf(tmp_err_file_name, sizeof(tmp_err_file_name), "%s." sge_U32CFormat "", TMP_ERR_FILE_EXECD, sge_u32c(my_pid));
   log_state_set_log_file(tmp_err_file_name);

   /* exit func for sge_exit() */
   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(EXECD);

   if (gdi_client_setup(EXECD, MAIN_THREAD, &alp, false) != AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }
   component_set_exit_func(execd_exit_func);
   
#if defined(SOLARIS)
   /* Init shared SMF libs if necessary */
   if (sge_smf_used() == 1 && sge_smf_init_libs() != 0) {
       sge_exit(1);
   }
#endif

   /* prepare daemonize */
   if (!getenv("SGE_ND")) {
      sge_daemonize_prepare();
   }

   if ((ret=sge_occupy_first_three())>=0) {
      CRITICAL(MSG_FILE_REDIRECTFD_I, ret);
      sge_exit(1);
   }

   /* unset XAUTHORITY if set */
   if (getenv("XAUTHORITY") != nullptr) {
      sge_unsetenv("XAUTHORITY");
   }

   parse_cmdline_execd(argv);   
   
   /* exit if we can't get communication handle (bind port) */
   max_enroll_tries = 30;
   while (cl_com_get_handle(prognames[EXECD],1) == nullptr) {
      gdi_client_prepare_enroll(&alp);
      max_enroll_tries--;

      if (max_enroll_tries <= 0 || shut_me_down) {
         /* exit after 30 seconds */
         if (printed_points != 0) {
            printf("\n");
         }
         CRITICAL(SFNMAX, MSG_COM_ERROR);
         sge_exit(1);
      }
      if (cl_com_get_handle(prognames[EXECD],1) == nullptr) {
        /* sleep when prepare_enroll() failed */
        sleep(1);
        if (max_enroll_tries < 27) {
           printf(".");
           printed_points++;
           fflush(stdout);
        }
      }
   }

   if (printed_points != 0) {
      printf("\n");
   }

   /*
    * now the commlib up and running. Set execd application status function 
    * ( commlib callback function for qping status information response 
    *   messages (SIRM) )
    */
   ret_val = cl_com_set_status_func(sge_execd_application_status);
   if (ret_val != CL_RETVAL_OK) {
      ERROR(SFNMAX, cl_get_error_text(ret_val));
   }

   /* test connection */
   {
      cl_com_SIRM_t* status = nullptr;
      ret_val = cl_commlib_get_endpoint_status(cl_com_get_handle(component_get_component_name(), 0),
                                               (char *)gdi_get_act_master_host(true),
                                               (char*)prognames[QMASTER], 1, &status);
      if (ret_val != CL_RETVAL_OK) {
         ERROR(SFNMAX, cl_get_error_text(ret_val));
         ERROR(SFNMAX, MSG_CONF_NOCONFBG);
      }
      cl_com_free_sirm_message(&status);
   }
   
   /* finalize daeamonize */
   if (getenv("SGE_ND") == nullptr) {
      sge_daemonize_finalize();
   }

   /* daemonizes if qmaster is unreachable */
   sge_setup_sge_execd_before_register();

   execd_exit_state = sge_execd_register_at_qmaster_try_forever();

   /*
    * Terminate on SIGTERM or hard communication error
    */
   if (execd_exit_state != 0 || shut_me_down != 0) {
      sge_shutdown(execd_exit_state);
      DRETURN(execd_exit_state);
   }

   sge_setup_execd_after_register(tmp_err_file_name);

   /* are we using qidle or not */
   sge_ls_qidle(mconf_get_use_qidle());
   sge_ls_gnu_ls(1);
   
   DPRINTF("use_qidle: %d\n", mconf_get_use_qidle());

   /* test load sensor (internal or external) */
   {
      lList *report_list = sge_build_load_report(component_get_qualified_hostname(), bootstrap_get_binary_path());
      lFreeList(&report_list);
   }

   /*
    * We write pid file when we are connected to qmaster. Otherwise an old
    * execd might overwrite our pidfile.
    */
   sge_write_pid(EXECD_PID_FILE);

   /*
    * At this point we are sure we are the only sge_execd and we are connected
    * to the current qmaster. First we have to report any reaped children
    * that might exist.
    */
   starting_up();

   /*
    * Log a warning message if execd hasn't been started by a superuser
    */
   if (!sge_is_start_user_superuser()) {
      WARNING(SFNMAX, MSG_SWITCH_USER_NOT_ROOT);
   }   

#ifdef COMPILE_DC
   if (ptf_init()) {
      CRITICAL(SFNMAX, MSG_EXECD_NOSTARTPTF);
      sge_exit(1);
   }
   INFO(SFNMAX, MSG_EXECD_STARTPDCANDPTF);
#endif

   master_job_list = oge::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   *master_job_list = lCreateList("master job list", JB_Type);
   job_list_read_from_disk(master_job_list, "master job list", 0, SPOOL_WITHIN_EXECD, job_initialize_job);

   /* clean up jobs hanging around (look in active_dir) */
   clean_up_old_jobs(1);
   execd_trash_load_report();
   sge_set_flush_lr_flag(true);

   sge_sig_handler_in_main_loop = 1;

   if (thread_prof_active_by_id(pthread_self())) {
      prof_start(SGE_PROF_CUSTOM1, nullptr);
      prof_start(SGE_PROF_CUSTOM2, nullptr);
      prof_start(SGE_PROF_GDI_REQUEST, nullptr);
   } else {
      prof_stop(SGE_PROF_CUSTOM1, nullptr);
      prof_stop(SGE_PROF_CUSTOM2, nullptr);
      prof_stop(SGE_PROF_GDI_REQUEST, nullptr);
   }

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);

   /* Start dispatching */
   execd_exit_state = sge_execd_process_messages();


   /*
    * This code is only reached when dispatcher terminates and execd goes down.
    */

   /* log if we received SIGPIPE signal */
   if (sge_sig_handler_sigpipe_received) {
       sge_sig_handler_sigpipe_received = 0;
       INFO("SIGPIPE received\n");
   }

#if defined(LINUX)
   free_procList();
#endif
   lFreeList(master_job_list);

   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   if (prof_is_active(SGE_PROF_ALL)) {
     time_t now = (time_t)sge_get_gmt();

      if (now > next_prof_output) {
         prof_output_info(SGE_PROF_ALL, false, "profiling summary:\n");
         prof_reset(SGE_PROF_ALL,nullptr);
         next_prof_output = now + 60;
      }
   }
   sge_prof_cleanup();

   sge_shutdown(execd_exit_state);
   DRETURN(execd_exit_state);
}


/*-------------------------------------------------------------
 * Function installed to be called just before exit() is called.
 * clean up
 *-------------------------------------------------------------*/
static void execd_exit_func(int i)
{
   DENTER(TOP_LAYER);

   gdi_client_shutdown();

   /* trigger load sensors shutdown */
   sge_ls_stop(0);

#ifdef COMPILE_DC
   ptf_stop();
#endif
   
#if defined(SOLARIS)
   if (sge_smf_used() == 1) {
      /* We don't do disable on svcadm restart */
      if (sge_strnullcmp(sge_smf_get_instance_state(), SCF_STATE_STRING_ONLINE) == 0 &&
          sge_strnullcmp(sge_smf_get_instance_next_state(), SCF_STATE_STRING_NONE) == 0) {      
         sge_smf_temporary_disable_instance();
      }
   }
#endif  
   DRETURN_VOID;
}

/****** execd/sge_execd_register_at_qmaster() **********************************
*  NAME
*     sge_execd_register_at_qmaster() -- modify execd list at qmaster site
*
*  SYNOPSIS
*     int sge_execd_register_at_qmaster(void) 
*
*  FUNCTION
*     add local execd name to SGE_EH_LIST in order to register at
*     qmaster
*
*  INPUTS
*     void - no input
*
*  RESULT
*     int - 0 = success / 1 = error
*
*  NOTES
*     MT-NOTE: sge_execd_register_at_qmaster() is not MT safe 
*
*******************************************************************************/
int sge_execd_register_at_qmaster(bool is_restart) {
   int return_value = 0;
   static int sge_last_register_error_flag = 0;
   lList *alp = nullptr;

   /* 
    * If it is a reconnect (is_restart == true) the act_qmaster file must be
    * re-read in order to update ctx qmaster cache when master migrates. 
    */
   const char *master_host = gdi_get_act_master_host(is_restart);

   DENTER(TOP_LAYER);

   /* We will not try to make a gdi request when qmaster is not alive. The
    * gdi will return with timeout after one minute. If qmaster is not alive
    * we will not try a gdi request!
    */
   if (master_host != nullptr && gdi_is_alive(&alp) == CL_RETVAL_OK) {
      lList *hlp = lCreateList("exechost starting", EH_Type);
      lListElem *hep = lCreateElem(EH_Type);
      lSetUlong(hep, EH_featureset_id, feature_get_active_featureset_id());

      // here we can add information for an auto add
      //lListElem *cep = lAddSubStr(hep, CE_name, "slots", EH_consumable_config_list, CE_Type);
      //lSetString(cep, CE_stringval, "5");
      //lSetDouble(cep, CE_doubleval, 5);
      const char *hgrp = sge_getenv(LOAD_ATTR_AUTO_EH_HOSTGROUP);
      if (hgrp != nullptr) {
         lListElem *lep = lAddSubStr(hep, HL_name, LOAD_ATTR_AUTO_EH_HOSTGROUP, EH_load_list, HL_Type);
         if (lep != nullptr) {
            lSetString(lep, HL_value, hgrp);
         }
      }

      lAppendElem(hlp, hep);

      /* register at qmaster */
      DPRINTF("*****  Register at qmaster   *****\n");
      // on a regular startup sge_execd registers at sge_qmaster by sending a
      // GDI ADD EXECHOST request
      u_long32 cmd = SGE_GDI_ADD;
      if (is_restart) {
         // in case of a restart the initial queue state will not be set
         cmd |= SGE_GDI_EXECD_RESTART;
      }
      alp = sge_gdi(SGE_EH_LIST, cmd, &hlp, nullptr, nullptr);
      lFreeList(&hlp);
   } else {
      DPRINTF("*****  Register at qmaster - qmaster not alive!  *****\n");
   }

   if (alp == nullptr) {
      if (sge_last_register_error_flag == 0) {
         WARNING(MSG_COM_CANTREGISTER_SS, master_host?master_host:"", MSG_COM_ERROR);
         sge_last_register_error_flag = 1;
      }
      return_value = 1;
   } else {
      const lListElem *aep = lFirst(alp);
      if (lGetUlong(aep, AN_status) != STATUS_OK) {
         if (sge_last_register_error_flag == 0) {
            WARNING(MSG_COM_CANTREGISTER_SS, master_host?master_host:"", lGetString(aep, AN_text));
            sge_last_register_error_flag = 1;
         }
         return_value = 1;
      }
   }
 
   if (return_value == 0) {
      sge_last_register_error_flag = 0;
      INFO(MSG_EXECD_REGISTERED_AT_QMASTER_S, master_host?master_host:"");
      last_qmaster_registration_time = sge_get_gmt();
   }
   lFreeList(&alp);
   DRETURN(return_value);
}


/*---------------------------------------------------------------------
 * parse_cmdline_execd
 *---------------------------------------------------------------------*/
static void parse_cmdline_execd(char **argv)
{
   lList *ref_list = nullptr, *alp = nullptr, *pcmdline = nullptr;
   const lListElem *aep;
   u_long32 help = 0;

   DENTER(TOP_LAYER);
            
   alp = sge_parse_cmdline_execd(argv+1, &pcmdline);
   if(alp) {
      /* 
      ** high level parsing error! show answer list
      */
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      /* TODO: replace with alpp and DRETURN */
      sge_exit(1);
   }

   alp = sge_parse_execd(&pcmdline, &ref_list, &help);
   lFreeList(&pcmdline);
   lFreeList(&ref_list);

   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      /* TODO: replace with alpp and DRETURN */
      sge_exit(1);
   }
   lFreeList(&alp);

   if(help) {
      /*
      ** user wanted only help. we can exit!
      */
      /* TODO: replace with alpp and DRETURN */
      sge_exit(0);
   }
   DRETURN_VOID;
}


/*-------------------------------------------------------------
 * sge_parse_cmdline_execd
 *
 *-------------------------------------------------------------*/ 
static lList *sge_parse_cmdline_execd(char **argv, lList **ppcmdline)
{
char **sp;
char **rp;
stringT str;
lList *alp = nullptr;

   DENTER(TOP_LAYER);

   rp = argv;
   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, &alp)) != sp)
         continue;

      /* -nostart-commd */
      if ((rp = parse_noopt(sp, "-nostart-commd", nullptr, ppcmdline, &alp)) != sp)
         continue;

      /* -lj */
      if ((rp = parse_until_next_opt(sp, "-lj", nullptr, ppcmdline, &alp)) != sp)
         continue;

      /* oops */
      snprintf(str, sizeof(str), MSG_PARSE_INVALIDARG_S, *sp);
      sge_usage(EXECD, stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(alp);
   }
   DRETURN(alp);
}


/*-------------------------------------------------------------
 * sge_parse_execd
 *
 *-------------------------------------------------------------*/
static lList *sge_parse_execd(lList **ppcmdline, lList **ppreflist, 
                              u_long32 *help) 
{
   lList *alp = nullptr;
   int usageshowed = 0;

   DENTER(TOP_LAYER);

   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */
   while(lGetNumberOfElem(*ppcmdline)) {
      /* -help */
      if(parse_flag(ppcmdline, "-help", &alp, help)) {
         usageshowed = 1;
         sge_usage(EXECD, stdout);
         break;
      }
   }
   
   if(lGetNumberOfElem(*ppcmdline)) {
      if(!usageshowed) {
         sge_usage(EXECD, stderr);
      }
      answer_list_add(&alp, MSG_PARSE_TOOMANYARGS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(alp);
   }

   DRETURN(alp);
}

/* JG: TODO: we have this searching code in many places!! */
/****** execd/execd_get_job_ja_task() ******************************************
*  NAME
*     execd_get_job_ja_task() -- search job and ja_task by id
*
*  SYNOPSIS
*     bool
*     execd_get_job_ja_task(u_long32 job_id, u_long32 ja_task_id,
*                           lListElem **job, lListElem **ja_task) 
*
*  FUNCTION
*     Searches the execd master lists for job and ja_task
*     defined by job_id and ja_task_id.
*
*  INPUTS
*     u_long32 job_id     - job id
*     u_long32 ja_task_id - ja_task id
*     lListElem **job     - returns job or nullptr if not found
*     lListElem **ja_task - returns ja_task or nullptr if not found
*
*  RESULT
*     bool - true if both job and ja_task are found, else false
*
*  NOTES
*     MT-NOTE: execd_get_job_ja_task() is MT safe 
*******************************************************************************/
bool execd_get_job_ja_task(u_long32 job_id, u_long32 ja_task_id, lListElem **job, lListElem **ja_task)
{
   const void *iterator = nullptr;

   DENTER(TOP_LAYER);

   *job = lGetElemUlongFirstRW(*oge::DataStore::get_master_list_rw(SGE_TYPE_JOB), JB_job_number, job_id, &iterator);
   while (*job != nullptr) {
      *ja_task = job_search_task(*job, nullptr, ja_task_id);
      if (*ja_task != nullptr) {
         DRETURN(true);
      }

      /* in execd, we have exactly one ja_task per job,
       * therefore we can have multiple jobs with the same job_id
       */
      *job = lGetElemUlongNextRW(*oge::DataStore::get_master_list(SGE_TYPE_JOB), JB_job_number, job_id, &iterator);
   }
   
   if (*job == nullptr) {
      ERROR(MSG_JOB_TASKWITHOUTJOB_U, sge_u32c(job_id));
   } else if (*ja_task == nullptr) {
      ERROR(MSG_JOB_TASKNOTASKINJOB_UU, sge_u32c(job_id), sge_u32c(ja_task_id));
   }

   *job = nullptr;
   *ja_task = nullptr;
   DRETURN(false);
}
