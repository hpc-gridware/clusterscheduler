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
 * @brief PRT-01 (CS-3044): functions whose generated code shows what the template path compiles to
 *
 * Not built by CMake: prt01_codegen.sh compiles this file to assembly with the
 * Release optimisation of the line build, on gcc and on clang, so that the code
 * of the typed getters can be inspected against the old accessor. Each function
 * is one access, so its assembly is the accessor's code.
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "cull/cull_test_api.h"

using namespace ocs::tom;

/**
 * @brief A number field read through a typed handle
 * @param job the job
 * @return the job number
 */
std::uint32_t prt01_typed_number(const ConstElement<JB> job) {
   return job.get<JB_job_number>();
}

/**
 * @brief A string field read through a typed handle
 * @param job the job
 * @return the owner
 */
const char *prt01_typed_string(const ConstElement<JB> job) {
   return job.get<JB_owner>();
}

/**
 * @brief A number field read through an untyped pointer, with the type comparison of N-H-9
 * @param ep the job
 * @return the job number
 */
std::uint32_t prt01_untyped_number(const lListElem *ep) {
   return ocs::tom::get<JB_job_number>(ep);
}

/**
 * @brief A string field read through an untyped pointer
 * @param ep the job
 * @return the owner
 */
const char *prt01_untyped_string(const lListElem *ep) {
   return ocs::tom::get<JB_owner>(ep);
}

/**
 * @brief The old accessor of a number field, for comparison
 * @param ep the job
 * @return the job number
 */
lUlong prt01_old_number(const lListElem *ep) {
   return lGetUlong(ep, JB_job_number);
}

/**
 * @brief The old accessor of a string field, for comparison
 * @param ep the job
 * @return the owner
 */
const char *prt01_old_string(const lListElem *ep) {
   return lGetString(ep, JB_owner);
}
