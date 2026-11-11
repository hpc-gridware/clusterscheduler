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
 * @brief The list structure through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how lists are walked and changed with the
 * typed object model. Every scenario does the same thing twice: a block marked
 * "--- CULL" uses only the old functions of the list library, a block marked
 * "--- TOM" uses only the typed handles. Each block builds its own data and
 * reduces its result to plain values (job numbers in order, whether every job
 * is found through its key), and only those values are compared. Each step
 * carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The typed operations are named after std::list: front(), back(),
 * push_front(), push_back(), insert_after(), erase(), extract(), splice(), and
 * next()/prev() on an element. They take and return handles that know the
 * object type, so a job list cannot be handed an element of another type, and
 * an owning handle (ElementPtr, ListPtr) shows at every call where ownership
 * moves. Errors the old functions abort on throw a logic error instead.
 *
 * The lists hold jobs, whose job number is a unique field with a hash table;
 * every scenario runs on a short list and on one above the hash threshold, and
 * checks that every job is still found through its key afterwards.
 *
 * In a Release build the last scenario times a walk and a reordering of a long
 * list with both forms and prints the ratio.
 */

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JAT;
using ocs::tom::JB;
using ocs::tom::List;
using ocs::tom::ListPtr;

namespace {

   /** @brief The list lengths every scenario runs on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /** @brief Job numbers in list order, the result every scenario compares */
   using Numbers = std::vector<std::uint32_t>;

   /**
    * @brief Build a job list the old way
    * @param name the list name
    * @param n the number of jobs
    * @param first the job number of the first job, the others count up
    * @return the list, owned by the caller
    */
   lList *old_jobs(const char *name, const int n, const std::uint32_t first) {
      // old: create the list and each element, set the key, append
      lList *lp = lCreateList(name, JB_Type);
      for (int i = 0; i < n; i++) {
         lListElem *ep = lCreateElem(JB_Type);
         lSetUlong(ep, JB_job_number, first + i);
         lAppendElem(lp, ep);
      }
      return lp;
   }

   /**
    * @brief Build the same job list with the typed handles
    * @param name the list name
    * @param n the number of jobs
    * @param first the job number of the first job, the others count up
    * @return the owning handle
    */
   ListPtr<JB> typed_jobs(const char *name, const int n, const std::uint32_t first) {
      // typed:   ListPtr owns the list and frees it with its elements when it goes out of scope;
      //          push_back() takes an ElementPtr by rvalue, so the move into the list shows at the call
      // differs: no lFreeList() needed; a job field set on a task does not compile
      ListPtr<JB> jobs = ListPtr<JB>::create(name);
      for (int i = 0; i < n; i++) {
         ElementPtr<JB> job = ElementPtr<JB>::create();
         job->set<JB_job_number>(first + i);
         jobs->push_back(std::move(job));
      }
      return jobs;
   }

