#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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
 * @brief Declarations for the resources a scheduling decision granted
 *
 * @see sge_grantedres.cc
 */

#include <string>

#include "sgeobj/cull/sge_grantedres_GRU_L.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_RESL.h"

const char *
granted_res_get_id_string(std::string &buffer, ocs::tom::ConstList<ocs::tom::RESL> resource_map_list);

ocs::tom::ConstElement<ocs::tom::GRU>
gru_list_search(ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list, const char *name, const char *host_name);
ocs::tom::Element<ocs::tom::GRU>
gru_list_search(ocs::tom::List<ocs::tom::GRU> granted_resources_list, const char *name, const char *host_name);
OCS_TRANSITIONAL lListElem *gru_list_search(lList *granted_resources_list, const char *name, const char *host_name);
