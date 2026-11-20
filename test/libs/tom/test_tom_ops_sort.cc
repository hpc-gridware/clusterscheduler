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
 * @brief Sorting through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how lists are sorted with the typed object
 * model. Every scenario does the same thing twice: a block marked CULL uses
 * only the old functions of the list library, a block marked TOM only the
 * typed handles. Each block builds its own data and reduces what came out to
 * plain values (the job numbers in list order), and the scenario compares
 * those. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface parses a format string at run time: lPSortList(lp,
 * "%I-%I+", JB_priority, JB_job_number), with the fields passed through
 * varargs unchecked. The typed form names the fields and their directions as
 * template arguments: jobs.sort<desc<JB_priority>, asc<JB_job_number>>(). A
 * field of another type does not compile (the compile_fail tests show it).
 * Both sort stably: elements with equal keys keep their order.
 *
 * In a Release build the last scenario times sorting and inserting with both
 * forms and prints the ratios.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "cull/cull_sort.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JB;
using ocs::tom::ListPtr;
using ocs::tom::SortOrder;
using ocs::tom::asc;
using ocs::tom::desc;
using ocs::tom::sort_order;

namespace {

   /** @brief The number of jobs most scenarios sort */
   constexpr std::uint32_t Jobs = 200;

   /**
    * @brief The priority of a job: five priorities, so that many jobs share one
    * @param number the job number
    * @return the priority
    */
   std::uint32_t priority_of(const std::uint32_t number) {
      return (number * 7) % 5;
   }

   /**
    * @brief The job numbers 1 .. n in a scrambled order, the same in both blocks
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
    * @brief The job numbers of an old list, in list order
    * @param lp the list
    * @return the numbers
    */
   std::vector<std::uint32_t> numbers_of(const lList *lp) {
      std::vector<std::uint32_t> numbers;
      for_each_ep_lv(ep, lp) {
         numbers.push_back(lGetUlong(ep, JB_job_number));
      }
      return numbers;
   }

   /**
    * @brief The job numbers of a typed list, in list order
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
    * @brief Build the jobs as a list of the old interface, in scrambled order
    * @param n the number of jobs
    * @return the list
    */
   lList *old_jobs(const std::uint32_t n) {
      lList *lp = lCreateList("jobs", JB_Type);
      for (const std::uint32_t number : scrambled(n)) {
         lListElem *ep = lAddElemUlong(&lp, JB_job_number, number, JB_Type);
         lSetUlong(ep, JB_priority, priority_of(number));
      }
      return lp;
   }

   /**
    * @brief Build the jobs as a typed list, in scrambled order
    * @param n the number of jobs
    * @return the list
    */
   ListPtr<JB> typed_jobs(const std::uint32_t n) {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      for (const std::uint32_t number : scrambled(n)) {
         jobs->emplace_back<JB_job_number>(number).set<JB_priority>(priority_of(number));
      }
      return jobs;
   }

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

} // namespace

/**
 * @brief Sort by two fields: the highest priority first, jobs of one priority by their number
 */
void test_sort() {
   // --- CULL
   lList *lp = old_jobs(Jobs);
   // old:     lPSortList(lp, "%I-%I+", JB_priority, JB_job_number)
   lPSortList(lp, "%I-%I+", JB_priority, JB_job_number);
   const std::vector<std::uint32_t> old_order = numbers_of(lp);
   lFreeList(&lp);

   // --- TOM
   ListPtr<JB> jobs = typed_jobs(Jobs);
   // typed:   jobs->sort<desc<JB_priority>, asc<JB_job_number>>()
   // differs: the fields and directions are template arguments; a field of another type does not compile
   jobs->sort<desc<JB_priority>, asc<JB_job_number>>();
   const std::vector<std::uint32_t> typed_order = numbers_of(jobs);

   const bool first_is_highest = !typed_order.empty() && priority_of(typed_order.front()) == 4 &&
                                 priority_of(typed_order.back()) == 0;
   CHECK(1, "sort<desc<F>, asc<G>>() sorts as lPSortList with \"%I-%I+\" sorts",
         old_order == typed_order && old_order.size() == Jobs && first_is_highest);
}

/**
 * @brief Keep a list sorted while inserting, with a sort order built once
 */
void test_insert_sorted() {
   // --- CULL
   // old:     lParseSortOrderVarArg(JB_Type, "%I-%I+", JB_priority, JB_job_number), lInsertSorted(), lFreeSortOrder()
   lSortOrder *so = lParseSortOrderVarArg(JB_Type, "%I-%I+", JB_priority, JB_job_number);
   lList *lp = lCreateList("jobs", JB_Type);
   for (const std::uint32_t number : scrambled(Jobs)) {
      lListElem *ep = lCreateElem(JB_Type);
      lSetUlong(ep, JB_job_number, number);
      lSetUlong(ep, JB_priority, priority_of(number));
      lInsertSorted(so, ep, lp);
   }
   const std::vector<std::uint32_t> old_order = numbers_of(lp);
   lFreeList(&lp);
   lFreeSortOrder(&so);

   // --- TOM
   // typed:   sort_order<desc<JB_priority>, asc<JB_job_number>>(), jobs->insert_sorted(order, std::move(job))
   // differs: the order is an owning handle and frees itself; insert_sorted() takes the element over
   const SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
   ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   for (const std::uint32_t number : scrambled(Jobs)) {
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(number);
      job->set<JB_priority>(priority_of(number));
      jobs->insert_sorted(order, std::move(job));
   }
   const std::vector<std::uint32_t> typed_order = numbers_of(jobs);

   CHECK(2, "insert_sorted() keeps a list in the order lInsertSorted keeps it in",
         old_order == typed_order && old_order.size() == Jobs);
}

