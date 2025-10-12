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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
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

#include "uti/ocs_Munge.h"
#include "uti/msg_utilib.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_csp_path.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_security.h"
#include "uti/sge_string.h"
#include "uti/sge_stdio.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/sge_time.h"
#include "uti/sge_mtutil.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_ClientServerBase.h"
#include "gdi/sge_gdi_data.h"
#include "gdi/msg_gdilib.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"

#include "msg_common.h"

static void gdi_default_exit_func(int i) {
   sge_security_exit(i);
   cl_com_cleanup_commlib();
   component_ts0_destroy();
}

/************* COMMLIB HANDLERS from sge_any_request ************************/

/* setup a communication error callback and mutex for it */
static pthread_mutex_t general_communication_error_mutex = PTHREAD_MUTEX_INITIALIZER;


/* local static struct to store communication errors. The boolean
 * values com_access_denied and com_endpoint_not_unique will never be
 * restored to false again
 */
typedef struct sge_gdi_com_error_type {
   int com_error;                        /* current commlib error */
   bool com_was_error;                    /* set if there was an communication error (but not CL_RETVAL_ACCESS_DENIED or CL_RETVAL_ENDPOINT_NOT_UNIQUE)*/
   int com_last_error;                   /* last logged commlib error */
   bool com_access_denied;                /* set when commlib reports CL_RETVAL_ACCESS_DENIED */
   int com_access_denied_counter;        /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   time_t com_access_denied_time;         /* timeout for counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   bool com_endpoint_not_unique;          /* set when commlib reports CL_RETVAL_ENDPOINT_NOT_UNIQUE */
   int com_endpoint_not_unique_counter;  /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   time_t com_endpoint_not_unique_time;   /* timeout for counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
} sge_gdi_com_error_t;

static sge_gdi_com_error_t sge_gdi_communication_error = {CL_RETVAL_OK,
                                                          false,
                                                          CL_RETVAL_OK,
                                                          false, 0, 0,
                                                          false, 0, 0};

/****** sge_any_request/general_communication_error() **************************
*  NAME
*     general_communication_error() -- callback for communication errors
*
*  SYNOPSIS
*     static void general_communication_error(int cl_error,
*                                             const char* error_message)
*
*  FUNCTION
*     This function is used by cl_com_set_error_func() to set the default
*     application error function for communication errors. On important
*     communication errors the communication lib will call this function
*     with a corresponding error number (within application context).
*
*     This function should never block. Treat it as a kind of signal handler.
*
*     The error_message parameter is freed by the commlib.
*
*  INPUTS
*     int cl_error              - commlib error number
*     const char* error_message - additional error text message
*
*  NOTES
*     MT-NOTE: general_communication_error() is MT safe
*     (static struct variable "sge_gdi_communication_error" is used)
*
*
*  SEE ALSO
*     sge_any_request/sge_get_com_error_flag()
*******************************************************************************/
void
ocs::gdi::ClientBase::general_communication_error(const cl_application_error_list_elem_t *commlib_error) {
   DENTER(GDI_LAYER);
   if (commlib_error != nullptr) {
      struct timeval now{};
      unsigned long time_diff;

      sge_mutex_lock("general_communication_error_mutex",
                     __func__, __LINE__, &general_communication_error_mutex);

      /* save the communication error to react later */
      sge_gdi_communication_error.com_error = commlib_error->cl_error;

      switch (commlib_error->cl_error) {
         case CL_RETVAL_OK: {
            break;
         }
         case CL_RETVAL_ACCESS_DENIED: {
            if (!sge_gdi_communication_error.com_access_denied) {
               /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
               /* increment counter only once per second and allow max CL_DEFINE_READ_TIMEOUT + 2 access denied */
               gettimeofday(&now, nullptr);
               if ((now.tv_sec - sge_gdi_communication_error.com_access_denied_time) > (3 * CL_DEFINE_READ_TIMEOUT)) {
                  sge_gdi_communication_error.com_access_denied_time = 0;
                  sge_gdi_communication_error.com_access_denied_counter = 0;
               }

               if (sge_gdi_communication_error.com_access_denied_time < now.tv_sec) {
                  if (sge_gdi_communication_error.com_access_denied_time == 0) {
                     time_diff = 1;
                  } else {
                     time_diff = now.tv_sec - sge_gdi_communication_error.com_access_denied_time;
                  }
                  sge_gdi_communication_error.com_access_denied_counter += time_diff;
                  if (sge_gdi_communication_error.com_access_denied_counter > (2 * CL_DEFINE_READ_TIMEOUT)) {
                     sge_gdi_communication_error.com_access_denied = true;
                  }
                  sge_gdi_communication_error.com_access_denied_time = now.tv_sec;
               }
            }
            break;
         }
         case CL_RETVAL_ENDPOINT_NOT_UNIQUE: {
            if (!sge_gdi_communication_error.com_endpoint_not_unique) {
               /* counts endpoint not unique errors (TODO: workaround for BT: 6350264, IZ: 1893) */
               /* increment counter only once per second and allow max CL_DEFINE_READ_TIMEOUT + 2 endpoint not unique */
               DPRINTF("got endpint not unique");
               gettimeofday(&now, nullptr);
               if ((now.tv_sec - sge_gdi_communication_error.com_endpoint_not_unique_time) >
                   (3 * CL_DEFINE_READ_TIMEOUT)) {
                  sge_gdi_communication_error.com_endpoint_not_unique_time = 0;
                  sge_gdi_communication_error.com_endpoint_not_unique_counter = 0;
               }

               if (sge_gdi_communication_error.com_endpoint_not_unique_time < now.tv_sec) {
                  if (sge_gdi_communication_error.com_endpoint_not_unique_time == 0) {
                     time_diff = 1;
                  } else {
                     time_diff = now.tv_sec - sge_gdi_communication_error.com_endpoint_not_unique_time;
                  }
                  sge_gdi_communication_error.com_endpoint_not_unique_counter += time_diff;
                  if (sge_gdi_communication_error.com_endpoint_not_unique_counter > (2 * CL_DEFINE_READ_TIMEOUT)) {
                     sge_gdi_communication_error.com_endpoint_not_unique = true;
                  }
                  sge_gdi_communication_error.com_endpoint_not_unique_time = now.tv_sec;
               }
            }
            break;
         }
         default: {
            sge_gdi_communication_error.com_was_error = true;
            break;
         }
      }


      /*
       * now log the error if not already reported the
       * least CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT seconds
       */
      if (!commlib_error->cl_already_logged &&
          sge_gdi_communication_error.com_last_error != sge_gdi_communication_error.com_error) {

         /*  never log the same messages again and again (commlib
          *  will erase cl_already_logged flag every CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT
          *  seconds (30 seconds), so we have to save the last one!
          */
         sge_gdi_communication_error.com_last_error = sge_gdi_communication_error.com_error;

         switch (commlib_error->cl_err_type) {
            case CL_LOG_ERROR: {
               if (commlib_error->cl_info != nullptr) {
                  ERROR(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  ERROR(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_WARNING: {
               if (commlib_error->cl_info != nullptr) {
                  WARNING(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  WARNING(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_INFO: {
               if (commlib_error->cl_info != nullptr) {
                  INFO(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  INFO(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_DEBUG: {
               if (commlib_error->cl_info != nullptr) {
                  DEBUG(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  DEBUG(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_OFF: {
               break;
            }
         }
      }
      sge_mutex_unlock("general_communication_error_mutex",
                       __func__, __LINE__, &general_communication_error_mutex);
   }
   DRETURN_VOID;
}

int
ocs::gdi::ClientBase::log_flush_func(cl_raw_list_t *list_p) {
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

/****** sge_any_request/sge_get_com_error_flag() *******************************
*  NAME
*     sge_get_com_error_flag() -- return gdi error flag state
*
*  SYNOPSIS
*     bool sge_get_com_error_flag(sge_gdi_stored_com_error_t error_type)
*
*  FUNCTION
*     This function returns the error flag for the specified error type
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_stored_com_error_t error_type - error type value
*
*  RESULT
*     bool - true: error has occured, false: error never occured
*
*  NOTES
*     MT-NOTE: sge_get_com_error_flag() is MT safe
*
*  SEE ALSO
*     sge_any_request/general_communication_error()
*******************************************************************************/
bool ocs::gdi::ClientBase::sge_get_com_error_flag(u_long32 progid, sge_gdi_stored_com_error_t error_type, bool reset_error_flag) {
   DENTER(GDI_LAYER);
   bool ret_val = false;
   sge_mutex_lock("general_communication_error_mutex", __func__, __LINE__, &general_communication_error_mutex);

   /*
    * never add a default case for that switch, because of compiler warnings
    * for un-"cased" values
    */

   /* TODO: remove component_get_component_id()/progid cases for QMASTER and EXECD after BT: 6350264, IZ: 1893 is fixed */
   switch (error_type) {
      case SGE_COM_ACCESS_DENIED: {
         ret_val = sge_gdi_communication_error.com_access_denied;
         if (reset_error_flag) {
            sge_gdi_communication_error.com_access_denied = false;
         }
         break;
      }
      case SGE_COM_ENDPOINT_NOT_UNIQUE: {
         if (progid == QMASTER || progid == EXECD) {
            ret_val = false;
         } else {
            ret_val = sge_gdi_communication_error.com_endpoint_not_unique;
         }
         if (reset_error_flag) {
            sge_gdi_communication_error.com_endpoint_not_unique = false;
         }
         break;
      }
      case SGE_COM_WAS_COMMUNICATION_ERROR: {
         ret_val = sge_gdi_communication_error.com_was_error;
         if (reset_error_flag) {
            sge_gdi_communication_error.com_was_error = false;  /* reset error flag */
         }
      }
   }
   sge_mutex_unlock("general_communication_error_mutex", __func__, __LINE__, &general_communication_error_mutex);
   DRETURN(ret_val);
}

int ocs::gdi::ClientBase::prepare_enroll(lList **answer_list) {
   DENTER(TOP_LAYER);

   cl_host_resolve_method_t resolve_method = CL_SHORT;
   cl_framework_t communication_framework = CL_CT_TCP;
   const char *default_domain = nullptr;
   int me_who = component_get_component_id();
   int cl_ret = CL_RETVAL_OK;

   /* context setup is complete => setup the commlib
   **
   ** there is a problem in qsub if it is used with CL_RW_THREAD, the signaling in qsub -sync
   ** makes qsub hang
   */

   if (!cl_com_setup_commlib_complete()) {
      char *env_sge_commlib_debug = getenv("SGE_DEBUG_LEVEL");
      switch (me_who) {
         case QMASTER:
         case DRMAA:
         case SCHEDD:
         case EXECD:
            INFO(SFNMAX, MSG_GDI_MULTI_THREADED_STARTUP);
            /* if SGE_DEBUG_LEVEL environment is set we use gdi log flush function */
            /* you can set commlib debug level with env SGE_COMMLIB_DEBUG */
            if (env_sge_commlib_debug != nullptr) {
               cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, log_flush_func);
            } else {
               /* here we use default commlib flush function */
               cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, nullptr);
            }
            break;
         default:
            INFO(SFNMAX, MSG_GDI_SINGLE_THREADED_STARTUP);
            if (env_sge_commlib_debug != nullptr) {
               cl_ret = cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, log_flush_func);
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

   cl_ret = cl_com_set_tag_name_func(ocs::gdi::ClientServerBase::to_string);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_com_set_tag_name_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   if (bootstrap_has_security_mode(BS_SEC_MODE_MUNGE)) {
#if defined (OCS_WITH_MUNGE)
      if (!ocs::uti::Munge::is_initialized()) {
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (!ocs::uti::Munge::initialize(&error_dstr)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_GDI_MUNGE_INIT_FAILED_S, sge_dstring_get_string(&error_dstr));
            DRETURN(CL_RETVAL_UNKNOWN);
         }
      }
#else
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_GDI_BUILT_WITHOUT_MUNGE);
      DRETURN(CL_RETVAL_UNKNOWN);
#endif
   }

   bool is_server = me_who == QMASTER || me_who == EXECD;

   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   if (handle == nullptr) {
      /* handle does not exist, create one */
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
      if (bootstrap_has_security_mode(BS_SEC_MODE_CSP)) {
         communication_framework = CL_CT_SSL;
#ifdef SECURE
         cl_ssl_setup_t *sec_ssl_setup_config = nullptr;
         cl_ssl_cert_mode_t ssl_cert_mode = CL_SSL_PEM_FILE;
         sge_csp_path_class_t *sge_csp = gdi_data_get_csp_path_obj();

         if (gdi_data_get_ssl_certificate() != nullptr) {
            ssl_cert_mode = CL_SSL_PEM_BYTE;
            sge_csp->set_cert_file(sge_csp, gdi_data_get_ssl_certificate());
            sge_csp->set_key_file(sge_csp, gdi_data_get_ssl_private_key());
         }
         sge_csp->dprintf(sge_csp);

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
                                          sge_csp->get_verify_func(sge_csp));           /* ssl_verify_func (cl_ssl_verify_func_t)  */
         if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
            DPRINTF("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret));
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, qualified_hostname, component_get_component_name(),
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
#else
         // @todo ERROR
#endif
      }

      // new SSL encryption mode
      if (bootstrap_has_security_mode(BS_SEC_MODE_TLS)) {
         communication_framework = CL_CT_SSL_TLS;
#if defined(OCS_WITH_OPENSSL)
         // sge_qmaster is not really a client, BUT in case master (from act_qmaster file) != qualified_hostname
         // it checks if there is a qmaster running on the other host - that's what it is using the client connection for
         bool needs_client = me_who != QMASTER;
         cl_ret = gdi_setup_tls_config(needs_client, is_server, answer_list, qualified_hostname, master, sge_qmaster_port);
         if (cl_ret != CL_RETVAL_OK) {
            DRETURN(cl_ret);
         }
#else
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX, MSG_SSL_NOT_BUILT_IN);
#endif
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
                                          is_server,
                                          (int)sge_execd_port,
                                          CL_TCP_DEFAULT,
                                          (char *) component_get_component_name(),
                                          my_component_id,
                                          1,
                                          0);
            cl_com_set_auto_close_mode(handle, CL_CM_AC_ENABLED);
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
                  ERROR(MSG_GDI_CANT_GET_COM_HANDLE_SSUUS, qualified_hostname, component_get_component_name(), static_cast<u_long32>(my_component_id), sge_execd_port, cl_get_error_text(cl_ret));
               }
            }
            break;

         case QMASTER:
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
                                          is_server,
                                          (int)sge_qmaster_port, /* create service on qmaster port */
                                          CL_TCP_DEFAULT,   /* use standard connect mode */
                                          (char *) component_get_component_name(),
                                          my_component_id,  /* this endpoint is called "qmaster"
                                                               and has id 1 */
                                          1,
                                          0); /* select timeout is set to 1 second 0 usec */
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
                  ERROR(MSG_GDI_CANT_GET_COM_HANDLE_SSUUS, qualified_hostname, component_get_component_name(), static_cast<u_long32>(my_component_id), sge_qmaster_port, cl_get_error_text(cl_ret));
               }
            } else {
               char act_resolved_qmaster_name[CL_MAXHOSTNAMELEN];
               cl_com_set_synchron_receive_timeout(handle, 5);
               master = gdi_get_act_master_host(true);

               if (master != nullptr) {
                  /* check a running qmaster on different host */
                  if (getuniquehostname(master, act_resolved_qmaster_name, 0) == CL_RETVAL_OK &&
                      sge_hostcmp(act_resolved_qmaster_name, qualified_hostname) != 0) {

                     DPRINTF("act_qmaster file contains host " SFQ " which doesn't match local host name " SFQ "\n",
                             master, qualified_hostname);

                     cl_com_set_error_func(nullptr);

                     int alive_back = ocs::gdi::ClientBase::gdi_is_alive(answer_list);

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
            handle = cl_com_create_handle(&cl_ret, communication_framework, CL_CM_CT_MESSAGE, is_server,
                                          (int)sge_qmaster_port, CL_TCP_DEFAULT,
                                          (char *) component_get_component_name(), my_component_id, 1, 0);
            if (handle == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, component_get_qualified_hostname(),
                                       component_get_component_name(), static_cast<u_long32>(my_component_id),
                                       sge_qmaster_port, cl_get_error_text(cl_ret));
            }
            break;
      }
      gdi_data_set_last_commlib_error(cl_ret);
   }

   if (me_who == QMASTER && getenv("SGE_TEST_SOCKET_BIND") != nullptr) {
      /* this is for testsuite socket bind test (issue 1096 ) */
      struct timeval now{};
      gettimeofday(&now, nullptr);

      /* if this environment variable is set, we wait 15 seconds after
         communication lib setup */
      DPRINTF("waiting for 60 seconds, because environment SGE_TEST_SOCKET_BIND is set\n");
      while (handle != nullptr && now.tv_sec - handle->start_time.tv_sec <= 60) {
         DPRINTF("timeout: " sge_u32 "\n", static_cast<u_long32>(now.tv_sec - handle->start_time.tv_sec));
         cl_commlib_trigger(handle, 1);
         gettimeofday(&now, nullptr);
      }
      DPRINTF("continue with setup\n");
   }
   DRETURN(cl_ret);
}

// * setup gdi for an application or thread
// * does not enroll. commlib setup has to be done separately or ocs::gdi::ClientBase::setup_and_enroll()
ocs::gdi::ErrorValue
ocs::gdi::ClientBase::setup(int component_id, u_long32 thread_id, lList **answer_list, bool is_qmaster_intern_client) {
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

   gdi_data_set_csp_path_obj(sge_csp_path_class_create(gdi_data_get_error_handle()));
   if (!gdi_data_get_csp_path_obj()) {
      // EB: TODO: I18N + replace by an end user error message
      CRITICAL("sgdi_data_get_csp_path_obj() failed");
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL);
      DRETURN(AE_ERROR);
   }

   component_set_exit_func(gdi_default_exit_func);

   DRETURN(AE_OK);
}

ocs::gdi::ErrorValue
ocs::gdi::ClientBase::setup_and_enroll(int component_id, u_long32 thread_id, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (gdi_data_is_setup()) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_WARNING, MSG_GDI_GDI_ALREADY_SETUP);
      DRETURN(AE_ALREADY_SETUP);
   }

   ocs::gdi::ErrorValue ret = setup(component_id, thread_id, answer_list, false);
   if (ret != AE_OK) {
      DRETURN(ret);
   }

   if (ocs::gdi::ClientBase::prepare_enroll(answer_list) != CL_RETVAL_OK) {
      DRETURN(AE_QMASTER_DOWN);
   }

   gdi_data_set_setup(true);

   DRETURN(AE_OK);
}

/****** gdi/setup/sge_gdi_shutdown() ******************************************
*  NAME
*     ocs::gdi::Client::sge_gdi_shutdown() -- gdi shutdown.
*
*  SYNOPSIS
*     int ocs::gdi::Client::sge_gdi_shutdown()
*
*  FUNCTION
*     This function has to be called before quitting the program. It
*     cancels registration at commd.
*
*  NOTES
*     MT-NOTES: ocs::gdi::ClientBase::setup_and_enroll() is MT safe
******************************************************************************/
int ocs::gdi::ClientBase::shutdown() {
   DENTER(GDI_LAYER);
   gdi_default_exit_func(0);
   DRETURN(0);
}

/*-----------------------------------------------------------------------
 * Read name of qmaster from master_file
 * -> master_file
 * <- return -1  error in err_str
 *           0   host name of master in master_host
 *           don't copy error to err_str if err_str = nullptr
 *    master_file name of file which should point to act_qmaster file
 *    copy name of qmaster host to master_host
 *
 * NOTES
 *    MT-NOTE: get_qm_name() is MT safe
 *-----------------------------------------------------------------------*/
int
ocs::gdi::ClientBase::get_qm_name(char *master_host, const char *master_file, char *err_str, size_t err_str_size) {
   FILE *fp;
   char buf[CL_MAXHOSTNAMELEN * 3 + 1], *cp, *first;
   size_t len;

   DENTER(TOP_LAYER);

   if (!master_host || !master_file) {
      if (err_str) {
         if (master_host) {
            snprintf(err_str, err_str_size, SFNMAX, MSG_GDI_NULLPOINTERPASSED);
         }
      }
      DRETURN(-1);
   }

   if (!(fp = fopen(master_file, "r"))) {
      ERROR(MSG_GDI_FOPEN_FAILED, master_file, strerror(errno));
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_GDI_OPENMASTERFILEFAILED_S, master_file);
      }
      DRETURN(-1);
   }

   /* read file in one sweep and append O Byte to the end */
   if (!(len = fread(buf, 1, CL_MAXHOSTNAMELEN * 3, fp))) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_GDI_READMASTERHOSTNAMEFAILED_S, master_file);
      }
   }
   buf[len] = '\0';

   /* Skip white space including newlines */
   cp = buf;
   while (*cp && (*cp == ' ' || *cp == '\t' || *cp == '\n'))
      cp++;

   first = cp;

   /* read all non-white space characters */
   while (*cp && !(*cp == ' ' || *cp == '\t' || *cp == '\n')) {
      cp++;
   }

   *cp = '\0';
   len = cp - first;

   if (len == 0) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_GDI_MASTERHOSTNAMEHASZEROLENGTH_S, master_file);
      }
      FCLOSE(fp);
      DRETURN(-1);
   }

   if (len > CL_MAXHOSTNAMELEN - 1) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_GDI_MASTERHOSTNAMEEXCEEDSCHARS_SI, master_file, CL_MAXHOSTNAMELEN);
      }
      FCLOSE(fp);
      DRETURN(-1);
   }

   FCLOSE(fp);
   strcpy(master_host, first);
   DRETURN(0);
