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

#include <cstring>

#include <pthread.h>

#include "basis_types.h"

#include "uti/sge_rmon.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"
#include "uti/sge_dstring.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_spool.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_uidgid.h"

#include "uti/msg_utilib.h"

#include "sge.h"

/* Must match Qxxx defines in sge_bootstrap.h */
const char *prognames[] = {
        "unknown",
        "qalter",        /* 1  */
        "qconf",         /* 2  */
        "qdel",          /* 3  */
        "qhold",         /* 4  */
        "qmaster",       /* 5  */
        "qmod",          /* 6  */
        "qresub",        /* 7  */
        "qrls",          /* 8  */
        "qselect",       /* 9  */
        "qsh",           /* 10 */
        "qrsh",          /* 11 */
        "qlogin",        /* 12 */
        "qstat",         /* 13 */
        "qsub",          /* 14 */
        "execd",         /* 15 */
        "qevent",        /* 16 */
        "qrsub",         /* 17 */
        "qrdel",         /* 18 */
        "qrstat",        /* 19 */
        "unknown",       /* 20 */
        "unknown",       /* 21 */
        "qmon",          /* 22 */
        "schedd",        /* 23 */
        "qacct",         /* 24 */
        "shadowd",       /* 25 */
        "qhost",         /* 26 */
        "spoolinit",     /* 27 */
        "japi",          /* 28 */
        "drmaa",         /* 29 */
        "qping",         /* 30 */
        "qquota",        /* 31 */
        "sge_share_mon"  /* 32 */
};

const char *threadnames[] = {
        "main",          /* 1 */
        "listener",      /* 2 */
        "event_master",  /* 3 */
        "timer",         /* 4 */
        "worker",        /* 5 */
        "signaler",      /* 6 */
        "scheduler",     /* 7 */
        "tester"         /* 8 */
};

// thread local storage (level 0)
typedef struct {
   char log_buffer[4 * MAX_STRING_SIZE];
} sge_bootstrap_tl0_t;

// thread local storage (level 1)
// initialization requires on data of level 0 (e.g. logging is used that requires a log-buffer)
// TODO: data can partially be shared between threads. cleanup required.
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

   // component information
   u_long32 component_id;
   char *component_name;
   char *thread_name;
   uid_t uid;
   gid_t gid;
   char *username;
   char *groupname;
   bool qmaster_internal;
   bool daemonized;
   char *qualified_hostname;
   char *unqualified_hostname;
   sge_exit_func_t exit_func;

   // files and paths
   char *cell_root;
   char *bootstrap_file;
   char *conf_file;
   char *sched_conf_file;
   char *act_qmaster_file;
   char *acct_file;
   char *reporting_file;
   char *local_conf_dir;
   char *shadow_masters_file;
   char *alias_file;
} sge_bootstrap_tl1_t;

static pthread_once_t bootstrap_once = PTHREAD_ONCE_INIT;
static pthread_key_t sge_bootstrap_tl0_key;
static pthread_key_t sge_bootstrap_tl1_key;

static void
bootstrap_thread_local_once_init();

static void
bootstrap_tl0_init(sge_bootstrap_tl0_t *tl);

static void
bootstrap_tl0_destroy(void *tl);

static void
bootstrap_tl1_init(sge_bootstrap_tl1_t *tl);

static void
bootstrap_tl1_destroy(void *tl);

static void
set_admin_user(sge_bootstrap_tl1_t *tl, const char *admin_user) {
   tl->admin_user = sge_strdup(tl->admin_user, admin_user);
}

static void
set_default_domain(sge_bootstrap_tl1_t *tl, const char *default_domain) {
   tl->default_domain = sge_strdup(tl->default_domain, default_domain);
}

static void
set_ignore_fqdn(sge_bootstrap_tl1_t *tl, bool ignore_fqdn) {
   tl->ignore_fqdn = ignore_fqdn;
}

