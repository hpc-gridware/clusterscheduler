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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_Category.h"

#include "ocs_CategorySchedd.h"

int
ocs::CategorySchedd::job_is_category_rejected(const lListElem *job) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   int ret = lGetBool(cat, CT_rejected);
   DRETURN(ret);
}

int
ocs::CategorySchedd::job_is_category_reservation_rejected(const lListElem *job) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   int ret = lGetBool(cat, CT_reservation_rejected);
   DRETURN(ret);
}

void
ocs::CategorySchedd::job_reject_category(const lListElem *job, bool with_reservation) {
   DENTER(TOP_LAYER);
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));

   lSetBool(cat, CT_rejected, true);
   if (with_reservation) {
      lSetBool(cat, CT_reservation_rejected, true);
   }
   DRETURN_VOID;
}
