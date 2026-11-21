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
 * @brief sge_execd - runs the jobs qmaster assigns to this host
 */
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include <cstdlib>
#include <utility>

#include "uti/ocs_Bootstrap.h"
#include "uti/ocs_Systemd.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_monitor.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_profiling.h"

#include "gdi/ocs_gdi_Client.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "spool/classic/read_write_job.h"

#include "sge_load_sensor.h"
#include "dispatcher.h"
#include "ocs_execd_systemd.h"
#include "execd_profiling.h"
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
#include "ocs_DebugParam.h"

#ifdef COMPILE_DC
#   include "ptf.h"
#   include "sgedefs.h"
#endif

#if defined(SOLARIS)
#   include "sge_smf.h"
#   include "sge_string.h"
#endif

#if defined(LINUX)
#  include "sge_proc.h"
#endif



/** @brief Set from a signal handler when jobs may be started
 *
 * The daemon does not start jobs while it is still working out what is already
 * running on this host.
 */
volatile int jobs_to_start = 1;

/* only used when running as SGE execd */
volatile int waiting4osjid = 1;   ///< Still waiting for an operating system job id to become available

/* Store the directory the execd runs in when in normal operation.
 * avoid calling getcwd, cause this catches zombies on sun and is a !?GRML call!
 */
char execd_spool_dir[SGE_PATH_MAX];   ///< Where the daemon spools; see the comment above for why it is cached

static void execd_exit_func(int i);
static void parse_cmdline_execd(char **argv);
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_cmdline_execd(char **argv,
                                                               ocs::tom::ListPtr<ocs::tom::SPA> &ppcmdline);
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_execd(ocs::tom::List<ocs::tom::SPA> ppcmdline, uint32_t *help);

static uint64_t last_qmaster_registration_time = 0;
/** @brief When this daemon last registered with qmaster
 * @return the time, or 0 when it never has
 */
uint64_t get_last_qmaster_register_time() {
   return last_qmaster_registration_time;
}

/**
 * @brief Commlib status callback function
 *
 *  This is the implementation of the commlib application status callback
 *  function. This function is called from the commlib when a connected
 *  client wants to get a SIRM (Status Information Response Message).
 *  The standard client for this action is the qping command.
 *  The callback function is set with cl_com_set_status_func() after
 *  commlib initalization.
 *  The function is called by a commlib function which may not run in the
 *  context of the execd application. This means no execd specific
 *  functions should be called (e.g. locking of global variables).
 *  status 0:  no errors
 *  status 1:  dispatcher has reached warning timeout
 *  status 2:  dispatcher has reached error timeout
 *  status 3:  dispatcher alive timeout struct not initalized
 *
 * @param info_message pointer to an char* inside commlib. info message must be malloced, commlib will free this memory.
 *
 * @return status of application
 *
 * @note This function is MT save
 */
unsigned long sge_execd_application_status(char **info_message) {
   return sge_monitor_status(info_message, 0);
}

/**
 * @brief The entry point of sge_execd
 *
 * Sets the daemon up, registers it at qmaster, reads the spooled jobs into the
 * job list of the data store and runs the dispatcher until the daemon shall exit.
 *
 * @param argc the number of command line arguments, unused
 * @param argv the command line arguments
 * @return the exit state of the daemon
 */
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "execd");

   int ret;
   int my_pid;
   int ret_val;
   int printed_points = 0;
   int max_enroll_tries;
   static char tmp_err_file_name[SGE_PATH_MAX];
   int execd_exit_state = 0;
   lList **master_job_list = nullptr;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   ocs::tom::install_model(ocs::tom::gcs_model());

#if defined(LINUX)
   gen_procList ();
