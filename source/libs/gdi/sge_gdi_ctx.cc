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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

#if defined(DARWIN)
#  include <grp.h>
#elif defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#  include <termio.h>
#elif defined(FREEBSD) || defined(NETBSD)
//#  include <termios.h>
#else
#  include <termio.h>
#endif

#include "comm/commlib.h"

#include "uti/sge_rmon.h"
#include "uti/sge_hostname.h"
#include "uti/sge_fgl.h"
#include "uti/sge_log.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_prog.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_profiling.h"
#include "uti/msg_utilib.h"
#include "uti/sge_time.h"
#include "uti/sge_csp_path.h"
#include "uti/sge_os.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"

#include "gdi/qm_name.h"
#include "gdi/sge_gdi2.h"
#include "gdi/sge_gdi3.h"
#include "gdi/msg_gdilib.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_object.h"

#include "sge.h"

#include "msg_common.h"

/*
** need this for lInit(nmv)
*/
extern lNameSpace nmv[];

/* pipe for sge_daemonize_prepare() and sge_daemonize_finalize() */
static int fd_pipe[2];

#if  1
/* TODO: throw this out asap */
void sc_mt_init(void);
#endif

#include "gdi/sge_gdi_ctx.h"
#include "gdi/sge_gdi2.h"
#include "gdi/sge_gdi_packet_internal.h"

typedef struct {
   sge_gdi_ctx_class_t* ctx;
} sge_gdi_ctx_thread_local_t;

static pthread_once_t sge_gdi_ctx_once = PTHREAD_ONCE_INIT;
static pthread_key_t  sge_gdi_ctx_key;
static void sge_gdi_thread_local_ctx_once_init(void);
static void sge_gdi_thread_local_ctx_destroy(void* theState);
static void sge_gdi_thread_local_ctx_init(sge_gdi_ctx_thread_local_t* theState);

static void sge_gdi_thread_local_ctx_once_init(void)
{
   pthread_key_create(&sge_gdi_ctx_key, sge_gdi_thread_local_ctx_destroy);
}

static void sge_gdi_thread_local_ctx_destroy(void* theState) {
   sge_gdi_ctx_thread_local_t *tl = (sge_gdi_ctx_thread_local_t*)theState;
   tl->ctx = nullptr;
   sge_free(&theState);
}

static void sge_gdi_thread_local_ctx_init(sge_gdi_ctx_thread_local_t* theState)
{
   memset(theState, 0, sizeof(sge_gdi_ctx_thread_local_t));
}

void sge_gdi_set_thread_local_ctx(sge_gdi_ctx_class_t* ctx) {

   DENTER(TOP_LAYER);

   pthread_once(&sge_gdi_ctx_once, sge_gdi_thread_local_ctx_once_init);
   {
      GET_SPECIFIC(sge_gdi_ctx_thread_local_t, tl, sge_gdi_thread_local_ctx_init, sge_gdi_ctx_key);
      tl->ctx = ctx;

      if (ctx != nullptr) {
         log_state_set_log_context(ctx);
      } else {
         log_state_set_log_context(nullptr);
      }
   }
   DRETURN_VOID;
}

static bool
sge_gdi_ctx_setup(int prog_number, const char* component_name, const char *thread_name, bool qmaster_internal_client);

static void sge_gdi_ctx_class_get_errors(lList **alpp, bool clear_errors);
static sge_csp_path_class_t* get_sge_csp_path();
static int ctx_get_last_commlib_error();
static void ctx_set_last_commlib_error(int cl_error);
static int sge_gdi_ctx_log_flush_func(cl_raw_list_t* list_p);

sge_gdi_ctx_class_t *
sge_gdi_ctx_class_create(int prog_number, const char *component_name, const char *thread_name, bool is_qmaster_internal_client, lList **alpp)
{
   sge_gdi_ctx_class_t *ret = (sge_gdi_ctx_class_t *)sge_malloc(sizeof(sge_gdi_ctx_class_t));

   DENTER(TOP_LAYER);

   if (!ret) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DRETURN(nullptr);
   }

   if (is_qmaster_internal_client) {
      ret->sge_gdi_packet_execute = sge_gdi_packet_execute_internal;
      ret->sge_gdi_packet_wait_for_result = sge_gdi_packet_wait_for_result_internal;
   } else {
      ret->sge_gdi_packet_execute = sge_gdi_packet_execute_external;
      ret->sge_gdi_packet_wait_for_result = sge_gdi_packet_wait_for_result_external;
   }

   if (!sge_gdi_ctx_setup(prog_number, component_name, thread_name, is_qmaster_internal_client)) {
      sge_gdi_ctx_class_get_errors(alpp, true);
      sge_gdi_ctx_class_destroy(&ret);
      DRETURN(nullptr);
   }

   /*
   ** set default exit func, maybe overwritten
   */
   uti_state_set_exit_func(gdi2_default_exit_func);

   DRETURN(ret);
}

