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

#include "ocs_DebugParam.h"
#include "sge_stdlib.h"

/** @brief Get thread name pattern from environment variable SGE_DEBUG_THREAD_NAME_PATTERN
 *
 *  The pattern is only read once and cached for subsequent calls.
 *
 *  @return thread name pattern or nullptr if not set
 */
const char *
ocs::DebugParam::get_thread_name_pattern() {
   static const char *thread_name_pattern = nullptr;

   if (thread_name_pattern == nullptr) {
      thread_name_pattern = sge_getenv("SGE_DEBUG_THREAD_NAME_PATTERN");
   }

   return thread_name_pattern;
}