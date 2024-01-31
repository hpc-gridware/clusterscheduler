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
#include "uti/setup_path.h"
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
void gdi_once_init(void);
void feature_mt_init(void);
void sc_mt_init(void);
#endif

#include "gdi/sge_gdi_ctx.h"
#include "gdi/sge_gdi2.h"
#include "gdi/sge_gdi_packet_internal.h"

typedef struct {
   sge_path_state_class_t* sge_path_state_obj;
   sge_csp_path_class_t* sge_csp_path_obj;

   char* component_name;
   char* thread_name;
   char* master;
   char* component_username;
   char* username;
   char* groupname;
   uid_t uid;
   gid_t gid;

   char *ssl_private_key;
   char *ssl_certificate;

   lList *alp;
   int last_commlib_error;
   sge_error_class_t *eh;

   bool is_setup;
   u_long32 last_qmaster_file_read;
} sge_gdi_ctx_t;

static pthread_key_t  gdi_state_key;
static pthread_once_t gdi_once_control = PTHREAD_ONCE_INIT;

typedef struct {
   /* gdi request base */
   u_long32 request_id;     /* incremented with each GDI request to have a unique request ID
                               it is ensured that the request ID is also contained in answer */
} gdi_state_t;

static void gdi_state_destroy(void* state) {
   sge_free(&state);
}

void gdi_once_init(void) {
   pthread_key_create(&gdi_state_key, &gdi_state_destroy);
}

static void gdi_state_init(gdi_state_t* state) {
   state->request_id = 0;
}

/****** gid/gdi_setup/gdi_mt_init() ************************************************
*  NAME
*     gdi_mt_init() -- Initialize GDI state for multi threading use.
*
*  SYNOPSIS
*     void gdi_mt_init(void) 
*
*  FUNCTION
*     Set up GDI. This function must be called at least once before any of the
*     GDI functions is used. This function is idempotent, i.e. it is safe to
*     call it multiple times.
*
*     Thread local storage for the GDI state information is reserved. 
*
*  INPUTS
*     void - NONE 
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: gdi_mt_init() is MT safe 
*
*******************************************************************************/
void gdi_mt_init(void)
{
   pthread_once(&gdi_once_control, gdi_once_init);
}

/****** libs/gdi/gdi_state_get_????() ************************************
*  NAME
*     gdi_state_get_????() - read access to gdilib global variables
*
*  FUNCTION
*     Provides access to either global variable or per thread global
*     variable.
*
******************************************************************************/
u_long32 gdi_state_get_next_request_id(void)
{
   GET_SPECIFIC(gdi_state_t, gdi_state,
                gdi_state_init, gdi_state_key, "gdi_state_get_next_request_id");
   gdi_state->request_id++;
   return gdi_state->request_id;
}

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
   tl->ctx = NULL;
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
      GET_SPECIFIC(sge_gdi_ctx_thread_local_t, tl, sge_gdi_thread_local_ctx_init, sge_gdi_ctx_key,
                "set_thread_local_ctx");
      tl->ctx = ctx;

      if (ctx != NULL) {
         log_state_set_log_context(ctx);
      } else {
         log_state_set_log_context(NULL);
      }
   }
   DRETURN_VOID;
}

static bool
sge_gdi_ctx_setup(sge_gdi_ctx_class_t *thiz, int prog_number, const char* component_name,
                  const char *thread_name, const char* username, const char *groupname,  bool qmaster_internal_client);

static void sge_gdi_ctx_destroy(void *theState);

static void sge_gdi_ctx_set_is_setup(sge_gdi_ctx_class_t *thiz, bool is_setup);
static bool sge_gdi_ctx_is_setup(sge_gdi_ctx_class_t *thiz);
static void sge_gdi_ctx_class_get_errors(sge_gdi_ctx_class_t *thiz, lList **alpp, bool clear_errors);
static void sge_gdi_ctx_class_error(sge_gdi_ctx_class_t *thiz, int error_type, int error_quality, const char* fmt, ...);
static sge_path_state_class_t* get_sge_path_state(sge_gdi_ctx_class_t *thiz);
static sge_csp_path_class_t* get_sge_csp_path(sge_gdi_ctx_class_t *thiz);
static cl_com_handle_t* get_com_handle(sge_gdi_ctx_class_t *thiz);
static const char* get_component_name(sge_gdi_ctx_class_t *thiz);
static const char* get_thread_name(sge_gdi_ctx_class_t *thiz);
static const char* get_master(sge_gdi_ctx_class_t *thiz, bool reread);
static const char* get_username(sge_gdi_ctx_class_t *thiz);
static const char* get_cell_root(sge_gdi_ctx_class_t *thiz);
static const char* get_groupname(sge_gdi_ctx_class_t *thiz);
static uid_t ctx_get_uid(sge_gdi_ctx_class_t *thiz);
static gid_t ctx_get_gid(sge_gdi_ctx_class_t *thiz);
static const char* get_bootstrap_file(sge_gdi_ctx_class_t *thiz);
static const char* get_act_qmaster_file(sge_gdi_ctx_class_t *thiz);
static const char* get_acct_file(sge_gdi_ctx_class_t *thiz);
static const char* get_reporting_file(sge_gdi_ctx_class_t *thiz);
static const char* get_shadow_master_file(sge_gdi_ctx_class_t *thiz);
static const char* get_private_key(sge_gdi_ctx_class_t *thiz);
static const char* get_certificate(sge_gdi_ctx_class_t *thiz);
static int ctx_get_last_commlib_error(sge_gdi_ctx_class_t *thiz);
static void ctx_set_last_commlib_error(sge_gdi_ctx_class_t *thiz, int cl_error);

