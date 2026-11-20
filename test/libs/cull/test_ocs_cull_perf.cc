/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

/** @file
 * @brief Performance test for the run-time typed accessors, the lookup on short lists and the template path of the
 *        typed object model (libs/tom)
 */

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

/// how often the accessor is called; keeps the test in the range of seconds
static constexpr uint64_t num_lookups = 1000000000;

/// the value the attribute is set to, so the printed sum is predictable
static constexpr std::uint32_t job_value = 42;

/// how often each short-list lookup is repeated
static constexpr uint64_t num_short_lookups = 20000000;

/**
 * @brief Measure lookups on a unique field of lists below the hash threshold
 *
 * Such a list has no table and is searched linearly. For a unique field the
 * search used to stop at the first match; since stage 2 of N-H-18 (CS-3052) it
 * goes on to the end to find a key held twice. This prints the cost of one
 * lookup for a key at the front, in the middle, at the end and for a key that
 * is not there, for lists of 4, 8 and 15 tasks; the miss always scanned the
 * whole list, so it is the reference. Built against the code before and after
 * the change it shows what the search to the end costs.
 *
 * The lookup is the run-time typed ocs::tom::find_first<std::uint32_t>(), which
 * checks the key field once and calls the core lookup ocs::tom::lookup_first(),
 * as lGetElemUlong() did before CS-3103.
 *
 * The same compiler barrier as in main() keeps every lookup inside the loop.
 */
static void
measure_short_lookups() {
   printf("lookup of JAT_task_number (unique, linear search below the hash threshold), ns per lookup\n");
   printf("%8s %8s %8s %8s %8s\n", "tasks", "front", "middle", "end", "miss");
   for (const std::uint32_t n : {4u, 8u, 15u}) {
      auto tasks = ocs::tom::ListPtr<ocs::tom::JAT>::create("tasks");
      for (std::uint32_t i = 1; i <= n; i++) {
         tasks->emplace_back<JAT_task_number>(i);
      }
      const lList *raw_tasks = tasks->raw();
      double ns[4];
      const std::uint32_t keys[4] = {1, n / 2 + 1, n, n + 1};
      for (int k = 0; k < 4; k++) {
         uint64_t found = 0;
         const auto start = std::chrono::high_resolution_clock::now();
         for (uint64_t i = 0; i < num_short_lookups; ++i) {
            found += ocs::tom::find_first<std::uint32_t>(raw_tasks, JAT_task_number, keys[k]) != nullptr ? 1 : 0;
            std::atomic_signal_fence(std::memory_order_acq_rel);
         }
         const auto end = std::chrono::high_resolution_clock::now();
         ns[k] = std::chrono::duration<double, std::nano>(end - start).count() / num_short_lookups;
         if (found != (k < 3 ? num_short_lookups : 0)) {
            printf("unexpected result for key " sge_u32 "\n", keys[k]);
         }
      }
      printf("%8u %8.1f %8.1f %8.1f %8.1f\n", static_cast<unsigned>(n), ns[0], ns[1], ns[2], ns[3]);
   }
}

/// the jobs of the list the template path is measured on
static constexpr std::uint32_t num_jobs = 10000;

/// how often the list is walked; 2000 walks of 10 000 jobs are 20 million rounds per variant
static constexpr int num_walks = 2000;

/**
 * @brief PRF-02 (CS-3044): reads and writes over a list of jobs, through the template path and the run-time path
 *
 * Each round reads a number field (JB_job_number) and a string field
 * (JB_owner) and writes a number field that is not hashed (JB_priority), once
 * through the run-time typed handles, which look the field up by its number
 * on every access as lGetUlong(), lGetString() and lSetUlong() did
 * (ocs::tom::RawElement::get<V>(nm), set(nm, v)), once through the typed list
 * and its element handles. The two variants alternate three times so that a
 * warm-up or a frequency change shows as a spread, not as a difference. The
 * ratio is recorded, not judged (PRF-02).
 *
 * The same compiler barrier as in main() ends every round, so the loop
 * measures the accessors and not what the optimiser can prove about them.
 */
