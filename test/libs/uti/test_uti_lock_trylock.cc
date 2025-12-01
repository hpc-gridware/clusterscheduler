/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include <random>

#include <unistd.h>
#include <cstdio>

#include "test_uti_lock_main.h"

#include "uti/sge_lock.h"
#include "uti/sge_time.h"
#include "uti/ocs_TerminationManager.h"

//#define THREAD_COUNT 64
//#define THREAD_RUN_TIME 120
#define THREAD_COUNT 8
#define THREAD_RUN_TIME 60

static int thread_count;
#if 0
static u_long32 maxlocks;
#endif
static u_long32 results[THREAD_COUNT];

static void *thread_function(void *anArg);

void set_thread_count(int count) {
   thread_count = 0;
}

int get_thread_demand() {
   return THREAD_COUNT;
}

void *(*get_thread_func())(void *anArg) {
   return thread_function;
}

void *get_thread_func_arg() {
   return nullptr;
}

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
long lock_counter = 0;

void incr_counter() {
   pthread_mutex_lock(&mutex_lock);
   lock_counter++;
   pthread_mutex_unlock(&mutex_lock);
}

void decr_counter() {
   pthread_mutex_lock(&mutex_lock);
   lock_counter--;
   pthread_mutex_unlock(&mutex_lock);
}

int getRandomNumber(const int min = 20, const int max = 1000) {
    std::random_device rd; // Seed for the random number engine
    std::mt19937 gen(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<> dis(min, max); // Define the range

    return dis(gen); // Generate and return the random number
}

static void *thread_function(void *anArg) {
   const u_long64 start = sge_get_gmt64();
   u_long32 count = 0;
   const int thread_id = thread_count++;
   bool read_thread = true;
   constexpr bool do_loop = true;

   DENTER(TOP_LAYER);

   while (do_loop) {
      // first thread is the write thread
      if (thread_id == 0) {
         read_thread = false;

         // try to get the write lock every 20ms for up to 1000ms, then enforce to get the lock
         constexpr long wait_time = 20000;
         constexpr long max_wait_time = 1000 * 1000;
         long remaining_wait_time = max_wait_time;
         bool do_try_lock = (max_wait_time > wait_time);
         bool got_lock = false;
         while (!got_lock) {
            if (do_try_lock) {
               got_lock = SGE_TRY_LOCK(LOCK_GLOBAL, LOCK_WRITE);
            } else {
               SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE);
               got_lock = true;
            }

            if (!got_lock) {
               remaining_wait_time -= wait_time;
               if (remaining_wait_time <= 0) {
                  sge_usleep(wait_time);
                  do_try_lock = false;
               }
            }
         }

         // got the lock, increment the counter and release the lock
         count++;
         incr_counter();
         usleep(getRandomNumber(10, 500));
         if (lock_counter != 1) {
            printf("error: lock_counter in worker should be exactly 1 but it is %ld\n", lock_counter);
            ocs::TerminationManager::trigger_abort();
         }
         decr_counter();
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      } else {
         SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
         count++;
         incr_counter();
         usleep(getRandomNumber(10, 1000));
         if (lock_counter < 1 || lock_counter > THREAD_COUNT - 1) {
            printf("error: lock_counter in reader should be bigger that 1 but not bigger than the amount of reader threads (%d) but it is %ld\n", THREAD_COUNT - 1, lock_counter);
            ocs::TerminationManager::trigger_abort();
         }
         decr_counter();
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
      }

      if (sge_get_gmt64() - start >= sge_gmt32_to_gmt64(THREAD_RUN_TIME)) {
         break;
      }
   }

   results[thread_id] = count;

   printf("%s thread %d got " sge_u32 " times the lock\n", read_thread ? "read" : "write", thread_id, count);

   DRETURN(nullptr);
}

int is_in_tolerance(u_long32 value1, u_long32 value2, u_int accepted_tolerance) {
   return 0;
}

int validate([[maybe_unused]]const int count) {
   return 0;
}
