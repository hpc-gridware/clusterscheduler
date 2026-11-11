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
 * @brief Module test of the typed list handles and the owning handles of the typed object model (CS-3043, HDL-05, HDL-06)
 */

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using namespace ocs::tom;

/**
 * @brief Build a list of tasks with consecutive numbers, the old way
 * @param n the number of tasks
 * @return the list
 */
static lList *
make_tasks(const std::uint32_t n) {
   lList *tasks = lCreateList("tasks", JAT_Type);
   for (std::uint32_t i = 1; i <= n; i++) {
      lAddElemUlong(&tasks, JAT_task_number, i, JAT_Type);
   }
   return tasks;
}

/**
 * @brief Count the matches of a range
 * @param range the range
 * @return the number of elements it yields
 */
template <typename Range>
static int count(const Range &range) {
   int n = 0;
   for ([[maybe_unused]] auto element : range) {
      n++;
   }
   return n;
}

/**
 * @brief HDL-05: ranges and lookups yield what the old interface yields
 * @return nothing; CHECK records the results
 */
static void
test_ranges_and_lookups() {
   lListElem *job = lCreateElem(JB_Type);
   lSetList(job, JB_ja_tasks, make_tasks(5));

   std::vector<const lListElem *> old_order;
   const lListElem *ep;
   for_each_ep(ep, lGetList(job, JB_ja_tasks)) {
      old_order.push_back(ep);
   }
   std::vector<const lListElem *> new_order;
   for (const auto task : Element<JB>{job}.get<JB_ja_tasks>()) {
      new_order.push_back(task);
   }
   CHECK(1, "range-for over the task list of a job yields the elements of for_each_ep, in order",
         new_order == old_order && new_order.size() == 5);

   for (const std::uint32_t n : {5u, 40u}) {
      lList *raw = make_tasks(n);
      // a second task 3, so that find_all has two matches; task numbers are unique, so take another field
      lSetUlong(lFirstRW(raw), JAT_status, 7);
      lSetUlong(lLastRW(raw), JAT_status, 7);
      const ConstList<JAT> tasks{raw};
      const bool lookups = tasks.find_first<JAT_task_number>(3u).raw() == lGetElemUlong(raw, JAT_task_number, 3) &&
                           tasks.find_first<JAT_task_number>(3u).get<JAT_task_number>() == 3 &&
                           !tasks.find_first<JAT_task_number>(n + 1) && tasks.size() == n;
      CHECK(n == 5 ? 2 : 4, n == 5 ? "find_first below the hash threshold" : "find_first above the hash threshold",
            lookups);
      CHECK(n == 5 ? 3 : 5, n == 5 ? "find_all below the hash threshold" : "find_all above the hash threshold",
            count(tasks.find_all<JAT_status>(7u)) == 2 && count(tasks.find_all<JAT_task_number>(n + 1)) == 0 &&
            tasks.find_all<JAT_task_number>(n + 1).empty());
      lFreeList(&raw);
   }

#ifdef ENABLE_DEBUG_CHECKS
   // a list of queues in the task list of a job is a wrong declaration, found when the handle is handed out
   lList *queues = lCreateList("queues", QU_Type);
   lAppendElem(queues, lCreateElem(QU_Type));
   lSetList(job, JB_ja_tasks, queues);
   bool thrown = false;
   try {
      static_cast<void>(Element<JB>{job}.get<JB_ja_tasks>());
   } catch (const FieldException &e) {
      thrown = e.reason() == FieldException::Reason::WrongSubType && std::strstr(e.what(), "JB_ja_tasks") != nullptr &&
               std::strstr(e.what(), "QU") != nullptr && std::strstr(e.what(), "JAT") != nullptr;
   }
   CHECK(6, "a sub-list of another type than declared throws in a debug build, naming field and both types", thrown);
#else
   CHECK(6, "the sub-type check exists in debug builds only; this is a release build", true);
#endif
   lFreeElem(&job);
}

/**
 * @brief HDL-06: owning handles free what they own exactly once
 *
 * Freeing twice or not at all shows under the address sanitizer.
 *
 * @return nothing; CHECK records the results
 */
static void
test_owning_handles() {
   ElementPtr<JAT> a = ElementPtr<JAT>::create();
   a.get().set<JAT_task_number>(1u);
   ElementPtr<JAT> b = std::move(a);
   CHECK(7, "a moved owning handle is empty, the target holds the element",
         !a && b && b.get().get<JAT_task_number>() == 1);

   lListElem *released = b.release();
   CHECK(8, "a released element is no longer owned", !b && released != nullptr);
   lFreeElem(&released);

   bool caught = false;
   try {
      // the task number is a unique key, so the two tasks need distinct ones
      ListPtr<JAT> list = ListPtr<JAT>::create("scoped");
      for (const std::uint32_t number : {1u, 2u}) {
         ElementPtr<JAT> task = ElementPtr<JAT>::create();
         task.get().set<JAT_task_number>(number);
         list.get().push_back(std::move(task));
      }
      throw std::runtime_error("leave the scope");
   } catch (const std::runtime_error &) {
      caught = true;
   }
   CHECK(9, "an exception leaving the scope frees the list and its elements", caught);

   ListPtr<JAT> first = ListPtr<JAT>::create("first");
   first.get().push_back(ElementPtr<JAT>::create());
   ListPtr<JAT> second = ListPtr<JAT>::create("second");
   second = std::move(first);
   CHECK(10, "move assignment frees the list held before and takes the other", !first && second.get().size() == 1);

   lListElem *job = lCreateElem(JB_Type);
   ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
   tasks.get().push_back(ElementPtr<JAT>::create());
   Element<JB>{job}.set<JB_ja_tasks>(std::move(tasks));
   CHECK(11, "a setter takes the owning handle over", !tasks && lGetNumberOfElem(lGetList(job, JB_ja_tasks)) == 1);
   lFreeElem(&job);
}

int main(int argc, char *argv[]) {
   lInit(nmv);
   return ocs::module_test::run(argc, argv, {
      {"LIST-01", "ranges and lookups",
       "a typed list iterates and looks up like the old interface",
       {"iterate the task list of a job with range-for and with for_each_ep",
        "look up present and absent keys with find_first and find_all, below and above the hash threshold",
        "in a debug build, read a task list that holds queues"},
       "the same elements in the same order; an absent key gives an empty handle or range; "
       "the wrong sub-list throws naming field and both types",
       {{"HDL-05", "covers"}}, {1, 2, 3, 4, 5, 6}, test_ranges_and_lookups},
      {"LIST-02", "owning handles",
       "owning handles free what they own exactly once",
       {"create, move and release elements; let an exception leave the scope of a list; move-assign lists; "
        "hand a list to a setter"},
       "every object freed once, the released one not by the handle; no leak under the address sanitizer",
       {{"HDL-06", "covers"}}, {7, 8, 9, 10, 11}, test_owning_handles},
   });
}
