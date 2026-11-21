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
 * @brief Module test of duplicate keys in unique fields: the debug checks, the dropped table and the logic error of the lookup (CS-3052, HDL-07, HDL-08)
 *
 * A list below the hash threshold is searched linearly, a list above it
 * through its table; every case runs on both. The free forms that replaced
 * the old interface throw where the old functions aborted on a duplicate.
 */

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_Select.h"
#include "tom/detail/ocs_ListP.h"

#include "ocs_module_test.h"
#include "tom/ocs_Types.h"

using namespace ocs::tom;

/** @brief A list below the hash threshold, searched linearly */
static constexpr std::uint32_t SHORT_LIST = 5;

/** @brief A list above the hash threshold, searched through its table */
static constexpr std::uint32_t LONG_LIST = 40;

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
 * @brief A list of tasks numbered 1 to n
 * @param n the number of tasks
 * @return the list
 */
static ListPtr<JAT>
make_tasks(const std::uint32_t n) {
   auto tasks = ListPtr<JAT>::create("tasks");
   for (std::uint32_t i = 1; i <= n; i++) {
      tasks->emplace_back<JAT_task_number>(i);
   }
   return tasks;
}

/**
 * @brief A list of user entries named u1 to un
 * @param n the number of entries
 * @return the list
 */
static ListPtr<UE>
make_entries(const std::uint32_t n) {
   auto entries = ListPtr<UE>::create("entries");
   for (std::uint32_t i = 1; i <= n; i++) {
      entries->emplace_back<UE_name>(("u" + std::to_string(i)).c_str());
   }
   return entries;
}

/**
 * @brief A list of execution hosts named host-1 to host-n, in lower case
 * @param n the number of hosts
 * @return the list
 */
