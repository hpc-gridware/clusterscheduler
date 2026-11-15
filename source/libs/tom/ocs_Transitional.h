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
 * @brief The marker of a transitional overload (N-X-2)
 *
 * A function converted to typed handles keeps its old signature as a thin
 * wrapper as long as an untyped caller is left. The declaration of such a
 * wrapper carries `OCS_TRANSITIONAL`:
 *
 * ```cpp
 * int nslots_granted(ocs::tom::ConstList<ocs::tom::JG> granted, const char *qhostname);
 * OCS_TRANSITIONAL int nslots_granted(const lList *granted, const char *qhostname);
 * ```
 *
 * In a normal build the marker is empty. A report build defines
 * `OCS_REPORT_TRANSITIONAL`, and every call of a wrapper becomes a deprecation
 * warning at the caller. `ocs_transitional_report.py` (gcs-extensions,
 * `source/tools/ocs_rewrite_access`) compiles the translation units that way
 * and lists for every wrapper the callers that are left; a wrapper without one
 * is removed.
 */

#ifdef OCS_REPORT_TRANSITIONAL
/** @brief Marks a transitional overload; deprecated in a report build, so that every untyped caller is listed */
#  define OCS_TRANSITIONAL [[deprecated("transitional overload (N-X-2): convert the caller")]]
#else
/** @brief Marks a transitional overload; empty in a normal build */
#  define OCS_TRANSITIONAL
#endif
