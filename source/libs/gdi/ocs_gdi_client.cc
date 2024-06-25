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

#include <cstdlib>
#include <cstdio>
#include <cstring>

#if defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#  include <termio.h>
#endif

#include "comm/commlib.h"

#include "uti/msg_utilib.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_csp_path.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_data.h"
#include "gdi/msg_gdilib.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"

static int
sge_gdi_ctx_log_flush_func(cl_raw_list_t *list_p) {
   int ret_val;

   DENTER(COMMD_LAYER);

   if (list_p == nullptr) {
      DRETURN(CL_RETVAL_LOG_NO_LOGLIST);
   }

   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      DRETURN(ret_val);
   }

   cl_log_list_elem_t *elem = cl_log_list_get_first_elem(list_p);
   while (elem != nullptr) {
      const char *param;
      if (elem->log_parameter == nullptr) {
         param = "";
      } else {
         param = elem->log_parameter;
      }

      switch (elem->log_type) {
         case CL_LOG_ERROR:
            if (log_state_get_log_level() >= LOG_ERR) {
               ERROR("%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_WARNING:
            if (log_state_get_log_level() >= LOG_WARNING) {
               WARNING("%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_INFO:
            if (log_state_get_log_level() >= LOG_INFO) {
               INFO("%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_DEBUG:
            if (log_state_get_log_level() >= LOG_DEBUG) {
               DEBUG("%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_OFF:
            break;
      }
      cl_log_list_del_log(list_p);
      elem = cl_log_list_get_first_elem(list_p);
   }

   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      DRETURN(ret_val);
   }
   DRETURN(CL_RETVAL_OK);
}

int gdi_client_prepare_enroll(lList **answer_list) {
   cl_host_resolve_method_t resolve_method = CL_SHORT;
   cl_framework_t communication_framework = CL_CT_TCP;
   const char *default_domain = nullptr;
   int cl_ret = CL_RETVAL_OK;

   DENTER(TOP_LAYER);

   /* context setup is complete => setup the commlib
   **
   ** there is a problem in qsub if it is used with CL_RW_THREAD, the signaling in qsub -sync
   ** makes qsub hang
   */

   if (!cl_com_setup_commlib_complete()) {
      char *env_sge_commlib_debug = getenv("SGE_DEBUG_LEVEL");
      switch (component_get_component_id()) {
         case QMASTER:
         case DRMAA:
         case SCHEDD:
         case EXECD:
            INFO(SFNMAX, MSG_GDI_MULTI_THREADED_STARTUP);
            /* if SGE_DEBUG_LEVEL environment is set we use gdi log flush function */
            /* you can set commlib debug level with env SGE_COMMLIB_DEBUG */
            if (env_sge_commlib_debug != nullptr) {
               cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, sge_gdi_ctx_log_flush_func);
            } else {
               /* here we use default commlib flush function */
               cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, nullptr);
            }
            break;
         default:
            INFO(SFNMAX, MSG_GDI_SINGLE_THREADED_STARTUP);
            if (env_sge_commlib_debug != nullptr) {
               cl_ret = cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, sge_gdi_ctx_log_flush_func);
            } else {
               cl_ret = cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, nullptr);
            }
            /*
            ** verbose logging is switched on by default
            */
            log_state_set_log_verbose(1);
      }

      if (cl_ret != CL_RETVAL_OK) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 "cl_com_setup_commlib failed: %s", cl_get_error_text(cl_ret));
         DRETURN(cl_ret);
      }
   }

   /* set the alias file */
   cl_ret = cl_com_set_alias_file(bootstrap_get_alias_file());
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_com_set_alias_file failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /* setup the resolve method */

   if (!bootstrap_get_ignore_fqdn()) {
      resolve_method = CL_LONG;
   }
   const char *help = bootstrap_get_default_domain();
   if (help != nullptr) {
      if (SGE_STRCASECMP(help, NONE_STR) != 0) {
         default_domain = help;
      }
   }

   cl_ret = cl_com_set_resolve_method(resolve_method, (char *) default_domain);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_com_set_resolve_method failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /*
   ** reresolve qualified hostname with use of host aliases 
   ** (corresponds to reresolve_me_qualified_hostname)
   */
   cl_ret = reresolve_qualified_hostname();
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING,
                              "reresolve hostname failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   cl_ret = cl_com_set_error_func(general_communication_error);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_com_set_error_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   cl_ret = cl_com_set_tag_name_func(sge_dump_message_tag);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_com_set_tag_name_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   if (handle == nullptr) {
      /* handle does not exist, create one */

      int me_who = component_get_component_id();
      const char *progname = component_get_component_name();
      const char *master = gdi_get_act_master_host(true);
      const char *qualified_hostname = component_get_qualified_hostname();
      u_long32 sge_qmaster_port = bootstrap_get_sge_qmaster_port();
      u_long32 sge_execd_port = bootstrap_get_sge_execd_port();
      int my_component_id = 0; /* 1 for daemons, 0=automatical for clients */

      if (master == nullptr && !(me_who == QMASTER)) {
         DRETURN(CL_RETVAL_UNKNOWN);
      }

      /*
      ** CSP initialize
      */
      if (strcasecmp(bootstrap_get_security_mode(), "csp") == 0) {
         cl_ssl_setup_t *sec_ssl_setup_config = nullptr;
         cl_ssl_cert_mode_t ssl_cert_mode = CL_SSL_PEM_FILE;
         sge_csp_path_class_t *sge_csp = gdi_data_get_csp_path_obj();

         if (gdi_data_get_ssl_certificate() != nullptr) {
            ssl_cert_mode = CL_SSL_PEM_BYTE;
            sge_csp->set_cert_file(sge_csp, gdi_data_get_ssl_certificate());
            sge_csp->set_key_file(sge_csp, gdi_data_get_ssl_private_key());
         }
         sge_csp->dprintf(sge_csp);

         communication_framework = CL_CT_SSL;
         cl_ret = cl_com_create_ssl_setup(&sec_ssl_setup_config,
                                          ssl_cert_mode,
                                          CL_SSL_v23,                                   /* ssl_method           */
                                          (char *) sge_csp->get_CA_cert_file(sge_csp),    /* ssl_CA_cert_pem_file */
                                          (char *) sge_csp->get_CA_key_file(sge_csp),     /* ssl_CA_key_pem_file  */
                                          (char *) sge_csp->get_cert_file(sge_csp),       /* ssl_cert_pem_file    */
                                          (char *) sge_csp->get_key_file(sge_csp),        /* ssl_key_pem_file     */
                                          (char *) sge_csp->get_rand_file(sge_csp),       /* ssl_rand_file        */
                                          (char *) sge_csp->get_reconnect_file(sge_csp),  /* ssl_reconnect_file   */
                                          (char *) sge_csp->get_crl_file(sge_csp),        /* ssl_crl_file         */
                                          sge_csp->get_refresh_time(sge_csp),           /* ssl_refresh_time     */
                                          (char *) sge_csp->get_password(sge_csp),        /* ssl_password         */
                                          sge_csp->get_verify_func(
                                                  sge_csp));           /* ssl_verify_func (cl_ssl_verify_func_t)  */
         if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
            DPRINTF("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret));
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, qualified_hostname, progname,
                                    0, sge_qmaster_port, cl_get_error_text(cl_ret));
            DRETURN(cl_ret);
         }

         /*
         ** set the CSP credential info into commlib
         */
         cl_ret = cl_com_specify_ssl_configuration(sec_ssl_setup_config);
         if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
            DPRINTF("return value of cl_com_specify_ssl_configuration(): %s\n", cl_get_error_text(cl_ret));
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, component_get_component_name(),
                                    0, sge_qmaster_port, cl_get_error_text(cl_ret));
            cl_com_free_ssl_setup(&sec_ssl_setup_config);
            DRETURN(cl_ret);
         }
         cl_com_free_ssl_setup(&sec_ssl_setup_config);
      }

      if (me_who == QMASTER || me_who == EXECD || me_who == SCHEDD || me_who == SHADOWD) {
         my_component_id = 1;
      }

      switch (me_who) {
         case EXECD:
            /* add qmaster as known endpoint */
            DPRINTF("re-read actual qmaster file (prepare_enroll)\n");
            cl_com_append_known_endpoint_from_name((char *) master,
                                                   (char *) prognames[QMASTER],
                                                   1,
                                                   (int)sge_qmaster_port,
                                                   CL_CM_AC_DISABLED,
                                                   true);
            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE,
                                          true,
                                          (int)sge_execd_port,
                                          CL_TCP_DEFAULT,
                                          (char *) component_get_component_name(),
                                          my_component_id,
                                          1,
                                          0);
            cl_com_set_auto_close_mode(handle, CL_CM_AC_ENABLED);
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
                  ERROR(MSG_GDI_CANT_GET_COM_HANDLE_SSUUS, qualified_hostname, component_get_component_name(), sge_u32c(my_component_id), sge_u32c(sge_execd_port), cl_get_error_text(cl_ret));
               }
            }
            break;

         case QMASTER:
            DPRINTF("creating QMASTER handle\n");
            cl_com_append_known_endpoint_from_name((char *) master,
                                                   (char *) prognames[QMASTER],
                                                   1,
                                                   (int)sge_qmaster_port,
                                                   CL_CM_AC_DISABLED,
                                                   true);

            /* do a later qmaster commlib listen before creating qmaster handle */
            /* CL_COMMLIB_DELAYED_LISTEN is set to false, because
                     enabling it might cause problems with current shadowd and
                     startup qmaster implementation */
            cl_commlib_set_global_param(CL_COMMLIB_DELAYED_LISTEN, false);

            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE, /* message based tcp communication */
                                          true,
                                          (int)sge_qmaster_port, /* create service on qmaster port */
                                          CL_TCP_DEFAULT,   /* use standard connect mode */
                                          (char *) component_get_component_name(),
                                          my_component_id,  /* this endpoint is called "qmaster" 
                                                               and has id 1 */
                                          1,
                                          0); /* select timeout is set to 1 second 0 usec */
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
                  ERROR(MSG_GDI_CANT_GET_COM_HANDLE_SSUUS, qualified_hostname, component_get_component_name(), sge_u32c(my_component_id), sge_u32c(sge_qmaster_port), cl_get_error_text(cl_ret));
               }
            } else {
               char act_resolved_qmaster_name[CL_MAXHOSTLEN];
               cl_com_set_synchron_receive_timeout(handle, 5);
               master = gdi_get_act_master_host(true);

               if (master != nullptr) {
                  /* check a running qmaster on different host */
                  if (getuniquehostname(master, act_resolved_qmaster_name, 0) == CL_RETVAL_OK &&
                      sge_hostcmp(act_resolved_qmaster_name, qualified_hostname) != 0) {

                     DPRINTF("act_qmaster file contains host " SFQ " which doesn't match local host name " SFQ "\n",
                             master, qualified_hostname);

                     cl_com_set_error_func(nullptr);

                     int alive_back = gdi_is_alive(answer_list);

                     cl_ret = cl_com_set_error_func(general_communication_error);
                     if (cl_ret != CL_RETVAL_OK) {
                        ERROR(SFNMAX, cl_get_error_text(cl_ret));
                     }

                     if (alive_back == CL_RETVAL_OK && getenv("SGE_TEST_HEARTBEAT_TIMEOUT") == nullptr) {
                        CRITICAL(MSG_GDI_MASTER_ON_HOST_X_RUNINNG_TERMINATE_S, master);
                        /* TODO: remove !!! */
                        sge_exit(1);
                     } else {
                        DPRINTF("qmaster on host " SFQ " is down\n", master);
                     }
                  } else {
                     DPRINTF("act_qmaster file contains local host name\n");
                  }
               } else {
                  DPRINTF("skipping qmaster alive check because act_qmaster is not availabe\n");
               }
            }
            break;

         default:
            /* this is for "normal" gdi clients of qmaster */
            DPRINTF("creating %s GDI handle\n", component_get_component_name());
            handle = cl_com_create_handle(&cl_ret, communication_framework, CL_CM_CT_MESSAGE, false,
                                          (int)sge_qmaster_port, CL_TCP_DEFAULT,
                                          (char *) component_get_component_name(), my_component_id, 1, 0);
            if (handle == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, component_get_qualified_hostname(),
                                       component_get_component_name(), sge_u32c(my_component_id),
                                       sge_u32c(sge_qmaster_port), cl_get_error_text(cl_ret));
            }
            break;
      }
      gdi_data_set_last_commlib_error(cl_ret);
   }

   if ((component_get_component_id() == QMASTER) && (getenv("SGE_TEST_SOCKET_BIND") != nullptr)) {
      /* this is for testsuite socket bind test (issue 1096 ) */
      struct timeval now{};
      gettimeofday(&now, nullptr);

      /* if this environment variable is set, we wait 15 seconds after
         communication lib setup */
      DPRINTF("waiting for 60 seconds, because environment SGE_TEST_SOCKET_BIND is set\n");
      while (handle != nullptr && now.tv_sec - handle->start_time.tv_sec <= 60) {
         DPRINTF("timeout: " sge_U32CFormat "\n", sge_u32c(now.tv_sec - handle->start_time.tv_sec));
         cl_commlib_trigger(handle, 1);
         gettimeofday(&now, nullptr);
      }
      DPRINTF("continue with setup\n");
   }
   DRETURN(cl_ret);
}

