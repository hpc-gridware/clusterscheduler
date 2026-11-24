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
 * @brief Must not compile (CS-3103): the free find_first_in_rw() on a typed handle, which is deleted
 *
 * On a typed handle there is one way per operation, the member (here
 * job.find_first_in_rw<LF, F>(key)). Without the deletion the call would bind silently to the form on
 * the raw pointer through the handle's implicit conversion to the element
 * pointer, which checks the element's type at run time only.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: (use of|call to) deleted function .*find_first_in_rw
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Look up an array task of a job handle with the free form
 * @param job the job
 * @return never compiles
 */
ocs::tom::ElementData *wrong_use(const ocs::tom::ConstElement<ocs::tom::JB> &job) {
   // the member job.find_first_in_rw<LF, F>(key) is the only way on a handle
   return ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(job, 1);
}
