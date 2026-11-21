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
 * @brief Module test of the free forms of ocs_FreeAccess.h (CS-3045, OPS-02)
 *
 * The free forms take raw pointers or handles and keep the run-time behaviour
 * of the functions they replaced: the lookups by key and their iterating
 * forms, adding and erasing by key in a list and in the list of a list field,
 * exchange and swap, the bit operations, get_or_create, list_ref and
 * get_or_empty, the owning-handle overloads, keep_last, merge_by_key and
 * modify_sublist, and the members of the handles that forward to them. Every
 * expected value is fixed; where the derivation is not obvious from the test
 * data, a comment states it.
 *
 * Where a free form throws a FieldException that nothing catches, the process
 * ends, as the replaced function's abort did; such cases run in a child
 * process.
 */

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "tom/ocs_Types.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/detail/ocs_State.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

#include "ocs_module_test.h"

namespace {

   /** @brief The list lengths the scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /** @brief The exit code of a child process that aborted */
   constexpr int Aborted = -1;

   /**
    * @brief Run a call in a child process with its output silenced and give its result
    *
    * Keeps expected messages and aborts out of the test output.
    *
    * @param call the call, returning a value from 0 to 255
    * @return the value, or #Aborted if the child ended with SIGABRT
    */
   int in_child(const std::function<int()> &call) {
      std::fflush(nullptr);
      const pid_t pid = fork();
      if (pid == 0) {
         [[maybe_unused]] FILE *quiet_stderr = std::freopen("/dev/null", "w", stderr);
         [[maybe_unused]] FILE *quiet_stdout = std::freopen("/dev/null", "w", stdout);
         _exit(call());
      }
      int status = 0;
      waitpid(pid, &status, 0);
      if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
         return Aborted;
      }
      return WIFEXITED(status) ? WEXITSTATUS(status) : -2;
   }

   /**
    * @brief A list of TEST elements with keys of every lookup type, some repeated where the field is not unique
    *
    * Element i holds TEST_uint32 i, TEST_uint64 i % 3, TEST_string "s<i % 4>" and TEST_host "h<i % 2>".
    *
    * @param n the number of elements
    * @return the owner of the list
    */
   ocs::tom::ListPtr<ocs::tom::TEST> make_test_list(const int n) {
      auto lp = ocs::tom::ListPtr<ocs::tom::TEST>::create("test");
      for (int i = 0; i < n; i++) {
         const auto ep = lp->push_back(ocs::tom::ElementPtr<ocs::tom::TEST>::create());
         ep.set<TEST_uint32>(static_cast<std::uint32_t>(i));
         ep.set<TEST_uint64>(static_cast<std::uint64_t>(i % 3));
         const std::string s = "s" + std::to_string(i % 4);
         ep.set<TEST_string>(s.c_str());
         const std::string h = "h" + std::to_string(i % 2);
         ep.set<TEST_host>(ocs::tom::Host{h.c_str()});
      }
      return lp;
   }

   /**
    * @brief A list of variables with the given names
    * @param names the names
    * @return the owner of the list
    */
   ocs::tom::ListPtr<ocs::tom::VA> make_names(std::initializer_list<const char *> names) {
      auto vars = ocs::tom::ListPtr<ocs::tom::VA>::create("vars");
      for (const char *name : names) {
         vars->emplace_back<VA_variable>(name);
      }
      return vars;
   }

   /**
    * @brief A job with the environment "A", "b" and the array tasks 1 and 2, for the forms on a list field
    * @return the owner of the job
    */
   ocs::tom::ElementPtr<ocs::tom::JB> make_job() {
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      job->set<JB_env_list>(make_names({"A", "b"}));
      auto tasks = ocs::tom::ListPtr<ocs::tom::JAT>::create("tasks");
      tasks->emplace_back<JAT_task_number>(1U);
      tasks->emplace_back<JAT_task_number>(2U);
      job->set<JB_ja_tasks>(std::move(tasks));
      return job;
   }

   /**
    * @brief A job reduced to its number, which holds none of the list fields
    * @return the owner of the job
    */
   ocs::tom::ElementPtr<ocs::tom::JB> make_reduced_job() {
      return ocs::tom::ElementPtr<ocs::tom::JB>::create(ocs::tom::layout<JB_job_number>());
   }

   /**
    * @brief The keys of a list of a string field, in order, as one string
    * @param lp the list, or nullptr
    * @param nm the string field
    * @return the keys followed by a blank each, "null" for a null list
    */
   std::string keys(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return "null";
      }
      std::string result;
      for (const auto ep : ocs::tom::ConstRawList(lp)) {
         result += ep.get<const char *>(nm);
         result += ' ';
      }
      return result;
   }

   /**
    * @brief The numbers of a list of a 32 bit field, in order, as one string
    * @param lp the list, or nullptr
    * @param nm the 32 bit field
    * @return the numbers followed by a blank each, "null" for a null list
    */
   std::string numbers(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return "null";
      }
      std::string result;
      for (const auto ep : ocs::tom::ConstRawList(lp)) {
         result += std::to_string(ep.get<std::uint32_t>(nm));
         result += ' ';
      }
      return result;
   }

   /**
    * @brief A call's result followed by a value read after the call, as one string
    *
    * The operands of + are not sequenced, so the call has to run before the
    * value is read in a statement of its own.
    *
    * @param result the call's result
    * @param after reads the value after the call
    * @return "result value"
    */
   template <typename After>
   std::string then(const int result, After after) {
      return std::to_string(result) + " " + std::to_string(after());
   }

   /**
    * @brief Print what the free forms gave and what was expected, when they differ
    * @param what the name of the scenario
    * @param seen what the free forms gave
    * @param expected what they should give
    */
   void show_if_differs(const char *what, const std::vector<std::string> &seen,
                        const std::vector<std::string> &expected) {
      if (seen == expected) {
         return;
      }
      const auto print = [](const char *label, const std::vector<std::string> &values) {
         printf("   %-8s", label);
         for (const std::string &v : values) {
            printf(" [%s]", v.c_str());
         }
         printf("\n");
      };
      printf("%s differs:\n", what);
      print("seen", seen);
      print("expected", expected);
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief FREE-01: walking every element with a key, find_first<F>(lp, key, &it) and find_next<F>()
 */
void test_find_all() {
   // the TEST_uint32 numbers of the walked elements, as one string
   const auto walk = [](auto first, auto next) {
      std::string seen;
      const void *it = nullptr;
      for (auto ep = first(&it); ep != nullptr; ep = next(&it)) {
         seen += std::to_string(ep.template get<TEST_uint32>()) + " ";
      }
      return seen;
   };
   // the numbers i < n for which a predicate holds, as one string: the elements make_test_list() gave the key
   const auto expect = [](const int n, auto predicate) {
      std::string seen;
      for (int i = 0; i < n; i++) {
         if (predicate(i)) {
            seen += std::to_string(i) + " ";
         }
      }
      return seen;
   };
   bool same = true;
   for (const int n : Lengths) {
      auto owner = make_test_list(n);
      lList *lp = owner.get().raw();
      const lList *clp = lp;
      same = same &&
             walk([&](const void **it) { return ocs::tom::find_first<TEST_string>(clp, "s1", it); },
                  [&](const void **it) { return ocs::tom::find_next<TEST_string>(clp, "s1", it); }) ==
                expect(n, [](const int i) { return i % 4 == 1; }) &&
             walk([&](const void **it) { return ocs::tom::find_first_rw<TEST_host>(lp, "h0", it); },
                  [&](const void **it) { return ocs::tom::find_next_rw<TEST_host>(lp, "h0", it); }) ==
                expect(n, [](const int i) { return i % 2 == 0; }) &&
             walk([&](const void **it) { return ocs::tom::find_first<TEST_uint64>(clp, 1, it); },
                  [&](const void **it) { return ocs::tom::find_next<TEST_uint64>(clp, 1, it); }) ==
                expect(n, [](const int i) { return i % 3 == 1; }) &&
             walk([&](const void **it) { return ocs::tom::find_first_rw<TEST_uint32>(lp, 4, it); },
                  [&](const void **it) { return ocs::tom::find_next_rw<TEST_uint32>(lp, 4, it); }) ==
                expect(n, [](const int i) { return i == 4; });
   }
   CHECK(1, "find_first<F>(lp, key, &it) and find_next<F>() walk every element with the key, in order, below and "
            "above the hash threshold",
         same);
}

/**
 * @brief FREE-02: lookups without regard to case and by a prefix pattern
 */
void test_find_string() {
   auto owner = make_test_list(10);
   lList *lp = owner.get().raw();
   const lList *clp = lp;
   const auto number = [](const auto ep) { return ep ? static_cast<int>(ep.template get<TEST_uint32>()) : -1; };
   // "S2" matches "s2" without regard to case: element 2 is the first; "s*" is a prefix pattern, element 0 the first
   const bool same = number(ocs::tom::find_first_nocase<TEST_string>(clp, "S2")) == 2 &&
                     number(ocs::tom::find_first_nocase_rw<TEST_string>(lp, "x")) == -1 &&
                     number(ocs::tom::find_first_like<TEST_string>(clp, "s3")) == 3 &&
                     number(ocs::tom::find_first_like_rw<TEST_string>(lp, "s*")) == 0 &&
                     ocs::tom::find_first_nocase<TEST_string>(nullptr, "S2") == nullptr &&
                     ocs::tom::find_first_like<TEST_string>(clp, nullptr) == nullptr;
   CHECK(2, "find_first_nocase<F> finds without regard to case, find_first_like<F> by a prefix pattern; a null list "
            "or pattern finds nothing",
         same);
}

/**
 * @brief FREE-03: lookups in the list of a list field
 */
void test_find_in() {
   auto job_owner = make_job();
   lListElem *job = job_owner.get().raw();
   const lListElem *b = job_owner->get<JB_env_list>().back().raw();
   const lListElem *task2 = job_owner->get<JB_ja_tasks>().back().raw();
   const bool found = ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "b") == b &&
                      ocs::tom::find_first_in_rw<JB_env_list, VA_variable>(job, "x") == nullptr &&
                      ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2) == task2 &&
                      b != nullptr && task2 != nullptr &&
                      ocs::tom::find_first_in<JB_env_list, VA_variable>(nullptr, "b") == nullptr;
   CHECK(3, "find_first_in<LF, F> finds the element of the sub-list with the key, nothing for an unknown key or a "
            "null element",
         found);

   // a job without the list field: the free form throws, which nothing catches
   auto rjob_owner = make_reduced_job();
   const lListElem *rjob = rjob_owner.get().raw();
   const bool aborts =
      in_child([&] { return ocs::tom::find_first_in<JB_env_list, VA_variable>(rjob, "b") != nullptr; }) == Aborted;
   CHECK(4, "an element without the list field ends the process", aborts);
}

