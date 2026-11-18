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
 * @brief The free forms for the mechanical rewrite against the old functions they replace (CS-3045, OPS-02)
 *
 * The mechanical rewrite replaces a call of the old interface with a constant
 * field by a free template on the same raw pointers: lGetElemStr(lp, CE_name,
 * name) becomes ocs::tom::find_first<CE_name>(lp, name), lAddSubStr(ep,
 * VA_variable, name, JB_env_list, VA_Type) becomes
 * ocs::tom::emplace_back_in<JB_env_list, VA_variable>(ep, name). The free form
 * checks the field, the key and the sub-type at compile time and keeps the
 * run-time behaviour of the old function, so that the rewrite changes nothing.
 *
 * This test is also an example for the rewrite: every scenario runs the old
 * call and its free form on the same input and compares what came out,
 * including the error cases. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     free:    the free form that replaces it
 *     differs: what the free form does differently, if anything
 *
 * One case differs on purpose: lSwapList() with a null first element lost
 * the list of the second, the free form changes neither; see the scenario.
 *
 * Where the old function aborts, the free form throws a FieldException that
 * nothing catches, so both end the process; such cases run in a child process.
 * In a Release build the last scenario times both forms.
 */

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

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
    * @param n the number of elements
    * @return the list; the caller frees it
    */
   lList *make_test_list(const int n) {
      lList *lp = lCreateList("test", TEST_Type);
      for (int i = 0; i < n; i++) {
         lListElem *ep = lCreateElem(TEST_Type);
         lSetUlong(ep, TEST_uint32, i);
         lSetUlong64(ep, TEST_uint64, i % 3);
         const std::string s = "s" + std::to_string(i % 4);
         lSetString(ep, TEST_string, s.c_str());
         const std::string h = "h" + std::to_string(i % 2);
         lSetHost(ep, TEST_host, h.c_str());
         lAppendElem(lp, ep);
      }
      return lp;
   }

   /**
    * @brief A job with an environment and array tasks, for the forms on the list of a list field
    * @return the job; the caller frees it
    */
   lListElem *make_job() {
      lListElem *job = lCreateElem(JB_Type);
      lAddSubStr(job, VA_variable, "A", JB_env_list, VA_Type);
      lAddSubStr(job, VA_variable, "b", JB_env_list, VA_Type);
      lAddSubUlong(job, JAT_task_number, 1, JB_ja_tasks, JAT_Type);
      lAddSubUlong(job, JAT_task_number, 2, JB_ja_tasks, JAT_Type);
      return job;
   }

   /**
    * @brief A job reduced to its number, which holds none of the list fields
    * @param jobs a list taking the full job, freed by the caller
    * @param reduced the reduced list, freed by the caller
    * @return the reduced job
    */
   lListElem *make_reduced_job(lList *&jobs, lList *&reduced) {
      jobs = lCreateList("jobs", JB_Type);
      lAppendElem(jobs, make_job());
      lEnumeration *what = lWhat("%T(%I)", JB_Type, JB_job_number);
      reduced = lSelect("reduced", jobs, nullptr, what);
      lFreeWhat(&what);
      return lFirstRW(reduced);
   }

   /**
    * @brief The keys of a list of a string field, in order, as one string
    * @param lp the list, or nullptr
    * @param nm the string field
    * @return the keys separated by blanks, "null" for a null list
    */
   std::string keys(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return "null";
      }
      std::string result;
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         result += lGetString(ep, nm);
         result += ' ';
      }
      return result;
   }

   /**
    * @brief The numbers of a list of a 32 bit field, in order, as one string
    * @param lp the list, or nullptr
    * @param nm the 32 bit field
    * @return the numbers separated by blanks, "null" for a null list
    */
   std::string numbers(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return "null";
      }
      std::string result;
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         result += std::to_string(lGetUlong(ep, nm));
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
    * @brief Print what both forms gave and what was expected, when they differ
    * @param what the name of the scenario
    * @param old_seen what the old functions gave
    * @param free_seen what the free forms gave
    * @param expected what both should give
    */
   template <typename V>
   void show_if_differs(const char *what, const std::vector<V> &old_seen, const std::vector<V> &free_seen,
                        const std::vector<V> &expected) {
      if (old_seen == expected && free_seen == expected) {
         return;
      }
      const auto print = [](const char *label, const std::vector<V> &values) {
         printf("   %-8s", label);
         for (const V &v : values) {
            if constexpr (std::is_same_v<V, std::string>) {
               printf(" [%s]", v.c_str());
            } else {
               printf(" [%d]", static_cast<int>(v));
            }
         }
         printf("\n");
      };
      printf("%s differs:\n", what);
      print("old", old_seen);
      print("free", free_seen);
      print("expected", expected);
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Lookups by a key in a list
 */
void test_find() {
   // old:     lGetElemStr(lp, TEST_string, "s1"), lGetElemHostRW(lp, TEST_host, "h1"), lGetElemUlong(lp, ...)
   // free:    find_first<TEST_string>(lp, "s1"), find_first_rw<TEST_host>(lp, "h1"), find_first<TEST_uint32>(lp, 3)
   // differs: nothing at run time; a key of another type than the field does not compile
   bool same = true;
   for (const int n : Lengths) {
      lList *lp = make_test_list(n);
      same = same && lGetElemStr(lp, TEST_string, "s1") == ocs::tom::find_first<TEST_string>(lp, "s1") &&
             lGetElemStrRW(lp, TEST_string, "s3") == ocs::tom::find_first_rw<TEST_string>(lp, "s3") &&
             lGetElemStr(lp, TEST_string, "none") == ocs::tom::find_first<TEST_string>(lp, "none") &&
             lGetElemHost(lp, TEST_host, "h1") == ocs::tom::find_first<TEST_host>(lp, "h1") &&
             lGetElemHostRW(lp, TEST_host, "H0") == ocs::tom::find_first_rw<TEST_host>(lp, "H0") &&
             lGetElemUlong(lp, TEST_uint32, 3) == ocs::tom::find_first<TEST_uint32>(lp, 3) &&
             lGetElemUlongRW(lp, TEST_uint32, 999) == ocs::tom::find_first_rw<TEST_uint32>(lp, 999) &&
             lGetElemUlong64(lp, TEST_uint64, 2) == ocs::tom::find_first<TEST_uint64>(lp, 2) &&
             lGetElemUlong64RW(lp, TEST_uint64, 1) == ocs::tom::find_first_rw<TEST_uint64>(lp, 1) &&
             ocs::tom::find_first<TEST_string>(lp, "s1") != nullptr;
      lFreeList(&lp);
   }
   CHECK(1, "find_first<F> and find_first_rw<F> find what lGetElemStr/Host/Ulong/Ulong64 find, below and above the "
            "hash threshold",
         same);

   // old:     lGetElemStr(nullptr, ...), lGetElemStr(lp, TEST_string, nullptr)
   // free:    the same arguments
   // differs: nothing; a null list or a null string key finds nothing
   lList *lp = make_test_list(5);
   const bool nulls = ocs::tom::find_first<TEST_string>(nullptr, "s1") == nullptr &&
                      lGetElemStr(nullptr, TEST_string, "s1") == nullptr &&
                      ocs::tom::find_first<TEST_string>(lp, nullptr) == nullptr &&
                      lGetElemStr(lp, TEST_string, nullptr) == nullptr &&
                      ocs::tom::find_first<TEST_host>(lp, nullptr) == nullptr &&
                      ocs::tom::find_first<TEST_uint32>(nullptr, 1) == nullptr;
   lFreeList(&lp);
   CHECK(2, "a null list or a null key finds nothing in both forms", nulls);
}

/**
 * @brief Walking every element with a key
 */
void test_find_all() {
   // old:     for (ep = lGetElemStrFirst(lp, F, key, &it); ep; ep = lGetElemStrNext(lp, F, key, &it))
   // free:    for (ep = find_first<F>(lp, key, &it); ep; ep = find_next<F>(lp, key, &it))
   // differs: nothing
   bool same = true;
   for (const int n : Lengths) {
      lList *lp = make_test_list(n);
      const auto walk_old = [&](auto first, auto next) {
         std::vector<const lListElem *> seen;
         const void *it = nullptr;
         for (const lListElem *ep = first(&it); ep != nullptr; ep = next(&it)) {
            seen.push_back(ep);
         }
         return seen;
      };
      same = same &&
             walk_old([&](const void **it) { return lGetElemStrFirst(lp, TEST_string, "s1", it); },
                      [&](const void **it) { return lGetElemStrNext(lp, TEST_string, "s1", it); }) ==
                walk_old([&](const void **it) { return ocs::tom::find_first<TEST_string>(lp, "s1", it); },
                         [&](const void **it) { return ocs::tom::find_next<TEST_string>(lp, "s1", it); }) &&
             walk_old([&](const void **it) { return lGetElemHostFirstRW(lp, TEST_host, "h0", it); },
                      [&](const void **it) { return lGetElemHostNextRW(lp, TEST_host, "h0", it); }) ==
                walk_old([&](const void **it) { return ocs::tom::find_first_rw<TEST_host>(lp, "h0", it); },
                         [&](const void **it) { return ocs::tom::find_next_rw<TEST_host>(lp, "h0", it); }) &&
             walk_old([&](const void **it) { return lGetElemUlong64First(lp, TEST_uint64, 1, it); },
                      [&](const void **it) { return lGetElemUlong64Next(lp, TEST_uint64, 1, it); }) ==
                walk_old([&](const void **it) { return ocs::tom::find_first<TEST_uint64>(lp, 1, it); },
                         [&](const void **it) { return ocs::tom::find_next<TEST_uint64>(lp, 1, it); }) &&
             walk_old([&](const void **it) { return lGetElemUlongFirstRW(lp, TEST_uint32, 4, it); },
                      [&](const void **it) { return lGetElemUlongNextRW(lp, TEST_uint32, 4, it); }) ==
                walk_old([&](const void **it) { return ocs::tom::find_first_rw<TEST_uint32>(lp, 4, it); },
                         [&](const void **it) { return ocs::tom::find_next_rw<TEST_uint32>(lp, 4, it); });
      lFreeList(&lp);
   }
   CHECK(3, "find_first<F>(lp, key, &it) and find_next<F>() walk the elements lGetElem<Type>First and Next walk",
         same);
}

/**
 * @brief Lookups without regard to case and by a prefix pattern
 */
void test_find_string() {
   // old:     lGetElemCaseStr(lp, TEST_string, "S2"), lGetElemStrLikeRW(lp, TEST_string, "s*")
   // free:    find_first_nocase<TEST_string>(lp, "S2"), find_first_like_rw<TEST_string>(lp, "s*")
   // differs: nothing
   lList *lp = make_test_list(10);
   const bool same = lGetElemCaseStr(lp, TEST_string, "S2") == ocs::tom::find_first_nocase<TEST_string>(lp, "S2") &&
                     lGetElemCaseStrRW(lp, TEST_string, "x") == ocs::tom::find_first_nocase_rw<TEST_string>(lp, "x") &&
                     lGetElemStrLike(lp, TEST_string, "s3") == ocs::tom::find_first_like<TEST_string>(lp, "s3") &&
                     lGetElemStrLikeRW(lp, TEST_string, "s*") == ocs::tom::find_first_like_rw<TEST_string>(lp, "s*") &&
                     ocs::tom::find_first_nocase<TEST_string>(lp, "S2") != nullptr &&
                     ocs::tom::find_first_nocase<TEST_string>(nullptr, "S2") == nullptr &&
                     ocs::tom::find_first_like<TEST_string>(lp, nullptr) == nullptr;
   lFreeList(&lp);
   CHECK(4, "find_first_nocase<F> and find_first_like<F> find what lGetElemCaseStr and lGetElemStrLike find", same);
}

/**
 * @brief A lookup in a list that does not hold the key field
 */
void test_find_missing_field() {
   // old:     lGetElemStr(tasks, TEST_string, "x") on a list of array tasks, or on a list reduced without the field
   // free:    find_first<TEST_string>(tasks, "x")
   // differs: nothing; both write the old message, record the soft path and find nothing. Run in a child each,
   //          to keep the messages out of the output.
   lList *tasks = lCreateList("tasks", JAT_Type);
   lAppendElem(tasks, lCreateElem(JAT_Type));
   lList *lp = make_test_list(5);
   lEnumeration *what = lWhat("%T(%I)", TEST_Type, TEST_uint32);
   lList *reduced = lSelect("reduced", lp, nullptr, what);
   lFreeWhat(&what);

   const auto none = [](const void *found) { return found == nullptr ? 0 : 1; };
   std::vector<int> old_seen;
   std::vector<int> free_seen;
   for (const lList *list : {static_cast<const lList *>(tasks), static_cast<const lList *>(reduced)}) {
      old_seen.push_back(in_child([&] { return none(lGetElemStr(list, TEST_string, "x")); }));
      old_seen.push_back(in_child([&] { return none(lGetElemHost(list, TEST_host, "x")); }));
      old_seen.push_back(in_child([&] { return none(lGetElemUlong64(list, TEST_uint64, 1)); }));
      old_seen.push_back(in_child([&] { return none(lGetElemCaseStr(list, TEST_string, "x")); }));
      free_seen.push_back(in_child([&] { return none(ocs::tom::find_first<TEST_string>(list, "x")); }));
      free_seen.push_back(in_child([&] { return none(ocs::tom::find_first<TEST_host>(list, "x")); }));
      free_seen.push_back(in_child([&] { return none(ocs::tom::find_first<TEST_uint64>(list, 1)); }));
      free_seen.push_back(in_child([&] { return none(ocs::tom::find_first_nocase<TEST_string>(list, "x")); }));
   }
   // the 32 bit field the reduced list holds still finds
   old_seen.push_back(lGetElemUlong(reduced, TEST_uint32, 2) != nullptr ? 1 : 0);
   free_seen.push_back(ocs::tom::find_first<TEST_uint32>(reduced, 2) != nullptr ? 1 : 0);
   lFreeList(&reduced);
   lFreeList(&lp);
   lFreeList(&tasks);
   CHECK(5, "a list of another type or a reduced list without the field finds nothing in both forms, without abort",
         old_seen == free_seen && old_seen == std::vector<int>({0, 0, 0, 0, 0, 0, 0, 0, 1}));
}

/**
 * @brief Lookups in the list of a list field
 */
void test_find_in() {
   // old:     lGetSubStr(job, VA_variable, "b", JB_env_list), lGetSubUlongRW(job, JAT_task_number, 2, JB_ja_tasks)
   // free:    find_first_in<JB_env_list, VA_variable>(job, "b"), find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2)
   // differs: a key field that is not of the list field's sub-type does not compile; the old function took two
   //          field numbers in an order easy to swap
   lListElem *job = make_job();
   const bool same =
      lGetSubStr(job, VA_variable, "b", JB_env_list) == ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "b") &&
      lGetSubStrRW(job, VA_variable, "x", JB_env_list) ==
         ocs::tom::find_first_in_rw<JB_env_list, VA_variable>(job, "x") &&
      lGetSubUlongRW(job, JAT_task_number, 2, JB_ja_tasks) ==
         ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2) &&
      ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "b") != nullptr &&
      // a null element finds nothing in both
      lGetSubStr(nullptr, VA_variable, "b", JB_env_list) == nullptr &&
      ocs::tom::find_first_in<JB_env_list, VA_variable>(nullptr, "b") == nullptr;
   lFreeElem(&job);
   CHECK(6, "find_first_in<LF, F> finds what lGetSubStr and lGetSubUlong find, nothing on a null element",
         same);

   // old:     lGetSubStr(reduced_job, ...) aborts
   // free:    find_first_in<JB_env_list, VA_variable>(reduced_job, ...) throws, which ends the process as well
   lList *jobs = nullptr;
   lList *reduced = nullptr;
   const lListElem *rjob = make_reduced_job(jobs, reduced);
   const bool aborts =
      in_child([&] { return lGetSubStr(rjob, VA_variable, "b", JB_env_list) != nullptr; }) == Aborted &&
      in_child([&] { return ocs::tom::find_first_in<JB_env_list, VA_variable>(rjob, "b") != nullptr; }) == Aborted;
   lFreeList(&reduced);
   lFreeList(&jobs);
   CHECK(7, "an element without the list field ends the process in both forms", aborts);
}

