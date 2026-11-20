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
 * @brief Unit tests for the type check of request data (CS-3125)
 *
 * qmaster checks with ocs_object_list_verify_type() in its listener thread
 * that the data of a request has the types its target and command expect:
 * a reduced descriptor of the type is accepted, a list or sublist of another
 * type is refused - also when it is empty, which the descriptor check of the
 * typed object model would otherwise meet later on the stored object.
 */

#include <cstdio>

#include "cull/cull_test_api.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_ElementCore.h"

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
 * @brief An exec host list reduced to the name and the access list
 * @param acl the access list to put into the host; taken over, may be nullptr
 * @return the list, one host "global"
 */
static lList *
reduced_host_list(lList *acl) {
   lEnumeration *what = lWhat("%T(%I %I)", EH_Type, EH_name, EH_acl);
   const lDescr *descr = nullptr;
   lReduceDescr(&descr, EH_Type, what);
   lFreeWhat(&what);
   lList *lp = lCreateList("host", descr);
   lListElem *ep = lCreateElem(descr);
   lSetHost(ep, EH_name, "global");
   lSetList(ep, EH_acl, acl);
   lAppendElem(lp, ep);
   ocs_cull_descr_release(&descr);
   return lp;
}

// ---------------------------------------------------------------------------
// ocs::tom::same_type(), ocs_object_list_verify_type()
// ---------------------------------------------------------------------------

/**
 * @brief Request data of the right type passes, of a wrong type it is refused
 */
static void
test_verify_type() {
   printf("\n--- request data types ---\n");

   // T01-T03: same_type itself
   {
      lEnumeration *what = lWhat("%T(%I)", EH_Type, EH_name);
      const lDescr *reduced = nullptr;
      lReduceDescr(&reduced, EH_Type, what);
      lFreeWhat(&what);
      CHECK(1, "a reduced descriptor is of the type of the full one", ocs::tom::same_type(reduced, EH_Type));
      CHECK(2, "two different types differ", !ocs::tom::same_type(ST_Type, US_Type));
      CHECK(3, "nullptr is no type", !ocs::tom::same_type(nullptr, EH_Type));
      ocs_cull_descr_release(&reduced);
   }

   // T04: a reduced host with an access list of usersets passes
   {
      lList *acl = nullptr;
      lAddElemStr(&acl, US_name, "arusers", US_Type);
      lList *lp = reduced_host_list(acl);
      CHECK(4, "a reduced host with a userset access list passes", ocs_object_list_verify_type(lp, EH_Type));
      lFreeList(&lp);
   }

   // T05: the list itself is of the wrong type
   {
      lList *lp = nullptr;
      lAddElemStr(&lp, ST_name, "global", ST_Type);
      CHECK(5, "strings sent as hosts are refused", !ocs_object_list_verify_type(lp, EH_Type));
      lFreeList(&lp);
   }

   // T06: an empty access list of the wrong type - the case the old code stored
   {
      lList *lp = reduced_host_list(lCreateList("acl", ST_Type));
      CHECK(6, "an empty sublist of the wrong type is refused", !ocs_object_list_verify_type(lp, EH_Type));
      lFreeList(&lp);
   }

   // T07: a non-empty access list of the wrong type - the case the old code aborted on
   {
      lList *acl = nullptr;
      lAddElemStr(&acl, ST_name, "arusers", ST_Type);
      lList *lp = reduced_host_list(acl);
      CHECK(7, "a non-empty sublist of the wrong type is refused", !ocs_object_list_verify_type(lp, EH_Type));
      lFreeList(&lp);
   }

   // T08: no data and no expected type
   CHECK(8, "no data passes", ocs_object_list_verify_type(nullptr, EH_Type));
   {
      lList *acl = lCreateList("acl", ST_Type);
      lList *lp = reduced_host_list(acl);
      CHECK(9, "without an expected top-level type the sublists are still checked",
            !ocs_object_list_verify_type(lp, nullptr));
      lFreeList(&lp);
   }
}

/**
 * @brief The shapes real clients send pass, an object field of the wrong type does not
 */
