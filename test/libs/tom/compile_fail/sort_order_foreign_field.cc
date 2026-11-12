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
 * @brief Must not compile (OPS-01): a sort order on fields of two types
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 * expect: no match|no matching|no viable|constraints not satisfied|cannot convert|could not convert|invalid initialization|invalid conversion
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief The wrong use
 */
void wrong_use() {
   // the first criterion names jobs, the second a user set field
   static_cast<void>(ocs::tom::sort_order<ocs::tom::desc<JB_priority>, ocs::tom::asc<US_name>>());
}
