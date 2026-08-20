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

/** @file
 * @brief Reads the cell's `bootstrap` file into @ref ocs::Bootstrap
 *
 * The setters and the file parser are private and run exactly once, on the
 * first getter call. Everything public in here is a getter that returns a
 * value already read from the file.
 */

#include <atomic>
#include <cassert>
#include <bitset>
#include <cstring>
#include <mutex>
#include <string_view>
#include <vector>
#include <array>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/msg_utilib.h"

#include "sge.h"

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
bool ocs::Bootstrap::ignore_fqdn = false;
char *ocs::Bootstrap::address_from_hostname = nullptr;
bool ocs::Bootstrap::trust_client_hostname = false;

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

#define MIN_CERTIFICATE_LIFETIME (120)                ///< shortest accepted certificate lifetime, in seconds
#define MAX_CERTIFICATE_LIFETIME (365 * 24 * 60 * 60) ///< longest accepted certificate lifetime, in seconds (one year)
#define MIN_CERTIFICATE_START_OFFSET (-300)           ///< earliest accepted certificate start, in seconds before now
#define DEFAULT_CERTIFICATE_START_OFFSET (-10)        ///< certificate start used when none is configured
#define MAX_CERTIFICATE_START_OFFSET (0)              ///< latest accepted certificate start: never in the future
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

/**
 * Matches one comma separated parameter against a "<key>=" prefix and hands back
 * the value belonging to it.
 *
 * The key is written once and its length is known at compile time. A match also
 * settles where the value starts: the prefix ends in the separator, so what follows
 * it is the value and no second search is needed.
 */
static bool
param_matches(const char *param, const std::string_view key, const char **value) {
   if (strncasecmp(param, key.data(), key.size()) != 0) {
      return false;
   }

   *value = param + key.size();
   return true;
}

/**
 * Reads the comma separated communication_params of the bootstrap file.
 *
 * Both parameters relax a check the communication library performs on incoming
 * connections, so both default to off and only take effect where an administrator
 * has asked for them.
 *
 * An unknown parameter is reported rather than passed over quietly. A setting that
 * is accepted without complaint but has no effect is hard to tell apart from one
 * that works, and costs whoever wrote it a long detour.
 */
void
ocs::Bootstrap::set_communication_params(const char *new_communication_params) {
   DENTER(TOP_LAYER);

   // "none" is how this file says that nothing is configured - default_domain and
   // spooling_lib use it the same way, and an administrator writing it here should not
   // be told that "none" is a parameter nobody knows
   if (new_communication_params == nullptr ||
       SGE_STRCASECMP(new_communication_params, NONE_STR) == 0) {
      DRETURN_VOID;
   }

   saved_vars_s *context = nullptr;
   const char *param = sge_strtok_r(new_communication_params, ",", &context);
   while (param != nullptr) {
      const char *str_value;

      if (param_matches(param, "address_from_hostname=", &str_value)) {
         if (sge_hostname_format_valid(str_value)) {
            address_from_hostname = sge_strdup(address_from_hostname, str_value);
         } else {
            WARNING(MSG_UTI_INVALIDADDRESSFORMAT_S, str_value);
         }
      } else if (param_matches(param, "trust_client_hostname=", &str_value)) {
         uint32_t value;
         parse_ulong_val(nullptr, &value, CEntry::Type::BOOL, str_value, nullptr, 0);
         trust_client_hostname = value != 0;
      } else {
         WARNING(MSG_UTI_UNKNOWNCOMMUNICATIONPARAM_S, param);
      }
      // next param
      param = sge_strtok_r(nullptr, ",", &context);
   }
   sge_free_saved_vars(context);

   DRETURN_VOID;
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
   // Deliberately not set_thread_count(): that helper reads 0 as "no value given,
   // take the default", which is right for the pools but wrong here. 0 is a value
   // in the documented range of scheduler_threads and means "run no scheduler", so
   // it has to survive.
   if (new_thread_count < 0) {
      new_thread_count = 0;
   } else if (new_thread_count > 1) {
      new_thread_count = 1;
   }
   scheduler_thread_count = new_thread_count;
}