FCLOSE_ERROR:
   DRETURN(-1);
}

/*********************************************************************
 Write the actual qmaster into the master_file
 -> master_file and master_host
 <- return -1   error in err_str
            0   means OK

   NOTES
      MT-NOTE: write_qm_name() is MT safe
 *********************************************************************/
int
ocs::gdi::ClientBase::write_qm_name(const char *master_host, const char *master_file, char *err_str, size_t err_str_size) {
   FILE *fp;

   if (!(fp = fopen(master_file, "w"))) {
      if (err_str)
         snprintf(err_str, err_str_size,  MSG_GDI_OPENWRITEMASTERHOSTNAMEFAILED_SS, master_file, strerror(errno));
      return -1;
   }

   if (fprintf(fp, "%s\n", master_host) == EOF) {
      if (err_str)
         snprintf(err_str, err_str_size, MSG_GDI_WRITEMASTERHOSTNAMEFAILED_S, master_file);
      FCLOSE(fp);
      return -1;
   }

   FCLOSE(fp);
   return 0;
FCLOSE_ERROR:
   return -1;
}

const char *
ocs::gdi::ClientBase::gdi_get_act_master_host(bool reread) {
   DENTER(BASIS_LAYER);

   sge_error_class_t *eh = gdi_data_get_error_handle();
   static bool error_already_logged = false;

   const char *old_master_host = gdi_data_get_master_host();
   if (old_master_host == nullptr || reread) {
      char err_str[SGE_PATH_MAX + 128];
      char master_name[CL_MAXHOSTNAMELEN];
      u_long64 now = sge_get_gmt64();

      /* fix system clock moved back situation */
      if (gdi_data_get_timestamp_qmaster_file() > now) {
         gdi_data_set_timestamp_qmaster_file(0);
      }

      if (gdi_data_get_master_host() == nullptr || now - gdi_data_get_timestamp_qmaster_file() >= sge_gmt32_to_gmt64(30)) {
         /* re-read act qmaster file (max. every 30 seconds) */
         DPRINTF("re-read actual qmaster file\n");
         gdi_data_set_timestamp_qmaster_file(now);

         if (ocs::gdi::ClientBase::get_qm_name(master_name, bootstrap_get_act_qmaster_file(), err_str, sizeof(err_str)) == -1) {
            if (eh != nullptr && !error_already_logged) {
               eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_READMASTERNAMEFAILED_S, err_str);
               error_already_logged = true;
            }
            DRETURN(nullptr);
         }
         error_already_logged = false;
         DPRINTF("(re-)read act_qmaster file. Old master host: " SFQ ". New master host " SFQ "\n",
            old_master_host != nullptr ? old_master_host : "<nullptr>", master_name);
         /*
         ** TODO: thread locking needed here ?
         */
         gdi_set_master_host(master_name);

         // Update the client certificate if the qmaster host name changed.
         // Old_master_host is nullptr in the first call, here nothing is to be done.
         if (bootstrap_has_security_mode(BS_SEC_MODE_TLS)) {
   #if defined(OCS_WITH_OPENSSL)
            // @todo need a new context
            //       - when the master host changed
            //       - when the certificate was renewed - how to find that out? Certificate file timestamp?
            //       for now simply always create a new context
            if (old_master_host != nullptr /* && strcmp(old_master_host, master_name) != 0 */) {
               lList *answer_list = nullptr;
               int cl_ret = gdi_update_client_tls_config(&answer_list, master_name);
               if (cl_ret != CL_RETVAL_OK) {
                  //DPRINTF(SFNMAX, "gdi_setup_tls_config failed: %s\n", cl_get_error_text(cl_ret));
                  answer_list_output(&answer_list);
               }
               lFreeList(&answer_list);
            }
   #else
               DPRINTF(SFNMAX, MSG_SSL_NOT_BUILT_IN);
   #endif
         }
      }
   }
   DRETURN(gdi_data_get_master_host());
}

