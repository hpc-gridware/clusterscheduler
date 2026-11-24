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
 * @brief Must not compile (CS-3103): the free list_ref<F, true>() on a typed handle, once a way past the type check
 *
 * On a typed handle there is one way per operation, the member
 * job.list_ref<F>(). The free forms on a raw pointer had a second template
 * parameter for the internal callers that skipped the run-time type check; with
 * it given, the call bound to the raw form through the handle's implicit
 * conversion and read the slot of a job field in a queue. The parameter is
 * gone, so the call finds no function at all.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: no matching function|too many template arguments|wrong number of template arguments
 */

#include <cstdint>

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/**
 * @brief Take the reference to a job list field of a queue handle with the former second template argument
 * @param queue the queue
 * @return never compiles
 */
ocs::tom::ListData **wrong_use(const ocs::tom::ConstElement<ocs::tom::QU> &queue) {
   // list_ref<F>() has no second template parameter any more; JB_env_list is no queue field either
   return ocs::tom::list_ref<JB_env_list, true>(queue);
}
