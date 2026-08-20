#pragma once
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
 * @brief Cluster-wide settings read once from the `bootstrap` file
 *
 * Declares @ref ocs::Bootstrap together with the names of the files that live
 * in the cell's `common` directory.
 */

#include <bitset>

#define PATH_SEPARATOR "/"                    ///< directory separator, as a string
#define COMMON_DIR "common"                   ///< cell subdirectory holding the configuration files below
#define BOOTSTRAP_FILE "bootstrap"            ///< the file @ref ocs::Bootstrap is read from
#define SCHED_CONF_FILE "sched_configuration" ///< scheduler configuration
#define ACCT_FILE "accounting"                ///< accounting records written by qmaster
#define REPORTING_FILE "reporting"            ///< reporting records written by qmaster
#define SHADOW_MASTERS_FILE "shadow_masters"  ///< hosts allowed to take over as qmaster
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'               ///< directory separator, as a character

namespace ocs {
   /**
    * @brief The contents of the cell's `bootstrap` file
    *
    * A static-only class: there is one bootstrap configuration per process and
    * it never changes at runtime. The file is read lazily — the first getter
    * to run triggers `init_from_file()` through `std::call_once`, so no
    * explicit initialisation call is needed and concurrent first calls are
    * safe.
    *
    * Reading the file is fatal on failure: a missing file, or a missing
    * mandatory entry, logs a `CRITICAL` message and calls #sge_exit. Getters
    * therefore never report an error.
    *
    * Of the 14 entries, the first 9 are mandatory; `security_params` and the
    * four thread counts may be absent and fall back to the defaults documented
    * on the corresponding getters.
    *
    * @see #bootstrap_get_bootstrap_file for where the file itself is located
    */
   class Bootstrap {
   public:
      /**
       * @brief Security mechanisms that can be enabled in the `bootstrap` file
       *
       * Used as an index into a `std::bitset`, so several modes can be active
       * at once — the file's `security_mode` entry is a comma separated list.
       *
       * @note Must stay in sync with the `sec_mode_names` array, which
       *       `init_from_file()` asserts at compile time.
       */
      typedef enum {
         BS_SECMODE_NONE = -1,     ///< no mode; not a valid bitset index

         BS_SEC_MODE_TLS,          ///< TLS transport security
         BS_SEC_MODE_MUNGE,        ///< MUNGE authentication

         // we still have code for AFS, CSP, DCE and KERBEROS, but it is probably broken
         BS_SEC_MODE_AFS,          ///< AFS token handling; legacy, likely broken
         BS_SEC_MODE_CSP,          ///< certificate security protocol; legacy, likely broken
         BS_SEC_MODE_DCE,          ///< DCE security; legacy, likely broken
         BS_SEC_MODE_KERBEROS,     ///< Kerberos security; legacy, likely broken

         BS_SEC_MODE_NUM_ENTRIES   ///< number of valid modes, and the bitset size
      } bs_sec_mode_t;

   private:
      // bootstrap file
      static char *admin_user;
      static char *default_domain;
      static bool has_default_domain_set;
      static char *spooling_method;
      static char *spooling_lib;
      static char *spooling_params;
      static char *binary_path;
      static char *qmaster_spool_dir;

      // one bit per bs_sec_mode_t; get_security_modes() renders it for logging
      // @todo we should get rid of this string and only keep the bitset, but for now we keep it for better logging
      static std::bitset<BS_SEC_MODE_NUM_ENTRIES> security_modes;
      static int certificate_lifetime;
      static int certificate_start_offset;
      static int listener_thread_count;
      static int worker_thread_count;
      static int reader_thread_count;
      static int scheduler_thread_count;
      static bool ignore_fqdn;

      // communication_params
      static char *address_from_hostname;
      static bool trust_client_hostname;

      static const char*
      get_name_for_sec_mode(bs_sec_mode_t mode) noexcept;

      static void
      set_admin_user(const char *new_admin_user);

      static void
      set_default_domain(const char *new_default_domain);

      static void
      set_ignore_fqdn(bool new_ignore_fqdn);

      static void
      set_spooling_method(const char *new_spooling_method);

      static void
      set_spooling_params(const char *new_spooling_params);

      static void
      set_spooling_lib(const char *new_spooling_lib);

      static void
      set_binary_path(const char *new_binary_path);

      static void
      set_qmaster_spool_dir(const char *new_qmaster_spool_dir);

      static void
      set_security_mode(const char *new_security_mode);

      static void
      set_security_params(const char *new_security_params);

      static void
      set_communication_params(const char *new_communication_params);

      static void
      set_thread_count(int &thread_count, int new_thread_count, int default_thread_count, int max_thread_count);

      static void
      set_listener_thread_count(int new_thread_count);

      static void
      set_worker_thread_count(int new_thread_count);

      static void
      set_reader_thread_count(int new_thread_count);

      static void
      set_scheduler_thread_count(int new_thread_count);

      static void
      log_all_parameter();

      static void
      init_from_file();

      static void
      ensure_initialized();
   public:
      static std::string
      get_security_modes();

      static const char *
      get_admin_user();

      static const char *
      get_default_domain();

      static bool
      has_default_domain();

      static bool
      get_ignore_fqdn();

      static const char *
      get_spooling_method();

      static const char *
      get_spooling_lib();

      static const char *
      get_spooling_params();

      static const char *
      get_binary_path();

      static const char *
      get_qmaster_spool_dir();

      static bool
      has_security_mode(bs_sec_mode_t mode);

      static int
      get_cert_lifetime();

      static int
      get_cert_start_offset();

      static int
      get_listener_thread_count();

      static int
      get_worker_thread_count();

      static int
      get_reader_thread_count();

      static int
      get_scheduler_thread_count();

      static const char *
      get_address_from_hostname();

      static bool
      get_trust_client_hostname();
   };

}