int
ocs::gdi::ClientBase::gdi_is_alive(lList **answer_list) {
   DENTER(TOP_LAYER);
   cl_com_SIRM_t *status = nullptr;
   int cl_ret = CL_RETVAL_OK;
   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   const char *comp_name = prognames[QMASTER];
   const char *comp_host = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
   int comp_id = 1;
   u_long32 comp_port = bootstrap_get_sge_qmaster_port();

   if (handle == nullptr) {
#if 0
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "handle not found %s:0", component_get_component_name());
#endif
      DRETURN(CL_RETVAL_PARAMS);
   }

   /*
    * update endpoint information of qmaster in commlib
    * qmaster could have changed due to migration
    */
   cl_com_append_known_endpoint_from_name((char *) comp_host, (char *) comp_name, comp_id,
                                          (int)comp_port, CL_CM_AC_DISABLED, true);

   DPRINTF("to->comp_host, to->comp_name, to->comp_id: %s/%s/%d\n", comp_host ? comp_host : "", comp_name ? comp_name : "", comp_id);
   cl_ret = cl_commlib_get_endpoint_status(handle, (char *) comp_host, (char *) comp_name, comp_id, &status);
   if (cl_ret != CL_RETVAL_OK) {
#if 0
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_commlib_get_endpoint_status failed: " SFQ, cl_get_error_text(cl_ret));
#endif
   } else {
      DEBUG(SFNMAX, MSG_GDI_QMASTER_STILL_RUNNING);
   }

   if (status != nullptr) {
      DEBUG(MSG_GDI_ENDPOINT_UPTIME_UU, static_cast<u_long32>(status->runtime), static_cast<u_long32>(status->application_status));
      cl_com_free_sirm_message(&status);
   }

   DRETURN(cl_ret);
}
