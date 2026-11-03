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
 * @brief Module test of the available CPU count and the reader thread count derived from it (CS-3009)
 */

#include <thread>

#if defined(LINUX)
#include <sched.h>
#endif

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_nprocs.h"

#include "ocs_module_test.h"

/**
 * @brief `reader_threads auto` gives half the CPUs, at least 4 and at most 32
 *
 * @return nothing; CHECK records the results
 */
static void
test_reader_threads_for_cpus() {
   using ocs::Bootstrap;
   CHECK(1, "up to 9 CPUs give the minimum of 4 reader threads",
         Bootstrap::reader_threads_for_cpus(1) == 4 && Bootstrap::reader_threads_for_cpus(8) == 4
         && Bootstrap::reader_threads_for_cpus(9) == 4);
   CHECK(2, "between the bounds the count is half the CPUs, rounded down",
         Bootstrap::reader_threads_for_cpus(10) == 5 && Bootstrap::reader_threads_for_cpus(16) == 8
         && Bootstrap::reader_threads_for_cpus(33) == 16);
   CHECK(3, "64 and more CPUs give the maximum of 32 reader threads",
         Bootstrap::reader_threads_for_cpus(64) == 32 && Bootstrap::reader_threads_for_cpus(200) == 32);
}

/**
 * @brief The available CPUs are at least one and never more than the machine has
 *
 * @return nothing; CHECK records the results
 */
static void
test_available_cpus() {
   const int available = ocs_available_cpus();
   CHECK(4, "at least one CPU is available, and no more than the machine has",
         available >= 1 && available <= sge_nprocs());
}

/**
 * @brief The available CPUs follow the CPU affinity of the calling thread
 *
 * Restricts a thread of its own to the first CPUs of the current affinity, so the rest of the test keeps its
 * affinity.
 *
 * @return nothing; CHECK records the results
 */
static void
test_affinity() {
#if defined(LINUX)
   const int before = ocs_available_cpus();
   if (before < 2) {
      ocs::module_test::unsupported("the affinity test needs at least two available CPUs");
      return;
   }

   bool restricted = false;
   int with_two = 0;
   int with_one = 0;
   std::thread other([&restricted, &with_two, &with_one]() {
      cpu_set_t all;
      CPU_ZERO(&all);
      if (sched_getaffinity(0, sizeof(all), &all) != 0) {
         return;
      }
      cpu_set_t two;
      CPU_ZERO(&two);
      int taken = 0;
      for (int cpu = 0; cpu < CPU_SETSIZE && taken < 2; cpu++) {
         if (CPU_ISSET(cpu, &all)) {
            CPU_SET(cpu, &two);
            taken++;
         }
      }
      if (sched_setaffinity(0, sizeof(two), &two) != 0) {
         return;
      }
      with_two = ocs_available_cpus();

      cpu_set_t one;
      CPU_ZERO(&one);
      for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
         if (CPU_ISSET(cpu, &two)) {
            CPU_SET(cpu, &one);
            break;
         }
      }
      restricted = sched_setaffinity(0, sizeof(one), &one) == 0;
      with_one = ocs_available_cpus();
   });
   other.join();

   CHECK(5, "a thread restricted to two CPUs has two available", restricted && with_two == 2);
   CHECK(6, "a thread restricted to one CPU has one available", restricted && with_one == 1);
   CHECK(7, "the restriction of another thread leaves this thread's count unchanged",
         ocs_available_cpus() == before);
#else
   ocs::module_test::unsupported("the CPU affinity is read on Linux only");
#endif
}

int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"CPUS-01", "reader threads for a number of CPUs",
       "reader_threads auto derives half the available CPUs as reader threads, bounded by 4 and 32",
       {"derive the count for 1, 8, 9, 10, 16, 33, 64 and 200 CPUs"},
       "4 up to 9 CPUs, half the CPUs between the bounds, 32 from 64 CPUs on",
       {}, {1, 2, 3}, test_reader_threads_for_cpus},
      {"CPUS-02", "available CPUs",
       "the count of available CPUs is plausible for the machine",
       {"ask for the available CPUs"},
       "at least 1 and at most the number of CPUs of the machine",
       {}, {4}, test_available_cpus},
      {"CPUS-03", "available CPUs follow the CPU affinity",
       "a thread whose CPU affinity is restricted sees only the CPUs it may run on",
       {"restrict a thread to two CPUs and ask", "restrict it to one CPU and ask",
        "ask in the main thread"},
       "two, then one CPU in the restricted thread; the main thread's count is unchanged",
       {}, {5, 6, 7}, test_affinity},
   });
}