/**
 * @brief The enabled security modes, rendered for logging
 *
 * @return a comma separated list of mode names in #bs_sec_mode_t order, or
 *         `"none"` when no mode is enabled
 */
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
   DPRINTF("   listener_threads          >%d<\n", listener_thread_count);
   DPRINTF("   worker_threads            >%d<\n", worker_thread_count);
   DPRINTF("   reader_threads            >%d<\n", reader_thread_count);
   DPRINTF("   scheduler_threads         >%d<\n", scheduler_thread_count);
   DPRINTF("   address_from_hostname     >%s<\n", address_from_hostname);
   DPRINTF("   trust_client_hostname     >%s<\n", trust_client_hostname ? "true" : "false");

   DRETURN_VOID;
}

void
ocs::Bootstrap::init_from_file() {
   DENTER(TOP_LAYER);

/// @cond   function local: entries read from the bootstrap file, and how many of them are mandatory
#define NUM_BOOTSTRAP 15
#define NUM_REQ_BOOTSTRAP 9
/// @endcond
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

           {"listener_threads",  false},
           {"worker_threads",    false},
           {"reader_threads",    false},
           {"scheduler_threads", false},
           {"communication_params", false},
   };
   char value[NUM_BOOTSTRAP][4097];
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
      uint32_t val;

      set_admin_user(value[0]);
      set_default_domain(value[1]);
      parse_ulong_val(nullptr, &val, CEntry::Type::BOOL, value[2], nullptr, 0);
      set_ignore_fqdn(val != 0);
      set_spooling_method(value[3]);
      set_spooling_lib(value[4]);

      set_spooling_params(value[5]);
      set_binary_path(value[6]);
      set_qmaster_spool_dir(value[7]);
      set_security_mode(value[8]);
      set_security_params(value[9]);

      parse_ulong_val(nullptr, &val, CEntry::Type::INT, value[10], nullptr, 0);
      set_listener_thread_count((int) val);
      parse_ulong_val(nullptr, &val, CEntry::Type::INT, value[11], nullptr, 0);
      set_worker_thread_count((int) val);
      parse_ulong_val(nullptr, &val, CEntry::Type::INT, value[12], nullptr, 0);
      set_reader_thread_count((int) val);
      // Note that an absent entry parses to 0 and therefore means "run no
      // scheduler", the same as an explicit 0. That is how it has always been,
      // and it does not surface in practice because the installation writes the
      // entry and the upgrade adds it.
      parse_ulong_val(nullptr, &val, CEntry::Type::INT, value[13], nullptr, 0);
      set_scheduler_thread_count((int) val);

      set_communication_params(value[14]);
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

/**
 * @brief The user the cluster's daemons administer files as
 *
 * @return the `admin_user` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_admin_user() {
   ensure_initialized();
   return admin_user;
}

/**
 * @brief The domain appended to unqualified hostnames
 *
 * @return the `default_domain` entry, which may be `"none"`; owned by the
 *         class, do not free. Use #has_default_domain to test it rather than
 *         comparing against `"none"` here.
 */
const char *
ocs::Bootstrap::get_default_domain() {
   ensure_initialized();
   return default_domain;
}

/**
 * @brief Is a usable default domain configured?
 *
 * @return true when `default_domain` is set and is not `"none"`
 */
bool ocs::Bootstrap::has_default_domain() {
   ensure_initialized();
   return has_default_domain_set;
}

/**
 * @brief Are hostnames compared on their short name only?
 *
 * @return the `ignore_fqdn` entry
 */
bool
ocs::Bootstrap::get_ignore_fqdn() {
   ensure_initialized();
   return ignore_fqdn;
}

