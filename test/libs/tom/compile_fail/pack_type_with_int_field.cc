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
 * @brief Must not compile (OPS-PACK): the full pack of a type with an Int32 or Int64 field
 *
 * The pack format cannot carry Int32 and Int64 fields; pack_value() failed
 * with PACK_FORMAT at run time. The full pack of a type holding one does not
 * compile; a selection without the field packs (pack(pb, what)). The build
 * compiles this file as an expected failure, see expect_compile_failure.cmake;
 * the error must match the line below.
 *
 * expect: cannot be packed|static assertion|static_assert
 */

#include "tom/ocs_Types.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

/**
 * @brief Pack an element of the test type, which has the Int32 field TEST_int and the Int64 field TEST_long
 * @param ep the element
 * @param pb the buffer
 */
void wrong_use(const ocs::tom::ConstElement<ocs::tom::TEST> ep, sge_pack_buffer &pb) {
   ep.pack(pb);
}