void sge_gdi_ctx_class_destroy(sge_gdi_ctx_class_t **pst)
{
   DENTER(TOP_LAYER);

   if (!pst || !*pst) {
      DRETURN_VOID;
   }

   /* free internal context structure */
   sge_free(pst);

   DRETURN_VOID;
}

static void sge_gdi_ctx_class_get_errors(lList **alpp, bool clear_errors)
{
   DENTER(TOP_LAYER);
   answer_list_from_sge_error(gdi3_get_error_handle(), alpp, clear_errors);
   DRETURN_VOID;
}

void
sge_gdi_ctx_class_error(int error_type, int error_quality, const char* fmt, ...) {
   DENTER(TOP_LAYER);
   if (gdi3_get_error_handle() && fmt != nullptr) {
      va_list arg_list;

      va_start(arg_list, fmt);
      sge_error_class_t *error_handle = gdi3_get_error_handle();
      error_handle->verror(error_handle, error_type, error_quality, fmt, arg_list);
      va_end(arg_list);
   }
   DRETURN_VOID;
}

static void sge_gdi_ctx_set_is_setup(bool is_setup)
{
   DENTER(TOP_LAYER);
   gdi3_set_setup(is_setup);
   DRETURN_VOID;
}


static bool sge_gdi_ctx_is_setup()
{
   DENTER(TOP_LAYER);
   DRETURN(gdi3_is_setup());
}


static bool
sge_gdi_ctx_setup(int prog_number, const char* component_name, const char *thread_name, bool qmaster_internal_client)
{
   DENTER(TOP_LAYER);

   /*
    * Call all functions which have to be called once for each process.
    * Those functions will then be called when the first thread of a process
    * creates its context. 
    */
   prof_mt_init();
   feature_mt_init();
   gdi3_mt_init();
   sc_mt_init();
   obj_mt_init();
   bootstrap_mt_init();
   sc_mt_init();
   fgl_mt_init();


   /* TODO: shall we do that here ? */
   lInit(nmv);

   bootstrap_set_qmaster_internal(qmaster_internal_client);
   bootstrap_set_component_id(prog_number);
   bootstrap_set_component_name(component_name ? component_name : prognames[prog_number]);
   bootstrap_set_thread_name(thread_name ? thread_name : prognames[prog_number]);
   bootstrap_log_parameter();

   if (feature_initialize_from_string(bootstrap_get_security_mode())) {
      CRITICAL((SGE_EVENT, "feature_initialize_from_string() failed"));
      DRETURN(false);
   }

#if 1
   gdi3_set_csp_path_obj(sge_csp_path_class_create(gdi3_get_error_handle()));
   if (!gdi3_get_csp_path_obj()) {
      CRITICAL((SGE_EVENT, "sge_csp_path_class_create() failed"));
      DRETURN(false);
   }
#endif

   DRETURN(true);
}

int sge_gdi_ctx_class_connect(sge_gdi_ctx_class_t *thiz)
{
   DENTER(TOP_LAYER);

   /* check if master is alive */
   int ret = sge_gdi_ctx_class_prepare_enroll(thiz);
   if (ret == CL_RETVAL_OK) {
      ret = sge_gdi_ctx_class_is_alive();
   }
   DRETURN(ret);
}