static void
set_spooling_method(sge_bootstrap_tl1_t *tl, const char *spooling_method) {
   tl->spooling_method = sge_strdup(tl->spooling_method, spooling_method);
}

static void
set_spooling_params(sge_bootstrap_tl1_t *tl, const char *spooling_params) {
   tl->spooling_params = sge_strdup(tl->spooling_params, spooling_params);
}

static void
set_spooling_lib(sge_bootstrap_tl1_t *tl, const char *spooling_lib) {
   tl->spooling_lib = sge_strdup(tl->spooling_lib, spooling_lib);
}

static void
set_binary_path(sge_bootstrap_tl1_t *tl, const char *binary_path) {
   tl->binary_path = sge_strdup(tl->binary_path, binary_path);
}

static void
set_qmaster_spool_dir(sge_bootstrap_tl1_t *tl, const char *qmaster_spool_dir) {
   tl->qmaster_spool_dir = sge_strdup(tl->qmaster_spool_dir, qmaster_spool_dir);
}

static void
set_security_mode(sge_bootstrap_tl1_t *tl, const char *security_mode) {
   tl->security_mode = sge_strdup(tl->security_mode, security_mode);
}

static void
set_listener_thread_count(sge_bootstrap_tl1_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   tl->listener_thread_count = thread_count;
}

static void
set_worker_thread_count(sge_bootstrap_tl1_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   tl->worker_thread_count = thread_count;
}

static void
set_scheduler_thread_count(sge_bootstrap_tl1_t *tl, int thread_count) {
   if (thread_count <= 0) {
      thread_count = 0;
   } else if (thread_count > 1) {
      thread_count = 1;
   }
   tl->scheduler_thread_count = thread_count;
}

static void
set_job_spooling(sge_bootstrap_tl1_t *tl, bool job_spooling) {
   tl->job_spooling = job_spooling;
}

static void
set_sge_root(sge_bootstrap_tl1_t *tl, const char *sge_root) {
   tl->sge_root = sge_strdup(tl->sge_root, sge_root);
}

static void
set_sge_cell(sge_bootstrap_tl1_t *tl, const char *sge_cell) {
   tl->sge_cell = sge_strdup(tl->sge_cell, sge_cell);
}

static void
set_sge_qmaster_port(sge_bootstrap_tl1_t *tl, u_long32 sge_qmaster_port) {
   tl->sge_qmaster_port = sge_qmaster_port;
}

static void
set_sge_execd_port(sge_bootstrap_tl1_t *tl, u_long32 sge_execd_port) {
   tl->sge_execd_port = sge_execd_port;
}

static void
set_from_services(sge_bootstrap_tl1_t *tl, bool from_services) {
   tl->from_services = from_services;
}

static void
set_qmaster_internal(sge_bootstrap_tl1_t *tl, bool qmaster_internal) {
   tl->qmaster_internal = qmaster_internal;
}

static void
set_daemonized(sge_bootstrap_tl1_t *tl, bool daemonized) {
   tl->daemonized = daemonized;
}

static void
set_component_id(sge_bootstrap_tl1_t *tl, u_long32 component_id) {
   tl->component_id = component_id;
}

static void
set_uid(sge_bootstrap_tl1_t *tl, uid_t uid) {
   tl->uid = uid;
}

static void
set_gid(sge_bootstrap_tl1_t *tl, gid_t gid) {
   tl->gid = gid;
}

static void
set_component_name(sge_bootstrap_tl1_t *tl, const char *component_name) {
   tl->component_name = sge_strdup(tl->component_name, component_name);
}

static void
set_thread_name(sge_bootstrap_tl1_t *tl, const char *thread_name) {
   tl->thread_name = sge_strdup(tl->thread_name, thread_name);
}

static void
set_username(sge_bootstrap_tl1_t *tl, const char *username) {
   tl->username = sge_strdup(tl->username, username);
}

static void
set_qualified_hostname(sge_bootstrap_tl1_t *tl, const char *qualified_hostname) {
   tl->qualified_hostname = sge_strdup(tl->qualified_hostname, qualified_hostname);
}

