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
 * @brief Must not compile (OPS-FREE): merge_by_key<Key, Value> with a key that is no key-capable field
 *
 * merge_by_key<Key, Value>() takes the key types find_first<F>() takes: 32 and
 * 64 bit numbers, strings and host names. A double compared for equality is no
 * key. The build compiles this file as an expected failure, see
 * expect_compile_failure.cmake; the error must match the line below.
 *
 * expect: no match|no matching|no viable|constraints not satisfied|not satisfied
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_List.h"

/**
 * @brief Merge two lists of the test type by its double field
 * @param dst the list merged into
 * @param src the list merged from
 */
void wrong_use(const ocs::tom::List<ocs::tom::TEST> dst, const ocs::tom::ConstList<ocs::tom::TEST> src) {
   ocs::tom::merge_by_key<TEST_double, TEST_string>(dst, src);
}
