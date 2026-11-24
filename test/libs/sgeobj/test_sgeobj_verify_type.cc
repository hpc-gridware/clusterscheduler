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

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
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
 * @brief An exec host list reduced to the name and the access list
 *
 * The access list is handed over raw, which stores it unchecked: the wrong type is what some of the checks
 * are about.
 *
 * @param acl the access list to put into the host; taken over, may be empty
 * @return the list, one host "global"
 */
static ocs::tom::ListPtr<ocs::tom::EH>
reduced_host_list(ocs::tom::RawListPtr acl) {
   const auto layout = ocs::tom::layout<EH_name, EH_acl>();
   auto lp = ocs::tom::ListPtr<ocs::tom::EH>::create("host", layout);
   auto ep = ocs::tom::ElementPtr<ocs::tom::EH>::create(layout);
   ep->set<EH_name>(ocs::tom::Host("global"));
   ocs::tom::set<EH_acl>(ep.get().raw(), acl.release());
   lp->push_back(std::move(ep));
   return lp;
}

/**
 * @brief A string list with one entry
 * @param name the name of the list
 * @param value the entry
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::ST>
string_list(const char *name, const char *value) {
   auto lp = ocs::tom::ListPtr<ocs::tom::ST>::create(name);
   lp->emplace_back<ST_name>(value);
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
      const ocs::tom::FieldDescr *reduced = ocs::tom::layout<EH_name>().get();
      CHECK(1, "a reduced descriptor is of the type of the full one",
            ocs::tom::same_type(reduced, ocs::tom::EH::descriptor.e));
      CHECK(2, "two different types differ",
            !ocs::tom::same_type(ocs::tom::ST::descriptor.e, ocs::tom::US::descriptor.e));
      CHECK(3, "nullptr is no type", !ocs::tom::same_type(nullptr, ocs::tom::EH::descriptor.e));
   }

   // T04: a reduced host with an access list of usersets passes
   {
      auto acl = ocs::tom::ListPtr<ocs::tom::US>::create("acl");
      acl->emplace_back<US_name>("arusers");
      auto lp = reduced_host_list(ocs::tom::RawListPtr(std::move(acl)));
      CHECK(4, "a reduced host with a userset access list passes",
            ocs_object_list_verify_type(*lp, ocs::tom::EH::descriptor.e));
      lp.reset();
   }

   // T05: the list itself is of the wrong type
   {
      auto lp = string_list("strings", "global");
      CHECK(5, "strings sent as hosts are refused", !ocs_object_list_verify_type(*lp, ocs::tom::EH::descriptor.e));
      lp.reset();
   }

   // T06: an empty access list of the wrong type - the case the old code stored
   {
      auto lp = reduced_host_list(ocs::tom::RawListPtr(ocs::tom::ListPtr<ocs::tom::ST>::create("acl")));
      CHECK(6, "an empty sublist of the wrong type is refused",
            !ocs_object_list_verify_type(*lp, ocs::tom::EH::descriptor.e));
      lp.reset();
   }

   // T07: a non-empty access list of the wrong type - the case the old code aborted on
   {
      auto lp = reduced_host_list(ocs::tom::RawListPtr(string_list("acl", "arusers")));
      CHECK(7, "a non-empty sublist of the wrong type is refused",
            !ocs_object_list_verify_type(*lp, ocs::tom::EH::descriptor.e));
      lp.reset();
   }

   // T08: no data and no expected type
   CHECK(8, "no data passes", ocs_object_list_verify_type(nullptr, ocs::tom::EH::descriptor.e));
   {
      auto lp = reduced_host_list(ocs::tom::RawListPtr(ocs::tom::ListPtr<ocs::tom::ST>::create("acl")));
      CHECK(9, "without an expected top-level type the sublists are still checked",
            !ocs_object_list_verify_type(*lp, nullptr));
      lp.reset();
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
      const auto task_layout = ocs::tom::layout<JAT_task_number, JAT_hold>();
      const auto job_layout = ocs::tom::layout<JB_job_number, JB_ja_tasks>();
      auto tasks = ocs::tom::ListPtr<ocs::tom::JAT>::create("tasks", task_layout);
      auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_layout);
      task->set<JAT_task_number>(1);
      tasks->push_back(std::move(task));
      auto lp = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs", job_layout);
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create(job_layout);
      job->set<JB_job_number>(4711);
      job->set<JB_ja_tasks>(std::move(tasks));
      lp->push_back(std::move(job));
      CHECK(10, "a reduced job with a reduced task list passes",
            ocs_object_list_verify_type(*lp, ocs::tom::JB::descriptor.e));
      lp.reset();
   }

   // T11: qdel - ID elements with a user list
   {
      auto lp = ocs::tom::ListPtr<ocs::tom::ID>::create("ids");
      const auto id = lp->emplace_back<ID_str>("4711");
      id.set<ID_user_list>(string_list("users", "*"));
      CHECK(11, "ID elements with a string user list pass",
            ocs_object_list_verify_type(*lp, ocs::tom::ID::descriptor.e));
      lp.reset();
   }

   // T12: procedures - an untyped value list is not compared
   {
      auto lp = ocs::tom::ListPtr<ocs::tom::SPP>::create("parameters");
      const auto spp = lp->emplace_back<SPP_name>("parameter");
      spp.set<SPP_value_list>(string_list("values", "value"));
      CHECK(12, "a parameter bundle with an untyped value list passes",
            ocs_object_list_verify_type(*lp, ocs::tom::SPP::descriptor.e));
      lp.reset();
   }

   // T13/T14: an object field, of its type and of another
   {
      auto lp = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
      const auto job = lp->emplace_back<JB_job_number>(4711);
      job.set<JB_binding>(ocs::tom::ElementPtr<ocs::tom::BN>::create());
      CHECK(13, "an object field of its type passes", ocs_object_list_verify_type(*lp, ocs::tom::JB::descriptor.e));
      // handed over raw, which stores the object of another type unchecked
      ocs::tom::set<JB_binding>(job.raw(), ocs::tom::ElementPtr<ocs::tom::ST>::create().release());
      CHECK(14, "an object field of another type is refused",
            !ocs_object_list_verify_type(*lp, ocs::tom::JB::descriptor.e));
      lp.reset();
   }

   // T16/T17: the where/what filters of an event client subscription are packed into PACK elements
   {
      auto lp = ocs::tom::ListPtr<ocs::tom::EV>::create("event clients");
      const auto ev = lp->emplace_back<EV_id>(0);
      auto subscriptions = ocs::tom::ListPtr<ocs::tom::EVS>::create("subscriptions");
      const auto evs = subscriptions->emplace_back<EVS_id>(1);
      ev.set<EV_subscribed>(std::move(subscriptions));
      evs.set<EVS_where>(ocs::tom::ElementPtr<ocs::tom::PACK>::create());
      CHECK(16, "a subscription filter packed as PACK passes",
            ocs_object_list_verify_type(*lp, ocs::tom::EV::descriptor.e));
      // handed over raw, which stores the object of another type unchecked
      ocs::tom::set<EVS_where>(evs.raw(), ocs::tom::ElementPtr<ocs::tom::ST>::create().release());
      CHECK(17, "a subscription filter of another type is refused",
            !ocs_object_list_verify_type(*lp, ocs::tom::EV::descriptor.e));
      lp.reset();
   }

   // T15: two different reductions of one type are of the same type
   {
      const ocs::tom::FieldDescr *a = ocs::tom::layout<EH_name>().get();
      const ocs::tom::FieldDescr *b = ocs::tom::layout<EH_name, EH_acl>().get();
      CHECK(15, "two reductions of one type are of the same type", ocs::tom::same_type(a, b));
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