static void
set_unqualified_hostname(sge_bootstrap_tl1_t *tl, const char *unqualified_hostname) {
   tl->unqualified_hostname = sge_strdup(tl->unqualified_hostname, unqualified_hostname);
}

static void
set_exit_func(sge_bootstrap_tl1_t *tl, sge_exit_func_t exit_func) {
   tl->exit_func = exit_func;
}

static void
set_groupname(sge_bootstrap_tl1_t *tl, const char *groupname) {
   tl->groupname = sge_strdup(tl->groupname, groupname);
}

static void
set_cell_root(sge_bootstrap_tl1_t *tl, const char *cell_root) {
   tl->cell_root = sge_strdup(tl->cell_root, cell_root);
}

static void
set_conf_file(sge_bootstrap_tl1_t *tl, const char *conf_file) {
   tl->conf_file = sge_strdup(tl->conf_file, conf_file);
}

static void
set_bootstrap_file(sge_bootstrap_tl1_t *tl, const char *bootstrap_file) {
   tl->bootstrap_file = sge_strdup(tl->bootstrap_file, bootstrap_file);
}

static void
set_act_qmaster_file(sge_bootstrap_tl1_t *tl, const char *act_qmaster_file) {
   tl->act_qmaster_file = sge_strdup(tl->act_qmaster_file, act_qmaster_file);
}

static void
set_acct_file(sge_bootstrap_tl1_t *tl, const char *acct_file) {
   tl->acct_file = sge_strdup(tl->acct_file, acct_file);
}

static void
set_reporting_file(sge_bootstrap_tl1_t *tl, const char *reporting_file) {
   tl->reporting_file = sge_strdup(tl->reporting_file, reporting_file);
}

static void
set_local_conf_dir(sge_bootstrap_tl1_t *tl, const char *local_conf_dir) {
   tl->local_conf_dir = sge_strdup(tl->local_conf_dir, local_conf_dir);
}

static void
set_shadow_masters_file(sge_bootstrap_tl1_t *tl, const char *shadow_masters_file) {
   tl->shadow_masters_file = sge_strdup(tl->shadow_masters_file, shadow_masters_file);
}

static void
set_sched_conf_file(sge_bootstrap_tl1_t *tl, const char *sched_conf_file) {
   tl->sched_conf_file = sge_strdup(tl->sched_conf_file, sched_conf_file);
}

static void
set_alias_file(sge_bootstrap_tl1_t *tl, const char *alias_file) {
   tl->alias_file = sge_strdup(tl->alias_file, alias_file);
}