   /**
    * @brief The job numbers of an old list, front to back
    * @param lp the list
    * @return the numbers
    */
   Numbers old_numbers(const lList *lp) {
      Numbers numbers;
      // old: walk with lFirst()/lNext()
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         numbers.push_back(lGetUlong(ep, JB_job_number));
      }
      return numbers;
   }

   /**
    * @brief The job numbers of a typed list, front to back
    * @param jobs the list
    * @return the numbers
    */
   Numbers typed_numbers(const List<JB> jobs) {
      Numbers numbers;
      // typed: range-for over the handle; the element handle reads its fields with get<F>()
      for (const ConstElement<JB> job : jobs) {
         numbers.push_back(job.get<JB_job_number>());
      }
      return numbers;
   }

   /**
    * @brief Whether every job of an old list is found through its key by the old lookup
    *
    * After a change, the hash table of the unique job number must still match
    * the list.
    *
    * @param lp the list
    * @return true if lGetElemUlong() finds each job itself
    */
   bool old_every_job_found(const lList *lp) {
      // old: lGetElemUlong() uses the hash table above the threshold
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         if (lGetElemUlong(lp, JB_job_number, lGetUlong(ep, JB_job_number)) != ep) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief Whether every job of a typed list is found through its key by the typed lookup
    * @param jobs the list
    * @return true if find_first<JB_job_number>() finds each job itself
    */
   bool typed_every_job_found(const List<JB> jobs) {
      // typed: find_first<F>() uses the same hash table; handles compare by the element they refer to
      for (const Element<JB> job : jobs) {
         if (jobs.find_first<JB_job_number>(job.get<JB_job_number>()) != job) {
            return false;
         }
      }
      return true;
   }

   /** @brief What a scenario compares: the job numbers in order, and whether every job is found through its key */
   struct State {
      Numbers numbers;   ///< the job numbers, front to back
      bool found;        ///< whether every job is found through its key
   };

   /**
    * @brief The state of an old list
    * @param lp the list
    * @return its job numbers and whether every job is found
    */
   State old_state(const lList *lp) {
      return {old_numbers(lp), old_every_job_found(lp)};
   }

   /**
    * @brief The state of a typed list
    * @param jobs the list
    * @return its job numbers and whether every job is found
    */
   State typed_state(const List<JB> jobs) {
      return {typed_numbers(jobs), typed_every_job_found(jobs)};
   }

   /**
    * @brief Whether the two forms ended in the same state, with every job found on both sides
    * @param old the state the old functions left
    * @param typed the state the typed handles left
    * @return true if they match
    */
   bool same(const State &old, const State &typed) {
      return old.numbers == typed.numbers && old.found && typed.found;
   }

   /**
    * @brief Build a task list the old way
    * @param n the number of tasks, numbered from 1
    * @return the list, owned by the caller
    */
   lList *old_tasks(const int n) {
      lList *lp = lCreateList("tasks", JAT_Type);
      for (int i = 1; i <= n; i++) {
         lListElem *ep = lCreateElem(JAT_Type);
         lSetUlong(ep, JAT_task_number, i);
         lAppendElem(lp, ep);
      }
      return lp;
   }

   /**
    * @brief Build the same task list with the typed handles
    * @param n the number of tasks, numbered from 1
    * @return the owning handle
    */
   ListPtr<JAT> typed_tasks(const int n) {
      ListPtr<JAT> list = ListPtr<JAT>::create("tasks");
      for (int i = 1; i <= n; i++) {
         ElementPtr<JAT> task = ElementPtr<JAT>::create();
         task->set<JAT_task_number>(i);
         list->push_back(std::move(task));
      }
      return list;
   }

   /**
    * @brief Run a call in a child process and tell whether it aborted
    * @param call the call
    * @return true if the child ended with SIGABRT
    */
   bool aborts(const std::function<void()> &call) {
      std::fflush(nullptr);
      const pid_t pid = fork();
      if (pid == 0) {
         // keep the expected messages out of the test output
         [[maybe_unused]] FILE *quiet_stderr = std::freopen("/dev/null", "w", stderr);
         [[maybe_unused]] FILE *quiet_stdout = std::freopen("/dev/null", "w", stdout);
         call();
         _exit(0);
      }
      int status = 0;
      waitpid(pid, &status, 0);
      return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
   }

   /**
    * @brief Whether a call throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   bool throws(const std::function<void()> &call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Walk a list forward and backward
 */
void test_walk() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     for (ep = lLast(lp); ep != nullptr; ep = lPrev(ep)), and lFirst()/lNext() forward
      // typed:   for (job = jobs.back(); job; job = job.prev()), and front()/next() or range-for forward
      // differs: back() of an empty list is an empty handle, not undefined as for std::list

      // --- CULL
      lList *old = old_jobs("old", n, 1);
      Numbers old_backward;
      for (const lListElem *ep = lLast(old); ep != nullptr; ep = lPrev(ep)) {
         old_backward.push_back(lGetUlong(ep, JB_job_number));
      }
      Numbers old_forward;
      for (const lListElem *ep = lFirst(old); ep != nullptr; ep = lNext(ep)) {
         old_forward.push_back(lGetUlong(ep, JB_job_number));
      }
      const State old_result = old_state(old);
      lFreeList(&old);

      // --- TOM
      ListPtr<JB> typed = typed_jobs("typed", n, 1);
      Numbers typed_backward;
      for (Element<JB> job = typed->back(); job; job = job.prev()) {
         typed_backward.push_back(job.get<JB_job_number>());
      }
      Numbers typed_forward;
      for (Element<JB> job = typed->front(); job; job = job.next()) {
         typed_forward.push_back(job.get<JB_job_number>());
      }
      const State typed_result = typed_state(*typed);

      ok = ok && old_backward == typed_backward && old_forward == typed_forward &&
           typed_forward.size() == static_cast<std::size_t>(n) && same(old_result, typed_result);
   }
   // --- TOM: an empty handle walks like an empty list
   const List<JB> none;
   ok = ok && !none.front() && !none.back() && none.size() == 0 && none.name() == nullptr;
   CHECK(1, "front/next and back/prev walk the list as lFirst/lNext and lLast/lPrev do", ok);
}

