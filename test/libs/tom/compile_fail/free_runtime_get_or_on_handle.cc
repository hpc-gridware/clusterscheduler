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
 * @brief Must not compile (CS-3103): the free run-time get_or<V>(h, nm, fallback) on a typed handle, which is deleted
 *
 * A typed handle has no run-time get_or() member; code generic over object
 * types writes get_or<V>(job.raw(), nm, fallback). Without the deletion the
 * call would bind silently to the raw form through the handle's implicit
 * conversion to the element pointer.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: (use of|call to) deleted function .*get_or
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Read a field given at run time from a job handle, or a fallback, with the free form
 * @param job the job
 * @param nm the field number
 * @return never compiles
 */
std::uint32_t wrong_use(const ocs::tom::ConstElement<ocs::tom::JB> &job, const int nm) {
   // get_or<std::uint32_t>(job.raw(), nm, 0) states that the code is generic
   return ocs::tom::get_or<std::uint32_t>(job, nm, 0);
}
