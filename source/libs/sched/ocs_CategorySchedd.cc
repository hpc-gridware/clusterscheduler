/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Category.h"

#include "ocs_CategorySchedd.h"

/**
 * @brief Was the category of this job already rejected?
 *
 * @param[in] job the job to look at
 *
 * @return non-zero if no job of this category can be dispatched in this
 *         scheduling run
 */
int
ocs::CategorySchedd::job_is_category_rejected(const lListElem *job) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   SGE_ASSERT(cat != nullptr);
   const int ret = lGetBool(cat, CT_rejected);
   DRETURN(ret);
}

/**
 * @brief Was the category of this job already rejected for reservation?
 *
 * @param[in] job the job to look at
 *
 * @return non-zero if no job of this category can get a reservation in
 *         this scheduling run
 */
int
ocs::CategorySchedd::job_is_category_reservation_rejected(const lListElem *job) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   SGE_ASSERT(cat != nullptr);
   const int ret = lGetBool(cat, CT_reservation_rejected);
   DRETURN(ret);
}

/**
 * @brief Marks the category of a job as rejected
 *
 * @param[in] job              the job whose category is rejected
 * @param[in] with_reservation true to also reject the category for
 *                             reservation, not only for dispatching now
 */
void
ocs::CategorySchedd::job_reject_category(const lListElem *job, bool with_reservation) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   SGE_ASSERT(cat != nullptr);
   lSetBool(cat, CT_rejected, true);
   if (with_reservation) {
      lSetBool(cat, CT_reservation_rejected, true);
   }
   DRETURN_VOID;
}
