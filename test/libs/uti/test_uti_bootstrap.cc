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

/*
 * Reads a bootstrap file and prints what was parsed out of it.
 *
 * The file cannot be named on the command line. Bootstrap derives its location as
 * $SGE_ROOT/$SGE_CELL/common/bootstrap and checks that both directories exist, so a
 * caller points SGE_ROOT and SGE_CELL at a directory tree it prepared rather than at
 * a single file. Reading goes through std::call_once, so one run covers one file.
 *
 * Every value is written to stdout as one "key=value" line. Complaints about the file
 * go to stderr through the usual logging, which keeps the two apart for a caller that
 * wants to examine either. A missing mandatory attribute never reaches this program:
 * the bootstrap code ends the process itself, and the exit status carries that.
 */

#include <cstdio>
#include <cstdlib>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"

static void
print_string(const char *key, const char *value) {
   printf("%s=%s\n", key, value != nullptr ? value : "");
}

static void
print_bool(const char *key, const bool value) {
   printf("%s=%s\n", key, value ? "true" : "false");
}

static void
print_int(const char *key, const int value) {
   printf("%s=%d\n", key, value);
}

int main(int argc, char *argv[]) {
   const char *sge_root = bootstrap_get_sge_root();
   const char *sge_cell = bootstrap_get_sge_cell();

   if (sge_root == nullptr || *sge_root == '\0') {
      fprintf(stderr, "SGE_ROOT is not set\n");
      return EXIT_FAILURE;
   }

   fprintf(stderr, "reading %s/%s/common/bootstrap\n", sge_root, sge_cell);

   // the first getter reads the file; a file that cannot be used ends the process here
   print_string("admin_user", ocs::Bootstrap::get_admin_user());
   print_string("default_domain", ocs::Bootstrap::get_default_domain());
   print_bool("has_default_domain", ocs::Bootstrap::has_default_domain());
   print_bool("ignore_fqdn", ocs::Bootstrap::get_ignore_fqdn());
   print_string("spooling_method", ocs::Bootstrap::get_spooling_method());
   print_string("spooling_lib", ocs::Bootstrap::get_spooling_lib());
   print_string("spooling_params", ocs::Bootstrap::get_spooling_params());
   print_string("binary_path", ocs::Bootstrap::get_binary_path());
   print_string("qmaster_spool_dir", ocs::Bootstrap::get_qmaster_spool_dir());

   print_string("security_modes", ocs::Bootstrap::get_security_modes().c_str());
   print_int("certificate_lifetime", ocs::Bootstrap::get_cert_lifetime());
   print_int("certificate_start_offset", ocs::Bootstrap::get_cert_start_offset());

   print_int("listener_threads", ocs::Bootstrap::get_listener_thread_count());
   print_int("worker_threads", ocs::Bootstrap::get_worker_thread_count());
   print_int("reader_threads", ocs::Bootstrap::get_reader_thread_count());
   print_int("scheduler_threads", ocs::Bootstrap::get_scheduler_thread_count());

   print_string("address_from_hostname", ocs::Bootstrap::get_address_from_hostname());
   print_bool("trust_client_hostname", ocs::Bootstrap::get_trust_client_hostname());

   return EXIT_SUCCESS;
}
