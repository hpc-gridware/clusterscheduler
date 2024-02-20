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
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <ctime>
#include <netdb.h>
#include <fcntl.h>

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_answer.h"

#include "gdi/qm_name.h"
#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_ctx.h"
#include "gdi/msg_gdilib.h"

#include "comm/commlib.h"

#include "sge.h"
#include "sig_handlers.h"
#include "qmaster_heartbeat.h"
#include "lock.h"
#include "startprog.h"
#include "shutdown.h"
#include "msg_common.h"
#include "msg_shadowd.h"

#if defined(SOLARIS)
#   include "sge_smf.h"
#   include "sge_string.h"
#endif

#ifndef FALSE
#   define FALSE 0
#endif

#ifndef TRUE
#   define TRUE  1
#endif

#define CHECK_INTERVAL      60
#define GET_ACTIVE_INTERVAL 240
#define DELAY_TIME          600

int main(int argc, char **argv);

static void
shadowd_exit_func(int i);

static int
check_if_valid_shadow(char *binpath, char *oldqmaster, const char *act_qmaster_file, const char *shadow_master_file,
                      const char *qualified_hostname, const char *binary_path);

static int
compare_qmaster_names(const char *act_qmaster_file, const char *old_qmaster);

static int
host_in_file(const char *, const char *);

static int
parse_cmdline_shadowd(int argc, char **argv);

static int
shadowd_is_old_master_enrolled(int sge_test_heartbeat, int sge_qmaster_port, char *oldqmaster);

static int
shadowd_is_old_master_enrolled(int sge_test_heartbeat, int sge_qmaster_port, char *oldqmaster) {
   cl_com_handle_t *handle = nullptr;
   cl_com_SIRM_t *status = nullptr;
   int ret;
   int is_up_and_running = 0;
   int commlib_error = CL_RETVAL_OK;

   DENTER(TOP_LAYER);

   /*
    * This is for testsuite testing to simulate qmaster outage.
    * For testing the environment variable SGE_TEST_HEARTBEAT_TIMEOUT
    * has to be set!
    */
   if (sge_test_heartbeat > 0) {
      DRETURN(is_up_and_running);
   }

   handle = cl_com_create_handle(&commlib_error, CL_CT_TCP, CL_CM_CT_MESSAGE, false, sge_qmaster_port, CL_TCP_DEFAULT,
                                 (char *) prognames[SHADOWD], 0, 1, 0);
   if (handle == nullptr) {
      CRITICAL((SGE_EVENT, SFNMAX, cl_get_error_text(commlib_error)));
      DRETURN(is_up_and_running);
   }

   DPRINTF(("Try to send status information message to previous master host "SFQ" to port %ld\n", oldqmaster, sge_qmaster_port));
   ret = cl_commlib_get_endpoint_status(handle, oldqmaster, (char *) prognames[QMASTER], 1, &status);
   if (ret != CL_RETVAL_OK) {
      DPRINTF(("cl_commlib_get_endpoint_status() returned "SFQ"\n", cl_get_error_text(ret)));
      is_up_and_running = 0;
      DPRINTF(("old qmaster not responding - No master found\n"));
   } else {
      DPRINTF(("old qmaster is still running\n"));
      is_up_and_running = 1;
   }

   if (status != nullptr) {
      DPRINTF(("endpoint is up since %ld seconds and has status %ld\n", status->runtime, status->application_status));
      cl_com_free_sirm_message(&status);
   }

   cl_commlib_shutdown_handle(handle, false);

   DRETURN(is_up_and_running);
}

