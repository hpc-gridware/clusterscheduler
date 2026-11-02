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
 * @brief Unit tests for the rules of the qmaster_params AUTO_ADD_HOSTS
 *
 * The host name side of a rule is compared the way host names are compared everywhere else in
 * the cluster, which depends on ignore_fqdn and default_domain of the bootstrap file.
 * ocs::Bootstrap reads that file once and keeps its setters private, so the settings cannot be
 * varied within one process. The test therefore takes the setting as its argument, writes a
 * throwaway bootstrap file for it, and is registered once per setting:
 *
 *    test_sgeobj_auto_add_hosts plain            ignore_fqdn false, no default_domain
 *    test_sgeobj_auto_add_hosts ignore_fqdn      ignore_fqdn true, no default_domain
 *    test_sgeobj_auto_add_hosts default_domain   ignore_fqdn false, default_domain example.com
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "cull/cull.h"

#include "uti/ocs_Bootstrap.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_AutoAddHosts.h"

static int s_fail = 0;
static char s_sge_root[128];

/** @brief Checks a condition and reports the result.
 *
 * Prints `ok` or `FAIL` with the test's id and label and counts the failure, so a run reports
 * every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/** @brief Creates a throwaway bootstrap environment.
 *
 * ocs::Bootstrap loads $SGE_ROOT/$SGE_CELL/common/bootstrap on first access and exits if it
 * cannot. Only ignore_fqdn and default_domain matter here, the other values only have to parse.
 *
 * @param ignore_fqdn value of ignore_fqdn
 * @param default_domain value of default_domain, "none" for no default domain
 * @return true if the environment could be created, false otherwise
 */
static bool
setup_bootstrap(bool ignore_fqdn, const char *default_domain) {
   char path[256];

   snprintf(s_sge_root, sizeof(s_sge_root), "/tmp/test_sgeobj_auto_add_hosts_%ld",
            static_cast<long>(getpid()));
   if (mkdir(s_sge_root, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);

   FILE *fp = fopen(path, "w");
   if (fp == nullptr) {
      return false;
   }
   fprintf(fp,
           "admin_user        none\n"
           "default_domain    %s\n"
           "ignore_fqdn       %s\n"
           "spooling_method   classic\n"
           "spooling_lib      none\n"
           "spooling_params   none\n"
           "binary_path       none\n"
           "qmaster_spool_dir none\n"
           "security_mode     none\n",
           default_domain, ignore_fqdn ? "true" : "false");
   fclose(fp);

   setenv("SGE_ROOT", s_sge_root, 1);
   setenv("SGE_CELL", "default", 1);
   return true;
}

/** @brief Removes the throwaway bootstrap environment created by setup_bootstrap(). */
static void
teardown_bootstrap() {
   char path[256];

   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);
   unlink(path);
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   rmdir(path);
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   rmdir(path);
   rmdir(s_sge_root);
}

/** @brief Parses a value of AUTO_ADD_HOSTS and matches a host against it.
 *
 * @param value the value of AUTO_ADD_HOSTS, must be valid
 * @param hostname name of the host
 * @param ip address of the host, may be nullptr
 * @param hostgroup set to the host group of the matching rule, may be nullptr
 * @return true if the value parses and one of its rules matches the host, false otherwise
 */
static bool
matches(const char *value, const char *hostname, const char *ip, std::string *hostgroup = nullptr) {
   std::vector<ocs::AutoAddHosts::Rule> rules;
   if (!ocs::AutoAddHosts::parse(value, rules, nullptr)) {
      printf("      cannot parse \"%s\"\n", value);
      return false;
   }

   std::string group;
   const bool ret = ocs::AutoAddHosts::match_rules(rules, hostname, ip, group);
   if (hostgroup != nullptr) {
      *hostgroup = group;
   }
   return ret;
}

/** @brief Tests the rules which do not depend on the host name settings of the bootstrap file.
 *
 * @param fqdn the fully qualified name of the test host
 */
static void
test_common(const char *fqdn) {
   printf("\n--- independent of ignore_fqdn and default_domain ---\n");

   const char *ip = "10.42.0.5";

   CHECK(1, "a network matches the address", matches("10.42.0.0/16", fqdn, ip));
   CHECK(2, "a foreign network does not match", !matches("10.43.0.0/16", fqdn, ip));
   CHECK(3, "a network does not match an unknown address", !matches("10.42.0.0/16", fqdn, nullptr));
   CHECK(4, "a plain address matches the address", matches("10.42.0.5", fqdn, ip));
   CHECK(5, "a foreign plain address does not match", !matches("10.42.0.6", fqdn, ip));
   CHECK(6, "a plain address is not cut down to its first octet", !matches("10.1.1.1", fqdn, ip));
   CHECK(7, "an address pattern matches the address", matches("10.42.*", fqdn, ip));
   CHECK(8, "a foreign address pattern does not match", !matches("10.43.*", fqdn, ip));

   CHECK(9, "the full name matches", matches(fqdn, fqdn, ip));
   CHECK(10, "the full name matches case-insensitively",
         matches("SERVER-00.Lab.Example.Com", fqdn, ip));
   CHECK(11, "a domain pattern matches the full name", matches("*.lab.example.com", fqdn, ip));
   CHECK(12, "a name pattern with domain matches", matches("server-0?.lab.example.com", fqdn, ip));
   CHECK(13, "a pattern matches case-insensitively", matches("SERVER-*.LAB.example.com", fqdn, ip));
   CHECK(14, "a foreign pattern does not match", !matches("gpu-*", fqdn, ip));
   CHECK(15, "a pattern with a dot in a bracket expression is not cut",
         matches("server-0[0.9].lab.example.com", fqdn, ip));

   std::string group;
   CHECK(16, "the first matching rule decides",
         matches("gpu-*:@gpu;10.42.0.0/16:@cloud;*.example.com:@other", fqdn, ip, &group) &&
         group == "@cloud");
   CHECK(17, "a rule without host group yields no host group",
         matches("10.42.0.0/16;*.example.com:@other", fqdn, ip, &group) && group.empty());
   CHECK(18, "no rule matches a foreign host", !matches("gpu-*:@gpu;10.43.0.0/16", fqdn, ip));
   CHECK(19, "a missing host name never matches", !matches("*", nullptr, ip));
}

