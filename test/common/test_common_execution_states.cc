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
 * get_sstate_description() maps the failure code a shepherd reports to the phrase a user is
 * shown: in the failed line of qacct -j, in the administrator mail, in the sge_qmaster messages
 * file and in the error string the execd builds from a job report.
 *
 * Its table is searched by code rather than indexed by it, so a code can only be missed by
 * adding a define without adding an entry - which had happened to three of them, so that they
 * came out as "invalid execution state" instead of naming the problem. These tests require
 * every code the shepherd can report to describe itself, which is what a new code would have
 * to do as well.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "execution_states.h"

static int failures = 0;

/** @brief Require that one error code is described
 *
 * @param code the SSTATE_* or ESSTATE_* value
 * @param name the name of the define, so that a failure says which code is missing
 */
static void
check_described(int code, const char *name) {
   const char *description = get_sstate_description(code);

   if (description != nullptr && description[0] != '\0' &&
       strcmp(description, "invalid execution state") != 0) {
      printf("ok    [%3d] %s: \"%s\"\n", code, name, description);
   } else {
      printf("FAIL  [%3d] %s has no description, it reads \"%s\"\n", code, name,
             description == nullptr ? "(null)" : description);
      failures++;
   }
}

/** @brief Require that a code which no shepherd reports is reported as unknown
 *
 * @param code the value to look up
 * @param what what is being looked up, for the output
 */
static void
check_unknown(int code, const char *what) {
   const char *description = get_sstate_description(code);

   if (description != nullptr && strcmp(description, "invalid execution state") == 0) {
      printf("ok    [%3d] %s is reported as unknown\n", code, what);
   } else {
      printf("FAIL  [%3d] %s reads \"%s\", where \"invalid execution state\" was expected\n",
             code, what, description == nullptr ? "(null)" : description);
      failures++;
   }
}

/** @brief Every code a shepherd can report has to name the step which failed */
static void
test_every_code_is_described() {
   // the three which were missing; they are in the range below as well, but a defect which
   // comes back has to be recognisable as that defect
   check_described(ESSTATE_PTF_CANT_GET_PIDS, "ESSTATE_PTF_CANT_GET_PIDS");
   check_described(SSTATE_AFS_PROBLEM, "SSTATE_AFS_PROBLEM");
   check_described(SSTATE_ADD_GRP_SET_ERROR, "SSTATE_ADD_GRP_SET_ERROR");

   // and the whole range, so that a code added later is noticed
   for (int code = SSTATE_FAILURE_BEFORE_JOB; code <= MAX_SSTATE; code++) {
      check_described(code, "the code");
   }
   check_described(SSTATE_FAILURE_AFTER_JOB, "SSTATE_FAILURE_AFTER_JOB");
}

/** @brief A code outside the table is reported as unknown, and a job which ran through has
 *         nothing to name
 */
static void
test_codes_outside_the_table() {
   check_unknown(MAX_SSTATE + 1, "the code above the shepherd states");
   check_unknown(SSTATE_FAILURE_AFTER_JOB - 1, "a code below the after job state");
   check_unknown(SSTATE_FAILURE_AFTER_JOB + 1, "a code above the after job state");
   check_unknown(-1, "a negative code");

   // 0 is STATUS_OK: the job ran through, so there is no step to name
   const char *description = get_sstate_description(0);
   if (description != nullptr && description[0] == '\0') {
      printf("ok    [  0] STATUS_OK describes nothing\n");
   } else {
      printf("FAIL  [  0] STATUS_OK reads \"%s\", where the empty string was expected\n",
             description == nullptr ? "(null)" : description);
      failures++;
   }
}

int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_common_execution_states");

   test_every_code_is_described();
   test_codes_outside_the_table();

   if (failures == 0) {
      printf("\nPASS - 0 failure(s)\n");
   } else {
      printf("\nFAIL - %d failure(s)\n", failures);
   }

   DRETURN(failures == 0 ? 0 : 1);
}
