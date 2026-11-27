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
 * CS-3123: job_get_smallest_unenrolled_task_id() and
 * job_get_biggest_unenrolled_task_id() look at the five hold lists of the
 * unenrolled tasks (n_h, u_h, o_h, s_h, a_h). The array dependency hold list
 * a_h had its condition inverted: its id replaced the result instead of being
 * compared with it, so qdel and the removal of an advance reservation left
 * tasks behind when tasks were held by -hold_jid_ad.
 *
 * CS-3134: ocs_job_exec_file_is_valid() and ocs_jobscript_key_is_valid() accept only the script file name
 * qmaster gives a job.
 */

#include <cstdio>
#include <cstring>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

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

/**
 * @brief Set a task id range list of a job
 * @tparam F the range list field, e.g. JB_ja_n_h_ids
 * @param job the job
 * @param ranges the ranges, e.g. "1-5"
 */
template <auto F>
static void
set_ranges(const ocs::tom::Element<ocs::tom::JB> job, const char *ranges) {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::RN> list;
   range_list_parse_from_string(list, &alp, ranges, false, false, INF_NOT_ALLOWED);
   alp.reset();
   job.set<F>(std::move(list));
}

// ---------------------------------------------------------------------------
// job_get_smallest/biggest_unenrolled_task_id()
// ---------------------------------------------------------------------------

/**
 * @brief The extremes over all five hold lists, including a_h (CS-3123)
 */
static void
test_unenrolled_extremes() {
   printf("\n--- smallest/biggest unenrolled task id ---\n");

   // tasks 1-5 pending without hold, 6-10 held by an array dependency
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   set_ranges<JB_ja_n_h_ids>(*job, "1-5");
   set_ranges<JB_ja_a_h_ids>(*job, "6-10");
   CHECK(1, "smallest over n_h 1-5 and a_h 6-10 is 1", job_get_smallest_unenrolled_task_id(*job) == 1);
   CHECK(2, "biggest over n_h 1-5 and a_h 6-10 is 10", job_get_biggest_unenrolled_task_id(*job) == 10);

   // the other way round
   auto other = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   set_ranges<JB_ja_a_h_ids>(*other, "1-5");
   set_ranges<JB_ja_n_h_ids>(*other, "6-10");
   CHECK(3, "smallest over a_h 1-5 and n_h 6-10 is 1", job_get_smallest_unenrolled_task_id(*other) == 1);
   CHECK(4, "biggest over a_h 1-5 and n_h 6-10 is 10", job_get_biggest_unenrolled_task_id(*other) == 10);

   // a_h alone, and no unenrolled task at all
   auto only_a_h = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   set_ranges<JB_ja_a_h_ids>(*only_a_h, "3-7");
   CHECK(5, "smallest of a_h 3-7 alone is 3", job_get_smallest_unenrolled_task_id(*only_a_h) == 3);
   CHECK(6, "biggest of a_h 3-7 alone is 7", job_get_biggest_unenrolled_task_id(*only_a_h) == 7);
   auto none = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   CHECK(7, "no unenrolled task gives 0", job_get_smallest_unenrolled_task_id(*none) == 0 &&
                                          job_get_biggest_unenrolled_task_id(*none) == 0);
}

/**
 * @brief The mail list of a job is written for the shepherd without a false "too long"
 *
 * mailrec_unparse() returned 1 for every list after an unbraced if got a DRETURN,
 * whose expansion ends in an unconditional return: the execd then logged "maillist
 * for job ... too long" for every job and wrote an empty mail list.
 */
static void
test_mailrec_unparse() {
   printf("\n--- mailrec_unparse ---\n");

   auto [rc, list] = mailrec_parse("user@host.example.com,other");
   char buf[1024];
   CHECK(8, "two recipients are parsed", rc == 0 && list->size() == 2);
   CHECK(9, "they fit into the buffer", mailrec_unparse(*list, buf, sizeof(buf)) == 0);
   CHECK(10, "and are written comma separated", strcmp(buf, "user@host.example.com,other") == 0);
   CHECK(11, "a buffer too small is reported", mailrec_unparse(*list, buf, 10) == 1);
}

/**
 * @brief Job and task ids from the command line ("<job>", "<job>.<range>", names) are split as before
 *
 * sge_parse_jobtasks() splits at the first '.' into the job id and the task range; a job id must be a whole
 * number of at least 1, a name is accepted only with include_names, and "\"*\"" always.
 */
