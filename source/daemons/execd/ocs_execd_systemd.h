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

namespace ocs::execd {

   bool
   execd_use_pdc_for_usage_collection();
   bool
   execd_use_systemd_for_usage_collection();
   bool
   execd_is_hybrid_usage_collection();

#if defined(OCS_WITH_SYSTEMD)

   void
   execd_systemd_init();

   bool
   execd_move_shepherd_to_scope();

   void
   execd_store_tight_pe_slice(const lListElem *job, lListElem *ja_task, const char *slice_name = nullptr);

   void
   execd_delete_tight_pe_slice(u_long32 job_id, u_long32 ja_task_id, const char *pe_task_id);

   void
   ptf_get_usage_from_systemd();

#endif
}
