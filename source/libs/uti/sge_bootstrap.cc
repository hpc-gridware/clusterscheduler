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

#include <string.h>
#include <pthread.h>

#include "basis_types.h"

#include "uti/sge_rmon.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"
#include "uti/sge_dstring.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_spool.h"
#include "uti/setup_path.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_arch.h"
#include "uti/sge_hostname.h"

#include "uti/msg_utilib.h"

#include "sge.h"

typedef struct {
   // bootstrap file
   char *admin_user;
   char *default_domain;
   char *spooling_method;
   char *spooling_lib;
   char *spooling_params;
   char *binary_path;
   char *qmaster_spool_dir;
   char *security_mode;
   int listener_thread_count;
   int worker_thread_count;
   int scheduler_thread_count;
   bool job_spooling;
   bool ignore_fqdn;

   // environment
   char *sge_root;
   char *sge_cell;
   u_long32 sge_qmaster_port;
   u_long32 sge_execd_port;
   bool from_services;
   bool qmaster_internal;

   // component information
   u_long32 component_id;
   char *component_name;
   char *thread_name;
} sge_bootstrap_tl_t;

static pthread_once_t bootstrap_once = PTHREAD_ONCE_INIT;
static pthread_key_t sge_bootstrap_tl_key;

static void
bootstrap_thread_local_once_init(void);

static void
bootstrap_tl_init(sge_bootstrap_tl_t *tl);

static void
bootstrap_thread_local_destroy(void *tl);

static void
set_admin_user(sge_bootstrap_tl_t *tl, const char *admin_user) {
   tl->admin_user = sge_strdup(tl->admin_user, admin_user);
}

static void
set_default_domain(sge_bootstrap_tl_t *tl, const char *default_domain) {
   tl->default_domain = sge_strdup(tl->default_domain, default_domain);
}

static void
set_ignore_fqdn(sge_bootstrap_tl_t *tl, bool ignore_fqdn) {
   tl->ignore_fqdn = ignore_fqdn;
}

static void
set_spooling_method(sge_bootstrap_tl_t *tl, const char *spooling_method) {
   tl->spooling_method = sge_strdup(tl->spooling_method, spooling_method);
}

static void
set_spooling_params(sge_bootstrap_tl_t *tl, const char *spooling_params) {
   tl->spooling_params = sge_strdup(tl->spooling_params, spooling_params);
}

static void
set_spooling_lib(sge_bootstrap_tl_t *tl, const char *spooling_lib) {
   tl->spooling_lib = sge_strdup(tl->spooling_lib, spooling_lib);
}

static void
set_binary_path(sge_bootstrap_tl_t *tl, const char *binary_path) {
   tl->binary_path = sge_strdup(tl->binary_path, binary_path);
}

static void
set_qmaster_spool_dir(sge_bootstrap_tl_t *tl, const char *qmaster_spool_dir) {
   tl->qmaster_spool_dir = sge_strdup(tl->qmaster_spool_dir, qmaster_spool_dir);
}

static void
set_security_mode(sge_bootstrap_tl_t *tl, const char *security_mode) {
   tl->security_mode = sge_strdup(tl->security_mode, security_mode);
}

static void
set_listener_thread_count(sge_bootstrap_tl_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   tl->listener_thread_count = thread_count;
}

static void
set_worker_thread_count(sge_bootstrap_tl_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   tl->worker_thread_count = thread_count;
}

static void
set_scheduler_thread_count(sge_bootstrap_tl_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 0;
   } else if (thread_count > 1) {
      thread_count = 1;
   }
   tl->scheduler_thread_count = thread_count;
}

static void
set_job_spooling(sge_bootstrap_tl_t *tl, bool job_spooling) {
   tl->job_spooling = job_spooling;
}

static void
set_sge_root(sge_bootstrap_tl_t *tl, const char *sge_root) {
   tl->sge_root = sge_strdup(tl->sge_root, sge_root);
}

