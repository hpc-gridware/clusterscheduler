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

#include "basis_types.h"
#include "sge.h"

#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_monitoring_level.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sge_sched_thread_rsmap.h"

static bool
gru_add_free_rsmap_ids(lListElem *gru, const char *name, const char *host_name, const lList *host_list,
                       u_long32 amount) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const lListElem *host = host_list_locate(host_list, host_name);
   if (host == nullptr) {
      ret = false;
   }
   DPRINTF("      ==> gru_add_free_rsmap_ids: %s, %s, %d\n", name, host_name, amount);
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
                  // we might call this function multiple times, e.g. if we have requested a RSMAP
                  // both for mater and slave tasks - then resl already exists when booking the slave tasks
                  lListElem *resl = lGetSubStrRW(gru, RESL_value, id, GRU_resource_map_list);
                  if (resl == nullptr) {
                     resl = lAddSubStr(gru, RESL_value, id, GRU_resource_map_list, RESL_Type);
                  }
                  if (free_amount >= amount) {
                     DPRINTF("      ==> gru_add_free_rsmap_ids: id %s, amount %d\n", id, amount);
                     lAddUlong(resl, RESL_amount, amount);
                     //lSetUlong(resl, RESL_amount, lGetUlong(resl, RESL_amount) + amount);
                     amount = 0;
                     // we are done
                     break;
                  } else {
                     DPRINTF("      ==> gru_add_free_rsmap_ids: id %s, amount %d\n", id, free_amount);
                     lAddUlong(resl, RESL_amount, free_amount);
                     //lSetUlong(resl, RESL_amount, lGetUlong(resl, RESL_amount) + free_amount);
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

   DRETURN(ret);
}

