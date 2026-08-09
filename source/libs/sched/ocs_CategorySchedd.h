#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * @brief The scheduler's use of job categories
 *
 * Jobs whose requests are identical form a **category**. If one job of a
 * category cannot be dispatched, no other job of it can either, so the
 * scheduler marks the category as rejected and skips the rest of its jobs
 * without evaluating them again. That is what keeps a scheduling run over
 * thousands of identical jobs affordable.
 *
 * Rejection is tracked twice, because a job may be dispatchable now but not
 * reservable, or the other way round.
 */

#include "cull/cull.h"

namespace ocs {
   /** @brief Marks and queries the rejected state of a job's category */
   class CategorySchedd {
   public:
      static void
      job_reject_category(const lListElem *job, bool with_reservation);

      static int
      job_is_category_rejected(const lListElem *job);

      static int
      job_is_category_reservation_rejected(const lListElem *job);
   };
}