/**
 * @brief FREE-04: adding and erasing by a key in a list
 */
void test_add_erase() {
   std::vector<std::string> seen;
   lList *strings = nullptr;
   lList *ulongs = nullptr;
   const auto added = [](const auto ep) { return std::string(ep != nullptr ? "added" : "null"); };
   seen.push_back(added(ocs::tom::emplace_back<TEST_string>(&strings, "a")));
   seen.push_back(added(ocs::tom::emplace_back<TEST_string>(&strings, "b")));
   seen.push_back(added(ocs::tom::emplace_back<TEST_string>(&strings, nullptr)));
   seen.push_back(added(ocs::tom::emplace_back<TEST_string>(nullptr, "c")));
   seen.push_back(added(ocs::tom::emplace_back<TEST_uint32>(&ulongs, 7)));
   seen.push_back(keys(strings, TEST_string));
   seen.push_back(numbers(ulongs, TEST_uint32));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(&strings, "x")));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(&strings, "a")));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(&strings, nullptr)));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(nullptr, "b")));
   seen.push_back(keys(strings, TEST_string));
   // the last element removed frees the list
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(&strings, "b")));
   seen.push_back(keys(strings, TEST_string));
   // a list that is nullptr already gives 1
   seen.push_back(std::to_string(ocs::tom::erase<TEST_string>(&strings, "b")));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_uint32>(&ulongs, 8)));
   seen.push_back(std::to_string(ocs::tom::erase<TEST_uint32>(&ulongs, 7)));
   seen.push_back(numbers(ulongs, TEST_uint32));
   ocs::tom::free_raw_list(strings);
   ocs::tom::free_raw_list(ulongs);

   const std::vector<std::string> expected{"added", "added", "null", "null", "added", "a b ", "7 ",
                                           "0",     "1",     "0",    "0",    "b ",    "1",   "null",
                                           "1",     "0",     "1",    "null"};
   show_if_differs("add and erase", seen, expected);
   CHECK(5, "emplace_back<F> creates the list on demand and gives nullptr for a null argument; erase<F> gives 0 for "
            "nothing found, 1 for a removal and frees an emptied list",
         seen == expected);

   // a list of another type: the old message, the soft path and nullptr, a delete finds nothing
   auto tasks_owner = ocs::tom::ListPtr<ocs::tom::JAT>::create("tasks");
   tasks_owner->push_back(ocs::tom::ElementPtr<ocs::tom::JAT>::create());
   lList *tasks = tasks_owner.get().raw();
   const std::vector<int> foreign{
      in_child([&] { return ocs::tom::emplace_back<TEST_host>(&tasks, "h") == nullptr ? 0 : 1; }),
      in_child([&] { return ocs::tom::erase<TEST_host>(&tasks, "h"); })};
   CHECK(6, "adding to or deleting from a list of another type gives nullptr and 0",
         foreign == std::vector<int>({0, 0}));
}

/**
 * @brief FREE-05: adding and erasing by a key in the list of a list field
 */
void test_add_erase_in() {
   std::vector<std::string> seen;
   auto job_owner = make_job();
   lListElem *job = job_owner.get().raw();
   seen.push_back(ocs::tom::emplace_back_in<JB_env_list, VA_variable>(job, "C") != nullptr ? "added" : "null");
   seen.push_back(ocs::tom::emplace_back_in<JB_env_list, VA_variable>(nullptr, "D") != nullptr ? "added" : "null");
   seen.push_back(std::to_string(ocs::tom::erase_in<JB_env_list, VA_variable>(job, "A")));
   seen.push_back(std::to_string(ocs::tom::erase_in<JB_env_list, VA_variable>(job, "x")));
   seen.push_back(std::to_string(ocs::tom::erase_in<JB_env_list, VA_variable>(nullptr, "b")));
   seen.push_back(keys(job_owner->get<JB_env_list>().raw(), VA_variable));
   seen.push_back(ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(job, 3) != nullptr ? "added" : "null");
   seen.push_back(std::to_string(ocs::tom::erase_in<JB_ja_tasks, JAT_task_number>(job, 1)));
   seen.push_back(numbers(job_owner->get<JB_ja_tasks>().raw(), JAT_task_number));

   const std::vector<std::string> expected{"added", "null", "1", "0", "0", "b C ", "added", "1", "2 3 "};
   show_if_differs("add and erase in", seen, expected);
   CHECK(7, "emplace_back_in<LF, F> appends to the sub-list, erase_in<LF, F> removes by key; a null element does "
            "nothing",
         seen == expected);

   // a job without the list field: the free forms throw, which nothing catches
   auto rjob_owner = make_reduced_job();
   lListElem *rjob = rjob_owner.get().raw();
   const bool aborts =
      in_child([&] { return ocs::tom::emplace_back_in<JB_env_list, VA_variable>(rjob, "X") != nullptr; }) ==
         Aborted &&
      in_child([&] { return ocs::tom::erase_in<JB_env_list, VA_variable>(rjob, "X"); }) == Aborted;
   CHECK(8, "an element without the list field ends the process", aborts);
}

/**
 * @brief FREE-06: the presence check
 */
void test_has() {
   auto job = make_job();
   auto rjob = make_reduced_job();
   const lListElem *task = job->get<JB_ja_tasks>().front().raw();
   std::vector<bool> seen;
   for (const lListElem *ep : {static_cast<const lListElem *>(job.get().raw()),
                               static_cast<const lListElem *>(rjob.get().raw()), task,
                               static_cast<const lListElem *>(nullptr)}) {
      seen.push_back(ocs::tom::has<JB_env_list>(ep));
      seen.push_back(ocs::tom::has<JB_job_number>(ep));
   }
   CHECK(9, "has<F> is true for a full job, for the held field of a reduced job, false for an absent field, an "
            "element of another type and a null element",
         seen == std::vector<bool>({true, true, false, true, false, false, false, false}));
}

