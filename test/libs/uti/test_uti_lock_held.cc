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
 * @brief Module test of the per thread lock bookkeeping (CS-3011)
 *
 * The bookkeeping exists only with `ENABLE_DEBUG_CHECKS`, which every Debug
 * build sets. In other builds every scenario reports itself as unsupported.
 */

#include <thread>

#include "uti/sge_lock.h"

#include "ocs_module_test.h"

#if defined(ENABLE_DEBUG_CHECKS)
/**
 * @brief Whether the calling thread holds a lock, as the pair (any mode, for writing)
 *
 * @param type the lock
 * @param any what ocs_lock_is_held() must say
 * @param write what ocs_lock_is_held_for_write() must say
 * @return true when both queries give the expected answer
 */
static bool
held(sge_locktype_t type, bool any, bool write) {
   return ocs_lock_is_held(type) == any && ocs_lock_is_held_for_write(type) == write;
}
#endif

/**
 * @brief Taking and releasing a lock is seen by the queries, nesting included
 *
 * @return nothing; CHECK records the results
 */
static void
test_lock_and_unlock() {
#if defined(ENABLE_DEBUG_CHECKS)
   const bool before = held(LOCK_GLOBAL, false, false) && held(LOCK_READER, false, false);

   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
   const bool read = held(LOCK_GLOBAL, true, false) && held(LOCK_READER, false, false);
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   const bool nested = held(LOCK_GLOBAL, true, false);
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   const bool released = held(LOCK_GLOBAL, false, false);

   SGE_LOCK(LOCK_READER, LOCK_WRITE);
   const bool write = held(LOCK_READER, true, true) && held(LOCK_GLOBAL, false, false);
   SGE_UNLOCK(LOCK_READER, LOCK_WRITE);
   const bool write_released = held(LOCK_READER, false, false);

   CHECK(1, "no lock is held before one is taken", before);
   CHECK(2, "a read lock is held for reading, and only that lock",
                           read);
   CHECK(3, "a nested read lock is held until the last release", nested && released);
   CHECK(4, "a write lock is held for writing, and is free again after the release",
                           write && write_released);
#else
   ocs::module_test::unsupported("the lock bookkeeping exists only with ENABLE_DEBUG_CHECKS");
#endif
}

/**
 * @brief A successful try lock is booked, a failed one is not
 *
 * @return nothing; CHECK records the results
 */
static void
test_try_lock() {
#if defined(ENABLE_DEBUG_CHECKS)
   const bool taken = SGE_TRY_LOCK(LOCK_LISTENER, LOCK_WRITE);
   const bool booked = held(LOCK_LISTENER, true, true);

   // another thread cannot take the lock now, and its failed attempt books nothing there
   bool other_taken = true;
   bool other_booked = true;
   std::thread other([&other_taken, &other_booked]() {
      other_taken = SGE_TRY_LOCK(LOCK_LISTENER, LOCK_READ);
      other_booked = ocs_lock_is_held(LOCK_LISTENER);
      if (other_taken) {
         SGE_UNLOCK(LOCK_LISTENER, LOCK_READ);
      }
   });
   other.join();

   SGE_UNLOCK(LOCK_LISTENER, LOCK_WRITE);

   CHECK(5, "a successful try lock is held", taken && booked);
   CHECK(6, "a failed try lock is not held", !other_taken && !other_booked);
#else
   ocs::module_test::unsupported("the lock bookkeeping exists only with ENABLE_DEBUG_CHECKS");
#endif
}

/**
 * @brief The bookkeeping is per thread: a lock held by one thread is not held by another
 *
 * @return nothing; CHECK records the results
 */
static void
test_per_thread() {
#if defined(ENABLE_DEBUG_CHECKS)
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   bool other_sees = true;
   bool other_own = false;
   std::thread other([&other_sees, &other_own]() {
      other_sees = ocs_lock_is_held(LOCK_GLOBAL);
      SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
      other_own = ocs_lock_is_held(LOCK_GLOBAL);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   });
   other.join();

   const bool still_held = ocs_lock_is_held(LOCK_GLOBAL);
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   CHECK(7, "another thread does not hold the lock this thread holds", !other_sees);
   CHECK(8, "another thread's own lock and release leave this thread's lock held",
                           other_own && still_held);
#else
   ocs::module_test::unsupported("the lock bookkeeping exists only with ENABLE_DEBUG_CHECKS");
#endif
}

int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"LOCKHELD-01", "taking and releasing a lock",
       "the debug bookkeeping knows which locks the calling thread holds and in which mode",
       {"take a read lock, nest it, release it twice", "take a write lock, release it"},
       "the read lock is held for reading until its last release, the write lock for writing until its "
       "release; no other lock is held meanwhile",
       {{"LCK-01", "covers"}}, {1, 2, 3, 4}, test_lock_and_unlock},
      {"LOCKHELD-02", "try lock",
       "a lock taken with a try lock is booked like one taken by blocking, a failed attempt is not",
       {"take a write lock with a try lock", "try to take it for reading in another thread"},
       "the first is held, the second fails and books nothing",
       {{"LCK-02", "covers"}}, {5, 6}, test_try_lock},
      {"LOCKHELD-03", "one thread's locks are not another's",
       "the bookkeeping is thread local",
       {"take a read lock", "ask in another thread, then take and release it there"},
       "the other thread does not hold it until it takes it itself, and its release leaves the first "
       "thread's lock held",
       {{"LCK-03", "covers"}}, {7, 8}, test_per_thread},
   });
}