static void
log_parameter(sge_bootstrap_tl1_t *tl) {
   DENTER(TOP_LAYER);

   DPRINTF(("ENVIRONMENT ===\n"));
   DPRINTF(("   sge_root             >%s<\n", tl->sge_root ? tl->sge_root : "NA"));
   DPRINTF(("   sge_cell             >%s<\n", tl->sge_cell ? tl->sge_cell : "NA"));
   DPRINTF(("   sge_qmaster_port     >%d<\n", tl->sge_qmaster_port));
   DPRINTF(("   sge_execd_port       >%d<\n", tl->sge_execd_port));
   DPRINTF(("   from_services        >%s<\n", tl->from_services ? "true" : "false"));

   DPRINTF(("COMPONENT ===\n"));
   DPRINTF(("   component_id         >%d<\n", tl->component_id));
   DPRINTF(("   component_name       >%s<\n", tl->component_name ? tl->component_name : "NA"));
   DPRINTF(("   thread_name          >%s<\n", tl->thread_name ? tl->thread_name : "NA"));
   DPRINTF(("   uid                  >%d<\n", tl->uid));
   DPRINTF(("   gid                  >%d<\n", tl->gid));
   DPRINTF(("   username             >%s<\n", tl->username ? tl->username : "NA"));
   DPRINTF(("   groupname            >%s<\n", tl->groupname ? tl->groupname : "NA"));
   DPRINTF(("   qmaster_internal     >%s<\n", tl->qmaster_internal ? "true" : "false"));
   DPRINTF(("   daemonized           >%s<\n", tl->daemonized ? "true" : "false"));
   DPRINTF(("   qualified_hostname   >%s<\n", tl->qualified_hostname ? tl->qualified_hostname : "NA"));
   DPRINTF(("   unqualified_hostname >%s<\n", tl->unqualified_hostname ? tl->unqualified_hostname : "NA"));
   DPRINTF(("   exit_func            >%p<\n", tl->exit_func));

   DPRINTF(("BOOTSTRAP FILE ===\n"));
   DPRINTF(("   admin_user           >%s<\n", tl->admin_user));
   DPRINTF(("   default_domain       >%s<\n", tl->default_domain));
   DPRINTF(("   ignore_fqdn          >%s<\n", tl->ignore_fqdn ? "true" : "false"));
   DPRINTF(("   spooling_method      >%s<\n", tl->spooling_method));
   DPRINTF(("   spooling_lib         >%s<\n", tl->spooling_lib));
   DPRINTF(("   spooling_params      >%s<\n", tl->spooling_params));
   DPRINTF(("   binary_path          >%s<\n", tl->binary_path));
   DPRINTF(("   qmaster_spool_dir    >%s<\n", tl->qmaster_spool_dir));
   DPRINTF(("   security_mode        >%s<\n", tl->security_mode));
   DPRINTF(("   job_spooling         >%s<\n", tl->job_spooling ? "true" : "false"));
   DPRINTF(("   listener_threads     >%d<\n", tl->listener_thread_count));
   DPRINTF(("   worker_threads       >%d<\n", tl->worker_thread_count));
   DPRINTF(("   scheduler_threads    >%d<\n", tl->scheduler_thread_count));

   DPRINTF(("FILES AND PATHS ===\n"));
   DPRINTF(("   cell_root            >%s<\n", tl->cell_root));
   DPRINTF(("   conf_file            >%s<\n", tl->bootstrap_file));
   DPRINTF(("   bootstrap_file       >%s<\n", tl->conf_file));
   DPRINTF(("   act_qmaster_file     >%s<\n", tl->act_qmaster_file));
   DPRINTF(("   acct_file            >%s<\n", tl->acct_file));
   DPRINTF(("   reporting_file       >%s<\n", tl->reporting_file));
   DPRINTF(("   local_conf_dir       >%s<\n", tl->local_conf_dir));
   DPRINTF(("   shadow_masters_file  >%s<\n", tl->shadow_masters_file));
   DPRINTF(("   alias_file           >%s<\n", tl->alias_file));

   DRETURN_VOID;
}

void
bootstrap_mt_init() {
   pthread_once(&bootstrap_once, bootstrap_thread_local_once_init);
}

static void
bootstrap_thread_local_once_init() {
   pthread_key_create(&sge_bootstrap_tl0_key, bootstrap_tl0_destroy);
   pthread_key_create(&sge_bootstrap_tl1_key, bootstrap_tl1_destroy);
}

static void
bootstrap_init_from_environment(sge_bootstrap_tl1_t *tl) {
   DENTER(TOP_LAYER);

   set_sge_root(tl, getenv("SGE_ROOT"));

   const char *sge_cell = getenv("SGE_CELL");
   set_sge_cell(tl, sge_cell != nullptr ? sge_cell : DEFAULT_CELL);

   bool from_services;
   set_sge_qmaster_port(tl, sge_get_qmaster_port(&from_services));
   set_from_services(tl, from_services);

   set_sge_execd_port(tl, sge_get_execd_port());

   set_qmaster_internal(tl, false);

   DRETURN_VOID;
}