/**
 * @brief FREE-07: exchanging and swapping list fields, exchanging a string
 */
void test_exchange() {
   std::vector<std::string> seen;
   auto a_owner = make_job();
   auto b_owner = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   b_owner->set<JB_env_list>(make_names({"Z"}));
   lListElem *a = a_owner.get().raw();
   lListElem *b = b_owner.get().raw();
   lList *other = make_names({"Q"}).release();
   const auto env = [](const ocs::tom::ElementPtr<ocs::tom::JB> &job) {
      return keys(job->get<JB_env_list>().raw(), VA_variable);
   };
   ocs::tom::detail::state_set_lerrno(0);
   seen.push_back(std::to_string(ocs::tom::exchange<JB_env_list>(a, &other)));
   seen.push_back(env(a_owner) + "/" + keys(other, VA_variable));
   seen.push_back(then(ocs::tom::exchange<JB_env_list>(nullptr, &other), ocs::tom::detail::state_get_lerrno));
   ocs::tom::detail::state_set_lerrno(0);
   seen.push_back(then(ocs::tom::exchange<JB_env_list>(a, nullptr), ocs::tom::detail::state_get_lerrno));
   seen.push_back(std::to_string(ocs::tom::swap<JB_env_list, JB_env_list>(a, b)));
   seen.push_back(env(a_owner) + "/" + env(b_owner));
   ocs::tom::detail::state_set_lerrno(0);
   seen.push_back(then(ocs::tom::swap<JB_env_list, JB_env_list>(b, nullptr), ocs::tom::detail::state_get_lerrno));
   seen.push_back(env(b_owner));

   auto t = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
   t->set<TEST_string>("held");
   char *s = strdup("new");
   seen.push_back(std::to_string(ocs::tom::exchange<TEST_string>(t.get().raw(), &s)));
   seen.push_back(std::string(t->get<TEST_string>()) + "/" + s);
   free(s);
   ocs::tom::free_raw_list(other);

   // a swap with a null second element changes neither element (lSwapList() lost the first's list there)
   const std::string elemnull = std::to_string(LEELEMNULL);
   const std::vector<std::string> expected{
      "0", "Q /A b ", "-1 " + elemnull, "-1 " + elemnull, "0", "Z /Q ", "-1 " + elemnull, "Q ", "0", "new/held"};
   show_if_differs("exchange", seen, expected);
   CHECK(10, "exchange<F> and swap<F, G> move the lists and the string, and give -1 with LEELEMNULL for a null "
             "argument, changing nothing",
         seen == expected);
}

/**
 * @brief FREE-08: a list created on demand, a list reference, a string never null, bit masks
 */
void test_small() {
   std::vector<std::string> seen;
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lList *created = ocs::tom::get_or_create<JB_env_list>(job.get().raw(), "env");
   seen.push_back(created != nullptr && created == job->get<JB_env_list>().raw() ? "created" : "none");
   seen.push_back(ocs::tom::get_or_create<JB_env_list>(job.get().raw(), "env") == created ? "same" : "other");
   seen.push_back(ocs::tom::get_or_create<JB_env_list>(nullptr, "env") == nullptr ? "null" : "list");
   lList **ref = ocs::tom::list_ref<JB_env_list>(static_cast<const lListElem *>(job.get().raw()));
   seen.push_back(ref != nullptr && *ref == created ? "ref" : "other");

   auto t = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
   const lListElem *t_raw = t.get().raw();
   seen.push_back(std::string("[") + ocs::tom::get_or_empty<TEST_string>(t_raw) + "]");
   t->set<TEST_string>("x");
   seen.push_back(std::string("[") + ocs::tom::get_or_empty<TEST_string>(t_raw) + "]");

   auto task_owner = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   lListElem *task = task_owner.get().raw();
   task_owner->set<JAT_status>(0x5);
   const auto status = [&] { return task_owner->get<JAT_status>(); };
   seen.push_back(ocs::tom::has_any_bits<JAT_status>(task, 0x4) ? "match" : "no");
   seen.push_back(ocs::tom::has_any_bits<JAT_status>(task, 0x2) ? "match" : "no");
   seen.push_back(then(ocs::tom::or_bits<JAT_status>(task, 0x2), status));
   seen.push_back(then(ocs::tom::and_bits<JAT_status>(task, 0x6), status));
   seen.push_back(then(ocs::tom::clear_bits<JAT_status>(task, 0x2), status));
   ocs::tom::detail::state_set_lerrno(0);
   seen.push_back(then(ocs::tom::clear_bits<JAT_status>(static_cast<lListElem *>(nullptr), 0x2),
                       ocs::tom::detail::state_get_lerrno));

   // 5 | 2 = 7, 7 & 6 = 6, 6 & ~2 = 4
   const std::vector<std::string> expected{"created", "same", "null",  "ref",  "[]",
                                           "[x]",     "match", "no",   "0 7", "0 6",
                                           "0 4",     "-1 " + std::to_string(LEELEMNULL)};
   show_if_differs("small ones", seen, expected);
   CHECK(11, "get_or_create creates the list once, list_ref gives the field's slot, get_or_empty never nullptr, the "
             "bit mask forms set and test bits and give -1 with LEELEMNULL for a null element",
         seen == expected);
}

/**
 * @brief FREE-09: an owning element handle takes the free forms, and the calls that take an element over take it
 *        moved in (rule L3)
 *
 * A local variable that owns its element becomes an ElementPtr: its field
 * accesses stay as they are, a call that takes the element over gets
 * std::move, and the destructor frees what no call took over. The handle
 * gives up its element only when the call took it.
 */