/**
 * @brief Move a job to its new place after its priority changed
 */
void test_resort() {
   constexpr std::uint32_t moved = 42;

   // --- CULL
   lSortOrder *so = lParseSortOrderVarArg(JB_Type, "%I-%I+", JB_priority, JB_job_number);
   lList *lp = old_jobs(Jobs);
   lSortList(lp, so);
   lListElem *ep = lGetElemUlongRW(lp, JB_job_number, moved);
   lSetUlong(ep, JB_priority, 9);
   // old:     lResortElem(so, ep, lp)
   lResortElem(so, ep, lp);
   const std::vector<std::uint32_t> old_order = numbers_of(lp);
   lFreeList(&lp);
   lFreeSortOrder(&so);

   // --- TOM
   const SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
   ListPtr<JB> jobs = typed_jobs(Jobs);
   jobs->sort(order);
   Element<JB> job = jobs->find_first<JB_job_number>(moved);
   job.set<JB_priority>(9);
   // typed:   jobs->resort(order, job)
   // differs: an element of another list throws ForeignList instead of being dechained from a wrong list
   jobs->resort(order, job);
   const std::vector<std::uint32_t> typed_order = numbers_of(jobs);

   CHECK(3, "resort() moves a changed job where lResortElem moves it",
         old_order == typed_order && !typed_order.empty() && typed_order.front() == moved);
}

/**
 * @brief Sort by a key the fields do not express: the last digit of the job number, then the number
 *
 * The old interface sorts only by fields, so there the bridge is to compute
 * the expected order from plain values.
 */
void test_sort_with_comparison() {
   // --- bridge: the expected order, from plain values
   std::vector<std::uint32_t> expected = scrambled(Jobs);
   std::sort(expected.begin(), expected.end(), [](const std::uint32_t a, const std::uint32_t b) {
      return a % 10 != b % 10 ? a % 10 < b % 10 : a < b;
   });

   // --- TOM
   ListPtr<JB> jobs = typed_jobs(Jobs);
   // typed:   jobs->sort([](ConstElement<JB> a, ConstElement<JB> b) { ... })
   // differs: no old form; lSortList() sorts by fields only
   jobs->sort([](const ConstElement<JB> a, const ConstElement<JB> b) {
      const std::uint32_t x = a.get<JB_job_number>();
      const std::uint32_t y = b.get<JB_job_number>();
      return x % 10 != y % 10 ? x % 10 < y % 10 : x < y;
   });

   CHECK(4, "sort(comparison) sorts by a computed key", numbers_of(jobs) == expected);
}

/**
 * @brief Equal keys keep their order: sorted by priority only, the jobs of one priority stay scrambled
 */
void test_stable() {
   std::vector<std::uint32_t> expected = scrambled(Jobs);
   std::stable_sort(expected.begin(), expected.end(), [](const std::uint32_t a, const std::uint32_t b) {
      return priority_of(a) < priority_of(b);
   });

   // --- CULL
   lList *lp = old_jobs(Jobs);
   // old:     lPSortList(lp, "%I+", JB_priority)
   // differs: stable since the sort moved into the core; qsort() made no promise
   lPSortList(lp, "%I+", JB_priority);
   const std::vector<std::uint32_t> old_order = numbers_of(lp);
   lFreeList(&lp);

   // --- TOM
   ListPtr<JB> jobs = typed_jobs(Jobs);
   jobs->sort<asc<JB_priority>>();

   CHECK(5, "both forms sort stably: jobs of one priority keep their order",
         numbers_of(jobs) == expected && old_order == expected);
}

/**
 * @brief The typed form throws where the old one returned -1 or would have read a missing field
 */
void test_errors() {
   // --- CULL
   lList *lp = lCreateList("jobs", JB_Type);
   // old:     lInsertSorted() with a null order returns -1
   lListElem *job = lCreateElem(JB_Type);
   const bool old_refused = lInsertSorted(nullptr, job, lp) == -1;
   lFreeElem(&job);
   lFreeList(&lp);

   // --- TOM
   // typed:   an empty order, an element of another list, a list whose elements lack a field of the order
   ListPtr<JB> jobs = typed_jobs(5);
   ListPtr<JB> others = typed_jobs(5);
   const SortOrder<JB> order = sort_order<asc<JB_priority>>();
   const SortOrder<JB> empty;
   const bool empty_order = throws([&] { jobs->insert_sorted(empty, ElementPtr<JB>::create()); },
                                   FieldException::Reason::NullElement);
   const bool foreign = throws([&] { jobs->resort(order, others->find_first<JB_job_number>(1)); },
                               FieldException::Reason::ForeignList);
   ListPtr<JB> reduced = jobs->select(ocs::tom::what<JB_job_number>());
   const bool absent = throws([&] { reduced->sort(order); }, FieldException::Reason::AbsentField);

   CHECK(6, "an empty order, an element of another list and a missing field throw",
         old_refused && empty_order && foreign && absent);
}