static int sge_gdi_ctx_class_prepare_enroll(sge_gdi_ctx_class_t *thiz);
static int sge_gdi_ctx_class_connect(sge_gdi_ctx_class_t *thiz);
static int sge_gdi_ctx_class_is_alive(sge_gdi_ctx_class_t *thiz);

static lList* sge_gdi_ctx_class_gdi_tsm(sge_gdi_ctx_class_t *thiz, const char *schedd_name, const char *cell);
static lList* sge_gdi_ctx_class_gdi_kill(sge_gdi_ctx_class_t *thiz, lList *id_list, const char *cell,
                                          u_long32 option_flags, u_long32 action_flag);

static int sge_gdi_ctx_log_flush_func(cl_raw_list_t* list_p);

static void sge_gdi_ctx_class_dprintf(sge_gdi_ctx_class_t *ctx);

sge_gdi_ctx_class_t *
sge_gdi_ctx_class_create(int prog_number, const char *component_name,
                         const char *thread_name,
                         const char *username, const char *groupname,
                         bool is_qmaster_internal_client,
                         lList **alpp)
{
   sge_gdi_ctx_class_t *ret = (sge_gdi_ctx_class_t *)sge_malloc(sizeof(sge_gdi_ctx_class_t));
   sge_gdi_ctx_t *gdi_ctx = NULL;

   DENTER(TOP_LAYER);

   if (!ret) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC,
                              ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DRETURN(NULL);
   }

   if (is_qmaster_internal_client) {
      ret->sge_gdi_packet_execute = sge_gdi_packet_execute_internal;
      ret->sge_gdi_packet_wait_for_result = sge_gdi_packet_wait_for_result_internal;
   } else {
      ret->sge_gdi_packet_execute = sge_gdi_packet_execute_external;
      ret->sge_gdi_packet_wait_for_result = sge_gdi_packet_wait_for_result_external;
   }
   ret->gdi = sge_gdi2;
   ret->gdi_multi = sge_gdi2_multi;
   ret->gdi_wait = sge_gdi2_wait;

   ret->prepare_enroll = sge_gdi_ctx_class_prepare_enroll;
   ret->connect = sge_gdi_ctx_class_connect;
   ret->is_alive = sge_gdi_ctx_class_is_alive;
   ret->tsm = sge_gdi_ctx_class_gdi_tsm;
   ret->kill = sge_gdi_ctx_class_gdi_kill;

   ret->get_sge_path_state = get_sge_path_state;
   ret->get_component_name = get_component_name;
   ret->get_thread_name = get_thread_name;

   ret->get_master = get_master;
   ret->get_username = get_username;
   ret->get_bootstrap_file = get_bootstrap_file;
   ret->get_act_qmaster_file = get_act_qmaster_file;
   ret->get_acct_file = get_acct_file;
   ret->get_reporting_file = get_reporting_file;
   ret->get_shadow_master_file = get_shadow_master_file;
   ret->get_cell_root = get_cell_root;
   ret->get_groupname = get_groupname;
   ret->get_uid = ctx_get_uid;
   ret->get_gid = ctx_get_gid;
   ret->get_com_handle = get_com_handle;

   ret->get_private_key = get_private_key;
   ret->get_certificate = get_certificate;

   ret->dprintf = sge_gdi_ctx_class_dprintf;

   ret->sge_gdi_ctx_handle = (sge_gdi_ctx_t*)sge_malloc(sizeof(sge_gdi_ctx_t));
   memset(ret->sge_gdi_ctx_handle, 0, sizeof(sge_gdi_ctx_t));

   if (!ret->sge_gdi_ctx_handle) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      sge_gdi_ctx_class_destroy(&ret);
      DRETURN(NULL);
   }

   /*
   ** create error handler of context
   */
   gdi_ctx = (sge_gdi_ctx_t*)ret->sge_gdi_ctx_handle;
   gdi_ctx->eh = sge_error_class_create();
   if (!gdi_ctx->eh) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DRETURN(NULL);
   }


   if (!sge_gdi_ctx_setup(ret, prog_number, component_name, thread_name, username, groupname, is_qmaster_internal_client)) {
      sge_gdi_ctx_class_get_errors(ret, alpp, true);
      sge_gdi_ctx_class_destroy(&ret);
      DRETURN(NULL);
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
   sge_gdi_ctx_destroy((*pst)->sge_gdi_ctx_handle);
   sge_free(pst);

   DRETURN_VOID;
}