/**
 * @brief Insert at the front and after an element
 */
void test_insert() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lInsertElem(lp, nullptr, ep) inserts at the front, lInsertElem(lp, after, ep) after an element
      // typed:   jobs.push_front(std::move(job)), jobs.insert_after(after, std::move(job))
      // differs: an `after` that is not in this list throws (ForeignList); the old function trusts it

      // --- CULL
      lList *old = old_jobs("old", n, 1);
      lListElem *ep = lCreateElem(JB_Type);
      lSetUlong(ep, JB_job_number, 1000);
      lInsertElem(old, nullptr, ep);
      ep = lCreateElem(JB_Type);
      lSetUlong(ep, JB_job_number, 1001);
      lInsertElem(old, lNextRW(lFirstRW(old)), ep);
      const State old_result = old_state(old);
      lFreeList(&old);

      // --- TOM
      ListPtr<JB> typed = typed_jobs("typed", n, 1);
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(1000);
      typed->push_front(std::move(job));
      job = ElementPtr<JB>::create();
      job->set<JB_job_number>(1001);
      typed->insert_after(typed->front().next(), std::move(job));
      const bool moved = !job;
      const State typed_result = typed_state(*typed);

      ok = ok && same(old_result, typed_result) && moved;
   }
   CHECK(2, "push_front and insert_after insert where lInsertElem does, and the keys stay found", ok);
}

/**
 * @brief Erase the first, a middle and the last element
 */
void test_erase() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lRemoveElem(lp, &ep) frees the element and sets ep to nullptr
      // typed:   jobs.erase(job) frees the element; the handle dangles afterwards, like an erased iterator

      // --- CULL: the first, a middle and the last job
      lList *old = old_jobs("old", n, 1);
      lListElem *ep = lFirstRW(old);
      lRemoveElem(old, &ep);
      ep = lFirstRW(old);
      for (int i = 0; i < n / 2; i++) {
         ep = lNextRW(ep);
      }
      lRemoveElem(old, &ep);
      ep = lLastRW(old);
      lRemoveElem(old, &ep);
      const std::uint32_t old_size = lGetNumberOfElem(old);
      const State old_result = old_state(old);
      lFreeList(&old);

      // --- TOM: the same three
      ListPtr<JB> typed = typed_jobs("typed", n, 1);
      typed->erase(typed->front());
      Element<JB> middle = typed->front();
      for (int i = 0; i < n / 2; i++) {
         middle = middle.next();
      }
      typed->erase(middle);
      typed->erase(typed->back());
      const std::size_t typed_size = typed->size();
      const State typed_result = typed_state(*typed);

      ok = ok && same(old_result, typed_result) && old_size == static_cast<std::uint32_t>(n - 3) &&
           typed_size == static_cast<std::size_t>(n - 3);
   }
   CHECK(3, "erase removes and frees as lRemoveElem does", ok);
}

/**
 * @brief Take an element out of one list and append it to another
 */
