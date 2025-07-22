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

#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_profiling.h"

#include "execd_profiling.h"

namespace ocs {
   void execd_profiling_initialize() {
      prof_set_level_name(SGE_PROF_CUSTOM1, "dispatcher", nullptr);
      prof_set_level_name(SGE_PROF_CUSTOM2, "systemd", nullptr);
      prof_set_level_name(SGE_PROF_CUSTOM3, "ptf/pdc", nullptr);
   }

   void execd_profiling_start_stop() {
      static bool profiling_started = false;
      // start / stop profiling depending on configuration (execd_params PROF_EXECD=true)
      if (profiling_started) {
         if (!thread_prof_active_by_id(pthread_self())) {
            prof_stop(SGE_PROF_CUSTOM1, nullptr);
            prof_stop(SGE_PROF_CUSTOM2, nullptr);
            prof_stop(SGE_PROF_CUSTOM3, nullptr);
            prof_stop(SGE_PROF_GDI_REQUEST, nullptr);
            profiling_started = false;
            DEBUG("profiling disabled");
         }
      } else {
         if (thread_prof_active_by_id(pthread_self())) {
            prof_start(SGE_PROF_CUSTOM1, nullptr);
            prof_start(SGE_PROF_CUSTOM2, nullptr);
            prof_start(SGE_PROF_CUSTOM3, nullptr);
            prof_start(SGE_PROF_GDI_REQUEST, nullptr);
            profiling_started = true;
            DEBUG("profiling enabled");
         }
      }
   }

   void execd_profiling_output() {
      static u_long64 next_prof_output = 0;
      if (thread_prof_active_by_id(pthread_self())) {
         thread_output_profiling("execd profiling summary:", &next_prof_output);
      }
   }

   void execd_profiling_cleanup() {
      sge_prof_cleanup();
   }
}
