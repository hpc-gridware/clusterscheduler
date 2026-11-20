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
 * @brief Unit tests for the classic job spooling in `libs/spool/classic`
 *
 * CS-3120: a task is written to a dot file which is then renamed. When the
 * rename fails, or the daemon dies between the two, the dot file stays behind.
 * Reading the job back must skip it - before the fix the dot file was taken
 * for a task, its id parsed as 0, and the whole job was restored without any
 * of its enrolled tasks.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "cull/cull.h"
#include "uti/sge_spool.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "spool/classic/read_write_job.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/sge_answer.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
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

static constexpr uint32_t JOB_ID = 42;
static constexpr uint32_t BLOCKED_JOB_ID = 43;

/**
 * @brief Build an array job with two enrolled tasks
 * @param job_id the job number
 * @return the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
make_array_job(const uint32_t job_id) {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(job_id);
   job->set<JB_job_name>("cs3120");
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::RN> structure;
   range_list_parse_from_string(structure, &alp, "1-2", false, true, INF_NOT_ALLOWED);
   alp.reset();
   ocs::tom::set<JB_ja_structure>(*job, std::move(structure));
   auto tasks = ocs::tom::ListPtr<ocs::tom::JAT>::create("ja_tasks");
   for (uint32_t task_id = 1; task_id <= 2; task_id++) {
      auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
      task->set<JAT_task_number>(task_id);
      tasks->push_back(std::move(task));
   }
   job->set<JB_ja_tasks>(std::move(tasks));
   return job;
}

/**
 * @brief Copy a file
 * @param from the source
 * @param to the target
 * @return true when the copy was written
 */
static bool
copy_file(const char *from, const char *to) {
   std::ifstream in(from, std::ios::binary);
   std::ofstream out(to, std::ios::binary);
   out << in.rdbuf();
   return in.good() && out.good();
}

// ---------------------------------------------------------------------------
// job_list_read_from_disk()
// ---------------------------------------------------------------------------

/**
 * @brief A dot file left behind in the task directory does not cost the job its tasks (CS-3120)
 */
static void
test_leftover_dot_file() {
   printf("\n--- classic job spooling: leftover dot file ---\n");

   auto job = make_array_job(JOB_ID);
   const int written = job_write_spool_file(*job, 0, nullptr, SPOOL_DEFAULT);
   CHECK(1, "the job with its tasks is spooled", written == 0);

   // the dot file of task 2, as a failed rename or a crash before it leaves it
   char task_file[SGE_PATH_MAX];
   char dot_file[SGE_PATH_MAX];
   sge_get_file_path(task_file, sizeof(task_file), TASK_SPOOL_DIR_AS_FILE, FORMAT_DEFAULT, SPOOL_DEFAULT,
                     JOB_ID, 2, nullptr);
   sge_get_file_path(dot_file, sizeof(dot_file), TASK_SPOOL_DIR_AS_FILE, FORMAT_DOT_FILENAME, SPOOL_DEFAULT,
                     JOB_ID, 2, nullptr);
   CHECK(2, "the dot file is created", copy_file(task_file, dot_file));

   lList *jobs = nullptr;
   job_list_read_from_disk(&jobs, "jobs", 0, SPOOL_DEFAULT, nullptr);
   const auto read = ocs::tom::ConstList<ocs::tom::JB>::from_typed(jobs).find_first<JB_job_number>(JOB_ID);
   CHECK(3, "the job is read back", static_cast<bool>(read));
   CHECK(4, "the job keeps both of its tasks", read && read.get<JB_ja_tasks>().size() == 2);
   lFreeList(&jobs);
}

/**
 * @brief A task file that cannot be renamed into place is reported as a failure (CS-3120)
 *
 * A non-empty directory where the file of task 2 belongs makes the rename fail.
 */
static void
test_failed_rename_is_reported() {
   printf("\n--- classic job spooling: failed rename ---\n");

   char task_file[SGE_PATH_MAX];
   sge_get_file_path(task_file, sizeof(task_file), TASK_SPOOL_DIR_AS_FILE, FORMAT_DEFAULT, SPOOL_DEFAULT,
                     BLOCKED_JOB_ID, 2, nullptr);
   std::error_code error;
   std::filesystem::create_directories(task_file, error);
   std::ofstream(std::string(task_file) + "/blocker") << "x";

   auto job = make_array_job(BLOCKED_JOB_ID);
   const int written = job_write_spool_file(*job, 0, nullptr, SPOOL_DEFAULT);
   CHECK(5, "a task that cannot be renamed into place is reported", written != 0);
}

/**
 * @brief Entry point: works in a fresh temporary directory, the spool tree is created relative to it
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   // one task per file, so the tasks are spooled in task files of their own
   setenv("SGE_MAX_TASKS_PER_FILE", "1", 1);

   char dir_template[] = "/tmp/test_spool_classic_job.XXXXXX";
   const char *dir = mkdtemp(dir_template);
   if (dir == nullptr || chdir(dir) != 0) {
      printf("FAIL - cannot create the temporary spool directory\n");
      return 1;
   }

   test_leftover_dot_file();
   test_failed_rename_is_reported();

   std::error_code error;
   if (chdir("/") != 0 || std::filesystem::remove_all(dir, error) == 0) {
      printf("note: the temporary spool directory %s was not removed\n", dir);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