static void
bootstrap_init_from_file(sge_bootstrap_tl1_t *tl) {
#define NUM_BOOTSTRAP 14
#define NUM_REQ_BOOTSTRAP 9
   bootstrap_entry_t name[NUM_BOOTSTRAP] = {
           {"admin_user",        true},
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
   dstring error_dstring = DSTRING_INIT;

   DENTER(TOP_LAYER);

   // early exist if we don't know where the bootstrap file is
   const char *bootstrap_file = tl->bootstrap_file;
   if (bootstrap_file != nullptr) {
      DPRINTF(("bootstrap file is %s", bootstrap_file));
   } else {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_UTI_CANNOTRESOLVEBOOTSTRAPFILE));
      DRETURN_VOID;
   }

   /* read bootstrapping information */
   if (sge_get_confval_array(bootstrap_file, NUM_BOOTSTRAP, NUM_REQ_BOOTSTRAP, name, value, &error_dstring)) {
      CRITICAL((SGE_EVENT, SFNMAX, sge_dstring_get_string(&error_dstring)));
      DRETURN_VOID;
   } else {
      u_long32 val;

      set_admin_user(tl, value[0]);
      set_default_domain(tl, value[1]);
      parse_ulong_val(nullptr, &val, TYPE_BOO, value[2], nullptr, 0);
      set_ignore_fqdn(tl, val != 0);
      set_spooling_method(tl, value[3]);
      set_spooling_lib(tl, value[4]);
      set_spooling_params(tl, value[5]);
      set_binary_path(tl, value[6]);
      set_qmaster_spool_dir(tl, value[7]);
      set_security_mode(tl, value[8]);
      if (strcmp(value[9], "") != 0) {
         parse_ulong_val(nullptr, &val, TYPE_BOO, value[9], nullptr, 0);
         set_job_spooling(tl, val != 0);
      } else {
         set_job_spooling(tl, true);
      }
      parse_ulong_val(nullptr, &val, TYPE_INT, value[10], nullptr, 0);
      set_listener_thread_count(tl, (int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[11], nullptr, 0);
      set_worker_thread_count(tl, (int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[12], nullptr, 0);
      set_scheduler_thread_count(tl, (int) val);
   }

   DRETURN_VOID;
}

static void
bootstrap_init_from_component(sge_bootstrap_tl1_t *tl) {
   // setup uid/gid and corresponding names
   char user[256];
   char group[256];
   uid_t uid = geteuid();
   gid_t gid = getegid();
   set_uid(tl, uid);
   set_gid(tl, gid);
   SGE_ASSERT(sge_uid2user(uid, user, sizeof(user), MAX_NIS_RETRIES) == 0)
   SGE_ASSERT(sge_gid2group(gid, group, sizeof(group), MAX_NIS_RETRIES) == 0)
   set_username(tl, user);
   set_groupname(tl, group);

   // setup short and long hostnames
   char *s = nullptr;
   stringT tmp_str;
   struct hostent *hent = nullptr;
   /* Fetch hostnames */
   SGE_ASSERT((gethostname(tmp_str, sizeof(tmp_str)) == 0));
   SGE_ASSERT(((hent = sge_gethostbyname(tmp_str, nullptr)) != nullptr));
   set_qualified_hostname(tl, hent->h_name);
   s = sge_dirname(hent->h_name, '.');
   set_unqualified_hostname(tl, s);
   sge_free(&s);
   /* Bad resolving in some networks leads to short qualified host names */
   if (!strcmp(tl->qualified_hostname, tl->unqualified_hostname)) {
      char tmp_addr[8];
      struct hostent *hent2 = nullptr;
      memcpy(tmp_addr, hent->h_addr, hent->h_length);
      SGE_ASSERT(((hent2 = sge_gethostbyaddr((const struct in_addr *) tmp_addr, nullptr)) != nullptr));

      set_qualified_hostname(tl, hent2->h_name);
      s = sge_dirname(hent2->h_name, '.');
      set_unqualified_hostname(tl, s);
      sge_free(&s);
      sge_free_hostent(&hent2);
   }
   sge_free_hostent(&hent);
}

static void
bootstrap_init_paths(sge_bootstrap_tl1_t *tl) {
   DENTER(TOP_LAYER);
   const char *sge_root = tl->sge_root;
   const char *sge_cell = tl->sge_cell;
   char buffer[2 * 1024];
   dstring bw;

   sge_dstring_init(&bw, buffer, sizeof(buffer));

   SGE_STRUCT_STAT sbuf{};
   if (SGE_STAT(sge_root, &sbuf)) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_SGEROOTNOTFOUND_S, sge_root));
      DRETURN_VOID;
   }

   if (!S_ISDIR(sbuf.st_mode)) {
      CRITICAL((SGE_EVENT, MSG_UTI_SGEROOTNOTADIRECTORY_S, sge_root));
      DRETURN_VOID;
   }

   /* cell_root */
   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s", sge_root, sge_cell);

   if (SGE_STAT(sge_dstring_get_string(&bw), &sbuf)) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NOSGECELL_S, sge_dstring_get_string(&bw)));
      DRETURN_VOID;
   }

   set_cell_root(tl, sge_dstring_get_string(&bw));
   const char *cell_root = tl->cell_root;

   /* common dir */
   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR);
   if (SGE_STAT(buffer, &sbuf)) {
      CRITICAL((SGE_EVENT, MSG_UTI_DIRECTORYNOTEXIST_S, buffer));
      DRETURN_VOID;
   }

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, BOOTSTRAP_FILE);
   set_bootstrap_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, CONF_FILE);
   set_conf_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, SCHED_CONF_FILE);
   set_sched_conf_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, ACT_QMASTER_FILE);
   set_act_qmaster_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, ACCT_FILE);
   set_acct_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, REPORTING_FILE);
   set_reporting_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, LOCAL_CONF_DIR);
   set_local_conf_dir(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, SHADOW_MASTERS_FILE);
   set_shadow_masters_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s", cell_root, COMMON_DIR, ALIAS_FILE);
   set_alias_file(tl, sge_dstring_get_string(&bw));

   DRETURN_VOID;
}

