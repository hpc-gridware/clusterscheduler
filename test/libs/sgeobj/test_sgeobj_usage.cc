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

#include <cmath>
#include <cstdio>
#include <cstring>

#include "uti/sge_component.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_Usage.h"
#include "sgeobj/ocs_UserProject.h"

/* CS-1239 step 10: pure-function unit tests for the usage primitives the
 * worker-thread booking helper (sge_book_finished_job_usage) and the TET
 * decay handler stand on. We test ocs::Usage::sum_usage and
 * ocs::UserProject::decay_userprj_usage directly because they live in
 * sgeobj with no qmaster / event-master dependency - the orchestration
 * around them (sge_event_spool, sge_commit_job, dirty-FIFO mark) is
 * exercised end-to-end by the testsuite check
 * system_tests/scheduler/sharetree_usage_booking. */

static int s_fail = 0;

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

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

/* Build a single-attribute scaled-usage list with cpu=value. Used both as
 * JAT_scaled_usage_list on the ja_task we feed to sum_usage and as
 * UU_usage on the seeded user for decay tests. */
static lList *
make_usage_list(const char *list_name, const char *attr, double value) {
   lList *l = lCreateList(list_name, UA_Type);
   lListElem *e = lAddElemStr(&l, UA_name, attr, UA_Type);
   lSetDouble(e, UA_value, value);
   return l;
}

static lListElem *
make_user(const char *name) {
   lListElem *u = lCreateElem(UU_Type);
   lSetString(u, UU_name, name);
   return u;
}

static lListElem *
make_project(const char *name) {
   lListElem *p = lCreateElem(PR_Type);
   lSetString(p, PR_name, name);
   return p;
}

/* Build a 1-task job. JB_job_number is what sum_usage keys debited entries
 * by; ja_task carries the JAT_scaled_usage_list that becomes the finish
 * delta. */
static lListElem *
make_job(uint32_t job_number, const char *owner, const char *project,
         double cpu) {
   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, job_number);
   lSetString(job, JB_owner, owner);
   if (project != nullptr) {
      lSetString(job, JB_project, project);
   }

   lList *ja_tasks = lCreateList("ja_tasks", JAT_Type);
   lListElem *ja_task = lCreateElem(JAT_Type);
   lSetUlong(ja_task, JAT_task_number, 1);
   lSetList(ja_task, JAT_scaled_usage_list, make_usage_list("scaled", "cpu", cpu));
   lAppendElem(ja_tasks, ja_task);
   lSetList(job, JB_ja_tasks, ja_tasks);

   return job;
}

static double
get_usage_value(const lListElem *parent, int list_field, const char *attr) {
   const lList *l = lGetList(parent, list_field);
   if (l == nullptr) {
      return -1.0;
   }
   const lListElem *e = lGetElemStr(l, UA_name, attr);
   if (e == nullptr) {
      return -1.0;
   }
   return lGetDouble(e, UA_value);
}

/* Test 1-4: sum_usage for a user-only job -------------------------------- */

static void
test_sum_usage_user_only(int *id) {
   printf("\n--- sum_usage: user-only job ---\n");

   lListElem *user = make_user("u1");
   lListElem *job = make_job(/*job_number=*/1, /*owner=*/"u1",
                             /*project=*/nullptr, /*cpu=*/10.0);
   lListElem *ja_task = lFirstRW(lGetListRW(job, JB_ja_tasks));

   ocs::Usage::sum_usage(job, ja_task, user, /*project=*/nullptr,
                         /*usage_weight_list=*/nullptr);

   CHECK_NEAR((*id)++, "UU_usage cpu == 10 after first finish",
              get_usage_value(user, UU_usage, "cpu"), 10.0, 1e-9);
   CHECK_NEAR((*id)++, "UU_long_term_usage cpu == 10",
              get_usage_value(user, UU_long_term_usage, "cpu"), 10.0, 1e-9);

   /* CS-1239 sharetree routing invariant: user-only job does NOT populate
    * UU_project[*].UPP_usage. */
   CHECK((*id)++, "UU_project remains empty for user-only job",
         lGetList(user, UU_project) == nullptr ||
         lGetNumberOfElem(lGetList(user, UU_project)) == 0);

   /* Debited-job entry for the finished job should be present so the
    * worker thread's drop_debited_job step has something to remove. */
   const lList *debited = lGetList(user, UU_debited_job_usage);
   const lListElem *upu = (debited != nullptr) ?
                          lGetElemUlong(debited, UPU_job_number, 1) : nullptr;
   CHECK((*id)++, "UU_debited_job_usage contains job 1", upu != nullptr);

   lFreeElem(&user);
   lFreeElem(&job);
}

/* Test 5-9: sum_usage for a user+project job ----------------------------- */