static void
test_client_shapes() {
   printf("\n--- request data of real clients ---\n");

   // T10: qalter - a reduced job with a reduced task list
   {
      lEnumeration *job_what = lWhat("%T(%I %I)", JB_Type, JB_job_number, JB_ja_tasks);
      lEnumeration *task_what = lWhat("%T(%I %I)", JAT_Type, JAT_task_number, JAT_hold);
      const lDescr *job_descr = nullptr;
      const lDescr *task_descr = nullptr;
      lReduceDescr(&job_descr, JB_Type, job_what);
      lReduceDescr(&task_descr, JAT_Type, task_what);
      lFreeWhat(&job_what);
      lFreeWhat(&task_what);
      lList *tasks = lCreateList("tasks", task_descr);
      lListElem *task = lCreateElem(task_descr);
      lSetUlong(task, JAT_task_number, 1);
      lAppendElem(tasks, task);
      lList *lp = lCreateList("jobs", job_descr);
      lListElem *job = lCreateElem(job_descr);
      lSetUlong(job, JB_job_number, 4711);
      lSetList(job, JB_ja_tasks, tasks);
      lAppendElem(lp, job);
      CHECK(10, "a reduced job with a reduced task list passes", ocs_object_list_verify_type(lp, JB_Type));
      lFreeList(&lp);
      ocs_cull_descr_release(&job_descr);
      ocs_cull_descr_release(&task_descr);
   }

   // T11: qdel - ID elements with a user list
   {
      lList *lp = nullptr;
      lListElem *id = lAddElemStr(&lp, ID_str, "4711", ID_Type);
      lList *users = nullptr;
      lAddElemStr(&users, ST_name, "*", ST_Type);
      lSetList(id, ID_user_list, users);
      CHECK(11, "ID elements with a string user list pass", ocs_object_list_verify_type(lp, ID_Type));
      lFreeList(&lp);
   }

   // T12: procedures - an untyped value list is not compared
   {
      lList *lp = nullptr;
      lListElem *spp = lAddElemStr(&lp, SPP_name, "parameter", SPP_Type);
      lList *values = nullptr;
      lAddElemStr(&values, ST_name, "value", ST_Type);
      lSetList(spp, SPP_value_list, values);
      CHECK(12, "a parameter bundle with an untyped value list passes", ocs_object_list_verify_type(lp, SPP_Type));
      lFreeList(&lp);
   }

   // T13/T14: an object field, of its type and of another
   {
      lList *lp = nullptr;
      lListElem *job = lAddElemUlong(&lp, JB_job_number, 4711, JB_Type);
      lSetObject(job, JB_binding, lCreateElem(BN_Type));
      CHECK(13, "an object field of its type passes", ocs_object_list_verify_type(lp, JB_Type));
      lSetObject(job, JB_binding, lCreateElem(ST_Type));
      CHECK(14, "an object field of another type is refused", !ocs_object_list_verify_type(lp, JB_Type));
      lFreeList(&lp);
   }

   // T16/T17: the where/what filters of an event client subscription are packed into PACK elements
   {
      lList *lp = nullptr;
      lListElem *ev = lAddElemUlong(&lp, EV_id, 0, EV_Type);
      lList *subscriptions = nullptr;
      lListElem *evs = lAddElemUlong(&subscriptions, EVS_id, 1, EVS_Type);
      lSetList(ev, EV_subscribed, subscriptions);
      lSetObject(evs, EVS_where, lCreateElem(PACK_Type));
      CHECK(16, "a subscription filter packed as PACK passes", ocs_object_list_verify_type(lp, EV_Type));
      lSetObject(evs, EVS_where, lCreateElem(ST_Type));
      CHECK(17, "a subscription filter of another type is refused", !ocs_object_list_verify_type(lp, EV_Type));
      lFreeList(&lp);
   }

   // T15: two different reductions of one type are of the same type
   {
      lEnumeration *what_a = lWhat("%T(%I)", EH_Type, EH_name);
      lEnumeration *what_b = lWhat("%T(%I %I)", EH_Type, EH_name, EH_acl);
      const lDescr *a = nullptr;
      const lDescr *b = nullptr;
      lReduceDescr(&a, EH_Type, what_a);
      lReduceDescr(&b, EH_Type, what_b);
      lFreeWhat(&what_a);
      lFreeWhat(&what_b);
      CHECK(15, "two reductions of one type are of the same type", ocs::tom::same_type(a, b));
      ocs_cull_descr_release(&a);
      ocs_cull_descr_release(&b);
   }
}

/**
 * @brief Entry point
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_verify_type();
   test_client_shapes();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