void test_owning() {
   // the explicit constructor from a raw element, which checks its type
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>(make_job().release());
   job->set<JB_job_number>(7);
   bool same = ocs::tom::add<JB_job_number>(job, 3) == 0 && job->get<JB_job_number>() == 10 &&
               job->has<JB_env_list>() &&
               ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "A") ==
                  ocs::tom::find_first_in<JB_env_list, VA_variable>(job.get(), "A") &&
               ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2) != nullptr &&
               ocs::tom::emplace_back_in<JB_env_list, VA_variable>(job, "C") != nullptr &&
               ocs::tom::erase_in<JB_env_list, VA_variable>(job, "C") == 1 &&
               ocs::tom::get_or_create<JB_env_list>(job, nullptr) == job->get<JB_env_list>().raw() &&
               *ocs::tom::list_ref<JB_env_list>(job) == job->get<JB_env_list>().raw() &&
               std::string(ocs::tom::get_or_empty<JB_owner>(job)).empty();
   char *owner = strdup("me");
   same = same && ocs::tom::exchange<JB_owner>(job, &owner) == 0 && owner == nullptr &&
          std::string(ocs::tom::get_or_empty<JB_owner>(job)) == "me";
   auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   task->set<JAT_status>(0x5);
   same = same && ocs::tom::has_any_bits<JAT_status>(task, 0x4) && ocs::tom::or_bits<JAT_status>(task, 0x2) == 0 &&
          ocs::tom::and_bits<JAT_status>(task, 0x6) == 0 && ocs::tom::clear_bits<JAT_status>(task, 0x2) == 0 &&
          task->get<JAT_status>() == 0x4;
   CHECK(12, "the free forms that remain on an owning element handle give what they give on the handle it owns", same);

   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   lListElem *const held = job.get().raw();
   ocs::tom::append(jobs.get().raw(), std::move(job));
   const bool appended = job == nullptr && jobs->front().raw() == held;
   auto second = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lListElem *const second_raw = second.get().raw();
   ocs::tom::insert_after(jobs.get().raw(), nullptr, std::move(second));
   const bool inserted = second == nullptr && jobs->front().raw() == second_raw;
   auto kept = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   bool not_taken = false;
   try {
      ocs::tom::append(nullptr, std::move(kept));
   } catch (const ocs::tom::FieldException &e) {
      not_taken = e.reason() == ocs::tom::FieldException::Reason::NullList && kept != nullptr;
   }
   CHECK(13, "append and insert_after take a moved owner's element over and leave the owner empty; on an error the "
             "owner keeps it",
         appended && inserted && not_taken);

   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   lListElem *const pe_raw = pe.get().raw();
   auto env = ocs::tom::ListPtr<ocs::tom::VA>::create("env");
   lList *const env_raw = env.get().raw();
   bool stored = ocs::tom::set<JAT_pe_object>(task.get().raw(), std::move(pe)) == 0 && pe == nullptr &&
                 task->get<JAT_pe_object>().raw() == pe_raw;
   lListElem *const first = jobs->front().raw();
   stored = stored && ocs::tom::set<JB_env_list>(first, std::move(env)) == 0 && env == nullptr &&
            jobs->front().get<JB_env_list>().raw() == env_raw;
   CHECK(14, "set<F> takes a moved list or element over", stored);

   auto bound = task->get<JAT_pe_object>().clone();
   lListElem *const bound_raw = bound.get().raw();
   auto other_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   const bool first_store =
      ocs::tom::set<JAT_pe_object>(other_task.get().raw(), std::move(bound)) == 0 && bound == nullptr;
   // an element bound to a field is refused by a second one: the moved-in handle gives it up, the element stays
   // bound to other_task (which frees it), and the refusing field keeps its own element
   lListElem *const kept_raw = task->get<JAT_pe_object>().raw();
   auto rebound = ocs::tom::ElementPtr<ocs::tom::PE>::from_typed(bound_raw);
   const bool refused = ocs::tom::set<JAT_pe_object>(task.get().raw(), std::move(rebound)) == -1 &&
                        rebound == nullptr &&
                        other_task->get<JAT_pe_object>().raw() == bound_raw &&
                        task->get<JAT_pe_object>().raw() == kept_raw;
   CHECK(15, "a field refusing a bound element leaves it where it is bound, the moved-in handle gives it up",
         first_store && refused);

   auto freed = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   const bool tests = static_cast<bool>(freed) && freed != nullptr;
   freed.reset();
   const ocs::tom::ListPtr<ocs::tom::VA> empty_list;
   CHECK(16, "an owning handle compares with nullptr and tests as a condition, empty after reset()",
         tests && freed == nullptr && !freed && empty_list == nullptr);

   auto vars = ocs::tom::ListPtr<ocs::tom::VA>::create("vars");
   auto var = ocs::tom::ElementPtr<ocs::tom::VA>::create();
   var->set<VA_variable>("A");
   vars->push_back(std::move(var));
   const ocs::tom::ConstList<ocs::tom::VA> view = vars;
   const ocs::tom::List<ocs::tom::VA> rw_view = vars;
   const lListElem *const front = view.front();
   const bool views = view.raw() == vars.get().raw() && rw_view.raw() == vars.get().raw() && view.size() == 1 &&
                      view.back().raw() == front && rw_view.front().raw() == front && rw_view.back().raw() == front;
   const bool empty_views = ocs::tom::ConstList<ocs::tom::VA>(empty_list).raw() == nullptr;
   static_assert(!std::is_constructible_v<ocs::tom::ConstList<ocs::tom::VA>, ocs::tom::ListPtr<ocs::tom::VA> &&>,
                 "no view of a temporary owning handle");
   static_assert(!std::is_constructible_v<ocs::tom::List<ocs::tom::VA>, ocs::tom::ListPtr<ocs::tom::VA> &&>,
                 "no view of a temporary owning handle");
   CHECK(17, "an owning list handle is viewed by List and ConstList, also when empty", views && empty_views);

   auto src = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   auto dst = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   src->set<JB_job_number>(4711);
   dst->set<JB_job_name>("old");
   auto src_env = ocs::tom::ListPtr<ocs::tom::VA>::create("env");
   auto src_var = ocs::tom::ElementPtr<ocs::tom::VA>::create();
   src_var->set<VA_variable>("X");
   src_env->push_back(std::move(src_var));
   src->set<JB_env_list>(std::move(src_env));
   const bool number = ocs::tom::copy_if_present<JB_job_number>(*dst, *src) &&
                       dst->get<JB_job_number>() == 4711;
   // the source holds the name field with nullptr: it is copied as it is
   const bool null_string = ocs::tom::copy_if_present<JB_job_name>(*dst, *src) &&
                            dst->get<JB_job_name>() == nullptr;
   const bool deep = ocs::tom::copy_if_present<JB_env_list>(*dst, *src) &&
                     dst->get<JB_env_list>().size() == 1 &&
                     dst->get<JB_env_list>().raw() != src->get<JB_env_list>().raw();
   const ocs::tom::ConstElement<ocs::tom::JB> none;
   const bool absent = !ocs::tom::copy_if_present<JB_job_number>(*dst, none) &&
                       dst->get<JB_job_number>() == 4711;
   CHECK(18, "copy_if_present copies a value, a nullptr string and a deep list copy, and nothing from a source "
             "without the field", number && null_string && deep && absent);
}

/**
 * @brief Build a variable list from name/value pairs
 * @param pairs the pairs, a nullptr name for an entry without a name
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::VA>
make_vars(std::initializer_list<std::pair<const char *, const char *>> pairs) {
   auto vars = ocs::tom::ListPtr<ocs::tom::VA>::create("vars");
   for (const auto &[name, value] : pairs) {
      auto var = ocs::tom::ElementPtr<ocs::tom::VA>::create();
      var->set<VA_variable>(name);
      var->set<VA_value>(value);
      vars->push_back(std::move(var));
   }
   return vars;
}

/**
 * @brief The values of a variable list, in order, joined by ','
 * @param vars the list
 * @return the values
 */
static std::string
values_of(const ocs::tom::ConstList<ocs::tom::VA> vars) {
   std::string out;
   for (const auto var : vars) {
      out += out.empty() ? "" : ",";
      const char *value = var.get<VA_value>();
      out += value != nullptr ? value : "-";
   }
   return out;
}

/**
 * @brief FREE-10: keep_last<Key> and merge_by_key<Key, Value> do what the definition list helpers did
 * @return nothing; CHECK records the results
 */
static void
test_keyed_merge() {
   // the lists hold a repeated key on purpose, as input for the merge; a debug build refuses that already at
   // the append (N-H-18), so the check is suspended here
   const ocs::tom::UniqueCheckSuspension suspension;

   // the later of two equal keys wins, the order of the rest stays; a nullptr name is never a repetition
   auto vars = make_vars({{"A", "1"}, {"B", "2"}, {"A", "3"}, {nullptr, "4"}, {nullptr, "5"}, {"B", "6"}});
   ocs::tom::keep_last<VA_variable>(*vars);
   CHECK(19, "keep_last keeps the last element of each key in place and every element without a name",
         values_of(vars) == "3,4,5,6");

   // a known key takes the new value, a new key is appended - also one appended by the same merge;
   // two nullptr names are the same key
   auto dst = make_vars({{"A", "1"}, {"B", "2"}});
   const auto src = make_vars({{"B", "20"}, {"C", "30"}, {"C", "31"}, {nullptr, "40"}, {nullptr, "41"}});
   ocs::tom::merge_by_key<VA_variable, VA_value>(*dst, src);
   const bool src_kept = values_of(src) == "20,30,31,40,41";
   CHECK(20, "merge_by_key sets the value of a known key and appends a copy of a new one, the source stays",
         values_of(dst) == "1,20,31,41" && src_kept);
}

/**
 * @brief Build a path list from host/path pairs, as `-o [host:]path,...` gives it
 * @param pairs the pairs, a nullptr host for a path without a host
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::PN>
make_paths(std::initializer_list<std::pair<const char *, const char *>> pairs) {
   auto paths = ocs::tom::ListPtr<ocs::tom::PN>::create("paths");
   for (const auto &[host, path] : pairs) {
      auto pn = ocs::tom::ElementPtr<ocs::tom::PN>::create();
      pn->set<PN_path>(path);
      pn->set<PN_host>(ocs::tom::Host{host});
      paths->push_back(std::move(pn));
   }
   return paths;
}

/**
 * @brief The paths of a path list, in order, joined by ','
 * @param paths the list
 * @return the paths
 */
static std::string
paths_of(const ocs::tom::ConstList<ocs::tom::PN> paths) {
   std::string out;
   for (const auto pn : paths) {
      out += out.empty() ? "" : ",";
      const char *path = pn.get<PN_path>();
      out += path != nullptr ? path : "-";
   }
   return out;
}