/*----------------------------------------------------------------------------*/
int
main(int argc, char **argv) {
   int heartbeat = 0;
   int last_heartbeat = 0;
   int latest_heartbeat = 0;
   int ret = 0;
   int delay = 0;
   time_t now, last;
/*    const char *cp; */
   char err_str[MAX_STRING_SIZE];
   char shadowd_pidfile[SGE_PATH_MAX];
   dstring ds;
   char buffer[256];
   pid_t shadowd_pid;

#if 1

   static int check_interval = CHECK_INTERVAL;
   static int get_active_interval = GET_ACTIVE_INTERVAL;
   static int delay_time = DELAY_TIME;
   static int sge_test_heartbeat = 0;

   char binpath[SGE_PATH_MAX];
   char oldqmaster[SGE_PATH_MAX];

   char shadow_err_file[SGE_PATH_MAX];
   char qmaster_out_file[SGE_PATH_MAX];

#endif

   lList *alp = nullptr;

   DENTER_MAIN(TOP_LAYER, "sge_shadowd");

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   /* initialize recovery control variables */
   {
      char *s;
      int val;
      if ((s = getenv("SGE_CHECK_INTERVAL")) &&
          sscanf(s, "%d", &val) == 1)
         check_interval = val;
      if ((s = getenv("SGE_GET_ACTIVE_INTERVAL")) &&
          sscanf(s, "%d", &val) == 1)
         get_active_interval = val;
      if ((s = getenv("SGE_DELAY_TIME")) &&
          sscanf(s, "%d", &val) == 1)
         delay_time = val;
      if ((s = getenv("SGE_TEST_HEARTBEAT_TIMEOUT")) &&
          sscanf(s, "%d", &val) == 1)
         sge_test_heartbeat = val;
   }

   /* This needs a better solution */
   umask(022);

#ifdef __SGE_COMPILE_WITH_GETTEXT__
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */

   log_state_set_log_file(TMP_ERR_FILE_SHADOWD);

   if (sge_setup2(SHADOWD, MAIN_THREAD, &alp, false) != AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   /* AA: TODO: change this */
   component_set_exit_func(shadowd_exit_func);
   sge_setup_sig_handlers(SHADOWD);

#if defined(SOLARIS)
   /* Init shared SMF libs if necessary */
   if (sge_smf_used() == 1 && sge_smf_init_libs() != 0) {
       sge_exit(1);
   }
#endif

   if (bootstrap_get_qmaster_spool_dir() != nullptr) {
      char *shadowd_name = SGE_SHADOWD;

      /* is there a running shadowd on this host (with unqualified name) */
      sprintf(shadowd_pidfile, "%s/" SHADOWD_PID_FILE, bootstrap_get_qmaster_spool_dir(), component_get_unqualified_hostname());

      DPRINTF(("pidfilename: %s\n", shadowd_pidfile));
      if ((shadowd_pid = sge_readpid(shadowd_pidfile))) {
         DPRINTF(("shadowd_pid: "sge_U32CFormat"\n", sge_u32c(shadowd_pid)));
         if (!sge_checkprog(shadowd_pid, shadowd_name, PSCMD)) {
            CRITICAL((SGE_EVENT, MSG_SHADOWD_FOUNDRUNNINGSHADOWDWITHPIDXNOTSTARTING_I, (int) shadowd_pid));
            sge_exit(1);
         }
      }

      sge_gdi_ctx_class_prepare_enroll();

      /* is there a running shadowd on this host (with aliased name) */
      sprintf(shadowd_pidfile, "%s/" SHADOWD_PID_FILE, bootstrap_get_qmaster_spool_dir(), component_get_qualified_hostname());
      DPRINTF(("pidfilename: %s\n", shadowd_pidfile));
      if ((shadowd_pid = sge_readpid(shadowd_pidfile))) {
         DPRINTF(("shadowd_pid: "sge_U32CFormat"\n", sge_u32c(shadowd_pid)));
         if (!sge_checkprog(shadowd_pid, shadowd_name, PSCMD)) {
            CRITICAL((SGE_EVENT, MSG_SHADOWD_FOUNDRUNNINGSHADOWDWITHPIDXNOTSTARTING_I, (int) shadowd_pid));
            sge_exit(1);
         }
      }
   } else {
      sge_gdi_ctx_class_prepare_enroll();
   }

   if (parse_cmdline_shadowd(argc, argv) == 1) {
      sge_exit(0);
   }

   if (bootstrap_get_qmaster_spool_dir() == nullptr) {
      CRITICAL((SGE_EVENT, MSG_SHADOWD_CANTREADQMASTERSPOOLDIRFROMX_S, bootstrap_get_bootstrap_file()));
      sge_exit(1);
   }

   if (chdir(bootstrap_get_qmaster_spool_dir())) {
      CRITICAL((SGE_EVENT, MSG_SHADOWD_CANTCHANGETOQMASTERSPOOLDIRX_S, bootstrap_get_qmaster_spool_dir()));
      sge_exit(1);
   }

   if (sge_set_admin_username(bootstrap_get_admin_user(), err_str)) {
      CRITICAL((SGE_EVENT, SFNMAX, err_str));
      sge_exit(1);
   }

   if (sge_switch2admin_user()) {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_SHADOWD_CANTSWITCHTOADMIN_USER));
      sge_exit(1);
   }

   sprintf(shadow_err_file, "messages_shadowd.%s", component_get_unqualified_hostname());
   sprintf(qmaster_out_file, "messages_qmaster.%s", component_get_unqualified_hostname());
   sge_copy_append(TMP_ERR_FILE_SHADOWD, shadow_err_file, SGE_MODE_APPEND);
   unlink(TMP_ERR_FILE_SHADOWD);
   log_state_set_log_as_admin_user(1);
   log_state_set_log_file(shadow_err_file);

   {
      int *tmp_fd_array = nullptr;
      unsigned long tmp_fd_count = 0;

      if (cl_com_set_handle_fds(cl_com_get_handle(prognames[SHADOWD], 0), &tmp_fd_array, &tmp_fd_count) ==
          CL_RETVAL_OK) {
         sge_daemonize(tmp_fd_array, tmp_fd_count);
         if (tmp_fd_array != nullptr) {
            sge_free(&tmp_fd_array);
         }
      } else {
         sge_daemonize(nullptr, 0);
      }
   }

   /* shadowd pid file will contain aliased name */
   sge_write_pid(shadowd_pidfile);

   starting_up();

   sge_setup_sig_handlers(SHADOWD);

   last_heartbeat = get_qmaster_heartbeat(QMASTER_HEARTBEAT_FILE, 30);

   last = (time_t) sge_get_gmt(); /* set time of last check time */

   delay = 0;
   while (!shut_me_down) {
      sleep(check_interval);

      /* get current heartbeat file content */
      heartbeat = get_qmaster_heartbeat(QMASTER_HEARTBEAT_FILE, 30);

      now = (time_t) sge_get_gmt();


      /* Only check when we could read the heartbeat file at least two times
       * (last_heartbeat and heartbeat) without error 
       */
      if (last_heartbeat > 0 && heartbeat > 0) {

         /*
          * OK we have to heartbeat entries to check. Check times ...
          * now  = current time
          * last = last check time
          */
         if ((now - last) >= (get_active_interval + delay)) {

            delay = 0;
            if (last_heartbeat == heartbeat) {
               DPRINTF(("heartbeat not changed since seconds: "sge_U32CFormat"\n", sge_u32c(now - last)));
               delay = delay_time; /* set delay time */

               /*
                * check if we are a possible new qmaster host (lock file of qmaster active, etc.)
                */
               ret = check_if_valid_shadow(binpath, oldqmaster,
                                           bootstrap_get_act_qmaster_file(),
                                           bootstrap_get_shadow_masters_file(),
                                           component_get_qualified_hostname(),
                                           bootstrap_get_binary_path());

               if (ret == 0) {
                  /* we can start a qmaster on this host */
                  if (qmaster_lock(QMASTER_LOCK_FILE)) {
                     ERROR((SGE_EVENT, SFNMAX, MSG_SHADOWD_FAILEDTOLOCKQMASTERSOMBODYWASFASTER));
                  } else {
                     int out, err;

                     /* still the old qmaster name in act_qmaster file and still the old heartbeat */
                     latest_heartbeat = get_qmaster_heartbeat(QMASTER_HEARTBEAT_FILE, 30);
                     /* TODO: what do we when there is a timeout ??? */
                     DPRINTF(("old qmaster name in act_qmaster and old heartbeat\n"));
                     if (!compare_qmaster_names(bootstrap_get_act_qmaster_file(), oldqmaster) &&
                         !shadowd_is_old_master_enrolled(sge_test_heartbeat, sge_get_qmaster_port(nullptr), oldqmaster) &&
                         (latest_heartbeat == heartbeat)) {
                        char qmaster_name[256];

                        strcpy(qmaster_name, SGE_PREFIX);
                        strcat(qmaster_name, prognames[QMASTER]);
                        DPRINTF(("qmaster_name: "SFN"\n", qmaster_name));

                        /*
                         * open logfile as admin user for initial qmaster/schedd 
                         * startup messages
                         */
                        out = SGE_OPEN3(qmaster_out_file, O_CREAT | O_WRONLY | O_APPEND,
                                        0644);
                        err = out;
                        if (out == -1) {
                           /*
                            * First priority is the master restart
                            * => ignore this error
                            */
                           out = 1;
                           err = 2;
                        }

                        sge_switch2start_user();
                        ret = startprog(out, err, nullptr, binpath, qmaster_name, nullptr);
                        sge_switch2admin_user();
                        if (ret) {
                           ERROR((SGE_EVENT, SFNMAX, MSG_SHADOWD_CANTSTARTQMASTER));
                        }
                        close(out);
                     } else {
                        qmaster_unlock(QMASTER_LOCK_FILE);
                     }
                  }
               } else {
                  if (ret == -1) {
                     /* just log the more important failures */
                     WARNING((SGE_EVENT, MSG_SHADOWD_DELAYINGSHADOWFUNCFORXSECONDS_U, sge_u32c(delay)));
                  }
               }
            }
            /* Begin a new interval, set timers and hearbeat to current values */
            last = now;
            last_heartbeat = heartbeat;
         }
      } else {
         if (last_heartbeat < 0 || heartbeat < 0) {
            /* There was an error reading heartbeat or last_heartbeat */
            DPRINTF(("can't read heartbeat file. last_heartbeat="sge_U32CFormat", heartbeat="sge_U32CFormat"\n",
                    sge_u32c(last_heartbeat), sge_u32c(heartbeat)));
         } else {
            DPRINTF(("have to read the heartbeat file twice to check time differences\n"));
         }
      }
   }

   sge_shutdown(0);

   DRETURN(EXIT_SUCCESS);
}

