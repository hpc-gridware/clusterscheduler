/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <atomic>
#include <cassert>
#include <bitset>
#include <cstring>
#include <mutex>
#include <vector>

#include "basis_types.h"

#include "uti/ocs_Bootstrap.h"
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
   "python_client", /* 33 */
   nullptr,
};

const char *threadnames[] = {
   "main",          /* 1 */
   "listener",      /* 2 */
   "event-master",  /* 3 */
   "timer",         /* 4 */
   "worker",        /* 5 */
   "signal",        /* 6 */
   "scheduler",     /* 7 */
   "mirror",        /* 8 */
   "reader",        /* 9 */
   nullptr
};

static const char *sec_mode_names[] = {
   "tls",
   "munge",
   "afs",
   "csp",
   "dce",
   "kerberos",
   NONE_STR
};

static std::once_flag bootstrap_once;
static std::atomic<bool> bootstrap_initialized{false};

char *ocs::Bootstrap::admin_user = nullptr;
char *ocs::Bootstrap::default_domain = nullptr;
bool ocs::Bootstrap::has_default_domain_set = false;
char *ocs::Bootstrap::spooling_method = nullptr;
char *ocs::Bootstrap::spooling_lib = nullptr;
char *ocs::Bootstrap::spooling_params = nullptr;
char *ocs::Bootstrap::binary_path = nullptr;
char *ocs::Bootstrap::qmaster_spool_dir = nullptr;
std::bitset<ocs::Bootstrap::BS_SEC_MODE_NUM_ENTRIES> ocs::Bootstrap::security_modes = std::bitset<BS_SEC_MODE_NUM_ENTRIES>(false);
int ocs::Bootstrap::certificate_lifetime = 365 * 24 * 60 * 60;
int ocs::Bootstrap::certificate_start_offset = -10;
int ocs::Bootstrap::listener_thread_count = 0;
int ocs::Bootstrap::worker_thread_count = 0;
int ocs::Bootstrap::reader_thread_count = 0;
int ocs::Bootstrap::scheduler_thread_count = 0;
bool ocs::Bootstrap::job_spooling = false;
bool ocs::Bootstrap::ignore_fqdn = false;

const char*
ocs::Bootstrap::get_name_for_sec_mode(bs_sec_mode_t mode) noexcept {
   auto idx = static_cast<std::size_t>(mode);

   if (idx < BS_SEC_MODE_NUM_ENTRIES) {
      return sec_mode_names[idx];
   }

   return "unknown";
}

void
ocs::Bootstrap::set_admin_user(const char *new_admin_user) {
   admin_user = sge_strdup(admin_user, new_admin_user);
}

void
ocs::Bootstrap::set_default_domain(const char *new_default_domain_new) {
   default_domain = sge_strdup(default_domain, new_default_domain_new);
   has_default_domain_set = default_domain != nullptr && SGE_STRCASECMP(default_domain, NONE_STR) != 0;
}

void
ocs::Bootstrap::set_ignore_fqdn(const bool new_ignore_fqdn) {
   ignore_fqdn = new_ignore_fqdn;
}

void
ocs::Bootstrap::set_spooling_method(const char *new_spooling_method) {
   spooling_method = sge_strdup(spooling_method, new_spooling_method);
}

void
ocs::Bootstrap::set_spooling_params(const char *new_spooling_params) {
   spooling_params = sge_strdup(spooling_params, new_spooling_params);
}

void
ocs::Bootstrap::set_spooling_lib(const char *new_spooling_lib) {
   spooling_lib = sge_strdup(spooling_lib, new_spooling_lib);
}

void
ocs::Bootstrap::set_binary_path(const char *new_binary_path) {
   binary_path = sge_strdup(binary_path, new_binary_path);
}

void
ocs::Bootstrap::set_qmaster_spool_dir(const char *new_qmaster_spool_dir) {
   qmaster_spool_dir = sge_strdup(qmaster_spool_dir, new_qmaster_spool_dir);
}

void
ocs::Bootstrap::set_security_mode(const char *new_security_mode) {
   DENTER(TOP_LAYER);

   saved_vars_s *context = nullptr;
   const char *mode = sge_strtok_r(new_security_mode, ",", &context);
   while (mode != nullptr) {
      if (strcmp(mode, "tls") == 0) {
         security_modes[BS_SEC_MODE_TLS] = true;
      } else if (strcmp(mode, "munge") == 0) {
         security_modes[BS_SEC_MODE_MUNGE] = true;
      } else if (strcmp(mode, "afs") == 0) {
         security_modes[BS_SEC_MODE_AFS] = true;
      } else if (strcmp(mode, "csp") == 0) {
         security_modes[BS_SEC_MODE_CSP] = true;
      } else if (strcmp(mode, "dce") == 0) {
         security_modes[BS_SEC_MODE_DCE] = true;
      } else if (strcmp(mode, "kerberos") == 0) {
         security_modes[BS_SEC_MODE_KERBEROS] = true;
      } else if (strcasecmp(mode, NONE_STR) == 0) {
         // "none" is a special case, it means that no security mode is enabled, so we just ignore it
         ;
      } else {
         // @todo Add error handling
         DPRINTF("invalid security mode %s\n", mode);
      }
      // next mode
      mode = sge_strtok_r(nullptr, ",", &context);
   }
   sge_free_saved_vars(context);

   DRETURN_VOID;
}