int sge_gdi_ctx_class_prepare_enroll(sge_gdi_ctx_class_t *thiz) {

   cl_host_resolve_method_t resolve_method = CL_SHORT;
   cl_framework_t  communication_framework = CL_CT_TCP;
   cl_com_handle_t* handle = nullptr;
   const char *help = nullptr;
   const char *default_domain = nullptr;
   int cl_ret = CL_RETVAL_OK;

   DENTER(TOP_LAYER);

   /* context setup is complete => setup the commlib
   ** TODO:
   ** there is a problem in qsub if it is used with CL_RW_THREAD, the signaling in qsub -sync
   ** makes qsub hang
   */

   if (cl_com_setup_commlib_complete() == false) {
      char* env_sge_commlib_debug = getenv("SGE_DEBUG_LEVEL");
      switch (uti_state_get_mewho()) {
         case QMASTER:
         case DRMAA:
         case SCHEDD:
         case EXECD:
            {
               INFO((SGE_EVENT, SFNMAX, MSG_GDI_MULTI_THREADED_STARTUP));
               /* if SGE_DEBUG_LEVEL environment is set we use gdi log flush function */
               /* you can set commlib debug level with env SGE_COMMLIB_DEBUG */
               if (env_sge_commlib_debug != nullptr) {
                  cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, sge_gdi_ctx_log_flush_func);
               } else {
                  /* here we use default commlib flush function */
                  cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, nullptr);
               }
            }
            break;
         default:
            {
               INFO((SGE_EVENT, SFNMAX, MSG_GDI_SINGLE_THREADED_STARTUP));
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
      }

      if (cl_ret != CL_RETVAL_OK) {
         sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "cl_com_setup_commlib failed: %s", cl_get_error_text(cl_ret));
         DRETURN(cl_ret);
      }
   }

   /* set the alias file */
   cl_ret = cl_com_set_alias_file(bootstrap_get_alias_file());
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
      sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "cl_com_set_alias_file failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /* setup the resolve method */

   if (!bootstrap_get_ignore_fqdn()) {
      resolve_method = CL_LONG;
   }
   if ((help = bootstrap_get_default_domain()) != nullptr) {
      if (SGE_STRCASECMP(help, NONE_STR) != 0) {
         default_domain = help;
      }
   }

   cl_ret = cl_com_set_resolve_method(resolve_method, (char*)default_domain);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
      sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "cl_com_set_resolve_method failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /*
   ** reresolve qualified hostname with use of host aliases 
   ** (corresponds to reresolve_me_qualified_hostname)
   */
   cl_ret = reresolve_qualified_hostname();
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
      sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING, "reresolve hostname failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }


   /* 
   ** TODO set a general_communication_error  
   */
   cl_ret = cl_com_set_error_func(general_communication_error);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
      sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "cl_com_set_error_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /* TODO set tag name function */
   cl_ret = cl_com_set_tag_name_func(sge_dump_message_tag);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
      sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "cl_com_set_tag_name_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

#ifdef DEBUG_CLIENT_SUPPORT
   /* set debug client callback function to rmon's debug client callback */
   cl_ret = cl_com_set_application_debug_client_callback_func(rmon_debug_client_callback);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, 
                         "cl_com_set_application_debug_client_callback_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }
