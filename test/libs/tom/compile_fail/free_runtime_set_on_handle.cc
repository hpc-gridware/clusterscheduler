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
 * @brief Must not compile (CS-3103): the free run-time set(h, nm, v) on a typed handle, which is deleted
 *
 * On a typed handle there is one way per operation, the member (here
 * job.set(nm, v)); code generic over object types writes set(job.raw(), nm, v).
 * Without the deletion the call would bind silently to the raw form through the
 * handle's implicit conversion to the element pointer.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: (use of|call to) deleted function .*set
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Set a field given at run time of a job handle with the free form
 * @param job the job
 * @param nm the field number
 */
void wrong_use(const ocs::tom::Element<ocs::tom::JB> &job, const int nm) {
   // the member job.set(nm, std::uint32_t{1}) is the only way on a handle
   ocs::tom::set(job, nm, std::uint32_t{1});
}