static void sge_gdi_ctx_class_get_errors(sge_gdi_ctx_class_t *thiz, lList **alpp, bool clear_errors)
{
   sge_gdi_ctx_t *gdi_ctx = NULL;

   DENTER(TOP_LAYER);

   if (!thiz || !thiz->sge_gdi_ctx_handle) {
      DRETURN_VOID;
   }

   gdi_ctx = (sge_gdi_ctx_t*)thiz->sge_gdi_ctx_handle;

   answer_list_from_sge_error(gdi_ctx->eh, alpp, clear_errors);

   DRETURN_VOID;
}

static void sge_gdi_ctx_class_error(sge_gdi_ctx_class_t *thiz, int error_type, int error_quality, const char* fmt, ...)
{
   sge_gdi_ctx_t *gdi_ctx = NULL;

   DENTER(TOP_LAYER);

   if (thiz != NULL && thiz->sge_gdi_ctx_handle != NULL) {
      gdi_ctx = (sge_gdi_ctx_t*)thiz->sge_gdi_ctx_handle;

      if (gdi_ctx->eh && fmt != NULL) {
         va_list arg_list;

         va_start(arg_list, fmt);
         gdi_ctx->eh->verror(gdi_ctx->eh, error_type, error_quality, fmt, arg_list);
         va_end(arg_list);
      }
   }
   DRETURN_VOID;
}

static void sge_gdi_ctx_set_is_setup(sge_gdi_ctx_class_t *thiz, bool is_setup)
{
   sge_gdi_ctx_t *gdi_ctx = NULL;

   DENTER(TOP_LAYER);

   if (!thiz || !thiz->sge_gdi_ctx_handle) {
      DRETURN_VOID;
   }

   gdi_ctx = (sge_gdi_ctx_t*)thiz->sge_gdi_ctx_handle;

   gdi_ctx->is_setup = is_setup;

   DRETURN_VOID;
}


static bool sge_gdi_ctx_is_setup(sge_gdi_ctx_class_t *thiz)
{
   sge_gdi_ctx_t *gdi_ctx = NULL;

   DENTER(TOP_LAYER);

   if (!thiz || !thiz->sge_gdi_ctx_handle) {
      DRETURN(false);
   }

   gdi_ctx = (sge_gdi_ctx_t*)thiz->sge_gdi_ctx_handle;

   DRETURN(gdi_ctx->is_setup);
}


static bool
sge_gdi_ctx_setup(sge_gdi_ctx_class_t *thiz, int prog_number, const char* component_name,
                  const char *thread_name, const char* username,
                  const char *groupname,  bool qmaster_internal_client)
{
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *)thiz->sge_gdi_ctx_handle;
   sge_error_class_t *eh = es->eh;

   DENTER(TOP_LAYER);

   /*
    * Call all functions which have to be called once for each process.
    * Those functions will then be called when the first thread of a process
    * creates its context. 
    */
   prof_mt_init();
   feature_mt_init();
   gdi_mt_init();
   sc_mt_init();
   obj_mt_init();
   bootstrap_mt_init();
   sc_mt_init();
   fgl_mt_init();
   path_mt_init();


   /* TODO: shall we do that here ? */
   lInit(nmv);

   bootstrap_set_qmaster_internal(qmaster_internal_client);

   if (feature_initialize_from_string(bootstrap_get_security_mode())) {
      CRITICAL((SGE_EVENT, "feature_initialize_from_string() failed"));
      DRETURN(false);
   }

#if 1
   es->sge_path_state_obj = sge_path_state_class_create(eh);
   if (!es->sge_path_state_obj) {
      CRITICAL((SGE_EVENT, "sge_path_state_class_create() failed"));
      DRETURN(false);
   }

   es->sge_csp_path_obj = sge_csp_path_class_create(eh);
   if (!es->sge_csp_path_obj) {
      CRITICAL((SGE_EVENT, "sge_csp_path_class_create() failed"));
      DRETURN(false);
   }