static void
set_sge_cell(sge_bootstrap_tl_t *tl, const char *sge_cell) {
   tl->sge_cell = sge_strdup(tl->sge_cell, sge_cell);
}

static void
set_sge_qmaster_port(sge_bootstrap_tl_t *tl, u_long32 sge_qmaster_port) {
   tl->sge_qmaster_port = sge_qmaster_port;
}

static void
set_sge_execd_port(sge_bootstrap_tl_t *tl, u_long32 sge_execd_port) {
   tl->sge_execd_port = sge_execd_port;
}

static void
set_from_services(sge_bootstrap_tl_t *tl, bool from_services) {
   tl->from_services = from_services;
}

static void
set_qmaster_internal(sge_bootstrap_tl_t *tl, bool qmaster_internal) {
   tl->qmaster_internal = qmaster_internal;
}

static void
set_component_id(sge_bootstrap_tl_t *tl, u_long32 component_id) {
   tl->component_id = component_id;
}

static void
set_component_name(sge_bootstrap_tl_t *tl, const char *component_name) {
   tl->component_name = sge_strdup(tl->component_name, component_name);
}

static void
set_thread_name(sge_bootstrap_tl_t *tl, const char *thread_name) {
   tl->thread_name = sge_strdup(tl->thread_name, thread_name);
}

static void
log_parameter(sge_bootstrap_tl_t *tl) {
   DENTER(TOP_LAYER);

   DPRINTF(("ENVIRONMENT\n"))
   DPRINTF(("sge_root            >%s<\n", tl->sge_root ? tl->sge_root : "NA"));
   DPRINTF(("sge_cell            >%s<\n", tl->sge_cell ? tl->sge_cell : "NA"));
   DPRINTF(("sge_qmaster_port    >%d<\n", tl->sge_qmaster_port));
   DPRINTF(("sge_execd_port      >%d<\n", tl->sge_execd_port));
   DPRINTF(("from_services       >%s<\n", tl->from_services ? "true" : "false"));
   DPRINTF(("qmaster_internal    >%s<\n", tl->qmaster_internal ? "true" : "false"));

   DPRINTF(("COMPONENT\n"))
   DPRINTF(("component_id        >%d<\n", tl->component_id));
   DPRINTF(("component_name      >%s<\n", tl->component_name ? tl->component_name : "NA"));
   DPRINTF(("thread_name         >%s<\n", tl->thread_name ? tl->thread_name : "NA"));

   DPRINTF(("BOOTSTRAP FILE\n"))
   DPRINTF(("admin_user          >%s<\n", tl->admin_user));
   DPRINTF(("default_domain      >%s<\n", tl->default_domain));
   DPRINTF(("ignore_fqdn         >%s<\n", tl->ignore_fqdn ? "true" : "false"));
   DPRINTF(("spooling_method     >%s<\n", tl->spooling_method));
   DPRINTF(("spooling_lib        >%s<\n", tl->spooling_lib));
   DPRINTF(("spooling_params     >%s<\n", tl->spooling_params));
   DPRINTF(("binary_path         >%s<\n", tl->binary_path));
   DPRINTF(("qmaster_spool_dir   >%s<\n", tl->qmaster_spool_dir));
   DPRINTF(("security_mode       >%s<\n", tl->security_mode));
   DPRINTF(("job_spooling        >%s<\n", tl->job_spooling ? "true" : "false"));
   DPRINTF(("listener_threads    >%d<\n", tl->listener_thread_count));
   DPRINTF(("worker_threads      >%d<\n", tl->worker_thread_count));
   DPRINTF(("scheduler_threads   >%d<\n", tl->scheduler_thread_count));

   DRETURN_VOID;
}

void
bootstrap_mt_init(void) {
   pthread_once(&bootstrap_once, bootstrap_thread_local_once_init);
}


static void
bootstrap_thread_local_once_init(void) {
   pthread_key_create(&sge_bootstrap_tl_key, bootstrap_thread_local_destroy);
}

