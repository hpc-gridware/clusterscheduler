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
 *
 * Also covers the list operations of the typed handles (CS-3077, OPS-01): walking, exchange and swap of list
 * fields, adding and erasing by key, lookups, keeping a list sorted and finding by a condition, each with fixed
 * expected values.
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Sort.h"
#include "tom/ocs_Raw.h"

#include "ocs_module_test.h"
#include "cull/cull_test_api.h"

using namespace ocs::tom;

/**
 * @brief Build a list of tasks with consecutive numbers, the old way
 * @param n the number of tasks
 * @return the list
 */
static lList *
make_tasks(const std::uint32_t n) {
   lList *tasks = lCreateList("tasks", ocs::tom::JAT::descriptor.e);
   for (std::uint32_t i = 1; i <= n; i++) {
      lAddElemUlong(&tasks, JAT_task_number, i, ocs::tom::JAT::descriptor.e);
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
   lListElem *job = lCreateElem(ocs::tom::JB::descriptor.e);
   lSetList(job, JB_ja_tasks, make_tasks(5));

   std::vector<const lListElem *> old_order;
   for (ConstRawElement ep = ConstRawList{lGetList(job, JB_ja_tasks)}.front(); ep; ep = ep.next()) {
      old_order.push_back(ep.raw());
   }
   std::vector<const lListElem *> new_order;
   for (const auto task : Element<JB>{job}.get<JB_ja_tasks>()) {
      new_order.push_back(task);
   }
   CHECK(1, "range-for over the task list of a job yields the elements of a walk with front() and next(), in order",
         new_order == old_order && new_order.size() == 5);

   for (const std::uint32_t n : {5u, 40u}) {
      lList *raw = make_tasks(n);
      // a second task 3, so that find_all has two matches; task numbers are unique, so take another field
      lSetUlong(RawList{raw}.front(), JAT_status, 7);
      lSetUlong(RawList{raw}.back(), JAT_status, 7);
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
   lList *queues = lCreateList("queues", ocs::tom::QU::descriptor.e);
   lAppendElem(queues, lCreateElem(ocs::tom::QU::descriptor.e));
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

   lListElem *job = lCreateElem(ocs::tom::JB::descriptor.e);
   ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
   tasks.get().push_back(ElementPtr<JAT>::create());
   Element<JB>{job}.set<JB_ja_tasks>(std::move(tasks));
   CHECK(11, "a setter takes the owning handle over", !tasks && lGetNumberOfElem(lGetList(job, JB_ja_tasks)) == 1);
   lFreeElem(&job);
}

namespace {

   /** @brief The list lengths the list scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /**
    * @brief Whether a call throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   bool throws_reason(const std::function<void()> &call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /**
    * @brief A job list with consecutive job numbers
    * @param name the list name
    * @param n the number of jobs
    * @param first the job number of the first job, the others count up
    * @return the owning handle
    */
   ListPtr<JB> numbered_jobs(const char *name, const int n, const std::uint32_t first) {
      ListPtr<JB> jobs = ListPtr<JB>::create(name);
      for (int i = 0; i < n; i++) {
         ElementPtr<JB> job = ElementPtr<JB>::create();
         job->set<JB_job_number>(first + i);
         jobs->push_back(std::move(job));
      }
      return jobs;
   }

   /**
    * @brief A task list with the task numbers 1 to n
    * @param n the number of tasks
    * @return the owning handle
    */
   ListPtr<JAT> numbered_tasks(const int n) {
      ListPtr<JAT> list = ListPtr<JAT>::create("tasks");
      for (int i = 1; i <= n; i++) {
         ElementPtr<JAT> task = ElementPtr<JAT>::create();
         task->set<JAT_task_number>(i);
         list->push_back(std::move(task));
      }
      return list;
   }

   /**
    * @brief Whether every job of a list is found through its key
    * @param jobs the list
    * @return true if find_first<JB_job_number>() finds each job itself
    */
   bool every_job_found(const List<JB> jobs) {
      for (const Element<JB> job : jobs) {
         if (jobs.find_first<JB_job_number>(job.get<JB_job_number>()) != job) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief A name made of a prefix and a number
    * @param prefix the prefix, e.g. "c"
    * @param i the number
    * @return the name, e.g. "c3"
    */
   std::string name_of(const char *prefix, const int i) {
      return prefix + std::to_string(i);
   }

   /**
    * @brief A list of complex entries named c1 .. cn, and of execution hosts named host-1 .. host-n
    * @param n the number of entries and hosts
    * @param[out] entries the complex entry list, created here
    * @param[out] hosts the host list, created here
    */
   void fill_entries_and_hosts(const int n, ListPtr<CE> &entries, ListPtr<EH> &hosts) {
      entries = ListPtr<CE>::create("entries");
      hosts = ListPtr<EH>::create("hosts");
      for (int i = 1; i <= n; i++) {
         entries->emplace_back<CE_name>(name_of("c", i).c_str());
         hosts->emplace_back<EH_name>(Host{name_of("host-", i).c_str()});
      }
   }

   /**
    * @brief A list of jobs numbered 1 .. n, owned alternately by alice and bob, job i with i % 3 tasks
    * @param n the number of jobs
    * @return the list
    */
   ListPtr<JB> owned_jobs(const int n) {
      ListPtr<JB> list = ListPtr<JB>::create("jobs");
      for (int i = 1; i <= n; i++) {
         const Element<JB> job = list->push_back(ElementPtr<JB>::create());
         job.set<JB_job_number>(i);
         job.set<JB_owner>(i % 2 == 0 ? "alice" : "bob");
         for (int t = 1; t <= i % 3; t++) {
            job.get_or_create<JB_ja_tasks>().emplace_back<JAT_task_number>(t);
         }
      }
      return list;
   }

   /**
    * @brief A string field read for comparison, "-" for no element
    * @param value the value read, or nullptr
    * @return the value as std::string
    */
   std::string text_of(const char *value) {
      return value != nullptr ? value : "-";
   }

   /** @brief The number of jobs the sort scenarios sort */
   constexpr std::uint32_t SortedJobs = 200;

   /**
    * @brief The priority of a job: five priorities, so that many jobs share one
    * @param number the job number
    * @return the priority
    */
   std::uint32_t priority_of(const std::uint32_t number) {
      return (number * 7) % 5;
   }

   /**
    * @brief The job numbers 1 .. n in a scrambled order
    * @param n the number of jobs
    * @return the numbers
    */
   std::vector<std::uint32_t> scrambled(const std::uint32_t n) {
      std::vector<std::uint32_t> numbers;
      for (std::uint32_t i = 0; i < n; i++) {
         numbers.push_back((i * 37) % n + 1);   // a permutation for an n that 37 does not divide
      }
      return numbers;
   }

   /**
    * @brief The job numbers of a list, in list order
    * @param jobs the list
    * @return the numbers
    */
   std::vector<std::uint32_t> numbers_of(const ListPtr<JB> &jobs) {
      std::vector<std::uint32_t> numbers;
      for (const ConstElement<JB> job : *jobs) {
         numbers.push_back(job.get<JB_job_number>());
      }
      return numbers;
   }

   /**
    * @brief The jobs in scrambled order, each with its priority
    * @param n the number of jobs
    * @return the list
    */
   ListPtr<JB> scrambled_jobs(const std::uint32_t n) {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      for (const std::uint32_t number : scrambled(n)) {
         jobs->emplace_back<JB_job_number>(number).set<JB_priority>(priority_of(number));
      }
      return jobs;
   }

   /**
    * @brief The job numbers sorted by priority, the highest first, and by number: the order "%I-%I+" gives
    * @param numbers the job numbers
    * @param priority the priority of a job number
    * @return the sorted numbers
    */
   template <typename Priority>
   std::vector<std::uint32_t> by_priority_then_number(std::vector<std::uint32_t> numbers, Priority priority) {
      std::sort(numbers.begin(), numbers.end(), [&priority](const std::uint32_t a, const std::uint32_t b) {
         return priority(a) != priority(b) ? priority(a) > priority(b) : a < b;
      });
      return numbers;
   }

   /** @brief One row of the TEST elements the condition scenario runs on */
   struct Row {
      std::uint32_t key;     ///< TEST_uint32, the unique key
      const char *s;         ///< TEST_string, may be nullptr
   };

   /**
    * @brief Four TEST elements keyed 1 to 4; the fourth holds no string
    * @return the owning handle
    */
   ListPtr<TEST> condition_rows() {
      constexpr Row rows[] = {{1, "alpha"}, {2, "beta"}, {3, "Gamma"}, {4, nullptr}};
      ListPtr<TEST> list = ListPtr<TEST>::create("rows");
      for (const Row &r : rows) {
         const Element<TEST> row = list->push_back(ElementPtr<TEST>::create());
         row.set<TEST_uint32>(r.key);
         row.set<TEST_string>(r.s);
      }
      return list;
   }

} // namespace

/**
 * @brief LIST-03: create and free, by scope, reset(), move assignment and release()
 * @return nothing; CHECK records the results
 */
static void
test_create_and_free() {
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(1);
   const std::uint32_t number = job->get<JB_job_number>();
   job.reset();
   const bool reset = !job;
   ListPtr<JB> jobs = numbered_jobs("first", 3, 1);
   jobs = numbered_jobs("second", 2, 1);
   const bool reassigned = jobs->size() == 2 && std::strcmp(jobs->name(), "second") == 0;
   // the bridge to raw pointers: release() gives the list up, the checked constructor takes it back
   lList *raw = jobs.release();
   const bool released = !jobs && raw != nullptr;
   ListPtr<JB> back(raw);
   CHECK(12, "create, reset, move assignment and release free and hand over; the checked constructor takes a "
             "released list back",
         number == 1 && reset && reassigned && released && back->size() == 2);
}

/**
 * @brief LIST-04: walk a list forward and backward
 * @return nothing; CHECK records the results
 */
static void
test_walk() {
   bool ok = true;
   for (const int n : Lengths) {
      ListPtr<JB> jobs = numbered_jobs("typed", n, 1);
      std::vector<std::uint32_t> backward;
      for (Element<JB> job = jobs->back(); job; job = job.prev()) {
         backward.push_back(job.get<JB_job_number>());
      }
      std::vector<std::uint32_t> forward;
      for (Element<JB> job = jobs->front(); job; job = job.next()) {
         forward.push_back(job.get<JB_job_number>());
      }
      std::vector<std::uint32_t> expected;
      for (int i = 1; i <= n; i++) {
         expected.push_back(static_cast<std::uint32_t>(i));
      }
      const std::vector<std::uint32_t> reversed(expected.rbegin(), expected.rend());
      ok = ok && forward == expected && backward == reversed && every_job_found(*jobs);
   }
   // an empty handle walks like an empty list
   const List<JB> none;
   ok = ok && !none.front() && !none.back() && none.size() == 0 && none.name() == nullptr;
   CHECK(13, "front/next walk the jobs 1 to n, back/prev n to 1; an empty handle walks to nothing", ok);
}

/**
 * @brief LIST-05: exchange and swap the task lists of jobs
 * @return nothing; CHECK records the results
 */
static void
test_exchange_and_swap() {
   // two jobs, the first starts with 3 tasks; exchange in 2, then swap with 5 tasks of the second
   ListPtr<JB> jobs = numbered_jobs("typed", 2, 1);
   Element<JB> job = jobs->front();
   Element<JB> second = job.next();
   job.set<JB_ja_tasks>(numbered_tasks(3));
   ListPtr<JAT> previous = job.exchange<JB_ja_tasks>(numbered_tasks(2));
   const bool exchanged = previous->size() == 3 && job.get<JB_ja_tasks>().size() == 2;
   second.set<JB_ja_tasks>(numbered_tasks(5));
   job.swap<JB_ja_tasks, JB_ja_tasks>(second);
   const bool swapped = job.get<JB_ja_tasks>().size() == 5 && second.get<JB_ja_tasks>().size() == 2;
   CHECK(14, "exchange returns the list held before, swap swaps the task lists of two jobs", exchanged && swapped);
}

/**
 * @brief LIST-06: errors: an element of another list, and an empty handle
 * @return nothing; CHECK records the results
 */
static void
test_list_errors() {
   ListPtr<JB> jobs = numbered_jobs("typed", 3, 1);
   ListPtr<JB> other = numbered_jobs("other", 3, 100);
   const bool foreign =
      throws_reason([&] { jobs->erase(other->front()); }, FieldException::Reason::ForeignList) &&
      throws_reason([&] { static_cast<void>(jobs->extract(other->front())); }, FieldException::Reason::ForeignList) &&
      throws_reason([&] { jobs->insert_after(other->front(), ElementPtr<JB>::create()); },
                    FieldException::Reason::ForeignList) &&
      jobs->size() == 3 && other->size() == 3;
   CHECK(15, "erase, extract and insert_after with an element of another list throw ForeignList and change nothing",
         foreign);

   const List<JB> none;
   const bool empty =
      throws_reason([&] { none.push_front(ElementPtr<JB>::create()); }, FieldException::Reason::EmptyHandle) &&
      throws_reason([&] { none.splice(*jobs); }, FieldException::Reason::EmptyHandle) && jobs->size() == 3;
   CHECK(16, "a modifying operation on an empty handle throws EmptyHandle", empty);
}

/**
 * @brief LIST-07: add to a list field of an element
 * @return nothing; CHECK records the results
 */
static void
test_add_to_field() {
   ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   const Element<JB> job = jobs->push_back(ElementPtr<JB>::create());
   const bool empty_before = !job.get<JB_env_list>();
   job.get_or_create<JB_env_list>().emplace_back<VA_variable>("DISPLAY");
   job.get_or_create<JB_env_list>().emplace_back<VA_variable>("HOME");
   std::vector<std::string> variables;
   for (const auto variable : job.get<JB_env_list>()) {
      variables.emplace_back(variable.get<VA_variable>());
   }
   const bool found = static_cast<bool>(job.get<JB_env_list>().find_first<VA_variable>("HOME"));
   CHECK(17, "get_or_create creates the list field once, emplace_back appends to it in order",
         empty_before && variables == std::vector<std::string>({"DISPLAY", "HOME"}) && found);
}

/**
 * @brief LIST-08: erase by key in a list field of an element
 * @return nothing; CHECK records the results
 */
static void
test_erase_in_field() {
   ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   const Element<JB> job = jobs->push_back(ElementPtr<JB>::create());
   job.get_or_create<JB_env_list>().emplace_back<VA_variable>("DISPLAY");
   const auto erased = job.get<JB_env_list>().erase<VA_variable>("DISPLAY");
   const bool kept_empty = job.get<JB_env_list>() && job.get<JB_env_list>().empty();
   CHECK(18, "erase in a list field removes the element with the key and leaves the list in place, empty",
         erased == 1 && kept_empty);
}

/**
 * @brief LIST-09: errors of emplace_back: a null key and a duplicate key in a unique field
 * @return nothing; CHECK records the results
 */
static void
test_keyed_errors() {
   ListPtr<CE> entries = ListPtr<CE>::create("entries");
   entries->emplace_back<CE_name>("c1");
   const bool refused =
      throws_reason([&] { entries->emplace_back<CE_name>(nullptr); }, FieldException::Reason::NullKey) &&
      entries->size() == 1;
   CHECK(19, "a null key throws NullKey, the list unchanged", refused);

#ifdef ENABLE_DEBUG_CHECKS
   const bool duplicate =
      throws_reason([&] { entries->emplace_back<CE_name>("c1"); }, FieldException::Reason::DuplicateKey) &&
      entries->size() == 1;
   CHECK(20, "a duplicate key in a unique field throws DuplicateKey in a debug build, the list unchanged", duplicate);
#else
   CHECK(20, "a duplicate key is checked in a debug build only", true);
#endif
}

/**
 * @brief LIST-10: look up by a number, a string and a host key
 * @return nothing; CHECK records the results
 */
static void
test_by_key() {
   bool ok = true;
   for (const int n : Lengths) {
      ListPtr<CE> entries;
      ListPtr<EH> hosts;
      fill_entries_and_hosts(n, entries, hosts);
      const ListPtr<JB> jobs = owned_jobs(n);
      std::vector<std::string> found;
      const auto job = jobs->find_first<JB_job_number>(3);
      found.push_back(text_of(job ? job.get<JB_owner>() : nullptr));
      found.push_back(jobs->find_first<JB_job_number>(999) ? "found" : "none");
      const auto entry = entries->find_first<CE_name>("c2");
      found.push_back(text_of(entry ? entry.get<CE_name>() : nullptr));
      found.push_back(entries->find_first<CE_name>("none") ? "found" : "none");
      // host names compare as the cluster compares them
      const auto host = hosts->find_first<EH_name>(Host{"HOST-2"});
      found.push_back(text_of(host ? host.get<EH_name>().c_str() : nullptr));
      ok = ok && found == std::vector<std::string>({"bob", "none", "c2", "none", "host-2"});
   }
   CHECK(21, "find_first by a number, a string and a host key finds hits and misses, below and above the hash "
             "threshold",
         ok);
}

/**
 * @brief LIST-11: every match of a key that several elements hold
 * @return nothing; CHECK records the results
 */
static void
test_all_matches() {
   bool ok = true;
   for (const int n : Lengths) {
      const ListPtr<JB> jobs = owned_jobs(n);
      std::vector<std::uint32_t> numbers;
      for (const ConstElement<JB> job : ConstList<JB>(*jobs).find_all<JB_owner>("alice")) {
         numbers.push_back(job.get<JB_job_number>());
      }
      // alice owns the even job numbers
      std::vector<std::uint32_t> expected;
      for (int i = 2; i <= n; i += 2) {
         expected.push_back(static_cast<std::uint32_t>(i));
      }
      ok = ok && numbers == expected;
   }
   CHECK(22, "find_all walks every job of one owner, in list order", ok);
}

/**
 * @brief LIST-12: a lookup in a list field of an element
 * @return nothing; CHECK records the results
 */
static void
test_sub_list() {
   // job 2 has two tasks, job 3 none
   ListPtr<JB> jobs = owned_jobs(5);
   const Element<JB> job2 = jobs->find_first<JB_job_number>(2);
   const Element<JB> job3 = jobs->find_first<JB_job_number>(3);
   const auto task = job2.get<JB_ja_tasks>().find_first<JAT_task_number>(2);
   const std::uint32_t task_number = task ? task.get<JAT_task_number>() : 0;
   // an empty list field is an empty list handle, so the lookup finds nothing
   const bool empty = !job3.get<JB_ja_tasks>().find_first<JAT_task_number>(1);
   CHECK(23, "a lookup in a sub-list finds the task, an empty sub-list finds nothing", task_number == 2 && empty);
}

/**
 * @brief LIST-13: case-insensitive and prefix lookups
 * @return nothing; CHECK records the results
 */
static void
test_nocase_and_like() {
   ListPtr<CE> entries;
   ListPtr<EH> hosts;
   fill_entries_and_hosts(5, entries, hosts);
   const auto found_name = [](const auto entry) { return text_of(entry ? entry.template get<CE_name>() : nullptr); };
   std::vector<std::string> found;
   found.push_back(found_name(entries->find_first_nocase<CE_name>("C3")));
   // a trailing '*' matches any rest: c1 is the first; "c4" matches only itself
   found.push_back(found_name(entries->find_first_like<CE_name>("c*")));
   found.push_back(found_name(entries->find_first_like<CE_name>("c4")));
   found.push_back(found_name(entries->find_first_like<CE_name>("x*")));
   CHECK(24, "find_first_nocase finds without regard to case, find_first_like by a prefix pattern",
         found == std::vector<std::string>({"c3", "c1", "c4", "-"}));
}

/**
 * @brief LIST-14: keep a list sorted while inserting, with a sort order built once
 * @return nothing; CHECK records the results
 */
static void
test_insert_sorted() {
   const SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
   ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   for (const std::uint32_t number : scrambled(SortedJobs)) {
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(number);
      job->set<JB_priority>(priority_of(number));
      jobs->insert_sorted(order, std::move(job));
   }
   const std::vector<std::uint32_t> expected = by_priority_then_number(scrambled(SortedJobs), priority_of);
   CHECK(25, "insert_sorted() keeps the list in the order of the sort order: the highest priority first, then by "
             "number",
         numbers_of(jobs) == expected && expected.size() == SortedJobs);
}

/**
 * @brief LIST-15: move a job to its new place after its priority changed
 * @return nothing; CHECK records the results
 */
static void
test_resort() {
   constexpr std::uint32_t moved = 42;
   const SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
   ListPtr<JB> jobs = scrambled_jobs(SortedJobs);
   jobs->sort(order);
   Element<JB> job = jobs->find_first<JB_job_number>(moved);
   job.set<JB_priority>(9);
   jobs->resort(order, job);
   // the order of a full sort with the new priority: 9 is above every other, so the job comes first
   const std::vector<std::uint32_t> expected = by_priority_then_number(
      scrambled(SortedJobs), [](const std::uint32_t number) { return number == moved ? 9 : priority_of(number); });
   CHECK(26, "resort() moves a changed job to the place a full sort gives it",
         numbers_of(jobs) == expected && expected.front() == moved);
}

/**
 * @brief LIST-16: sort by a key the fields do not express: the last digit of the job number, then the number
 * @return nothing; CHECK records the results
 */
static void
test_sort_with_comparison() {
   std::vector<std::uint32_t> expected = scrambled(SortedJobs);
   std::sort(expected.begin(), expected.end(), [](const std::uint32_t a, const std::uint32_t b) {
      return a % 10 != b % 10 ? a % 10 < b % 10 : a < b;
   });
   ListPtr<JB> jobs = scrambled_jobs(SortedJobs);
   jobs->sort([](const ConstElement<JB> a, const ConstElement<JB> b) {
      const std::uint32_t x = a.get<JB_job_number>();
      const std::uint32_t y = b.get<JB_job_number>();
      return x % 10 != y % 10 ? x % 10 < y % 10 : x < y;
   });
   CHECK(27, "sort(comparison) sorts by a computed key", numbers_of(jobs) == expected);
}

/**
 * @brief LIST-17: the sort operations throw on an empty order, a foreign element and a missing field
 * @return nothing; CHECK records the results
 */
static void
test_sort_errors() {
   ListPtr<JB> jobs = scrambled_jobs(5);
   ListPtr<JB> others = scrambled_jobs(5);
   const SortOrder<JB> order = sort_order<asc<JB_priority>>();
   const SortOrder<JB> empty;
   const bool empty_order = throws_reason([&] { jobs->insert_sorted(empty, ElementPtr<JB>::create()); },
                                          FieldException::Reason::NullElement);
   const bool foreign = throws_reason([&] { jobs->resort(order, others->find_first<JB_job_number>(1)); },
                                      FieldException::Reason::ForeignList);
   ListPtr<JB> reduced = jobs->select(ocs::tom::what<JB_job_number>());
   const bool absent = throws_reason([&] { reduced->sort(order); }, FieldException::Reason::AbsentField);
   CHECK(28, "an empty order throws NullElement, an element of another list ForeignList, a missing field "
             "AbsentField",
         empty_order && foreign && absent);
}

/**
 * @brief LIST-18: find forward and backward by a condition, and test one element
 * @return nothing; CHECK records the results
 */
static void
test_find_by_condition() {
   const ListPtr<TEST> rows = condition_rows();
   const Where<TEST> odd = field<TEST_uint32>().has_all_bits(1);
   const Element<TEST> last = rows->find_last_if(odd);
   std::vector<std::uint32_t> seen;
   seen.push_back(rows->find_if(odd).get<TEST_uint32>());
   seen.push_back(rows->find_if(odd).next_if(odd).get<TEST_uint32>());
   seen.push_back(last.get<TEST_uint32>());
   seen.push_back(last.prev_if(odd).get<TEST_uint32>());
   seen.push_back(rows->front().matches(odd));
   seen.push_back(rows->front().next().matches(odd));
   // the odd keys are 1 and 3
   CHECK(29, "find_if, next_if, find_last_if, prev_if and matches find the odd keys 1 and 3 forward and backward",
         seen == std::vector<std::uint32_t>({1, 3, 3, 1, 1, 0}));
}

int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"LIST-01", "ranges and lookups",
       "a typed list iterates and looks up like the old interface",
       {"iterate the task list of a job with range-for and with front() and next()",
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
      {"LIST-03", "create and free", "owning handles create, reset, move-assign and hand over a list",
       {"create and reset a job; move-assign a job list; release a list and take it back"},
       "everything freed once; the list taken back holds its jobs", {{"OPS-01", "covers"}}, {12},
       test_create_and_free},
      {"LIST-04", "walk", "front/next and back/prev walk a list",
       {"walk a list of 5 and of 40 jobs forward and backward; walk an empty handle"},
       "the job numbers in order and reversed; nothing for an empty handle", {{"OPS-01", "covers"}}, {13},
       test_walk},
      {"LIST-05", "exchange and swap", "exchange and swap move list fields",
       {"exchange a job's task list, swap the task lists of two jobs"}, "the task counts follow",
       {{"OPS-01", "covers"}}, {14}, test_exchange_and_swap},
      {"LIST-06", "list errors", "foreign elements and empty handles throw",
       {"erase, extract and insert after an element of another list; modify an empty handle"},
       "ForeignList and EmptyHandle thrown, nothing changed", {{"OPS-01", "covers"}}, {15, 16}, test_list_errors},
      {"LIST-07", "add to a field", "get_or_create and emplace_back add to a list field",
       {"add two variables to the environment of a job without one"}, "the list created, both in order",
       {{"OPS-01", "covers"}}, {17}, test_add_to_field},
      {"LIST-08", "erase in a field", "erase by key in a list field",
       {"erase the only variable of a job's environment"}, "removed; the list stays, empty",
       {{"OPS-01", "covers"}}, {18}, test_erase_in_field},
      {"LIST-09", "keyed errors", "emplace_back throws on a null key and a duplicate key",
       {"add by a null name; add a name the list holds"}, "NullKey and, in a debug build, DuplicateKey thrown",
       {{"OPS-01", "covers"}}, {19, 20}, test_keyed_errors},
      {"LIST-10", "by key", "find_first by a number, a string and a host key",
       {"look up jobs by number, complex entries by name and hosts by name, in lists of 5 and 40"},
       "the element or none", {{"OPS-01", "covers"}}, {21}, test_by_key},
      {"LIST-11", "all matches", "find_all walks every element with a key",
       {"walk the jobs of one owner"}, "the even job numbers in order", {{"OPS-01", "covers"}}, {22},
       test_all_matches},
      {"LIST-12", "sub-list", "a lookup in a list field",
       {"look up a task of a job, and in a job without tasks"}, "the task; none for an empty sub-list",
       {{"OPS-01", "covers"}}, {23}, test_sub_list},
      {"LIST-13", "nocase and like", "case-insensitive and prefix lookups",
       {"look up a complex entry by name in other case and by prefix"}, "the first match or none",
       {{"OPS-01", "covers"}}, {24}, test_nocase_and_like},
      {"LIST-14", "insert sorted", "insert_sorted() keeps a list sorted",
       {"insert 200 jobs one by one into a list sorted by priority and number"}, "the order of a full sort",
       {{"OPS-01", "covers"}}, {25}, test_insert_sorted},
      {"LIST-15", "resort", "resort() moves a changed element",
       {"raise the priority of one job of a sorted list and move it"}, "the order of a full sort, the job first",
       {{"OPS-01", "covers"}}, {26}, test_resort},
      {"LIST-16", "comparison", "sort(comparison) sorts by a key the fields do not express",
       {"sort jobs by the last digit of their number, then by number"}, "the order of a plain std::sort",
       {{"OPS-01", "covers"}}, {27}, test_sort_with_comparison},
      {"LIST-17", "sort errors", "the sort operations throw on an empty order, a foreign element and a missing field",
       {"insert with an empty order, resort an element of another list, sort a reduced list"},
       "NullElement, ForeignList and AbsentField thrown", {{"OPS-01", "covers"}}, {28}, test_sort_errors},
      {"LIST-18", "find by condition", "find_if and its siblings find by a condition",
       {"find the odd keys forward and backward, test two elements"}, "the keys 1 and 3; 1 and 0",
       {{"OPS-01", "covers"}}, {29}, test_find_by_condition},
   });
}