/**
 * @brief The spooling method qmaster uses, e.g. `classic` or `berkeleydb`
 *
 * @return the `spooling_method` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_spooling_method() {
   ensure_initialized();
   return spooling_method;
}

/**
 * @brief The shared library implementing the spooling method
 *
 * @return the `spooling_lib` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_spooling_lib() {
   ensure_initialized();
   return spooling_lib;
}

/**
 * @brief Arguments handed to the spooling library at startup
 *
 * @return the `spooling_params` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_spooling_params() {
   ensure_initialized();
   return spooling_params;
}

/**
 * @brief The directory the cluster's binaries are installed in
 *
 * @return the `binary_path` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_binary_path() {
   ensure_initialized();
   return binary_path;
}

/**
 * @brief The directory qmaster spools into
 *
 * @return the `qmaster_spool_dir` entry; owned by the class, do not free
 */
const char *
ocs::Bootstrap::get_qmaster_spool_dir() {
   ensure_initialized();
   return qmaster_spool_dir;
}

/**
 * @brief Is one specific security mode enabled?
 *
 * Several modes can be enabled at once, so this is the only correct way to
 * ask — do not parse the string from #get_security_modes.
 *
 * @param mode the mode to test
 * @return true when @p mode is enabled; false for an out-of-range @p mode,
 *         including #BS_SECMODE_NONE
 */
bool
ocs::Bootstrap::has_security_mode(bs_sec_mode_t mode) {
   ensure_initialized();
   auto idx = static_cast<std::size_t>(mode);
   return (idx < BS_SEC_MODE_NUM_ENTRIES) ? security_modes.test(idx) : false;
}

/**
 * @brief How long a generated certificate stays valid
 *
 * Read from `certificate_lifetime=` in the `security_params` entry and clamped
 * to [#MIN_CERTIFICATE_LIFETIME, #MAX_CERTIFICATE_LIFETIME].
 *
 * @return the lifetime in seconds; one year when not configured
 */
int
ocs::Bootstrap::get_cert_lifetime() {
   ensure_initialized();
   return certificate_lifetime;
}

/**
 * @brief How far before "now" a generated certificate becomes valid
 *
 * The offset absorbs clock skew between the hosts of the cluster. Read from
 * `certificate_start_offset=` in the `security_params` entry and clamped to
 * [#MIN_CERTIFICATE_START_OFFSET, #MAX_CERTIFICATE_START_OFFSET].
 *
 * @return the offset in seconds; negative or zero, and
 *         #DEFAULT_CERTIFICATE_START_OFFSET when not configured
 */
int
ocs::Bootstrap::get_cert_start_offset() {
   ensure_initialized();
   return certificate_start_offset;
}

/**
 * @brief Size of the qmaster listener thread pool
 *
 * @return the `listener_threads` entry, 4 when unset or not positive, capped
 *         at 32
 */
int
ocs::Bootstrap::get_listener_thread_count() {
   ensure_initialized();
   return listener_thread_count;
}

/**
 * @brief Size of the qmaster worker thread pool
 *
 * @return the `worker_threads` entry, 4 when unset or not positive, capped
 *         at 32
 */
int
ocs::Bootstrap::get_worker_thread_count() {
   ensure_initialized();
   return worker_thread_count;
}

/**
 * @brief Size of the qmaster reader thread pool
 *
 * @return the `reader_threads` entry, 4 when unset or not positive, capped
 *         at 32
 */
int
ocs::Bootstrap::get_reader_thread_count() {
   ensure_initialized();
   return reader_thread_count;
}

/**
 * @brief Size of the scheduler thread pool
 *
 * @return 1 when a scheduler is to run, 0 when `scheduler_threads` switches it
 *         off. Clamped to a maximum of one, since a second scheduler thread is
 *         not supported. An absent entry gives 0, like an explicit 0.
 */
int
ocs::Bootstrap::get_scheduler_thread_count() {
   ensure_initialized();
   return scheduler_thread_count;
}

/**
 * Format from which the address of a client is derived instead of resolving its
 * host name, or nullptr when no format is configured and names are resolved as usual.
 */
const char *
ocs::Bootstrap::get_address_from_hostname() {
   ensure_initialized();
   return address_from_hostname;
}

/**
 * Whether the host name a client announces is accepted as its identity without
 * being checked against the address the connection arrives from.
 */
bool
ocs::Bootstrap::get_trust_client_hostname() {
   ensure_initialized();
   return trust_client_hostname;
}