static void
bootstrap_tl0_init(sge_bootstrap_tl0_t *tl) {
   DENTER(TOP_LAYER);
   memset(tl, 0, sizeof(sge_bootstrap_tl0_t));
   DRETURN_VOID;
}

static void
bootstrap_tl0_destroy(void *tl) {
   auto _tl = (sge_bootstrap_tl0_t *) tl;

   // wrapping structure
   sge_free(&_tl);
}

static void
bootstrap_tl1_init(sge_bootstrap_tl1_t *tl) {
   static bool already_shown = false;

   DENTER(TOP_LAYER);
   memset(tl, 0, sizeof(sge_bootstrap_tl1_t));

   // 1) do environment setup first because file based init depends on that
   bootstrap_init_from_environment(tl);

   // 2) now paths can be derived
   bootstrap_init_paths(tl);

   // 3) now we can read the bootstrap file
   bootstrap_init_from_file(tl);

   // now everything else can be done
   bootstrap_init_from_component(tl);

   if (!already_shown) {
      log_parameter(tl);
      already_shown = true;
   }
   DRETURN_VOID;
}

static void
bootstrap_tl1_destroy(void *tl) {
   auto _tl = (sge_bootstrap_tl1_t *) tl;

   // component parameters
   sge_free(&(_tl->component_name));
   sge_free(&(_tl->thread_name));
   sge_free(&(_tl->username));
   sge_free(&(_tl->groupname));
   sge_free(&(_tl->qualified_hostname));
   sge_free(&(_tl->unqualified_hostname));

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

   // files and paths
   sge_free(&(_tl->sge_root));
   sge_free(&(_tl->cell_root));
   sge_free(&(_tl->bootstrap_file));
   sge_free(&(_tl->conf_file));
   sge_free(&(_tl->sched_conf_file));
   sge_free(&(_tl->act_qmaster_file));
   sge_free(&(_tl->acct_file));
   sge_free(&(_tl->reporting_file));
   sge_free(&(_tl->local_conf_dir));
   sge_free(&(_tl->shadow_masters_file));
   sge_free(&(_tl->alias_file));

   // wrapping structure
   sge_free(&_tl);
}