/*-----------------------------------------------------------------
 * shadowd_exit_func
 * function installed to be called just before exit() is called.
 *-----------------------------------------------------------------*/
static void
shadowd_exit_func(int i) {
#if defined(SOLARIS)
   if (sge_smf_used() == 1) {
      /* We don't do disable on svcadm restart */
      if (sge_strnullcmp(sge_smf_get_instance_state(), SCF_STATE_STRING_ONLINE) == 0 &&
          sge_strnullcmp(sge_smf_get_instance_next_state(), SCF_STATE_STRING_NONE) == 0) {      
         sge_smf_temporary_disable_instance();
      }
   }
#endif
   exit(i);
}

/*-----------------------------------------------------------------
 * compare_qmaster_names
 * see if old qmaster name and current qmaster name are still the same
 *-----------------------------------------------------------------*/
static int
compare_qmaster_names(const char *act_qmaster_file, const char *oldqmaster) {
   char newqmaster[SGE_PATH_MAX];
   int ret;

   DENTER(TOP_LAYER);

   if (get_qm_name(newqmaster, act_qmaster_file, nullptr)) {
      WARNING((SGE_EVENT, MSG_SHADOWD_CANTREADACTQMASTERFILEX_S, act_qmaster_file));
      DRETURN(-1);
   }

   ret = sge_hostcmp(newqmaster, oldqmaster);

   DPRINTF(("strcmp() of old and new qmaster returns: "sge_U32CFormat"\n", sge_u32c(ret)));

   DRETURN(ret);
}