#endif

   handle = cl_com_get_handle(bootstrap_get_component_name(), 0);
   if (handle == nullptr) {
      /* handle does not exist, create one */

      int me_who = uti_state_get_mewho();
      const char *progname = uti_state_get_sge_formal_prog_name();
      const char *master = gdi3_get_act_master_host(true);
      const char *qualified_hostname = uti_state_get_qualified_hostname();
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
         sge_csp_path_class_t *sge_csp = get_sge_csp_path();

         if (gdi3_get_ssl_certificate() != nullptr) {
            ssl_cert_mode = CL_SSL_PEM_BYTE;
            sge_csp->set_cert_file(sge_csp, gdi3_get_ssl_certificate());
            sge_csp->set_key_file(sge_csp, gdi3_get_ssl_private_key());
         }
         sge_csp->dprintf(sge_csp);

         communication_framework = CL_CT_SSL;
         cl_ret = cl_com_create_ssl_setup(&sec_ssl_setup_config,
                                       ssl_cert_mode,
                                       CL_SSL_v23,                                   /* ssl_method           */
                                       (char*)sge_csp->get_CA_cert_file(sge_csp),    /* ssl_CA_cert_pem_file */
                                       (char*)sge_csp->get_CA_key_file(sge_csp),     /* ssl_CA_key_pem_file  */
                                       (char*)sge_csp->get_cert_file(sge_csp),       /* ssl_cert_pem_file    */
                                       (char*)sge_csp->get_key_file(sge_csp),        /* ssl_key_pem_file     */
                                       (char*)sge_csp->get_rand_file(sge_csp),       /* ssl_rand_file        */
                                       (char*)sge_csp->get_reconnect_file(sge_csp),  /* ssl_reconnect_file   */
                                       (char*)sge_csp->get_crl_file(sge_csp),        /* ssl_crl_file         */
                                       sge_csp->get_refresh_time(sge_csp),           /* ssl_refresh_time     */
                                       (char*)sge_csp->get_password(sge_csp),        /* ssl_password         */
                                       sge_csp->get_verify_func(sge_csp));           /* ssl_verify_func (cl_ssl_verify_func_t)  */
         if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
            DPRINTF(("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret)));
            sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                               qualified_hostname, progname, 0, sge_qmaster_port, cl_get_error_text(cl_ret));
            DRETURN(cl_ret);
         }

         /*
         ** set the CSP credential info into commlib
         */
         cl_ret = cl_com_specify_ssl_configuration(sec_ssl_setup_config);
         if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
            DPRINTF(("return value of cl_com_specify_ssl_configuration(): %s\n", cl_get_error_text(cl_ret)));
            sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                               bootstrap_get_component_name(), 0, sge_qmaster_port, cl_get_error_text(cl_ret));
            cl_com_free_ssl_setup(&sec_ssl_setup_config);
            DRETURN(cl_ret);
         }
         cl_com_free_ssl_setup(&sec_ssl_setup_config);
      }

      if ( me_who == QMASTER ||
           me_who == EXECD   ||
           me_who == SCHEDD  ||
           me_who == SHADOWD ) {
         my_component_id = 1;
      }

      switch (me_who) {

         case EXECD:
            /* add qmaster as known endpoint */
            DPRINTF(("re-read actual qmaster file (prepare_enroll)\n"));
            cl_com_append_known_endpoint_from_name((char*)master,
                                                   (char*)prognames[QMASTER],
                                                   1,
                                                   sge_qmaster_port,
                                                   CL_CM_AC_DISABLED,
                                                   true);
            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE,
                                          true,
                                          sge_execd_port,
                                          CL_TCP_DEFAULT,
                                          (char*)bootstrap_get_component_name(),
                                          my_component_id,
                                          1,
                                          0);
            cl_com_set_auto_close_mode(handle, CL_CM_AC_ENABLED);
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
                  /*
                  ** TODO: eh error handler does no logging
                  */
                  ERROR((SGE_EVENT, MSG_GDI_CANT_GET_COM_HANDLE_SSUUS,
                                    qualified_hostname,
                                    bootstrap_get_component_name(),
                                    sge_u32c(my_component_id),
                                    sge_u32c(sge_execd_port),
                                    cl_get_error_text(cl_ret)));
               }
            }
            break;

         case QMASTER:
            DPRINTF(("creating QMASTER handle\n"));
            cl_com_append_known_endpoint_from_name((char*)master,
                                                   (char*) prognames[QMASTER],
                                                   1,
                                                   sge_qmaster_port,
                                                   CL_CM_AC_DISABLED ,
                                                   true);

            /* do a later qmaster commlib listen before creating qmaster handle */
            /* TODO: CL_COMMLIB_DELAYED_LISTEN is set to false, because
                     enabling it might cause problems with current shadowd and
                     startup qmaster implementation */
            cl_commlib_set_global_param(CL_COMMLIB_DELAYED_LISTEN, false);

            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE, /* message based tcp communication */
                                          true,
                                          sge_qmaster_port, /* create service on qmaster port */
                                          CL_TCP_DEFAULT,   /* use standard connect mode */
                                          (char*)bootstrap_get_component_name(),
                                          my_component_id,  /* this endpoint is called "qmaster" 
                                                               and has id 1 */
                                          1,
                                          0); /* select timeout is set to 1 second 0 usec */
            if (handle == nullptr) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error()) {
                  /*
                  ** TODO: eh error handler does no logging
                  */
                  ERROR((SGE_EVENT, MSG_GDI_CANT_GET_COM_HANDLE_SSUUS,
                                       qualified_hostname,
                                       bootstrap_get_component_name(),
                                       sge_u32c(my_component_id),
                                       sge_u32c(sge_qmaster_port),
                                       cl_get_error_text(cl_ret)));
               }
            } else {
               /* TODO: remove reresolving here */
               int alive_back = 0;
               char act_resolved_qmaster_name[CL_MAXHOSTLEN];
               cl_com_set_synchron_receive_timeout(handle, 5);
               /* TODO: reresolve master */
               master = gdi3_get_act_master_host(true);

               if (master != nullptr) {
                  /* TODO: sge_hostcmp uses implicitly bootstrap state info */
                  /* check a running qmaster on different host */
                  if (getuniquehostname(master, act_resolved_qmaster_name, 0) == CL_RETVAL_OK &&
                        sge_hostcmp(act_resolved_qmaster_name, qualified_hostname) != 0) {
                     DPRINTF(("act_qmaster file contains host "SFQ" which doesn't match local host name "SFQ"\n",
                              master, qualified_hostname));

                     cl_com_set_error_func(nullptr);

                     alive_back = sge_gdi_ctx_class_is_alive();
                     cl_ret = cl_com_set_error_func(general_communication_error);
                     if (cl_ret != CL_RETVAL_OK) {
                        ERROR((SGE_EVENT, SFNMAX, cl_get_error_text(cl_ret)));
                     }

                     if (alive_back == CL_RETVAL_OK && getenv("SGE_TEST_HEARTBEAT_TIMEOUT") == nullptr ) {
                        CRITICAL((SGE_EVENT, MSG_GDI_MASTER_ON_HOST_X_RUNINNG_TERMINATE_S, master));
                        /* TODO: remove !!! */
                        SGE_EXIT(nullptr, 1);
                     } else {
                        DPRINTF(("qmaster on host "SFQ" is down\n", master));
                     }
                  } else {
                     DPRINTF(("act_qmaster file contains local host name\n"));
                  }
               } else {
                  DPRINTF(("skipping qmaster alive check because act_qmaster is not availabe\n"));
               }
            }
            break;

         default:
            /* this is for "normal" gdi clients of qmaster */
            DPRINTF(("creating %s GDI handle\n", bootstrap_get_component_name()));
            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE,
                                          false,
                                          sge_qmaster_port,
                                          CL_TCP_DEFAULT,
                                          (char*)bootstrap_get_component_name(),
                                          my_component_id,
                                          1,
                                          0);
            if (handle == nullptr) {
/*             if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) { */
                  sge_gdi_ctx_class_error(STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                     MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                                     uti_state_get_qualified_hostname(),
                                     bootstrap_get_component_name(),
                                     sge_u32c(my_component_id),
                                     sge_u32c(sge_qmaster_port),
                                     cl_get_error_text(cl_ret));
/*             }        */
            }
            break;
      }
      ctx_set_last_commlib_error(cl_ret);
   }