/**
 * @brief In a Release build, time sorting and inserting with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr std::uint32_t n = 10000;
   constexpr int rounds = 50;
   constexpr std::uint32_t inserted = 2000;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };

   // --- CULL
   lList *lp = old_jobs(n);
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lPSortList(lp, r % 2 == 0 ? "%I-%I+" : "%I+%I-", JB_priority, JB_job_number);
   }
   auto t1 = std::chrono::steady_clock::now();
   const std::vector<std::uint32_t> old_order = numbers_of(lp);
   lFreeList(&lp);
   lSortOrder *so = lParseSortOrderVarArg(JB_Type, "%I-%I+", JB_priority, JB_job_number);
   lp = lCreateList("jobs", JB_Type);
   auto t2 = std::chrono::steady_clock::now();
   for (const std::uint32_t number : scrambled(inserted)) {
      lListElem *ep = lCreateElem(JB_Type);
      lSetUlong(ep, JB_job_number, number);
      lSetUlong(ep, JB_priority, priority_of(number));
      lInsertSorted(so, ep, lp);
   }
   auto t3 = std::chrono::steady_clock::now();
   lFreeList(&lp);
   lFreeSortOrder(&so);

   // --- TOM
   ListPtr<JB> jobs = typed_jobs(n);
   auto t4 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      if (r % 2 == 0) {
         jobs->sort<desc<JB_priority>, asc<JB_job_number>>();
      } else {
         jobs->sort<asc<JB_priority>, desc<JB_job_number>>();
      }
   }
   auto t5 = std::chrono::steady_clock::now();
   const std::vector<std::uint32_t> typed_order = numbers_of(jobs);
   const SortOrder<JB> order = sort_order<desc<JB_priority>, asc<JB_job_number>>();
   ListPtr<JB> sorted = ListPtr<JB>::create("jobs");
   auto t6 = std::chrono::steady_clock::now();
   for (const std::uint32_t number : scrambled(inserted)) {
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(number);
      job->set<JB_priority>(priority_of(number));
      sorted->insert_sorted(order, std::move(job));
   }
   auto t7 = std::chrono::steady_clock::now();

   printf("timing, %d sorts of %u jobs, %u jobs inserted sorted:\n", rounds, n, inserted);
   printf("   sort:          old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t4, t5),
          ms(t4, t5) / ms(t0, t1));
   printf("   insert sorted: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t2, t3), ms(t6, t7),
          ms(t6, t7) / ms(t2, t3));
   CHECK(7, "the timing loops left the jobs in the same order", old_order == typed_order);
#endif
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   lInit(nmv);
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"OPS-SORT-01", "sort", "sort<desc<F>, asc<G>>() sorts as lPSortList does",
       {"sort 200 jobs by priority, the highest first, and by number"}, "the same order", {{"OPS-01", "covers"}},
       {1}, test_sort},
      {"OPS-SORT-02", "insert sorted", "insert_sorted() keeps a list sorted as lInsertSorted does",
       {"insert 200 jobs one by one into a list sorted by priority and number"}, "the same order",
       {{"OPS-01", "covers"}}, {2}, test_insert_sorted},
      {"OPS-SORT-03", "resort", "resort() moves a changed element as lResortElem does",
       {"raise the priority of one job of a sorted list and move it"}, "the same order, the job first",
       {{"OPS-01", "covers"}}, {3}, test_resort},
      {"OPS-SORT-04", "comparison", "sort(comparison) sorts by a key the fields do not express",
       {"sort jobs by the last digit of their number, then by number"}, "the order of a plain std::sort",
       {{"OPS-01", "covers"}}, {4}, test_sort_with_comparison},
      {"OPS-SORT-05", "stability", "both forms keep equal elements in their order",
       {"sort scrambled jobs by priority only"}, "the jobs of one priority in their scrambled order",
       {{"OPS-01", "covers"}}, {5}, test_stable},
      {"OPS-SORT-06", "errors", "the typed form throws on an empty order, a foreign element and a missing field",
       {"insert with an empty order, resort an element of another list, sort a reduced list"},
       "NullElement, ForeignList and AbsentField thrown; -1 from the old form", {{"OPS-01", "covers"}}, {6},
       test_errors},
      {"OPS-SORT-07", "timing", "a Release build times both forms",
       {"sort 10 000 jobs 50 times by two keys in alternating directions; insert 2 000 jobs sorted"},
       "ratios printed", {{"OPS-01", "covers"}}, {7}, test_timing},
   });
}