/**
 * @brief Build a mail recipient list from user/mail domain pairs, as `-M user[@domain],...` gives it
 * @param pairs the pairs, a nullptr domain for a recipient without one
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::MR>
make_mails(std::initializer_list<std::pair<const char *, const char *>> pairs) {
   auto mails = ocs::tom::ListPtr<ocs::tom::MR>::create("mails");
   for (const auto &[user, domain] : pairs) {
      auto mail = ocs::tom::ElementPtr<ocs::tom::MR>::create();
      mail->set<MR_user>(user);
      mail->set<MR_host>(domain);
      mails->push_back(std::move(mail));
   }
   return mails;
}

/**
 * @brief The recipients of a mail recipient list, in order, as user[@domain] joined by ','
 * @param mails the list
 * @return the recipients
 */
static std::string
mails_of(const ocs::tom::ConstList<ocs::tom::MR> mails) {
   std::string out;
   for (const auto mail : mails) {
      out += out.empty() ? "" : ",";
      const char *user = mail.get<MR_user>();
      out += user != nullptr ? user : "-";
      if (const char *domain = mail.get<MR_host>(); domain != nullptr) {
         out += "@";
         out += domain;
      }
   }
   return out;
}

/**
 * @brief FREE-11: host keys compare as host names in keep_last and merge_by_key, string keys exactly
 *
 * find_first() and modify_sublist() compare host names with sge_hostcmp() (case-insensitive); keep_last and
 * merge_by_key do the same. The domain of a mail recipient (MR_host) is a string field, not a host field: it
 * compares exactly, so two recipients in different mail domains stay apart.
 *
 * @return nothing; CHECK records the results
 */
static void
test_host_keys() {
   // "HostA" and "hosta" name one host: the later wins; two nullptr hosts are never a repetition
   auto paths = make_paths({{"HostA", "/a"}, {"hosta", "/b"}, {"HostB", "/c"}, {nullptr, "/d"}, {nullptr, "/e"}});
   ocs::tom::keep_last<PN_host>(*paths);
   CHECK(21, "keep_last on a host key keeps one element per host name, compared as find_first compares it",
         paths_of(paths) == "/b,/c,/d,/e");

   // a host known under another case takes the new value; two nullptr hosts are the same key
   auto dst = make_paths({{"HostA", "/a"}, {nullptr, "/n"}});
   const auto src = make_paths({{"HOSTA", "/b"}, {"hostc", "/c"}, {nullptr, "/m"}});
   ocs::tom::merge_by_key<PN_host, PN_path>(*dst, src);
   CHECK(22, "merge_by_key on a host key finds a host name in another case, appends a new one",
         paths_of(dst) == "/b,/m,/c" &&
         dst->find_first<PN_host>(ocs::tom::Host{"hosta"}).raw() == dst->front().raw());

   // mail domains are no cluster host names: neither the part after the first dot nor the case makes two the same
   auto mails = make_mails({{"alice", "mail.example.com"}, {"carol", "Example.COM"}});
   const auto more = make_mails({{"bob", "mail.example.org"}, {"dave", "example.com"}, {"erin", "Example.COM"}});
   ocs::tom::merge_by_key<MR_host, MR_user>(*mails, more);
   CHECK(23, "merge_by_key on the mail domain compares it exactly, a domain differing in a later label "
             "or in case is another one",
         mails_of(mails) == "alice@mail.example.com,erin@Example.COM,bob@mail.example.org,dave@example.com");
}

/**
 * @brief FREE-12: copy_if_present into an empty destination, Element::set of an element bound elsewhere, a bound
 *        element through a setter whose first check fails
 * @return nothing; CHECK records the results
 */
static void
test_completed_contracts() {
   auto src = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   src->set<JB_job_number>(4711);
   const bool empty_dst = !ocs::tom::copy_if_present<JB_job_number>(ocs::tom::Element<ocs::tom::JB>(), *src);
   CHECK(24, "copy_if_present into an empty destination returns false, though the source holds the field",
         empty_dst && src->get<JB_job_number>() == 4711);

   // the member setter refuses an element bound to another object with BoundElement; the element stays there
   auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   auto other_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   lListElem *const pe_raw = pe.get().raw();
   other_task->set<JAT_pe_object>(std::move(pe));
   auto rebound = ocs::tom::ElementPtr<ocs::tom::PE>::from_typed(pe_raw);
   bool bound_refused = false;
   try {
      task->set<JAT_pe_object>(std::move(rebound));
   } catch (const ocs::tom::FieldException &e) {
      bound_refused = e.reason() == ocs::tom::FieldException::Reason::BoundElement;
   }
   CHECK(25, "Element::set of an element bound elsewhere throws BoundElement and leaves it where it is bound",
         bound_refused && rebound == nullptr && other_task->get<JAT_pe_object>().raw() == pe_raw &&
            task->get<JAT_pe_object>().raw() == nullptr);

   // a check that fails before the store - an empty handle, a null element - must not free a bound element
   // either: the setters give it up before their first check
   auto rebound_empty = ocs::tom::ElementPtr<ocs::tom::PE>::from_typed(pe_raw);
   bool empty_refused = false;
   try {
      ocs::tom::Element<ocs::tom::JAT>().set<JAT_pe_object>(std::move(rebound_empty));
   } catch (const ocs::tom::FieldException &e) {
      empty_refused = e.reason() == ocs::tom::FieldException::Reason::EmptyHandle;
   }
   CHECK(26, "Element::set on an empty handle throws EmptyHandle and leaves a bound element where it is bound",
         empty_refused && rebound_empty == nullptr && other_task->get<JAT_pe_object>().raw() == pe_raw);

   auto rebound_null = ocs::tom::ElementPtr<ocs::tom::PE>::from_typed(pe_raw);
   const int null_rc = ocs::tom::set<JAT_pe_object>(static_cast<lListElem *>(nullptr), std::move(rebound_null));
   CHECK(27, "the free set on a null element returns -1 and leaves a bound element where it is bound",
         null_rc == -1 && rebound_null == nullptr && other_task->get<JAT_pe_object>().raw() == pe_raw);
}

/**
 * @brief FREE-13: take<F>() moves the element out of an object field
 * @return nothing; CHECK records the results
 */
static void
test_take_object() {
   auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   pe->set<PE_name>("make");
   lListElem *const pe_raw = pe.get().raw();
   task->set<JAT_pe_object>(std::move(pe));

   auto taken = task->take<JAT_pe_object>();
   CHECK(28, "take gives the element the object field held, not a copy, and leaves the field empty",
         taken.get().raw() == pe_raw && task->get<JAT_pe_object>().raw() == nullptr &&
            std::strcmp(taken->get<PE_name>(), "make") == 0);

   // the element taken is free: another object takes it over; an empty field gives an empty handle
   auto other_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   other_task->set<JAT_pe_object>(std::move(taken));
   const auto nothing = task->take<JAT_pe_object>();
   CHECK(29, "the element taken can be stored in another object; take on an empty field gives an empty handle",
         other_task->get<JAT_pe_object>().raw() == pe_raw && taken == nullptr && nothing == nullptr);

   // a reduced element without the field and an empty handle throw, as get and set do
   auto reduced = ocs::tom::ElementPtr<ocs::tom::JAT>::create(ocs::tom::layout<JAT_task_number>());
   bool absent = false;
   try {
      static_cast<void>(reduced->take<JAT_pe_object>());
   } catch (const ocs::tom::FieldException &e) {
      absent = e.reason() == ocs::tom::FieldException::Reason::AbsentField;
   }
   bool empty = false;
   try {
      static_cast<void>(ocs::tom::Element<ocs::tom::JAT>().take<JAT_pe_object>());
   } catch (const ocs::tom::FieldException &e) {
      empty = e.reason() == ocs::tom::FieldException::Reason::EmptyHandle;
   }
   CHECK(30, "take on a reduced element without the field throws AbsentField, on an empty handle EmptyHandle",
         absent && empty);
}

/**
 * @brief A job whose environment holds the given variables
 * @param pairs the variables as name/value pairs, a nullptr name for an entry without a name
 * @param with_list false for a job without an environment list
 * @return the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
make_env_job(std::initializer_list<std::pair<const char *, const char *>> pairs, const bool with_list = true) {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   if (with_list) {
      job->set<JB_env_list>(make_vars(pairs));
   }
   return job;
}

/**
 * @brief The values of a job's environment, joined by ',', or "null" without an environment list
 * @param job the job
 * @return the values
 */
