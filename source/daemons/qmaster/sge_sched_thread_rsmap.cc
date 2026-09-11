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
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_str.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_monitoring_level.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sge_sched_thread_rsmap.h"
#include "msg_qmaster.h"

#include "ocs_GrantedResources.h"

/**
 * @brief choose the resource map ids this assignment is granted, and record the choice
 *
 * The ids are decided here, once, and kept on the assignment in a->granted_rsmaps - a GRU_Type
 * list keyed the same way the granted resource list is, by name and host. Booking then copies
 * them (see gru_list_apply_selected_rsmap_ids()) instead of searching the host a second time.
 *
 * Before this, matching and booking each derived the ids from the host configuration and the
 * host utilization, and agreed only because nothing is debited between the two. Nothing
 * enforced that, and it does not hold where the lists differ between the two points - inside an
 * advance reservation sge_ar_swap_resource_lists() swaps them back before booking, which is why
 * a same=id request is refused there today (CS-2730).
 *
 * The decision is still taken during booking for now. What has changed is that there is one
 * place which takes it and a place to keep it, so moving it earlier - to where matching knows
 * which instances it chose - is a change to this function's caller and not to the booking.
 *
 * @param a          the assignment, receives the selection in a->granted_rsmaps
 * @param name       name of the resource map
 * @param host_name  host the instances are taken on
 * @param host_list  the host list holding configuration and utilization
 * @param amount     how many instances to select
 * @param same_key   nullptr when the request carries no same= constraint, otherwise what the
 *                   granted instances have to agree in: "id" or the name of a characteristic
 * @return true on success, false when no set of ids could be selected
 */
static bool
rsmap_select_granted_ids(sge_assignment_t *a, const char *name, const char *host_name,
                         const lList *host_list, u_long32 amount, const char *same_key) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const lListElem *host = host_list_locate(host_list, host_name);
   if (host == nullptr) {
      ret = false;
   }
   DPRINTF("      ==> rsmap_select_granted_ids: %s, %s, %d\n", name, host_name, amount);

   // the selection for this map on this host, created on the first request scope which
   // reaches it and added to by the ones which follow
   lListElem *gru = nullptr;
   if (ret) {
      gru = gru_list_search(a->granted_rsmaps, name, host_name);
      if (gru == nullptr) {
         gru = lAddElemStr(&(a->granted_rsmaps), GRU_name, name, GRU_Type);
         if (gru == nullptr) {
            ret = false;
         } else {
            lSetHost(gru, GRU_host, host_name);
            lSetUlong(gru, GRU_type, GRU_RESOURCE_MAP_TYPE);
         }
      }
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
         } else if (same_key != nullptr) {
            // The request requires every instance to agree - in the identifier, or in a
            // characteristic they carry. Matching established that some group can serve the
            // amount, from this same configuration and this same utilization, so asking again
            // here reaches the same group without it having been carried along.
            //
            // This function is entered once per request scope, so a parallel job which requests
            // the map for its master and for its slave tasks arrives here twice. The constraint
            // covers every grant of the complex for the job, so the second visit takes from the
            // group the first one took from, which is what passing the instances granted so far
            // tells it - they also say what this job already holds, which the host utilization
            // does not know yet.
            lList *selected = nullptr;
            if (!centry_rsmap_select_group_instances(resource_definition, resource_utilization,
                                                     lGetList(gru, GRU_resource_map_list),
                                                     same_key, amount, &selected)) {
               // matching said otherwise, so the two have diverged; refuse rather than hand
               // out a mixed set behind the constraint's back
               DPRINTF("rsmap_select_granted_ids: no group of %s on %s sharing one %s holds %d\n",
                       name, host_name, same_key, amount);
               ret = false;
            } else {
               lListElem *sel_ep;
               for_each_rw (sel_ep, selected) {
                  const char *id = lGetString(sel_ep, RESL_value);
                  const u_long32 take = lGetUlong(sel_ep, RESL_amount);

                  lListElem *resl = lGetSubStrRW(gru, RESL_value, id, GRU_resource_map_list);
                  if (resl == nullptr) {
                     resl = lAddSubStr(gru, RESL_value, id, GRU_resource_map_list, RESL_Type);
                     const lListElem *defined_ep = lGetSubStr(resource_definition, RESL_value, id,
                                                              CE_resource_map_list);
                     const lList *src_props = (defined_ep != nullptr)
                                              ? lGetList(defined_ep, RESL_properties) : nullptr;
                     if (src_props != nullptr) {
                        lSetList(resl, RESL_properties, lCopyList("granted_properties", src_props));
                     }
                  }
                  DPRINTF("      ==> rsmap_select_granted_ids: same %s, id %s, amount %d\n",
                          same_key, id, take);
                  lAddUlong(resl, RESL_amount, take);
               }
               lFreeList(&selected);
            }
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
                     /* CS-2462: propagate per-instance characteristics (e.g. the
                      * "devices" characteristic used for systemd device isolation)
                      * from the host's RSMAP definition into the granted RESL, so
                      * they reach sge_execd along with the job start order. Must
                      * be a deep copy (lCopyList): the source list lives on the
                      * exec host object and would otherwise be shared with the
                      * ja_task-owned granted_resources_list. */
                     const lList *src_props = lGetList(defined_ep, RESL_properties);
                     if (src_props != nullptr) {
                        lSetList(resl, RESL_properties,
                                 lCopyList("granted_properties", src_props));
                     }
                  }
                  if (free_amount >= amount) {
                     DPRINTF("      ==> rsmap_select_granted_ids: id %s, amount %d\n", id, amount);
                     lAddUlong(resl, RESL_amount, amount);
                     //lSetUlong(resl, RESL_amount, lGetUlong(resl, RESL_amount) + amount);
                     amount = 0;
                     // we are done
                     break;
                  } else {
                     DPRINTF("      ==> rsmap_select_granted_ids: id %s, amount %d\n",
                             id, free_amount);
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
         // The resource has no capacity at host level. For a RSMAP that is an inconsistency: it can
         // only be configured on a host (see sge_complex(5)) and we would have to select instances
         // of it here.
         // Any other consumable may well be configured on the queue, or be satisfied from a load
         // value reported by the execution daemon. There is nothing to record for it in the granted
         // resource list: that list only carries the RSMAP ids and the binding information into the
         // booking, numeric consumables are debited from the job request itself
         // (see rc_debit_consumable()). So we skip it instead of failing.
         if (type != TYPE_RSMAP) {
            DPRINTF("gru_list_add_request: %s not defined at host level, nothing to grant\n", name);
            DRETURN(true);
         }
         DPRINTF("gru_list_add_request: RSMAP %s not found on host %s\n", name, host_name);
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
         // does the request require all the instances to carry the same id?
         DSTRING_STATIC(same_param, 64);
         const lListElem *request = job_get_hard_request(a->job, name, false);
         const char *same_key = nullptr;
         if (request != nullptr &&
             centry_rsmap_get_request_param(request, RSMAP_REQUEST_PARAM_SAME, &same_param)) {
            same_key = sge_dstring_get_string(&same_param);
         }

         ret = rsmap_select_granted_ids(a, name, host_name, host_list, amount * slots, same_key);
      }
   } else {
      // couldn't malloc gru?
      DPRINTF("gru_list_add_request: couldn't malloc GRU\n");
      ret = false;
   }

   DRETURN(ret);
}

