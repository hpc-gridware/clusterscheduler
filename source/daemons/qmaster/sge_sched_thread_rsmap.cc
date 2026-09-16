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
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_str.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_monitoring_level.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sched/sge_resource_utilization.h"
#include "sched/schedd_message.h"
#include "sched/sge_schedd_text.h"

#include "sge_sched_thread_rsmap.h"
#include "msg_qmaster.h"

#include "ocs_GrantedResources.h"

static void
gru_report_booking_failure(const sge_assignment_t *a, const lListElem *job,
                           const lListElem *ja_task, const char *name, const char *host_name);

/**
 * @brief add the instances a selection names to the granted resource map, with their characteristics
 *
 * The selection says which identifiers and how many of each; this records them and copies the
 * per instance characteristics across. The copy has to be deep: the source lives on the exec
 * host object and the granted list is owned by the task (CS-2462).
 *
 * @param gru                 the granted resource element being built
 * @param resource_definition the resource map on the host, for the characteristics
 * @param selected            the (identifier, amount) pairs to record
 */
static void
gru_add_selected_instances(lListElem *gru, const lListElem *resource_definition,
                           const lList *selected) {
   const lListElem *sel_ep;
   for_each_ep (sel_ep, selected) {
      const char *id = lGetString(sel_ep, RESL_value);

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
      lAddUlong(resl, RESL_amount, lGetUlong(sel_ep, RESL_amount));
   }
}

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

   // Inside an advance reservation the instances come from what the reservation holds, not
   // from what the host has free: the reservation reserved particular instances and a job
   // running in it has to be granted from that set. Its copy of the host carries them, and its
   // utilization records what the other jobs in the reservation took. Outside a reservation
   // the host itself answers both questions.
   const lListElem *host = nullptr;
   if (a->ar != nullptr) {
      host = lGetSubHost(a->ar, EH_name, host_name, AR_reserved_hosts);
   }
   if (host == nullptr) {
      host = host_list_locate(host_list, host_name);
   }
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
      if (resource_definition == nullptr) {
         // the map is not configured on this host, which matching should have ruled out
         ret = false;
      } else {
         {
            // A host which has never had anything booked against this map has no utilization
            // entry for it at all, which means everything is free rather than nothing.
            // There is no separate check of the amount here. It would have to ask the same
            // question over the same window as the selection below, and asking it of the
            // utilization of this moment instead is wrong wherever the window is not now: a
            // reservation is granted for a time at which the instances it needs are free, and
            // what is in use while it is being worked out says nothing about that. The
            // selection answers both questions at once - it cannot name the instances unless
            // there are enough of them - so this asks it once.

            // A same= constraint requires every instance to agree - in the identifier, or in a
            // characteristic they carry - and matching established that some group can serve
            // the amount, from this same configuration and this same utilization. Without one,
            // any free instance will do and they are taken in the order the map defines them.
            //
            // Either way this function is entered once per request scope, so a parallel job
            // which requests the map for its master and for its slave tasks arrives here twice.
            // What has been granted so far is passed in: it says which group the first visit
            // took from, and how much of each instance this job already holds, which the host
            // utilization does not know yet because nothing is debited until the assignment is
            // complete.
            // what is not free over the time this job will run, the same question matching
            // asked of the same lists in rsmap_same_slots()
            lList *taken = utilization_rsmap_max(resource_utilization, a->now, a->start,
                                                 a->duration);

            lList *selected = nullptr;
            const lList *already = lGetList(gru, GRU_resource_map_list);
            bool selected_ok;

            if (same_key != nullptr) {
               selected_ok = centry_rsmap_select_group_instances(resource_definition, taken,
                                                                 already, same_key, amount,
                                                                 &selected);
            } else {
               selected_ok = centry_rsmap_select_instances(resource_definition, taken, already,
                                                           amount, &selected);
            }
            lFreeList(&taken);

            if (!selected_ok) {
               // matching said otherwise, so the two have diverged; refuse rather than hand
               // out a set which does not meet the request
               DPRINTF("rsmap_select_granted_ids: %s on %s cannot serve %d%s%s\n",
                       name, host_name, amount,
                       same_key != nullptr ? " sharing one " : "",
                       same_key != nullptr ? same_key : "");
               ret = false;
            } else {
               gru_add_selected_instances(gru, resource_definition, selected);
               lFreeList(&selected);
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief book one granted request of a job into the granted resource list
 *
 * @param a                      the assignment
 * @param granted_resources_list the list being built
 * @param request                the request being booked, from the scope it was written in
 * @param host_name              host it is booked on
 * @param host_list              the host list
 * @param amount                 the requested amount
 * @param slots                  how often it is taken here
 * @return true on success, false when a resource map could not be granted
 */
static bool
gru_list_add_request(sge_assignment_t *a, lList **granted_resources_list, const lListElem *request,
                     const char *host_name, const lList *host_list, double amount, u_long32 slots) {
   DENTER(TOP_LAYER);

   const char *name = lGetString(request, CE_name);
   const u_long32 consumable = lGetUlong(request, CE_consumable);
   const u_long32 type = lGetUlong(request, CE_valtype);

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
         // Does the request require all the instances to carry the same id? It is read from
         // the request being booked rather than looked up by name, so that it is the one the
         // scope in hand actually carries. A lookup by name has to pick a scope, and a
         // constraint written in the master scope is then lost - matching walks every scope
         // and would have enforced it, and the two would disagree (CS-2769).
         DSTRING_STATIC(same_param, 64);
         const char *same_key = nullptr;
         if (centry_rsmap_get_request_param(request, RSMAP_REQUEST_PARAM_SAME, &same_param)) {
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
gru_list_apply_selected_rsmap_ids(const sge_assignment_t *a, const lListElem *ja_task,
                                  lList *granted_resources_list) {
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
         gru_report_booking_failure(a, a->job, ja_task, name, host_name);
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
 * @brief report a resource which could not be granted on a host already selected for the task
 *
 * Only a resource map reaches this: gru_list_add_request() skips any other kind of consumable,
 * which is debited from the request itself rather than being granted as instances.
 *
 * It means matching and booking have disagreed. Both read the same host configuration and the
 * same utilization, with nothing debited between them, and matching checks everything booking
 * checks - the amount, the map being configured on the host, and a same= constraint on both the
 * sequential and the parallel path. So this is an inconsistency inside the scheduler and not a
 * host which filled up, which is why it is an error and not a warning (CS-2673, CS-2751).
 *
 * The message goes to the job as well as to the log. Left only in the qmaster messages file it
 * is invisible to the person whose job is affected, and a job which is not started because of it
 * would otherwise sit pending with nothing to say why.
 *
 * @param a         the assignment, for the scheduling message
 * @param job       the job the task belongs to
 * @param ja_task   the task which is being started
 * @param name      name of the resource which could not be booked
 * @param host_name host the booking was attempted on
 */
static void
gru_report_booking_failure(const sge_assignment_t *a, const lListElem *job,
                           const lListElem *ja_task, const char *name, const char *host_name) {
   ERROR(MSG_JOB_CANNOTBOOKRESOURCE_SSUU, name, host_name,
         lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number));
   schedd_mes_add(a->monitor_alpp, a->monitor_next_run, lGetUlong(job, JB_job_number),
                  SCHEDD_INFO_CANNOTBOOKRESOURCE_SS, name, host_name);
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
         double amount = lGetDouble(request, CE_doubleval);
         DPRINTF("  global: %s, %d, %f\n", name, debit_slots, amount);
         if (!gru_list_add_request(a, &granted_resources_list, request, host_name,
                                   host_list, amount, debit_slots)) {
            gru_report_booking_failure(a, job, ja_task, name, host_name);
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
            double amount = lGetDouble(request, CE_doubleval);
            DPRINTF("  master: %s, %d, %f\n", name, debit_slots, amount);
            if (!gru_list_add_request(a, &granted_resources_list, request, host_name,
                                      host_list, amount, debit_slots)) {
               gru_report_booking_failure(a, job, ja_task, name, host_name);
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
         double amount = lGetDouble(request, CE_doubleval);
         DPRINTF("  slave: %s, %d, %f\n", name, debit_slots, amount);
         if (!gru_list_add_request(a, &granted_resources_list, request, host_name,
                                   host_list, amount, debit_slots)) {
            gru_report_booking_failure(a, job, ja_task, name, host_name);
            ret = false;
         }
      }

      last_host = host_name;
   }

   // The ids were selected once, onto the assignment; hand them to the resource maps they
   // were selected for. Doing it after the walk rather than inside it means a map requested
   // in more than one scope is copied once, when its selection is complete.
   if (granted_resources_list != nullptr &&
       !gru_list_apply_selected_rsmap_ids(a, ja_task, granted_resources_list)) {
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