#ifdef DEBUG_CLIENT_SUPPORT
   /* set rmon callback for message printing (after handle creation) */
   rmon_set_print_callback(gdi_rmon_print_callback_function);
#endif

   if ((uti_state_get_mewho() == QMASTER) && (getenv("SGE_TEST_SOCKET_BIND") != nullptr)) {
      /* this is for testsuite socket bind test (issue 1096 ) */
         struct timeval now;
         gettimeofday(&now,nullptr);

         /* if this environment variable is set, we wait 15 seconds after
            communication lib setup */
         DPRINTF(("waiting for 60 seconds, because environment SGE_TEST_SOCKET_BIND is set\n"));
         while ( handle != nullptr && now.tv_sec - handle->start_time.tv_sec  <= 60 ) {
            DPRINTF(("timeout: "sge_U32CFormat"\n",sge_u32c(now.tv_sec - handle->start_time.tv_sec)));
            cl_commlib_trigger(handle, 1);
            gettimeofday(&now,nullptr);
         }
         DPRINTF(("continue with setup\n"));
   }
   DRETURN(cl_ret);
}


lList* sge_gdi_ctx_class_gdi_tsm(sge_gdi_ctx_class_t *thiz)
{
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   alp = gdi2_tsm(thiz);

   DRETURN(alp);

}

lList* sge_gdi_ctx_class_gdi_kill(sge_gdi_ctx_class_t *thiz, lList *id_list, const char *cell,
                                          u_long32 option_flags, u_long32 action_flag)
{
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   alp = gdi2_kill(thiz, id_list, cell, option_flags, action_flag);

   DRETURN(alp);

}

/** --------- getter/setter ------------------------------------------------- */
static sge_csp_path_class_t* get_sge_csp_path()
{
   return gdi3_get_csp_path_obj();
}

static int ctx_get_last_commlib_error() {
   return gdi3_get_last_commlib_error();
}

static void ctx_set_last_commlib_error(int cl_error) {
   gdi3_set_last_commlib_error(cl_error);
}