static std::string
env_of(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   const auto env = job.get<JB_env_list>();
   return env ? values_of(env) : "null";
}

/** @brief What one modify_sublist() call gave and reported */
struct SublistRun {
   ocs::tom::SublistOutcome outcome;   ///< the result
   std::string events;                 ///< the events as "<event>:<key>", separated by blanks
};

/**
 * @brief Apply the environment of a request to a job with modify_sublist(), recording the events
 *
 * Two variables differ when their values differ.
 *
 * @param dst the job to modify
 * @param request the request; its environment is consumed
 * @param mode how to apply it
 * @param member true to call the member Element::modify_sublist() instead of the free form
 * @return the outcome and the events
 */
static SublistRun
run_modify_sublist(const ocs::tom::Element<ocs::tom::JB> dst, const ocs::tom::Element<ocs::tom::JB> request,
                   const ocs::tom::SublistMode mode, const bool member = false) {
   std::string events;
   const auto differs = [](const ocs::tom::ConstElement<ocs::tom::VA> a,
                           const ocs::tom::ConstElement<ocs::tom::VA> b) {
      const char *x = a.get<VA_value>();
      const char *y = b.get<VA_value>();
      return x == nullptr || y == nullptr ? x != y : std::strcmp(x, y) != 0;
   };
   const auto report = [&events](const ocs::tom::SublistEvent event, const char *key) {
      static const char *const names[] = {"missing", "held", "notheld", "created", "appended"};
      events += events.empty() ? "" : " ";
      events += std::string(names[static_cast<int>(event)]) + ":" + (key != nullptr ? key : "-");
   };
   const auto outcome =
      member ? dst.modify_sublist<JB_env_list, VA_variable>(request, mode, differs, report)
             : ocs::tom::modify_sublist<JB_env_list, VA_variable>(dst, request, mode, differs, report);
   return {outcome, events};
}

/**
 * @brief FREE-14: modify_sublist<ListF, SubKeyF> does what attr_mod_sub_list() did
 * @return nothing; CHECK records the results
 */
static void
test_modify_sublist() {
   constexpr ocs::tom::SublistMode set_mode{};
   constexpr ocs::tom::SublistMode change_mode{.change = true};
   constexpr ocs::tom::SublistMode append_mode{.append = true};
   constexpr ocs::tom::SublistMode remove_mode{.remove = true};

   // set: the request's list replaces the object's as a copy; a request without a list removes it
   {
      auto dst = make_env_job({{"A", "1"}, {"B", "2"}});
      auto request = make_env_job({{"C", "3"}});
      const auto run = run_modify_sublist(dst.get(), request.get(), set_mode);
      auto dst2 = make_env_job({{"A", "1"}});
      auto empty_request = make_env_job({}, false);
      const auto run2 = run_modify_sublist(dst2.get(), empty_request.get(), set_mode);
      CHECK(31, "set replaces the sub-list by a copy, also by none, and reports nothing",
            run.outcome.ok && run.outcome.changed && run.events.empty() && env_of(dst.get()) == "3" &&
            env_of(request.get()) == "3" && run2.outcome.ok && run2.outcome.changed && env_of(dst2.get()) == "null");
   }

   // change: a differing held key is replaced and goes to the end, an equal one is dropped, a new one appended
   {
      auto dst = make_env_job({{"A", "1"}, {"B", "2"}});
      auto request = make_env_job({{"B", "20"}, {"C", "30"}, {"A", "1"}});
      const auto run = run_modify_sublist(dst.get(), request.get(), change_mode);
      auto same = make_env_job({{"A", "1"}});
      auto same_request = make_env_job({{"A", "1"}});
      const auto run2 = run_modify_sublist(same.get(), same_request.get(), change_mode);
      CHECK(32, "change replaces a differing key, drops an equal one and appends a key not found, consuming the "
            "request",
            run.outcome.ok && run.outcome.changed && run.events == "appended:C" && env_of(dst.get()) == "1,20,30" &&
            env_of(request.get()).empty() && run2.outcome.ok && !run2.outcome.changed && run2.events.empty() &&
            env_of(same.get()) == "1");
   }

   // append: a new key is appended; a held key fails the operation and nothing after it is applied
   {
      auto dst = make_env_job({{"A", "1"}});
      auto request = make_env_job({{"B", "2"}, {"A", "9"}, {"C", "3"}});
      const auto run = run_modify_sublist(dst.get(), request.get(), append_mode);
      auto dst2 = make_env_job({{"A", "1"}});
      auto request2 = make_env_job({{"B", "2"}});
      const auto run2 = run_modify_sublist(dst2.get(), request2.get(), append_mode);
      CHECK(33, "append of a held key fails and applies nothing, append of a key not found adds it",
            !run.outcome.ok && !run.outcome.changed && run.events == "held:A" && env_of(dst.get()) == "1" &&
            env_of(request.get()) == "2,3" && run2.outcome.ok && run2.outcome.changed &&
            run2.events == "appended:B" && env_of(dst2.get()) == "1,2");
   }

   // remove: every element with the key goes, a key not found is reported, an emptied list is removed
   {
      // the object holds a key twice on purpose; a debug build refuses that at the append (N-H-18)
      const ocs::tom::UniqueCheckSuspension suspension;
      auto dst = make_env_job({{"A", "1"}, {"B", "2"}, {"A", "3"}});
      auto request = make_env_job({{"A", nullptr}, {"X", nullptr}});
      const auto run = run_modify_sublist(dst.get(), request.get(), remove_mode);
      auto dst2 = make_env_job({{"A", "1"}});
      auto request2 = make_env_job({{"A", nullptr}});
      const auto run2 = run_modify_sublist(dst2.get(), request2.get(), remove_mode);
      CHECK(34, "remove removes every element with a key, reports a key not found and removes an emptied list",
            run.outcome.ok && run.outcome.changed && run.events == "notheld:X" && env_of(dst.get()) == "2" &&
            run2.outcome.ok && run2.outcome.changed && run2.events.empty() && env_of(dst2.get()) == "null");
   }

   // a missing sub-list: change creates it as a copy of the request's, remove finds nothing
   {
      auto dst = make_env_job({}, false);
      auto request = make_env_job({{"A", "1"}, {"B", "2"}});
      const auto run = run_modify_sublist(dst.get(), request.get(), change_mode);
      auto dst2 = make_env_job({}, false);
      auto request2 = make_env_job({{"A", nullptr}});
      const auto run2 = run_modify_sublist(dst2.get(), request2.get(), remove_mode);
      CHECK(35, "a missing sub-list is created as a copy of the request's, a removal from it reports the key",
            run.outcome.ok && run.outcome.changed && run.events == "created:A" && env_of(dst.get()) == "1,2" &&
            run2.outcome.ok && !run2.outcome.changed && run2.events == "notheld:A" &&
            env_of(dst2.get()) == "null");
   }

   // a key missing in the request, and a request reduced without the list field
   {
      auto dst = make_env_job({{"A", "1"}, {"B", "2"}});
      auto request = make_env_job({{nullptr, "5"}});
      const auto run = run_modify_sublist(dst.get(), request.get(), change_mode);

      auto rjob = make_reduced_job();
      auto dst2 = make_env_job({{"A", "1"}});
      const auto run2 = run_modify_sublist(dst2.get(), rjob.get(), change_mode);
      CHECK(36, "a missing key is reported for every element compared and fails; a request without the list "
            "field fails silently",
            !run.outcome.ok && !run.outcome.changed && run.events == "missing:- missing:-" &&
            env_of(dst.get()) == "1,2" && !run2.outcome.ok && !run2.outcome.changed && run2.events.empty() &&
            env_of(dst2.get()) == "1");
   }
}

/**
 * @brief FREE-15: the members of an element handle that forward to a free form give what the free form gives
 *
 * Each member runs on one of two equal objects, the free form on the other; the results and the objects
 * afterwards are compared. The owning handle reaches the members through operator->.
 *
 * @return nothing; CHECK records the results
 */
