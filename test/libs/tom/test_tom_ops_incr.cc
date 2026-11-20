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
 * @brief Increments through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how a number field is increased with the
 * typed object model. Every scenario does the same thing twice: a block marked
 * CULL uses only the old functions of the list library, a block marked TOM
 * only the typed handles. Each block builds its own data and reduces what came
 * out to plain values (numbers, flags), and the scenario compares those. Each
 * step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface has one function per field type, lAddUlong(),
 * lAddUlong64() and lAddDouble(), each returning a status. The typed form is
 * one template, add<F>(offset), whose offset type follows from the field and
 * which returns the new value, as operator+= does. Both keep the hash table of
 * the list right when the field is a hashed key, and both leave a field alone
 * when the offset is 0.
 *
 * For the mechanical rewrite there is also a free form, ocs::tom::add<F>(ep,
 * offset), which takes a raw element and keeps the old error behaviour; its
 * scenario is a bridge between the two interfaces by design.
 *
 * In a Release build the last scenario times increments on a plain and on a
 * hashed field with both forms and prints the ratios.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JAT;
using ocs::tom::ListPtr;

namespace {

   /** @brief The list lengths the scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /**
    * @brief Whether a function throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   template <typename Call>
   bool throws(Call call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /**
    * @brief The values of one array task after the increments, as plain numbers
    */
   struct Values {
      std::uint32_t status = 0;   ///< JAT_status, a 32 bit field
      std::uint64_t start = 0;    ///< JAT_start_time, a 64 bit field
      double tickets = 0.0;       ///< JAT_tix, a double field

      /**
       * @brief Whether two results are the same
       * @param other the other result
       * @return true if every value is equal
       */
      bool operator==(const Values &other) const = default;
   };

   /**
    * @brief What a block of the hashed-key scenario finds after renumbering one task
    */
   struct Renumbered {
      bool old_key_found = true;   ///< whether the lookup by the old task number still finds an element
      bool new_key_found = false;  ///< whether the lookup by the new task number finds the renumbered task
      std::size_t size = 0;        ///< the number of tasks

