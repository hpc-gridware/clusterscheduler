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

#include <pthread.h>

#include "basis_types.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"

#include "uti/msg_utilib.h"

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
        "sge_share_mon", /* 32 */
        nullptr,
};

const char *threadnames[] = {
        "main",          /* 1 */
        "listener",      /* 2 */
        "event_master",  /* 3 */
        "timer",         /* 4 */
        "worker",        /* 5 */
        "signal",        /* 6 */
        "scheduler",     /* 7 */
        "mirror",        /* 8 */
        nullptr
};


// thread local storage (level 1)
// initialization depends on data of level 0 (e.g. logging)
// TODO: data can partially be shared between threads. cleanup required.
typedef struct {
   // bootstrap file
   pthread_mutex_t mutex;
   bool init_done;
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
} sge_bootstrap_ts1_t;

static sge_bootstrap_ts1_t sge_bootstrap_tl1 = {
        PTHREAD_MUTEX_INITIALIZER, // mutex
        false, // init_done
        nullptr, // admin_user
        nullptr, // default_domain
        nullptr, // spooling_method
        nullptr, // spooling_lib
        nullptr, // spooling_params
        nullptr, // binary_path
        nullptr, // qmaster_spool_dir
        nullptr, // security_mode
        0, // listener_thread_count
        0, // worker_thread_count
        0, // scheduler_thread_count
        false, // job_spooling
        false, // ignore_fqdn
};

static void
set_admin_user(const char *admin_user) {
   sge_bootstrap_tl1.admin_user = sge_strdup(sge_bootstrap_tl1.admin_user, admin_user);
}

static void
set_default_domain(const char *default_domain) {
   sge_bootstrap_tl1.default_domain = sge_strdup(sge_bootstrap_tl1.default_domain, default_domain);
}

static void
set_ignore_fqdn(bool ignore_fqdn) {
   sge_bootstrap_tl1.ignore_fqdn = ignore_fqdn;
}

static void
set_spooling_method(const char *spooling_method) {
   sge_bootstrap_tl1.spooling_method = sge_strdup(sge_bootstrap_tl1.spooling_method, spooling_method);
}

static void
set_spooling_params(const char *spooling_params) {
   sge_bootstrap_tl1.spooling_params = sge_strdup(sge_bootstrap_tl1.spooling_params, spooling_params);
}

static void
set_spooling_lib(const char *spooling_lib) {
   sge_bootstrap_tl1.spooling_lib = sge_strdup(sge_bootstrap_tl1.spooling_lib, spooling_lib);
}

static void
set_binary_path(const char *binary_path) {
   sge_bootstrap_tl1.binary_path = sge_strdup(sge_bootstrap_tl1.binary_path, binary_path);
}

static void
set_qmaster_spool_dir(const char *qmaster_spool_dir) {
   sge_bootstrap_tl1.qmaster_spool_dir = sge_strdup(sge_bootstrap_tl1.qmaster_spool_dir, qmaster_spool_dir);
}

static void
set_security_mode(const char *security_mode) {
   sge_bootstrap_tl1.security_mode = sge_strdup(sge_bootstrap_tl1.security_mode, security_mode);
}

static void
set_listener_thread_count(int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   sge_bootstrap_tl1.listener_thread_count = thread_count;
}

static void
set_worker_thread_count(int thread_count) {
   if (thread_count <= 0) {
      thread_count = 2;
   } else if (thread_count > 16) {
      thread_count = 16;
   }
   sge_bootstrap_tl1.worker_thread_count = thread_count;
}

static void
set_scheduler_thread_count(int thread_count) {
   if (thread_count <= 0) {
      thread_count = 0;
   } else if (thread_count > 1) {
      thread_count = 1;
   }
   sge_bootstrap_tl1.scheduler_thread_count = thread_count;
}

static void
set_job_spooling(bool job_spooling) {
   sge_bootstrap_tl1.job_spooling = job_spooling;
}

static void
bootstrap_log_ts1_parameter() {
   DENTER(TOP_LAYER);

   DPRINTF("BOOTSTRAP FILE ===\n");
   DPRINTF("   admin_user           >%s<\n", sge_bootstrap_tl1.admin_user);
   DPRINTF("   default_domain       >%s<\n", sge_bootstrap_tl1.default_domain);
   DPRINTF("   ignore_fqdn          >%s<\n", sge_bootstrap_tl1.ignore_fqdn ? "true" : "false");
   DPRINTF("   spooling_method      >%s<\n", sge_bootstrap_tl1.spooling_method);
   DPRINTF("   spooling_lib         >%s<\n", sge_bootstrap_tl1.spooling_lib);
   DPRINTF("   spooling_params      >%s<\n", sge_bootstrap_tl1.spooling_params);
   DPRINTF("   binary_path          >%s<\n", sge_bootstrap_tl1.binary_path);
   DPRINTF("   qmaster_spool_dir    >%s<\n", sge_bootstrap_tl1.qmaster_spool_dir);
   DPRINTF("   security_mode        >%s<\n", sge_bootstrap_tl1.security_mode);
   DPRINTF("   job_spooling         >%s<\n", sge_bootstrap_tl1.job_spooling ? "true" : "false");
   DPRINTF("   listener_threads     >%d<\n", sge_bootstrap_tl1.listener_thread_count);
   DPRINTF("   worker_threads       >%d<\n", sge_bootstrap_tl1.worker_thread_count);
   DPRINTF("   scheduler_threads    >%d<\n", sge_bootstrap_tl1.scheduler_thread_count);

   DRETURN_VOID;
}

