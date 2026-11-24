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

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"

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
 * @return the job, owned by the caller, or nullptr when parsing failed
 */
static lListElem *
build_job(const bool with_script) {
   const char *argv[] = {"-h", "-b", "y", "-N", "exec_file_test", "/bin/true", nullptr};
   lList *alp = nullptr;
   lList *opts = nullptr;

   opt_list_append_opts_from_qsub_cmdline(QSUB, &opts, &alp, argv, environ);
   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      lFreeList(&opts);
      return nullptr;
   }
   lFreeList(&alp);

   lListElem *job = nullptr;
   u_long32 sync_opt = SYNC_NO;
   alp = cull_parse_job_parameter(component_get_uid(), component_get_username(), bootstrap_get_cell_root(),
                                  component_get_unqualified_hostname(), component_get_qualified_hostname(), opts,
                                  &job, &sync_opt);
   lFreeList(&opts);
   if (answer_list_has_error(&alp) || job == nullptr) {
      answer_list_output(&alp);
      lFreeElem(&job);
      return nullptr;
   }
   lFreeList(&alp);

   if (!with_script) {
      lSetString(job, JB_script_file, nullptr);
   }
   lSetString(job, JB_exec_file, FORGED_EXEC_FILE);
   return job;
}

/**
 * @brief Submit a job and return the version qmaster stored
 *
 * @param job the job to submit, taken over
 * @return the job as qmaster answers it, owned by the caller, or nullptr when the submission was refused
 */
static lListElem *
submit(lListElem *job) {
   lList *lp = lCreateList("jobs", JB_Type);
   lAppendElem(lp, job);

   /* the request list is replaced by the response: the new version of the job */
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_JB_LIST, ocs::gdi::Command::SGE_GDI_ADD,
                                          ocs::gdi::SubCommand::SGE_GDI_RETURN_NEW_VERSION, &lp, nullptr, nullptr);
   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      lFreeList(&lp);
      return nullptr;
   }
   lFreeList(&alp);
   lListElem *new_job = lp != nullptr ? lDechainElem(lp, lFirstRW(lp)) : nullptr;
   lFreeList(&lp);
   return new_job;
}

/**
 * @brief Read a job back from qmaster
 *
 * @param job_id the job number
 * @return a copy of the job, owned by the caller, or nullptr when it is absent
 */
static lListElem *
fetch(const u_long32 job_id) {
   lList *lp = nullptr;
   lCondition *where = lWhere("%T(%I==%u)", JB_Type, JB_job_number, job_id);
   lEnumeration *what = lWhat("%T(%I %I %I)", JB_Type, JB_job_number, JB_exec_file, JB_script_file);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_JB_LIST, ocs::gdi::Command::SGE_GDI_GET,
                                          ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
   lFreeList(&alp);
   lFreeWhere(&where);
   lFreeWhat(&what);
   lListElem *found = lp != nullptr && lFirst(lp) != nullptr ? lDechainElem(lp, lFirstRW(lp)) : nullptr;
   lFreeList(&lp);
   return found;
}

/**
 * @brief Delete a job; errors are ignored, the job may already be gone
 *
 * @param job_id the job number, 0 does nothing
 */
static void
remove_job(const u_long32 job_id) {
   if (job_id == 0) {
      return;
   }
   const std::string id = std::to_string(job_id);
   lList *idlp = nullptr;
   lAddElemStr(&idlp, ID_str, id.c_str(), ID_Type);
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_JB_LIST, ocs::gdi::Command::SGE_GDI_DEL,
                                          ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &idlp, nullptr, nullptr);
   lFreeList(&alp);
   lFreeList(&idlp);
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
   lList *alp = nullptr;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QSUB, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   /* ---- a job without a script: qmaster must not keep the client's name --- */
   u_long32 no_script_id = 0;
   {
      lListElem *job = build_job(false);
      CHECK(1, "a job without a script is built with a forged JB_exec_file", job != nullptr);
      lListElem *stored = job != nullptr ? submit(job) : nullptr;
      CHECK(2, "qmaster accepts the job", stored != nullptr);
      if (stored != nullptr) {
         no_script_id = lGetUlong(stored, JB_job_number);
         CHECK(3, "the answer carries no script file name", is_empty(lGetString(stored, JB_exec_file)));
      }
      lFreeElem(&stored);
      lListElem *fetched = fetch(no_script_id);
      CHECK(4, "the stored job carries no script file name",
            fetched != nullptr && is_empty(lGetString(fetched, JB_exec_file)));
      lFreeElem(&fetched);
   }

   /* ---- a job with a script: qmaster names the file itself ----------------- */
   u_long32 script_id = 0;
   {
      lListElem *job = build_job(true);
      CHECK(5, "a job with a script is built with a forged JB_exec_file", job != nullptr);
      lListElem *stored = job != nullptr ? submit(job) : nullptr;
      CHECK(6, "qmaster accepts the job", stored != nullptr);
      std::string expected;
      if (stored != nullptr) {
         script_id = lGetUlong(stored, JB_job_number);
         expected = std::string(EXEC_DIR) + "/" + std::to_string(script_id);
      }
      lFreeElem(&stored);
      lListElem *fetched = fetch(script_id);
      const char *exec_file = fetched != nullptr ? lGetString(fetched, JB_exec_file) : nullptr;
      CHECK(7, "the stored job carries job_scripts/<job id>", exec_file != nullptr && expected == exec_file);
      CHECK(8, "the forged name is gone", exec_file == nullptr || strcmp(exec_file, FORGED_EXEC_FILE) != 0);
      lFreeElem(&fetched);
   }

   remove_job(no_script_id);
   remove_job(script_id);
   {
      lListElem *a = fetch(no_script_id);
      lListElem *b = fetch(script_id);
      CHECK(9, "both jobs are deleted", a == nullptr && b == nullptr);
      lFreeElem(&a);
      lFreeElem(&b);
   }

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