      /**
       * @brief Whether two results are the same
       * @param other the other result
       * @return true if every value is equal
       */
      bool operator==(const Renumbered &other) const = default;
   };

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Add to a 32 bit, a 64 bit and a double field
 */
void test_add() {
   // old:     lAddUlong(ep, JAT_status, 2); lAddUlong64(ep, JAT_start_time, 1000); lAddDouble(ep, JAT_tix, 0.5)
   // typed:   task.add<JAT_status>(2); task.add<JAT_start_time>(1000); task.add<JAT_tix>(0.5)
   // differs: one template for every number type; the offset type follows from the field

   // --- CULL
   Values old_values;
   {
      lListElem *ep = lCreateElem(JAT_Type);
      lSetUlong(ep, JAT_status, 1);
      lSetUlong64(ep, JAT_start_time, 100);
      lSetDouble(ep, JAT_tix, 1.25);
      lAddUlong(ep, JAT_status, 2);
      lAddUlong64(ep, JAT_start_time, 1000);
      lAddDouble(ep, JAT_tix, 0.5);
      // an offset of 0 changes nothing
      lAddUlong(ep, JAT_status, 0);
      old_values = {lGetUlong(ep, JAT_status), lGetUlong64(ep, JAT_start_time), lGetDouble(ep, JAT_tix)};
      lFreeElem(&ep);
   }

   // --- TOM
   Values typed_values;
   {
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task->set<JAT_status>(1);
      task->set<JAT_start_time>(100);
      task->set<JAT_tix>(1.25);
      task->add<JAT_status>(2);
      task->add<JAT_start_time>(1000);
      task->add<JAT_tix>(0.5);
      task->add<JAT_status>(0);
      typed_values = {task->get<JAT_status>(), task->get<JAT_start_time>(), task->get<JAT_tix>()};
   }

   const Values expected{3, 1100, 1.75};
   CHECK(1, "add adds to a 32 bit, a 64 bit and a double field as lAddUlong, lAddUlong64 and lAddDouble do",
         old_values == expected && typed_values == expected);
}

/**
 * @brief Add to a hashed key field of an element in a list
 */
void test_hashed_key() {
   // old:     lAddUlong(task, JAT_task_number, 100) on a task in a list
   // typed:   task.add<JAT_task_number>(100)
   // differs: nothing; both take the element out of the hash table under the old key and put it in under the new
   bool ok = true;
   for (const int n : Lengths) {
      // --- CULL
      Renumbered old_result;
      {
         lList *tasks = lCreateList("tasks", JAT_Type);
         for (int i = 1; i <= n; i++) {
            lListElem *ep = lCreateElem(JAT_Type);
            lSetUlong(ep, JAT_task_number, i);
            lAppendElem(tasks, ep);
         }
         lListElem *task = lGetElemUlongRW(tasks, JAT_task_number, 3);
         lAddUlong(task, JAT_task_number, 100);
         old_result = {lGetElemUlong(tasks, JAT_task_number, 3) != nullptr,
                       lGetElemUlong(tasks, JAT_task_number, 103) == task,
                       static_cast<std::size_t>(lGetNumberOfElem(tasks))};
         lFreeList(&tasks);
      }

      // --- TOM
      Renumbered typed_result;
      {
         ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
         for (int i = 1; i <= n; i++) {
            tasks->push_back(ElementPtr<JAT>::create()).set<JAT_task_number>(i);
         }
         const Element<JAT> task = tasks->find_first<JAT_task_number>(3);
         task.add<JAT_task_number>(100);
         typed_result = {static_cast<bool>(tasks->find_first<JAT_task_number>(3)),
                         tasks->find_first<JAT_task_number>(103).get<JAT_task_number>() == 103,
                         tasks->size()};
      }

      const Renumbered expected{false, true, static_cast<std::size_t>(n)};
      ok = ok && old_result == expected && typed_result == expected;
   }
   CHECK(2, "add on a hashed key moves the element to its new key, below and above the hash threshold", ok);
}

/**
 * @brief The result of an increment
 */
void test_result() {
   // old:     lAddUlong(ep, JAT_status, 5) returns 0, the new value is read afterwards
   // typed:   task.add<JAT_status>(5) returns the new value
   // differs: the old functions return a status that no caller reads; the typed one returns the sum, as
   //          operator+= does, so `if (task.add<JAT_status>(1) > limit)` needs no second read

   // --- CULL
   std::vector<double> old_seen;
   {
      lListElem *ep = lCreateElem(JAT_Type);
      lSetUlong(ep, JAT_status, 0xFFFFFFFEU);
      old_seen.push_back(lAddUlong(ep, JAT_status, 5));   // the status
      old_seen.push_back(lGetUlong(ep, JAT_status));      // the 32 bit sum wrapped around
      old_seen.push_back(lAddDouble(ep, JAT_tix, 2.5));
      old_seen.push_back(lGetDouble(ep, JAT_tix));
      lFreeElem(&ep);
   }

   // --- TOM
   std::vector<double> typed_seen;
   {
      ElementPtr<JAT> task = ElementPtr<JAT>::create();
      task->set<JAT_status>(0xFFFFFFFEU);
      typed_seen.push_back(task->add<JAT_status>(5));   // the new value, wrapped around
      typed_seen.push_back(task->add<JAT_tix>(2.5));
   }

   CHECK(3, "the old form returns 0 and the sum is read afterwards, the typed form returns the sum",
         (old_seen == std::vector<double>{0, 3, 0, 2.5}) && (typed_seen == std::vector<double>{3, 2.5}));
}

/**
 * @brief The free form for the rewrite against the old functions, on raw elements
 */
void test_free_form() {
   // old:     lAddUlong(ep, nm, 1), lAddDouble(ep, nm, 1) on a null element and on a full element
   // typed:   ocs::tom::add<JAT_status>(ep, 1), ocs::tom::add<JAT_tix>(ep, 1.0)
   // differs: nothing, by design: the free form replaces the old call in the mechanical rewrite of M3 and
   //          keeps its return values and error numbers. On a field the element does not hold the old form
   //          aborts and the typed one throws (N-L-4, CS-3062); test_tom_legacy checks the abort, this test
   //          the throw. It takes the raw element, so this scenario is a bridge
   //          between the interfaces: both blocks work on elements created by the list library.
   const auto run = [](auto add_status, auto add_tix) {
      std::vector<int> seen;
      lList *tasks = lCreateList("tasks", JAT_Type);
      lAppendElem(tasks, lCreateElem(JAT_Type));
      lListElem *full = lFirstRW(tasks);
      for (lListElem *ep : {static_cast<lListElem *>(nullptr), full}) {
         cull_state_set_lerrno(0);
         seen.push_back(add_status(ep));
         seen.push_back(cull_state_get_lerrno());
         cull_state_set_lerrno(0);
         seen.push_back(add_tix(ep));
         seen.push_back(cull_state_get_lerrno());
      }
      seen.push_back(static_cast<int>(lGetUlong(full, JAT_status)));
      lFreeList(&tasks);
      return seen;
   };

   // --- CULL
   const std::vector<int> old_seen = run([](lListElem *ep) { return lAddUlong(ep, JAT_status, 1); },
                                         [](lListElem *ep) { return lAddDouble(ep, JAT_tix, 1.0); });
   // --- TOM (free form)
   const std::vector<int> typed_seen = run([](lListElem *ep) { return ocs::tom::add<JAT_status>(ep, 1); },
                                           [](lListElem *ep) { return ocs::tom::add<JAT_tix>(ep, 1.0); });

   // null element: -1 with LEELEMNULL; a full element: 0
   const std::vector<int> expected{-1, LEELEMNULL, -1, LEELEMNULL, 0, 0, 0, 0, 1};

   // a field the element does not hold: the free form throws AbsentField, where lAddUlong and lAddDouble abort
   lList *tasks = lCreateList("tasks", JAT_Type);
   lAppendElem(tasks, lCreateElem(JAT_Type));
   lEnumeration *what = lWhat("%T(%I)", JAT_Type, JAT_task_number);
   lList *reduced = lSelect("reduced", tasks, nullptr, what);
   int absent_throws = 0;
   for (int i = 0; i < 2; i++) {
      try {
         static_cast<void>(i == 0 ? ocs::tom::add<JAT_status>(lFirstRW(reduced), 1)
                                  : ocs::tom::add<JAT_tix>(lFirstRW(reduced), 1.0));
      } catch (const ocs::tom::FieldException &e) {
         absent_throws += e.reason() == ocs::tom::FieldException::Reason::AbsentField ? 1 : 0;
      }
   }
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&tasks);
   CHECK(4, "ocs::tom::add<F>(ep, offset) returns and sets the error numbers as lAddUlong and lAddDouble do, and "
            "throws on an absent field",
         old_seen == expected && typed_seen == expected && absent_throws == 2);
}

