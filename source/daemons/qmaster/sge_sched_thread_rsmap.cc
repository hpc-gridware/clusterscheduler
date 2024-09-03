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

#include "basis_types.h"
#include "sge.h"

#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"

#include "uti/sge_string.h"

#include "sge_sched_thread_rsmap.h"

static bool
gru_add_free_rsmap_ids(lListElem *gru, const char *name, const char *host_name, const lList *host_list,
                       u_long32 amount) {
   bool ret = true;

   const lListElem *host = host_list_locate(host_list, host_name);
   if (host == nullptr) {
      ret = false;
   }

   if (ret) {
      const lListElem *resource_definition = lGetSubStr(host, CE_name, name, EH_consumable_config_list);
      const lListElem *resource_utilization = lGetSubStr(host, RUE_name, name, EH_resource_utilization);
      if (resource_definition == nullptr || resource_utilization == nullptr) {
         ret = false;
      } else {
         u_long32 defined = lGetDouble(resource_definition, CE_doubleval);
         u_long32 used = lGetDouble(resource_utilization, RUE_utilized_now);
         if ((defined - used) < amount) {
            // not enough available
            ret = false;
         } else {
            const lListElem *defined_ep;
            for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
               const char *id = lGetString(defined_ep, RESL_value);
               u_long32 free_amount = lGetUlong(defined_ep, RESL_amount);
               const lListElem *used_ep = lGetSubStr(resource_utilization, RESL_value, id,
                                                     RUE_utilized_now_resource_map_list);
               if (used_ep != nullptr) {
                  free_amount -= lGetUlong(used_ep, RESL_amount);
               }
               if (free_amount > 0) {
                  lListElem *resl = lAddSubStr(gru, RESL_value, id, GRU_resource_map_list, RESL_Type);
                  if (free_amount >= amount) {
                     lSetUlong(resl, RESL_amount, amount);
                     amount = 0;
                     // we are done
                     break;
                  } else {
                     lSetUlong(resl, RESL_amount, free_amount);
                     amount -= free_amount;
                  }
               }
            }
            // should never happen, it would mean that RUE_utilized_now is not consistent
            // with the per id counters
            if (amount > 0) {
               ret = false;
            }
         }
      }
   }

   return ret;
}

static bool
gru_list_add_request(lList **granted_resources_list, const char *name, u_long32 type,
                     const char *host_name, const lList *host_list, double amount, u_long32 slots) {
   bool ret = true;

   // try to get the resource from the local host, if not available, then from the global host
   const lListElem *host = host_list_locate(host_list, host_name);
   if (host == nullptr || lGetSubStr(host, CE_name, name, EH_consumable_config_list) == nullptr) {
      host_name = SGE_GLOBAL_NAME;
      host = host_list_locate(host_list, host_name);
      if (host == nullptr || lGetSubStr(host, CE_name, name, EH_consumable_config_list) == nullptr) {
         // may never happen, the global host must exist and the resource must be defined somewhere on host level
         // @todo what about queue resources?
         return false;
      }
   }

   lListElem *gru = gru_list_search(*granted_resources_list, name, host_name);
   if (gru == nullptr) {
      gru = lAddElemStr(granted_resources_list, GRU_name, name, GRU_Type);
   }
   if (gru != nullptr) {
      lSetHost(gru, GRU_host, host_name);
      lSetDouble(gru, GRU_amount, amount * slots);

      if (type == TYPE_RSMAP) {
         lSetUlong(gru, GRU_type, GRU_RESOURCE_MAP_TYPE);
         ret = gru_add_free_rsmap_ids(gru, name, host_name, host_list, amount * slots);
      } else {
         lSetUlong(gru, GRU_type, GRU_HARD_REQUEST_TYPE);
      }
   } else {
      // couldn't malloc gru?
      ret = false;
   }

   return ret;
}

/**
 * @brief add a granted resource list to a just scheduled ja_task
 *
 * The granted resource list is built from the (granted) hard requests of the job
 * and for RSMAPs by searching free ids in the hosts' complex_values lists.
 *
 * @param ja_task
 * @param job
 * @param gdil
 * @param host_list
 * @return true in case of success, false in case of errors
 */
bool add_granted_resource_list(lListElem *ja_task, const lListElem *job,
                               const lList *gdil, const lList *host_list) {
   // check input parameters
   if (ja_task == nullptr || job == nullptr || gdil == nullptr || host_list == nullptr) {
      return false;
   }

   bool ret = true;
   lList *granted_resources_list = nullptr;

   // loop over the hard resource requests
   const lList *hard_requests = job_get_hard_resource_list(job);
   const lListElem *request;
   for_each_ep(request, hard_requests) {
      const char *name = lGetString(request, CE_name);

      u_long32 type = lGetUlong(request, CE_valtype);
      u_long32 consumable = lGetUlong(request, CE_consumable);
      double amount = lGetDouble(request, CE_doubleval);
      // we only add consumables to the granted resources list
      if (consumable == CONSUMABLE_NO) {
         continue;
      }

      // loop over the gdil and figure out the hosts
      // Attention: One host can appear multiple times in gdil (for different queue instances)!
      const lListElem *gdil_ep;
      const char *last_host = nullptr;
      for_each_ep(gdil_ep, gdil) {
         const char *host_name = lGetHost(gdil_ep, JG_qhostname);

         if (consumable == CONSUMABLE_HOST && sge_strnullcmp(last_host, host_name) == 0) {
            // we book HOST consumables only once per host
            continue;
         }
         last_host = host_name;

         u_long32 slots = lGetUlong(gdil_ep, JG_slots);
         if (consumable == CONSUMABLE_JOB || consumable == CONSUMABLE_HOST) {
            // we consume only once for the master task / once per host
            slots = 1;
         }

         ret = gru_list_add_request(&granted_resources_list, name, type, host_name, host_list, amount, slots);
         if (!ret) {
            break;
         }

         if (consumable == CONSUMABLE_JOB) {
            // for job consumables only the resources of the master task (first gdil entry) are used
            break;
         }
      }
   }

   // if we had some consumables, add the list to the ja_task
   if (ret) {
      if (granted_resources_list != nullptr) {
         lSetList(ja_task, JAT_granted_resources_list, granted_resources_list);
      }
   } else {
      lFreeList(&granted_resources_list);
   }

   return ret;
}