void test_extract() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     ep = lDechainElem(lp, ep); lAppendElem(other, ep);
      //          between the two calls the caller owns ep and must free it on every early return
      // typed:   ElementPtr<JB> job = jobs.extract(job); other.push_back(std::move(job));
      // differs: the extracted element is owned by the handle, freed if it is not moved on;
      //          std::set::extract hands out a node handle the same way

      // --- CULL: move the second job to the other list, take out the last one and free it
      lList *old = old_jobs("old", n, 1);
      lList *old_other = old_jobs("old other", 3, 500);
      lListElem *ep = lNextRW(lFirstRW(old));
      ep = lDechainElem(old, ep);
      lAppendElem(old_other, ep);
      lListElem *old_dropped = lDechainElem(old, lLastRW(old));
      lFreeElem(&old_dropped);
      const State old_result = old_state(old);
      const State old_other_result = old_state(old_other);
      lFreeList(&old);
      lFreeList(&old_other);

      // --- TOM: the same; an extracted element that is not moved on is freed by its handle
      //          (checked by the sanitizer runs)
      ListPtr<JB> typed = typed_jobs("typed", n, 1);
      ListPtr<JB> typed_other = typed_jobs("typed other", 3, 500);
      ElementPtr<JB> job = typed->extract(typed->front().next());
      typed_other->push_back(std::move(job));
      ElementPtr<JB> dropped = typed->extract(typed->back());
      const bool held = static_cast<bool>(dropped);
      const State typed_result = typed_state(*typed);
      const State typed_other_result = typed_state(*typed_other);

      ok = ok && same(old_result, typed_result) && same(old_other_result, typed_other_result) && held;
   }
   CHECK(4, "extract dechains as lDechainElem does, into an owning handle", ok);
}

/**
 * @brief Move all elements of one list to another
 */
void test_splice() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lAddList(lp, &other) moves the elements and frees the other list
      // typed:   jobs.splice(std::move(other)) with an owning handle
      // old:     lAppendList(lp, other) moves the elements, the other list stays, empty
      // typed:   jobs.splice(other) with a non-owning handle, as std::list::splice

      // --- CULL
      lList *old = old_jobs("old", n, 1);
      lList *old_owned = old_jobs("old owned", 3, 500);
      lList *old_kept = old_jobs("old kept", 3, 600);
      lAddList(old, &old_owned);
      lAppendList(old, old_kept);
      const bool old_moved = old_owned == nullptr && lGetNumberOfElem(old_kept) == 0;
      const State old_result = old_state(old);
      lFreeList(&old);
      lFreeList(&old_kept);

      // --- TOM
      ListPtr<JB> typed = typed_jobs("typed", n, 1);
      ListPtr<JB> typed_owned = typed_jobs("typed owned", 3, 500);
      ListPtr<JB> typed_kept = typed_jobs("typed kept", 3, 600);
      typed->splice(std::move(typed_owned));
      typed->splice(*typed_kept);
      const bool typed_moved = !typed_owned && typed_kept->empty();
      const State typed_result = typed_state(*typed);

      ok = ok && same(old_result, typed_result) && old_moved && typed_moved;
   }
   CHECK(5, "splice moves all elements as lAddList and lAppendList do", ok);
}

/**
 * @brief Exchange and swap the task lists of jobs
 */
