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
#include "uti/sge_hostname.h"

#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_str.h"

#include "ocs_BindingSchedd.h"

/** @brief Find the binding in use (either now, reservation, within AR or for reservation within AR
 */
bool
ocs::BindingSchedd::find_initial_in_use(const sge_assignment_t *a, const lListElem *host, TopologyString &host_in_use) {
   DENTER(TOP_LAYER);

   bool in_reservation_mode = !host_in_use.is_empty();

   // multiple things come together in this function
   // - reservation or now scheduling
   // - scheduling within an AR or not
   // - initialize and uninitialized utilized_now_binding
   // independent of that we try to find out what is currently in use on the host for the specific scenario

   bool found_now_utilization = false;

   // If we have no binding utilization from a resource schedule as input, then we will try to find
   // the utilization from the host itself. This might still be uninitialized.
   if (in_reservation_mode) {
      DPRINTF("Host's %s reservation binding util. is:  %s\n", lGetHost(host, EH_name), host_in_use.to_product_topology_string().c_str());
   } else {
      const lListElem *utilization_slots = lGetSubStr(host, RUE_name, SGE_ATTR_SLOTS, EH_resource_utilization);
      if (utilization_slots != nullptr) {
         const char *src = lGetString(utilization_slots, RUE_utilized_now_binding_inuse);
         if (src != nullptr) {
            host_in_use.reset_topology(src);
            found_now_utilization = true;
            DPRINTF("Host's %s binding now utilization is:    %s\n", lGetHost(host, EH_name), host_in_use.to_product_topology_string().c_str());
         } else {
            DPRINTF("Host %s has still no binding now utilization\n", lGetHost(host, EH_name));
         }
      } else {
         DPRINTF("Host %s has still no binding now utilization\n", lGetHost(host, EH_name));
      }
   }

   // now assigment + now utilization not initialized + no AR scheduling => use the topology string as a starting point for binding
   if (!in_reservation_mode && !found_now_utilization) {
      const char *internal_topo = lGetString(host, EH_internal_topology);
      if (internal_topo != nullptr) {
         host_in_use.reset_topology(internal_topo);
         found_now_utilization = true;
         DPRINTF("Internal topology of host %s:            %s\n", lGetHost(host, EH_name), host_in_use.to_product_topology_string().c_str());
      }
   }

   // For non-AR scheduling we are done
   if (a->ar == nullptr) {
      if (found_now_utilization) {
         DRETURN(true);
      } else {
         DPRINTF("Host %s has no initial now utilization (should not be possible)\n", lGetHost(host, EH_name));
         DRETURN(false);
      }
   }

   // For AR scheduling we have to mask those parts of the topology that where not granted to the AR
   // this is independent of reservation or now scheduling
   ocs::TopologyString ar_in_use;
   const char *hostname = lGetHost(host, EH_name);
   const lListElem *granted_resource;
   bool first_entry = true;
   for_each_ep(granted_resource, lGetList(a->ar, AR_granted_resources_list)) {
      // skip resources that do not contain binding information
      if (lGetUlong(granted_resource, GRU_type) != GRU_BINDING_TYPE) {
         continue;
      }
      // skip resources that do not belong to the host
      if (sge_hostcmp(hostname, lGetHost(granted_resource, GRU_host)) != 0) {
         continue;
      }

      // accumulate binding information of the AR
      const lListElem *binding_in_use_elem;
      for_each_ep(binding_in_use_elem, lGetList(granted_resource, GRU_binding_inuse)) {
         const char *bind_str = lGetString(binding_in_use_elem, ST_name);
         if (bind_str != nullptr) {
            if (first_entry) {
               ar_in_use.reset_topology(bind_str);
               first_entry = false;
            } else {
               ocs::TopologyString binding_to_add(bind_str);
               ar_in_use.mark_nodes_as_used_or_unused(binding_to_add, true);
            }
            found_now_utilization = true;
         }
      }
   }
   DPRINTF("Granted binding for AR on %s:            %s\n", lGetHost(host, EH_name), ar_in_use.to_product_topology_string().c_str());

   // Invert the binding string. The AR binds what can be used by jobs.
   ar_in_use.invert_binding();
   DPRINTF("Inverted granted binding for AR on %s:   %s\n", lGetHost(host, EH_name), ar_in_use.to_product_topology_string().c_str());

   // Remove the non-granted parts from the binding mask
   if (found_now_utilization) {
      host_in_use.mark_nodes_as_used_or_unused(ar_in_use, true);
   } else {
      host_in_use.reset_topology(ar_in_use.to_string(true, true, true, false, false, false));
   }
   DPRINTF("Final binding used for AR scheduling %s: %s\n", lGetHost(host, EH_name), host_in_use.to_product_topology_string().c_str());

   DRETURN(true);
}