// * setup gdi for an application or thread
// * does not enroll. commlib setup has to be done separately or gdi_client_setup_and_enroll()
int
gdi_client_setup(int component_id, u_long32 thread_id, lList **answer_list, bool is_qmaster_intern_client) {
   DENTER(TOP_LAYER);

   lInit(nmv);
   component_set_component_id(component_id);
   component_set_qmaster_internal(is_qmaster_intern_client);
   component_set_thread_name(threadnames[thread_id] ? threadnames[thread_id] : prognames[component_id]);

   // TODO: EB: this can be taken from boostrap module instead of accessing the environment again.
   const char *sge_root = getenv("SGE_ROOT");
   if (sge_root == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SGEROOTNOTSET);
      DRETURN(AE_ERROR);
   }

   char user[128] = "";
   if (sge_uid2user(geteuid(), user, sizeof(user), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEUSER);
      DRETURN(AE_ERROR);
   }

   char group[128] = "";
   if (sge_gid2group(getegid(), group, sizeof(group), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEGROUP);
      DRETURN(AE_ERROR);
   }

   if (feature_initialize_from_string(bootstrap_get_security_mode(), answer_list)) {
      // answer was set by feature_initialize_from_string()
      DRETURN(AE_ERROR);
   }

   gdi_data_set_csp_path_obj(sge_csp_path_class_create(gdi_data_get_error_handle()));
   if (!gdi_data_get_csp_path_obj()) {
      // EB: TODO: I18N + replace by an end user error message
      CRITICAL("sge_csp_path_class_create() failed");
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL);
      DRETURN(AE_ERROR);
   }

   component_set_exit_func(gdi_default_exit_func);

   DRETURN(AE_OK);
}

int
gdi_client_setup_and_enroll(int component_id, u_long32 thread_id, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (gdi_data_is_setup()) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_WARNING, MSG_GDI_GDI_ALREADY_SETUP);
      DRETURN(AE_ALREADY_SETUP);
   }

   int ret = gdi_client_setup(component_id, thread_id, answer_list, false);
   if (ret != AE_OK) {
      DRETURN(ret);
   }

   if (gdi_client_prepare_enroll(answer_list) != CL_RETVAL_OK) {
      DRETURN(AE_QMASTER_DOWN);
   }

   gdi_data_set_setup(true);

   DRETURN(AE_OK);
}

/****** gdi/setup/sge_gdi_shutdown() ******************************************
*  NAME
*     sge_gdi_shutdown() -- gdi shutdown.
*
*  SYNOPSIS
*     int sge_gdi_shutdown()
*
*  FUNCTION
*     This function has to be called before quitting the program. It
*     cancels registration at commd.
*
*  NOTES
*     MT-NOTES: gdi_client_setup_and_enroll() is MT safe
******************************************************************************/
int gdi_client_shutdown() {
   DENTER(GDI_LAYER);
   gdi_default_exit_func(0);
   DRETURN(0);
}
