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
 * @brief Must not compile (CS-3103): the free find_first_rw<F>() on a read-only typed list, which is deleted
 *
 * Through the implicit conversion to const ocs::tom::ListData * the call would hand out a mutable element of a list
 * held
 * read-only. On a typed list there is one way per operation, the member: find_first<F>() gives a ConstElement on
 * a ConstList and an Element on a List.
 *
 * The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: (use of|call to) deleted function .*find_first_rw
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_List.h"
#include "tom/ocs_FreeAccess.h"

/**
 * @brief A mutable job out of a read-only job list, with the free form
 * @param jobs the job list
 * @return never compiles
 */
ocs::tom::Element<ocs::tom::JB> wrong_use(const ocs::tom::ConstList<ocs::tom::JB> jobs) {
   // jobs.find_first<JB_job_number>(1) is the only way on a typed list, and it gives a ConstElement
   return ocs::tom::find_first_rw<JB_job_number>(jobs, 1);
}
