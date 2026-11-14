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

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"

#include "uti/sge.h"

#include "basis_types.h"

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
   lList *answer_list = nullptr;
   std::vector<std::pair<std::string, std::string>> groups;
   const bool valid = parse_usage_patterns_value(&answer_list, value, groups);

   std::string got_groups;
   for (const std::pair<std::string, std::string> &group : groups) {
      if (!got_groups.empty()) {
         got_groups += ";";
      }
      got_groups += group.first + ":" + group.second;
   }
   const int errors = static_cast<int>(lGetNumberOfElem(answer_list));

   int ret = 0;
   if (valid != expected_valid || got_groups != expected_groups || errors != expected_errors) {
      printf("FAILED: %s\n", test);
      printf("   got:      valid=%d groups=\"%s\" errors=%d\n", valid, got_groups.c_str(), errors);
      printf("   expected: valid=%d groups=\"%s\" errors=%d\n", expected_valid, expected_groups,
             expected_errors);
      answer_list_output(&answer_list);
      ret = 1;
   }
   lFreeList(&answer_list);

   return ret;
}

int
main(int argc, char *argv[]) {
   int ret = 0;

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