#endif

   if (component_name == NULL) {
      es->component_name = strdup(prognames[prog_number]);
   } else {
      es->component_name = strdup(component_name);
   }

   if (thread_name == NULL) {
      es->thread_name = strdup(prognames[prog_number]);
   } else {
      es->thread_name = strdup(thread_name);
   }

   /* set uid and gid */
   {
      struct passwd *pwd;
      struct passwd pw_struct;
      char *buffer;
      int size;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      pwd = sge_getpwnam_r(username, &pw_struct, buffer, size);

      if (!pwd) {
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "sge_getpwnam_r failed for username %s", username);
         sge_free(&buffer);
         DRETURN(false);
      }
      es->uid = pwd->pw_uid;
      if (groupname != NULL) {
         gid_t gid;
         if (sge_group2gid(groupname, &gid, MAX_NIS_RETRIES) == 1) {
            eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "sge_group2gid failed for groupname %s", groupname);
            sge_free(&buffer);
            DRETURN(false);
         }
         es->gid = gid;
      } else {
         es->gid = pwd->pw_gid;
      }

      sge_free(&buffer);
   }

   es->username = strdup(username);

   /*
   ** groupname
   */
   if (groupname != NULL) {
      es->groupname = strdup(groupname);
   } else {
      if (_sge_gid2group(es->gid, &(es->gid), &(es->groupname), MAX_NIS_RETRIES)) {
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_GETGRGIDXFAILEDERRORX_U, sge_u32c(es->gid));
         DRETURN(false);
      }
   }

   /*
   ** set the component_username and check if login is needed
   */
   {
      struct passwd *pwd = NULL;
      char *buffer;
      int size;
      struct passwd pwentry;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      if (getpwuid_r((uid_t)getuid(), &pwentry, buffer, size, &pwd) == 0) {
         es->component_username = sge_strdup(es->component_username, pwd->pw_name);
         sge_free(&buffer);
      } else {
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "getpwuid_r failed");
         sge_free(&buffer);
         DRETURN(false);
      }
#if 0
      /*
      ** TODO: Login to system somehow and send something like a token with request
      **       similar like the secret key in CSP mode
      */
      DPRINTF(("es->username: '%s', es->component_username: '%s'\n", es->username, es->component_username));      
      if (strcmp(es->username, es->component_username) != 0) {
#if 1      
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "!!!! Alert login needed !!!!!");
         DRETURN(false);
#else
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "!!!! First time login !!!!!");
/*          sge_authenticate(es->username, callback_handler); */
         
#endif
      }
#endif
   }

   DRETURN(true);
}

static void sge_gdi_ctx_destroy(void *theState)
{
   sge_gdi_ctx_t *s = (sge_gdi_ctx_t *)theState;

   DENTER(TOP_LAYER);

   sge_path_state_class_destroy(&(s->sge_path_state_obj));
   sge_csp_path_class_destroy(&(s->sge_csp_path_obj));
   sge_free(&(s->master));
   sge_free(&(s->username));
   sge_free(&(s->groupname));
   sge_free(&(s->component_name));
   sge_free(&(s->thread_name));
   sge_free(&(s->component_username));
   sge_free(&(s->ssl_certificate));
   sge_free(&(s->ssl_private_key));
   sge_error_class_destroy(&(s->eh));
   sge_free(&s);

   DRETURN_VOID;
}

static int sge_gdi_ctx_class_connect(sge_gdi_ctx_class_t *thiz)
{

   int ret = 0;
   bool is_alive_check = true;

   DENTER(TOP_LAYER);

   /*
   ** TODO: must contain similar functionality as sge_gdi_setup
   */

   ret = sge_gdi_ctx_class_prepare_enroll(thiz);

   /* check if master is alive */
   if (ret == CL_RETVAL_OK && is_alive_check) {
      const char *master = thiz->get_master(thiz, true);
      DPRINTF(("thiz->get_master(thiz) = %s\n", master));
      ret = thiz->is_alive(thiz);
   }

   DRETURN(ret);
}