#endif

   set_thread_name(pthread_self(),"Execd Thread");

   ocs::execd_profiling_initialize();

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
   snprintf(tmp_err_file_name, sizeof(tmp_err_file_name), "%s." pid_t_fmt, TMP_ERR_FILE_EXECD, my_pid);
   log_state_set_log_file(tmp_err_file_name);

   /* exit func for sge_exit() */
   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(EXECD);

   ocs::TerminationManager::install_signal_handler();

   if (ocs::gdi::ClientBase::setup(EXECD, MAIN_THREAD, &alp, false) != ocs::gdi::ErrorValue::AE_OK) {
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
   bool in_nd_mode = ocs::DebugParam::is_component_in_nd_mode();
   if (!in_nd_mode) {
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

   // initialize the admin user
   char err_str[MAX_STRING_SIZE];
   const char *admin_user = ocs::Bootstrap::get_admin_user();
   if (sge_set_admin_username(admin_user, err_str, sizeof(err_str))) {
      CRITICAL(SFNMAX, err_str);
      sge_exit(1);
   }

   /* exit if we can't get a communication handle (bind port) */
   max_enroll_tries = 30;
   while (cl_com_get_handle(to_cstr(EXECD),1) == nullptr) {
      ocs::gdi::ClientBase::prepare_enroll(&alp);
      max_enroll_tries--;

      if (max_enroll_tries <= 0 || shut_me_down) {
         /* exit after 30 seconds */
         if (printed_points != 0) {
            printf("\n");
         }
         CRITICAL(SFNMAX, MSG_COM_ERROR);
         sge_exit(1);
      }
      if (cl_com_get_handle(to_cstr(EXECD),1) == nullptr) {
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
                                               (char *)ocs::gdi::ClientBase::gdi_get_act_master_host(true),
                                               to_cstr(QMASTER), 1, &status);
      if (ret_val != CL_RETVAL_OK) {
         ERROR(SFNMAX, cl_get_error_text(ret_val));
         ERROR(SFNMAX, MSG_CONF_NOCONFBG);
      }
      cl_com_free_sirm_message(&status);
   }

   /* finalize daemonize */
   if (!in_nd_mode) {
      sge_daemonize_finalize();
   }

   /* daemonizes if qmaster is unreachable */
   sge_setup_sge_execd_before_register();

   /* here we have to wait for qmaster registration
    *
    * the registration has to happen before the configuration of the host is fetched: a host which
    * adds itself to the cluster is not known to the qmaster before it registered and may not read
    * the configuration yet. See the qmaster_params AUTO_ADD_HOSTS.
    */
   while (sge_execd_register_at_qmaster(false) != 0) {
      if (ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_ACCESS_DENIED, true)) {
         /* This is no error */
         DPRINTF("*****  got SGE_COM_ACCESS_DENIED from qmaster  *****\n");
      }
      if (ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         execd_exit_state = ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE;
         break;
      }
      if (shut_me_down != 0) {
         break;
      }
      sleep(30);

      // the qmaster might have been started on a different host or the act_qmaster file
      // might have been corrected in the meantime: re-read it before the next attempt
      ocs::gdi::ClientBase::gdi_get_act_master_host(true);
   }

   /*
    * Terminate on SIGTERM or hard communication error
    */
   if (execd_exit_state != 0 || shut_me_down != 0) {
      sge_shutdown(execd_exit_state);
      DRETURN(execd_exit_state);
   }

   sge_setup_sge_execd_after_register(tmp_err_file_name);

   /* are we using qidle or not */
   sge_ls_qidle(mconf_get_use_qidle());
   sge_ls_gnu_ls(1);

   DPRINTF("use_qidle: %d\n", mconf_get_use_qidle());

   /* test load sensor (internal or external) */
   {
      const auto report_list = sge_build_load_report(component_get_qualified_hostname(),
                                                     ocs::Bootstrap::get_binary_path());
   }

   /*
    * We write a pid file when we are connected to qmaster. Otherwise, an old
    * execd might overwrite our pidfile.
    */
   sge_write_pid(EXECD_PID_FILE);

   /*
    * At this point, we are sure we are the only sge_execd, and we are connected
    * to the current qmaster. First, we have to report any reaped children
    * that might exist.
    */
   starting_up();

#if defined (OCS_WITH_SYSTEMD)
   ocs::execd::execd_systemd_init();
#endif

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

   master_job_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   // the data store holds the job list until the daemon goes down
   ocs::DataStore::replace_master_list<SGE_TYPE_JOB>(ocs::tom::ListPtr<ocs::tom::JB>::create("master job list"));
   job_list_read_from_disk(master_job_list, "master job list", 0, SPOOL_WITHIN_EXECD, job_initialize_job);

   /* clean up jobs hanging around (look in active_dir) */
   clean_up_old_jobs(true);
   execd_trash_load_report();
   sge_set_flush_lr_flag(true);

   sge_sig_handler_in_main_loop = 1;

   // Start dispatching - sge_execd_process_messages() will only return when sge_execd shall exit.
   execd_exit_state = sge_execd_process_messages();

   // This code is only reached when the dispatcher terminates and execd goes down.

   /* log if we received SIGPIPE signal */
   if (sge_sig_handler_sigpipe_received) {
       sge_sig_handler_sigpipe_received = 0;
       INFO("SIGPIPE received");
   }

#if defined(LINUX)
   free_procList();
#endif
   // the data store gives the job list up, as lFreeList() did
   ocs::DataStore::replace_master_list<SGE_TYPE_JOB>(nullptr);

   ocs::execd_profiling_cleanup();

   sge_shutdown(execd_exit_state);

   DRETURN(execd_exit_state);
}


