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
 * @brief Must not compile (RAW-ACC): hand a typed owner to the by-value object setter without a conversion
 *
 * The raw owners take a typed owner only explicitly, so that a typed owner
 * never becomes a temporary raw owner that frees the element when an op
 * throws. The by-value setter set(ep, nm, RawElementPtr) has no typed
 * overload: the caller writes RawElementPtr(std::move(job)) and so states
 * that the ownership passes at the call. The build compiles this file as an
 * expected failure, see expect_compile_failure.cmake; the error must match
 * the line below.
 * expect: no matching|constraints not satisfied|could not match|no viable
 */

#include <utility>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Raw.h"

/**
 * @brief The wrong use
 * @param ep an element of type TEST
 * @param job a typed owner of a job
 */
void wrong_use(lListElem *ep, ocs::tom::ElementPtr<ocs::tom::JB> &&job) {
   ocs::tom::set(ep, TEST_obj_jb, std::move(job));
}