/**
 * @brief Adding and deleting by a key in a list
 */
void test_add_erase() {
   // old:     lAddElemStr(&lp, TEST_string, "a", TEST_Type), lDelElemStr(&lp, TEST_string, "a")
   // free:    emplace_back<TEST_string>(&lp, "a"), erase<TEST_string>(&lp, "a")
   // differs: the descriptor comes from the field, not from a parameter
   const auto run = [](auto add_str, auto add_ulong, auto del_str, auto del_ulong) {
      std::vector<std::string> seen;
      lList *strings = nullptr;
      lList *ulongs = nullptr;
      seen.push_back(add_str(&strings, "a") != nullptr ? "added" : "null");
      seen.push_back(add_str(&strings, "b") != nullptr ? "added" : "null");
      seen.push_back(add_str(&strings, nullptr) != nullptr ? "added" : "null");
      seen.push_back(add_str(nullptr, "c") != nullptr ? "added" : "null");
      seen.push_back(add_ulong(&ulongs, 7) != nullptr ? "added" : "null");
      seen.push_back(keys(strings, TEST_string));
      seen.push_back(numbers(ulongs, TEST_uint32));
      seen.push_back(std::to_string(del_str(&strings, "x")));
      seen.push_back(std::to_string(del_str(&strings, "a")));
      seen.push_back(std::to_string(del_str(&strings, nullptr)));
      seen.push_back(std::to_string(del_str(nullptr, "b")));
      seen.push_back(keys(strings, TEST_string));
      // the last element removed frees the list
      seen.push_back(std::to_string(del_str(&strings, "b")));
      seen.push_back(keys(strings, TEST_string));
      // a list that is nullptr already gives 1
      seen.push_back(std::to_string(del_str(&strings, "b")));
      seen.push_back(std::to_string(del_ulong(&ulongs, 8)));
      seen.push_back(std::to_string(del_ulong(&ulongs, 7)));
      seen.push_back(numbers(ulongs, TEST_uint32));
      lFreeList(&strings);
      lFreeList(&ulongs);
      return seen;
   };

   // --- CULL
   const std::vector<std::string> old_seen =
      run([](lList **lpp, const char *k) { return lAddElemStr(lpp, TEST_string, k, TEST_Type); },
          [](lList **lpp, lUlong k) { return lAddElemUlong(lpp, TEST_uint32, k, TEST_Type); },
          [](lList **lpp, const char *k) { return lDelElemStr(lpp, TEST_string, k); },
          [](lList **lpp, lUlong k) { return lDelElemUlong(lpp, TEST_uint32, k); });
   // --- free form
   const std::vector<std::string> free_seen =
      run([](lList **lpp, const char *k) { return ocs::tom::emplace_back<TEST_string>(lpp, k); },
          [](lList **lpp, lUlong k) { return ocs::tom::emplace_back<TEST_uint32>(lpp, k); },
          [](lList **lpp, const char *k) { return ocs::tom::erase<TEST_string>(lpp, k); },
          [](lList **lpp, lUlong k) { return ocs::tom::erase<TEST_uint32>(lpp, k); });

   const std::vector<std::string> expected{"added", "added", "null", "null", "added", "a b ", "7 ",
                                           "0",     "1",     "0",    "0",    "b ",    "1",   "null",
                                           "1",     "0",     "1",    "null"};
   show_if_differs("add and erase", old_seen, free_seen, expected);
   CHECK(8, "emplace_back<F> and erase<F> return, create and free the lists as lAddElemStr/Ulong and "
            "lDelElemStr/Ulong do",
         old_seen == expected && free_seen == expected);

   // old:     lAddElemHost(&tasks, TEST_host, "h", TEST_Type) on a list of array tasks
   // free:    emplace_back<TEST_host>(&tasks, "h")
   // differs: nothing; both write the message for a field of another type, record the soft path and give
   //          nullptr, and a delete finds nothing. Before CS-3045 the old function appended a TEST element to the
   //          list of array tasks, which then held two types.
   lList *tasks = lCreateList("tasks", JAT_Type);
   lAppendElem(tasks, lCreateElem(JAT_Type));
   const std::vector<int> old_foreign{
      in_child([&] { return lAddElemHost(&tasks, TEST_host, "h", TEST_Type) == nullptr ? 0 : 1; }),
      in_child([&] { return lDelElemHost(&tasks, TEST_host, "h"); })};
   const std::vector<int> free_foreign{
      in_child([&] { return ocs::tom::emplace_back<TEST_host>(&tasks, "h") == nullptr ? 0 : 1; }),
      in_child([&] { return ocs::tom::erase<TEST_host>(&tasks, "h"); })};
   lFreeList(&tasks);
   show_if_differs("foreign list", old_foreign, free_foreign, std::vector<int>({0, 0}));
   CHECK(9, "adding to or deleting from a list of another type gives nullptr and 0 in both forms",
         old_foreign == free_foreign && old_foreign == std::vector<int>({0, 0}));
}

