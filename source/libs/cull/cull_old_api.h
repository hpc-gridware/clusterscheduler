#pragma once
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
 * @brief The old function interface of the list library, complete; not public
 *
 * Everything the umbrella header cull/cull.h pulled in before CS-3103 (M6): the
 * list and element functions with list walking (lFirst(), lNext(), the
 * for_each_* macros), field access, where/what, sort, dump, pack of objects,
 * the pack buffer primitives (cull/pack.h, tom/pack.h), hashing and the cull
 * state. Product code includes cull/cull.h instead, which keeps only the data
 * types; objects are reached through the typed object model and the raw API
 * (tom/ocs_*.h), packbuffers through tom/ocs_PackCore.h.
 *
 * Included only by libs/cull and libs/tom, which implement the old interface,
 * and by the tests through cull/cull_test_api.h (libs/sgeobj/tom may include
 * it as well). The build checks this, see test/libs/cull/check_cull_api.cmake.
 */

#include "cull/cull_listP.h"
#include "cull/cull_list.h"
#include "cull/cull_where.h"
#include "cull/cull_what.h"
#include "cull/cull_what_elem.h"
#include "cull/cull_what_print.h"
#include "cull/cull_db.h"
#include "cull/cull_multitype.h"
#include "cull/cull_dump_scan.h"
#include "cull/cull_lerrno.h"
#include "cull/cull_pack.h"
#include "cull/cull_sort.h"
#include "cull/cull_tree.h"
#include "cull/cull_hash.h"
#include "cull/cull_state.h"
#include "cull/pack.h"
