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
 * @brief Must not compile (OPS-02): a changing forwarding member through a constant element handle
 *
 * or_bits<F>() changes the element, so it is a member of Element only, as the free form takes a mutable
 * handle; ConstElement has no such member. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: has no member named|no member named
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief Set a bit of an array task's status through a constant handle
 * @param task the array task
 * @return 0 on success
 */
int wrong_use(const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   // the status bits can be set only through a mutable handle
   return task.or_bits<JAT_status>(0x2);
}