/**
 * @brief Adding and deleting by a key in the list of a list field
 */
void test_add_erase_in() {
   // old:     lAddSubStr(job, VA_variable, "C", JB_env_list, VA_Type), lDelSubStr(job, VA_variable, "A", JB_env_list)
   // free:    emplace_back_in<JB_env_list, VA_variable>(job, "C"), erase_in<JB_env_list, VA_variable>(job, "A")
   // differs: the descriptor comes from the field; a key field of another sub-type does not compile
   const auto run = [](auto add, auto del, auto add_task, auto del_task) {
      std::vector<std::string> seen;
      lListElem *job = make_job();
      seen.push_back(add(job, "C") != nullptr ? "added" : "null");
      seen.push_back(add(nullptr, "D") != nullptr ? "added" : "null");
      seen.push_back(std::to_string(del(job, "A")));
      seen.push_back(std::to_string(del(job, "x")));
      seen.push_back(std::to_string(del(nullptr, "b")));
      seen.push_back(keys(lGetList(job, JB_env_list), VA_variable));
      seen.push_back(add_task(job, 3) != nullptr ? "added" : "null");
      seen.push_back(std::to_string(del_task(job, 1)));
      seen.push_back(numbers(lGetList(job, JB_ja_tasks), JAT_task_number));
      lFreeElem(&job);
      return seen;
   };

   // --- CULL
   const std::vector<std::string> old_seen = run(
      [](lListElem *ep, const char *k) { return lAddSubStr(ep, VA_variable, k, JB_env_list, VA_Type); },
      [](lListElem *ep, const char *k) { return lDelSubStr(ep, VA_variable, k, JB_env_list); },
      [](lListElem *ep, lUlong k) { return lAddSubUlong(ep, JAT_task_number, k, JB_ja_tasks, JAT_Type); },
      [](lListElem *ep, lUlong k) { return lDelSubUlong(ep, JAT_task_number, k, JB_ja_tasks); });
   // --- free form
   const std::vector<std::string> free_seen = run(
      [](lListElem *ep, const char *k) { return ocs::tom::emplace_back_in<JB_env_list, VA_variable>(ep, k); },
      [](lListElem *ep, const char *k) { return ocs::tom::erase_in<JB_env_list, VA_variable>(ep, k); },
      [](lListElem *ep, lUlong k) { return ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(ep, k); },
      [](lListElem *ep, lUlong k) { return ocs::tom::erase_in<JB_ja_tasks, JAT_task_number>(ep, k); });

   const std::vector<std::string> expected{"added", "null", "1", "0", "0", "b C ", "added", "1", "2 3 "};
   show_if_differs("add and erase in", old_seen, free_seen, expected);
   CHECK(10, "emplace_back_in<LF, F> and erase_in<LF, F> do what lAddSubStr/Ulong and lDelSubStr/Ulong do",
         old_seen == expected && free_seen == expected);

   // old:     lAddSubStr(reduced_job, ...) and lDelSubStr(reduced_job, ...) abort
   // free:    the free forms throw, which ends the process as well
   lList *jobs = nullptr;
   lList *reduced = nullptr;
   lListElem *rjob = make_reduced_job(jobs, reduced);
   const bool aborts =
      in_child([&] { return lAddSubStr(rjob, VA_variable, "X", JB_env_list, VA_Type) != nullptr; }) == Aborted &&
      in_child([&] { return lDelSubStr(rjob, VA_variable, "X", JB_env_list); }) == Aborted &&
      in_child([&] { return ocs::tom::emplace_back_in<JB_env_list, VA_variable>(rjob, "X") != nullptr; }) ==
         Aborted &&
      in_child([&] { return ocs::tom::erase_in<JB_env_list, VA_variable>(rjob, "X"); }) == Aborted;
   lFreeList(&reduced);
   lFreeList(&jobs);
   CHECK(11, "an element without the list field ends the process in both forms", aborts);
}

