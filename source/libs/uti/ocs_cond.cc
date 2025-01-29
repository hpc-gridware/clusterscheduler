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

#include <time.h>

#include "ocs_cond.h"

namespace ocs::uti {
   /****** ocs_cond/condition_initialize() ***************************************
   *  \brief Initialize a condition variable.
   *
   *  \details
   *  This function initializes a condition variable with a monotonic clock attribute.
   *  It sets the clock attribute to CLOCK\_MONOTONIC to ensure that the condition
   *  variable uses a monotonic clock for timeouts.
   *
   *  \param condition  Pointer to the condition variable to be initialized.
   *
   *  \return
   *  \li 0 - Success
   *  \li Non-zero - Error code returned by pthread\_cond\_init
   ******************************************************************************/
   int condition_initialize(pthread_cond_t *condition) {
      int ret{0};

      pthread_condattr_t attr;
      pthread_condattr_init(&attr);
      pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

      ret = pthread_cond_init(condition, &attr);

      return ret;
   }

   /****** ocs_cond/condition_timedwait() ****************************************
   *  \brief Wait for a condition variable with a timeout.
   *
   *  \details
   *  This function waits for a condition variable to be signaled or for a timeout
   *  to occur. It uses a monotonic clock to calculate the timeout period.
   *
   *  \param[in] condition  Pointer to the condition variable to wait on.
   *  \param[in] mutex      Pointer to the mutex that is associated with the condition variable.
   *  \param[in] timeout_sec  Timeout period in seconds.
   *  \param[in] timeout_usec Timeout period in microseconds.
   *
   *  \return
   *  \li 0 - Success
   *  \li Non-zero - Error code returned by pthread\_cond\_timedwait
   ******************************************************************************/
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
