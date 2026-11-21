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
 * @brief Must not compile (CS-3103): the free has<F>() on a typed handle, which is deleted
 *
 * On a typed handle there is one way per operation, the member. Element derives from ConstElement, so the deleted ConstElement overload is found through the base class.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: deleted
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_FreeAccess.h"

/**
 * @brief Whether a job handle holds a field, with the free form
 * @param job the job
 * @return never compiles
 */
bool wrong_use(const ocs::tom::Element<ocs::tom::JB> &job) {
   // the member job.has<JB_job_number>() is the only way on a handle
   return ocs::tom::has<JB_job_number>(job);
}