/** @brief Applies additional constraints to the given binding in use
 */
void
ocs::BindingSchedd::find_final_in_use(const sge_assignment_t *a, TopologyString &binding_in_use) {
   DENTER(TOP_LAYER);

   // find all parameter related to binding
   std::string binding_filter = Job::binding_get_filter(a->job);
   std::string binding_sort = Job::binding_get_sort(a->job);

   // @todo CS-732: Here we should remove all threads that are masked by an admin manually
   // e.g binding_params or m_topology set in complex values

   // @todo CS-732: If a job also requests RSMAPS and when those have a topology mask set then
   // we can remove all threads that can never match in case certain RSMAPS are already in use by others

   // @todo CS-732: If affinity information is available and also enforced we can remove all threads
   // where the affinity mask will prevent scheduling

   // @todo CS-732: If memory binding is active and constrains for NUMA memory (or caches) apply them
   // we can remove those threads belonging to NUMA nodes not having enough memory available

   DRETURN_VOID;
}

// @brief Tries a binding for `slots` and returns the number of slots where a binding could be found
double
ocs::BindingSchedd::test_strategy(const sge_assignment_t *a, const lListElem *host, double slots, const TopologyString &binding_in_use) {
   DENTER(TOP_LAYER);

   // if no slots are specified (PE scheduling), then we try to maximize slots.
   if (slots == 0.0) {
      slots = static_cast<double>(std::numeric_limits<int>::max());
      DPRINTF("max_binding_idleness: try to find binding for %f slots with binding %s\n", slots, binding_in_use.to_product_topology_string().c_str());
   } else {
      DPRINTF("max_binding_idleness: try to find binding for requested %f slots with binding %s\n", slots, binding_in_use.to_product_topology_string().c_str());
   }

   // We return as if we can handle all slots if:
   // - binding is disabled.
   // - in case this method should be called not on host-level
   // - the job has no binding requests.
   if (a == nullptr || !a->is_binding_enabled
      || host == nullptr
      || a->job == nullptr || lGetObject(a->job, JB_new_binding) == nullptr) {
      DRETURN(slots);
   }

   // Use the given binding in use and apply additional filter based on context and do sort
   ocs::TopologyString tmp_binding_in_use(binding_in_use.to_string(true, true, true, false, false, false));
   find_final_in_use(a, tmp_binding_in_use);

   // handle different binding types
   unsigned binding_amount = Job::binding_get_amount(a->job);
   //BindingStrategy::Strategy binding_strategy = Job::binding_get_strategy(a->job);
   BindingType::Type binding_type = Job::binding_get_type(a->job);
   BindingUnit::Unit binding_unit = Job::binding_get_unit(a->job);
   BindingStart::Start binding_start = Job::binding_get_start(a->job);
   BindingEnd::End binding_end = Job::binding_get_end(a->job);
   if (binding_type == BindingType::HOST) {

      // host binding can handle all slots per definition
      auto ids = tmp_binding_in_use.find_n_packed_units(binding_amount, binding_unit, binding_start, binding_end);
      if (ids.size() >= binding_amount) {
         // enough units found -> report that requested slots are idle
         DPRINTF("max_binding_idleness: enough units found for host-binding of %f slots\n", slots);
         DRETURN(slots);
      }

   } else if (binding_type == BindingType::SLOT) {

      // find the requested binding for all slots
      int max_slots = 0;
      for (max_slots = 0; max_slots < slots; max_slots++) {
         auto ids = tmp_binding_in_use.find_n_packed_units(binding_amount, binding_unit, binding_start, binding_end);
         if (ids.size() < binding_amount) {
            break;
         }

         // add the task binding to the binding mask
         tmp_binding_in_use.mark_units_as_used_or_unused(ids, binding_unit, true);
      }

      if (max_slots > 0) {
         double min = std::min<double>(max_slots, slots);
         DPRINTF("max_binding_idleness: units found for slots-binding of %f/%f slots\n", min, slots);
         DRETURN(min);
      }
   } else {
      // new binding type?
   }

   DPRINTF("max_binding_idleness: no binding found for %s\n", lGetHost(host, EH_name));
   DRETURN(0.0);
}

/** @brief Apply the binding strategy and store the decision in the assignment structure
 */
