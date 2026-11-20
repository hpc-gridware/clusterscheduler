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
 */

#include <cstdio>
#include <cstring>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
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
 * @param job the job
 * @param field the range list field, e.g. JB_ja_n_h_ids
 * @param ranges the ranges, e.g. "1-5"
 */
static void
set_ranges(const ocs::tom::Element<ocs::tom::JB> job, const int field, const char *ranges) {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::RN> list;
   range_list_parse_from_string(list, &alp, ranges, false, false, INF_NOT_ALLOWED);
   alp.reset();
   // the field is a run-time number, so the list is handed over raw
   ocs::tom::set(job, field, ocs::tom::RawListPtr(std::move(list)));
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
   set_ranges(*job, JB_ja_n_h_ids, "1-5");
   set_ranges(*job, JB_ja_a_h_ids, "6-10");
   CHECK(1, "smallest over n_h 1-5 and a_h 6-10 is 1", job_get_smallest_unenrolled_task_id(*job) == 1);
   CHECK(2, "biggest over n_h 1-5 and a_h 6-10 is 10", job_get_biggest_unenrolled_task_id(*job) == 10);

   // the other way round
   auto other = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   set_ranges(*other, JB_ja_a_h_ids, "1-5");
   set_ranges(*other, JB_ja_n_h_ids, "6-10");
   CHECK(3, "smallest over a_h 1-5 and n_h 6-10 is 1", job_get_smallest_unenrolled_task_id(*other) == 1);
   CHECK(4, "biggest over a_h 1-5 and n_h 6-10 is 10", job_get_biggest_unenrolled_task_id(*other) == 10);

   // a_h alone, and no unenrolled task at all
   auto only_a_h = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   set_ranges(*only_a_h, JB_ja_a_h_ids, "3-7");
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
 * @brief Entry point
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_unenrolled_extremes();
   test_mailrec_unparse();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
