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
 * @brief Performance test for the CULL attribute accessors in `libs/cull`
 */

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>

#include "sgeobj/sge_job.h"

/// how often the accessor is called; keeps the test in the range of seconds
static constexpr uint64_t num_lookups = 1000000000;

/// the value the attribute is set to, so the printed sum is predictable
static constexpr lUlong job_value = 42;

/** @brief Measure num_lookups calls of lGetUlong() on one element
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
   lListElem *elem = lCreateElem(JB_Type);
   lSetUlong(elem, JB_job_number, job_value);

   uint64_t sum = 0;
   auto time_start = std::chrono::high_resolution_clock::now();
   for (uint64_t i = 0; i < num_lookups; ++i) {
      sum += lGetUlong(elem, JB_job_number);
      std::atomic_signal_fence(std::memory_order_acq_rel);
   }
   auto time_end = std::chrono::high_resolution_clock::now();
   auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);

   printf("%" PRIu64 " lGetUlong() calls took %.3f s (sum is %" PRIu64 ", expected %" PRIu64 ")\n",
          num_lookups, millisecs.count() / 1000.0, sum, num_lookups * job_value);

   lFreeElem(&elem);

   return 0;
}