static void
bootstrap_init_from_environment(sge_bootstrap_tl_t *tl) {
   DENTER(TOP_LAYER);

   set_sge_root(tl, getenv("SGE_ROOT"));

   const char *sge_cell = getenv("SGE_CELL");
   set_sge_cell(tl, sge_cell != NULL ? sge_cell : DEFAULT_CELL);

   bool from_services;
   set_sge_qmaster_port(tl, sge_get_qmaster_port(&from_services));
   set_from_services(tl, from_services);

   set_sge_execd_port(tl, sge_get_execd_port());

   set_qmaster_internal(tl, false);

#if 0
   char  user[128] = "";
   if (sge_uid2user(geteuid(), user, sizeof(user), MAX_NIS_RETRIES)) {
      CRITICAL((SGE_EVENT, MSG_SYSTEM_RESOLVEUSER));
      DRETURN_VOID;
   }

   char  group[128] = "";
   if (sge_gid2group(getegid(), group, sizeof(group), MAX_NIS_RETRIES)) {
      CRITICAL((SGE_EVENT, MSG_SYSTEM_RESOLVEGROUP));
      DRETURN_VOID;
   }
#endif

   DRETURN_VOID;
}


static void
bootstrap_init_from_file(sge_bootstrap_tl_t *tl) {
#define NUM_BOOTSTRAP 14
#define NUM_REQ_BOOTSTRAP 9
   /*const char **/
   bootstrap_entry_t name[NUM_BOOTSTRAP] = {{"admin_user",        true},
                                            {"default_domain",    true},
                                            {"ignore_fqdn",       true},
                                            {"spooling_method",   true},
                                            {"spooling_lib",      true},
                                            {"spooling_params",   true},
                                            {"binary_path",       true},
                                            {"qmaster_spool_dir", true},
                                            {"security_mode",     true},
                                            {"job_spooling",      false},
                                            {"listener_threads",  false},
                                            {"worker_threads",    false},
                                            {"scheduler_threads", false},
   };
   char value[NUM_BOOTSTRAP][1025];
   const char *bootstrap_file;
   dstring error_dstring = DSTRING_INIT;
   const char *sge_cell = sge_get_default_cell();
   char buffer[2*1024];
   dstring path;
   const char * sge_root;

   DENTER(TOP_LAYER);

   // EB: OGE-116 TODO path should depend only on functions provided by this module
   // build path of the bootstrap files
   sge_dstring_init(&path, buffer, sizeof(buffer));
   sge_root = sge_get_root_dir(0, buffer, sizeof(buffer)-1, 1);
   sge_dstring_sprintf_append(&path, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", sge_root, sge_cell, COMMON_DIR, BOOTSTRAP_FILE);
   bootstrap_file = sge_dstring_get_string(&path);

   // early exist if we don't know where the bootstrap file is
   if (bootstrap_file != NULL) {
      DPRINTF(("bootstrap file is %s", bootstrap_file))
   } else {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_UTI_CANNOTRESOLVEBOOTSTRAPFILE));
      DRETURN_VOID;
   }

   /* read bootstrapping information */
   if (sge_get_confval_array(bootstrap_file, NUM_BOOTSTRAP, NUM_REQ_BOOTSTRAP, name, value, &error_dstring)) {
      CRITICAL((SGE_EVENT, SFNMAX, sge_dstring_get_string(&error_dstring)));
      DRETURN_VOID;
   } else {
      /* store bootstrapping information */
      set_admin_user(tl, value[0]);
      set_default_domain(tl, value[1]);
      {
         u_long32 uval;
         parse_ulong_val(NULL, &uval, TYPE_BOO, value[2], NULL, 0);
         set_ignore_fqdn(tl, uval ? true : false);
      }
      set_spooling_method(tl, value[3]);
      set_spooling_lib(tl, value[4]);
      set_spooling_params(tl, value[5]);
      set_binary_path(tl, value[6]);
      set_qmaster_spool_dir(tl, value[7]);
      set_security_mode(tl, value[8]);
      if (strcmp(value[9], "")) {
         u_long32 uval = 0;
         parse_ulong_val(NULL, &uval, TYPE_BOO, value[9], NULL, 0);
         set_job_spooling(tl, uval ? true : false);
      } else {
         set_job_spooling(tl, true);
      }
      {
         u_long32 val = 0;
         parse_ulong_val(NULL, &val, TYPE_INT, value[10], NULL, 0);
         set_listener_thread_count(tl, val);
      }
      {
         u_long32 val = 0;
         parse_ulong_val(NULL, &val, TYPE_INT, value[11], NULL, 0);
         set_worker_thread_count(tl, val);
      }
      {
         u_long32 val = 0;
         parse_ulong_val(NULL, &val, TYPE_INT, value[12], NULL, 0);
         set_scheduler_thread_count(tl, val);
      }

      log_parameter(tl);
   }

   DRETURN_VOID;
}

