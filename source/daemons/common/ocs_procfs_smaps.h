#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Reading per-process memory detail out of `/proc/<pid>/smaps`
 */

namespace ocs {

   /** @brief The memory figures summed out of one process's `/proc/<pid>/smaps`
    *
    * `smaps` reports per mapping; these are the totals. They separate memory a
    * process really owns from memory it merely shares, which the coarse `rss`
    * figure cannot.
    */
   struct smaps_sums_t {
      uint64_t pss_kb;   ///< sum of "Pss:"
      uint64_t smem_kb;  ///< sum of "Shared_Clean:" + "Shared_Dirty:" + "Shared_Hugetlb:"
      uint64_t pmem_kb;  ///< sum of "Private_Clean:" + "Private_Dirty:" + "Private_Hugetlb:"
   };

   bool procfs_parse_smaps_sums(pid_t pid, smaps_sums_t &out);
}
