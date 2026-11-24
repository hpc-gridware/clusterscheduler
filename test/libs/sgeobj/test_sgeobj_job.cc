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
 * @brief Unit tests for jobs in `libs/sgeobj`
 *
 * CS-3134: ocs_job_exec_file_is_valid() and ocs_jobscript_key_is_valid() accept only the script file name
 * qmaster gives a job, `job_scripts/<job id>`. Everything else is a name a client could have sent.
 */


#include "basis_types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_job.h"

#include "ocs_module_test.h"

/**
 * @brief Only the script file name qmaster gives a job is accepted (CS-3134)
 *
 * @return nothing; CHECK records the results
 */
static void
test_exec_file_is_valid() {
   CHECK(1, "job_scripts/<job id> of the job is accepted", ocs_job_exec_file_is_valid("job_scripts/42", 42));
   CHECK(2, "the name of another job is refused", !ocs_job_exec_file_is_valid("job_scripts/43", 42) &&
                                                   !ocs_job_exec_file_is_valid("job_scripts/420", 42) &&
                                                   !ocs_job_exec_file_is_valid("job_scripts/042", 42));
   CHECK(3, "absolute paths and path tricks are refused",
         !ocs_job_exec_file_is_valid("/etc/passwd", 42) &&
            !ocs_job_exec_file_is_valid("/tmp/job_scripts/42", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/../42", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/42/x", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/42/", 42) &&
            !ocs_job_exec_file_is_valid("./job_scripts/42", 42));
   CHECK(4, "an empty name, nullptr and job id 0 are refused",
         !ocs_job_exec_file_is_valid("", 42) && !ocs_job_exec_file_is_valid(nullptr, 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/0", 0));

   CHECK(5, "a spooling key with the job's own script is accepted",
         ocs_jobscript_key_is_valid("JOBSCRIPT:42", "job_scripts/42"));
   CHECK(6, "a spooling key with a forged script or a bad job id is refused",
         !ocs_jobscript_key_is_valid("JOBSCRIPT:42", "/etc/passwd") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:41", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:42x", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT42", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid(nullptr, "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:42", nullptr));
}

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   lInit(nmv);

   return ocs::module_test::run(argc, argv, {
      {"JOB-01", "script file name of a job (CS-3134)",
       "only job_scripts/<job id> is accepted as the script file name of a job, by name and from a "
       "job script spooling key",
       {"check the job's own name", "check names of other jobs, absolute paths and path tricks",
        "check an empty name, nullptr and job id 0", "check spooling keys"},
       "the job's own name is accepted, every other name is refused",
       {}, {1, 2, 3, 4, 5, 6}, test_exec_file_is_valid},
   });
}