int
ocs::BindingSchedd::apply_strategy(sge_assignment_t *a, int slots, const lListElem *host, TopologyString& topo_in_use) {
   DENTER(TOP_LAYER);

   // We return as if we can handle all slots if:
   // - binding is disabled.
   // - in case this method should be called not on host-level
   // - the job has no binding requests.
   if (a == nullptr || !a->is_binding_enabled
      || host == nullptr
      || a->job == nullptr || lGetObject(a->job, JB_new_binding) == nullptr) {
      DRETURN(slots);
   }

   // Early exit in the case of host binding when binding was already done.
   // We know per definition that we can handle all slots.
   const char *hostname = lGetHost(host, EH_name);
   BindingType::Type binding_type= Job::binding_get_type(a->job);
   lListElem *binding_done = lGetElemHostRW(a->binding_to_use, BN_specific_hostname, hostname);
   if (binding_type == BindingType::HOST && binding_done != nullptr) {
      DPRINTF("find_binding: host binding already done for host %s\n", hostname);
      DRETURN(slots);
   }

#ifdef WITH_EXTENSIONS
#endif

   // Use the given binding in use and apply additional filter based on context and do sort
   find_initial_in_use(a, host, topo_in_use);
   find_final_in_use(a, topo_in_use);

   // distinguish between host and slot-based binding
   unsigned binding_amount = Job::binding_get_amount(a->job);
   BindingUnit::Unit binding_unit = Job::binding_get_unit(a->job);
   BindingStart::Start binding_start = Job::binding_get_start(a->job);
   BindingEnd::End binding_end = Job::binding_get_end(a->job);
   if (binding_type == BindingType::HOST) {

      // find the requested binding
      auto ids = topo_in_use.find_n_packed_units(binding_amount, binding_unit, binding_start, binding_end);
      if (ids.size() < binding_amount) {
         DPRINTF("find_binding: not enough units found for binding request (only %u/%u)\n", ids.size(), binding_amount);
         DRETURN(0); // host binding cannot be fulfilled
      }

      // create a binding mask that only contains those units that we will bind for this host binding
      TopologyString binding_to_use(topo_in_use.to_string(true, true, true, false, false, true));
      binding_to_use.mark_units_as_used_or_unused(ids, binding_unit, true);
      for (auto id : ids) {
         binding_to_use.mark_node_as_used_or_unused(id, true);
      }
      DPRINTF("find_binding: host binding for this job will be %s\n", binding_to_use.to_product_topology_string().c_str());

      // store the binding decision in the assignment structure
      binding_done = lAddElemHost(&(a->binding_to_use), BN_specific_hostname, hostname, BN_Type);
      lSetString(binding_done, BN_specific_binding, binding_to_use.to_string(true, true, true, false, false, false).c_str());
      lSetList(binding_done, BN_specific_binding_list, nullptr);
      DRETURN(slots);

   } else if (binding_type == BindingType::SLOT) {

      // Find the initial ID for the next task binding if there are already bindings otherwise start with 0
      u_long32 next_binding_id_for_task = 0;
      if (binding_done != nullptr) {
         next_binding_id_for_task = lGetNumberOfElem(lGetList(binding_done, BN_specific_binding_list));
      }

      // Use the current binding on the host as a starting point for the binding decision
      // or continue binding at that position where it previously finished on that host (e.g. PE round_robin)
      ocs::TopologyString host_binding_to_use;
      if (binding_done == nullptr) {
         host_binding_to_use.reset_topology(topo_in_use.to_string(true, true, true, false, false, false));
      } else {
         host_binding_to_use.reset_topology(lGetString(binding_done, BN_specific_binding));
      }

      // find binding for each slot
      TopologyString task_binding_to_use;
      int max_slots;
      for (max_slots = 0; max_slots < slots; max_slots++) {

         // find the requested binding
         auto ids = topo_in_use.find_n_packed_units(binding_amount, binding_unit, binding_start, binding_end);
         if (ids.size() < binding_amount) {
            DPRINTF("find_binding: not enough units found for binding request (only %u/%u)\n", ids.size(), binding_amount);
            break;
         }

         // create a binding mask that only contains those units that we will bind for this slot binding on the host
         task_binding_to_use.reset_topology(topo_in_use.to_string(true, true, true, false, false, true));
         task_binding_to_use.mark_units_as_used_or_unused(ids, binding_unit, true);

         DPRINTF("find_binding: slot binding for task %d will be %s\n", max_slots, hostname, task_binding_to_use.to_product_topology_string().c_str());

         // add the task binding to the host binding mask
         host_binding_to_use.mark_units_as_used_or_unused(ids, binding_unit, true);

         // add the task binding to the binding list for the host
         topo_in_use.mark_units_as_used_or_unused(ids, binding_unit, true);

         // store the binding decision for the task
         if (binding_done == nullptr) {
            binding_done = lAddElemHost(&(a->binding_to_use), BN_specific_hostname, hostname, BN_Type);
         }
         lListElem *binding_for_task = lAddSubUlong(binding_done, ST_id, next_binding_id_for_task, BN_specific_binding_list, ST_Type);
         lSetString(binding_for_task, ST_name, task_binding_to_use.to_string(true, true, true, false, false, false).c_str());

         // continue with the next task
         next_binding_id_for_task++;
      }

      // exit if no binding was found
      if (max_slots == 0) {
         DPRINTF("find_binding: no binding possible for all %d task(s)\n", slots);
         DRETURN(0);
      }

      // store the binding decision in the assignment structure
      if (binding_done) {
         lSetString(binding_done, BN_specific_binding, host_binding_to_use.to_string(true, true, true, false, false, false).c_str());
         DPRINTF("find_binding: slot binding for all tasks on will give us binding %s\n", host_binding_to_use.to_product_topology_string().c_str());
      }

      DRETURN(max_slots);
   } else {
      // binding type not supported
      DRETURN(0);
   }

   // no binding was requested => job gets what it wants => return with success for all slots
   DRETURN(slots);
}