/*-----------------------------------------------------------------
 * check_if_valid_shadow
 * return 0 if we are a valid shadow
 *        -1 if not
 *        -2 if lock file exits or master was running on same machine
 *-----------------------------------------------------------------*/
static int
check_if_valid_shadow(char *binpath, char *oldqmaster, const char *act_qmaster_file, const char *shadow_master_file,
                      const char *qualified_hostname, const char *binary_path) {
   struct hostent *hp;

   DENTER(TOP_LAYER);

   if (isLocked(QMASTER_LOCK_FILE)) {
      DPRINTF(("lock file exits\n"));
      DRETURN(-2);
   }

   /* we can't read act_qmaster file */
   if (get_qm_name(oldqmaster, act_qmaster_file, nullptr)) {
      WARNING((SGE_EVENT, MSG_SHADOWD_CANTREADACTQMASTERFILEX_S, act_qmaster_file));
      DRETURN(-1);
   }

   /* we can't resolve hostname of old qmaster */
   hp = sge_gethostbyname_retry(oldqmaster);
   if (hp == (struct hostent *) nullptr) {
      WARNING((SGE_EVENT, MSG_SHADOWD_CANTRESOLVEHOSTNAMEFROMACTQMASTERFILE_SS,
              act_qmaster_file, oldqmaster));
      DRETURN(-1);
   }

   /* we are on the same machine as old qmaster */
   if (!strcmp(hp->h_name, qualified_hostname)) {
      sge_free_hostent(&hp);
      DPRINTF(("qmaster was running on same machine\n"));
      DRETURN(-2);
   }

   sge_free_hostent(&hp);


   /* we are not in the shadow master file */
   if (host_in_file(qualified_hostname, shadow_master_file)) {
      WARNING((SGE_EVENT, MSG_SHADOWD_NOTASHADOWMASTERFILE_S, shadow_master_file));
      DRETURN(-1);
   }

   sge_strlcpy(binpath, binary_path, SGE_PATH_MAX); /* copy global configuration path */
   DPRINTF((""SFQ"\n", binpath));
   DPRINTF(("we are a candidate for shadow master\n"));

   DRETURN(0);
}

