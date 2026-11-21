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
 * @brief Must not compile (TRT): a field table that declares Hash on a field the hash tables cannot hold
 *
 * The hash tables of the list library hold 32 and 64 bit numbers, strings and
 * host names only; make_descriptor() refuses Hash or Unique on any other field
 * type at compile time, where ocs::tom::detail::hash_create() logged an error at run time
 * and built no table. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: constant expression|Hash or Unique
 */

#include "tom/ocs_FieldTraits.h"

/** @brief The field numbers of a type made up for this test */
enum BAD_Field {
   BAD_flag = 1   ///< a bool field declared hashed
};

/** @brief A type whose only field is a hashed bool */
struct BAD {
   /** @brief The fields: a bool declared hashed */
   static constexpr ocs::tom::FieldInfo fields[] = {
      {BAD_flag, "BAD_flag", ocs::tom::FieldType::Bool, ocs::tom::Flag::Hash},
   };

   /** @brief The descriptor: does not compile */
   static constexpr auto descriptor = ocs::tom::make_descriptor(fields);
};
