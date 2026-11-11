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
 * @brief Creating, copying and freeing through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how ownership works in the typed object
 * model. Every scenario does the same thing twice, once with the old functions
 * of the list library and once with the typed handles, and compares the
 * results. The two are kept apart: the block marked CULL uses only the old
 * functions, the block marked TOM only the typed handles, each builds its own
 * data and reduces its result to plain values (numbers, strings, flags), and
 * only those are compared. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The owning handles ElementPtr and ListPtr work like std::unique_ptr: they
 * free what they hold when they go out of scope, are moved but never copied,
 * and offer get(), release() and reset(). A copy is always an explicit call,
 * clone(), so it shows in the code. With the old interface every lCreate*()
 * and lCopy*() needs its lFree*() on every path, including the early returns;
 * with the owning handles that is the compiler's job.
 *
 * The lists hold jobs with task lists, so a copy is a deep copy of more than
 * one level. In a Release build the last scenario times copying and creating
 * with both forms and prints the ratio.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::Hashing;
using ocs::tom::JAT;
using ocs::tom::JB;
using ocs::tom::List;
using ocs::tom::ListPtr;

namespace {

   /** @brief The list lengths the list scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /** @brief Job numbers and their task counts in list order, the result the scenarios compare */
   using Jobs = std::vector<std::pair<std::uint32_t, std::size_t>>;

   /**
    * @brief Build a job list with tasks the old way
    * @param name the list name
    * @param n the number of jobs; job i has i % 4 tasks
    * @return the list, owned by the caller, who must free it on every path
    */
   lList *old_jobs(const char *name, const int n) {
      lList *lp = lCreateList(name, JB_Type);
      for (int i = 1; i <= n; i++) {
         lListElem *ep = lCreateElem(JB_Type);
         lSetUlong(ep, JB_job_number, i);
         lList *tasks = lCreateList("tasks", JAT_Type);
         for (int t = 1; t <= i % 4; t++) {
            lListElem *task = lCreateElem(JAT_Type);
            lSetUlong(task, JAT_task_number, t);
            lAppendElem(tasks, task);
         }
         lSetList(ep, JB_ja_tasks, tasks);
         lAppendElem(lp, ep);
      }
      return lp;
   }

   /**
    * @brief Build the same job list with the typed handles
    * @param name the list name
    * @param n the number of jobs; job i has i % 4 tasks
    * @return the owning handle, which frees the list with its jobs and tasks
    */
   ListPtr<JB> typed_jobs(const char *name, const int n) {
      ListPtr<JB> jobs = ListPtr<JB>::create(name);
      for (int i = 1; i <= n; i++) {
         ElementPtr<JB> job = ElementPtr<JB>::create();
         job->set<JB_job_number>(i);
         ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
         for (int t = 1; t <= i % 4; t++) {
            ElementPtr<JAT> task = ElementPtr<JAT>::create();
            task->set<JAT_task_number>(t);
            tasks->push_back(std::move(task));
         }
         // the job takes the task list over; tasks is empty afterwards
         job->set<JB_ja_tasks>(std::move(tasks));
         jobs->push_back(std::move(job));
      }
      return jobs;
   }

   /**
    * @brief Job numbers and task counts of a list, read the old way
    * @param lp the list
    * @return them, front to back
    */
   Jobs old_contents(const lList *lp) {
      Jobs result;
      const lListElem *ep;
      for_each_ep(ep, lp) {
         result.emplace_back(lGetUlong(ep, JB_job_number), lGetNumberOfElem(lGetList(ep, JB_ja_tasks)));
      }
      return result;
   }