static void
test_member_element_forms() {
   // lookups and adding and erasing by key in a sub-list
   {
      auto a = make_job();
      auto b = make_job();
      const ocs::tom::ConstElement<ocs::tom::JB> ca = a.get();
      const ocs::tom::ConstElement<ocs::tom::JB> cb = b.get();
      const ocs::tom::ConstElement<ocs::tom::JB> none;
      bool same = ca.find_first_in<JB_env_list, VA_variable>("A") ==
                     ocs::tom::find_first_in<JB_env_list, VA_variable>(ca, "A") &&
                  ca.find_first_in<JB_env_list, VA_variable>("A") != nullptr &&
                  ca.find_first_in<JB_env_list, VA_variable>("x") == nullptr &&
                  none.find_first_in<JB_env_list, VA_variable>("A") == nullptr &&
                  a->find_first_in<JB_env_list, VA_variable>("b") ==
                     ocs::tom::find_first_in<JB_env_list, VA_variable>(a, "b") &&
                  ca.find_first_in_rw<JB_ja_tasks, JAT_task_number>(2).raw() ==
                     ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(ca, 2).raw() &&
                  ca.find_first_in_rw<JB_ja_tasks, JAT_task_number>(2) != nullptr;

      const ocs::tom::Element<ocs::tom::JB> ea = a.get();
      const ocs::tom::Element<ocs::tom::JB> eb = b.get();
      same = same && (ea.emplace_back_in<JB_env_list, VA_variable>("C") != nullptr) ==
                        (ocs::tom::emplace_back_in<JB_env_list, VA_variable>(eb, "C") != nullptr);
      same = same && a->emplace_back_in<JB_ja_tasks, JAT_task_number>(3) != nullptr &&
             ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(eb, 3) != nullptr;
      same = same && ea.erase_in<JB_env_list, VA_variable>("A") ==
                        ocs::tom::erase_in<JB_env_list, VA_variable>(eb, "A");
      same = same && ea.erase_in<JB_env_list, VA_variable>("x") ==
                        ocs::tom::erase_in<JB_env_list, VA_variable>(eb, "x");
      same = same && a->erase_in<JB_ja_tasks, JAT_task_number>(1) ==
                        ocs::tom::erase_in<JB_ja_tasks, JAT_task_number>(eb, 1);
      same = same && ocs::tom::Element<ocs::tom::JB>().erase_in<JB_env_list, VA_variable>("b") == 0;
      const std::string env_a = keys(a->get<JB_env_list>().raw(), VA_variable);
      const std::string tasks_a = numbers(a->get<JB_ja_tasks>().raw(), JAT_task_number);
      same = same && env_a == "b C " && tasks_a == "2 3 " &&
             env_a == keys(cb.get<JB_env_list>().raw(), VA_variable) &&
             tasks_a == numbers(cb.get<JB_ja_tasks>().raw(), JAT_task_number);
      CHECK(37, "find_first_in, find_first_in_rw, emplace_back_in and erase_in as members give what the free "
                "forms give, on the handle, the owning handle and an empty handle",
            same);
   }

   // the list reference, the string never null, the bit masks and the atomic accessors
   {
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      job->get_or_create<JB_env_list>("env");
      const ocs::tom::ConstElement<ocs::tom::JB> cjob = job.get();
      bool same = cjob.list_ref<JB_env_list>() == ocs::tom::list_ref<JB_env_list>(cjob) &&
                  *job->list_ref<JB_env_list>() == job->get<JB_env_list>().raw() &&
                  std::string(cjob.get_or_empty<JB_owner>()) == ocs::tom::get_or_empty<JB_owner>(cjob) &&
                  std::string(cjob.get_or_empty<JB_owner>()).empty();
      job->set<JB_owner>("me");
      same = same && std::string(job->get_or_empty<JB_owner>()) == "me";

      auto ta = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
      auto tb = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
      ta->set<JAT_status>(0x5);
      tb->set<JAT_status>(0x5);
      const ocs::tom::Element<ocs::tom::JAT> eb = tb.get();
      same = same && ta->has_any_bits<JAT_status>(0x4) == ocs::tom::has_any_bits<JAT_status>(eb, 0x4) &&
             ta->has_any_bits<JAT_status>(0x2) == ocs::tom::has_any_bits<JAT_status>(eb, 0x2);
      same = same && ta->or_bits<JAT_status>(0x2) == ocs::tom::or_bits<JAT_status>(eb, 0x2) &&
             ta->get<JAT_status>() == eb.get<JAT_status>();
      same = same && ta->and_bits<JAT_status>(0x6) == ocs::tom::and_bits<JAT_status>(eb, 0x6) &&
             ta->get<JAT_status>() == eb.get<JAT_status>();
      same = same && ta->clear_bits<JAT_status>(0x2) == ocs::tom::clear_bits<JAT_status>(eb, 0x2) &&
             ta->get<JAT_status>() == eb.get<JAT_status>() && eb.get<JAT_status>() == 0x4;
      // 5 | 2 = 7, 7 & 6 = 6, 6 & ~2 = 4; an empty handle gives -1 with LEELEMNULL in both forms
      ocs::tom::detail::state_set_lerrno(0);
      const int member_null = ocs::tom::Element<ocs::tom::JAT>().clear_bits<JAT_status>(0x2);
      const int member_errno = ocs::tom::detail::state_get_lerrno();
      ocs::tom::detail::state_set_lerrno(0);
      const int free_null = ocs::tom::clear_bits<JAT_status>(ocs::tom::Element<ocs::tom::JAT>(), 0x2);
      same = same && member_null == -1 && member_null == free_null && member_errno == LEELEMNULL &&
             ocs::tom::detail::state_get_lerrno() == LEELEMNULL;

      auto ha = ocs::tom::ElementPtr<ocs::tom::HM>::create();
      auto hb = ocs::tom::ElementPtr<ocs::tom::HM>::create();
      const ocs::tom::Element<ocs::tom::HM> hbe = hb.get();
      same = same && ha->set_atomic<HM_last_used>(4711) == ocs::tom::set_atomic<HM_last_used>(hbe, 4711) &&
             ha->get_atomic<HM_last_used>() == ocs::tom::get_atomic<HM_last_used>(hbe) &&
             ocs::tom::ConstElement<ocs::tom::HM>(ha.get()).get_atomic<HM_last_used>() == 4711;
      CHECK(38, "list_ref, get_or_empty, has_any_bits, or_bits, and_bits, clear_bits, get_atomic and set_atomic as "
                "members give what the free forms give, also for an empty handle",
            same);
   }

   // copying a field and modifying a sub-list: this element is the destination
   {
      auto src = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      auto da = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      auto db = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      src->set<JB_job_number>(4711);
      src->set<JB_env_list>(make_names({"X"}));
      da->set<JB_job_name>("old");
      db->set<JB_job_name>("old");
      const ocs::tom::Element<ocs::tom::JB> dbe = db.get();
      const ocs::tom::ConstElement<ocs::tom::JB> none;
      const ocs::tom::ConstElement<ocs::tom::JB> from = src.get();
      bool same = da->copy_if_present<JB_job_number>(from) == ocs::tom::copy_if_present<JB_job_number>(dbe, from) &&
                  da->copy_if_present<JB_job_name>(from) == ocs::tom::copy_if_present<JB_job_name>(dbe, from) &&
                  da->copy_if_present<JB_env_list>(from) == ocs::tom::copy_if_present<JB_env_list>(dbe, from) &&
                  da->copy_if_present<JB_owner>(none) == ocs::tom::copy_if_present<JB_owner>(dbe, none);
      // the number copied, the name copied as nullptr, the list deeply; nothing from the empty source
      same = same && da->get<JB_job_number>() == 4711 && db->get<JB_job_number>() == 4711 &&
             da->get<JB_job_name>() == nullptr && db->get<JB_job_name>() == nullptr &&
             keys(da->get<JB_env_list>().raw(), VA_variable) == "X " &&
             keys(db->get<JB_env_list>().raw(), VA_variable) == "X " &&
             da->get<JB_env_list>().raw() != src->get<JB_env_list>().raw();

      constexpr ocs::tom::SublistMode change_mode{.change = true};
      auto oa = make_env_job({{"A", "1"}, {"B", "2"}});
      auto ob = make_env_job({{"A", "1"}, {"B", "2"}});
      auto ra = make_env_job({{"B", "20"}, {"C", "30"}, {"A", "1"}});
      auto rb = make_env_job({{"B", "20"}, {"C", "30"}, {"A", "1"}});
      const auto member_run = run_modify_sublist(oa.get(), ra.get(), change_mode, true);
      const auto free_run = run_modify_sublist(ob.get(), rb.get(), change_mode);
      same = same && member_run.outcome.ok == free_run.outcome.ok &&
             member_run.outcome.changed == free_run.outcome.changed && member_run.events == free_run.events &&
             env_of(oa.get()) == env_of(ob.get()) && env_of(ra.get()) == env_of(rb.get()) &&
             env_of(oa.get()) == "1,20,30";
      CHECK(39, "copy_if_present and modify_sublist as members, this element the destination, give what the free "
                "forms give",
            same);
   }
}