static int sge_gdi_ctx_class_prepare_enroll(sge_gdi_ctx_class_t *thiz) {

   sge_path_state_class_t* path_state = thiz->get_sge_path_state(thiz);
   cl_host_resolve_method_t resolve_method = CL_SHORT;
   cl_framework_t  communication_framework = CL_CT_TCP;
   cl_com_handle_t* handle = NULL;
   const char *help = NULL;
   const char *default_domain = NULL;
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
               if (env_sge_commlib_debug != NULL) {
                  cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, sge_gdi_ctx_log_flush_func);
               } else {
                  /* here we use default commlib flush function */
                  cl_ret = cl_com_setup_commlib(CL_RW_THREAD, CL_LOG_OFF, NULL);
               }
            }
            break;
         default:
            {
               INFO((SGE_EVENT, SFNMAX, MSG_GDI_SINGLE_THREADED_STARTUP));
               if (env_sge_commlib_debug != NULL) {
                  cl_ret = cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, sge_gdi_ctx_log_flush_func);
               } else {
                  cl_ret = cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, NULL);
               }
               /*
               ** verbose logging is switched on by default
               */
               log_state_set_log_verbose(1);
            }
      }

      if (cl_ret != CL_RETVAL_OK) {
         sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                            "cl_com_setup_commlib failed: %s", cl_get_error_text(cl_ret));
         DRETURN(cl_ret);
      }
   }

   /* set the alias file */
   cl_ret = cl_com_set_alias_file((char*)path_state->get_alias_file(path_state));
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                         "cl_com_set_alias_file failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /* setup the resolve method */

   if (!bootstrap_get_ignore_fqdn()) {
      resolve_method = CL_LONG;
   }
   if ((help = bootstrap_get_default_domain()) != NULL) {
      if (SGE_STRCASECMP(help, NONE_STR) != 0) {
         default_domain = help;
      }
   }

   cl_ret = cl_com_set_resolve_method(resolve_method, (char*)default_domain);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                         "cl_com_set_resolve_method failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /*
   ** reresolve qualified hostname with use of host aliases 
   ** (corresponds to reresolve_me_qualified_hostname)
   */
   cl_ret = reresolve_qualified_hostname();
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING,
                         "reresolve hostname failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }


   /* 
   ** TODO set a general_communication_error  
   */
   cl_ret = cl_com_set_error_func(general_communication_error);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                         "cl_com_set_error_func failed: %s", cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   /* TODO set tag name function */
   cl_ret = cl_com_set_tag_name_func(sge_dump_message_tag);
   if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                         "cl_com_set_tag_name_func failed: %s", cl_get_error_text(cl_ret));
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

   handle = thiz->get_com_handle(thiz);
   if (handle == NULL) {
      /* handle does not exist, create one */

      int me_who = uti_state_get_mewho();
      const char *progname = uti_state_get_sge_formal_prog_name();
      const char *master = thiz->get_master(thiz, true);
      const char *qualified_hostname = uti_state_get_qualified_hostname();
      u_long32 sge_qmaster_port = bootstrap_get_sge_qmaster_port();
      u_long32 sge_execd_port = bootstrap_get_sge_execd_port();
      int my_component_id = 0; /* 1 for daemons, 0=automatical for clients */

      if (master == NULL && !(me_who == QMASTER)) {
         DRETURN(CL_RETVAL_UNKNOWN);
      }

      /*
      ** CSP initialize
      */
      if (strcasecmp(bootstrap_get_security_mode(), "csp") == 0) {
         cl_ssl_setup_t *sec_ssl_setup_config = NULL;
         cl_ssl_cert_mode_t ssl_cert_mode = CL_SSL_PEM_FILE;
         sge_csp_path_class_t *sge_csp = get_sge_csp_path(thiz);

         if (thiz->get_certificate(thiz) != NULL) {
            ssl_cert_mode = CL_SSL_PEM_BYTE;
            sge_csp->set_cert_file(sge_csp, thiz->get_certificate(thiz));
            sge_csp->set_key_file(sge_csp, thiz->get_private_key(thiz));
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
         if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
            DPRINTF(("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret)));
            sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                               qualified_hostname,
                               progname, 0,
                               sge_qmaster_port,
                               cl_get_error_text(cl_ret));
            DRETURN(cl_ret);
         }

         /*
         ** set the CSP credential info into commlib
         */
         cl_ret = cl_com_specify_ssl_configuration(sec_ssl_setup_config);
         if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
            DPRINTF(("return value of cl_com_specify_ssl_configuration(): %s\n", cl_get_error_text(cl_ret)));
            sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                               (char*)thiz->get_component_name(thiz), 0,
                               sge_qmaster_port,
                               cl_get_error_text(cl_ret));
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
                                          (char*)thiz->get_component_name(thiz),
                                          my_component_id,
                                          1,
                                          0);
            cl_com_set_auto_close_mode(handle, CL_CM_AC_ENABLED);
            if (handle == NULL) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
                  /*
                  ** TODO: eh error handler does no logging
                  */
                  ERROR((SGE_EVENT, MSG_GDI_CANT_GET_COM_HANDLE_SSUUS,
                                    qualified_hostname,
                                    (char*)thiz->get_component_name(thiz),
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
                                          (char*)thiz->get_component_name(thiz),
                                          my_component_id,  /* this endpoint is called "qmaster" 
                                                               and has id 1 */
                                          1,
                                          0); /* select timeout is set to 1 second 0 usec */
            if (handle == NULL) {
               if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) {
                  /*
                  ** TODO: eh error handler does no logging
                  */
                  ERROR((SGE_EVENT, MSG_GDI_CANT_GET_COM_HANDLE_SSUUS,
                                       qualified_hostname,
                                       (char*)thiz->get_component_name(thiz),
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
               master = thiz->get_master(thiz, true);

               if (master != NULL) {
                  /* TODO: sge_hostcmp uses implicitly bootstrap state info */
                  /* check a running qmaster on different host */
                  if (getuniquehostname(master, act_resolved_qmaster_name, 0) == CL_RETVAL_OK &&
                        sge_hostcmp(act_resolved_qmaster_name, qualified_hostname) != 0) {
                     DPRINTF(("act_qmaster file contains host "SFQ" which doesn't match local host name "SFQ"\n",
                              master, qualified_hostname));

                     cl_com_set_error_func(NULL);

                     alive_back = thiz->is_alive(thiz);
                     cl_ret = cl_com_set_error_func(general_communication_error);
                     if (cl_ret != CL_RETVAL_OK) {
                        ERROR((SGE_EVENT, SFNMAX, cl_get_error_text(cl_ret)));
                     }

                     if (alive_back == CL_RETVAL_OK && getenv("SGE_TEST_HEARTBEAT_TIMEOUT") == NULL ) {
                        CRITICAL((SGE_EVENT, MSG_GDI_MASTER_ON_HOST_X_RUNINNG_TERMINATE_S, master));
                        /* TODO: remove !!! */
                        SGE_EXIT(NULL, 1);
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
            DPRINTF(("creating %s GDI handle\n", thiz->get_component_name(thiz)));
            handle = cl_com_create_handle(&cl_ret,
                                          communication_framework,
                                          CL_CM_CT_MESSAGE,
                                          false,
                                          sge_qmaster_port,
                                          CL_TCP_DEFAULT,
                                          (char*)thiz->get_component_name(thiz),
                                          my_component_id,
                                          1,
                                          0);
            if (handle == NULL) {
/*             if (cl_ret != CL_RETVAL_OK && cl_ret != ctx_get_last_commlib_error(thiz)) { */
                  sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                     MSG_GDI_CANT_CONNECT_HANDLE_SSUUS,
                                     uti_state_get_qualified_hostname(),
                                     thiz->get_component_name(thiz),
                                     sge_u32c(my_component_id),
                                     sge_u32c(sge_qmaster_port),
                                     cl_get_error_text(cl_ret));
/*             }        */
            }
            break;
      }
      ctx_set_last_commlib_error(thiz, cl_ret);
   }

#ifdef DEBUG_CLIENT_SUPPORT
   /* set rmon callback for message printing (after handle creation) */
   rmon_set_print_callback(gdi_rmon_print_callback_function);
#endif

   if ((uti_state_get_mewho() == QMASTER) && (getenv("SGE_TEST_SOCKET_BIND") != NULL)) {
      /* this is for testsuite socket bind test (issue 1096 ) */
         struct timeval now;
         gettimeofday(&now,NULL);

         /* if this environment variable is set, we wait 15 seconds after
            communication lib setup */
         DPRINTF(("waiting for 60 seconds, because environment SGE_TEST_SOCKET_BIND is set\n"));
         while ( handle != NULL && now.tv_sec - handle->start_time.tv_sec  <= 60 ) {
            DPRINTF(("timeout: "sge_U32CFormat"\n",sge_u32c(now.tv_sec - handle->start_time.tv_sec)));
            cl_commlib_trigger(handle, 1);
            gettimeofday(&now,NULL);
         }
         DPRINTF(("continue with setup\n"));
   }
   DRETURN(cl_ret);
}

static int sge_gdi_ctx_class_is_alive(sge_gdi_ctx_class_t *thiz)
{
   cl_com_SIRM_t* status = NULL;
   int cl_ret = CL_RETVAL_OK;
   cl_com_handle_t *handle = thiz->get_com_handle(thiz);

   /* TODO */
   const char* comp_name = prognames[QMASTER];
   const char* comp_host = thiz->get_master(thiz, false);
   int         comp_id   = 1;
   int         comp_port = bootstrap_get_sge_qmaster_port();

   DENTER(TOP_LAYER);

   if (handle == NULL) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                "handle not found %s:0", thiz->get_component_name(thiz));
      DRETURN(CL_RETVAL_PARAMS);
   }

   /*
    * update endpoint information of qmaster in commlib
    * qmaster could have changed due to migration
    */
   cl_com_append_known_endpoint_from_name((char*)comp_host, (char*)comp_name, comp_id,
                                          comp_port, CL_CM_AC_DISABLED, true);

   DPRINTF(("to->comp_host, to->comp_name, to->comp_id: %s/%s/%d\n", comp_host?comp_host:"", comp_name?comp_name:"", comp_id));
   cl_ret = cl_commlib_get_endpoint_status(handle, (char*)comp_host, (char*)comp_name, comp_id, &status);
   if (cl_ret != CL_RETVAL_OK) {
      sge_gdi_ctx_class_error(thiz, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                "cl_commlib_get_endpoint_status failed: "SFQ, cl_get_error_text(cl_ret));
   } else {
      DEBUG((SGE_EVENT, SFNMAX, MSG_GDI_QMASTER_STILL_RUNNING));
   }

   if (status != NULL) {
      DEBUG((SGE_EVENT,MSG_GDI_ENDPOINT_UPTIME_UU, sge_u32c(status->runtime) ,
             sge_u32c(status->application_status)));
      cl_com_free_sirm_message(&status);
   }

   DRETURN(cl_ret);
}

static lList* sge_gdi_ctx_class_gdi_tsm(sge_gdi_ctx_class_t *thiz, const char *schedd_name, const char *cell)
{
   lList *alp = NULL;

   DENTER(TOP_LAYER);

   alp = gdi2_tsm(thiz, schedd_name, cell);

   DRETURN(alp);

}

static lList* sge_gdi_ctx_class_gdi_kill(sge_gdi_ctx_class_t *thiz, lList *id_list, const char *cell,
                                          u_long32 option_flags, u_long32 action_flag)
{
   lList *alp = NULL;

   DENTER(TOP_LAYER);

   alp = gdi2_kill(thiz, id_list, cell, option_flags, action_flag);

   DRETURN(alp);

}

static void sge_gdi_ctx_class_dprintf(sge_gdi_ctx_class_t *ctx)
{
   DENTER(TOP_LAYER);

   if (ctx == NULL) {
      DRETURN_VOID;
   }
   DPRINTF(("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"));

   (ctx->get_sge_path_state(ctx))->dprintf(ctx->get_sge_path_state(ctx));
#if 1
   bootstrap_log_parameter();
#endif

   DPRINTF(("master: %s\n", ctx->get_master(ctx, false)));
   DPRINTF(("uid/username: %d/%s\n", (int) ctx->get_uid(ctx), ctx->get_username(ctx)));
   DPRINTF(("gid/groupname: %d/%s\n", (int) ctx->get_gid(ctx), ctx->get_groupname(ctx)));

   DPRINTF(("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"));

   DRETURN_VOID;
}


/** --------- getter/setter ------------------------------------------------- */
static cl_com_handle_t* get_com_handle(sge_gdi_ctx_class_t *thiz)
{
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return cl_com_get_handle(es->component_name, 0);
}

static sge_path_state_class_t* get_sge_path_state(sge_gdi_ctx_class_t *thiz)
{
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->sge_path_state_obj;
}

static sge_csp_path_class_t* get_sge_csp_path(sge_gdi_ctx_class_t *thiz)
{
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->sge_csp_path_obj;
}

static const char* get_master(sge_gdi_ctx_class_t *thiz, bool reread) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   sge_path_state_class_t* path_state = thiz->get_sge_path_state(thiz);
   sge_error_class_t *eh = es ? es->eh : NULL;
   static bool error_already_logged = false;

   DENTER(BASIS_LAYER);

   if (es->master == NULL || reread) {
      char err_str[SGE_PATH_MAX+128];
      char master_name[CL_MAXHOSTLEN];
      u_long32 now = sge_get_gmt();

      /* fix system clock moved back situation */
      if (es->last_qmaster_file_read > now) {
         es->last_qmaster_file_read = 0;
      }

      if (es->master == NULL || now - es->last_qmaster_file_read >= 30) {
         /* re-read act qmaster file (max. every 30 seconds) */
         DPRINTF(("re-read actual qmaster file\n"));
         es->last_qmaster_file_read = now;

         if (get_qm_name(master_name, path_state->get_act_qmaster_file(path_state), err_str) == -1) {
            if (eh != NULL && !error_already_logged) {
               eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_READMASTERNAMEFAILED_S, err_str);
               error_already_logged = true;
            }
            DRETURN(NULL);
         }
         error_already_logged = false;
         DPRINTF(("(re-)reading act_qmaster file. Got master host \"%s\"\n", master_name));
         /*
         ** TODO: thread locking needed here ?
         */
         es->master = sge_strdup(es->master,master_name);
      }
   }
   DRETURN(es->master);
}