   /**
    * @brief Whether every job of a list is found through its key, the old way
    * @param lp the list
    * @return true if every job is found
    */
   bool old_every_job_found(const lList *lp) {
      const lListElem *ep;
      for_each_ep(ep, lp) {
         if (lGetElemUlong(lp, JB_job_number, lGetUlong(ep, JB_job_number)) != ep) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief Job numbers and task counts of a list
    * @param jobs the list
    * @return them, front to back
    */
   Jobs contents(const ocs::tom::ConstList<JB> jobs) {
      Jobs result;
      for (const ConstElement<JB> job : jobs) {
         result.emplace_back(job.get<JB_job_number>(), job.get<JB_ja_tasks>().size());
      }
      return result;
   }

   /**
    * @brief Whether every job of a list is found through its key
    * @param jobs the list
    * @return true if every job is found
    */
   bool every_job_found(const List<JB> jobs) {
      for (const Element<JB> job : jobs) {
         if (jobs.find_first<JB_job_number>(job.get<JB_job_number>()) != job) {
            return false;
         }
      }
      return true;
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Create and free, by scope, reset(), move assignment and release()
 */
void test_create_and_free() {
   // old:     lListElem *ep = lCreateElem(JB_Type); ... lFreeElem(&ep);
   //          the free must be on every path out of the function
   // typed:   ElementPtr<JB> job = ElementPtr<JB>::create(); the destructor frees it
   // differs: a job field set on another type does not compile; nothing to free by hand
   // old:     lFreeElem(&ep) frees and sets ep to nullptr
   // typed:   job.reset(), as std::unique_ptr::reset()
   // old:     lFreeList(&lp); lp = other;
   // typed:   jobs = std::move(other); the move assignment frees what jobs held
   // old:     a function returning lList * hands ownership to the caller by convention only
   // typed:   release() gives the raw list up, the checked constructor takes it back
   // differs: the checked constructor throws if the list holds elements of another type

   // --- CULL
   lListElem *ep = lCreateElem(JB_Type);
   lSetUlong(ep, JB_job_number, 1);
   const std::uint32_t old_number = lGetUlong(ep, JB_job_number);
   lFreeElem(&ep);
   const bool old_reset = ep == nullptr;
   lList *lp = old_jobs("first", 3);
   lFreeList(&lp);
   lp = old_jobs("second", 2);
   const bool old_reassigned = lGetNumberOfElem(lp) == 2 && std::strcmp(lGetListName(lp), "second") == 0;
   lList *handed = lp;   // ownership passes by convention, nothing marks lp as given up
   lp = nullptr;
   const bool old_handed = lp == nullptr && lGetNumberOfElem(handed) == 2;
   lFreeList(&handed);

   // --- TOM
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(1);
   const std::uint32_t typed_number = job->get<JB_job_number>();
   job.reset();
   const bool typed_reset = !job;
   ListPtr<JB> jobs = typed_jobs("first", 3);
   jobs = typed_jobs("second", 2);
   const bool typed_reassigned = jobs->size() == 2 && std::strcmp(jobs->name(), "second") == 0;
   // the bridge to raw pointers is the subject of this step: release() and the checked constructor
   lList *raw = jobs.release();
   const bool released = !jobs && raw != nullptr;
   ListPtr<JB> back(raw);
   const bool typed_handed = released && back->size() == 2;

   CHECK(1, "create, reset, move assignment and release free and hand over as lCreate*/lFree* do",
         old_number == 1 && typed_number == old_number && old_reset && typed_reset && old_reassigned &&
         typed_reassigned && old_handed && typed_handed);
}

/**
 * @brief Clone an element: a deep copy, as lCopyElem
 */
void test_clone_element() {
   // old:     lListElem *copy = lCopyElem(ep); ... lFreeElem(&copy);
   // typed:   ElementPtr<JB> copy = job.clone();
   // differs: the copy is owned by the handle; the original handle cannot be copied by accident
   // the copy is deep: changing its tasks leaves the original as it was, and a reduced element stays reduced

   // --- CULL
   lList *old = old_jobs("old", 3);
   lListElem *old_copy = lCopyElem(lLastRW(old));
   lSetUlong(old_copy, JB_job_number, 99);
   lSetList(old_copy, JB_ja_tasks, nullptr);
   const bool old_deep = lGetUlong(lLast(old), JB_job_number) == 3 &&
                         lGetNumberOfElem(lGetList(lLast(old), JB_ja_tasks)) == 3 &&
                         lGetList(old_copy, JB_ja_tasks) == nullptr;
   lFreeElem(&old_copy);
   lEnumeration *what = lWhat("%T(%I)", JB_Type, JB_job_number);
   lList *old_reduced = lSelect("reduced", old, nullptr, what);
   lListElem *old_reduced_copy = lCopyElem(lFirst(old_reduced));
   const bool old_stays_reduced = lGetPosViaElem(old_reduced_copy, JB_job_number, SGE_NO_ABORT) >= 0 &&
                                  lGetPosViaElem(old_reduced_copy, JB_ja_tasks, SGE_NO_ABORT) < 0;
   lFreeElem(&old_reduced_copy);
   lFreeList(&old_reduced);
   lFreeWhat(&what);
   lFreeList(&old);

   // --- TOM
   ListPtr<JB> typed = typed_jobs("typed", 3);
   ElementPtr<JB> copy = typed->back().clone();
   copy->set<JB_job_number>(99);
   copy->set<JB_ja_tasks>(ListPtr<JAT>{});
   const bool typed_deep = typed->back().get<JB_job_number>() == 3 &&
                           typed->back().get<JB_ja_tasks>().size() == 3 && !copy->get<JB_ja_tasks>();
   ListPtr<JB> reduced = typed->select(ocs::tom::what<JB_job_number>(), "reduced");
   ElementPtr<JB> reduced_copy = reduced->front().clone();
   const bool typed_stays_reduced = reduced_copy->has<JB_job_number>() && !reduced_copy->has<JB_ja_tasks>();

   CHECK(2, "clone copies an element deeply as lCopyElem does, and keeps a reduction",
         old_deep && typed_deep && old_stays_reduced && typed_stays_reduced);
}

/**
 * @brief Clone a list: a deep copy with its hash tables, as lCopyList
 */
void test_clone_list() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lList *copy = lCopyList("copy", lp); ... lFreeList(&copy);
      // typed:   ListPtr<JB> copy = jobs.clone("copy");
      // old:     lCopyList(nullptr, lp) keeps the name
      // typed:   jobs.clone() keeps the name
      // the copy is independent: erasing from it leaves the source as it was

      // --- CULL
      lList *old = old_jobs("jobs", n);
      lList *old_copy = lCopyList("copy", old);
      lList *old_same_name = lCopyList(nullptr, old);
      lListElem *first = lFirstRW(old_copy);
      lRemoveElem(old_copy, &first);
      const Jobs old_result = old_contents(old_copy);
      const bool old_ok = lGetNumberOfElem(old) == static_cast<std::uint32_t>(n) && old_every_job_found(old_copy) &&
                          std::strcmp(lGetListName(old_copy), "copy") == 0 &&
                          std::strcmp(lGetListName(old_same_name), "jobs") == 0;
      lFreeList(&old_same_name);
      lFreeList(&old_copy);
      lFreeList(&old);

      // --- TOM
      ListPtr<JB> typed = typed_jobs("jobs", n);
      ListPtr<JB> copy = typed->clone("copy");
      ListPtr<JB> same_name = typed->clone();
      copy->erase(copy->front());
      const Jobs typed_result = contents(*copy);
      const bool typed_ok = typed->size() == static_cast<std::size_t>(n) && every_job_found(*copy) &&
                            std::strcmp(copy->name(), "copy") == 0 &&
                            std::strcmp(same_name->name(), "jobs") == 0;

      ok = ok && old_result == typed_result && old_ok && typed_ok;
   }
   CHECK(3, "clone copies a list deeply as lCopyList does, keys found, name given or kept", ok);
}

/**
 * @brief Cloning an empty handle gives an empty owning handle, as copying nullptr gives nullptr
 */
void test_clone_empty() {
   // old:     lCopyList("x", nullptr) and lCopyElem(nullptr) give nullptr, used as "copy if there is one"
   // typed:   clone() of an empty handle gives an empty owning handle
   // differs: nothing; an empty source is not an error in either form

   // --- CULL
   const bool old_empty = lCopyList("x", nullptr) == nullptr && lCopyElem(nullptr) == nullptr;

   // --- TOM
   const List<JB> no_list;
   const Element<JB> no_job;
   const bool typed_empty = !no_list.clone("x") && !no_job.clone();

   CHECK(4, "clone of an empty handle gives an empty owning handle, as copying nullptr gives nullptr",
         typed_empty && old_empty);
}

/**
 * @brief A list without hash tables
 */
void test_no_hashing() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lCreateListHash("jobs", JB_Type, false)
      // typed:   ListPtr<JB>::create("jobs", Hashing::Off)
      // differs: an enum instead of a bare bool, so the call says what it does
      // lookups still find every job, by searching

      // --- CULL
      lList *old = lCreateListHash("old", JB_Type, false);
      for (int i = 1; i <= n; i++) {
         lListElem *ep = lCreateElem(JB_Type);
         lSetUlong(ep, JB_job_number, i);
         lAppendElem(old, ep);
      }
      // the old interface has no function for the hash state; its list structure shows it
      const bool old_ok = old->nslots == 0 && old_every_job_found(old);
      lFreeList(&old);

      // --- TOM
      ListPtr<JB> typed = ListPtr<JB>::create("typed", Hashing::Off);
      for (int i = 1; i <= n; i++) {
         ElementPtr<JB> job = ElementPtr<JB>::create();
         job->set<JB_job_number>(i);
         typed->push_back(std::move(job));
      }
      // bridge: the typed interface does not show the hash state, by design; this check alone looks
      // through raw() at the list structure, the scenario's subject
      const bool typed_ok = typed->raw()->nslots == 0 && every_job_found(*typed);

      ok = ok && old_ok && typed_ok;
   }
   // --- CULL
   lList *old_hashed = lCreateList("hashed", JB_Type);
   ok = ok && old_hashed->nslots > 0;
   lFreeList(&old_hashed);
   // --- TOM (bridge through raw(), as above)
   ListPtr<JB> hashed = ListPtr<JB>::create("hashed");
   ok = ok && hashed->raw()->nslots > 0;
   CHECK(5, "Hashing::Off creates a list without hash tables, as lCreateListHash with false", ok);
}

