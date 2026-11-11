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
 * @brief Lookups through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how elements are found in lists with the
 * typed object model. Every scenario looks up the same thing twice, once with
 * the old functions of the list library and once with the typed handles, and
 * compares the results. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old lookups exist once per key type (lGetElemUlong, lGetElemStr,
 * lGetElemHost, ...), take the field number and the key unchecked, and come in
 * a const and an RW variant. The typed lookup is one template, find_first<F>(),
 * whose key type follows from the field, so a string key for a number field
 * does not compile, and whose result is a const or a mutable handle depending
 * on the list handle it is called on. find_all<F>() is a range over every
 * match, in place of the First/Next loop with its iterator variable.
 *
 * A lookup in a list field of an element (lGetSubStr, lGetSubUlong, ...) is
 * the typed getter followed by the lookup:
 * job.get<JB_ja_tasks>().find_first<JAT_task_number>(n).
 *
 * Each scenario has a CULL section, which uses only the old functions of the
 * list library, and a TOM section, which uses only the typed object model.
 * Each section builds its own lists and reduces what it found to plain values
 * (numbers, strings); only those values are compared.
 *
 * In a Release build the last scenario times a million lookups by name with
 * both forms and prints the ratio.
 */

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::CE;
using ocs::tom::ConstElement;
using ocs::tom::ConstList;
using ocs::tom::EH;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JAT;
using ocs::tom::JB;
using ocs::tom::ListPtr;

namespace {

   /** @brief The list lengths the scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /**
    * @brief A name made of a prefix and a number
    * @param prefix the prefix, e.g. "c"
    * @param i the number
    * @return the name, e.g. "c3"
    */
   std::string name(const char *prefix, const int i) {
      return prefix + std::to_string(i);
   }

   /**
    * @brief CULL: a list of complex entries named c1 .. cn, and of execution hosts named host-1 .. host-n
    * @param n the number of entries and hosts
    * @param[out] entries the complex entry list, created here, owned by the caller
    * @param[out] hosts the host list, created here, owned by the caller
    */
   void old_fill(const int n, lList *&entries, lList *&hosts) {
      entries = lCreateList("entries", CE_Type);
      hosts = lCreateList("hosts", EH_Type);
      for (int i = 1; i <= n; i++) {
         lAddElemStr(&entries, CE_name, name("c", i).c_str(), CE_Type);
         lAddElemHost(&hosts, EH_name, name("host-", i).c_str(), EH_Type);
      }
   }

   /**
    * @brief TOM: a list of complex entries named c1 .. cn, and of execution hosts named host-1 .. host-n
    * @param n the number of entries and hosts
    * @param[out] entries the complex entry list, created here
    * @param[out] hosts the host list, created here
    */
   void typed_fill(const int n, ListPtr<CE> &entries, ListPtr<EH> &hosts) {
      entries = ListPtr<CE>::create("entries");
      hosts = ListPtr<EH>::create("hosts");
      for (int i = 1; i <= n; i++) {
         entries->emplace_back<CE_name>(name("c", i).c_str());
         hosts->emplace_back<EH_name>(ocs::tom::Host{name("host-", i).c_str()});
      }
   }

   /**
    * @brief CULL: a list of jobs numbered 1 .. n, owned alternately by alice and bob, job i with i % 3 tasks
    * @param n the number of jobs
    * @return the list, owned by the caller
    */
   lList *old_jobs(const int n) {
      lList *list = lCreateList("jobs", JB_Type);
      for (int i = 1; i <= n; i++) {
         lListElem *job = lCreateElem(JB_Type);
         lSetUlong(job, JB_job_number, i);
         lSetString(job, JB_owner, i % 2 == 0 ? "alice" : "bob");
         for (int t = 1; t <= i % 3; t++) {
            lAddSubUlong(job, JAT_task_number, t, JB_ja_tasks, JAT_Type);
         }
         lAppendElem(list, job);
      }
      return list;
   }