static int sge_gdi_ctx_log_flush_func(cl_raw_list_t* list_p)
{
   int ret_val;
   cl_log_list_elem_t* elem = nullptr;

   DENTER(COMMD_LAYER);

   if (list_p == nullptr) {
      DRETURN(CL_RETVAL_LOG_NO_LOGLIST);
   }

   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      DRETURN(ret_val);
   }

   while ((elem = cl_log_list_get_first_elem(list_p) ) != nullptr) {
      char* param;
      if (elem->log_parameter == nullptr) {
         param = "";
      } else {
         param = elem->log_parameter;
      }

      switch(elem->log_type) {
         case CL_LOG_ERROR:
            if (log_state_get_log_level() >= LOG_ERR) {
               ERROR((SGE_EVENT,  "%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name));
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_WARNING:
            if (log_state_get_log_level() >= LOG_WARNING) {
               WARNING((SGE_EVENT,"%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name));
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_INFO:
            if (log_state_get_log_level() >= LOG_INFO) {
               INFO((SGE_EVENT,   "%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name));
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_DEBUG:
            if (log_state_get_log_level() >= LOG_DEBUG) {
               DEBUG((SGE_EVENT,  "%-15s=> %s %s (%s)", elem->log_thread_name, elem->log_message, param, elem->log_module_name));
            } else {
               printf("%-15s=> %s %s (%s)\n", elem->log_thread_name, elem->log_message, param, elem->log_module_name);
            }
            break;
         case CL_LOG_OFF:
            break;
      }
      cl_log_list_del_log(list_p);
   }

   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      DRETURN(ret_val);
   }
   DRETURN(CL_RETVAL_OK);
}

/*
** TODO: 
** only helper function to do the setup for clients similar to sge_setup()
*/
int
sge_setup2(sge_gdi_ctx_class_t **context, u_long32 progid, u_long32 thread_id, lList **alpp, bool is_qmaster_intern_client)
{
   char  user[128] = "";
   char  group[128] = "";
   const char *sge_root = nullptr;

   DENTER(TOP_LAYER);

   if (context == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_GDI_CONTEXT_NULL);
      DRETURN(AE_ERROR);
   }

   sge_getme(progid);
   bootstrap_mt_init();
   bootstrap_set_component_id(progid);
   gdi3_mt_init();

   /*
   ** TODO:
   ** get the environment for now here  -> sge_env_class_t should be enhanced and used instead as input param
   */
   sge_root = getenv("SGE_ROOT");
   if (sge_root == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SGEROOTNOTSET);
      DRETURN(AE_ERROR);
   }

   if (sge_uid2user(geteuid(), user, sizeof(user), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEUSER);
      DRETURN(AE_ERROR);
   }

   if (sge_gid2group(getegid(), group, sizeof(group), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEGROUP);
      DRETURN(AE_ERROR);
   }

   /* a dynamic eh handler is created */
   *context = sge_gdi_ctx_class_create(progid, prognames[progid], threadnames[thread_id], is_qmaster_intern_client, alpp);

   if (*context == nullptr) {
      DRETURN(AE_ERROR);
   }

   /* 
   ** TODO: we set the log state context here 
   **       this should be done more explicitily !!!
   */
   log_state_set_log_context(*context);

   /* 
   ** TODO: bootstrap info is used in cull functions sge_hostcpy
   **       ignore_fqdn, domain_name
   **       Therefore we have to set it into the thread ctx
   */
   sge_gdi_set_thread_local_ctx(*context);

   DRETURN(AE_OK);
}

/*
** TODO: 
** only helper function to do the setup for clients similar to sge_gdi_setup()
*/
int sge_gdi2_setup(sge_gdi_ctx_class_t **context_ref, u_long32 progid, u_long32 thread_id, lList **alpp)
{
   int ret = AE_OK;
   bool alpp_was_null = true;

   DENTER(TOP_LAYER);

   if (context_ref && sge_gdi_ctx_is_setup()) {
      if (alpp_was_null) {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, SFNMAX, MSG_GDI_GDI_ALREADY_SETUP));
      } else {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                 MSG_GDI_GDI_ALREADY_SETUP);
      }
      DRETURN(AE_ALREADY_SETUP);
   }
   ret = sge_setup2(context_ref, progid, thread_id, alpp, false);
   if (ret != AE_OK) {
      DRETURN(ret);
   }

   if (sge_gdi_ctx_class_prepare_enroll(*context_ref) != CL_RETVAL_OK) {
      sge_gdi_ctx_class_get_errors(alpp, true);
      DRETURN(AE_QMASTER_DOWN);
   }

   sge_gdi_ctx_set_is_setup(true);

   DRETURN(AE_OK);
}


/****** uti/os/sge_daemonize_prepare() *****************************************
*  NAME
*     sge_daemonize_prepare() -- prepare daemonize of process
*
*  SYNOPSIS
*     int sge_daemonize_prepare(void) 
*
*  FUNCTION
*     The parent process will wait for the child's successful daemonizing.
*     The client process will report successful daemonizing by a call to
*     sge_daemonize_finalize().
*     The parent process will exit with one of the following exit states:
*
*     typedef enum uti_deamonize_state_type {
*        SGE_DEAMONIZE_OK           = 0,  ok 
*        SGE_DAEMONIZE_DEAD_CHILD   = 1,  child exited before sending state 
*        SGE_DAEMONIZE_TIMEOUT      = 2   timeout whild waiting for state 
*     } uti_deamonize_state_t;
*
*     Daemonize the current application. Throws ourself into the
*     background and dissassociates from any controlling ttys.
*     Don't close filedescriptors mentioned in 'keep_open'.
*      
*     sge_daemonize_prepare() and sge_daemonize_finalize() will replace
*     sge_daemonize() for multithreaded applications.
*     
*     sge_daemonize_prepare() must be called before starting any thread. 
*
*
*  INPUTS
*     void - none
*
*  RESULT
*     int - true on success, false on error
*
*  SEE ALSO
*     uti/os/sge_daemonize_finalize()
*******************************************************************************/
bool sge_daemonize_prepare() {
   pid_t pid;
   int fd;

   int is_daemonized = uti_state_get_daemonized();

   DENTER(TOP_LAYER);

#ifndef NO_SGE_COMPILE_DEBUG
   if (TRACEON) {
      DRETURN(false);
   }
#endif

   if (is_daemonized) {
      DRETURN(true);
   }

   /* create pipe */
   if ( pipe(fd_pipe) < 0) {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_UTI_DAEMONIZE_CANT_PIPE));
      DRETURN(false);
   }

   if ( fcntl(fd_pipe[0], F_SETFL, O_NONBLOCK) != 0) {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_UTI_DAEMONIZE_CANT_FCNTL_PIPE));
      DRETURN(false);
   }

   /* close all fd's except pipe and first 3 */
   {
      int keep_open[5];
      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      keep_open[3] = fd_pipe[0];
      keep_open[4] = fd_pipe[1];
      sge_close_all_fds(keep_open, 5);
   }

   /* first fork */
   pid=fork();
   if (pid <0) {
      CRITICAL((SGE_EVENT, MSG_PROC_FIRSTFORKFAILED_S , strerror(errno)));
      exit(1);
   }

   if (pid > 0) {
      char line[256];
      int line_p = 0;
      int retries = 60;
      int exit_status = SGE_DAEMONIZE_TIMEOUT;
      int back;
      int errno_value = 0;

      /* close send pipe */
      close(fd_pipe[1]);

      /* check pipe for message from child */
      while (line_p < 4 && retries-- > 0) {
         errno = 0;
         back = read(fd_pipe[0], &line[line_p], 1);
         errno_value = errno;
         if (back > 0) {
            line_p++;
         } else {
            if (back != -1) {
               if (errno_value != EAGAIN ) {
                  retries=0;
                  exit_status = SGE_DAEMONIZE_DEAD_CHILD;
               }
            }
            DPRINTF(("back=%d errno=%d\n",back,errno_value));
            sleep(1);
         }
      }

      if (line_p >= 4) {
         line[3] = 0;
         exit_status = atoi(line);
         DPRINTF(("received: \"%d\"\n", exit_status));
      }

      switch(exit_status) {
         case SGE_DEAMONIZE_OK:
            INFO((SGE_EVENT, SFNMAX, MSG_UTI_DAEMONIZE_OK));
            break;
         case SGE_DAEMONIZE_DEAD_CHILD:
            WARNING((SGE_EVENT, SFNMAX, MSG_UTI_DAEMONIZE_DEAD_CHILD));
            break;
         case SGE_DAEMONIZE_TIMEOUT:
            WARNING((SGE_EVENT, SFNMAX, MSG_UTI_DAEMONIZE_TIMEOUT));
            break;
      }
      /* close read pipe */
      close(fd_pipe[0]);
      exit(exit_status); /* parent exit */
   }

   /* child */
   SETPGRP;

   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
      /* disassociate contolling tty */
      ioctl(fd, TIOCNOTTY, (char *) nullptr);
      close(fd);
   }


   /* second fork */
   pid=fork();
   if (pid < 0) {
      CRITICAL((SGE_EVENT, MSG_PROC_SECONDFORKFAILED_S , strerror(errno)));
      exit(1);
   }
   if ( pid > 0) {
      /* close read and write pipe for second child and exit */
      close(fd_pipe[0]);
      close(fd_pipe[1]);
      exit(0);
   }

   /* child of child */

   /* close read pipe */
   close(fd_pipe[0]);

   DRETURN(true);
}