/*-------------------------------------------------------------
 * Function installed to be called just before exit() is called.
 * clean up
 *-------------------------------------------------------------*/
static void execd_exit_func(int i) {
   DENTER(TOP_LAYER);

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

   // Shutdown commlib connections
   cl_com_cleanup_commlib();

   component_ts0_destroy();
   DRETURN_VOID;
}

/**
 * @brief Modify execd list at qmaster site
 *
 * add local execd name to SGE_EH_LIST in order to register at
 * qmaster
 *
 * @param is_restart whether the daemon is re-registering after losing contact
 * @return 0 = success / 1 = error
 *
 * @note MT-NOTE: sge_execd_register_at_qmaster() is not MT safe
 */
int sge_execd_register_at_qmaster(bool is_restart) {
   DENTER(TOP_LAYER);

   int return_value = 0;
   static int sge_last_register_error_flag = 0;
   // the answer list, filled through an lList ** by gdi_is_alive() and returned by sge_gdi()
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   /*
    * If it is a reconnect (is_restart is true) the act_qmaster file must be
    * re-read in order to update ctx qmaster cache when master migrates.
    */
   const char *master_host = ocs::gdi::ClientBase::gdi_get_act_master_host(is_restart);

   /* We will not try to make a gdi request when qmaster is not alive. The
    * gdi will return with timeout after one minute. If qmaster is not alive
    * we will not try a gdi request!
    */
   if (master_host != nullptr && ocs::gdi::ClientBase::gdi_is_alive(&alp) == CL_RETVAL_OK) {
      auto hlp = ocs::tom::ListPtr<ocs::tom::EH>::create("exechost starting");
      hlp->push_back(ocs::tom::ElementPtr<ocs::tom::EH>::create());

      /* register at qmaster */
      DPRINTF("*****  Register at qmaster   *****\n");
      if (!is_restart) {
         /*
          * This is a regular startup.
          */
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::ADD,
                                             ocs::gdi::SubCommand::NONE, hlp);
      } else {
         /*
          * Indicate this is a restart to qmaster.
          * This is used for the initial_state of queue_configuration implementation.
          */
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::ADD,
                                             ocs::gdi::SubCommand::EXECD_RESTART, hlp);
      }
      hlp.reset();
   } else {
      DPRINTF("*****  Register at qmaster - qmaster not alive!  *****\n");
   }

   if (!alp) {
      if (sge_last_register_error_flag == 0) {
         WARNING(MSG_COM_CANTREGISTER_SS, master_host?master_host:"", MSG_COM_ERROR);
         sge_last_register_error_flag = 1;
      }
      return_value = 1;
   } else {
      const auto aep = alp->front();
      if (aep.get<AN_status>() != STATUS_OK) {
         if (sge_last_register_error_flag == 0) {
            WARNING(MSG_COM_CANTREGISTER_SS, master_host?master_host:"", aep.get<AN_text>());
            sge_last_register_error_flag = 1;
         }
         return_value = 1;
      }
   }

   if (return_value == 0) {
      sge_last_register_error_flag = 0;
      INFO(MSG_EXECD_REGISTERED_AT_QMASTER_S, master_host?master_host:"");
      last_qmaster_registration_time = sge_get_gmt64();
   }
   DRETURN(return_value);
}


/**
 * @brief Parse the command line of sge_execd; exits the daemon on an error or after -help
 *
 * @param argv the command line arguments, the name of the program first
 */
