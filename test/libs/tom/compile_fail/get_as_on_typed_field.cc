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
 * @brief Must not compile (CS-3103): read a field whose sub-type the model names with get_as()
 *
 * get_as() exists for list and object fields without a sub-type only; a field with a sub-type gives its typed
 * handle from get<F>() and needs no check at run time. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 * expect: no matching|constraints not satisfied|could not match|no viable
 */

#include <cstddef>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief The wrong use
 * @param job a job
 * @return the number of environment variables
 */
std::size_t wrong_use(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job.get_as<JB_env_list, ocs::tom::VA>().size();
}
