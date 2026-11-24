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
 * @brief qmaster sets the script file name of a submitted job itself (CS-3134)
 *
 * `JB_exec_file` names the file qmaster spools a job script to, and execd writes and later deletes the script
 * under that name. No client sets the field. qmaster used to keep a value sent by the client when the job had
 * no script, and then read, wrote and deleted that file as the admin user.
 *
 * This test submits held jobs through GDI, the way a modified client would, with a forged `JB_exec_file`, and
 * reads back what qmaster stored:
 * - a job without a script: the field must be empty,
 * - a job with a script (`-b y`): the field must be `job_scripts/<job id>`.
 *
 * The jobs are held and deleted at the end; they never run. Needs a running qmaster: registered DISABLED for
 * a plain ctest run, installed into testbin/ and driven by the testsuite check module_test_cluster.
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "ocs_client_parse.h"
#include "parse_job_cull.h"

#include "basis_types.h"

extern char **environ;

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so a run reports every problem
 * rather than stopping at the first.
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

/** @brief The forged script file name; the jobs are held, nothing ever touches it */
static constexpr const char *FORGED_EXEC_FILE = "/tmp/test_qmaster_job_exec_file.forged";

/**
 * @brief Build a held binary job the way qsub does
 *
 * @param with_script true: keep the command as `JB_script_file`; false: drop it, so the job has no script
 * @return the job, or an empty handle when parsing failed
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
build_job(const bool with_script) {
   const char *argv[] = {"-h", "-b", "y", "-N", "exec_file_test", "/bin/true", nullptr};
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::SPA> opts;

   opt_list_append_opts_from_qsub_cmdline(QSUB, opts, &alp, argv, environ);
   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      return {};
   }

   ocs::tom::ElementPtr<ocs::tom::JB> job;
   uint32_t sync_opt = SYNC_NO;
   alp = cull_parse_job_parameter(component_get_uid(), component_get_username(), bootstrap_get_cell_root(),
                                  component_get_unqualified_hostname(), component_get_qualified_hostname(), *opts,
                                  job, &sync_opt);
   if (answer_list_has_error(&alp) || !job) {
      answer_list_output(&alp);
      return {};
   }

   if (!with_script) {
      job->set<JB_script_file>(nullptr);
   }
   job->set<JB_exec_file>(FORGED_EXEC_FILE);
   return job;
}

/**
 * @brief Submit a job and return the version qmaster stored
 *
 * @param job the job to submit, taken over
 * @return the job as qmaster answers it, or an empty handle when the submission was refused
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
submit(ocs::tom::ElementPtr<ocs::tom::JB> &&job) {
   auto lp = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   lp->push_back(std::move(job));

   /* the request list is replaced by the response: the new version of the job */
   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::ADD,
                                        ocs::gdi::SubCommand::RETURN_NEW_VERSION, lp);
   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      return {};
   }
   const auto new_job = lp ? lp->front() : ocs::tom::ConstElement<ocs::tom::JB>();
   return new_job ? new_job.clone() : ocs::tom::ElementPtr<ocs::tom::JB>();
}

/**
 * @brief Read a job back from qmaster
 *
 * @param job_id the job number
 * @return a copy of the job, or an empty handle when it is absent
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
fetch(const uint32_t job_id) {
   ocs::tom::ListPtr<ocs::tom::JB> lp;
   const auto where = ocs::tom::field<JB_job_number>() == job_id;
   const auto what = ocs::tom::what<JB_job_number, JB_exec_file, JB_script_file>();

   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::GET,
                                        ocs::gdi::SubCommand::NONE, lp, where, what);
   alp.reset();
   const auto found = lp ? lp->front() : ocs::tom::ConstElement<ocs::tom::JB>();
   return found ? found.clone() : ocs::tom::ElementPtr<ocs::tom::JB>();
}

/**
 * @brief Delete a job; errors are ignored, the job may already be gone
 *
 * @param job_id the job number, 0 does nothing
 */
static void
remove_job(const uint32_t job_id) {
   if (job_id == 0) {
      return;
   }
   const std::string id = std::to_string(job_id);
   auto idlp = ocs::tom::ListPtr<ocs::tom::ID>::create("job id list");
   idlp->emplace_back<ID_str>(id.c_str());
   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::DEL,
                                        ocs::gdi::SubCommand::NONE, idlp);
   alp.reset();
}

/**
 * @brief Whether a stored script file name is empty
 *
 * @param exec_file the stored name
 * @return true for nullptr and ""
 */
static bool
is_empty(const char *exec_file) {
   return exec_file == nullptr || exec_file[0] == '\0';
}

// ---------------------------------------------------------------------------

/**
 * @brief Run the checks against a running qmaster
 * @param argc unused
 * @param argv unused
 * @return 0 if all checks pass, 1 otherwise, 99 without a cluster
 */
int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_job_exec_file");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QSUB, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   /* ---- a job without a script: qmaster must not keep the client's name --- */
   uint32_t no_script_id = 0;
   {
      auto job = build_job(false);
      CHECK(1, "a job without a script is built with a forged JB_exec_file", job != nullptr);
      auto stored = job ? submit(std::move(job)) : ocs::tom::ElementPtr<ocs::tom::JB>();
      CHECK(2, "qmaster accepts the job", stored != nullptr);
      if (stored) {
         no_script_id = stored->get<JB_job_number>();
         CHECK(3, "the answer carries no script file name", is_empty(stored->get<JB_exec_file>()));
      }
      auto fetched = fetch(no_script_id);
      CHECK(4, "the stored job carries no script file name",
            fetched != nullptr && is_empty(fetched->get<JB_exec_file>()));
   }

   /* ---- a job with a script: qmaster names the file itself ----------------- */
   uint32_t script_id = 0;
   {
      auto job = build_job(true);
      CHECK(5, "a job with a script is built with a forged JB_exec_file", job != nullptr);
      auto stored = job ? submit(std::move(job)) : ocs::tom::ElementPtr<ocs::tom::JB>();
      CHECK(6, "qmaster accepts the job", stored != nullptr);
      std::string expected;
      if (stored) {
         script_id = stored->get<JB_job_number>();
         expected = std::string(EXEC_DIR) + "/" + std::to_string(script_id);
      }
      auto fetched = fetch(script_id);
      const char *exec_file = fetched ? fetched->get<JB_exec_file>() : nullptr;
      CHECK(7, "the stored job carries job_scripts/<job id>",
            exec_file != nullptr && expected == exec_file);
      CHECK(8, "the forged name is gone", exec_file == nullptr || strcmp(exec_file, FORGED_EXEC_FILE) != 0);
   }

   remove_job(no_script_id);
   remove_job(script_id);
   CHECK(9, "both jobs are deleted", fetch(no_script_id) == nullptr && fetch(script_id) == nullptr);

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