   /**
    * @brief TOM: a list of jobs numbered 1 .. n, owned alternately by alice and bob, job i with i % 3 tasks
    * @param n the number of jobs
    * @return the list
    */
   ListPtr<JB> typed_jobs(const int n) {
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
   std::string text(const char *value) {
      return value != nullptr ? value : "-";
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
         [[maybe_unused]] FILE *quiet_stderr = std::freopen("/dev/null", "w", stderr);
         [[maybe_unused]] FILE *quiet_stdout = std::freopen("/dev/null", "w", stdout);
         call();
         _exit(0);
      }
      int status = 0;
      waitpid(pid, &status, 0);
      return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Look up by a number, a string and a host key
 */
void test_by_key() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lGetElemUlong(lp, JB_job_number, 3), and lGetElemUlongRW() for a mutable element
      // typed:   jobs.find_first<JB_job_number>(3); the handle's constness decides the result's
      // differs: lGetElemUlong(lp, JB_owner, 3) compiles and fails at run time;
      //          find_first<JB_owner>(3) does not compile
      // old:     lGetElemStr(lp, CE_name, "c2")
      // typed:   entries.find_first<CE_name>("c2")
      // old:     lGetElemHost(lp, EH_name, "HOST-2") compares host names as the cluster does
      // typed:   hosts.find_first<EH_name>(Host{"HOST-2"}); the key type Host says it is a host name
      //
      // Each section writes what it found: the owner of job 3, whether job 999 was found,
      // the name found for "c2" and "none", and the host name found for "HOST-2".
      std::vector<std::string> old_found;
      std::vector<std::string> typed_found;

      // --- CULL
      {
         lList *entries = nullptr;
         lList *hosts = nullptr;
         old_fill(n, entries, hosts);
         lList *jobs = old_jobs(n);

         const lListElem *job = lGetElemUlong(jobs, JB_job_number, 3);
         old_found.push_back(text(job != nullptr ? lGetString(job, JB_owner) : nullptr));
         old_found.push_back(lGetElemUlong(jobs, JB_job_number, 999) != nullptr ? "found" : "none");
         const lListElem *entry = lGetElemStr(entries, CE_name, "c2");
         old_found.push_back(text(entry != nullptr ? lGetString(entry, CE_name) : nullptr));
         old_found.push_back(lGetElemStr(entries, CE_name, "none") != nullptr ? "found" : "none");
         const lListElem *host = lGetElemHost(hosts, EH_name, "HOST-2");
         old_found.push_back(text(host != nullptr ? lGetHost(host, EH_name) : nullptr));

         lFreeList(&jobs);
         lFreeList(&entries);
         lFreeList(&hosts);
      }

      // --- TOM
      {
         ListPtr<CE> entries;
         ListPtr<EH> hosts;
         typed_fill(n, entries, hosts);
         const ListPtr<JB> jobs = typed_jobs(n);

         const auto job = jobs->find_first<JB_job_number>(3);
         typed_found.push_back(text(job ? job.get<JB_owner>() : nullptr));
         typed_found.push_back(jobs->find_first<JB_job_number>(999) ? "found" : "none");
         const auto entry = entries->find_first<CE_name>("c2");
         typed_found.push_back(text(entry ? entry.get<CE_name>() : nullptr));
         typed_found.push_back(entries->find_first<CE_name>("none") ? "found" : "none");
         const auto host = hosts->find_first<EH_name>(ocs::tom::Host{"HOST-2"});
         typed_found.push_back(text(host ? host.get<EH_name>().c_str() : nullptr));
      }

      const std::vector<std::string> expected = {"bob", "none", "c2", "none", "host-2"};
      ok = ok && old_found == expected && typed_found == expected;
   }
   CHECK(1, "find_first finds what lGetElemUlong, lGetElemStr and lGetElemHost find, hits and misses", ok);
}

/**
 * @brief Every match of a key that several elements hold
 */
void test_all_matches() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     const void *it = nullptr;
      //          for (ep = lGetElemStrFirst(lp, JB_owner, "alice", &it); ep; ep = lGetElemStrNext(lp, JB_owner, "alice", &it))
      // typed:   for (auto job : jobs.find_all<JB_owner>("alice"))
      // differs: no iterator variable, the field and key are given once

      // --- CULL
      std::vector<std::uint32_t> old_numbers;
      {
         lList *jobs = old_jobs(n);
         const void *it = nullptr;
         for (const lListElem *ep = lGetElemStrFirst(jobs, JB_owner, "alice", &it); ep != nullptr;
              ep = lGetElemStrNext(jobs, JB_owner, "alice", &it)) {
            old_numbers.push_back(lGetUlong(ep, JB_job_number));
         }
         lFreeList(&jobs);
      }

      // --- TOM
      std::vector<std::uint32_t> typed_numbers;
      {
         const ListPtr<JB> jobs = typed_jobs(n);
         for (const ConstElement<JB> job : ConstList<JB>(*jobs).find_all<JB_owner>("alice")) {
            typed_numbers.push_back(job.get<JB_job_number>());
         }
      }

      ok = ok && old_numbers == typed_numbers && typed_numbers.size() == static_cast<std::size_t>(n / 2);
   }
   CHECK(2, "find_all walks the matches as the First/Next loop does", ok);
}