static const char* get_component_name(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   const char* ret = NULL;
   DENTER(BASIS_LAYER);
   ret = es->component_name;
   DRETURN(ret);
}

static const char* get_thread_name(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   const char* ret = NULL;
   DENTER(BASIS_LAYER);
   ret = es->thread_name;
   DRETURN(ret);
}

static const char* get_private_key(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   const char *pkey = NULL;

   DENTER(BASIS_LAYER);
   pkey = es->ssl_private_key;
   DRETURN(pkey);
}

static const char* get_certificate(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   const char *cert = NULL;

   DENTER(BASIS_LAYER);
   cert = es->ssl_certificate;
   DRETURN(cert);
}

static const char* get_cell_root(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t* path_state = thiz->get_sge_path_state(thiz);
   const char *cell_root = NULL;

   DENTER(BASIS_LAYER);
   cell_root = path_state->get_cell_root(path_state);
   DRETURN(cell_root);
}

static const char* get_bootstrap_file(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t *path_state = thiz->get_sge_path_state(thiz);
   return path_state->get_bootstrap_file(path_state);
}

static const char* get_act_qmaster_file(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t *path_state = thiz->get_sge_path_state(thiz);
   return path_state->get_act_qmaster_file(path_state);
}

static const char* get_acct_file(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t *path_state = thiz->get_sge_path_state(thiz);
   return path_state->get_acct_file(path_state);
}

