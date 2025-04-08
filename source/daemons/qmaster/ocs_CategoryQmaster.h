#pragma once
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

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"

#include "sge_c_gdi.h"

namespace ocs {
   class CategoryQmaster {
   public:
      static bool
      attach_job(lList **master_category_list, lListElem **category, lListElem *job,
                 const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                 bool send_events, u_long32 gdi_session);

      static bool
      detach_job(lList **master_category_list, lListElem *job, bool send_events, u_long32 gdi_session);

      static void
      reattach_job(lList **master_category_list, lListElem *job,
                   const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                   bool send_events, u_long32 gdi_session);

      static void
      refresh_cat_data_in_job(lList *master_category_list, lListElem *job);

      static void
      attach_all_jobs(lList *master_job_list,
                      const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                      bool send_events, u_long32 gdi_session);

      static void
      reattach_all_jobs(lList *master_job_list,
                        const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                        bool send_events, u_long32 gdi_session);

      static void
      reset_tmp_data();

      static void
      refresh_cat_data_all_jobs(lList *master_category_list, lList *master_job_list);
   };
}