void test_exchange_and_swap() {
   // old:     lList *tmp = new_list; lXchgList(ep, JB_ja_tasks, &tmp); ... lFreeList(&tmp);
   //          tmp is in and out parameter, and the caller must remember to free what comes back
   // typed:   ListPtr<JAT> previous = job.exchange<JB_ja_tasks>(std::move(new_list));
   // differs: as std::exchange, the new list goes in and the old one comes back, owned;
   //          a list of another sub-type does not compile
   // old:     lSwapList(to, JB_ja_tasks, from, JB_ja_tasks)
   // typed:   job.swap<JB_ja_tasks, JB_ja_tasks>(second)
   // differs: both fields must hold the same sub-type, checked at compile time

   // --- CULL: two jobs, the first starts with 3 tasks; exchange in 2, then swap with 5 tasks of the second
   lList *old = old_jobs("old", 2, 1);
   lListElem *old_job = lFirstRW(old);
   lListElem *old_second = lNextRW(old_job);
   lSetList(old_job, JB_ja_tasks, old_tasks(3));
   lList *tmp = old_tasks(2);
   lXchgList(old_job, JB_ja_tasks, &tmp);
   const Numbers old_exchanged = {lGetNumberOfElem(tmp), lGetNumberOfElem(lGetList(old_job, JB_ja_tasks))};
   lFreeList(&tmp);
   lSetList(old_second, JB_ja_tasks, old_tasks(5));
   lSwapList(old_job, JB_ja_tasks, old_second, JB_ja_tasks);
   const Numbers old_swapped = {lGetNumberOfElem(lGetList(old_job, JB_ja_tasks)),
                                lGetNumberOfElem(lGetList(old_second, JB_ja_tasks))};
   lFreeList(&old);

   // --- TOM: the same
   ListPtr<JB> typed = typed_jobs("typed", 2, 1);
   Element<JB> job = typed->front();
   Element<JB> second = job.next();
   job.set<JB_ja_tasks>(typed_tasks(3));
   ListPtr<JAT> previous = job.exchange<JB_ja_tasks>(typed_tasks(2));
   const Numbers typed_exchanged = {static_cast<std::uint32_t>(previous->size()),
                                    static_cast<std::uint32_t>(job.get<JB_ja_tasks>().size())};
   second.set<JB_ja_tasks>(typed_tasks(5));
   job.swap<JB_ja_tasks, JB_ja_tasks>(second);
   const Numbers typed_swapped = {static_cast<std::uint32_t>(job.get<JB_ja_tasks>().size()),
                                  static_cast<std::uint32_t>(second.get<JB_ja_tasks>().size())};

   const bool exchanged = old_exchanged == typed_exchanged && typed_exchanged == Numbers{3, 2};
   const bool swapped = old_swapped == typed_swapped && typed_swapped == Numbers{5, 2};
   CHECK(6, "exchange returns the list held before, as lXchgList; swap swaps as lSwapList", exchanged && swapped);
}

/**
 * @brief Errors: an element of another list, and an empty handle
 */
void test_errors() {
   // old:     lRemoveElem() and lDechainElem() with an element of another list abort the process
   // typed:   erase() and extract() throw a logic error with reason ForeignList, and change nothing

   // --- TOM
   ListPtr<JB> typed = typed_jobs("typed", 3, 1);
   ListPtr<JB> other = typed_jobs("other", 3, 100);
   const bool foreign = throws([&] { typed->erase(other->front()); }, FieldException::Reason::ForeignList) &&
                        throws([&] { static_cast<void>(typed->extract(other->front())); },
                               FieldException::Reason::ForeignList) &&
                        throws([&] { typed->insert_after(other->front(), ElementPtr<JB>::create()); },
                               FieldException::Reason::ForeignList) &&
                        typed->size() == 3 && other->size() == 3;

   // --- CULL
   lList *old = old_jobs("old", 3, 1);
   lList *old_other = old_jobs("old other", 3, 100);
   const bool old_aborts = aborts([&] {
                              lListElem *ep = lFirstRW(old_other);
                              lRemoveElem(old, &ep);
                           }) &&
                           aborts([&] { static_cast<void>(lDechainElem(old, lFirstRW(old_other))); });
   lFreeList(&old);
   lFreeList(&old_other);
   CHECK(7, "an element of another list throws ForeignList where the old functions abort", foreign && old_aborts);

   // old:     lInsertElem(nullptr, ...) and lRemoveElem(nullptr, ...) return -1
   // typed:   a modifying operation on an empty handle throws a logic error with reason EmptyHandle

   // --- TOM
   const List<JB> none;
   const bool empty = throws([&] { none.push_front(ElementPtr<JB>::create()); }, FieldException::Reason::EmptyHandle) &&
                      throws([&] { none.splice(*typed); }, FieldException::Reason::EmptyHandle) &&
                      typed->size() == 3;

   // --- CULL
   lListElem *ep = lCreateElem(JB_Type);
   const bool old_returns = lInsertElem(nullptr, nullptr, ep) == -1;
   lFreeElem(&ep);
   CHECK(8, "a modifying operation on an empty handle throws EmptyHandle where the old one returns -1",
         empty && old_returns);
}