/**
 * @brief give every resource map in the granted resource list the ids selected for it
 *
 * The two lists are keyed alike, by name and host, so this is a copy and not a second search.
 * It is a deep copy: the granted resource list is handed to the ja_task and outlives the
 * assignment, which frees its selection in assignment_release().
 *
 * @param a                       the assignment carrying the selection
 * @param granted_resources_list  the list being built for the task
 * @return true on success, false when a selection has no resource map to belong to
 */
static bool
gru_list_apply_selected_rsmap_ids(const sge_assignment_t *a, lList *granted_resources_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const lListElem *selected;
   for_each_ep (selected, a->granted_rsmaps) {
      const char *name = lGetString(selected, GRU_name);
      const char *host_name = lGetHost(selected, GRU_host);

      lListElem *gru = gru_list_search(granted_resources_list, name, host_name);
      if (gru == nullptr) {
         // the selection is made from the same walk which creates these, so this cannot
         // happen without the two having drifted apart
         DPRINTF("gru_list_apply_selected_rsmap_ids: no granted resource %s on host %s\n",
                 name, host_name);
         ret = false;
         continue;
      }

      const lList *ids = lGetList(selected, GRU_resource_map_list);
      if (ids != nullptr) {
         lSetList(gru, GRU_resource_map_list, lCopyList("granted_rsmap_ids", ids));
      }
   }

   DRETURN(ret);
}

/**
 * @brief report a resource which could not be booked for a just scheduled task
 *
 * Without this the task is started with fewer granted resources than it asked for and nothing
 * anywhere says so, which makes the situation impossible to recognise in a production build
 * (CS-2673).
 *
 * @param job       the job the task belongs to
 * @param ja_task   the task which is being started
 * @param name      name of the resource which could not be booked
 * @param host_name host the booking was attempted on
 */
static void
gru_report_booking_failure(const lListElem *job, const lListElem *ja_task, const char *name,
                           const char *host_name) {
   WARNING(MSG_JOB_CANNOTBOOKRESOURCE_SSUU, name, host_name,
           lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number));
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

      DPRINTF("gdil_ep: %s, %d slots%s\n", host_name, slots, is_master_task ? ", master task" : "");

      // add the binding_touse information (copy from JG to GRU)
      const lList *binding_to_use_list = lGetList(gdil_ep, JG_binding_to_use);
      ocs::GrantedResources::add_binding_to_use(&granted_resources_list, host_name, binding_to_use_list);

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
         if (!gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name,
                                   host_list, amount, debit_slots)) {
            gru_report_booking_failure(job, ja_task, name, host_name);
            ret = false;
         }
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
            if (!gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name,
                                      host_list, amount, debit_slots)) {
               gru_report_booking_failure(job, ja_task, name, host_name);
               ret = false;
            }
         }
         // we booked a master task, what remains are the slave tasks (one less slot)
         is_master_task = false;
         adjust_slave_task_debit_slots(a->pe, slots);
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
         if (!gru_list_add_request(a, &granted_resources_list, name, consumable, type, host_name,
                                   host_list, amount, debit_slots)) {
            gru_report_booking_failure(job, ja_task, name, host_name);
            ret = false;
         }
      }

      last_host = host_name;
   }

   // The ids were selected once, onto the assignment; hand them to the resource maps they
   // were selected for. Doing it after the walk rather than inside it means a map requested
   // in more than one scope is copied once, when its selection is complete.
   if (granted_resources_list != nullptr &&
       !gru_list_apply_selected_rsmap_ids(a, granted_resources_list)) {
      ret = false;
   }

   // If we had some consumables, add the list to the ja_task - also when one of the requests
   // could not be booked. Such a resource says nothing about the others: throwing the list away
   // would cost the task the RSMAP ids and the binding information of every unrelated request
   // as well (CS-2672).
   if (granted_resources_list != nullptr) {
      lSetList(ja_task, JAT_granted_resources_list, granted_resources_list);
   }

#if 0
   DPRINTF("add_granted_resource_list: after adding binding information\n");
   lWriteElemTo(ja_task, stderr);
#endif

   DRETURN(ret);
}