/**
 * @brief A lookup in a list field of an element
 */
void test_sub_list() {
   // old:     lGetSubUlong(job, JAT_task_number, 2, JB_ja_tasks)
   // typed:   job.get<JB_ja_tasks>().find_first<JAT_task_number>(2)
   // differs: two steps instead of one function per key type; the sub-type of the list field is
   //          known, so a field of another type than JAT does not compile
   //
   // Job 2 has two tasks, job 3 none. Each section gives the task number found in job 2
   // (0 for none) and whether a lookup in job 3 found nothing.

   // --- CULL
   std::uint32_t old_task = 0;
   bool old_empty = false;
   {
      lList *jobs = old_jobs(5);
      const lListElem *job2 = lGetElemUlong(jobs, JB_job_number, 2);
      const lListElem *job3 = lGetElemUlong(jobs, JB_job_number, 3);
      const lListElem *task = lGetSubUlong(job2, JAT_task_number, 2, JB_ja_tasks);
      old_task = task != nullptr ? lGetUlong(task, JAT_task_number) : 0;
      old_empty = lGetSubUlong(job3, JAT_task_number, 1, JB_ja_tasks) == nullptr;
      lFreeList(&jobs);
   }

   // --- TOM
   std::uint32_t typed_task = 0;
   bool typed_empty = false;
   {
      ListPtr<JB> jobs = typed_jobs(5);
      const Element<JB> job2 = jobs->find_first<JB_job_number>(2);
      const Element<JB> job3 = jobs->find_first<JB_job_number>(3);
      const auto task = job2.get<JB_ja_tasks>().find_first<JAT_task_number>(2);
      typed_task = task ? task.get<JAT_task_number>() : 0;
      // an empty list field is an empty list handle, so the lookup finds nothing, as lGetSubUlong does
      typed_empty = !job3.get<JB_ja_tasks>().find_first<JAT_task_number>(1);
   }

   const bool found = old_task == 2 && typed_task == 2;
   const bool empty = old_empty && typed_empty;
   CHECK(3, "a lookup in a sub-list finds what lGetSubUlong finds, an empty sub-list finds nothing", found && empty);
}

/**
 * @brief Case-insensitive and prefix lookups
 */
void test_nocase_and_like() {
   // old:     lGetElemCaseStr(lp, CE_name, "C3")
   // typed:   entries.find_first_nocase<CE_name>("C3")
   // old:     lGetElemStrLike(lp, CE_name, "c*"), a trailing '*' matches any rest
   // typed:   entries.find_first_like<CE_name>("c*")
   // differs: a field that is not a string field does not compile for either typed lookup
   //
   // Each section gives the names found for "C3" (no case), "c*", "c4" and "x*" (like).

   // --- CULL
   std::vector<std::string> old_found;
   {
      lList *entries = nullptr;
      lList *hosts = nullptr;
      old_fill(5, entries, hosts);
      const auto found_name = [](const lListElem *ep) { return text(ep != nullptr ? lGetString(ep, CE_name) : nullptr); };
      old_found.push_back(found_name(lGetElemCaseStr(entries, CE_name, "C3")));
      old_found.push_back(found_name(lGetElemStrLike(entries, CE_name, "c*")));
      old_found.push_back(found_name(lGetElemStrLike(entries, CE_name, "c4")));
      old_found.push_back(found_name(lGetElemStrLike(entries, CE_name, "x*")));
      lFreeList(&entries);
      lFreeList(&hosts);
   }

   // --- TOM
   std::vector<std::string> typed_found;
   {
      ListPtr<CE> entries;
      ListPtr<EH> hosts;
      typed_fill(5, entries, hosts);
      const auto found_name = [](const auto entry) { return text(entry ? entry.template get<CE_name>() : nullptr); };
      typed_found.push_back(found_name(entries->find_first_nocase<CE_name>("C3")));
      typed_found.push_back(found_name(entries->find_first_like<CE_name>("c*")));
      typed_found.push_back(found_name(entries->find_first_like<CE_name>("c4")));
      typed_found.push_back(found_name(entries->find_first_like<CE_name>("x*")));
   }

   const std::vector<std::string> expected = {"c3", "c1", "c4", "-"};
   const bool nocase = old_found[0] == expected[0] && typed_found[0] == expected[0];
   const bool like = old_found == expected && typed_found == expected;
   CHECK(4, "find_first_nocase and find_first_like find what lGetElemCaseStr and lGetElemStrLike find", nocase && like);
}