/*----------------------------------------------------------------------
 * host_in_file
 * look if resolved host is in "file"
 * return  
 *         0 if present 
 *         1 if not
 *        -1 error occured
 *----------------------------------------------------------------------*/
static int
host_in_file(const char *host, const char *file) {
   FILE *fp;
   char buf[512], *cp;

   DENTER(TOP_LAYER);

   fp = fopen(file, "r");
   if (!fp) {
      DRETURN(-1);
   }

   while (fgets(buf, sizeof(buf), fp)) {
      for (cp = strtok(buf, " \t\n,"); cp; cp = strtok(nullptr, " \t\n,")) {
         char *resolved_host = nullptr;
         cl_com_cached_gethostbyname(cp, &resolved_host, nullptr, nullptr, nullptr);
         if (resolved_host) {
            if (!sge_hostcmp(host, resolved_host)) {
               FCLOSE(fp);
               sge_free(&resolved_host);
               DRETURN(0);
            }
            sge_free(&resolved_host);
         }
      }
   }

   FCLOSE(fp);
   DRETURN(1);

   FCLOSE_ERROR:
DRETURN(0);
}

/*---------------------------------------------------------------------
 * parse_cmdline_shadowd
 *---------------------------------------------------------------------*/
static int
parse_cmdline_shadowd(int argc, char **argv) {
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER);

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   /*
   ** -help
   */
   if ((argc == 2) && !strcmp(argv[1], "-help")) {
#define PRINTITD(o, d) print_option_syntax(stdout,o,d)

      fprintf(stdout, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

      fprintf(stdout, "%s sge_shadowd [options]\n", MSG_GDI_USAGE_USAGESTRING);

      PRINTITD(MSG_GDI_USAGE_help_OPT, MSG_GDI_UTEXT_help_OPT);
      DRETURN(1);
   }

   DRETURN(0);
}
