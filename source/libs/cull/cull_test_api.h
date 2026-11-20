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
 * @brief The old function interface of the list library for tests only
 *
 * The CULL core tests (test/libs/cull) and the module tests that still build
 * their test data with the old interface include this header instead of
 * cull/cull.h. It makes the complete old interface (cull/cull_old_api.h)
 * visible, which product code cannot reach any more since CS-3103 (M6). The
 * tests are converted to the typed object model in a ticket of their own; until
 * then this header is the one exception.
 *
 * No file outside a test/ directory may include it; the build checks this, see
 * test/libs/cull/check_cull_api.cmake.
 */

#include "cull/cull_old_api.h"