/****** uti/os/sge_daemonize_finalize() ****************************************
*  NAME
*     sge_daemonize_finalize() -- finalize daemonize process
*
*  SYNOPSIS
*     int sge_daemonize_finalize(fd_set *keep_open) 
*
*  FUNCTION
*     report successful daemonizing to the parent process and close
*     all file descriptors. Set file descirptors 0, 1 and 2 to /dev/null 
*
*     sge_daemonize_prepare() and sge_daemonize_finalize() will replace
*     sge_daemonize() for multithreades applications.
*
*     sge_daemonize_finalize() must be called by the thread who have called
*     sge_daemonize_prepare().
*
*  INPUTS
*     fd_set *keep_open - file descriptor set to keep open
*
*  RESULT
*     int - true on success
*
*  SEE ALSO
*     uti/os/sge_daemonize_prepare()
*******************************************************************************/
bool sge_daemonize_finalize()
{
   int failed_fd;
   char tmp_buffer[4];
   int is_daemonized = uti_state_get_daemonized();

   DENTER(TOP_LAYER);

   /* don't call this function twice */
   if (is_daemonized) {
      DRETURN(true);
   }

   /* The response id has 4 byte, send it to father process */
   snprintf(tmp_buffer, 4, "%3d", SGE_DEAMONIZE_OK );
   if (write(fd_pipe[1], tmp_buffer, 4) != 4) {
      dstring ds = DSTRING_INIT;
      CRITICAL((SGE_EVENT, MSG_FILE_CANNOT_WRITE_SS, "fd_pipe[1]", sge_strerror(errno, &ds)));
      sge_dstring_free(&ds);
   }

   sleep(2); /* give father time to read the status */

   /* close write pipe */
   close(fd_pipe[1]);

   /* close first three file descriptors */
#ifndef __INSURE__
   close(0);
   close(1);
   close(2);

   /* new descriptors acquired for stdin, stdout, stderr should be 0,1,2 */
   failed_fd = sge_occupy_first_three();
   if (failed_fd  != -1) {
      CRITICAL((SGE_EVENT, MSG_CANNOT_REDIRECT_STDINOUTERR_I, failed_fd));
      SGE_EXIT(nullptr, 0);
   }
#endif

   SETPGRP;

   /* now have finished daemonizing */
   uti_state_set_daemonized(true);

   DRETURN(true);
}

