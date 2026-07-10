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
 * CS-2404 regression probe. Submits a single job with a caller-provided
 * DRMAA_NATIVE_SPECIFICATION into user hold, prints the job id on stdout,
 * and exits. The caller (typically the testsuite check at
 * testsuite/src/checktree/system_tests/drmaa/precedence) is responsible for
 * inspecting the resulting job via `qstat -j <jobid>` to verify that the
 * request precedence rules (sge_request → script special comments →
 * DRMAA_NATIVE_SPECIFICATION → DRMAA API attributes) were honoured, and for
 * deleting the job afterwards.
 *
 * The test intentionally leaves the job in user hold so the caller has time
 * to inspect it without racing the scheduler.
 *
 * Usage:
 *   test_drmaa_cs2404 <script_path> <native_spec>
 *
 * <script_path>  Path to a script the job template's REMOTE_COMMAND points at.
 *                For CS-2404 coverage this is typically a small shell script
 *                that carries "#$ -q ..." special comments the caller wants
 *                folded into the precedence chain.
 * <native_spec>  Value passed verbatim as DRMAA_NATIVE_SPECIFICATION. Should
 *                include "-h" (or the DRMAA API hold is set explicitly here)
 *                and whatever -q / -l switches exercise the scenario under
 *                test. Pass an empty string to submit without any native spec.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "drmaa.h"

static char errorbuf[DRMAA_ERROR_STRING_BUFFER];

int main(int argc, char **argv) {
   if (argc != 3) {
      fprintf(stderr, "Usage: %s <script_path> <native_spec>\n", argv[0]);
      return 1;
   }
   const char *script_path = argv[1];
   const char *native_spec = argv[2];

   int drmaa_errno;
   while ((drmaa_errno = drmaa_init(nullptr, errorbuf, sizeof(errorbuf) - 1))
          == DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE) {
      sleep(1);
   }
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_init failed: %s\n", errorbuf);
      return 1;
   }

   drmaa_job_template_t *jt = nullptr;
   if (drmaa_allocate_job_template(&jt, errorbuf, sizeof(errorbuf) - 1)
       != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_allocate_job_template failed: %s\n", errorbuf);
      drmaa_exit(nullptr, 0);
      return 1;
   }

   drmaa_set_attribute(jt, DRMAA_REMOTE_COMMAND, script_path, nullptr, 0);
   drmaa_set_attribute(jt, DRMAA_JOIN_FILES, "y", nullptr, 0);
   drmaa_set_attribute(jt, DRMAA_OUTPUT_PATH, ":/dev/null", nullptr, 0);

   /* Submit the job in user hold so the caller can inspect its effective
    * request set before the scheduler dispatches it. Using the DRMAA API
    * knob rather than folding "-h" into the native spec keeps this behaviour
    * orthogonal to whatever precedence scenario the caller is exercising.
    */
   if (drmaa_set_attribute(jt, DRMAA_JS_STATE, "drmaa_hold",
                           errorbuf, sizeof(errorbuf) - 1)
       != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_set_attribute(DRMAA_JS_STATE) failed: %s\n", errorbuf);
      drmaa_delete_job_template(jt, nullptr, 0);
      drmaa_exit(nullptr, 0);
      return 1;
   }

   if (native_spec != nullptr && *native_spec != '\0') {
      if (drmaa_set_attribute(jt, DRMAA_NATIVE_SPECIFICATION, native_spec,
                              errorbuf, sizeof(errorbuf) - 1)
          != DRMAA_ERRNO_SUCCESS) {
         fprintf(stderr, "drmaa_set_attribute(DRMAA_NATIVE_SPECIFICATION) failed: %s\n",
                 errorbuf);
         drmaa_delete_job_template(jt, nullptr, 0);
         drmaa_exit(nullptr, 0);
         return 1;
      }
   }

   char jobid[DRMAA_JOBNAME_BUFFER];
   if (drmaa_run_job(jobid, sizeof(jobid) - 1, jt, errorbuf, sizeof(errorbuf) - 1)
       != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_run_job failed: %s\n", errorbuf);
      drmaa_delete_job_template(jt, nullptr, 0);
      drmaa_exit(nullptr, 0);
      return 1;
   }

   drmaa_delete_job_template(jt, nullptr, 0);

   /* Single line on stdout is the contract with the caller. Anything else
    * goes to stderr so `qstat -j <jobid>` parsing stays trivial. */
   printf("%s\n", jobid);

   if (drmaa_exit(errorbuf, sizeof(errorbuf) - 1) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_exit failed: %s\n", errorbuf);
      return 1;
   }

   return 0;
}
