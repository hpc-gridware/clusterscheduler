/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Reads the debug settings from the environment
 */

#include "ocs_DebugParam.h"

#include <cstdlib>

/**
 * @brief Which threads should produce debug output
 *
 * The pattern is read from `SGE_DEBUG_THREAD_NAME_PATTERN` once and cached. The trace output calls this
 * function, so it reads the environment with getenv() and not with a traced helper like sge_getenv(),
 * which would come back here before the value is cached.
 *
 * @return the pattern from the environment, or nullptr when unset;
 *         owned by the environment, do not free
 */
const char *
ocs::DebugParam::get_thread_name_pattern() {
   static const char *const thread_name_pattern = std::getenv("SGE_DEBUG_THREAD_NAME_PATTERN");
   return thread_name_pattern;
}

/**
 * @brief Is this component running in no-daemon mode?
 *
 * `SGE_ND` is read once and the result cached. Like get_thread_name_pattern(), the environment is read
 * with getenv(), as the debug infrastructure must not use traced helpers.
 *
 * @return true when the component was asked to stay in the foreground
 */
bool ocs::DebugParam::is_component_in_nd_mode() {
   static const bool in_nd_mode = std::getenv("SGE_ND") != nullptr;
   return in_nd_mode;
}
