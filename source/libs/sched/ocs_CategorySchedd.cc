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
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CT.h"

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
ocs::CategorySchedd::job_is_category_rejected(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   auto cat = ocs::tom::ConstElement<ocs::tom::CT>(static_cast<const lListElem *>(ocs::tom::get<JB_category>(job)));
   SGE_ASSERT(cat);
   const int ret = ocs::tom::get<CT_rejected>(cat);
   DRETURN(ret);
}

/**
 * @brief job_is_category_rejected() for an untyped caller
 * @param[in] job the job (`JB_Type`)
 * @return as job_is_category_rejected()
 */
int
ocs::CategorySchedd::job_is_category_rejected(const lListElem *job) {
   return job_is_category_rejected(ocs::tom::ConstElement<ocs::tom::JB>(job));
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
ocs::CategorySchedd::job_is_category_reservation_rejected(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   auto cat = ocs::tom::ConstElement<ocs::tom::CT>(static_cast<const lListElem *>(ocs::tom::get<JB_category>(job)));
   SGE_ASSERT(cat);
   const int ret = ocs::tom::get<CT_reservation_rejected>(cat);
   DRETURN(ret);
}

/**
 * @brief job_is_category_reservation_rejected() for an untyped caller
 * @param[in] job the job (`JB_Type`)
 * @return as job_is_category_reservation_rejected()
 */
int
ocs::CategorySchedd::job_is_category_reservation_rejected(const lListElem *job) {
   return job_is_category_reservation_rejected(ocs::tom::ConstElement<ocs::tom::JB>(job));
}

/**
 * @brief Marks the category of a job as rejected
 *
 * @param[in] job              the job whose category is rejected
 * @param[in] with_reservation true to also reject the category for
 *                             reservation, not only for dispatching now
 */
void
ocs::CategorySchedd::job_reject_category(const ocs::tom::ConstElement<ocs::tom::JB> job, const bool with_reservation) {
   DENTER(TOP_LAYER);
   auto cat = ocs::tom::Element<ocs::tom::CT>(static_cast<lListElem *>(ocs::tom::get<JB_category>(job)));
   SGE_ASSERT(cat);
   ocs::tom::set<CT_rejected>(cat, true);
   if (with_reservation) {
      ocs::tom::set<CT_reservation_rejected>(cat, true);
   }
   DRETURN_VOID;
}

/**
 * @brief job_reject_category() for an untyped caller
 * @param[in] job              the job (`JB_Type`)
 * @param[in] with_reservation as job_reject_category()
 */
void
ocs::CategorySchedd::job_reject_category(const lListElem *job, const bool with_reservation) {
   job_reject_category(ocs::tom::ConstElement<ocs::tom::JB>(job), with_reservation);
}