static void
bootstrap_init_from_file() {
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
   const char *bootstrap_file = bootstrap_get_bootstrap_file();
   if (bootstrap_file != nullptr) {
      DPRINTF("bootstrap file is %s\n", bootstrap_file);
   } else {
      CRITICAL(SFNMAX, MSG_UTI_CANNOTRESOLVEBOOTSTRAPFILE);
      sge_exit(1);
   }

   /* read bootstrapping information */
   if (sge_get_confval_array(bootstrap_file, NUM_BOOTSTRAP, NUM_REQ_BOOTSTRAP, name, value, &error_dstring)) {
      CRITICAL(SFNMAX, sge_dstring_get_string(&error_dstring));
      sge_exit(1);
   } else {
      u_long32 val;

      set_admin_user(value[0]);
      set_default_domain(value[1]);
      parse_ulong_val(nullptr, &val, TYPE_BOO, value[2], nullptr, 0);
      set_ignore_fqdn(val != 0);
      set_spooling_method(value[3]);
      set_spooling_lib(value[4]);
      set_spooling_params(value[5]);
      set_binary_path(value[6]);
      set_qmaster_spool_dir(value[7]);
      set_security_mode(value[8]);
      if (strcmp(value[9], "") != 0) {
         parse_ulong_val(nullptr, &val, TYPE_BOO, value[9], nullptr, 0);
         set_job_spooling(val != 0);
      } else {
         set_job_spooling(true);
      }
      parse_ulong_val(nullptr, &val, TYPE_INT, value[10], nullptr, 0);
      set_listener_thread_count((int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[11], nullptr, 0);
      set_worker_thread_count((int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[12], nullptr, 0);
      set_scheduler_thread_count((int) val);
   }

   DRETURN_VOID;
}

static void
bootstrap_ts1_init() {
   DENTER(TOP_LAYER);
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   bootstrap_init_from_file();
   sge_bootstrap_tl1.init_done = true;
   bootstrap_log_ts1_parameter();
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   DRETURN_VOID;
}

// returns true if all thread local data structures are initialized
// but has not the side effect to cause the initialisation.
bool
bootstrap_is_initialized() {
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   bool init_done = sge_bootstrap_tl1.init_done;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   return init_done;
}

const char *
bootstrap_get_admin_user() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *admin_user = sge_bootstrap_tl1.admin_user;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return admin_user;
}

const char *
bootstrap_get_default_domain() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *default_domain = sge_bootstrap_tl1.default_domain;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return default_domain;
}

bool
bootstrap_get_ignore_fqdn() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   bool ignore_fqdn = sge_bootstrap_tl1.ignore_fqdn;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return ignore_fqdn;
}

const char *
bootstrap_get_spooling_method() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *spooling_method = sge_bootstrap_tl1.spooling_method;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return spooling_method;
}

const char *
bootstrap_get_spooling_lib() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *spooling_lib = sge_bootstrap_tl1.spooling_lib;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return spooling_lib;
}

const char *
bootstrap_get_spooling_params() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *spooling_params = sge_bootstrap_tl1.spooling_params;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return spooling_params;
}

const char *
bootstrap_get_binary_path() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *binary_path = sge_bootstrap_tl1.binary_path;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return binary_path;
}

const char *
bootstrap_get_qmaster_spool_dir() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *qmaster_spool_dir = sge_bootstrap_tl1.qmaster_spool_dir;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return qmaster_spool_dir;
}

const char *
bootstrap_get_security_mode() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   const char *security_mode = sge_bootstrap_tl1.security_mode;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   // this is thread safe because the string will never change after initialized
   return security_mode;
}

int
bootstrap_get_listener_thread_count() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   int listener_thread_count = sge_bootstrap_tl1.listener_thread_count;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   return listener_thread_count;
}

int
bootstrap_get_worker_thread_count() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   int worker_thread_count = sge_bootstrap_tl1.worker_thread_count;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   return worker_thread_count;
}

int bootstrap_get_scheduler_thread_count() {
   if (!bootstrap_is_initialized()) {
      bootstrap_ts1_init();
   }
   pthread_mutex_lock(&sge_bootstrap_tl1.mutex);
   int scheduler_thread_count = sge_bootstrap_tl1.scheduler_thread_count;
   pthread_mutex_unlock(&sge_bootstrap_tl1.mutex);
   return scheduler_thread_count;
}