/** @brief Copy the binding decision from the assignment structure to the gdil element
 */
bool
ocs::BindingSchedd::copy_strategy(const sge_assignment_t *a) {
   DENTER(TOP_LAYER);
   bool host_specific_binding = false;
   bool task_specific_binding = false;

   // copy the host specific binding from the assignment to the gdil element
   lListElem *jg_elem;
   for_each_rw(jg_elem, a->gdil) {
      const char *hostname = lGetHost(jg_elem, JG_qhostname);

      // try to find a binding for the host
      lListElem *binding_elem = lGetElemHostRW(a->binding_to_use, BN_specific_hostname, hostname);
      if (binding_elem == nullptr) {
         DPRINTF("copy_binding: no binding found for host %s\n", hostname);
         DRETURN(true);
      }

      // check if the binding list for tasks is available
      lList *task_binding_list = lGetListRW(binding_elem, BN_specific_binding_list);
      if (task_binding_list == nullptr) {
         host_specific_binding = true;
      } else {
         task_specific_binding = true;
      }

      if (host_specific_binding) {
         DPRINTF("copy_binding: host specific binding for host %s\n", hostname);

         // copy the binding decision for the host (either for one task or for all tasks share the same binding)
         const char *binding_str = lGetString(binding_elem, BN_specific_binding);
         lListElem *jg_to_use = lAddSubUlong(jg_elem, ST_id, 0, JG_binding_to_use, ST_Type);
         lSetString(jg_to_use, ST_name, binding_str);
      } else if (task_specific_binding) {
         DPRINTF("copy_binding: task specific binding for host %s\n", hostname);

         u_long32 slots = lGetUlong(jg_elem, JG_slots);

         // create the binding list in the granted element that will hold all task bindings
         lList *jg_binding_list = lGetListRW(jg_elem, JG_binding_to_use);
         if (jg_binding_list == nullptr) {
            jg_binding_list = lCreateList("binding_touse", ST_Type);
            lSetList(jg_elem, JG_binding_to_use, jg_binding_list);
         }

         // copy the binding decision for the amount of tasks that where granted
         while (slots > 0) {

            // get the first binding for a task
            lListElem *task_binding_elem = lFirstRW(task_binding_list);
            if (task_binding_elem == nullptr) {
               DPRINTF("copy_binding: something went wrong, no task binding found for slot %d\n", slots);
               DRETURN(false);
            }

            // move the element from the task binding list to the granted element
            ocs::TopologyString task_binding(lGetString(task_binding_elem, ST_name));
            DPRINTF("copy_binding: moved task binding %s for slot %d\n", task_binding.to_product_topology_string().c_str(), slots);
            lDechainElem(task_binding_list, task_binding_elem);
            lAppendElem(jg_binding_list, task_binding_elem);

            // handle the next slot's binding
            slots--;
         }
      }
   }

#if 0
   DPRINTF("binding copy: gdil list after copy/move\n");
   lWriteListTo(a->gdil, stderr);
#endif
   DRETURN(true);
}
