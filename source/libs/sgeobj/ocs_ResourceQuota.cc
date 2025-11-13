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

#include <string>
#include <unordered_set>

#include "cull/cull_list.h"

#include "uti/sge_rmon_macros.h"

#include "ocs_ResourceQuota.h"
#include "ocs_ObjectContainer.h"

#include "msg_sgeobjlib.h"
#include "cull/sge_resource_quota_RQRL_L.h"

/** @brief Check resource quota limits list for duplicate names
 *  @param centry_list RQRL_Type list to check for duplicates
 *  @param answer_list If not nullptr, error messages are added to this list
 *  @param object_name Optional name of the object owning the centry list
 *  @return true if duplicates were found, false otherwise
 */
bool
ocs::ResourceQuota::has_duplicates(const lList *centry_list, lList **answer_list, const std::string& object_name) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::ObjectContainer::has_duplicates(centry_list, RQRL_name, answer_list, object_name));
}