static const char* get_reporting_file(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t *path_state = thiz->get_sge_path_state(thiz);
   return path_state->get_reporting_file(path_state);
}


static const char* get_shadow_master_file(sge_gdi_ctx_class_t *thiz) {
   sge_path_state_class_t *path_state = thiz->get_sge_path_state(thiz);
   return path_state->get_shadow_masters_file(path_state);
}


static const char* get_username(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->username;
}

static const char* get_groupname(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->groupname;
}

static int ctx_get_last_commlib_error(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->last_commlib_error;
}

static void ctx_set_last_commlib_error(sge_gdi_ctx_class_t *thiz, int cl_error) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   es->last_commlib_error = cl_error;
}

static uid_t ctx_get_uid(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->uid;
}

static gid_t ctx_get_gid(sge_gdi_ctx_class_t *thiz) {
   sge_gdi_ctx_t *es = (sge_gdi_ctx_t *) thiz->sge_gdi_ctx_handle;
   return es->gid;
}

static int sge_gdi_ctx_log_flush_func(cl_raw_list_t* list_p)
{
   int ret_val;
   cl_log_list_elem_t* elem = NULL;

   DENTER(COMMD_LAYER);

   if (list_p == NULL) {
      DRETURN(CL_RETVAL_LOG_NO_LOGLIST);
   }

   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      DRETURN(ret_val);
   }

   while ((elem = cl_log_list_get_first_elem(list_p) ) != NULL) {
      char* param;
      if (elem->log_parameter == NULL) {
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
sge_setup2(sge_gdi_ctx_class_t **context, u_long32 progid, u_long32 thread_id,
           lList **alpp, bool is_qmaster_intern_client)
{
   char  user[128] = "";
   char  group[128] = "";
   const char *sge_root = NULL;

   DENTER(TOP_LAYER);

   if (context == NULL) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_GDI_CONTEXT_NULL);
      DRETURN(AE_ERROR);
   }

   sge_getme(progid);

   /*
   ** TODO:
   ** get the environment for now here  -> sge_env_class_t should be enhanced and used instead as input param
   */
   sge_root = getenv("SGE_ROOT");
   if (sge_root == NULL) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC,
                              ANSWER_QUALITY_CRITICAL, MSG_SGEROOTNOTSET);
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
   *context = sge_gdi_ctx_class_create(progid, prognames[progid],
                                       threadnames[thread_id], user, group,
                                       is_qmaster_intern_client, alpp);

   if (*context == NULL) {
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

   if (context_ref && sge_gdi_ctx_is_setup(*context_ref)) {
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

   if ((*context_ref)->prepare_enroll(*context_ref) != CL_RETVAL_OK) {
      sge_gdi_ctx_class_get_errors(*context_ref, alpp, true);
      DRETURN(AE_QMASTER_DOWN);
   }

   sge_gdi_ctx_set_is_setup(*context_ref, true);

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
bool sge_daemonize_prepare(sge_gdi_ctx_class_t *ctx) {
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
      ioctl(fd, TIOCNOTTY, (char *) NULL);
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
bool sge_daemonize_finalize(sge_gdi_ctx_class_t *ctx)
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
      SGE_EXIT(NULL, 0);
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
      ioctl(fd, TIOCNOTTY, (char *) NULL);
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
      SGE_EXIT(NULL, 0);
   }

   SETPGRP;

   uti_state_set_daemonized(true);

   DRETURN(1);
}
