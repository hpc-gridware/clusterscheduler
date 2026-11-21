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
 * @brief Must not compile (CS-3103): the free has<F, true>() on a typed handle, once a way past the deletion
 *
 * On a typed handle there is one way per operation, the member job.has<F>().
 * The free has<F>() had a second template parameter for the internal callers;
 * with it given, the deleted overload for a handle was not viable and the call
 * bound to the raw form through the handle's implicit conversion. The
 * parameter is gone, so the call finds no function at all.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: no matching function
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Test a field of a job handle with the free form and the former second template argument
 * @param job the job
 * @return never compiles
 */
bool wrong_use(const ocs::tom::Element<ocs::tom::JB> &job) {
   // the free has<F>() has no second template parameter any more; job.has<JB_job_number>() is the way
   return ocs::tom::has<JB_job_number, true>(job);
}
