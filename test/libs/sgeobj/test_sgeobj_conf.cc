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

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "tom/ocs_ElementCore.h"

#include "uti/sge.h"

#include "basis_types.h"

/**
 * @brief Build a CONF element holding the given entry names in the given order
 * @param names the entry names, ended by nullptr
 * @return the configuration
 */
static ocs::tom::ElementPtr<ocs::tom::CONF>
make_conf(const char *names[]) {
   auto conf = ocs::tom::ElementPtr<ocs::tom::CONF>::create();

   conf->set<CONF_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
   for (int i = 0; names[i] != nullptr; i++) {
      ocs::tom::Element<ocs::tom::CF> ep = conf->emplace_back_in<CONF_entries, CF_name>(names[i]);
      ep.set<CF_value>("value");
   }

   return conf;
}

/**
 * @brief Compare the entry names of a configuration against the expected ones
 *
 * Prints both sequences if they differ.
 *
 * @param test the name of the test case, printed on failure
 * @param conf the configuration
 * @param expected the expected entry names, ended by nullptr
 * @return the number of errors found (0 or 1)
 */
static int
check_conf(const char *test, const ocs::tom::ConstElement<ocs::tom::CONF> conf, const char *expected[]) {
   ocs::tom::ConstList<ocs::tom::CF> entries = conf.get<CONF_entries>();
   ocs::tom::ConstElement<ocs::tom::CF> ep;
   int i = 0;

   for (ep = entries.front(); ep; ep = ep.next()) {
      if (expected[i] == nullptr || strcmp(ep.get<CF_name>(), expected[i]) != 0) {
         printf("FAILED: %s\n", test);
         printf("   got:     ");
         for (ep = entries.front(); ep; ep = ep.next()) {
            printf("%s ", ep.get<CF_name>());
         }
         printf("\n   expected:");
         for (int j = 0; expected[j] != nullptr; j++) {
            printf("%s ", expected[j]);
         }
         printf("\n");
         return 1;
      }
      i++;
   }

   if (expected[i] != nullptr) {
      printf("FAILED: %s - configuration is missing entries, starting at %s\n", test, expected[i]);
      return 1;
   }

   return 0;
}

/**
 * @brief Parse a usage_patterns value and compare the outcome against the expected one.
 *
 * @param[in] test name of the test case, printed on failure
 * @param[in] value the usage_patterns value to parse
 * @param[in] expected_valid expected return value of parse_usage_patterns_value()
 * @param[in] expected_groups expected groups, written as name:expression;... ("" for none)
 * @param[in] expected_errors expected number of error messages in the answer list
 * @return the number of errors found (0 or 1)
 */
static int
check_usage_patterns(const char *test, const char *value, bool expected_valid,
                     const char *expected_groups, int expected_errors) {
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   std::vector<std::pair<std::string, std::string>> groups;
   const bool valid = parse_usage_patterns_value(&answer_list, value, groups);

   std::string got_groups;
   for (const std::pair<std::string, std::string> &group : groups) {
      if (!got_groups.empty()) {
         got_groups += ";";
      }
      got_groups += group.first + ":" + group.second;
   }
   const int errors = static_cast<int>(answer_list->size());

   int ret = 0;
   if (valid != expected_valid || got_groups != expected_groups || errors != expected_errors) {
      printf("FAILED: %s\n", test);
      printf("   got:      valid=%d groups=\"%s\" errors=%d\n", valid, got_groups.c_str(), errors);
      printf("   expected: valid=%d groups=\"%s\" errors=%d\n", expected_valid, expected_groups,
             expected_errors);
      answer_list_output(&answer_list);
      ret = 1;
   }
   answer_list.reset();

   return ret;
}

/**
 * @brief Run the checks of the configuration ordering and of the usage patterns
 * @param argc the number of arguments, unused
 * @param argv the arguments, unused
 * @return the number of failed checks
 */