void
bootstrap_log_parameter() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   log_parameter(tl);
}

const char *
bootstrap_get_admin_user() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->admin_user;
}

void
bootstrap_set_admin_user(const char *admin_user) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_admin_user(tl, admin_user);
}

const char *
bootstrap_get_default_domain() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->default_domain;
}


void
bootstrap_set_default_domain(const char *default_domain) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_default_domain(tl, default_domain);
}

bool
bootstrap_get_ignore_fqdn() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->ignore_fqdn;
}

void
bootstrap_set_ignore_fqdn(bool ignore_fqdn) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_ignore_fqdn(tl, ignore_fqdn);
}

const char *
bootstrap_get_spooling_method() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->spooling_method;
}

void
bootstrap_set_spooling_method(const char *spooling_method) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_spooling_method(tl, spooling_method);
}

const char *
bootstrap_get_spooling_lib() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->spooling_lib;
}

void
bootstrap_set_spooling_lib(const char *spooling_lib) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_spooling_lib(tl, spooling_lib);
}

const char *
bootstrap_get_spooling_params() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->spooling_params;
}

void
bootstrap_set_spooling_params(const char *spooling_params) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_spooling_params(tl, spooling_params);
}

const char *
bootstrap_get_binary_path() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->binary_path;
}

void
bootstrap_set_binary_path(const char *binary_path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_binary_path(tl, binary_path);
}

const char *
bootstrap_get_qmaster_spool_dir() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->qmaster_spool_dir;
}

void
bootstrap_set_qmaster_spool_dir(const char *qmaster_spool_dir) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_qmaster_spool_dir(tl, qmaster_spool_dir);
}

const char *
bootstrap_get_security_mode() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->security_mode;
}

void
bootstrap_set_security_mode(const char *security_mode) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_security_mode(tl, security_mode);
}

int
bootstrap_get_listener_thread_count() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->listener_thread_count;
}

void
bootstrap_set_listener_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_listener_thread_count(tl, thread_count);
}

int
bootstrap_get_worker_thread_count() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->worker_thread_count;
}

void bootstrap_set_worker_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_worker_thread_count(tl, thread_count);
}

int bootstrap_get_scheduler_thread_count() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);

   return tl->scheduler_thread_count;
}

void bootstrap_set_scheduler_thread_count(int thread_count) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_scheduler_thread_count(tl, thread_count);
}

bool bootstrap_get_job_spooling() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->job_spooling;
}

void bootstrap_set_job_spooling(bool job_spooling) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_job_spooling(tl, job_spooling);
}

const char *
bootstrap_get_sge_root() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->sge_root;
}

void
bootstrap_set_sge_root(const char *sge_root) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_sge_root(tl, sge_root);
}


const char *
bootstrap_get_sge_cell() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->sge_cell;
}

void
bootstrap_set_sge_cell(const char *sge_cell) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_sge_cell(tl, sge_cell);
}

u_long32
bootstrap_get_sge_qmaster_port() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->sge_qmaster_port;
}

void bootstrap_set_sge_qmaster_port(u_long32 sge_qmaster_port) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_sge_qmaster_port(tl, sge_qmaster_port);
}

u_long32
bootstrap_get_sge_execd_port() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->sge_execd_port;
}

void
bootstrap_set_sge_execd_port(u_long32 sge_execd_port) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_sge_execd_port(tl, sge_execd_port);
}

bool
bootstrap_is_from_services() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->from_services;
}

void
bootstrap_set_from_services(bool from_services) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_from_services(tl, from_services);
}

bool
bootstrap_is_qmaster_internal() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->qmaster_internal;
}

