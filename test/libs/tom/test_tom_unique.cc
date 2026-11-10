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
 * @brief Module test of duplicate keys in unique fields, stage 1: the debug checks and the dropped table (CS-3052, HDL-07, HDL-08)
 *
 * A list below the hash threshold is searched linearly, a list above it
 * through its table; every case runs on both. The old interface aborts on a
 * duplicate, so those cases run in a child process.
 */

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using namespace ocs::tom;

/** @brief A list below the hash threshold, searched linearly */
static constexpr std::uint32_t SHORT_LIST = 5;

/** @brief A list above the hash threshold, searched through its table */
static constexpr std::uint32_t LONG_LIST = 40;

/**
 * @brief Whether a call aborts, run in a child process
 * @param call the call
 * @return true if the child ended with SIGABRT
 */
static bool
aborts(const std::function<void()> &call) {
   std::fflush(nullptr);
   const pid_t pid = fork();
   if (pid == 0) {
      // keep the expected messages and the stack trace out of the test output
      std::freopen("/dev/null", "w", stderr);
      std::freopen("/dev/null", "w", stdout);
      call();
      _exit(0);
   }
   int status = 0;
   waitpid(pid, &status, 0);
   return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

/**
 * @brief Whether a call throws the duplicate key exception, with a message naming field, key and list
 * @param call the call
 * @param field the field name the message must hold
 * @param key the key the message must hold, quoted
 * @param list the list name the message must hold
 * @return true if it threw so
 */
static bool
throws_duplicate(const std::function<void()> &call, const char *field, const char *key, const char *list) {
   try {
      call();
   } catch (const FieldException &e) {
      const std::string quoted = std::string{"\""} + key + "\"";
      return e.reason() == FieldException::Reason::DuplicateKey && std::strstr(e.what(), field) != nullptr &&
             std::strstr(e.what(), quoted.c_str()) != nullptr && std::strstr(e.what(), list) != nullptr;
   }
   return false;
}

/**
 * @brief A list of tasks numbered 1 to n, filled through the old interface
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
 * @brief A list of user entries named u1 to un
 * @param n the number of entries
 * @return the list
 */
static lList *
make_entries(const std::uint32_t n) {
   lList *entries = lCreateList("entries", UE_Type);
   for (std::uint32_t i = 1; i <= n; i++) {
      lAddElemStr(&entries, UE_name, ("u" + std::to_string(i)).c_str(), UE_Type);
   }
   return entries;
}

/**
 * @brief A list of execution hosts named host-1 to host-n, in lower case
 * @param n the number of hosts
 * @return the list
 */
static lList *
make_hosts(const std::uint32_t n) {
   lList *hosts = lCreateList("hosts", EH_Type);
   for (std::uint32_t i = 1; i <= n; i++) {
      lAddElemHost(&hosts, EH_name, ("host-" + std::to_string(i)).c_str(), EH_Type);
   }
   return hosts;
}

/**
 * @brief Whether the table of a field of a list is built and published
 * @param lp the list
 * @param nm the field
 * @return true if it is
 */
static bool
has_table(const lList *lp, const int nm) {
   return list_hash_table(lp, lGetPosInDescr(lGetListDescr(lp), nm)) != nullptr;
}

/**
 * @brief Whether every task of a list is found under its number, and the list holds n tasks
 * @param tasks the list
 * @param n the expected number of tasks
 * @return true if the list is consistent
 */
static bool
tasks_consistent(const lList *tasks, const std::uint32_t n) {
   if (lGetNumberOfElem(tasks) != n) {
      return false;
   }
   const lListElem *ep;
   for_each_ep(ep, tasks) {
      if (lGetElemUlong(tasks, JAT_task_number, lGetUlong(ep, JAT_task_number)) != ep) {
         return false;
      }
   }
   return true;
}

/**
 * @brief HDL-07 step 5: a debug build catches a duplicate key at the append and at the set
 * @return nothing; CHECK records the results
 */
static void
test_debug_checks() {
#ifdef ENABLE_DEBUG_CHECKS
   int check = 1;
   for (const std::uint32_t n : {SHORT_LIST, LONG_LIST}) {
      const char *size = n == SHORT_LIST ? "below the hash threshold" : "above the hash threshold";

      lList *tasks = make_tasks(n);
      const List<JAT> typed{tasks};
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task.get().set<JAT_task_number>(3u);
      const bool typed_append = throws_duplicate([&] { typed.append(std::move(task)); }, "JAT_task_number", "3", "tasks") &&
                                task && lGetNumberOfElem(tasks) == n;
      const Element<JAT> last{lLastRW(tasks)};
      const bool typed_set = throws_duplicate([&] { last.set<JAT_task_number>(3u); }, "JAT_task_number", "3", "tasks") &&
                             last.get<JAT_task_number>() == n;
      CHECK(check++, (std::string{"a typed append and a typed set of a duplicate number throw, "} + size).c_str(),
            typed_append && typed_set);

      const bool old_number = aborts([&] {
         lListElem *ep = lCreateElem(JAT_Type);
         lSetUlong(ep, JAT_task_number, 3);
         lAppendElem(tasks, ep);
      }) && aborts([&] { lSetUlong(lLastRW(tasks), JAT_task_number, 3); }) &&
                              aborts([&] { lAddUlong(lLastRW(tasks), JAT_task_number, 3 - n); });
      lFreeList(&tasks);

      lList *entries = make_entries(n);
      const bool typed_string =
         throws_duplicate([&] { Element<UE>{lLastRW(entries)}.set<UE_name>("u2"); }, "UE_name", "u2", "entries");
      const bool old_string = aborts([&] { lAddElemStr(&entries, UE_name, "u2", UE_Type); }) &&
                              aborts([&] { lSetString(lLastRW(entries), UE_name, "u2"); });
      lFreeList(&entries);

      // host names are compared as the table compares them: a name in upper case is the same host
      lList *hosts = make_hosts(n);
      const bool typed_host = throws_duplicate([&] { Element<EH>{lLastRW(hosts)}.set<EH_name>(Host{"HOST-2"}); },
                                               "EH_name", "HOST-2", "hosts");
      const bool old_host = aborts([&] { lAddElemHost(&hosts, EH_name, "HOST-2", EH_Type); }) &&
                            aborts([&] { lSetHost(lLastRW(hosts), EH_name, "HOST-2"); });
      lFreeList(&hosts);

      CHECK(check++, (std::string{"a typed set of a duplicate string and host name throws, "} + size).c_str(),
            typed_string && typed_host);
      CHECK(check++, (std::string{"the old interface aborts on a duplicate number, string and host name, "} + size).c_str(),
            old_number && old_string && old_host);
   }
#else
   for (int check = 1; check <= 6; check++) {
      CHECK(check, "the checks at the append and the set exist in debug builds only; this is a release build", true);
   }
#endif
}

/**
 * @brief HDL-07 step 4: an append or set of a duplicate drops the published table, in every build
 *
 * In a debug build the checks are suspended, so that the release behaviour
 * is what runs.
 *
 * @return nothing; CHECK records the results
 */
static void
test_dropped_table() {
   const UniqueCheckSuspension suspension;

   lList *tasks = make_tasks(LONG_LIST);
   static_cast<void>(lGetElemUlong(tasks, JAT_task_number, 1));
   const bool built = has_table(tasks, JAT_task_number);
   lListElem *ep = lCreateElem(JAT_Type);
   lSetUlong(ep, JAT_task_number, 3);
   lAppendElem(tasks, ep);
   const bool dropped = !has_table(tasks, JAT_task_number);
   static_cast<void>(lGetElemUlong(tasks, JAT_task_number, 3));
   const bool rebuilt = has_table(tasks, JAT_task_number);
   CHECK(7, "an append of a duplicate number does not throw, drops the table, and the next lookup builds it again",
         built && dropped && rebuilt && lGetNumberOfElem(tasks) == LONG_LIST + 1);
   lFreeList(&tasks);

   lList *entries = make_entries(LONG_LIST);
   static_cast<void>(lGetElemStr(entries, UE_name, "u1"));
   const bool string_built = has_table(entries, UE_name);
   lSetString(lLastRW(entries), UE_name, "u2");
   const bool string_dropped = !has_table(entries, UE_name);
   CHECK(8, "a set of a duplicate string drops the table", string_built && string_dropped);
   lFreeList(&entries);

   lList *unique = make_tasks(LONG_LIST);
   static_cast<void>(lGetElemUlong(unique, JAT_task_number, 1));
   lListElem *renamed = lLastRW(unique);
   lSetUlong(renamed, JAT_task_number, 1000);
   lListElem *more = lCreateElem(JAT_Type);
   lSetUlong(more, JAT_task_number, 1001);
   lAppendElem(unique, more);
   CHECK(9, "a set and an append of new keys keep the table and are found through it",
         has_table(unique, JAT_task_number) && lGetElemUlong(unique, JAT_task_number, 1000) == renamed &&
         lGetElemUlong(unique, JAT_task_number, 1001) == more &&
         lGetElemUlong(unique, JAT_task_number, LONG_LIST) == nullptr && tasks_consistent(unique, LONG_LIST + 1));
   lFreeList(&unique);
}

/**
 * @brief HDL-08: an operation that throws leaves the list, its elements and its table consistent
 *
 * Freeing twice or not at all shows under the address sanitizer.
 *
 * @return nothing; CHECK records the results
 */
static void
test_valid_after_throw() {
#ifdef ENABLE_DEBUG_CHECKS
   lList *tasks = make_tasks(LONG_LIST);
   static_cast<void>(lGetElemUlong(tasks, JAT_task_number, 1));
   const List<JAT> typed{tasks};

   bool thrown = false;
   try {
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task.get().set<JAT_task_number>(7u);
      typed.append(std::move(task));
   } catch (const FieldException &) {
      thrown = true;
   }
   CHECK(10, "after a throw from an append the list, its elements and its table are consistent",
         thrown && has_table(tasks, JAT_task_number) && tasks_consistent(tasks, LONG_LIST));

   thrown = false;
   try {
      Element<JAT>{lFirstRW(tasks)}.set<JAT_task_number>(7u);
   } catch (const FieldException &) {
      thrown = true;
   }
   CHECK(11, "after a throw from a set the list, its elements and its table are consistent",
         thrown && has_table(tasks, JAT_task_number) && tasks_consistent(tasks, LONG_LIST) &&
         lGetUlong(lFirst(tasks), JAT_task_number) == 1);

   lEnumeration *what = lWhat("%T(%I)", JAT_Type, JAT_task_number);
   lList *reduced = lSelect("reduced", tasks, nullptr, what);
   lFreeWhat(&what);
   thrown = false;
   try {
      Element<JAT>{lFirstRW(reduced)}.set<JAT_status>(1u);
   } catch (const FieldException &e) {
      thrown = e.reason() == FieldException::Reason::AbsentField;
   }
   CHECK(12, "after a throw from a set on an absent field the reduced list is consistent",
         thrown && tasks_consistent(reduced, LONG_LIST));
   lFreeList(&reduced);

   Element<JAT>{lFirstRW(tasks)}.set<JAT_task_number>(500u);
   const bool usable = lGetElemUlong(tasks, JAT_task_number, 500) == lFirst(tasks) &&
                       lGetElemUlong(tasks, JAT_task_number, 1) == nullptr && tasks_consistent(tasks, LONG_LIST);
   CHECK(13, "the list stays usable for a change and a lookup", usable);
   lFreeList(&tasks);
#else
   for (int check = 10; check <= 13; check++) {
      CHECK(check, "the throws come from the debug checks, which exist in debug builds only; this is a release build",
            true);
   }
#endif
}

int main(int argc, char *argv[]) {
   lInit(nmv);
   return ocs::module_test::run(argc, argv, {
      {"UNIQUE-01", "debug checks",
       "a debug build catches a duplicate key in a unique field where it is created",
       {"append a task with a number the list holds, and set a task's number to one the list holds, through a "
        "typed handle and through the old interface",
        "set a user entry's name and a host's name to one the list holds, the host name in upper case",
        "each below and above the hash threshold"},
       "the typed side throws, naming field, key and list, and changes nothing; the old interface aborts",
       {{"HDL-07", "covers step 5"}}, {1, 2, 3, 4, 5, 6}, test_debug_checks},
      {"UNIQUE-02", "dropped table",
       "an append or a set of a duplicate key drops the table it would corrupt",
       {"build the table of a list above the threshold with a lookup, then append a duplicate number",
        "the same with a set of a duplicate string", "set and append new keys"},
       "no throw; the table is gone and the next lookup builds it again; new keys keep the table",
       {{"HDL-07", "covers step 4, without the throw of the lookup, which follows in stage 2"}}, {7, 8, 9},
       test_dropped_table},
      {"UNIQUE-03", "valid after a throw",
       "an operation that throws leaves its list consistent",
       {"in a list above the threshold whose table exists, throw from a typed append and a typed set of a duplicate "
        "number, and from a set on an absent field of a reduced element",
        "check the number of elements and look every element up; then change one and look it up"},
       "every check holds; no leak or double free under the address sanitizer",
       {{"HDL-08", "covers"}}, {10, 11, 12, 13}, test_valid_after_throw},
   });
}