/**
 * @brief The presence check
 */
void test_has() {
   // old:     lGetPosViaElem(ep, JB_env_list, SGE_NO_ABORT) >= 0
   // free:    has<JB_env_list>(ep)
   // differs: a bool instead of a position compared with -1 or 0
   lList *jobs = nullptr;
   lList *reduced = nullptr;
   const lListElem *rjob = make_reduced_job(jobs, reduced);
   const lListElem *task = lFirst(lGetList(lFirst(jobs), JB_ja_tasks));
   std::vector<bool> old_seen;
   std::vector<bool> free_seen;
   for (const lListElem *ep : {lFirst(jobs), rjob, task, static_cast<const lListElem *>(nullptr)}) {
      old_seen.push_back(lGetPosViaElem(ep, JB_env_list, SGE_NO_ABORT) >= 0);
      old_seen.push_back(lGetPosViaElem(ep, JB_job_number, SGE_NO_ABORT) != -1);
      free_seen.push_back(ocs::tom::has<JB_env_list>(ep));
      free_seen.push_back(ocs::tom::has<JB_job_number>(ep));
   }
   lFreeList(&reduced);
   lFreeList(&jobs);
   CHECK(12, "has<F> is true exactly where lGetPosViaElem(ep, F, SGE_NO_ABORT) gives a position",
         old_seen == free_seen &&
            old_seen == std::vector<bool>({true, true, false, true, false, false, false, false}));
}

/**
 * @brief Exchanging and swapping list fields, exchanging a string
 */
void test_exchange() {
   // old:     lXchgList(job, JB_env_list, &lp), lXchgString(ep, TEST_string, &s), lSwapList(a, JB_env_list, b, ...)
   // free:    exchange<JB_env_list>(job, &lp), exchange<TEST_string>(ep, &s), swap<JB_env_list, JB_env_list>(a, b)
   // differs: lSwapList with a null first element had already taken the list out of the second and lost it; the
   //          free form changes neither element then
   const auto run = [](auto xchg, auto xchg_str, auto swp) {
      std::vector<std::string> seen;
      lListElem *a = make_job();
      lListElem *b = lCreateElem(JB_Type);
      lAddSubStr(b, VA_variable, "Z", JB_env_list, VA_Type);
      lList *other = nullptr;
      lAddElemStr(&other, VA_variable, "Q", VA_Type);
      cull_state_set_lerrno(0);
      seen.push_back(std::to_string(xchg(a, &other)));
      seen.push_back(keys(lGetList(a, JB_env_list), VA_variable) + "/" + keys(other, VA_variable));
      seen.push_back(then(xchg(nullptr, &other), cull_state_get_lerrno));
      cull_state_set_lerrno(0);
      seen.push_back(then(xchg(a, nullptr), cull_state_get_lerrno));
      seen.push_back(std::to_string(swp(a, b)));
      seen.push_back(keys(lGetList(a, JB_env_list), VA_variable) + "/" + keys(lGetList(b, JB_env_list), VA_variable));
      cull_state_set_lerrno(0);
      seen.push_back(then(swp(b, nullptr), cull_state_get_lerrno));
      seen.push_back(keys(lGetList(b, JB_env_list), VA_variable));

      lListElem *t = lCreateElem(TEST_Type);
      lSetString(t, TEST_string, "held");
      char *s = strdup("new");
      seen.push_back(std::to_string(xchg_str(t, &s)));
      seen.push_back(std::string(lGetString(t, TEST_string)) + "/" + s);
      free(s);
      lFreeElem(&t);
      lFreeList(&other);
      lFreeElem(&a);
      lFreeElem(&b);
      return seen;
   };

   // --- CULL
   const std::vector<std::string> old_seen =
      run([](lListElem *ep, lList **lpp) { return lXchgList(ep, JB_env_list, lpp); },
          [](lListElem *ep, char **s) { return lXchgString(ep, TEST_string, s); },
          [](lListElem *to, lListElem *from) { return lSwapList(to, JB_env_list, from, JB_env_list); });
   // --- free form
   const std::vector<std::string> free_seen =
      run([](lListElem *ep, lList **lpp) { return ocs::tom::exchange<JB_env_list>(ep, lpp); },
          [](lListElem *ep, char **s) { return ocs::tom::exchange<TEST_string>(ep, s); },
          [](lListElem *to, lListElem *from) { return ocs::tom::swap<JB_env_list, JB_env_list>(to, from); });

   const std::string elemnull = std::to_string(LEELEMNULL);
   const std::vector<std::string> expected{
      "0", "Q /A b ", "-1 " + elemnull, "-1 " + elemnull, "0", "Z /Q ", "-1 " + elemnull, "Q ", "0", "new/held"};
   show_if_differs("exchange", old_seen, free_seen, expected);
   CHECK(13, "exchange<F> and swap<F, G> return, set the error numbers and move the lists as lXchgList, lXchgString "
             "and lSwapList do",
         old_seen == expected && free_seen == expected);
}

/**
 * @brief The small ones: a list created on demand, a list reference, a string never null, bit masks
 */