/**
 * @brief The errors of the typed form
 */
void test_errors() {
   // old:     lAddUlong(nullptr, ...) returns -1; a duplicate in a unique field aborts in a debug build
   // typed:   add<F>() on an empty handle throws EmptyHandle; a duplicate throws DuplicateKey
   // differs: the typed form throws where the old one returned -1 or aborted; only the typed side is run,
   //          since the old abort would end the test
   // --- TOM
   const Element<JAT> empty;
   const bool empty_handle =
      throws([&] { static_cast<void>(empty.add<JAT_status>(1)); }, FieldException::Reason::EmptyHandle);
   CHECK(5, "add on an empty handle throws EmptyHandle", empty_handle);

#ifdef ENABLE_DEBUG_CHECKS
   ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
   for (int i = 1; i <= 3; i++) {
      tasks->push_back(ElementPtr<JAT>::create()).set<JAT_task_number>(i);
   }
   const Element<JAT> first = tasks->find_first<JAT_task_number>(1);
   const bool duplicate =
      throws([&] { static_cast<void>(first.add<JAT_task_number>(1)); }, FieldException::Reason::DuplicateKey) &&
      first.get<JAT_task_number>() == 1 && tasks->find_first<JAT_task_number>(1);
   CHECK(6, "a duplicate in a unique field throws DuplicateKey in a debug build, the old value kept", duplicate);
#else
   CHECK(6, "a duplicate key is checked in a debug build only", true);
#endif
}