/**
 * @brief In a Release build, time copying and creating with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 10000;
   constexpr int rounds = 20;
   // each form times its own data: the CULL loops a list built the old way, the TOM loops a typed one
   lList *old = old_jobs("old", n);
   ListPtr<JB> typed = typed_jobs("typed", n);
   volatile std::uint64_t sink = 0;

   // copy the whole list with its tasks
   // --- CULL
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lList *copy = lCopyList("copy", old);
      sink = sink + lGetNumberOfElem(copy);
      lFreeList(&copy);
   }
   auto t1 = std::chrono::steady_clock::now();
   // --- TOM
   for (int r = 0; r < rounds; r++) {
      ListPtr<JB> copy = typed->clone("copy");
      sink = sink + copy->size();
   }
   auto t2 = std::chrono::steady_clock::now();

   // create, set and free single jobs
   // --- CULL
   for (int r = 0; r < rounds; r++) {
      for (int i = 0; i < n; i++) {
         lListElem *ep = lCreateElem(JB_Type);
         lSetUlong(ep, JB_job_number, i);
         sink = sink + lGetUlong(ep, JB_job_number);
         lFreeElem(&ep);
      }
   }
   auto t3 = std::chrono::steady_clock::now();
   // --- TOM
   for (int r = 0; r < rounds; r++) {
      for (int i = 0; i < n; i++) {
         ElementPtr<JB> job = ElementPtr<JB>::create();
         job->set<JB_job_number>(i);
         sink = sink + job->get<JB_job_number>();
      }
   }
   auto t4 = std::chrono::steady_clock::now();

   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   printf("timing, %d rounds over %d jobs:\n", rounds, n);
   printf("   copy list:   old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t1, t2),
          ms(t1, t2) / ms(t0, t1));
   printf("   create/free: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t2, t3), ms(t3, t4),
          ms(t3, t4) / ms(t2, t3));
   lFreeList(&old);
   CHECK(6, "the timing loops ran", sink != 0);
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
      {"OPS-OWN-01", "create and free", "the owning handles create, free and hand over as the old functions do",
       {"create and free by scope, reset(), move assignment, release() and taking back"}, "no leak, same values",
       {{"OPS-01", "covers"}}, {1}, test_create_and_free},
      {"OPS-OWN-02", "clone an element", "clone copies an element deeply as lCopyElem does",
       {"clone a job with tasks, change the copy; clone a reduced job"}, "original unchanged, reduction kept",
       {{"OPS-01", "covers"}}, {2}, test_clone_element},
      {"OPS-OWN-03", "clone a list", "clone copies a list deeply as lCopyList does",
       {"clone lists of 5 and 40 jobs with tasks, change the copy"}, "same contents, keys found, source unchanged",
       {{"OPS-01", "covers"}}, {3}, test_clone_list},
      {"OPS-OWN-04", "clone empty", "cloning an empty handle gives an empty owning handle",
       {"clone an empty list and an empty element handle"}, "empty, as nullptr for the old copies",
       {{"OPS-01", "covers"}}, {4}, test_clone_empty},
      {"OPS-OWN-05", "no hashing", "Hashing::Off creates a list without hash tables",
       {"fill lists of 5 and 40 jobs without hashing"}, "no hash tables, every job found",
       {{"OPS-01", "covers"}}, {5}, test_no_hashing},
      {"OPS-OWN-06", "timing", "a Release build times both forms",
       {"copy a list of 10 000 jobs and create 10 000 jobs, 20 times, with both forms"}, "ratios printed",
       {{"OPS-01", "covers"}}, {6}, test_timing},
   });
}