void test_small() {
   // old:     lGetOrCreateList(job, JB_env_list, "env", VA_Type), lGetListRef(job, JB_env_list),
   //          lGetStringNotNull(ep, TEST_string), lMatchUlongBitMask / lOrUlongBitMask / lAndUlongBitMask /
   //          lClearUlongBitMask(task, JAT_status, mask)
   // free:    get_or_create<JB_env_list>(job, "env"), list_ref<JB_env_list>(job), get_or_empty<TEST_string>(ep),
   //          has_bits / or_bits / and_bits / clear_bits<JAT_status>(task, mask)
   // differs: the descriptor of a created list comes from the field
   const auto run = [](auto create, auto ref, auto not_null, auto match, auto set_or, auto set_and, auto clear) {
      std::vector<std::string> seen;
      lListElem *job = lCreateElem(JB_Type);
      lList *created = create(job);
      seen.push_back(created != nullptr && created == lGetList(job, JB_env_list) ? "created" : "none");
      seen.push_back(create(job) == created ? "same" : "other");
      seen.push_back(create(nullptr) == nullptr ? "null" : "list");
      seen.push_back(ref(job) == lGetListRef(job, JB_env_list) ? "ref" : "other");
      lFreeElem(&job);

      lListElem *t = lCreateElem(TEST_Type);
      seen.push_back(std::string("[") + not_null(t) + "]");
      lSetString(t, TEST_string, "x");
      seen.push_back(std::string("[") + not_null(t) + "]");
      lFreeElem(&t);

      lListElem *task = lCreateElem(JAT_Type);
      lSetUlong(task, JAT_status, 0x5);
      seen.push_back(match(task, 0x4) ? "match" : "no");
      seen.push_back(match(task, 0x2) ? "match" : "no");
      seen.push_back(then(set_or(task, 0x2), [&] { return lGetUlong(task, JAT_status); }));
      seen.push_back(then(set_and(task, 0x6), [&] { return lGetUlong(task, JAT_status); }));
      seen.push_back(then(clear(task, 0x2), [&] { return lGetUlong(task, JAT_status); }));
      cull_state_set_lerrno(0);
      seen.push_back(then(clear(nullptr, 0x2), cull_state_get_lerrno));
      lFreeElem(&task);
      return seen;
   };

   // --- CULL
   const std::vector<std::string> old_seen =
      run([](lListElem *ep) { return lGetOrCreateList(ep, JB_env_list, "env", VA_Type); },
          [](const lListElem *ep) { return lGetListRef(ep, JB_env_list); },
          [](const lListElem *ep) { return lGetStringNotNull(ep, TEST_string); },
          [](lListElem *ep, std::uint32_t m) { return lMatchUlongBitMask(ep, JAT_status, m); },
          [](lListElem *ep, std::uint32_t m) { return lOrUlongBitMask(ep, JAT_status, m); },
          [](lListElem *ep, std::uint32_t m) { return lAndUlongBitMask(ep, JAT_status, m); },
          [](lListElem *ep, std::uint32_t m) { return lClearUlongBitMask(ep, JAT_status, m); });
   // --- free form
   const std::vector<std::string> free_seen =
      run([](lListElem *ep) { return ocs::tom::get_or_create<JB_env_list>(ep, "env"); },
          [](const lListElem *ep) { return ocs::tom::list_ref<JB_env_list>(ep); },
          [](const lListElem *ep) { return ocs::tom::get_or_empty<TEST_string>(ep); },
          [](lListElem *ep, std::uint32_t m) { return ocs::tom::has_bits<JAT_status>(ep, m); },
          [](lListElem *ep, std::uint32_t m) { return ocs::tom::or_bits<JAT_status>(ep, m); },
          [](lListElem *ep, std::uint32_t m) { return ocs::tom::and_bits<JAT_status>(ep, m); },
          [](lListElem *ep, std::uint32_t m) { return ocs::tom::clear_bits<JAT_status>(ep, m); });

   const std::vector<std::string> expected{"created", "same", "null",  "ref",  "[]",
                                           "[x]",     "match", "no",   "0 7", "0 6",
                                           "0 4",     "-1 " + std::to_string(LEELEMNULL)};
   show_if_differs("small ones", old_seen, free_seen, expected);
   CHECK(14, "get_or_create, list_ref, get_or_empty and the bit mask forms do what lGetOrCreateList, lGetListRef, "
             "lGetStringNotNull and the bit mask functions do",
         old_seen == expected && free_seen == expected);
}

/**
 * @brief The errors that end the process
 */
void test_aborts() {
   // old:     lXchgList on a reduced job, lGetListRef and lMatchUlongBitMask on a null element, lAddElemUlong with a
   //          key held already in a unique field (debug build)
   // free:    the free forms throw a FieldException, which nothing catches
   // differs: nothing, the process ends in both
   lList *jobs = nullptr;
   lList *reduced = nullptr;
   lListElem *rjob = make_reduced_job(jobs, reduced);
   lList *other = nullptr;
   std::vector<int> old_seen{
      in_child([&] { return lXchgList(rjob, JB_env_list, &other); }),
      in_child([] { return lGetListRef(nullptr, JB_env_list) != nullptr ? 1 : 0; }),
      in_child([] { return lMatchUlongBitMask(nullptr, JAT_status, 1) ? 1 : 0; }),
      in_child([&] { return lGetOrCreateList(rjob, JB_env_list, "env", VA_Type) != nullptr ? 1 : 0; })};
   std::vector<int> free_seen{
      in_child([&] { return ocs::tom::exchange<JB_env_list>(rjob, &other); }),
      in_child([] { return ocs::tom::list_ref<JB_env_list>(nullptr) != nullptr ? 1 : 0; }),
      in_child([] { return ocs::tom::has_bits<JAT_status>(nullptr, 1) ? 1 : 0; }),
      in_child([&] { return ocs::tom::get_or_create<JB_env_list>(rjob, "env") != nullptr ? 1 : 0; })};
   lFreeList(&reduced);
   lFreeList(&jobs);
#ifdef ENABLE_DEBUG_CHECKS
   // a unique key field with a hash table, which checks the key on adding
   lList *tasks = nullptr;
   for (lUlong i = 1; i <= 40; i++) {
      lAddElemUlong(&tasks, JAT_task_number, i, JAT_Type);
   }
   static_cast<void>(lGetElemUlong(tasks, JAT_task_number, 1));
   old_seen.push_back(in_child([&] { return lAddElemUlong(&tasks, JAT_task_number, 1, JAT_Type) != nullptr ? 1 : 0; }));
   free_seen.push_back(in_child([&] { return ocs::tom::emplace_back<JAT_task_number>(&tasks, 1) != nullptr ? 1 : 0; }));
   lFreeList(&tasks);
#endif
   const std::vector<int> expected(old_seen.size(), Aborted);
   show_if_differs("aborts", old_seen, free_seen, expected);
   CHECK(15, "an element without the field, a null element where the old getter aborts and, in a debug build, a "
             "duplicate key end the process in both forms",
         old_seen == expected && free_seen == expected);
}

/**
 * @brief In a Release build, time a lookup and an add and delete by key with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 10000;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   std::vector<std::string> names;
   for (int i = 0; i < n; i++) {
      names.push_back("name" + std::to_string(i));
   }

   // one round of each form, so that the second measures a warm allocator; the ratio is taken from the second
   double old_ms[3] = {};
   double free_ms[3] = {};
   int old_found = 0;
   int free_found = 0;
   lList *old_list = nullptr;
   lList *free_list = nullptr;
   for (int round = 0; round < 2; round++) {
      // --- CULL
      old_found = 0;
      auto t0 = std::chrono::steady_clock::now();
      for (const std::string &name : names) {
         lAddElemStr(&old_list, CE_name, name.c_str(), CE_Type);
      }
      auto t1 = std::chrono::steady_clock::now();
      for (int r = 0; r < 10; r++) {
         for (const std::string &name : names) {
            old_found += lGetElemStr(old_list, CE_name, name.c_str()) != nullptr ? 1 : 0;
         }
      }
      auto t2 = std::chrono::steady_clock::now();
      // delete from the back, so that each delete looks a key up through the hash table
      for (auto it = names.rbegin(); it != names.rend(); ++it) {
         lDelElemStr(&old_list, CE_name, it->c_str());
      }
      auto t3 = std::chrono::steady_clock::now();

      // --- free form
      free_found = 0;
      auto t4 = std::chrono::steady_clock::now();
      for (const std::string &name : names) {
         ocs::tom::emplace_back<CE_name>(&free_list, name.c_str());
      }
      auto t5 = std::chrono::steady_clock::now();
      for (int r = 0; r < 10; r++) {
         for (const std::string &name : names) {
            free_found += ocs::tom::find_first<CE_name>(free_list, name.c_str()) != nullptr ? 1 : 0;
         }
      }
      auto t6 = std::chrono::steady_clock::now();
      for (auto it = names.rbegin(); it != names.rend(); ++it) {
         ocs::tom::erase<CE_name>(&free_list, it->c_str());
      }
      auto t7 = std::chrono::steady_clock::now();
      old_ms[0] = ms(t0, t1);
      old_ms[1] = ms(t1, t2);
      old_ms[2] = ms(t2, t3);
      free_ms[0] = ms(t4, t5);
      free_ms[1] = ms(t5, t6);
      free_ms[2] = ms(t6, t7);
   }

   printf("timing, %d complex entries by name, second of two rounds:\n", n);
   printf("   add:          old %8.2f ms   free %8.2f ms   free/old %.2f\n", old_ms[0], free_ms[0], free_ms[0] / old_ms[0]);
   printf("   find (x 10):  old %8.2f ms   free %8.2f ms   free/old %.2f\n", old_ms[1], free_ms[1], free_ms[1] / old_ms[1]);
   printf("   delete:       old %8.2f ms   free %8.2f ms   free/old %.2f\n", old_ms[2], free_ms[2], free_ms[2] / old_ms[2]);
   CHECK(16, "the timing loops found every element and emptied both lists",
         old_found == 10 * n && free_found == 10 * n && old_list == nullptr && free_list == nullptr);
#endif
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
// the free forms that return an element or a list give a handle of the key field's type (CS-3045)
static_assert(std::is_same_v<decltype(ocs::tom::find_first<CE_name>(nullptr, "")), ocs::tom::ConstElement<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first_rw<CE_name>(nullptr, "")), ocs::tom::Element<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first<CE_name>(nullptr, "", nullptr)),
                             ocs::tom::ConstElement<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_next_rw<CE_name>(nullptr, "", nullptr)),
                             ocs::tom::Element<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first_nocase<CE_name>(nullptr, "")),
                             ocs::tom::ConstElement<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first_like_rw<CE_name>(nullptr, "")),
                             ocs::tom::Element<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first_in<JB_env_list, VA_variable>(nullptr, "")),
                             ocs::tom::ConstElement<ocs::tom::VA>>);
static_assert(std::is_same_v<decltype(ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(nullptr, 1)),
                             ocs::tom::Element<ocs::tom::JAT>>);
static_assert(std::is_same_v<decltype(ocs::tom::emplace_back<CE_name>(nullptr, "")), ocs::tom::Element<ocs::tom::CE>>);
static_assert(std::is_same_v<decltype(ocs::tom::emplace_back_in<JB_env_list, VA_variable>(nullptr, "")),
                             ocs::tom::Element<ocs::tom::VA>>);
static_assert(std::is_same_v<decltype(ocs::tom::get_or_create<JB_env_list>(nullptr, nullptr)), ocs::tom::List<ocs::tom::VA>>);

/**
 * @brief The handles the free forms return stand in for the old pointers
 *
 * The rewrite keeps every variable that takes the result of a free form a
 * raw pointer; the handle has to convert to it, compare with nullptr and
 * test as a condition, as the pointer did.
 */