int
main(int argc, char *argv[]) {
   int ret = 0;

   // a configuration coming in in an arbitrary order is brought into the defined order
   {
      static const char *in[] = {"topology_file", "loglevel", "mailer", "execd_spool_dir", nullptr};
      static const char *out[] = {"execd_spool_dir", "mailer", "loglevel", "topology_file", nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      ret += check_conf("arbitrary order is sorted", *conf, out);
      conf.reset();
   }

   // a configuration which is already sorted is left alone
   {
      static const char *in[] = {"execd_spool_dir", "mailer", "loglevel", "topology_file", nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      ret += check_conf("sorted order is kept", *conf, in);
      conf.reset();
   }

   // entries which are not known are kept and appended at the end, in the incoming order
   {
      static const char *in[] = {"zzz_unknown", "loglevel", "aaa_unknown", "mailer", nullptr};
      static const char *out[] = {"mailer", "loglevel", "zzz_unknown", "aaa_unknown", nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      ret += check_conf("unknown entries are appended", *conf, out);
      conf.reset();
   }

   // a name appearing more than once keeps all its occurrences, next to each other; a debug build refuses the
   // duplicate at the append (N-H-18), so the checks are suspended for this case
   {
      const ocs::tom::UniqueCheckSuspension suspension;
      static const char *in[] = {"loglevel", "mailer", "loglevel", nullptr};
      static const char *out[] = {"mailer", "loglevel", "loglevel", nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      ret += check_conf("duplicate names are kept", *conf, out);
      conf.reset();
   }

   // entry names are matched case insensitively, just like in merge_configuration()
   {
      static const char *in[] = {"Topology_File", "MAILER", nullptr};
      static const char *out[] = {"MAILER", "Topology_File", nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      ret += check_conf("names are matched case insensitively", *conf, out);
      conf.reset();
   }

   // a configuration without any entry does not cause any harm
   {
      static const char *in[] = {nullptr};
      auto conf = make_conf(in);

      conf_sort_entries(*conf);
      if (conf->get<CONF_entries>()) {
         printf("FAILED: empty configuration - entry list should still be empty\n");
         ret++;
      }
      conf.reset();
   }

   // usage_patterns: valid groups are returned in configuration order
   ret += check_usage_patterns("usage_patterns valid", "agent:agent_*|tok*;gpu:nvidia*", true,
                               "agent:agent_*|tok*;gpu:nvidia*", 0);
   ret += check_usage_patterns("usage_patterns nullptr", nullptr, true, "", 0);
   ret += check_usage_patterns("usage_patterns empty", "", true, "", 0);
   ret += check_usage_patterns("usage_patterns empty entries", ";gpu:nvidia*;;", true, "gpu:nvidia*", 0);
   ret += check_usage_patterns("usage_patterns names which only contain a reserved name",
                               "rusage_x:a*;my_usage:b*;eusage2:c*", true,
                               "rusage_x:a*;my_usage:b*;eusage2:c*", 0);

   // usage_patterns: reserved group names are rejected, also in other case, the rest is kept
   ret += check_usage_patterns("usage_patterns reserved names",
                               "agent:agent_*;usage:usg_*;rusage:rus_*;eusage:eus_*;gpu:nvidia*", false,
                               "agent:agent_*;gpu:nvidia*", 3);
   ret += check_usage_patterns("usage_patterns reserved names in other case", "RUsage:a*;EUSAGE:b*;Usage:c*",
                               false, "", 3);

   // usage_patterns: a group name given twice is rejected, the first group is kept
   ret += check_usage_patterns("usage_patterns duplicate name", "gpu:nvidia*;gpu:amd*", false,
                               "gpu:nvidia*", 1);
   ret += check_usage_patterns("usage_patterns duplicate name in other case", "gpu:nvidia*;GPU:amd*", false,
                               "gpu:nvidia*", 1);

   // usage_patterns: entries which are not name:expression are rejected
   ret += check_usage_patterns("usage_patterns syntax", "gpu;:nvidia*;gpu:;agent:agent_*", false,
                               "agent:agent_*", 3);

   if (ret == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: test NOT solved!\n");
   }

   return ret;
}