/**
 * @brief In a Release build, time increments on a plain and on a hashed field with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 1000;
   constexpr int rounds = 1000;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };

   // --- CULL
   lList *old_tasks = lCreateList("tasks", JAT_Type);
   for (int i = 1; i <= n; i++) {
      lListElem *ep = lCreateElem(JAT_Type);
      lSetUlong(ep, JAT_task_number, i);
      lAppendElem(old_tasks, ep);
   }
   // build the hash table, as a lookup does
   static_cast<void>(lGetElemUlong(old_tasks, JAT_task_number, 1));
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      for (lListElem *ep = lFirstRW(old_tasks); ep != nullptr; ep = lNextRW(ep)) {
         lAddDouble(ep, JAT_tix, 1.0);
      }
   }
   auto t1 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      // renumber every task upwards, last first, so that no key is held twice
      for (lListElem *ep = lLastRW(old_tasks); ep != nullptr; ep = lPrevRW(ep)) {
         lAddUlong(ep, JAT_task_number, n);
      }
   }
   auto t2 = std::chrono::steady_clock::now();
   const double old_sum = lGetDouble(lFirst(old_tasks), JAT_tix);
   lFreeList(&old_tasks);

   // --- TOM
   ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
   for (int i = 1; i <= n; i++) {
      tasks->push_back(ElementPtr<JAT>::create()).set<JAT_task_number>(i);
   }
   static_cast<void>(tasks->find_first<JAT_task_number>(1));
   auto t3 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      for (const Element<JAT> task : *tasks) {
         task.add<JAT_tix>(1.0);
      }
   }
   auto t4 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      for (Element<JAT> task = tasks->back(); task; task = task.prev()) {
         task.add<JAT_task_number>(n);
      }
   }
   auto t5 = std::chrono::steady_clock::now();
   const double typed_sum = tasks->front().get<JAT_tix>();

   printf("timing, %d rounds of increments on %d array tasks:\n", rounds, n);
   printf("   plain double: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t3, t4),
          ms(t3, t4) / ms(t0, t1));
   printf("   hashed key:   old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t1, t2), ms(t4, t5),
          ms(t4, t5) / ms(t1, t2));
   CHECK(7, "the timing loops ran with the same sums", old_sum == typed_sum && old_sum == rounds);
#endif
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
      {"OPS-INCR-01", "add", "add adds to a number field as lAddUlong, lAddUlong64 and lAddDouble do",
       {"add to a 32 bit, a 64 bit and a double field of an array task, and add 0"}, "the same sums",
       {{"OPS-01", "covers"}}, {1}, test_add},
      {"OPS-INCR-02", "hashed key", "add on a hashed key moves the element to its new key",
       {"renumber one task of a list of 5 and of 40 tasks"}, "found by the new number, not by the old one",
       {{"OPS-01", "covers"}}, {2}, test_hashed_key},
      {"OPS-INCR-03", "result", "the typed form returns the new value, the old one a status",
       {"add to a 32 bit field past its maximum and to a double field"}, "0 and the sum read afterwards; the sum",
       {{"OPS-01", "covers"}}, {3}, test_result},
      {"OPS-INCR-04", "free form", "the free form for the rewrite keeps the old error behaviour",
       {"add with both forms to a null element, a reduced element without the field and a full element"},
       "the same returns and error numbers; AbsentField thrown where the old form aborts", {{"OPS-01", "covers"}},
       {4}, test_free_form},
      {"OPS-INCR-05", "errors", "the typed form throws on an empty handle and on a duplicate key",
       {"add to an empty handle; renumber a task to a number another task holds"},
       "EmptyHandle and, in a debug build, DuplicateKey thrown", {{"OPS-01", "covers"}}, {5, 6}, test_errors},
      {"OPS-INCR-06", "timing", "a Release build times both forms",
       {"add 1 000 times to a double field and to the hashed task number of 1 000 array tasks, with both forms"},
       "ratios printed", {{"OPS-01", "covers"}}, {7}, test_timing},
   });
}
