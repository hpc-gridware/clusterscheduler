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
 * @brief Must not compile (RAW-ACC): read a mutable list handle through a read-only raw handle
 *
 * The method of ConstRawElement takes the value types of a read-only element
 * only, as the free form does. The build compiles this file as an expected
 * failure, see expect_compile_failure.cmake; the error must match the line
 * below.
 * expect: no matching|constraints not satisfied|could not match
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Raw.h"

/**
 * @brief The wrong use
 * @param ep a job
 * @return the number of elements of the job's environment
 */
std::size_t wrong_use(const lListElem *ep) {
   const ocs::tom::ConstRawElement job(ep);
   const ocs::tom::RawList env = job.get<ocs::tom::RawList>(JB_env_list);
   return env.size();
}