static void parse_cmdline_execd(char **argv) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::SPA> pcmdline;
   uint32_t help = 0;

   auto alp = sge_parse_cmdline_execd(argv+1, pcmdline);
   if(alp) {
      /*
      ** high level parsing error! show answer list
      */
      for (const auto aep : *alp) {
         fprintf(stderr, "%s", aep.get<AN_text>());
      }
      alp.reset();
      pcmdline.reset();
      /* TODO: replace with alpp and DRETURN */
      sge_exit(1);
   }

   alp = sge_parse_execd(*pcmdline, &help);
   pcmdline.reset();

   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for (const auto aep : *alp) {
         fprintf(stderr, "%s", aep.get<AN_text>());
      }
      alp.reset();
      /* TODO: replace with alpp and DRETURN */
      sge_exit(1);
   }
   alp.reset();

   if(help) {
      /*
      ** user wanted only help. we can exit!
      */
      /* TODO: replace with alpp and DRETURN */
      sge_exit(0);
   }
   DRETURN_VOID;
}


/**
 * @brief Parse the command line options of sge_execd into a list of options
 *
 * @param argv the command line arguments, without the name of the program
 * @param[out] ppcmdline receives the options found
 * @return the owning handle on the answer list of an invalid argument, empty if all arguments are valid
 */
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_cmdline_execd(char **argv,
                                                               ocs::tom::ListPtr<ocs::tom::SPA> &ppcmdline) {
   DENTER(TOP_LAYER);

   char **sp;
   char **rp;
   stringT str;
   // filled through an lList ** by the parsing functions
   ocs::tom::ListPtr<ocs::tom::AN> alp;

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


/**
 * @brief Evaluate the parsed command line options of sge_execd
 *
 * @param ppcmdline the options; an option evaluated is removed
 * @param[out] help set if -help was given
 * @return the owning handle on the answer list of an error, empty if there is none
 */
static ocs::tom::ListPtr<ocs::tom::AN>
sge_parse_execd(const ocs::tom::List<ocs::tom::SPA> ppcmdline, uint32_t *help) {
   DENTER(TOP_LAYER);

   // filled through an lList ** by the parsing functions
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   int usageshowed = 0;

   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */
   while (!ppcmdline.empty()) {
      /* -help */
      if(parse_flag(ppcmdline, "-help", &alp, help)) {
         usageshowed = 1;
         sge_usage(EXECD, stdout);
         break;
      }
   }

   if (!ppcmdline.empty()) {
      if(!usageshowed) {
         sge_usage(EXECD, stderr);
      }
      answer_list_add(&alp, MSG_PARSE_TOOMANYARGS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(alp);
   }

   DRETURN(alp);
}

/* JG: TODO: we have this searching code in many places!! */
/**
 * @brief Search job and ja_task by id
 *
 * Searches the execd master lists for job and ja_task
 * defined by job_id and ja_task_id.
 *
 * @param job_id job id
 * @param ja_task_id ja_task id
 * @param[out] job returns job or an empty handle if not found; the job belongs to the job list
 * @param[out] ja_task returns ja_task or an empty handle if not found
 * @param ignore_missing_job_task do not log when the job or task is absent
 *
 * @return true if both job and ja_task are found, else false
 *
 * @note MT-NOTE: execd_get_job_ja_task() is MT safe
 */
bool execd_get_job_ja_task(const uint32_t job_id, const uint32_t ja_task_id, ocs::tom::Element<ocs::tom::JB> *job,
                           ocs::tom::Element<ocs::tom::JAT> *ja_task, const bool ignore_missing_job_task) {
   DENTER(TOP_LAYER);

   *job = nullptr;
   *ja_task = nullptr;

   /* in execd, we have exactly one ja_task per job,
    * therefore we can have multiple jobs with the same job_id
    */
   const auto job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
   for (const auto jep : job_list.find_all<JB_job_number>(job_id)) {
      *ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(jep, nullptr, ja_task_id));
      if (*ja_task) {
         *job = jep;
         DRETURN(true);
      }
   }

   // check if we should report an error in the message file
   // when cleanup of the active jobs directories is done then missing
   // references are expected (e.g. because KEEP_ACTIVE changed
   // as before, the job is empty once all jobs with the id have been looked at
   if (!ignore_missing_job_task) {
      if (!*job) {
         ERROR(MSG_JOB_TASKWITHOUTJOB_U, job_id);
      } else if (!*ja_task) {
         ERROR(MSG_JOB_TASKNOTASKINJOB_UU, job_id, ja_task_id);
      }
   }

   *job = nullptr;
   *ja_task = nullptr;
   DRETURN(false);
}