void test_handles() {
   // old:     const lListElem *ce = lGetElemStr(lp, CE_name, "h_rt"); if (ce != nullptr) ...
   // free:    const lListElem *ce = ocs::tom::find_first<CE_name>(lp, "h_rt"); the same lines
   // differs: the result is a ConstElement<CE>, which converts to the pointer
   lList *lp = lCreateList("ce", CE_Type);
   lListElem *added = ocs::tom::emplace_back<CE_name>(&lp, "h_rt");
   const lListElem *found = ocs::tom::find_first<CE_name>(lp, "h_rt");
   lListElem *found_rw = ocs::tom::find_first_rw<CE_name>(lp, "h_rt");
   CHECK(17, "the handle converts to the pointer", added != nullptr && found == added && found_rw == added);
   const auto missing = ocs::tom::find_first<CE_name>(lp, "none");
   CHECK(18, "an empty handle compares equal to nullptr and tests false", missing == nullptr && !missing);
   if (const auto hit = ocs::tom::find_first<CE_name>(lp, "h_rt")) {
      CHECK(19, "a found handle tests true and holds the element", hit.raw() == added);
   } else {
      CHECK(19, "a found handle tests true and holds the element", false);
   }
   CHECK(20, "a null list gives an empty handle", ocs::tom::find_first<CE_name>(nullptr, "h_rt") == nullptr);
   lFreeList(&lp);

   lListElem *job = lCreateElem(JB_Type);
   lList *env = ocs::tom::get_or_create<JB_env_list>(job, "env");
   CHECK(21, "the list handle converts to the list", env != nullptr && env == lGetList(job, JB_env_list));
   CHECK(22, "a null element gives an empty list handle",
         ocs::tom::get_or_create<JB_env_list>(nullptr, nullptr) == nullptr);
   lFreeElem(&job);
}

/**
 * @brief The old walking names take a handle and keep its type (CS-3078)
 *
 * lFirst(), lNext() and their variants are overloaded for the handles: the
 * walk over a list handle gives element handles of the list's type, and the
 * old walking macros run unchanged over a handle, with a handle or a pointer
 * as the running variable.
 */
void test_walk() {
   // old:     for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep))
   // typed:   the same line with lp a ConstList<CE>: ep may be a ConstElement<CE>
   // differs: nothing; a pointer argument still takes the old function
   lList *lp = nullptr;
   for (const char *name : {"a", "b", "c"}) {
      ocs::tom::emplace_back<CE_name>(&lp, name);
   }
   const auto ces = ocs::tom::List<ocs::tom::CE>::from_typed(lp);
   static_assert(std::is_same_v<decltype(lFirst(ces)), ocs::tom::ConstElement<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lFirstRW(ces)), ocs::tom::Element<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lLast(ces)), ocs::tom::ConstElement<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lNext(lFirst(ces))), ocs::tom::ConstElement<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lNextRW(lFirstRW(ces))), ocs::tom::Element<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lPrev(lLast(ces))), ocs::tom::ConstElement<ocs::tom::CE>>);
   static_assert(std::is_same_v<decltype(lFirst(static_cast<const lList *>(lp))), const lListElem *>);

   std::vector<const lListElem *> old_walk;
   for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
      old_walk.push_back(ep);
   }
   std::vector<const lListElem *> typed_walk;
   for (ocs::tom::ConstElement<ocs::tom::CE> ep = lFirst(ces); ep; ep = lNext(ep)) {
      typed_walk.push_back(ep.raw());
   }
   std::vector<const lListElem *> back_walk;
   for (auto ep = lLast(ces); ep; ep = lPrev(ep)) {
      back_walk.insert(back_walk.begin(), ep.raw());
   }
   CHECK(23, "lFirst() and lNext() on a handle walk the elements lFirst() and lNext() on the pointer walk",
         old_walk.size() == 3 && typed_walk == old_walk && back_walk == old_walk);

   std::vector<const lListElem *> macro_handle;
   ocs::tom::ConstElement<ocs::tom::CE> handle;
   for_each_ep(handle, ces) {
      macro_handle.push_back(handle.raw());
   }
   std::vector<const lListElem *> macro_pointer;
   const lListElem *pointer = nullptr;
   for_each_ep(pointer, ces) {
      macro_pointer.push_back(pointer);
   }
   std::vector<const lListElem *> macro_lv;
   for_each_ep_lv(ep, ces) {
      macro_lv.push_back(ep);
   }
   CHECK(24, "for_each_ep runs over a list handle with a handle or a pointer, for_each_ep_lv with its pointer",
         macro_handle == old_walk && macro_pointer == old_walk && macro_lv == old_walk);

   const ocs::tom::ConstList<ocs::tom::CE> empty;
   CHECK(25, "an empty list handle and an empty element handle walk to nothing",
         !lFirst(empty) && !lLast(empty) && !lNext(ocs::tom::ConstElement<ocs::tom::CE>()));
   lFreeList(&lp);
}

/**
 * @brief The free forms on an element handle behave as on the raw pointer, without the run-time type check (L2)
 *
 * A variable that became a handle keeps its free forms. The handle's type is
 * known, so a field of another type does not compile (see
 * compile_fail/free_handle_foreign_field.cc), and an empty handle and a
 * reduced element are the only errors left at run time.
 */