/** @brief Tests the host name rules with ignore_fqdn false and no default domain.
 *
 * Host names are compared as they are, only case-insensitively.
 */
static void
test_plain() {
   printf("\n--- ignore_fqdn false, no default_domain ---\n");

   const char *fqdn = "server-00.lab.example.com";
   const char *ip = "10.42.0.5";

   CHECK(20, "the setting of this run is in effect",
         !ocs::Bootstrap::get_ignore_fqdn() && !ocs::Bootstrap::has_default_domain());
   test_common(fqdn);

   CHECK(21, "the short name does not match the full name", !matches("server-00", fqdn, ip));
   CHECK(22, "a short name pattern does not match the full name", !matches("server-0?", fqdn, ip));
   CHECK(23, "the short name matches a short name", matches("server-00", "Server-00", ip));
   CHECK(24, "a full name in another domain does not match",
         !matches("server-00.other.org", fqdn, ip));
}

/** @brief Tests the host name rules with ignore_fqdn true.
 *
 * Host names are compared without their domain, this is what the installation writes by
 * default (CS-3002).
 */
static void
test_ignore_fqdn() {
   printf("\n--- ignore_fqdn true, no default_domain ---\n");

   const char *fqdn = "server-00.lab.example.com";
   const char *ip = "10.42.0.5";

   CHECK(30, "the setting of this run is in effect",
         ocs::Bootstrap::get_ignore_fqdn() && !ocs::Bootstrap::has_default_domain());
   test_common(fqdn);

   CHECK(31, "the short name matches the full name", matches("server-00", fqdn, ip));
   CHECK(32, "the short name matches case-insensitively", matches("SERVER-00", fqdn, ip));
   CHECK(33, "a short name pattern matches the full name", matches("server-0?", fqdn, ip));
   CHECK(34, "a full name in another domain matches", matches("server-00.other.org", fqdn, ip));
   CHECK(35, "a foreign short name does not match", !matches("server-01", fqdn, ip));
   CHECK(36, "a short name rule names its host group",
         [&]() {
            std::string group;
            return matches("server-00:@autoadd", fqdn, ip, &group) && group == "@autoadd";
         }());
}

/** @brief Tests the host name rules with ignore_fqdn false and a default domain.
 *
 * A host name without a domain stands for the host in the default domain.
 */
static void
test_default_domain() {
   printf("\n--- ignore_fqdn false, default_domain example.com ---\n");

   const char *fqdn = "server-00.lab.example.com";
   const char *ip = "10.42.0.5";

   CHECK(40, "the setting of this run is in effect",
         !ocs::Bootstrap::get_ignore_fqdn() && ocs::Bootstrap::has_default_domain());
   test_common(fqdn);

   CHECK(41, "the short name matches the host in the default domain",
         matches("server-00", "server-00.example.com", ip));
   CHECK(42, "the full name matches a host known by its short name",
         matches("server-00.example.com", "server-00", ip));
   CHECK(43, "a pattern with the default domain matches a host known by its short name",
         matches("server-*.example.com", "server-00", ip));
   CHECK(44, "the short name does not match a host in another domain",
         !matches("server-00", fqdn, ip));
}

int
main(int argc, char *argv[]) {
   lInit(nmv);

   const char *mode = argc > 1 ? argv[1] : "";
   bool ignore_fqdn = false;
   const char *default_domain = "none";
   void (*test)() = nullptr;

   if (strcmp(mode, "plain") == 0) {
      test = test_plain;
   } else if (strcmp(mode, "ignore_fqdn") == 0) {
      ignore_fqdn = true;
      test = test_ignore_fqdn;
   } else if (strcmp(mode, "default_domain") == 0) {
      default_domain = "example.com";
      test = test_default_domain;
   } else {
      printf("usage: %s plain|ignore_fqdn|default_domain\n", argv[0]);
      return 1;
   }

   if (!setup_bootstrap(ignore_fqdn, default_domain)) {
      printf("FAIL - cannot create the throwaway bootstrap environment\n");
      teardown_bootstrap();
      return 1;
   }
   test();
   teardown_bootstrap();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