/**
 * @brief FREE-16: the members of a list handle that forward to a free form give what the free form gives
 * @return nothing; CHECK records the results
 */
static void
test_member_list_forms() {
   // repeated keys on purpose, as in test_keyed_merge()
   const ocs::tom::UniqueCheckSuspension suspension;

   auto la = make_vars({{"A", "1"}, {"B", "2"}, {"A", "3"}, {nullptr, "4"}, {nullptr, "5"}, {"B", "6"}});
   auto lb = make_vars({{"A", "1"}, {"B", "2"}, {"A", "3"}, {nullptr, "4"}, {nullptr, "5"}, {"B", "6"}});
   la->keep_last<VA_variable>();
   ocs::tom::keep_last<VA_variable>(*lb);
   const bool kept = values_of(la) == values_of(lb) && values_of(la) == "3,4,5,6";

   auto da = make_vars({{"A", "1"}, {"B", "2"}});
   auto db = make_vars({{"A", "1"}, {"B", "2"}});
   const auto src = make_vars({{"B", "20"}, {"C", "30"}, {"C", "31"}, {nullptr, "40"}, {nullptr, "41"}});
   const ocs::tom::List<ocs::tom::VA> dav = da;
   dav.merge_by_key<VA_variable, VA_value>(src);
   ocs::tom::merge_by_key<VA_variable, VA_value>(*db, src);
   auto dc = make_vars({{"A", "1"}});
   dc->merge_by_key<VA_variable, VA_value>(ocs::tom::ConstList<ocs::tom::VA>());
   const bool merged = values_of(da) == values_of(db) && values_of(da) == "1,20,31,41" &&
                       values_of(src) == "20,30,31,40,41" && values_of(dc) == "1";
   CHECK(40, "keep_last and merge_by_key as members, this list the destination, give what the free forms give, "
             "on the list handle and the owning handle",
         kept && merged);
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"FREE-01", "find all", "find_first<F> with an iterator and find_next<F> walk every element with a key",
       {"walk every element with a repeated key, for every key type, below and above the hash threshold"},
       "the elements holding the key, in list order", {{"OPS-02", "covers"}}, {1}, test_find_all},
      {"FREE-02", "nocase and like", "find_first_nocase<F> and find_first_like<F> find by case and by a prefix",
       {"look up a string without regard to case and by a prefix pattern; a null list and a null pattern"},
       "the first element matching, or nullptr", {{"OPS-02", "covers"}}, {2}, test_find_string},
      {"FREE-03", "find in", "find_first_in<LF, F> looks up in a sub-list",
       {"look up in the environment and the array tasks of a job, a null element, a job without the lists"},
       "the element or nullptr; the process ends without the list field", {{"OPS-02", "covers"}}, {3, 4},
       test_find_in},
      {"FREE-04", "add and erase", "emplace_back<F> and erase<F> add and delete by key in a list",
       {"add and delete strings and numbers, with null arguments, in a list of another type"},
       "the list created on demand, an emptied list freed; nullptr and 0 for a foreign list",
       {{"OPS-02", "covers"}}, {5, 6}, test_add_erase},
      {"FREE-05", "add and erase in", "emplace_back_in<LF, F> and erase_in<LF, F> work on a sub-list",
       {"add to and delete from the environment and the array tasks of a job, a null element, a job without them"},
       "the sub-lists changed; the process ends without the list field", {{"OPS-02", "covers"}}, {7, 8},
       test_add_erase_in},
      {"FREE-06", "has", "has<F> tells whether an element holds a field",
       {"a full job, a reduced job, an array task and a null element"}, "true only where the field is held",
       {{"OPS-02", "covers"}}, {9}, test_has},
      {"FREE-07", "exchange and swap", "exchange<F> and swap<F, G> move lists and strings",
       {"exchange a list and a string, swap two lists, with null arguments"},
       "the lists and strings moved; -1 with LEELEMNULL and nothing changed for a null argument",
       {{"OPS-02", "covers"}}, {10}, test_exchange},
      {"FREE-08", "small ones", "get_or_create, list_ref, get_or_empty and the bit masks",
       {"create a list on demand, take its reference, read an unset string, set and clear bits"},
       "the list created once, the slot, an empty string, the bit results", {{"OPS-02", "covers"}}, {11},
       test_small},
      {"FREE-09", "owning", "an owning element handle takes the free forms and moves into the calls that take its "
       "element over",
       {"field access on an owning job and task, append and insert_after moved in, set of an object and a list "
        "field moved in, a refused store, nullptr comparison, List/ConstList views of an owning list, "
        "copy_if_present of a value, a nullptr string, a list and from an empty source"},
       "the same results as on the handle; a moved handle is empty, a refused one keeps its element",
       {{"OPS-02", "covers"}}, {12, 13, 14, 15, 16, 17, 18}, test_owning},
      {"FREE-10", "keyed merge", "keep_last<Key> and merge_by_key<Key, Value> compress and merge definition lists",
       {"a variable list with repeated and missing names, merged and compressed"},
       "the last element of a key wins in place, a known key takes the new value, a new one is appended",
       {{"OPS-02", "covers"}}, {19, 20}, test_keyed_merge},
      {"FREE-11", "host keys", "keep_last<Key> and merge_by_key<Key, Value> compare host keys as host names, "
       "the mail domain of a recipient exactly",
       {"a path list with one host in two cases and paths without a host, compressed and merged",
        "a mail recipient list merged with recipients in other domains and one domain in another case"},
       "one element per host name; a host in another case takes the new value; every mail domain spelled "
       "differently stays a recipient of its own", {{"OPS-02", "covers"}}, {21, 22, 23}, test_host_keys},
      {"FREE-12", "contracts", "copy_if_present<F> into an empty destination, Element::set of a bound element",
       {"copy into an empty handle", "store an element bound to another object through the member setter",
        "store a bound element through an empty handle and into a null element"},
       "false from copy_if_present; BoundElement, EmptyHandle or -1, the element stays bound",
       {{"OPS-02", "covers"}}, {24, 25, 26, 27}, test_completed_contracts},
      {"FREE-13", "take object", "take<F>() moves the element out of an object field",
       {"take a sub-object and store it in another object", "take from an empty field, a reduced element and "
        "an empty handle"},
       "the same element, the field empty; an empty handle for an empty field; AbsentField and EmptyHandle",
       {{"OPS-02", "covers"}}, {28, 29, 30}, test_take_object},
      {"FREE-14", "sub-list modify", "modify_sublist<ListF, SubKeyF> sets, changes, appends and removes by key",
       {"set, change, append and remove a job's environment by name, with a missing sub-list, a key not found, "
        "a key held twice, a missing key and a request without the list field"},
       "the expected lists, results and events in order; the request's sub-list consumed",
       {{"OPS-02", "covers"}}, {31, 32, 33, 34, 35, 36}, test_modify_sublist},
      {"FREE-15", "element members", "the members of an element handle forward to the free forms",
       {"find_first_in, find_first_in_rw, emplace_back_in, erase_in, list_ref, get_or_empty, the bit masks, the "
        "atomic accessors, copy_if_present and modify_sublist as members and as free forms on equal objects, on "
        "the handle, the owning handle and an empty handle"},
       "the same results and the same objects afterwards", {{"OPS-02", "covers"}}, {37, 38, 39},
       test_member_element_forms},
      {"FREE-16", "list members", "the members of a list handle forward to the free forms",
       {"keep_last and merge_by_key as members and as free forms on equal lists, also with an empty source"},
       "the same lists afterwards", {{"OPS-02", "covers"}}, {40}, test_member_list_forms},
   });
}
