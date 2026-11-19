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
 * @brief Must not compile (OPS-ATOM): an atomic read of a 32 bit field
 *
 * get_atomic<F>() reads 64 bit unsigned fields only, as lGetUlong64Atomic()
 * did; a 32 bit field cannot tear and has no atomic accessor. The build
 * compiles this file as an expected failure, see expect_compile_failure.cmake;
 * the error must match the line below.
 *
 * expect: no match|no matching|no viable|constraints not satisfied|not satisfied
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief An atomic read of the 32 bit field of the test type
 * @param ep the element
 * @return the value
 */
std::uint64_t wrong_use(const ocs::tom::ConstElement<ocs::tom::TEST> ep) {
   // TEST_uint32 is a 32 bit field
   return ocs::tom::get_atomic<TEST_uint32>(ep);
}
