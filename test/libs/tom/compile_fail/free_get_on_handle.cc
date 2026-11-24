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
 * @brief Must not compile (CS-3103): the free get<F>() on a typed handle, which is deleted
 *
 * On a typed handle there is one way per operation, the member. Without the
 * deleted overload the call would bind silently to the raw form through the
 * handle's implicit conversion to ocs::tom::ElementData *.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: deleted|use the member
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief Read the job number of a job handle with the free form
 * @param job the job
 * @return never compiles
 */
unsigned wrong_use(const ocs::tom::Element<ocs::tom::JB> &job) {
   // the member job.get<JB_job_number>() is the only way on a handle
   return ocs::tom::get<JB_job_number>(job);
}
