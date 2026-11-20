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
 * @brief Must not compile (CS-3103, M6): call a function of the old list interface from product code
 *
 * Product code includes cull/cull.h and the typed object model. Neither makes
 * the old function interface visible any more: cull/cull.h holds only the data
 * types, and the old functions are declared in cull/cull_old_api.h, which only
 * libs/cull, libs/tom, libs/sgeobj/tom and the tests (cull/cull_test_api.h)
 * include. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 * expect: was not declared|undeclared identifier
 */

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"

/**
 * @brief The wrong use
 * @return the element, never reached
 */
lListElem *wrong_use() {
   const lDescr *descr = nullptr;
   return lCreateElem(descr);
}
