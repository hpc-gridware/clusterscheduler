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

#include <bitset>

#define PATH_SEPARATOR "/"
#define COMMON_DIR "common"
#define BOOTSTRAP_FILE "bootstrap"
#define CONF_FILE "configuration"
#define SCHED_CONF_FILE "sched_configuration"
#define ACCT_FILE "accounting"
#define REPORTING_FILE "reporting"
#define LOCAL_CONF_DIR "local_conf"
#define SHADOW_MASTERS_FILE "shadow_masters"
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'

namespace ocs {
   class Bootstrap {
   public:
      // security modes, must be in sync with the bitset in sge_bootstrap_ts1_t and the sec_mode_names array
      typedef enum {
         BS_SECMODE_NONE = -1,

         BS_SEC_MODE_TLS,
         BS_SEC_MODE_MUNGE,

         // we still have code for AFS, CSP, DCE and KERBEROS, but it is probably broken
         BS_SEC_MODE_AFS,
         BS_SEC_MODE_CSP,
         BS_SEC_MODE_DCE,
         BS_SEC_MODE_KERBEROS,

         // number of possible entries
         BS_SEC_MODE_NUM_ENTRIES
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

      // this string is only used for logging and debugging, the actual modes are stored in the bitset below
      // @todo we should get rid of this string and only keep the bitset, but for now we keep it for better logging
      static std::bitset<BS_SEC_MODE_NUM_ENTRIES> security_modes;
      static int certificate_lifetime;
      static int certificate_start_offset;
      static int listener_thread_count;
      static int worker_thread_count;
      static int reader_thread_count;
      static int scheduler_thread_count;
      static bool job_spooling;
      static bool ignore_fqdn;

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
      set_job_spooling(bool new_job_spooling);


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
   };

}