static void
bootstrap_tl_init(sge_bootstrap_tl_t *tl) {
   DENTER(TOP_LAYER);
   memset(tl, 0, sizeof(sge_bootstrap_tl_t));

   // do environment setup first because file based init depends on that
   bootstrap_init_from_environment(tl);
   bootstrap_init_from_file(tl);
   DRETURN_VOID;
}

static void
bootstrap_thread_local_destroy(void *tl) {
   sge_bootstrap_tl_t *_tl = (sge_bootstrap_tl_t *) tl;

   // component parameters
   sge_free(&(_tl->component_name));
   sge_free(&(_tl->thread_name));

   // environment parameters
   sge_free(&(_tl->sge_root));
   sge_free(&(_tl->sge_cell));

   // bootstrap file parameters
   sge_free(&(_tl->admin_user));
   sge_free(&(_tl->default_domain));
   sge_free(&(_tl->spooling_method));
   sge_free(&(_tl->spooling_lib));
   sge_free(&(_tl->spooling_params));
   sge_free(&(_tl->binary_path));
   sge_free(&(_tl->qmaster_spool_dir));
   sge_free(&(_tl->security_mode));

   // wrapping structure
   sge_free(&_tl);
}

void
bootstrap_log_parameter(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_dprintf");
   log_parameter(tl);
}

const char *
bootstrap_get_admin_user(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_admin_user");
   return tl->admin_user;
}

void
bootstrap_set_admin_user(const char *admin_user) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_admin_user");
   set_admin_user(tl, admin_user);
}

const char *
bootstrap_get_default_domain(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_default_domain");
   return tl->default_domain;
}


void
bootstrap_set_default_domain(const char *default_domain) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_default_domain");
   set_default_domain(tl, default_domain);
}

bool
bootstrap_get_ignore_fqdn(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_ignore_fqdn");
   return tl->ignore_fqdn;
}

void
bootstrap_set_ignore_fqdn(bool ignore_fqdn) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_ignore_fqdn");
   set_ignore_fqdn(tl, ignore_fqdn);
}

const char *
bootstrap_get_spooling_method(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_spooling_method");
   return tl->spooling_method;
}

void
bootstrap_set_spooling_method(const char *spooling_method) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_spooling_method");
   set_spooling_method(tl, spooling_method);
}

const char *
bootstrap_get_spooling_lib(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_spooling_lib");
   return tl->spooling_lib;
}

void
bootstrap_set_spooling_lib(const char *spooling_lib) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_spooling_lib");
   set_spooling_lib(tl, spooling_lib);
}

const char *
bootstrap_get_spooling_params(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_spooling_params");
   return tl->spooling_params;
}

void
bootstrap_set_spooling_params(const char *spooling_params) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_spooling_params");
   set_spooling_params(tl, spooling_params);
}

const char *
bootstrap_get_binary_path(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_binary_path");
   return tl->binary_path;
}