static void
test_sum_usage_user_and_project(int *id) {
   printf("\n--- sum_usage: user+project job ---\n");

   lListElem *user = make_user("u1");
   lListElem *project = make_project("p1");
   lListElem *job = make_job(/*job_number=*/2, /*owner=*/"u1",
                             /*project=*/"p1", /*cpu=*/7.0);
   lListElem *ja_task = lFirstRW(lGetListRW(job, JB_ja_tasks));

   ocs::Usage::sum_usage(job, ja_task, user, project,
                         /*usage_weight_list=*/nullptr);

   /* PR_ side gets the full delta. */
   CHECK_NEAR((*id)++, "PR_usage cpu == 7 after first finish",
              get_usage_value(project, PR_usage, "cpu"), 7.0, 1e-9);
   CHECK_NEAR((*id)++, "PR_long_term_usage cpu == 7",
              get_usage_value(project, PR_long_term_usage, "cpu"), 7.0, 1e-9);

   /* CS-1239 sharetree routing: project-bound finish populates
    * UU_project[p1].UPP_usage, NOT UU_usage. */
   const lList *upp_list = lGetList(user, UU_project);
   const lListElem *upp = (upp_list != nullptr) ?
                          lGetElemStr(upp_list, UPP_name, "p1") : nullptr;
   CHECK((*id)++, "UU_project[p1] entry created", upp != nullptr);
   if (upp != nullptr) {
      CHECK_NEAR((*id)++, "UU_project[p1].UPP_usage cpu == 7",
                 get_usage_value(upp, UPP_usage, "cpu"), 7.0, 1e-9);
   } else {
      ++(*id);
   }
   CHECK((*id)++, "UU_usage stays empty for project-bound finish",
         lGetList(user, UU_usage) == nullptr ||
         get_usage_value(user, UU_usage, "cpu") <= 0.0);

   /* Debited entry recorded on the user object (sum_usage routes debited
    * to UU_ when user is non-null). */
   const lList *debited = lGetList(user, UU_debited_job_usage);
   const lListElem *upu = (debited != nullptr) ?
                          lGetElemUlong(debited, UPU_job_number, 2) : nullptr;
   CHECK((*id)++, "UU_debited_job_usage contains job 2", upu != nullptr);

   lFreeElem(&user);
   lFreeElem(&project);
   lFreeElem(&job);
}

/* Test 10-12: decay_userprj_usage over wallclock time -------------------- */

static void
test_decay_over_interval(int *id) {
   printf("\n--- decay_userprj_usage: halftime-based decay ---\n");

   /* Seed a user with cpu=100 stamped at t0. */
   lListElem *user = make_user("u1");
   lSetList(user, UU_usage, make_usage_list("usage", "cpu", 100.0));

   const uint64_t t0 = sge_get_gmt64();
   lSetUlong64(user, UU_usage_time_stamp, t0);

   /* 1-hour halftime via the default decay constant; nullptr decay_list
    * routes through the same default. */
   ocs::Usage::calculate_default_decay_constant(/*halftime_hours=*/1);

   /* Advance time by exactly one halftime - cpu should halve. */
   const uint64_t t1 = t0 + sge_gmt32_to_gmt64(3600);
   ocs::UserProject::decay_userprj_usage(user, /*is_user=*/true,
                                         /*decay_list=*/nullptr,
                                         /*seqno=*/1, t1);

   /* 5% tolerance: the decay primitive uses an exponential-equivalent
    * approximation, not a closed-form halving. */
   CHECK_NEAR((*id)++, "UU_usage cpu ~= 50 after one halftime",
              get_usage_value(user, UU_usage, "cpu"), 50.0, 2.5);

   /* Same seqno again: idempotent, no further decay. */
   const double after_first = get_usage_value(user, UU_usage, "cpu");
   ocs::UserProject::decay_userprj_usage(user, true, nullptr, 1, t1);
   CHECK_NEAR((*id)++, "second call with same seqno is a no-op",
              get_usage_value(user, UU_usage, "cpu"), after_first, 1e-9);

   /* New seqno + another halftime: cpu should halve again (~25). */
   const uint64_t t2 = t1 + sge_gmt32_to_gmt64(3600);
   ocs::UserProject::decay_userprj_usage(user, true, nullptr, /*seqno=*/2, t2);
   CHECK_NEAR((*id)++, "UU_usage cpu ~= 25 after two halftimes",
              get_usage_value(user, UU_usage, "cpu"), 25.0, 1.5);

   lFreeElem(&user);
}

/* main ------------------------------------------------------------------- */

int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_usage");
   component_set_daemonized(true);
   sge_prof_set_enabled(false);
   lInit(nmv);

   int id = 1;
   test_sum_usage_user_only(&id);
   test_sum_usage_user_and_project(&id);
   test_decay_over_interval(&id);

   printf("\n--- summary: %s ---\n", s_fail == 0 ? "all green" : "FAILURES");
   return s_fail;
}
