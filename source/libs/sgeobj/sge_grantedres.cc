/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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
 * @brief The resources a scheduling decision granted
 *
 * @see sge_grantedres.h
 */

#include "sgeobj/sge_host.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "uti/sge_string.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sge_grantedres.h"

/**
 * @brief print resource map ids to a string
 *
 * Every id is appended as often as its amount says, separated by blanks.
 *
 * @param buffer c++ std::string buffer the ids are appended to
 * @param resource_map_list resource map list (RESL_Type), e.g. from CE_Type or GRU_Type; may be an empty handle
 *
 * @return pointer to the c string of @p buffer
 */
const char *
granted_res_get_id_string(std::string &buffer, const ocs::tom::ConstList<ocs::tom::RESL> resource_map_list) {
   DENTER(TOP_LAYER);

   for (const auto resl : resource_map_list) {
      const char *value = ocs::tom::get<RESL_value>(resl);
      const uint32_t amount = ocs::tom::get<RESL_amount>(resl);
      for (uint32_t i = 0; i < amount; ++i) {
         if (!buffer.empty()) {
            buffer += ' ';
         }
         buffer += value;
      }
   }

   DRETURN(buffer.c_str());
}

/**
 * @brief search a GRU_Type element in a list
 *
 * A GRU_Type Object is uniquely defined by resource name and host name.
 * We search the element with these two keys.
 *
 * @param granted_resources_list the granted resources (GRU_Type), may be an empty handle
 * @param name the resource name
 * @param host_name the host name
 *
 * @return the GRU_Type element if found, else an empty handle
 *
 * @todo we have no primary key and no hashes,
 *       but with big PE jobs could have a significant number of elements
 */
ocs::tom::ConstElement<ocs::tom::GRU>
gru_list_search(const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list, const char *name,
                const char *host_name) {
   // no DENTER: called per job/queue in the scheduler
   for (const auto gru : granted_resources_list) {
      if (sge_strnullcmp(ocs::tom::get<GRU_name>(gru), name) == 0 &&
          sge_hostcmp(ocs::tom::get<GRU_host>(gru), host_name) == 0) {
         return gru;
      }
   }

   return nullptr;
}

/**
 * @brief search a GRU_Type element in a list, for a caller that changes it
 *
 * @param granted_resources_list the granted resources (GRU_Type), may be an empty handle
 * @param name the resource name
 * @param host_name the host name
 *
 * @return the GRU_Type element if found, else an empty handle
 *
 * @see gru_list_search(ocs::tom::ConstList<ocs::tom::GRU>, const char *, const char *)
 */
ocs::tom::Element<ocs::tom::GRU>
gru_list_search(const ocs::tom::List<ocs::tom::GRU> granted_resources_list, const char *name,
                const char *host_name) {
   // no DENTER: called per job/queue in the scheduler
   const ocs::tom::ConstElement<ocs::tom::GRU> gru =
         gru_list_search(static_cast<ocs::tom::ConstList<ocs::tom::GRU>>(granted_resources_list), name, host_name);

   return ocs::tom::Element<ocs::tom::GRU>::from_typed(const_cast<lListElem *>(gru.raw()));
}

/**
 * @brief gru_list_search() for an untyped caller
 *
 * @param granted_resources_list the granted resources (GRU_Type), checked; may be nullptr
 * @param name the resource name
 * @param host_name the host name
 *
 * @return as gru_list_search()
 */
lListElem *
gru_list_search(lList *granted_resources_list, const char *name, const char *host_name) {
   return gru_list_search(ocs::tom::List<ocs::tom::GRU>(granted_resources_list), name, host_name);
}