#define MIN_CERTIFICATE_LIFETIME (120)
#define MAX_CERTIFICATE_LIFETIME (365 * 24 * 60 * 60)
#define MIN_CERTIFICATE_START_OFFSET (-300)
#define DEFAULT_CERTIFICATE_START_OFFSET (-10)
#define MAX_CERTIFICATE_START_OFFSET (0)
void
ocs::Bootstrap::set_security_params(const char *new_security_params) {
   DENTER(TOP_LAYER);

   saved_vars_s *context = nullptr;
   const char *param = sge_strtok_r(new_security_params, ",", &context);
   while (param != nullptr) {
      if (strncasecmp(param, "certificate_lifetime=", strlen("certificate_lifetime=")) == 0) {
         const char *str_value = std::strchr(param, '=');
         if (str_value != nullptr) {
            int value = atoi(str_value + 1);
            if (value < MIN_CERTIFICATE_LIFETIME) {
               value = MIN_CERTIFICATE_LIFETIME;
            }
            if (value > MAX_CERTIFICATE_LIFETIME) {
               value = MAX_CERTIFICATE_LIFETIME;
            }
            certificate_lifetime = value;
         }
      } else if (strncasecmp(param, "certificate_start_offset=", strlen("certificate_start_offset=")) == 0) {
         const char *str_value = std::strchr(param, '=');
         if (str_value != nullptr) {
            int value = atoi(str_value + 1);
            if (value < MIN_CERTIFICATE_START_OFFSET) {
               value = MIN_CERTIFICATE_START_OFFSET;
            }
            if (value > MAX_CERTIFICATE_START_OFFSET) {
               value = MAX_CERTIFICATE_START_OFFSET;
            }
            certificate_start_offset = value;
         }
      } else {
         DPRINTF("invalid security parameter %s\n", param);
      }
      // next param
      param = sge_strtok_r(nullptr, ",", &context);
   }
   sge_free_saved_vars(context);
}

void
ocs::Bootstrap::set_thread_count(int &thread_count, int new_thread_count, int default_thread_count, int max_thread_count) {
   if (new_thread_count <= 0) {
      thread_count = default_thread_count;
   } else if (new_thread_count > max_thread_count) {
      thread_count = max_thread_count;
   } else {
      thread_count = new_thread_count;
   }
}

// IMPORTANT NOTE: The maximum thread count must not exceed FIFO_LOCK_QUEUE_LENGTH in sge_lock.h, otherwise the
// locking mechanism for the thread pools will break down and cause deadlocks

void
ocs::Bootstrap::set_listener_thread_count(int new_thread_count) {
   set_thread_count(listener_thread_count, new_thread_count, 4, 32);
}

void
ocs::Bootstrap::set_worker_thread_count(int new_thread_count) {
   set_thread_count(worker_thread_count, new_thread_count, 4, 32);
}

void
ocs::Bootstrap::set_reader_thread_count(int new_thread_count) {
   set_thread_count(reader_thread_count, new_thread_count, 4, 32);
}


void
ocs::Bootstrap::set_scheduler_thread_count(int new_thread_count) {
   set_thread_count(scheduler_thread_count, new_thread_count, 1, 1);
}

void
ocs::Bootstrap::set_job_spooling(const bool new_job_spooling) {
   job_spooling = new_job_spooling;
}

std::string
ocs::Bootstrap::get_security_modes() {
   DENTER(TOP_LAYER);
   std::string result;

   for (std::size_t i = 0; i < BS_SEC_MODE_NUM_ENTRIES; i++) {
      if (security_modes.test(i)) {
         if (!result.empty())
            result += ",";
         result += get_name_for_sec_mode(static_cast<bs_sec_mode_t>(i));
      }
   }

   if (result.empty()) {
      DRETURN(NONE_STR);
   }
   DRETURN(result);
}

void
ocs::Bootstrap::log_all_parameter() {
   DENTER(TOP_LAYER);

   DPRINTF("BOOTSTRAP FILE ===\n");
   DPRINTF("   admin_user                >%s<\n", admin_user);
   DPRINTF("   default_domain            >%s<\n", default_domain);
   DPRINTF("   ignore_fqdn               >%s<\n", ignore_fqdn ? "true" : "false");
   DPRINTF("   spooling_method           >%s<\n", spooling_method);
   DPRINTF("   spooling_lib              >%s<\n", spooling_lib);
   DPRINTF("   spooling_params           >%s<\n", spooling_params);
   DPRINTF("   binary_path               >%s<\n", binary_path);
   DPRINTF("   qmaster_spool_dir         >%s<\n", qmaster_spool_dir);
   DPRINTF("   security_modes            >%s<\n", get_security_modes().c_str());
   DPRINTF("   certificate_lifetime      >%d<\n", certificate_lifetime);
   DPRINTF("   certificate_start_offset  >%d<\n", certificate_start_offset);
   DPRINTF("   job_spooling              >%s<\n", job_spooling ? "true" : "false");
   DPRINTF("   listener_threads          >%d<\n", listener_thread_count);
   DPRINTF("   worker_threads            >%d<\n", worker_thread_count);
   DPRINTF("   reader_threads            >%d<\n", reader_thread_count);
   DPRINTF("   scheduler_threads         >%d<\n", scheduler_thread_count);

   DRETURN_VOID;
}