[[maybe_unused]] static ListPtr<EH>
make_hosts(const std::uint32_t n) {
   auto hosts = ListPtr<EH>::create("hosts");
   for (std::uint32_t i = 1; i <= n; i++) {
      hosts->emplace_back<EH_name>(Host{("host-" + std::to_string(i)).c_str()});
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
has_table(const ConstRawList lp, const int nm) {
   return list_hash_table(lp.raw(), field_pos(lp.descr(), nm)) != nullptr;
}

/**
 * @brief Whether every task of a list is found under its number, and the list holds n tasks
 * @param tasks the list
 * @param n the expected number of tasks
 * @return true if the list is consistent
 */
static bool
tasks_consistent(const ConstList<JAT> tasks, const std::uint32_t n) {
   if (tasks.size() != n) {
      return false;
   }
   for (const auto task : tasks) {
      if (tasks.find_first<JAT_task_number>(task.get<JAT_task_number>()).raw() != task.raw()) {
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

      auto tasks = make_tasks(n);
      const List<JAT> typed = tasks.get();
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task.get().set<JAT_task_number>(3u);
      const bool typed_append = throws_duplicate([&] { typed.push_back(std::move(task)); }, "JAT_task_number", "3", "tasks") &&
                                task && typed.size() == n;
      const Element<JAT> last = typed.back();
      const bool typed_set = throws_duplicate([&] { last.set<JAT_task_number>(3u); }, "JAT_task_number", "3", "tasks") &&
                             last.get<JAT_task_number>() == n;
      CHECK(check++, (std::string{"a typed append and a typed set of a duplicate number throw, "} + size).c_str(),
            typed_append && typed_set);

      // the free forms that replaced lAppendElem(), lSetUlong() and lAddUlong(), which aborted here
      ElementPtr<JAT> other = ElementPtr<JAT>::create();
      other.get().set<JAT_task_number>(3u);
      const bool free_number =
         throws_duplicate([&] { ocs::tom::append(typed.raw(), std::move(other)); }, "JAT_task_number", "3", "tasks") &&
         throws_duplicate([&] { ocs::tom::set<JAT_task_number>(last.raw(), 3u); }, "JAT_task_number", "3", "tasks") &&
         throws_duplicate([&] { ocs::tom::add<JAT_task_number>(last.raw(), 3 - n); }, "JAT_task_number", "3",
                          "tasks") &&
         typed.size() == n && last.get<JAT_task_number>() == n;

      auto entries = make_entries(n);
      const bool typed_string =
         throws_duplicate([&] { entries->back().set<UE_name>("u2"); }, "UE_name", "u2", "entries");
      // the free forms that replaced lAddElemStr() and lSetString(); the list reference form needs the raw list
      lList *entries_raw = entries.release();
      const bool free_string =
         throws_duplicate([&] { static_cast<void>(ocs::tom::emplace_back<UE_name>(&entries_raw, "u2")); }, "UE_name",
                          "u2", "entries") &&
         throws_duplicate([&] { ocs::tom::set<UE_name>(RawList{entries_raw}.back().raw(), "u2"); }, "UE_name", "u2",
                          "entries");
      entries = ListPtr<UE>(entries_raw);
      const bool free_string_unchanged = entries->size() == n;

      // host names are compared as the table compares them: a name in upper case is the same host
      auto hosts = make_hosts(n);
      const bool typed_host =
         throws_duplicate([&] { hosts->back().set<EH_name>(Host{"HOST-2"}); }, "EH_name", "HOST-2", "hosts");
      // the free forms that replaced lAddElemHost() and lSetHost()
      lList *hosts_raw = hosts.release();
      const bool free_host =
         throws_duplicate([&] { static_cast<void>(ocs::tom::emplace_back<EH_name>(&hosts_raw, "HOST-2")); },
                          "EH_name", "HOST-2", "hosts") &&
         throws_duplicate([&] { ocs::tom::set<EH_name>(RawList{hosts_raw}.back().raw(), "HOST-2"); }, "EH_name",
                          "HOST-2", "hosts");
      hosts = ListPtr<EH>(hosts_raw);
      const bool free_host_unchanged = hosts->size() == n;

      CHECK(check++, (std::string{"a typed set of a duplicate string and host name throws, "} + size).c_str(),
            typed_string && typed_host);
      CHECK(check++, (std::string{"the free forms that replaced the old interface throw on a duplicate number, "
                                  "string and host name, where the old functions aborted, "} + size).c_str(),
            free_number && free_string && free_string_unchanged && free_host && free_host_unchanged);
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

   auto tasks = make_tasks(LONG_LIST);
   static_cast<void>(tasks->find_first<JAT_task_number>(1u));
   const bool built = has_table(tasks.get(), JAT_task_number);
   auto ep = ElementPtr<JAT>::create();
   ep->set<JAT_task_number>(3u);
   tasks->push_back(std::move(ep));
   const bool dropped = !has_table(tasks.get(), JAT_task_number);
   // stage 2: the next lookup builds the table again, meets the duplicate and throws, publishing nothing
   const bool lookup_throws = throws_duplicate([&] { static_cast<void>(tasks->find_first<JAT_task_number>(3u)); },
                                               "JAT_task_number", "3", "tasks");
   CHECK(7, "an append of a duplicate number does not throw and drops the table; the next lookup throws and publishes "
            "no table", built && dropped && lookup_throws && !has_table(tasks.get(), JAT_task_number) &&
            tasks->size() == LONG_LIST + 1);

   auto entries = make_entries(LONG_LIST);
   static_cast<void>(entries->find_first<UE_name>("u1"));
   const bool string_built = has_table(entries.get(), UE_name);
   entries->back().set<UE_name>("u2");
   const bool string_dropped = !has_table(entries.get(), UE_name);
   CHECK(8, "a set of a duplicate string drops the table", string_built && string_dropped);

   auto unique = make_tasks(LONG_LIST);
   static_cast<void>(unique->find_first<JAT_task_number>(1u));
   const Element<JAT> renamed = unique->back();
   renamed.set<JAT_task_number>(1000u);
   auto more_owner = ElementPtr<JAT>::create();
   more_owner->set<JAT_task_number>(1001u);
   const Element<JAT> more = unique->push_back(std::move(more_owner));
   CHECK(9, "a set and an append of new keys keep the table and are found through it",
         has_table(unique.get(), JAT_task_number) &&
         unique->find_first<JAT_task_number>(1000u).raw() == renamed.raw() &&
         unique->find_first<JAT_task_number>(1001u).raw() == more.raw() &&
         !unique->find_first<JAT_task_number>(LONG_LIST) && tasks_consistent(unique.get(), LONG_LIST + 1));
}

/**
 * @brief A list holding a key twice in a unique field, built with the debug check suspended
 * @param n the number of tasks; task n gets the number 2
 * @return the list, without a table
 */
static ListPtr<JAT>
make_tasks_with_duplicate(const std::uint32_t n) {
   const UniqueCheckSuspension suspension;
   auto tasks = make_tasks(n);
   tasks->back().set<JAT_task_number>(2u);
   return tasks;
}

/**
 * @brief HDL-07 steps 1 to 3: a lookup on a unique field that holds a key twice throws
 *
 * Above the hash threshold the lookup builds the table, meets the second
 * element and publishes nothing, in every build; below it a debug build's
 * linear search goes on to the end of the list, while a release build stops at
 * the first match and finds the first of the two (CS-3052). The free lookup on
 * a raw list, which replaced lGetElemUlong(), throws where the core throws, as
 * the old lookup aborted there.
 *
 * @return nothing; CHECK records the results
 */
static void
test_lookup() {
   int check = 14;
#ifdef ENABLE_DEBUG_CHECKS
   constexpr bool linear_finds_duplicate = true;
#else
   constexpr bool linear_finds_duplicate = false;   // a release build stops a linear lookup at the first match
#endif
   for (const std::uint32_t n : {LONG_LIST, SHORT_LIST}) {
      const char *size = n == SHORT_LIST ? "below the hash threshold" : "above the hash threshold";
      const bool finds_duplicate = n == LONG_LIST || linear_finds_duplicate;
      auto tasks = make_tasks_with_duplicate(n);
      const List<JAT> typed = tasks.get();

      // several threads look up at the same time, twice each; every lookup throws and none publishes a table
      std::vector<int> threw(4, 0);
      std::vector<std::thread> threads;
      for (std::size_t t = 0; t < threw.size(); t++) {
         threads.emplace_back([&, t] {
            // the field names of the message come from the object model, which is process-wide
            ocs::tom::install_model(ocs::tom::gcs_model());
            for (int i = 0; i < 2; i++) {
               threw[t] += throws_duplicate([&] { static_cast<void>(typed.find_first<JAT_task_number>(2u)); },
                                            "JAT_task_number", "2", "tasks") ? 1 : 0;
            }
         });
      }
      for (auto &thread : threads) {
         thread.join();
      }
      bool all = true;
      for (const int t : threw) {
         all = all && t == (finds_duplicate ? 2 : 0);
      }
      CHECK(check++, (std::string{finds_duplicate ? "every lookup of the repeated key throws, from four threads, and "
                                                    "no table is published, "
                                                  : "no lookup of the repeated key throws in a release build, from "
                                                    "four threads, and no table is published, "} + size).c_str(),
            all && !has_table(typed, JAT_task_number));

      // the free lookup on the raw list: throws where the old lookup aborted, else finds the first of the two
      const bool free_lookup =
         finds_duplicate
            ? throws_duplicate([&] { static_cast<void>(ocs::tom::find_first<JAT_task_number>(typed.raw(), 2u)); },
                               "JAT_task_number", "2", "tasks")
            : static_cast<const lListElem *>(ocs::tom::find_first<JAT_task_number>(typed.raw(), 2u)) ==
                 typed.front().next().raw();
      CHECK(check++, (std::string{finds_duplicate ? "the free lookup throws, " : "the free lookup finds the first of "
                                                                                 "the two, "} + size).c_str(),
            free_lookup);

      // above the threshold every lookup builds the table and throws; below it only the repeated key is found twice
      const bool other = n == LONG_LIST
                            ? throws_duplicate([&] { static_cast<void>(typed.find_first<JAT_task_number>(1u)); },
                                               "JAT_task_number", "2", "tasks")
                            : typed.find_first<JAT_task_number>(1u).raw() == typed.front().raw();
      CHECK(check++, (std::string{"a lookup of another key "} +
                      (n == LONG_LIST ? "throws as well, the table cannot be built, " : "finds its element, ") + size)
                        .c_str(), other);
   }

   // a field hashed without CULL_UNIQUE holds a key several times by design: found first, then the next
   auto changes = ListPtr<CX>::create("changes");
   for (std::uint32_t i = 0; i < LONG_LIST; i++) {
      changes->emplace_back<CX_variable>(i % 2 == 0 ? "a" : "b");
   }
   const void *iterator = nullptr;
   const lListElem *first = ocs::tom::find_first<CX_variable>(changes->raw(), "a", &iterator);
   const lListElem *second = ocs::tom::find_next<CX_variable>(changes->raw(), "a", &iterator);
   CHECK(20, "a non unique hashed field holding a key many times is looked up and iterated as before",
         first == changes->front().raw() && second != nullptr && second != first &&
         std::strcmp(ConstElement<CX>{second}.get<CX_variable>(), "a") == 0);
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
   auto tasks = make_tasks(LONG_LIST);
   const List<JAT> typed = tasks.get();
   static_cast<void>(typed.find_first<JAT_task_number>(1u));

   bool thrown = false;
   try {
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task.get().set<JAT_task_number>(7u);
      typed.push_back(std::move(task));
   } catch (const FieldException &) {
      thrown = true;
   }
   CHECK(10, "after a throw from an append the list, its elements and its table are consistent",
         thrown && has_table(typed, JAT_task_number) && tasks_consistent(typed, LONG_LIST));

   thrown = false;
   try {
      typed.front().set<JAT_task_number>(7u);
   } catch (const FieldException &) {
      thrown = true;
   }
   CHECK(11, "after a throw from a set the list, its elements and its table are consistent",
         thrown && has_table(typed, JAT_task_number) && tasks_consistent(typed, LONG_LIST) &&
         typed.front().get<JAT_task_number>() == 1);

   auto reduced = typed.select(what<JAT_task_number>(), "reduced");
   thrown = false;
   try {
      reduced->front().set<JAT_status>(1u);
   } catch (const FieldException &e) {
      thrown = e.reason() == FieldException::Reason::AbsentField;
   }
   CHECK(12, "after a throw from a set on an absent field the reduced list is consistent",
         thrown && tasks_consistent(reduced.get(), LONG_LIST));

   typed.front().set<JAT_task_number>(500u);
   const bool usable = typed.find_first<JAT_task_number>(500u).raw() == typed.front().raw() &&
                       !typed.find_first<JAT_task_number>(1u) && tasks_consistent(typed, LONG_LIST);
   CHECK(13, "the list stays usable for a change and a lookup", usable);
#else
   for (int check = 10; check <= 13; check++) {
      CHECK(check, "the throws come from the debug checks, which exist in debug builds only; this is a release build",
            true);
   }
#endif
}

int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"UNIQUE-01", "debug checks",
       "a debug build catches a duplicate key in a unique field where it is created",
       {"append a task with a number the list holds, and set a task's number to one the list holds, through a "
        "typed handle and through the free forms that replaced the old interface",
        "set a user entry's name and a host's name to one the list holds, the host name in upper case",
        "each below and above the hash threshold"},
       "both throw, naming field, key and list, and change nothing (the old interface aborted)",
       {{"HDL-07", "covers step 5"}}, {1, 2, 3, 4, 5, 6}, test_debug_checks},
      {"UNIQUE-02", "dropped table",
       "an append or a set of a duplicate key drops the table it would corrupt",
       {"build the table of a list above the threshold with a lookup, then append a duplicate number",
        "the same with a set of a duplicate string", "set and append new keys"},
       "no throw; the table is gone, the next lookup throws and publishes none; new keys keep the table",
       {{"HDL-07", "covers step 4"}}, {7, 8, 9},
       test_dropped_table},
      {"UNIQUE-03", "valid after a throw",
       "an operation that throws leaves its list consistent",
       {"in a list above the threshold whose table exists, throw from a typed append and a typed set of a duplicate "
        "number, and from a set on an absent field of a reduced element",
        "check the number of elements and look every element up; then change one and look it up"},
       "every check holds; no leak or double free under the address sanitizer",
       {{"HDL-08", "covers"}}, {10, 11, 12, 13}, test_valid_after_throw},
      {"UNIQUE-04", "lookup",
       "a lookup on a unique field that holds a key twice is a logic error in every build",
       {"fill a list above and one below the hash threshold with a task number twice, without a lookup",
        "look the number up through the typed list, twice each from four threads",
        "look it up through the free lookup on the raw list",
        "look up another number",
        "look up and iterate a key held many times in a non unique hashed field"},
       "the typed and the free lookups throw naming field, key and list, and no table is published; above "
       "the threshold every lookup throws, below it only the repeated key; the non unique field is unaffected",
       {{"HDL-07", "covers steps 1 to 3"}}, {14, 15, 16, 17, 18, 19, 20}, test_lookup},
   });
}
