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
 * @brief Unit tests for usage in `libs/sgeobj`
 */

#include <cmath>
#include <cstdio>
#include <cstring>

#include "uti/sge_component.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/ocs_Usage.h"
#include "sgeobj/ocs_UserProject.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_UPU.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/* CS-1239 step 10: pure-function unit tests for the usage primitives the
 * worker-thread booking helper (sge_book_finished_job_usage) and the TET
 * decay handler stand on. We test ocs::Usage::sum_usage and
 * ocs::UserProject::decay_userprj_usage directly because they live in
 * sgeobj with no qmaster / event-master dependency - the orchestration
 * around them (sge_event_spool, sge_commit_job, dirty-FIFO mark) is
 * exercised end-to-end by the testsuite check
 * system_tests/scheduler/sharetree_usage_booking. */

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
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

/** @brief #CHECK for a floating point value, within a tolerance
 *
 * Usage figures come out of arithmetic on doubles, so an exact comparison would
 * fail on a rounding difference that means nothing.
 *
 * @param id the test number
 * @param label what the check is about
 * @param actual the value produced
 * @param expected the value wanted
 * @param tol how far apart the two may be
 */
#define CHECK_NEAR(id, label, actual, expected, tol) \
   do { \
      const double a = (actual); \
      const double e = (expected); \
      if (std::fabs(a - e) > (tol)) { \
         printf("FAIL  [T%02d] %s (got %.6f, expected %.6f +/- %.6f)\n", \
                (id), (label), a, e, (double)(tol)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s (%.6f)\n", (id), (label), a); \
      } \
   } while (0)

/* Helpers ----------------------------------------------------------------- */

/**
 * @brief Build a single-attribute scaled-usage list with attr=value
 *
 * Used both as JAT_scaled_usage_list on the ja_task we feed to sum_usage and
 * as UU_usage on the seeded user for decay tests.
 *
 * @param list_name the name of the list
 * @param attr the usage attribute
 * @param value its value
 * @return the usage list
 */
static ocs::tom::ListPtr<ocs::tom::UA>
make_usage_list(const char *list_name, const char *attr, const double value) {
   auto l = ocs::tom::ListPtr<ocs::tom::UA>::create(list_name);
   ocs::tom::Element<ocs::tom::UA> e = l->emplace_back<UA_name>(attr);
   e.set<UA_value>(value);
   return l;
}

/**
 * @brief Build a user object
 * @param name the user name
 * @return the user
 */
static ocs::tom::ElementPtr<ocs::tom::UU>
make_user(const char *name) {
   auto u = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   u->set<UU_name>(name);
   return u;
}

/**
 * @brief Build a project object
 * @param name the project name
 * @return the project
 */
static ocs::tom::ElementPtr<ocs::tom::PR>
make_project(const char *name) {
   auto p = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   p->set<PR_name>(name);
   return p;
}

/**
 * @brief Build a 1-task job
 *
 * JB_job_number is what sum_usage keys debited entries by; ja_task carries the
 * JAT_scaled_usage_list that becomes the finish delta.
 *
 * @param job_number the job number
 * @param owner the job owner
 * @param project the job project, or nullptr
 * @param cpu the cpu usage of the task
 * @return the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
make_job(const uint32_t job_number, const char *owner, const char *project,
         const double cpu) {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(job_number);
   job->set<JB_owner>(owner);
   if (project != nullptr) {
      job->set<JB_project>(project);
   }

   auto ja_tasks = ocs::tom::ListPtr<ocs::tom::JAT>::create("ja_tasks");
   auto ja_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   ja_task->set<JAT_task_number>(1);
   ja_task->set<JAT_scaled_usage_list>(make_usage_list("scaled", "cpu", cpu));
   ja_tasks->push_back(std::move(ja_task));
   job->set<JB_ja_tasks>(std::move(ja_tasks));

   return job;
}

/**
 * @brief The value of an entry of a usage list of a user or a project
 * @param parent the user or the project
 * @param list_field the usage list field, given at run time
 * @param attr the name of the entry
 * @return the value, -1.0 if the list or the entry is missing
 */
static double
get_usage_value(const ocs::tom::ConstRawElement parent, const int list_field, const char *attr) {
   const lList *l = parent.get<const lList *>(list_field);
   if (l == nullptr) {
      return -1.0;
   }
   ocs::tom::ConstElement<ocs::tom::UA> e = ocs::tom::find_first<UA_name>(l, attr);
   if (e == nullptr) {
      return -1.0;
   }
   return e.get<UA_value>();
}

/**
 * @brief sum_usage for a user-only job (checks 1-4)
 * @param id the running check number, advanced by every CHECK
 * @return nothing; CHECK records the results
 */
static void
test_sum_usage_user_only(int *id) {
   printf("\n--- sum_usage: user-only job ---\n");

   auto user = make_user("u1");
   auto job = make_job(/*job_number=*/1, /*owner=*/"u1",
                       /*project=*/nullptr, /*cpu=*/10.0);
   ocs::tom::Element<ocs::tom::JAT> ja_task = job->get<JB_ja_tasks>().front();

   ocs::Usage::sum_usage(*job, ja_task, *user, /*project=*/ocs::tom::Element<ocs::tom::PR>(),
                         /*usage_weight_list=*/ocs::tom::ConstList<ocs::tom::UA>());

   CHECK_NEAR((*id)++, "UU_usage cpu == 10 after first finish",
              get_usage_value(*user, UU_usage, "cpu"), 10.0, 1e-9);
   CHECK_NEAR((*id)++, "UU_long_term_usage cpu == 10",
              get_usage_value(*user, UU_long_term_usage, "cpu"), 10.0, 1e-9);

   /* CS-1239 sharetree routing invariant: user-only job does NOT populate
    * UU_project[*].UPP_usage. */
   CHECK((*id)++, "UU_project remains empty for user-only job",
         !user->get<UU_project>() || user->get<UU_project>().size() == 0);

   /* Debited-job entry for the finished job should be present so the
    * worker thread's drop_debited_job step has something to remove. */
   ocs::tom::ConstList<ocs::tom::UPU> debited = user->get<UU_debited_job_usage>();
   const lListElem *upu = (debited != nullptr) ?
                          static_cast<const lListElem *>(debited.find_first<UPU_job_number>(1)) : nullptr;
   CHECK((*id)++, "UU_debited_job_usage contains job 1", upu != nullptr);
}

/**
 * @brief sum_usage for a user+project job (checks 5-9)
 * @param id the running check number, advanced by every CHECK
 * @return nothing; CHECK records the results
 */
static void
test_sum_usage_user_and_project(int *id) {
   printf("\n--- sum_usage: user+project job ---\n");

   auto user = make_user("u1");
   auto project = make_project("p1");
   auto job = make_job(/*job_number=*/2, /*owner=*/"u1",
                       /*project=*/"p1", /*cpu=*/7.0);
   ocs::tom::Element<ocs::tom::JAT> ja_task = job->get<JB_ja_tasks>().front();

   ocs::Usage::sum_usage(*job, ja_task, *user, *project,
                         /*usage_weight_list=*/ocs::tom::ConstList<ocs::tom::UA>());

   /* PR_ side gets the full delta. */
   CHECK_NEAR((*id)++, "PR_usage cpu == 7 after first finish",
              get_usage_value(*project, PR_usage, "cpu"), 7.0, 1e-9);
   CHECK_NEAR((*id)++, "PR_long_term_usage cpu == 7",
              get_usage_value(*project, PR_long_term_usage, "cpu"), 7.0, 1e-9);

   /* CS-1239 sharetree routing: project-bound finish populates
    * UU_project[p1].UPP_usage, NOT UU_usage. */
   ocs::tom::ConstList<ocs::tom::UPP> upp_list = user->get<UU_project>();
   const lListElem *upp = (upp_list != nullptr) ?
                          static_cast<const lListElem *>(upp_list.find_first<UPP_name>("p1")) : nullptr;
   CHECK((*id)++, "UU_project[p1] entry created", upp != nullptr);
   if (upp != nullptr) {
      CHECK_NEAR((*id)++, "UU_project[p1].UPP_usage cpu == 7",
                 get_usage_value(upp, UPP_usage, "cpu"), 7.0, 1e-9);
   } else {
      ++(*id);
   }
   CHECK((*id)++, "UU_usage stays empty for project-bound finish",
         static_cast<const lList *>(user->get<UU_usage>()) == nullptr ||
         get_usage_value(*user, UU_usage, "cpu") <= 0.0);

   /* Debited entry recorded on the user object (sum_usage routes debited
    * to UU_ when user is non-null). */
   ocs::tom::ConstList<ocs::tom::UPU> debited = user->get<UU_debited_job_usage>();
   const lListElem *upu = (debited != nullptr) ?
                          static_cast<const lListElem *>(debited.find_first<UPU_job_number>(2)) : nullptr;
   CHECK((*id)++, "UU_debited_job_usage contains job 2", upu != nullptr);
}

/**
 * @brief decay_userprj_usage over wallclock time (checks 10-12)
 * @param id the running check number, advanced by every CHECK
 * @return nothing; CHECK records the results
 */
static void
test_decay_over_interval(int *id) {
   printf("\n--- decay_userprj_usage: halftime-based decay ---\n");

   /* Seed a user with cpu=100 stamped at t0. */
   auto user = make_user("u1");
   user->set<UU_usage>(make_usage_list("usage", "cpu", 100.0));

   const uint64_t t0 = sge_get_gmt64();
   user->set<UU_usage_time_stamp>(t0);

   /* 1-hour halftime via the default decay constant; nullptr decay_list
    * routes through the same default. */
   ocs::Usage::calculate_default_decay_constant(/*halftime_hours=*/1);

   /* Advance time by exactly one halftime - cpu should halve. */
   const uint64_t t1 = t0 + sge_gmt32_to_gmt64(3600);
   ocs::UserProject::decay_userprj_usage(*user,
                                         /*decay_list=*/nullptr,
                                         /*seqno=*/1, t1);

   /* 5% tolerance: the decay primitive uses an exponential-equivalent
    * approximation, not a closed-form halving. */
   CHECK_NEAR((*id)++, "UU_usage cpu ~= 50 after one halftime",
              get_usage_value(*user, UU_usage, "cpu"), 50.0, 2.5);

   /* Same seqno again: idempotent, no further decay. */
   const double after_first = get_usage_value(*user, UU_usage, "cpu");
   ocs::UserProject::decay_userprj_usage(*user, nullptr, 1, t1);
   CHECK_NEAR((*id)++, "second call with same seqno is a no-op",
              get_usage_value(*user, UU_usage, "cpu"), after_first, 1e-9);

   /* New seqno + another halftime: cpu should halve again (~25). */
   const uint64_t t2 = t1 + sge_gmt32_to_gmt64(3600);
   ocs::UserProject::decay_userprj_usage(*user, nullptr, /*seqno=*/2, t2);
   CHECK_NEAR((*id)++, "UU_usage cpu ~= 25 after two halftimes",
              get_usage_value(*user, UU_usage, "cpu"), 25.0, 1.5);
}

/* main ------------------------------------------------------------------- */

int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_usage");
   component_set_daemonized(true);
   sge_prof_set_enabled(false);
   ocs::tom::install_model(ocs::tom::gcs_model());

   int id = 1;
   test_sum_usage_user_only(&id);
   test_sum_usage_user_and_project(&id);
   test_decay_over_interval(&id);

   printf("\n--- summary: %s ---\n", s_fail == 0 ? "all green" : "FAILURES");
   return s_fail;
}