/****** uti/os/sge_daemonize() ************************************************
*  NAME
*     sge_daemonize() -- Daemonize the current application
*
*  SYNOPSIS
*     int sge_daemonize(fd_set *keep_open)
*
*  FUNCTION
*     Daemonize the current application. Throws ourself into the
*     background and dissassociates from any controlling ttys.
*     Don't close filedescriptors mentioned in 'keep_open'.
*
*  INPUTS
*     fd_set *keep_open - bitmask
*     args   optional args
*
*  RESULT
*     int - Successfull?
*         1 - Yes
*         0 - No
*
*  NOTES
*     MT-NOTES: sge_daemonize() is not MT safe
******************************************************************************/
int sge_daemonize(int *keep_open, unsigned long nr_of_fds, sge_gdi_ctx_class_t *ctx)
{

   int fd;
   pid_t pid;
   int failed_fd;

   DENTER(TOP_LAYER);

#ifndef NO_SGE_COMPILE_DEBUG
   if (TRACEON) {
      DRETURN(0);
   }
#endif

   if (uti_state_get_daemonized()) {
      DRETURN(1);
   }

   if ((pid=fork())!= 0) {             /* 1st child not pgrp leader */
      if (pid<0) {
         CRITICAL((SGE_EVENT, MSG_PROC_FIRSTFORKFAILED_S , strerror(errno)));
      }
      exit(0);
   }

   SETPGRP;

   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
      /* disassociate contolling tty */
      ioctl(fd, TIOCNOTTY, (char *) nullptr);
      close(fd);
   }

   if ((pid=fork())!= 0) {
      if (pid<0) {
         CRITICAL((SGE_EVENT, MSG_PROC_SECONDFORKFAILED_S , strerror(errno)));
      }
      exit(0);
   }

   /* close all file descriptors */
   sge_close_all_fds(keep_open, nr_of_fds);

   /* new descriptors acquired for stdin, stdout, stderr should be 0,1,2 */
   failed_fd = sge_occupy_first_three();
   if (failed_fd  != -1) {
      CRITICAL((SGE_EVENT, MSG_CANNOT_REDIRECT_STDINOUTERR_I, failed_fd));
      SGE_EXIT(nullptr, 0);
   }

   SETPGRP;

   uti_state_set_daemonized(true);

   DRETURN(1);
}
