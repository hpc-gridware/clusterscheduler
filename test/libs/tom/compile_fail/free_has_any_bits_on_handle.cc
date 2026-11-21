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
 * @brief Must not compile (CS-3103): the free has_any_bits() on a typed handle, which is deleted
 *
 * On a typed handle there is one way per operation, the member (here
 * task.has_any_bits<F>(mask)). Without the deletion the call would bind silently to the form on
 * the raw pointer through the handle's implicit conversion to the element
 * pointer, which checks the element's type at run time only.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: (use of|call to) deleted function .*has_any_bits
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Test a status bit of an array task handle with the free form
 * @param task the array task
 * @return never compiles
 */
bool wrong_use(const ocs::tom::ConstElement<ocs::tom::JAT> &task) {
   // the member task.has_any_bits<F>(mask) is the only way on a handle
   return ocs::tom::has_any_bits<JAT_status>(task, 1);
}
