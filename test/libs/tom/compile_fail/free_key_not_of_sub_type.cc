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
 * @brief Must not compile (OPS-02): a free lookup in a sub-list by a field of another type than the list holds
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: no match|no matching|no viable|constraints not satisfied|not satisfied|cannot convert|could not convert|invalid conversion
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief A lookup in the environment of a job by an array task field
 * @param job the job
 * @return the element
 */
const lListElem *wrong_use(const lListElem *job) {
   // JB_env_list holds VA elements; lGetSubUlong(job, JAT_task_number, 1, JB_env_list) compiled
   return ocs::tom::find_first_in<JB_env_list, JAT_task_number>(job, 1);
}
