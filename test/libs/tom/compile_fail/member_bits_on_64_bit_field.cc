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
 * @brief Must not compile (OPS-02): a forwarding member of an element handle with a field of the wrong type
 *
 * The member has_any_bits<F>() takes a 32 bit field of the handle's type, as the free form does; a 64 bit
 * field fails the requires-clause of the member. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: no match|no matching|no viable|constraints not satisfied|not satisfied
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief A bit mask test on a 64 bit field through the member form
 * @param ep the element
 * @return whether a bit is set
 */
bool wrong_use(const ocs::tom::ConstElement<ocs::tom::TEST> ep) {
   // the bit mask members work on 32 bit fields only
   return ep.has_any_bits<TEST_uint64>(1);
}