static void
measure_template_path() {
   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   for (std::uint32_t i = 1; i <= num_jobs; i++) {
      jobs->emplace_back<JB_job_number>(i).set<JB_owner>("user");
   }
   const ocs::tom::List<ocs::tom::JB> typed = *jobs;
   const ocs::tom::RawList raw = typed;

   printf("PRF-02: %d walks over %u jobs, read 2 fields and write 1 per job, ms\n", num_walks,
          static_cast<unsigned>(num_jobs));
   printf("%8s %10s %10s %8s\n", "round", "run-time", "template", "ratio");
   for (int round = 1; round <= 3; round++) {
      uint64_t sum_raw = 0;
      auto start = std::chrono::high_resolution_clock::now();
      for (int w = 0; w < num_walks; w++) {
         for (const ocs::tom::RawElement ep : raw) {
            sum_raw += ep.get<std::uint32_t>(JB_job_number);
            sum_raw += ep.get<const char *>(JB_owner) != nullptr ? 1 : 0;
            ep.set(JB_priority, static_cast<std::uint32_t>(w));
            std::atomic_signal_fence(std::memory_order_acq_rel);
         }
      }
      const double ms_raw =
              std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();

      uint64_t sum_template = 0;
      start = std::chrono::high_resolution_clock::now();
      for (int w = 0; w < num_walks; w++) {
         for (const ocs::tom::Element<ocs::tom::JB> job : typed) {
            sum_template += job.get<JB_job_number>();
            sum_template += job.get<JB_owner>() != nullptr ? 1 : 0;
            job.set<JB_priority>(static_cast<std::uint32_t>(w));
            std::atomic_signal_fence(std::memory_order_acq_rel);
         }
      }
      const double ms_template =
              std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();

      if (sum_raw != sum_template) {
         printf("the two variants read different values: %" PRIu64 " and %" PRIu64 "\n", sum_raw, sum_template);
      }
      printf("%8d %10.1f %10.1f %8.2f\n", round, ms_raw, ms_template, ms_raw / ms_template);
   }
}

/**
 * @brief Measure num_lookups run-time typed reads of one field of one element
 *
 * The read is ocs::tom::get<std::uint32_t>(ep, nm): the field is looked up by
 * its number and its type checked on every call, as lGetUlong() did before
 * CS-3103.
 *
 * Two details are what make this a measurement rather than a compiler quiz,
 * and both were missing until CS-2901:
 *
 * The values read are summed and printed. Without a consumer the loop is dead
 * code, and the compiler is free to remove as much of it as it can see through.
 *
 * Each iteration ends in a compiler barrier. The element and the attribute
 * never change, so an accessor the compiler can look into is loop invariant -
 * it would read the value once, hoist the descriptor lookup, the bounds check
 * and the flag test out of the loop and leave an empty counter behind. That is
 * precisely what happened once LTO became effective (CS-2894): the same binary
 * took 7727 ms without and 2401 ms with LTO, which looked like a 3.2x gain and
 * was removed work. std::atomic_signal_fence emits no instruction; it only
 * forbids moving memory accesses across it, which is enough to keep every call
 * inside the loop while still allowing the call itself to be inlined - so what
 * is measured is the accessor, not what the optimiser can prove about it.
 *
 * @return 0, always - this test measures, it does not assert
 */
int main() {
   ocs::tom::install_model(ocs::tom::gcs_model());

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(job_value);
   const lListElem *elem = job->raw();

   uint64_t sum = 0;
   auto time_start = std::chrono::high_resolution_clock::now();
   for (uint64_t i = 0; i < num_lookups; ++i) {
      sum += ocs::tom::get<std::uint32_t>(elem, JB_job_number);
      std::atomic_signal_fence(std::memory_order_acq_rel);
   }
   auto time_end = std::chrono::high_resolution_clock::now();
   auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);

   printf("%" PRIu64 " get<std::uint32_t>() calls took %.3f s (sum is %" PRIu64 ", expected %" PRIu64 ")\n",
          num_lookups, millisecs.count() / 1000.0, sum, num_lookups * job_value);

   job.reset();

   measure_short_lookups();
   measure_template_path();

   return 0;
}
