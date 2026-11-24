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
 * @brief Must not compile (RAW-ACC): set a field given at run time from a std::string
 *
 * The run-time setters take one of the nine plain value types only, so a
 * value is never converted implicitly; a std::string must be passed as
 * c_str(). The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 * expect: no matching|constraints not satisfied|could not match
 */

#include <string>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Raw.h"

/**
 * @brief The wrong use
 * @param ep an element of type TEST
 */
void wrong_use(ocs::tom::ElementData *ep) {
   const std::string text = "text";
   ocs::tom::set(ep, TEST_string, text);
}
