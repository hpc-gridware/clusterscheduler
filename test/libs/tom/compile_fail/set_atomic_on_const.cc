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
 * @brief Must not compile (OPS-ATOM): an atomic store through a constant handle
 *
 * set_atomic<F>() takes a mutable handle, as set<F>() does; a ConstElement does
 * not convert to an Element. The build compiles this file as an expected
 * failure, see expect_compile_failure.cmake; the error must match the line
 * below.
 *
 * expect: no match|no matching|no viable|constraints not satisfied|not satisfied
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief An atomic store into the not hashed 64 bit field of the match cache through a constant handle
 * @param ep the element
 * @return 0 on success
 */
int wrong_use(const ocs::tom::ConstElement<ocs::tom::HM> ep) {
   // HM_last_used is atomically writable, but only through a mutable handle
   return ocs::tom::set_atomic<HM_last_used>(ep, 1);
}