void
bootstrap_set_binary_path(const char *binary_path) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_binary_path");
   set_binary_path(tl, binary_path);
}

const char *
bootstrap_get_qmaster_spool_dir(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_qmaster_spool_dir");
   return tl->qmaster_spool_dir;
}

void
bootstrap_set_qmaster_spool_dir(const char *qmaster_spool_dir) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_qmaster_spool_dir");
   set_qmaster_spool_dir(tl, qmaster_spool_dir);
}

const char *
bootstrap_get_security_mode(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_security_mode");
   return tl->security_mode;
}

void
bootstrap_set_security_mode(const char *security_mode) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_security_mode");
   set_security_mode(tl, security_mode);
}

int
bootstrap_get_listener_thread_count(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_listener_thread_count");
   return tl->listener_thread_count;
}

void
bootstrap_set_listener_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_listener_thread_count");
   set_listener_thread_count(tl, thread_count);
}

int
bootstrap_get_worker_thread_count(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_worker_thread_count");
   return tl->worker_thread_count;
}

void bootstrap_set_worker_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_worker_thread_count");
   set_worker_thread_count(tl, thread_count);
}

int bootstrap_get_scheduler_thread_count(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_scheduler_thread_count");

   return tl->scheduler_thread_count;
}

void bootstrap_set_scheduler_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_scheduler_thread_count");
   set_scheduler_thread_count(tl, thread_count);
}

bool bootstrap_get_job_spooling(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_job_spooling");
   return tl->job_spooling;
}

void bootstrap_set_job_spooling(bool job_spooling) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_job_spooling");
   set_job_spooling(tl, job_spooling);
}

const char *
bootstrap_get_sge_root(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_sge_root");
   return tl->sge_root;
}

void
bootstrap_set_sge_root(const char *sge_root) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_sge_root");
   set_sge_root(tl, sge_root);
}


const char *
bootstrap_get_sge_cell(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_sge_cell");
   return tl->sge_cell;
}

void
bootstrap_set_sge_cell(const char *sge_cell) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_sge_cell");
   set_sge_cell(tl, sge_cell);
}

u_long32
bootstrap_get_sge_qmaster_port(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_sge_qmaster_port");
   return tl->sge_qmaster_port;
}

void bootstrap_set_sge_qmaster_port(u_long32 sge_qmaster_port) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_sge_qmaster_port");
   set_sge_qmaster_port(tl, sge_qmaster_port);
}

u_long32
bootstrap_get_sge_execd_port(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_sge_execd_port");
   return tl->sge_execd_port;
}

void
bootstrap_set_sge_execd_port(u_long32 sge_execd_port) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_sge_execd_port");
   set_sge_execd_port(tl, sge_execd_port);
}

bool
bootstrap_is_from_services(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_is_from_services");
   return tl->from_services;
}

void
bootstrap_set_from_services(bool from_services) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_from_services");
   set_from_services(tl, from_services);
}

bool
bootstrap_is_qmaster_internal(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_is_qmaster_internal");
   return tl->qmaster_internal;
}

void
bootstrap_set_qmaster_internal(bool qmaster_internal) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_qmaster_internal");
   set_qmaster_internal(tl, qmaster_internal);
}

u_long32
bootstrap_get_component_id(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_component_id");
   return tl->component_id;
}

void
bootstrap_set_component_id(u_long32 component_id) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_component_id");
   set_component_id(tl, component_id);
}

const char *
bootstrap_get_component_name(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_component_name");
   return tl->component_name;
}

void
bootstrap_set_component_name(const char *component_name) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_component_name");
   set_component_name(tl, component_name);
}

const char *
bootstrap_get_thread_name(void) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_get_thread_name");
   return tl->thread_name;
}

void
bootstrap_set_thread_name(const char *thread_name) {
   GET_SPECIFIC(sge_bootstrap_tl_t, tl, bootstrap_tl_init, sge_bootstrap_tl_key, "bootstrap_set_thread_name");
   set_thread_name(tl, thread_name);
}