/**
 * @brief In a Release build, time a walk and a reordering of a long list with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 10000;
   constexpr int rounds = 50;
   // each form is timed on the list it built itself
   lList *old = old_jobs("old", n, 1);
   ListPtr<JB> typed = typed_jobs("typed", n, 1);
   volatile std::uint64_t sink = 0;

   // walk: sum the job numbers forward and backward; --- CULL, then --- TOM
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      std::uint64_t sum = 0;
      for (const lListElem *ep = lFirst(old); ep != nullptr; ep = lNext(ep)) {
         sum += lGetUlong(ep, JB_job_number);
      }
      for (const lListElem *ep = lLast(old); ep != nullptr; ep = lPrev(ep)) {
         sum += lGetUlong(ep, JB_job_number);
      }
      sink = sink + sum;
   }
   auto t1 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      std::uint64_t sum = 0;
      for (const ConstElement<JB> job : *typed) {
         sum += job.get<JB_job_number>();
      }
      for (Element<JB> job = typed->back(); job; job = job.prev()) {
         sum += job.get<JB_job_number>();
      }
      sink = sink + sum;
   }
   auto t2 = std::chrono::steady_clock::now();

   // reorder: move the first element to the back, n times per round; --- CULL, then --- TOM
   for (int r = 0; r < rounds; r++) {
      for (int i = 0; i < n; i++) {
         lAppendElem(old, lDechainElem(old, lFirstRW(old)));
      }
   }
   auto t3 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      for (int i = 0; i < n; i++) {
         typed->push_back(typed->extract(typed->front()));
      }
   }
   auto t4 = std::chrono::steady_clock::now();

   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   printf("timing, %d rounds over %d jobs:\n", rounds, n);
   printf("   walk:    old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t1, t2), ms(t1, t2) / ms(t0, t1));
   printf("   reorder: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t2, t3), ms(t3, t4), ms(t3, t4) / ms(t2, t3));
   const bool ok = same(old_state(old), typed_state(*typed));
   lFreeList(&old);
   CHECK(9, "the timing loops leave both lists equal", ok && sink != 0);
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
   return ocs::module_test::run(argc, argv, {
      {"OPS-LIST-01", "walk", "front/next and back/prev walk a list as the old functions do",
       {"walk a list of 5 and of 40 jobs forward and backward with both forms"}, "the same job numbers in order",
       {{"OPS-01", "covers"}}, {1}, test_walk},
      {"OPS-LIST-02", "insert", "push_front and insert_after insert where lInsertElem does",
       {"insert a job at the front and after the second job"}, "same order, every key found",
       {{"OPS-01", "covers"}}, {2}, test_insert},
      {"OPS-LIST-03", "erase", "erase removes and frees as lRemoveElem does",
       {"erase the first, a middle and the last job"}, "same order, every key found",
       {{"OPS-01", "covers"}}, {3}, test_erase},
      {"OPS-LIST-04", "extract", "extract dechains into an owning handle as lDechainElem does",
       {"extract a job and append it to another list; extract one and drop it"}, "same lists, no leak",
       {{"OPS-01", "covers"}}, {4}, test_extract},
      {"OPS-LIST-05", "splice", "splice moves all elements as lAddList and lAppendList do",
       {"splice an owned list and a kept list onto a job list"}, "same lists, the moved lists empty or gone",
       {{"OPS-01", "covers"}}, {5}, test_splice},
      {"OPS-LIST-06", "exchange and swap", "exchange and swap move list fields as lXchgList and lSwapList do",
       {"exchange a job's task list, swap the task lists of two jobs"}, "the task counts follow",
       {{"OPS-01", "covers"}}, {6}, test_exchange_and_swap},
      {"OPS-LIST-07", "errors", "foreign elements and empty handles throw where the old functions abort or return -1",
       {"erase, extract and insert after an element of another list; modify an empty handle"},
       "ForeignList and EmptyHandle thrown, nothing changed", {{"OPS-01", "covers"}}, {7, 8}, test_errors},
      {"OPS-LIST-08", "timing", "a Release build times both forms",
       {"walk and reorder 10 000 jobs 50 times with both forms"}, "ratios printed, lists equal",
       {{"OPS-01", "covers"}}, {9}, test_timing},
   });
}