void
ocs::Bootstrap::init_from_file() {
#define NUM_BOOTSTRAP 15
#define NUM_REQ_BOOTSTRAP 9
   DENTER(TOP_LAYER);
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
           {"security_params",   false},
           {"job_spooling",      false},

           {"listener_threads",  false},
           {"worker_threads",    false},
           {"reader_threads",    false},
           {"scheduler_threads", false},
   };
   char value[NUM_BOOTSTRAP][1025];
   dstring error_dstring = DSTRING_INIT;

   // ensure that the number of entries in the sec_mode_names array matches the number of entries in the bs_sec_mode_t enum
   // and that both match the size of the bitset in sge_bootstrap_ts1_t
   static_assert(std::size(sec_mode_names) == BS_SEC_MODE_NUM_ENTRIES + 1, "sec_mode_names must match BS_SEC_MODE_NUM_ENTRIES");

   for (int i = 0; i < NUM_BOOTSTRAP; ++i) {
      *value[i] = '\0';
   }

   // early exist if we don't know where the bootstrap file is
   const char *bootstrap_file = bootstrap_get_bootstrap_file();
   if (bootstrap_file == nullptr) {
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
      set_security_params(value[9]);
      if (strcmp(value[10], "") != 0) {
         parse_ulong_val(nullptr, &val, TYPE_BOO, value[10], nullptr, 0);
         set_job_spooling(val != 0);
      } else {
         set_job_spooling(true);
      }

      parse_ulong_val(nullptr, &val, TYPE_INT, value[11], nullptr, 0);
      set_listener_thread_count((int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[12], nullptr, 0);
      set_worker_thread_count((int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[13], nullptr, 0);
      set_reader_thread_count((int) val);
      parse_ulong_val(nullptr, &val, TYPE_INT, value[14], nullptr, 0);
      set_scheduler_thread_count((int) val);
   }

   log_all_parameter();

   // mark bootstrap as initialized
   bootstrap_initialized.store(true, std::memory_order_release);
   DRETURN_VOID;
}

inline void
ocs::Bootstrap::ensure_initialized() {
   // if bootstrap is already initialized, we can skip the call_once and just return
   if (!bootstrap_initialized.load(std::memory_order_acquire)) {
      std::call_once(bootstrap_once, init_from_file);
   }
}

const char *
ocs::Bootstrap::get_admin_user() {
   ensure_initialized();
   return admin_user;
}

const char *
ocs::Bootstrap::get_default_domain() {
   ensure_initialized();
   return default_domain;
}

bool ocs::Bootstrap::has_default_domain() {
   ensure_initialized();
   return has_default_domain_set;
}

bool
ocs::Bootstrap::get_ignore_fqdn() {
   ensure_initialized();
   return ignore_fqdn;
}

const char *
ocs::Bootstrap::get_spooling_method() {
   ensure_initialized();
   return spooling_method;
}

const char *
ocs::Bootstrap::get_spooling_lib() {
   ensure_initialized();
   return spooling_lib;
}

const char *
ocs::Bootstrap::get_spooling_params() {
   ensure_initialized();
   return spooling_params;
}

const char *
ocs::Bootstrap::get_binary_path() {
   ensure_initialized();
   return binary_path;
}

const char *
ocs::Bootstrap::get_qmaster_spool_dir() {
   ensure_initialized();
   return qmaster_spool_dir;
}

bool
ocs::Bootstrap::has_security_mode(bs_sec_mode_t mode) {
   ensure_initialized();
   auto idx = static_cast<std::size_t>(mode);
   return (idx < BS_SEC_MODE_NUM_ENTRIES) ? security_modes.test(idx) : false;
}

int
ocs::Bootstrap::get_cert_lifetime() {
   ensure_initialized();
   return certificate_lifetime;
}

int
ocs::Bootstrap::get_cert_start_offset() {
   ensure_initialized();
   return certificate_start_offset;
}

int
ocs::Bootstrap::get_listener_thread_count() {
   ensure_initialized();
   return listener_thread_count;
}

int
ocs::Bootstrap::get_worker_thread_count() {
   ensure_initialized();
   return worker_thread_count;
}

int
ocs::Bootstrap::get_reader_thread_count() {
   ensure_initialized();
   return reader_thread_count;
}

int
ocs::Bootstrap::get_scheduler_thread_count() {
   ensure_initialized();
   return scheduler_thread_count;
}