void test_handle_forms() {
   // old:     lSetUlong(job, JB_job_number, 7); lGetSubStr(job, VA_variable, "A", JB_env_list)
   // free:    ocs::tom::set<JB_job_number>(job, 7); ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "A")
   // differs: with job an Element<JB>, the same lines take the overload for the handle
   lListElem *job = make_job();
   const auto handle = ocs::tom::Element<ocs::tom::JB>::from_typed(job);
   const ocs::tom::ConstElement<ocs::tom::JB> const_handle = handle;
   bool same = ocs::tom::set<JB_job_number>(handle, 7) == 0 && ocs::tom::add<JB_job_number>(handle, 3) == 0 &&
               ocs::tom::get<JB_job_number>(const_handle) == 10 && ocs::tom::get<JB_job_number>(job) == 10;
   same = same && ocs::tom::has<JB_env_list>(const_handle) &&
          ocs::tom::find_first_in<JB_env_list, VA_variable>(const_handle, "A") ==
             ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "A") &&
          ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(const_handle, 2) ==
             ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2);
   same = same && ocs::tom::emplace_back_in<JB_env_list, VA_variable>(handle, "C") != nullptr &&
          ocs::tom::erase_in<JB_env_list, VA_variable>(handle, "C") == 1;
   same = same && ocs::tom::get_or_create<JB_env_list>(handle, nullptr) == lGetList(job, JB_env_list) &&
          ocs::tom::list_ref<JB_env_list>(const_handle) == lGetListRef(job, JB_env_list) &&
          std::string(ocs::tom::get_or_empty<JB_owner>(const_handle)).empty();
   char *owner = strdup("me");
   same = same && ocs::tom::exchange<JB_owner>(handle, &owner) == 0 && owner == nullptr &&
          std::string(ocs::tom::get_or_empty<JB_owner>(const_handle)) == "me";
   lList *env = nullptr;
   same = same && ocs::tom::exchange<JB_env_list>(handle, &env) == 0 && env != nullptr &&
          ocs::tom::exchange<JB_env_list>(handle, &env) == 0 && env == nullptr;
   lListElem *other = make_job();
   const auto other_handle = ocs::tom::Element<ocs::tom::JB>::from_typed(other);
   lList *before = lGetListRW(job, JB_env_list);
   same = same && ocs::tom::swap<JB_env_list, JB_env_list>(handle, other_handle) == 0 &&
          lGetList(other, JB_env_list) == before;
   lFreeElem(&other);
   const auto task = ocs::tom::Element<ocs::tom::JAT>::from_typed(lFirstRW(lGetList(job, JB_ja_tasks)));
   same = same && ocs::tom::set<JAT_status>(task, 0x5) == 0 && ocs::tom::has_bits<JAT_status>(task, 0x4) &&
          ocs::tom::or_bits<JAT_status>(task, 0x2) == 0 && ocs::tom::and_bits<JAT_status>(task, 0x6) == 0 &&
          ocs::tom::clear_bits<JAT_status>(task, 0x2) == 0 && ocs::tom::get<JAT_status>(task) == 0x4;
   CHECK(26, "the free forms on an element handle give what they give on the raw pointer", same);

   const ocs::tom::Element<ocs::tom::JB> empty;
   cull_state_set_lerrno(0);
   bool null_reason = false;
   try {
      static_cast<void>(ocs::tom::get<JB_job_number>(empty));
   } catch (const ocs::tom::FieldException &e) {
      null_reason = e.reason() == ocs::tom::FieldException::Reason::NullElement;
   }
   CHECK(27, "an empty handle: get throws NullElement, set gives -1 with LEELEMNULL, has is false",
         null_reason && ocs::tom::set<JB_job_number>(empty, 1) == -1 && cull_state_get_lerrno() == LEELEMNULL &&
            !ocs::tom::has<JB_env_list>(empty));

   lList *jobs = nullptr;
   lList *reduced = nullptr;
   const auto reduced_job = ocs::tom::Element<ocs::tom::JB>::from_typed(make_reduced_job(jobs, reduced));
   bool absent_reason = false;
   try {
      static_cast<void>(ocs::tom::get<JB_owner>(reduced_job));
   } catch (const ocs::tom::FieldException &e) {
      absent_reason = e.reason() == ocs::tom::FieldException::Reason::AbsentField;
   }
   CHECK(28, "a reduced element without the field: get throws AbsentField, has is false, the held field reads",
         absent_reason && !ocs::tom::has<JB_env_list>(reduced_job) && ocs::tom::get<JB_job_number>(reduced_job) == 0);
   lFreeList(&reduced);
   lFreeList(&jobs);
   lFreeElem(&job);
}

/**
 * @brief OPS-FREE-16: an owning element handle takes the free forms, and the old functions that take an element over
 *        take it moved in (rule L3)
 *
 * A local variable that owns its element becomes an ElementPtr: its field
 * accesses stay as they are, a call that takes the element over gets
 * std::move, and the destructor frees what no call took over. The handle
 * gives up its element only when the call took it.
 */
void test_owning() {
   // old:     lListElem *job = lCreateElem(JB_Type); lSetUlong(job, JB_job_number, 7); lAppendElem(jobs, job);
   // free:    auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create(); ocs::tom::set<JB_job_number>(job, 7);
   //          lAppendElem(jobs, std::move(job));
   // differs: the transfer shows at the call, and an element no call takes over is freed
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>(make_job());
   bool same = ocs::tom::set<JB_job_number>(job, 7) == 0 && ocs::tom::add<JB_job_number>(job, 3) == 0 &&
               ocs::tom::get<JB_job_number>(job) == 10 && ocs::tom::has<JB_env_list>(job) &&
               ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "A") ==
                  ocs::tom::find_first_in<JB_env_list, VA_variable>(job.get(), "A") &&
               ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 2) != nullptr &&
               ocs::tom::emplace_back_in<JB_env_list, VA_variable>(job, "C") != nullptr &&
               ocs::tom::erase_in<JB_env_list, VA_variable>(job, "C") == 1 &&
               ocs::tom::get_or_create<JB_env_list>(job, nullptr) == lGetList(*job, JB_env_list) &&
               ocs::tom::list_ref<JB_env_list>(job) == lGetListRef(*job, JB_env_list) &&
               std::string(ocs::tom::get_or_empty<JB_owner>(job)).empty();
   char *owner = strdup("me");
   same = same && ocs::tom::exchange<JB_owner>(job, &owner) == 0 && owner == nullptr &&
          std::string(ocs::tom::get_or_empty<JB_owner>(job)) == "me";
   auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   same = same && ocs::tom::set<JAT_status>(task, 0x5) == 0 && ocs::tom::has_bits<JAT_status>(task, 0x4) &&
          ocs::tom::or_bits<JAT_status>(task, 0x2) == 0 && ocs::tom::and_bits<JAT_status>(task, 0x6) == 0 &&
          ocs::tom::clear_bits<JAT_status>(task, 0x2) == 0 && ocs::tom::get<JAT_status>(task) == 0x4;
   CHECK(29, "the free forms on an owning element handle give what they give on the handle it owns", same);

   lList *jobs = lCreateList("jobs", JB_Type);
   lListElem *const held = job.get().raw();
   const bool appended = lAppendElem(jobs, std::move(job)) == 0 && job == nullptr && lFirst(jobs) == held;
   auto second = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lListElem *const second_raw = second.get().raw();
   const bool inserted = lInsertElem(jobs, nullptr, std::move(second)) == 0 && second == nullptr &&
                         lFirst(jobs) == second_raw;
   auto kept = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   const bool not_taken = lAppendElem(nullptr, std::move(kept)) == -1 && kept != nullptr;
   CHECK(30, "lAppendElem and lInsertElem take a moved handle's element over and leave the handle empty; on an "
             "error the handle keeps it", appended && inserted && not_taken);

   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   lListElem *const pe_raw = pe.get().raw();
   auto env = ocs::tom::ListPtr<ocs::tom::VA>::create("env");
   lList *const env_raw = env.get().raw();
   bool stored = ocs::tom::set<JAT_pe_object>(task, std::move(pe)) == 0 && pe == nullptr &&
                 lGetObject(*task, JAT_pe_object) == pe_raw;
   lListElem *const first = lFirstRW(jobs);
   stored = stored && ocs::tom::set<JB_env_list>(first, std::move(env)) == 0 && env == nullptr &&
            lGetList(first, JB_env_list) == env_raw;
   auto env2 = ocs::tom::ListPtr<ocs::tom::VA>::create("env2");
   lList *const env2_raw = env2.get().raw();
   auto pe2 = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   lListElem *const pe2_raw = pe2.get().raw();
   stored = stored && lSetList(first, JB_env_list, std::move(env2)) == 0 && env2 == nullptr &&
            lGetList(first, JB_env_list) == env2_raw && lSetObject(*task, JAT_pe_object, std::move(pe2)) == 0 &&
            pe2 == nullptr && lGetObject(*task, JAT_pe_object) == pe2_raw;
   CHECK(31, "set<F> and lSetList/lSetObject take a moved list or element over, by the field and by its number",
         stored);

   auto bound = ocs::tom::ElementPtr<ocs::tom::PE>(lCopyElem(lGetObject(*task, JAT_pe_object)));
   lListElem *const bound_raw = bound.get().raw();
   auto other_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   const bool first_store = ocs::tom::set<JAT_pe_object>(other_task, std::move(bound)) == 0 && bound == nullptr;
   // an element bound to a field is refused by a second one: the moved-in handle keeps it
   auto rebound = ocs::tom::ElementPtr<ocs::tom::PE>::from_typed(bound_raw);
   const bool refused = ocs::tom::set<JAT_pe_object>(task, std::move(rebound)) == -1 && rebound != nullptr;
   static_cast<void>(rebound.release());   // still bound to other_task, which frees it
   CHECK(32, "a field refusing the element leaves it with the moved-in handle", first_store && refused);

   auto freed = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   const bool tests = static_cast<bool>(freed) && freed != nullptr;
   freed.reset();
   const ocs::tom::ListPtr<ocs::tom::VA> empty_list;
   CHECK(33, "an owning handle compares with nullptr and tests as a condition, empty after reset()",
         tests && freed == nullptr && !freed && empty_list == nullptr);

   auto vars = ocs::tom::ListPtr<ocs::tom::VA>::create("vars");
   auto var = ocs::tom::ElementPtr<ocs::tom::VA>::create();
   ocs::tom::set<VA_variable>(var, "A");
   vars->push_back(std::move(var));
   const ocs::tom::ConstList<ocs::tom::VA> view = vars;
   const ocs::tom::List<ocs::tom::VA> rw_view = vars;
   const lListElem *const front = view.front();
   const bool views = view.raw() == vars.get().raw() && rw_view.raw() == vars.get().raw() && view.size() == 1 &&
                      static_cast<const lListElem *>(lFirst(vars)) == front &&
                      static_cast<const lListElem *>(lFirstRW(vars)) == front &&
                      static_cast<const lListElem *>(lLast(vars)) == front &&
                      static_cast<const lListElem *>(lLastRW(vars)) == front;
   const bool empty_views = ocs::tom::ConstList<ocs::tom::VA>(empty_list).raw() == nullptr && !lFirst(empty_list);
   static_assert(!std::is_constructible_v<ocs::tom::ConstList<ocs::tom::VA>, ocs::tom::ListPtr<ocs::tom::VA> &&>,
                 "no view of a temporary owning handle");
   static_assert(!std::is_constructible_v<ocs::tom::List<ocs::tom::VA>, ocs::tom::ListPtr<ocs::tom::VA> &&>,
                 "no view of a temporary owning handle");
   CHECK(34, "an owning list handle is viewed by List and ConstList, and lFirst/lLast take it, also when empty",
         views && empty_views);
   lFreeList(&jobs);
}