static bool
gru_list_add_request(sge_assignment_t *a, lList **granted_resources_list, const char *name, u_long32 consumable, u_long32 type,
                     const char *host_name, const lList *host_list, double amount, u_long32 slots) {
   DENTER(TOP_LAYER);

   bool ret = true;

   // try to get the resource from the local host, if not available, then from the global host
   const lListElem *host = host_list_locate(host_list, host_name);
   if (host == nullptr || lGetSubStr(host, CE_name, name, EH_consumable_config_list) == nullptr) {
      host_name = SGE_GLOBAL_NAME;
      host = a->gep;
      if (host == nullptr || lGetSubStr(host, CE_name, name, EH_consumable_config_list) == nullptr) {
         // may never happen, the global host must exist and the resource must be defined somewhere on host level
         // @todo what about queue resources?
         DPRINTF("gru_list_add_request: resource %s not found on host %s\n", name, host_name);
         DRETURN(false);
      }
   }
   DPRINTF("  ==> gru_list_add_request: booking %s: " sge_u32 " * %f from host %s\n", name, slots, amount, host_name);
   lListElem *gru = gru_list_search(*granted_resources_list, name, host_name);
   if (gru == nullptr) {
      DPRINTF("   -> adding new GRU\n");
      gru = lAddElemStr(granted_resources_list, GRU_name, name, GRU_Type);
      if (gru != nullptr) {
         // initialize GRU element
         lSetHost(gru, GRU_host, host_name);

         if (type == TYPE_RSMAP) {
            lSetUlong(gru, GRU_type, GRU_RESOURCE_MAP_TYPE);
         } else {
            lSetUlong(gru, GRU_type, GRU_HARD_REQUEST_TYPE);
         }
      }
   } else {
      // if we have already booked a per-host consumable in the global host - don't repeat it
      if (consumable == CONSUMABLE_HOST && host == a->gep) {
         DPRINTF("   ==> gru_list_add_request: skipping subsequent per-host consumable %s on global host\n", name);
         DRETURN(true); // this is OK
      }
   }
   if (gru != nullptr) {
      // do the booking
      DPRINTF("   ==> gru_list_add_request: booking %f * %d\n", amount, slots);
      lAddDouble(gru, GRU_amount, amount * slots);
      if (type == TYPE_RSMAP) {
         ret = gru_add_free_rsmap_ids(gru, name, host_name, host_list, amount * slots);
      }
   } else {
      // couldn't malloc gru?
      DPRINTF("gru_list_add_request: couldn't malloc GRU\n");
      ret = false;
   }

   DRETURN(ret);
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
bool add_granted_resource_list(sge_assignment_t *a, lListElem *ja_task, const lListElem *job, const lList *host_list) {
   DENTER(TOP_LAYER);

   // check input parameters
   if (ja_task == nullptr || job == nullptr || a->gdil == nullptr || host_list == nullptr) {
      DRETURN(false);
   }

   bool ret = true;
   lList *granted_resources_list = nullptr;

   // loop over the gdil and figure out the hosts
   // Attention: One host can appear multiple times in gdil (for different queue instances)!
   const lListElem *gdil_ep;
   bool is_master_task = true;
   const char *last_host = nullptr;
   for_each_ep(gdil_ep, a->gdil) {
      int slots = lGetUlong(gdil_ep, JG_slots);
      const char *host_name = lGetHost(gdil_ep, JG_qhostname);
      DPRINTF("gdil_ep: %s, %d slots %s\n", host_name, slots, is_master_task ? ", master task" : "");

      // book the global resources
      const lListElem *request;
      for_each_ep (request, job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL)) {
         u_long32 consumable = lGetUlong(request, CE_consumable);

         if (consumable == CONSUMABLE_NO ||
             (consumable == CONSUMABLE_JOB && !is_master_task) ||
             (consumable == CONSUMABLE_HOST && sge_strnullcmp(last_host, host_name) == 0)) {
            continue;
         }

         int debit_slots = consumable_get_debit_slots(consumable, slots);
         const char *name = lGetString(request, CE_name);
         u_long32 type = lGetUlong(request, CE_valtype);
         double amount = lGetDouble(request, CE_doubleval);
         DPRINTF("  global: %s, %d, %f\n", name, debit_slots, amount);
         ret = gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name, host_list, amount, debit_slots);
         if (!ret) {
            break;
         }
      }
      if (!ret) {
         break;
      }

      // book the master resources
      if (is_master_task) {
         for_each_ep (request, job_get_hard_resource_list(job, JRS_SCOPE_MASTER)) {
            u_long32 consumable = lGetUlong(request, CE_consumable);

            if (consumable == CONSUMABLE_NO) {
               continue;
            }

            int debit_slots = 1;
            const char *name = lGetString(request, CE_name);
            u_long32 type = lGetUlong(request, CE_valtype);
            double amount = lGetDouble(request, CE_doubleval);
            DPRINTF("  master: %s, %d, %f\n", name, debit_slots, amount);
            ret = gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name, host_list, amount,
                                       debit_slots);
            if (!ret) {
               break;
            }
         }
         // we booked a master task, what remains are the slave tasks (one less slot)
         is_master_task = false;
         adjust_slave_task_debit_slots(a->pe, slots);
      }
      if (!ret) {
         break;
      }

      // book slave resources
      for_each_ep (request, job_get_hard_resource_list(job, JRS_SCOPE_SLAVE)) {
         u_long32 consumable = lGetUlong(request, CE_consumable);

         if (consumable == CONSUMABLE_NO) {
            continue;
         }

         int debit_slots = consumable_get_debit_slots(consumable, slots);
         const char *name = lGetString(request, CE_name);
         u_long32 type = lGetUlong(request, CE_valtype);
         double amount = lGetDouble(request, CE_doubleval);
         DPRINTF("  slave: %s, %d, %f\n", name, debit_slots, amount);
         ret = gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name, host_list, amount, debit_slots);
         if (!ret) {
            break;
         }
      }
      if (!ret) {
         break;
      }

      last_host = host_name;
   }

   // if we had some consumables, add the list to the ja_task
   if (ret) {
      if (granted_resources_list != nullptr) {
         lSetList(ja_task, JAT_granted_resources_list, granted_resources_list);
      }
   } else {
      lFreeList(&granted_resources_list);
   }

   DRETURN(ret);
}

