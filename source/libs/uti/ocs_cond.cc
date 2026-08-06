/*___INFO__MARK_BEGIN_NEW__*/
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
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Implementation of the condition variable helpers
 */

#include <time.h>

#include "ocs_cond.h"

namespace ocs::uti {
   /**
    * @brief Initialize a condition variable
    *
    * Sets the clock attribute to `CLOCK_MONOTONIC`, so timeouts are unaffected
    * by changes to the system clock.
    *
    * @param condition the condition variable to initialize
    * @return 0 on success, otherwise the error code from `pthread_cond_init()`
    */
   int condition_initialize(pthread_cond_t *condition) {
      int ret{0};

#ifdef DARWIN
      ret = pthread_cond_init(condition, nullptr);
#else
      pthread_condattr_t attr;
      pthread_condattr_init(&attr);
      pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

      ret = pthread_cond_init(condition, &attr);
#endif

      return ret;
   }

   /**
    * @brief Wait on a condition variable with a relative timeout
    *
    * The timeout is relative to now and measured against `CLOCK_MONOTONIC`, so
    * the caller does not have to build an absolute deadline and the wait is
    * unaffected by system clock changes. @p mutex must be held on entry and is
    * held again on return.
    *
    * @param condition the condition variable to wait on
    * @param mutex the mutex associated with @p condition
    * @param timeout_sec seconds to wait
    * @param timeout_usec additional microseconds to wait
    * @return 0 when the condition was signalled, otherwise the error code from
    *         `pthread_cond_timedwait()` — `ETIMEDOUT` when the wait expired
    */
   int condition_timedwait(pthread_cond_t *condition, pthread_mutex_t *mutex, long timeout_sec, long timeout_usec) {
      int ret{0};

      timespec ts{};
      clock_gettime(CLOCK_MONOTONIC, &ts);
      ts.tv_sec += timeout_sec;
      ts.tv_nsec += timeout_usec * 1000;
      if (ts.tv_nsec >= 1000000000) {
         ts.tv_sec += ts.tv_nsec / 1000000000;
         ts.tv_nsec %= 1000000000;
      }
      ret = pthread_cond_timedwait(condition, mutex, &ts);

      return ret;
   }
} // namespace