void
bootstrap_set_qmaster_internal(bool qmaster_internal) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_qmaster_internal(tl, qmaster_internal);
}

bool
bootstrap_is_daemonized() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->daemonized;
}

void
bootstrap_set_daemonized(bool daemonized) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_daemonized(tl, daemonized);
}

u_long32
bootstrap_get_component_id() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->component_id;
}

void
bootstrap_set_component_id(u_long32 component_id) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_component_id(tl, component_id);
   set_component_name(tl, prognames[component_id]);
}

const char *
bootstrap_get_component_name() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->component_name;
}

const char *
bootstrap_get_thread_name() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->thread_name;
}

void
bootstrap_set_thread_name(const char *thread_name) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_thread_name(tl, thread_name);
}

uid_t
bootstrap_get_uid() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->uid;
}

void
bootstrap_set_uid(uid_t uid) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_uid(tl, uid);
}

gid_t
bootstrap_get_gid() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->gid;
}

void
bootstrap_set_gid(gid_t gid) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_gid(tl, gid);
}

const char *
bootstrap_get_username() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->username;
}

void
bootstrap_set_username(const char *username) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_username(tl, username);
}

const char *
bootstrap_get_qualified_hostname() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->qualified_hostname;
}

void
bootstrap_set_qualified_hostname(const char *qualified_hostname) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_qualified_hostname(tl, qualified_hostname);
}

const char *
bootstrap_get_unqualified_hostname() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->unqualified_hostname;
}

void
bootstrap_set_unqualified_hostname(const char *unqualified_hostname) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_unqualified_hostname(tl, unqualified_hostname);
}

sge_exit_func_t
bootstrap_get_exit_func() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->exit_func;
}

char *
bootstrap_get_log_buffer() {
   GET_SPECIFIC(sge_bootstrap_tl0_t, tl, bootstrap_tl0_init, sge_bootstrap_tl0_key);
   return tl->log_buffer;
}

void
bootstrap_set_exit_func(sge_exit_func_t exit_func) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_exit_func(tl, exit_func);
}

const char *
bootstrap_get_groupname() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->groupname;
}

void
bootstrap_set_groupname(const char *groupname) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_groupname(tl, groupname);
}

const char *
bootstrap_get_cell_root() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->cell_root;
}

void
bootstrap_set_cell_root(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_cell_root(tl, path);
}

const char *
bootstrap_get_bootstrap_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->bootstrap_file;
}

void
bootstrap_set_bootstrap_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_bootstrap_file(tl, path);
}

const char *
bootstrap_get_conf_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->conf_file;
}

void
bootstrap_set_conf_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_conf_file(tl, path);
}

const char *
bootstrap_get_sched_conf_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->sched_conf_file;
}

void
bootstrap_set_sched_conf_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_sched_conf_file(tl, path);
}

const char *
bootstrap_get_act_qmaster_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->act_qmaster_file;
}

void
bootstrap_set_act_qmaster_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_act_qmaster_file(tl, path);
}

const char *
bootstrap_get_acct_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->acct_file;
}

void
bootstrap_set_acct_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_acct_file(tl, path);
}

const char *
bootstrap_get_reporting_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->reporting_file;
}

void
bootstrap_set_reporting_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_reporting_file(tl, path);
}

const char *
bootstrap_get_local_conf_dir() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->local_conf_dir;
}

void
bootstrap_set_local_conf_dir(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_local_conf_dir(tl, path);
}

const char *
bootstrap_get_shadow_masters_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->shadow_masters_file;
}

void
bootstrap_set_shadow_masters_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_shadow_masters_file(tl, path);
}

const char *
bootstrap_get_alias_file() {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   return tl->alias_file;
}

void
bootstrap_set_alias_file(const char *path) {
   GET_SPECIFIC(sge_bootstrap_tl1_t, tl, bootstrap_tl1_init, sge_bootstrap_tl1_key);
   set_alias_file(tl, path);
}