int main(int argc, char *argv[]) {
   lInit(nmv);
   return ocs::module_test::run(argc, argv, {
      {"OPS-FREE-01", "find", "find_first<F> finds what the old lookups find",
       {"look up string, host, 32 and 64 bit keys in lists of 5 and 40 elements; null list and null key"},
       "the same element or nullptr", {{"OPS-02", "covers"}}, {1, 2}, test_find},
      {"OPS-FREE-02", "find all", "find_first<F> with an iterator and find_next<F> walk what the old ones walk",
       {"walk every element with a repeated key, for every key type, below and above the hash threshold"},
       "the same elements in the same order", {{"OPS-02", "covers"}}, {3}, test_find_all},
      {"OPS-FREE-03", "nocase and like", "find_first_nocase<F> and find_first_like<F> find what the old ones find",
       {"look up a string without regard to case and by a prefix pattern"}, "the same element or nullptr",
       {{"OPS-02", "covers"}}, {4}, test_find_string},
      {"OPS-FREE-04", "missing field", "a list without the key field finds nothing in both forms",
       {"look up in a list of another type and in a list reduced without the field"},
       "nullptr from both, no abort; the field the reduced list holds still finds", {{"OPS-02", "covers"}}, {5},
       test_find_missing_field},
      {"OPS-FREE-05", "find in", "find_first_in<LF, F> finds what the old lookups in a sub-list find",
       {"look up in the environment and the array tasks of a job, a null element, a job without the lists"},
       "the same element or nullptr; both end the process without the list field", {{"OPS-02", "covers"}}, {6, 7},
       test_find_in},
      {"OPS-FREE-06", "add and erase", "emplace_back<F> and erase<F> do what lAddElem and lDelElem do",
       {"add and delete strings and numbers, with null arguments, in a list of another type"},
       "the same returns and lists, an emptied list freed", {{"OPS-02", "covers"}}, {8, 9}, test_add_erase},
      {"OPS-FREE-07", "add and erase in", "emplace_back_in<LF, F> and erase_in<LF, F> do what lAddSub and lDelSub do",
       {"add to and delete from the environment and the array tasks of a job, a null element, a job without them"},
       "the same returns and lists; both end the process without the list field", {{"OPS-02", "covers"}}, {10, 11},
       test_add_erase_in},
      {"OPS-FREE-08", "has", "has<F> replaces the presence check with lGetPosViaElem",
       {"a full job, a reduced job, an array task and a null element"}, "the same answers",
       {{"OPS-02", "covers"}}, {12}, test_has},
      {"OPS-FREE-09", "exchange and swap", "exchange<F> and swap<F, G> do what lXchgList, lXchgString and lSwapList do",
       {"exchange a list and a string, swap two lists, with null arguments"},
       "the same returns, error numbers and lists", {{"OPS-02", "covers"}}, {13}, test_exchange},
      {"OPS-FREE-10", "small ones", "get_or_create, list_ref, get_or_empty and the bit masks do what the old ones do",
       {"create a list on demand, take its reference, read an unset string, set and clear bits"},
       "the same results", {{"OPS-02", "covers"}}, {14}, test_small},
      {"OPS-FREE-11", "aborts", "where the old function aborts, the free form ends the process as well",
       {"a reduced job, a null element where a getter is meant, a duplicate key in a debug build"},
       "both end the process", {{"OPS-02", "covers"}}, {15}, test_aborts},
      {"OPS-FREE-12", "timing", "a Release build times both forms",
       {"add, find and delete 10 000 complex entries by name, with both forms, twice"}, "ratios printed",
       {{"OPS-02", "covers"}}, {16}, test_timing},
      {"OPS-FREE-13", "handles", "the free forms that return an element or a list give a handle that stands in for the pointer",
       {"store a found and an added element in a raw pointer, compare with nullptr, test as a condition"},
       "the same pointers, an empty handle for nothing found", {{"OPS-02", "covers"}}, {17, 18, 19, 20, 21, 22}, test_handles},
      {"OPS-FREE-14", "walk", "lFirst(), lNext() and their variants on a handle keep its type",
       {"walk a list handle forwards and backwards, with the old macros and a handle or a pointer as variable"},
       "the same elements as the walk over the pointer", {{"OPS-02", "covers"}}, {23, 24, 25}, test_walk},
      {"OPS-FREE-15", "handle forms", "the free forms on an element handle behave as on the pointer, without the type check",
       {"every form with an element argument on a job and a task handle, an empty handle, a reduced job"},
       "the same results; NullElement and AbsentField as the only errors", {{"OPS-02", "covers"}}, {26, 27, 28},
       test_handle_forms},
      {"OPS-FREE-16", "owning", "an owning element handle takes the free forms and moves into the calls that take its element over",
       {"field access on an owning job and task, append and insert moved in, set of an object and a list field moved in, "
        "a refused store, nullptr comparison, List/ConstList views and lFirst/lLast of an owning list"},
       "the same results as on the handle; a moved handle is empty, a refused one keeps its element",
       {{"OPS-02", "covers"}}, {29, 30, 31, 32, 33, 34}, test_owning},
   });
}