static void
test_parse_jobtasks() {
   printf("\n--- sge_parse_jobtasks ---\n");

   ocs::tom::ListPtr<ocs::tom::ID> ids;
   auto [plain_ret, plain] = sge_parse_jobtasks(ids, "123", nullptr, false, nullptr);
   CHECK(12, "a job id alone: accepted, stored as given, no task range",
         plain_ret == 1 && plain && strcmp(plain.get<ID_str>(), "123") == 0 && !plain.get<ID_ja_structure>());

   auto [range_ret, ranged] = sge_parse_jobtasks(ids, "42.1-5:2", nullptr, false, nullptr);
   const auto range = ranged ? ranged.get<ID_ja_structure>().front() : ocs::tom::ConstElement<ocs::tom::RN>();
   CHECK(13, "job id and task range: split at the '.', the range parsed",
         range_ret == 1 && ranged && strcmp(ranged.get<ID_str>(), "42") == 0 && range &&
            range.get<RN_min>() == 1 && range.get<RN_max>() == 5 && range.get<RN_step>() == 2);

   CHECK(14, "two elements are in the id list", ids && ids->size() == 2);

   auto [empty_ret, empty] = sge_parse_jobtasks(ids, "", nullptr, false, nullptr);
   auto [frac_ret, frac] = sge_parse_jobtasks(ids, "1.5x", nullptr, false, nullptr);
   auto [zero_ret, zero] = sge_parse_jobtasks(ids, "0", nullptr, false, nullptr);
   CHECK(15, "an empty string, a bad task range and job id 0 are refused without an element",
         empty_ret == -1 && !empty && frac_ret == -1 && !frac && zero_ret == -1 && !zero);

   auto [name_ret, name] = sge_parse_jobtasks(ids, "myjob", nullptr, false, nullptr);
   auto [named_ret, named] = sge_parse_jobtasks(ids, "myjob", nullptr, true, nullptr);
   auto [all_ret, all] = sge_parse_jobtasks(ids, "\"*\"", nullptr, false, nullptr);
   CHECK(16, "a name only with include_names, \"*\" always",
         name_ret == -1 && !name && named_ret == 1 && named && strcmp(named.get<ID_str>(), "myjob") == 0 &&
            all_ret == 1 && all);
}

/**
 * @brief Only the script file name qmaster gives a job is accepted (CS-3134)
 *
 * ocs_job_exec_file_is_valid() accepts exactly `job_scripts/<job id>`; ocs_jobscript_key_is_valid() takes the
 * job id from the spooling key `JOBSCRIPT:<job id>`. Everything else is a name a client could have sent.
 */
static void
test_exec_file_is_valid() {
   printf("\n--- ocs_job_exec_file_is_valid ---\n");

   CHECK(17, "job_scripts/<job id> of the job is accepted", ocs_job_exec_file_is_valid("job_scripts/42", 42));
   CHECK(18, "the name of another job is refused", !ocs_job_exec_file_is_valid("job_scripts/43", 42) &&
                                                    !ocs_job_exec_file_is_valid("job_scripts/420", 42) &&
                                                    !ocs_job_exec_file_is_valid("job_scripts/042", 42));
   CHECK(19, "absolute paths and path tricks are refused",
         !ocs_job_exec_file_is_valid("/etc/passwd", 42) &&
            !ocs_job_exec_file_is_valid("/tmp/job_scripts/42", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/../42", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/42/x", 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/42/", 42) &&
            !ocs_job_exec_file_is_valid("./job_scripts/42", 42));
   CHECK(20, "an empty name, nullptr and job id 0 are refused",
         !ocs_job_exec_file_is_valid("", 42) && !ocs_job_exec_file_is_valid(nullptr, 42) &&
            !ocs_job_exec_file_is_valid("job_scripts/0", 0));

   CHECK(21, "a spooling key with the job's own script is accepted",
         ocs_jobscript_key_is_valid("JOBSCRIPT:42", "job_scripts/42"));
   CHECK(22, "a spooling key with a forged script or a bad job id is refused",
         !ocs_jobscript_key_is_valid("JOBSCRIPT:42", "/etc/passwd") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:41", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:42x", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT42", "job_scripts/42") &&
            !ocs_jobscript_key_is_valid(nullptr, "job_scripts/42") &&
            !ocs_jobscript_key_is_valid("JOBSCRIPT:42", nullptr));
}

/**
 * @brief -suspend_remote is a job attribute (CS-2194)
 *
 * A job nobody set it on means "not specified", whatever client created it; yes and no survive the round
 * trip, and the effective submit command line names the option only when it was specified.
 */
static void
test_suspend_remote() {
   printf("\n--- -suspend_remote job attribute ---\n");

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   CHECK(23, "a new job has -suspend_remote not specified",
         ocs_job_get_suspend_remote(*job) == ocs::Ternary::Unset);

   DSTRING_STATIC(dstr, 1024);
   CHECK(24, "the command line of a job without the option does not name it",
         strstr(job_get_effective_command_line(*job, &dstr, "qrsh"), "-suspend_remote") == nullptr);

   ocs_job_set_suspend_remote(*job, ocs::Ternary::Yes);
   const bool yes = ocs_job_get_suspend_remote(*job) == ocs::Ternary::Yes;
   CHECK(25, "yes is stored and shown as -suspend_remote yes",
         yes && strstr(job_get_effective_command_line(*job, &dstr, "qrsh"), " -suspend_remote yes") != nullptr);

   ocs_job_set_suspend_remote(*job, ocs::Ternary::No);
   const bool no = ocs_job_get_suspend_remote(*job) == ocs::Ternary::No;
   CHECK(26, "no is stored and shown as -suspend_remote no",
         no && strstr(job_get_effective_command_line(*job, &dstr, "qrsh"), " -suspend_remote no") != nullptr);

   ocs_job_set_suspend_remote(*job, ocs::Ternary::Unset);
   CHECK(27, "unset is stored as 0, the value of a job that never had the option",
         ocs_job_get_suspend_remote(*job) == ocs::Ternary::Unset && job->get<JB_suspend_remote>() == 0);
}

/**
 * @brief Entry point
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_unenrolled_extremes();
   test_mailrec_unparse();
   test_parse_jobtasks();
   test_exec_file_is_valid();
   test_suspend_remote();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