/**
 * @brief Errors: a sub-list lookup on an element without the list field
 */
void test_errors() {
   // old:     lGetSubUlong() on an element without the list field aborts the process
   // typed:   get<JB_ja_tasks>() throws a logic error with reason AbsentField

   // --- CULL
   lList *source = old_jobs(3);
   lEnumeration *what = lWhat("%T(%I)", JB_Type, JB_job_number);
   lList *reduced = lSelect("reduced", source, nullptr, what);
   const lListElem *old_job = lFirst(reduced);
   const bool old_aborts = aborts([&] { static_cast<void>(lGetSubUlong(old_job, JAT_task_number, 1, JB_ja_tasks)); });
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&source);

   // --- TOM
   bool absent = false;
   {
      const ListPtr<JB> jobs = typed_jobs(3);
      const ListPtr<JB> typed_reduced = jobs->select(ocs::tom::what<JB_job_number>(), "reduced");
      const ConstElement<JB> job = ConstList<JB>(*typed_reduced).front();
      try {
         static_cast<void>(job.get<JB_ja_tasks>());
      } catch (const FieldException &e) {
         absent = e.reason() == FieldException::Reason::AbsentField;
      }
   }

   CHECK(5, "a sub-list lookup without the list field throws AbsentField where the old one aborts", absent && old_aborts);
}

/**
 * @brief In a Release build, time lookups by name with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 10000;
   constexpr int lookups = 1000000;
   std::vector<std::string> names;
   for (int i = 1; i <= n; i++) {
      names.push_back(name("c", i));
   }
   // each loop counts the lookups that found an element, so the compiler cannot drop them
   volatile int old_hits = 0;
   volatile int typed_hits = 0;

   // --- CULL
   lList *old_entries = nullptr;
   lList *old_hosts = nullptr;
   old_fill(n, old_entries, old_hosts);
   const auto t0 = std::chrono::steady_clock::now();
   for (int i = 0; i < lookups; i++) {
      if (lGetElemStr(old_entries, CE_name, names[i % n].c_str()) != nullptr) {
         old_hits = old_hits + 1;
      }
   }
   const auto t1 = std::chrono::steady_clock::now();
   lFreeList(&old_entries);
   lFreeList(&old_hosts);

   // --- TOM
   ListPtr<CE> entries;
   ListPtr<EH> hosts;
   typed_fill(n, entries, hosts);
   const auto t2 = std::chrono::steady_clock::now();
   for (int i = 0; i < lookups; i++) {
      if (entries->find_first<CE_name>(names[i % n].c_str())) {
         typed_hits = typed_hits + 1;
      }
   }
   const auto t3 = std::chrono::steady_clock::now();

   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   printf("timing, %d lookups by name in %d complex entries:\n", lookups, n);
   printf("   by name: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t2, t3), ms(t2, t3) / ms(t0, t1));
   CHECK(6, "the timing loops ran", old_hits == lookups && typed_hits == lookups);
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
      {"OPS-LOOKUP-01", "by key", "find_first finds what the old lookups find",
       {"look up jobs by number, complex entries by name and hosts by name, in lists of 5 and 40"},
       "same element or none", {{"OPS-01", "covers"}}, {1}, test_by_key},
      {"OPS-LOOKUP-02", "all matches", "find_all walks the matches as the First/Next loop does",
       {"walk the jobs of one owner"}, "same jobs in the same order", {{"OPS-01", "covers"}}, {2}, test_all_matches},
      {"OPS-LOOKUP-03", "sub-list", "a lookup in a list field finds what lGetSubUlong finds",
       {"look up a task of a job, and in a job without tasks"}, "same task, none for an empty sub-list",
       {{"OPS-01", "covers"}}, {3}, test_sub_list},
      {"OPS-LOOKUP-04", "nocase and like", "case-insensitive and prefix lookups find what the old ones find",
       {"look up a complex entry by name in other case and by prefix"}, "same element or none",
       {{"OPS-01", "covers"}}, {4}, test_nocase_and_like},
      {"OPS-LOOKUP-05", "errors", "a sub-list lookup without the list field throws where the old one aborts",
       {"look up in the sub-list of a reduced job"}, "AbsentField thrown, the old lookup aborts",
       {{"OPS-01", "covers"}}, {5}, test_errors},
      {"OPS-LOOKUP-06", "timing", "a Release build times both forms",
       {"a million lookups by name in 10 000 complex entries"}, "ratio printed", {{"OPS-01", "covers"}}, {6},
       test_timing},
   });
}
