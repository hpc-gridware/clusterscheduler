/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_hostname.h"

#include "cull/cull.h"

#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_str.h"

#include "debit.h"
#include "sge_job_schedd.h"
#include "sge_resource_utilization.h"
#include "sge_select_queue.h"
#include "sge_serf.h"
#include "uti/sge.h"

#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_schedd.h"

static void utilization_normalize(lList *diagram);
static u_long64 utilization_endtime(u_long64 start, u_long64 duration);

static void utilization_find_time_or_prevstart_or_prev(const lList *diagram, 
      u_long64 time, lListElem **hit, lListElem **before);

static int 
rqs_add_job_utilization(lListElem *jep, const lListElem *pe, u_long32 task_id, const char *type, lListElem *rule,
                        dstring rue_name, const lList *centry_list, int slots, const char *obj_name,
                        u_long64 start_time, u_long64 duration, bool is_master_task, bool do_per_host_booking);

static void add_calendar_to_schedule(lList *queue_list, u_long64 now);

static void set_utilization(lList *uti_list, u_long64 from, u_long64 till, double uti);

static lListElem *newResourceElem(u_long64 time, double amount);

static bool print_resource_utilization = getenv("SGE_PRINT_RESOURCE_UTILIZATION") == nullptr ? false : true;

/****** sge_resource_utilization/utilization_print_to_dstring() ****************
*  NAME
*     utilization_print_to_dstring() -- Print resource utilization to dstring
*
*  SYNOPSIS
*     bool utilization_print_to_dstring(const lListElem *this_elem, dstring 
*     *string) 
*
*  FUNCTION
*     Print resource utlilzation as plain number to dstring.
*
*  INPUTS
*     const lListElem *this_elem - A RUE_Type element
*     dstring *string            - The string 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: utilization_print_to_dstring() is MT safe
*******************************************************************************/
bool utilization_print_to_dstring(const lListElem *this_elem, dstring *string)
{
   if (!this_elem || !string) 
      return true;
   return double_print_to_dstring(lGetDouble(this_elem, RUE_utilized_now), string);
}


static void utilization_print_all(const lList* pe_list, lList *host_list, const lList *queue_list, const lList *ar_list)
{
   const lListElem *ep, *cr;
   const char *name;

   DENTER(TOP_LAYER);

   /* pe_list */
   for_each_ep(ep, pe_list) {
      name = lGetString(ep, PE_name);
      DPRINTF("-------------------------------------------\n");
      DPRINTF("PARALLEL ENVIRONMENT \"%s\"\n", name);
      for_each_ep(cr, lGetList(ep, PE_resource_utilization)) {
         utilization_print(cr, name, false);
      }
   }

   /* global */
   if ((ep=host_list_locate(host_list, SGE_GLOBAL_NAME))) {
      DPRINTF("-------------------------------------------\n");
      DPRINTF("GLOBAL HOST RESOURCES\n");
      for_each_ep(cr, lGetList(ep, EH_resource_utilization)) {
         utilization_print(cr, SGE_GLOBAL_NAME, false);
      }
   }

   /* exec hosts */
   for_each_ep(ep, host_list) {
      name = lGetHost(ep, EH_name);
      if (sge_hostcmp(name, SGE_GLOBAL_NAME)) {
         DPRINTF("-------------------------------------------\n");
         DPRINTF("EXEC HOST \"%s\"\n", name);
         for_each_ep(cr, lGetList(ep, EH_resource_utilization)) {
            utilization_print(cr, name, true);
         }
      }
   }

   /* queue instances */
   for_each_ep(ep, queue_list) {
      name = lGetString(ep, QU_full_name);
      if (strcmp(name, SGE_TEMPLATE_NAME) != 0) {
         DPRINTF("-------------------------------------------\n");
         DPRINTF("QUEUE \"%s\"\n", name);
         for_each_ep(cr, lGetList(ep, QU_resource_utilization)) {
            utilization_print(cr, name, false);
         }
      }
   }
   DPRINTF("-------------------------------------------\n");

   /* advance reservations */
   for_each_ep(ep, ar_list) {
      u_long32 ar_id = lGetUlong(ep, AR_id);

      const lListElem *host;
      for_each_ep(host, lGetList(ep, AR_reserved_hosts)) {
         name = lGetHost(host, EH_name);
         DPRINTF("-------------------------------------------\n");
         DPRINTF("AR " sge_u32 " HOST \"%s\"\n", ar_id, name);
         for_each_ep(cr, lGetList(host, EH_resource_utilization)) {
            utilization_print(cr, name, false);
         }
      }
      const lListElem *queue;
      for_each_ep(queue, lGetList(ep, AR_reserved_queues)) {
         name = lGetString(queue, QU_full_name);
         if (strcmp(name, SGE_TEMPLATE_NAME)) {
            DPRINTF("-------------------------------------------\n");
            DPRINTF("AR " sge_u32 " QUEUE \"%s\"\n", ar_id, name);
            for_each_ep(cr, lGetList(queue, QU_resource_utilization)) {
               utilization_print(cr, name, false);
            }
         }
      }
   }
   DPRINTF("-------------------------------------------\n");
   
   DRETURN_VOID;
}

void utilization_print(const lListElem *cr, const char *object_name, bool show_binding_inuse)
{
   DENTER(TOP_LAYER);

   const lListElem *rde;
   DSTRING_STATIC(dstr, 64);

   if (object_name == nullptr) {
      object_name = "<unknown_object>";
   }
   const char *name = lGetString(cr, RUE_name);
   double utilized_now = lGetDouble(cr, RUE_utilized_now);

   DPRINTF("resource utilization: %s: utilized-now: %s=%f\n", object_name, name, utilized_now);

   for_each_ep(rde, lGetList(cr, RUE_utilized)) {
      u_long64 time = lGetUlong64(rde, RDE_time);
      double amount = lGetDouble(rde, RDE_amount);
      const char *time_str = sge_ctime64(time, &dstr);

      if (show_binding_inuse) {
         const char *binding_inuse_str = lGetString(rde, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString binding_in_use_obj(binding_inuse_str);
            DPRINTF("\t%s %f (%s)\n", time_str, amount, binding_in_use_obj.to_product_topology_string().c_str());
         } else {
            DPRINTF("\t%s %f\n", time_str, amount);
         }

      } else {
         DPRINTF("\t%s %f\n", time_str, amount);
      }
   }
   DPRINTF("resource utilization: %s: utilized-now-non-exclusive: %s=%f\n", object_name, name, lGetDouble(cr, RUE_utilized_now_nonexclusive));
   for_each_ep(rde, lGetList(cr, RUE_utilized_nonexclusive)) {
      DPRINTF("\t%s  %f\n", sge_ctime64(lGetUlong64(rde, RDE_time), &dstr), lGetDouble(rde, RDE_amount));
   }

   DRETURN_VOID;
}

static u_long64 utilization_endtime(u_long64 start, u_long64 duration)
{
   u_long64 end_time;

   DENTER(BASIS_LAYER);

   if (((double)start + (double)duration) < ((double)U_LONG64_MAX)) {
      end_time = start + duration;
   } else {
      end_time = U_LONG64_MAX;
   }

   DRETURN(end_time);
}


/****** sge_resource_utilization/utilization_add() *****************************
*  NAME
*     utilization_add() -- Debit a jobs resource utilization
*
*  SYNOPSIS
*     int utilization_add(lListElem *cr, u_long64 start_time, u_long64
*     duration, double utilization, u_long32 job_id, u_long32 ja_taskid, 
*     u_long32 level, const char *object_name, const char *type) 
*
*  FUNCTION
*     A jobs resource utilization is debited into the resource 
*     utilization diagram at the given time for the given duration.
*
*  INPUTS
*     lListElem *cr           - Resource utilization entry (RUE_Type)
*     u_long64 start_time     - Start time of utilization
*     u_long64 duration       - Duration
*     double utilization      - Amount
*     u_long32 job_id         - Job id 
*     u_long32 ja_taskid      - Task id
*     u_long32 level          - *_TAG
*     const char *object_name - The objects name
*     const char *type        - String denoting type of utilization entry.
*     bool is_job             - reserve for job or for advance reservation
*     bool implicit_non_exclusive - add implicit entry for non-exclusive jobs
*                                   requesting a exclusive centry
*
*  RESULT
*     int - 0 on success
*
*  NOTES
*     MT-NOTE: utilization_add() is not MT safe 
*******************************************************************************/
int utilization_add(lListElem *cr, u_long64 start_time, u_long64 duration, double utilization,
                     u_long32 job_id, u_long32 ja_taskid, u_long32 level, const char *object_name,
                     const char *type, bool for_job, bool implicit_non_exclusive, const lList *binding_touse) {
   DENTER(TOP_LAYER);
   lList *resource_diagram;
   lListElem *thiz, *prev, *start, *end;
   const char *name = lGetString(cr, RUE_name);
   u_long64 end_time;
   int nm;

   if (implicit_non_exclusive) {
      nm = RUE_utilized_nonexclusive;
   } else {
      nm = RUE_utilized;
   }
   resource_diagram = lGetListRW(cr, nm);

   /* A reservation is only necessary in one of the following cases:
      - for_job is true (this means no advance reservation request) 
      - reservation is enabled and job duration not zero
      - queue is already reserved by an advance reservation (resource_diagram != nullptr)
   */
   /* AR queues have a resource diagram and we must reflect changes for this queues */
   if (for_job && (sconf_get_max_reservations() == 0 || duration == 0) && resource_diagram == nullptr) {
      DPRINTF("max reservations reached or duration is 0\n");
      DRETURN(0);
   }

   end_time = utilization_endtime(start_time, duration);

   serf_record_entry(job_id, ja_taskid, (type!=nullptr)?type:"<unknown>", start_time, end_time, level, object_name, name, utilization);

   // @todo CS-731: DONE: add all entries in the list instead of just the first
   bool handle_binding = false;
   ocs::TopologyString binding_to_use_obj;
   if (level == HOST_TAG && strcmp(name, SGE_ATTR_SLOTS) == 0 && binding_touse != nullptr) {
      handle_binding = true;

      // we have at least one binding_to_use element. copy the first one to the binding_touse_dstr
      const lListElem *to_use_elem = lFirst(binding_touse);
      ocs::TopologyString binding_to_add(lGetString(to_use_elem, ST_name));
      binding_to_use_obj.mark_nodes_as_used_or_unused(binding_to_add, true);

      // add all cores/threads of additional elements to the binding_to_use_dstr
      to_use_elem = lNext(to_use_elem);
      while (to_use_elem) {
         binding_to_add.reset_topology(lGetString(to_use_elem, ST_name));
         binding_to_use_obj.mark_nodes_as_used_or_unused(binding_to_add, true);
         to_use_elem = lNext(to_use_elem);
      }
   }
   DPRINTF("utilization_add: binding to add is %s\n", binding_to_use_obj.to_product_topology_string().c_str());

   /* ensure the resource diagram is initialized */
   if (resource_diagram == nullptr) {
      resource_diagram = lCreateList(name, RDE_Type);
      lSetList(cr, nm, resource_diagram);
   }

   utilization_find_time_or_prevstart_or_prev(resource_diagram, start_time, &start, &prev);

   double util_prev = 0.0;
   const char *binding_prev = nullptr;
   if (start) {
      DPRINTF("utilization_add: A\n");
      // if the start element is already there, we can just add the utilization to it
      lAddDouble(start, RDE_amount, utilization);
      // @todo CS-731: DONE: add binding_inuse information to the start element
      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (lGetString(start, RDE_binding_inuse) != nullptr) {
            topo_binding_now.reset_topology(lGetString(start, RDE_binding_inuse));
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused(start, RDE_binding_inuse, topo_binding_now,
                                                                binding_to_use_obj, true);
      }
   } else {
      DPRINTF("utilization_add: B\n");
      // no start element found, so we need to create one
      // if there is a previous element, we can add its amount and binding_inuse to the new element
      // otherwise we just create a new element with the utilization. it is the new list beginning
      if (prev != nullptr) {
         util_prev = lGetDouble(prev, RDE_amount);
         binding_prev = lGetString(prev, RDE_binding_inuse);
      }

      // create a new start element with the utilization and binding_inuse
      start = lCreateElem(RDE_Type);
      lSetUlong64(start, RDE_time, start_time);
      lSetDouble(start, RDE_amount, utilization + util_prev);
      // @todo CS-731: DONE: add binding_inuse information to the start element
      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (binding_prev != nullptr) {
            topo_binding_now.reset_topology(binding_prev);
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused(start, RDE_binding_inuse, topo_binding_now,
                                                                binding_to_use_obj, true);
      }

      // Insert the new element with our start time after the previous element that has an earlier start time
      lInsertElem(resource_diagram, prev, start);
   }

   end = nullptr;
   prev = start;
   thiz = lNextRW(start);

   /* find existing end element or the element before */
   while (thiz) {
      if (end_time == lGetUlong64(thiz, RDE_time)) {
         end = thiz;
         break;
      }
      if (end_time < lGetUlong64(thiz, RDE_time)) {
         break;
      }

      DPRINTF("utilization_add: C\n");

      /* increment amount of elements in-between */
      lAddDouble(thiz, RDE_amount, utilization);
      // @todo CS-731: DONE: add binding_inuse information to the thiz element
      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (lGetString(thiz, RDE_binding_inuse) != nullptr) {
            topo_binding_now.reset_topology(lGetString(thiz, RDE_binding_inuse));
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused(thiz, RDE_binding_inuse, topo_binding_now,
                                                                binding_to_use_obj, true);
      }
      prev = thiz;
      thiz = lNextRW(thiz);
   }

   if (!end) {
      DPRINTF("utilization_add: D\n");
      util_prev = lGetDouble(prev, RDE_amount);
      binding_prev = lGetString(prev, RDE_binding_inuse);

      end = lCreateElem(RDE_Type);
      lSetUlong64(end, RDE_time, end_time);
      lSetDouble(end, RDE_amount, util_prev - utilization);
      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (binding_prev != nullptr) {
            topo_binding_now.reset_topology(binding_prev);
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused(end, RDE_binding_inuse, topo_binding_now,
                                                                binding_to_use_obj, false);
      }

      lInsertElem(resource_diagram, prev, end);
   }

   utilization_normalize(resource_diagram);

   DPRINTF("utilization_add: E\n");
   // @todo CS-731: disable when finished
#if 1
   DSTRING_STATIC(combined_name, 1024);
   sge_dstring_sprintf(&combined_name, "after normalize %s-%s", object_name, name);
   utilization_print(cr, sge_dstring_get_string(&combined_name), handle_binding);
#endif

   DRETURN(0);
}

/** @brief finds the element with the specified time or the element before it
 *
 * This function searches for an element in the utilization diagram
 * that matches the specified time. If an element with the exact time is found,
 * that element is returned in the `hit` pointer. If no such element exists,
 * the function finds the element that is before the specified time
 * and returns it in the `before` pointer. If no such element exists,
 * the `before` pointer is set to nullptr.
 *
 *  @param diagram the utilization diagram to search in
 *  @param time the time to search for
 *  @param hit pointer to store the found element (nullptr if not found)
 *  @param before pointer to store the element before the found one (nullptr if no such exists)
 */
static void
utilization_find_time_or_prevstart_or_prev(const lList *diagram, u_long64 time, lListElem **hit, lListElem **before) {
   lListElem *start = nullptr;
   lListElem *thiz = lFirstRW(diagram);
   lListElem *prev = nullptr;

   while (thiz) {
      u_long64 rde_time = lGetUlong64(thiz, RDE_time);
      if (time == rde_time) {
         start = thiz;
         break;
      }
      if (time < rde_time) {
         break;
      }
      prev = thiz;
      thiz = lNextRW(thiz);
   }

   *hit = start;
   *before = prev;
}

/* normalize utilization diagram 

   Note: Diagram doesn't need to vanish (nullptr list)
         as long as utilization is added only
*/
static void utilization_normalize(lList *diagram)
{
   lListElem *thiz, *next;
   double util_prev;
   const char *bind_prev;

   thiz = lFirstRW(diagram);

   while (thiz && lGetDouble(thiz, RDE_amount) == 0.0) {
      lRemoveElem(diagram, &thiz);
      thiz = lFirstRW(diagram);
   }

   if (thiz == nullptr) {
      return;
   }
   
   if ((next = lNextRW(thiz)) == nullptr) {
      return;
   }

   util_prev = lGetDouble(thiz, RDE_amount);
   bind_prev = lGetString(thiz, RDE_binding_inuse);

   while ((thiz = next) != nullptr) {
      next = lNextRW(thiz);

      double util = lGetDouble(thiz, RDE_amount);
      const char *bind = lGetString(thiz, RDE_binding_inuse);

      // values need to be the same, and binding also needs to be the same so that we can remove an entry
      if (util_prev == util &&
          ((bind_prev == nullptr && bind == nullptr) || (bind_prev != nullptr && bind != nullptr && strcmp(bind_prev, bind) == 0))) {
         lRemoveElem(diagram, &thiz);
      } else {
         util_prev = util;
         bind_prev = bind;
      }
   }

   return;
}

double increase_util_depending_on_binding(const sge_assignment_t *a, const lListElem *host, ocs::TopologyString &binding_inuse, double util, double total, double slots) {
   // no binding string => no util change
   if (a != nullptr && host != nullptr) {
      double util_candidate = total - max_binding_idleness(a, host, slots, binding_inuse);
      util = MAX(util, util_candidate);
   }
   return util;
}

/****** sge_resource_utilization/utilization_queue_end() ***********************
*  NAME
*     utilization_queue_end() -- Determine utilization at queue end time
*
*  SYNOPSIS
*     double utilization_queue_end(const lListElem *cr) 
*
*  FUNCTION
*     Determine utilization at queue end time. Jobs that last until 
*     ever can cause a non-zero utilization.
*
*  INPUTS
*     const lListElem *cr - Resource utilization entry (RUE_utilized)
*     bool for_excl_request - For exclusive request
*
*  RESULT
*     double - queue end utilization
*
*  NOTES
*     MT-NOTE: utilization_queue_end() is MT safe 
*******************************************************************************/
double utilization_queue_end(const sge_assignment_t *a, const lListElem *host, const lListElem *cr, double total, double request, double slots, bool for_excl_request, ocs::TopologyString& binding_inuse) {
   DENTER(TOP_LAYER);

#if 1
   utilization_print(cr, "the object", false);
#endif

   double max = 0.0;
   const char *binding_inuse_str = nullptr;
   const lListElem *ep = lLast(lGetList(cr, RUE_utilized));
   if (ep) {
      if (lGetUlong64(ep, RDE_time) != U_LONG64_MAX) {
         max = lGetDouble(ep, RDE_amount);
         binding_inuse_str = lGetString(ep, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
         }
      } else {
         max = lGetDouble(lPrev(ep), RDE_amount);
         binding_inuse_str = lGetString(lPrev(ep), RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
         }
      }
   }

   if (for_excl_request) {
      double max_nonexclusive;
      const char *binding_inuse_nonexclusive_str = nullptr;
      ep = lLast(lGetList(cr, RUE_utilized_nonexclusive));
      if (ep) {
         if (lGetUlong64(ep, RDE_time) != U_LONG64_MAX) {
            max_nonexclusive = lGetDouble(ep, RDE_amount);
            binding_inuse_nonexclusive_str = lGetString(ep, RDE_binding_inuse);
            if (binding_inuse_nonexclusive_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_nonexclusive_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, tmp_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else {
            max_nonexclusive = lGetDouble(lPrev(ep), RDE_amount);
            binding_inuse_nonexclusive_str = lGetString(lPrev(ep), RDE_binding_inuse);
            if (binding_inuse_nonexclusive_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_nonexclusive_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, tmp_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         }
         if (max_nonexclusive > max) {
            max = max_nonexclusive;
            binding_inuse_str = binding_inuse_nonexclusive_str;
         }
      }
   }

   // the caller is interested in the binding of the entry
   if (binding_inuse_str != nullptr) {
      binding_inuse.reset_topology(binding_inuse_str);
   }

   if (binding_inuse.is_empty()) {
      DPRINTF("utilization_queue_end: current utilization is %f\n", max);
   } else {
      DPRINTF("utilization_queue_end: current utilization is %f %s\n", max, binding_inuse.to_product_topology_string().c_str());
   }
   DRETURN(max);
}

/** @brief Returns the maximum utilization within a timeframe and additional details
 *
 * @param cr Resource utilization entry (RUE_utilized)
 * @param start_time Start time of the timeframe
 * @param duration Duration of timeframe
 * @param for_excl_request Whether to check for exclusive requests
 * @param[out] binding_inuse Only available for slots on host level.
 * @return Maximum utilization value
 */
double
utilization_max(const sge_assignment_t *a, const lListElem *host, const lListElem *cr,
                u_long64 start_time, u_long64 duration, double total, double request, double slots,
                bool for_excl_request, ocs::TopologyString& combined_binding_inuse)
{
   DENTER(TOP_LAYER);

   const lListElem *rde;
   lListElem *start, *prev;
   double max = 0.0;
   u_long64 end_time = utilization_endtime(start_time, duration);
   const char *binding_inuse_str = nullptr;
   bool found_requested_max = false;

   // ----------------------------------------------------------------------------------------------------------------
   // find current utilization
   if (start_time == DISPATCH_TIME_NOW) {
      max = lGetDouble(cr, RUE_utilized_now);
      binding_inuse_str = lGetString(cr, RUE_utilized_now_binding_inuse);
      if (binding_inuse_str != nullptr) {
         ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
         max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
      }

      if (for_excl_request) {
         max = MAX(lGetDouble(cr, RUE_utilized_now_nonexclusive), max);
      }

      if (combined_binding_inuse.is_empty()) {
         DPRINTF("utilization_max: current utilization is %f\n", max);
      } else {
         DPRINTF("utilization_max: current utilization is %f %s\n", max, combined_binding_inuse.to_product_topology_string().c_str());
      }
      found_requested_max = true;
   }

   // ----------------------------------------------------------------------------------------------------------------
   // find utilization at the queue end
   if (!found_requested_max && start_time == DISPATCH_TIME_QUEUE_END) {
      max = utilization_queue_end(a, host, cr, total, request, slots, for_excl_request, combined_binding_inuse);
      // increase_util_depending_on_binding was done in utilization_queue_end()
      DPRINTF("utilization_max: queue end utilization is %f\n", max);
      found_requested_max = true;
   }

   // ----------------------------------------------------------------------------------------------------------------
   // find max utilization before queue end
   if (!found_requested_max) {
      DSTRING_STATIC(dstr, 64);
      DPRINTF("utilization_max: before queue end with start time  %s (" sge_u32 ")\n", sge_ctime64(start_time, &dstr), start_time);
#if 1
      utilization_print(cr, "the object", true);
#endif

      utilization_find_time_or_prevstart_or_prev(lGetList(cr, RUE_utilized), start_time, &start, &prev);
      u_long64 time = 0;
      if (start) {
         rde = lNext(start);
         max = lGetDouble(start, RDE_amount);
         time = lGetUlong64(start, RDE_time);
         DPRINTF("utilization_max: found entry with exact start time  %s (" sge_u32 ")\n", sge_ctime64(time, &dstr), time);

         binding_inuse_str = lGetString(start, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            combined_binding_inuse.reset_topology(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, combined_binding_inuse, max, total, slots);
         }
      } else if (prev) {
         rde = lNext(prev);
         max = lGetDouble(prev, RDE_amount);
         time = lGetUlong64(prev, RDE_time);
         DPRINTF("utilization_max: found entry before start time  %s (" sge_u32 ")\n", sge_ctime64(time, &dstr), time);

         binding_inuse_str = lGetString(prev, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            combined_binding_inuse.reset_topology(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, combined_binding_inuse, max, total, slots);
         }
      } else {
         rde = lFirst(lGetList(cr, RUE_utilized));
      }

      /* now watch out for the maximum before end time */
      while (rde != nullptr && end_time > lGetUlong64(rde, RDE_time)) {
         double candidate_max = lGetDouble(rde, RDE_amount);
         u_long64 candidate_time = lGetUlong64(rde, RDE_time);

         binding_inuse_str = lGetString(rde, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
            combined_binding_inuse.mark_nodes_as_used_or_unused(tmp_binding_inuse, true);
            candidate_max = increase_util_depending_on_binding(a, host, combined_binding_inuse, candidate_max, total, slots);

            DPRINTF("utilization_max: end not reached. looking at %s (" sge_u32 ") with combined binding %s\n",
                    sge_ctime64(candidate_time, &dstr), candidate_time, combined_binding_inuse.to_product_topology_string().c_str());
         }

         if (candidate_max > max) {
            max = candidate_max;
            time = candidate_time;
         }
         rde = lNext(rde);
      }

      if (for_excl_request) {
         double max_nonexclusive = 0.0;
         u_long64 time_nonexclusive = 0;
         ocs::TopologyString combined_binding_inuse_nonexclusive;

         utilization_find_time_or_prevstart_or_prev(lGetList(cr, RUE_utilized_nonexclusive), start_time, &start, &prev);
         if (start) {
            rde = lNext(start);
            max_nonexclusive = lGetDouble(start, RDE_amount);
            time_nonexclusive = lGetUlong64(start, RDE_time);
            binding_inuse_str = lGetString(start, RDE_binding_inuse);
            if (binding_inuse_str != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else if (prev) {
            rde = lNext(prev);

            max_nonexclusive = lGetDouble(prev, RDE_amount);
            time_nonexclusive = lGetUlong64(prev, RDE_time);
            binding_inuse_str = lGetString(prev, RDE_binding_inuse);
            if (binding_inuse_str != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else {
            rde = lFirst(lGetList(cr, RUE_utilized_nonexclusive));
         }

         /* now watch out for the maximum before end time */
         while (rde != nullptr && end_time > lGetUlong64(rde, RDE_time)) {
            double candidate_max_nonexclusive = lGetDouble(rde, RDE_amount);

            binding_inuse_str = lGetString(rde, RDE_binding_inuse);
            if (binding_inuse_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_str);
               combined_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(tmp_binding_inuse_nonexclusive, true);
               candidate_max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, candidate_max_nonexclusive, total, slots);
            }

            if (candidate_max_nonexclusive > max_nonexclusive) {
               max_nonexclusive = candidate_max_nonexclusive;
               time_nonexclusive = lGetUlong64(rde, RDE_time);
            }
            rde = lNext(rde);
         }
         if (max_nonexclusive > max) {
            max = max_nonexclusive;
            time = time_nonexclusive;
            combined_binding_inuse.reset_topology(combined_binding_inuse_nonexclusive.to_string(true, true, true));
         }
      }

      if (combined_binding_inuse.is_empty()) {
         DPRINTF("utilization_max: before end time with max %f at %s (" sge_u64 ")\n", max, sge_ctime64(time, &dstr), time);
      } else {
         DPRINTF("utilization_max: before end time with max %f at %s (" sge_u64 ") and combined binding of %s\n", max, sge_ctime64(time, &dstr), time, combined_binding_inuse.to_product_topology_string().c_str());
      }
   }

   DRETURN(max);
}

/** @brief Determine first time before diagrams end where utilization is below max_util.
 *
 * Searches the resource utilization diagram for the first time before the end of the diagram
 * where the utilization is below max_util. If such a time is found, it is returned. If no such time is found,
 * DISPATCH_TIME_NOW is returned.
 *
 * Considers only the utilization of regular resources (excl and non-excl requests).
 * For binding (slots on host level) the utilization and the binding pattern are considered.
 * This means that the returned time is the earliest time when additionally a binding pattern can be found that
 * allows the request to be scheduled between that time and the diagrams end.
 *
 * @note The current implementation does not consider that the utilization can go down again after it has gone up.
 * As a consequence, the returned time is not necessarily the earliest time when the utilization is below max_util.
 * It does also not consider the duration of the request. Therefore, there might be an earlier time before the
 * returned time.
 * However, it is guaranteed that between the returned time and the end of the diagram, the utilization
 * is below max_util and additionally for slots binding on hosts, one binding pattern will fit between the returned
 * time and diagrams end.
 *
 * @param a Assignment structure (required for binding consideration)
 * @param host The host element (required for binding consideration)
 * @param cr Resource utilization entry (RUE_utilized)
 * @param max_util The maximum utilization we're asking
 * @param total Total amount of the resource (required for binding considerations)
 * @param slots The number of slots on the host (required for binding consideration)
 * @param object_name Name of the queue/host/global for monitoring purposes.
 * @param for_excl_request match for exclusive request
 * @param combined_binding_inuse Only available for slots on the host level.
 *                               Show the combined binding of all entries below the earliest time
 * @return The earliest time or DISPATCH_TIME_NOW.
 */
u_long64
utilization_below(const sge_assignment_t *a, const lListElem *host, const lListElem *cr, double max_util, double total,
                  double slots, const char *object_name, bool for_excl_request, ocs::TopologyString& combined_binding_inuse) {
   DENTER(TOP_LAYER);
   const lListElem *rde;
   double util = 0;
   u_long64 when = DISPATCH_TIME_NOW;
   const char *binding_inuse_str = nullptr;
   DSTRING_STATIC(dstr, 64);

#if 1
   DPRINTF("utilization_below:\n");
   utilization_print(cr, object_name, true);
#endif

   // search backward starting from the diagram's end
   bool reuse_prev_util = false;
   for_each_rev (rde, lGetList(cr, RUE_utilized)) {

      // avoid doing work twice: reuse fetched data once and look at each binding pattern also only once
      if (!reuse_prev_util) {
         util = lGetDouble(rde, RDE_amount);
         binding_inuse_str = lGetString(rde, RDE_binding_inuse);
         if (binding_inuse_str != nullptr) {
            combined_binding_inuse.reset_topology(binding_inuse_str);
            DPRINTF("XXX combined_binding_inuse: %s\n", combined_binding_inuse.to_product_topology_string().c_str());
            util = increase_util_depending_on_binding(a, host, combined_binding_inuse, util, total, slots);
         }
      } else {

         // going upward, we need to combine the binding utilization. If a gap is found, it needs to
         // reach from the earliest time found to the end of the diagram (tetris-like)
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString combined_binding_to_add(binding_inuse_str);
            combined_binding_inuse.mark_nodes_as_used_or_unused(combined_binding_to_add, true);
            DPRINTF("XXX combined_binding_inuse: %s\n", combined_binding_inuse.to_product_topology_string().c_str());
         }
      }

      // if jobs fits then look at the diagram's entry before that.
      if (util <= max_util) {
         const lListElem *p = lPrev(rde);
         if (p != nullptr) {

            util = lGetDouble(p, RDE_amount);
            binding_inuse_str = lGetString(p, RDE_binding_inuse);
            if (binding_inuse_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
               tmp_binding_inuse.mark_nodes_as_used_or_unused(combined_binding_inuse, true);
               util = increase_util_depending_on_binding(a, host, tmp_binding_inuse, util, total, slots);
            }
            reuse_prev_util = true;

            // if the previous entry does not fit, then we found the time when the utilization goes above max_util
            // and the current entry is the first one below max_util. if the previous entry also fits,
            // we continue with the next entry
            if (util > max_util) {
               DPRINTF("utilization_below: found max utilization at due to combined binding in resource diagram %s\n", sge_ctime64(lGetUlong64(p, RDE_time), &dstr));
               when = lGetUlong64(rde, RDE_time);
               break;
            }
         }
      }
   }

   // repeat the same from above for non-exclusive entries if requested
   ocs::TopologyString combined_binding_inuse_nonexclusive;
   reuse_prev_util = false;
   if (for_excl_request) {
      u_long64 when_nonexclusive = DISPATCH_TIME_NOW;
      const char *binding_inuse_str_nonexclusive = nullptr;

      for_each_rev (rde, lGetList(cr, RUE_utilized_nonexclusive)) {
         if (!reuse_prev_util) {
            util = lGetDouble(rde, RDE_amount);
            binding_inuse_str_nonexclusive = lGetString(rde, RDE_binding_inuse);
            if (binding_inuse_str_nonexclusive != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str_nonexclusive);
               DPRINTF("XXX combined_binding_inuse: %s\n", combined_binding_inuse_nonexclusive.to_product_topology_string().c_str());
               util = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, util, total, slots);
            }
         } else {
            if (binding_inuse_str_nonexclusive != nullptr) {
               ocs::TopologyString combined_binding_to_add(binding_inuse_str_nonexclusive);
               combined_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(combined_binding_to_add, true);
               DPRINTF("XXX combined_binding_inuse: %s\n", combined_binding_inuse_nonexclusive.to_product_topology_string().c_str());
            }
         }

         if (util <= max_util) {
            const lListElem *p = lPrev(rde);
            if (p != nullptr) {

               util = lGetDouble(p, RDE_amount);
               binding_inuse_str_nonexclusive = lGetString(p, RDE_binding_inuse);
               if (binding_inuse_str_nonexclusive != nullptr) {
                  ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_str_nonexclusive);
                  tmp_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(combined_binding_inuse_nonexclusive, true);
                  util = increase_util_depending_on_binding(a, host, tmp_binding_inuse_nonexclusive, util, total, slots);
               }
               reuse_prev_util = true;

               if (util > max_util) {
                  DPRINTF("utilization_below: found max utilization-nonexclusive at due to combined binding in resource diagram %s\n", sge_ctime64(lGetUlong64(p, RDE_time), &dstr));
                  when_nonexclusive = lGetUlong64(rde, RDE_time);
                  break;
               }
            }
         }
      }

      // When the time found for non-exclusive entries is later
      if (when_nonexclusive > when) {
         when = when_nonexclusive;
         combined_binding_inuse.reset_topology(combined_binding_inuse_nonexclusive.to_string(true, true, true));
      }
   }

   if (when == DISPATCH_TIME_NOW) {
      if (host == nullptr) {
         DPRINTF("utilization_below: no utilization\n");
      } else {
         DPRINTF("utilization_below: no utilization. binding is %s\n", combined_binding_inuse.to_product_topology_string().c_str());
      }
   } else {
      if (binding_inuse_str == nullptr) {
         DPRINTF("utilization_below: found %f (%f) starting at " sge_u64 "\n", max_util, util, when);
      } else {
         DPRINTF("utilization_below: found %f (%f) starting at %s (" sge_u64 ") with binding %s\n", max_util, util, sge_ctime64(when, &dstr), when, combined_binding_inuse.to_product_topology_string().c_str());
      }
   }

   DRETURN(when); 
}

/****** sge_resource_utilization/add_job_utilization() *************************
*  NAME
*     add_job_utilization() -- Debit assignements' utilization to all schedules
*
*  SYNOPSIS
*     int add_job_utilization(const sge_assignment_t *a, const char *type) 
*
*  FUNCTION
*     The resouce utilization of an assignment is debited into the schedules 
*     of global, host and queue instance resource containers and limitation
*     rule sets. For parallel jobs debitation is made also with the parallel
*     environement schedule.
*
*  INPUTS
*     const sge_assignment_t *a - The assignement
*     const char *type          - A string that is used to monitor assignment
*                                 type
*     bool for_job_scheduling   - utilize for job or for advance reservation
*
*  RESULT
*     int - 
*
*  NOTES
*     MT-NOTE: add_job_utilization() is MT safe 
*******************************************************************************/
int add_job_utilization(const sge_assignment_t *a, const char *type, bool for_job_scheduling)
{
   DENTER(TOP_LAYER);

   lListElem *qep;
   lListElem *hep;

   if (a->ar_id == 0) {
      /* debit non-AR-job */

      dstring rue_name = DSTRING_INIT;
      /* parallel environment  */
      if (a->pe) {
         utilization_add(lFirstRW(lGetList(a->pe, PE_resource_utilization)), a->start, a->duration, a->slots,
               a->job_id, a->ja_task_id, PE_TAG, lGetString(a->pe, PE_name), type, for_job_scheduling, false, nullptr);
      }

      bool is_master_task = true;
      const lListElem *gdil_ep;
      const char *last_eh_name = nullptr;
      bool do_per_global_host_booking = true;
      for_each_ep(gdil_ep, a->gdil) {
         int slots = lGetUlong(gdil_ep, JG_slots);
         const char *eh_name = lGetHost(gdil_ep, JG_qhostname);
         const char *qname = lGetString(gdil_ep, JG_qname);
         const char* pe = (a->pe)?lGetString(a->pe, PE_name):nullptr;
         const char *queue_instance = lGetString(gdil_ep, JG_qname);
         char *queue = cqueue_get_name_from_qinstance(queue_instance);
         const lListElem *rqs = nullptr;
         bool do_per_host_booking = host_do_per_host_booking(&last_eh_name, eh_name);

         // global
         // we really need to do it per gdil_ep, because we have to consider is_master_task and ign_sreq_on_mhost
         rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, a->gep, a->centry_list, slots,
                                EH_consumable_config_list, EH_resource_utilization, SGE_GLOBAL_NAME,
                                a->start, a->duration, GLOBAL_TAG, for_job_scheduling, is_master_task, do_per_global_host_booking);

         // host
         if ((hep = host_list_locate(a->host_list, eh_name)) != nullptr) {
            rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, hep, a->centry_list, slots,
                                   EH_consumable_config_list, EH_resource_utilization, eh_name, a->start,
                                   a->duration, HOST_TAG, for_job_scheduling, is_master_task, do_per_host_booking);
         }

         // queue
         if ((qep = qinstance_list_locate2(a->queue_list, qname)) != nullptr) {
            /* 
             * The nullptr case happens in case of queues that were sorted out b/c they
             * are unknown, in some suspend state or in calendar disable state. As long 
             * as we do not intend to schedule future resource utilization for those 
             * queues it's valid to simply ignore resource utilizations decided in former 
             * schedule runs: running/suspneded/migrating jobs.
             * 
             */
            rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, qep, a->centry_list, slots,
                                   QU_consumable_config_list, QU_resource_utilization, qname, a->start,
                                   a->duration, QUEUE_TAG, for_job_scheduling, is_master_task, false);
         }

         /* resource quotas */
         for_each_ep(rqs, a->rqs_list) {
            lListElem *rule = nullptr;

            if (!lGetBool(rqs, RQS_enabled)) {
               continue;
            }

            rule = rqs_get_matching_rule(rqs, a->user, a->group, a->grp_list, a->project, pe, eh_name, queue, a->acl_list,
                                         a->hgrp_list, nullptr);
            if (rule != nullptr) {

               rqs_get_rue_string(&rue_name, rule, a->user, a->project, eh_name, queue, pe);

               rqs_add_job_utilization(a->job, a->pe, a->ja_task_id, type, rule, rue_name,
                                       a->centry_list, slots, lGetString(rqs, RQS_name),
                                       a->start, a->duration, is_master_task, do_per_host_booking);
            }
         }

         sge_free(&queue);
         is_master_task = false;
         do_per_global_host_booking = false;
      }

      sge_dstring_free(&rue_name);
   } else {
      /* debit AR-job */
      if (a->ar != nullptr) {
         lListElem *ar_global_host = lGetSubHostRW(a->ar, EH_name, SGE_GLOBAL_NAME, AR_reserved_hosts);

         bool is_master_task = true;
         bool do_per_global_host_booking = true;
         const char *last_eh_name = nullptr;
         const lListElem *gdil_ep;
         for_each_ep(gdil_ep, a->gdil) {
            int slots = lGetUlong(gdil_ep, JG_slots);
            const char *qname = lGetString(gdil_ep, JG_qname);
            const char *eh_name = lGetHost(gdil_ep, JG_qhostname);
            bool do_per_host_booking = host_do_per_host_booking(&last_eh_name, eh_name);

            if ((qep = lGetSubStrRW(a->ar, QU_full_name, qname, AR_reserved_queues)) != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, qep, a->centry_list, slots,
                                      QU_consumable_config_list, QU_resource_utilization, qname, a->start,
                                      a->duration, QUEUE_TAG, for_job_scheduling, is_master_task, do_per_host_booking);
            }
            if (ar_global_host != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, ar_global_host, a->centry_list, slots,
                                      EH_consumable_config_list, EH_resource_utilization, SGE_GLOBAL_NAME, a->start,
                                      a->duration, HOST_TAG, for_job_scheduling, is_master_task, do_per_global_host_booking);
            }
            lListElem *host = lGetSubHostRW(a->ar, EH_name, eh_name, AR_reserved_hosts);
            if (host != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, host, a->centry_list, slots,
                                      EH_consumable_config_list, EH_resource_utilization, SGE_GLOBAL_NAME, a->start,
                                      a->duration, HOST_TAG, for_job_scheduling, is_master_task, do_per_host_booking);
            }
            is_master_task = false;
            do_per_global_host_booking = false;
         }
      }
   }

   DRETURN(0);
}

int rc_add_job_utilization(const lListElem *gdil, lListElem *jep, const lListElem *pe, u_long32 task_id, const char *type, lListElem *ep,
                           const lList *centry_list, int slots, int config_nm, int actual_nm, const char *obj_name,
                           u_long64 start_time, u_long64 duration, u_long32 tag, bool for_job_scheduling,
                           bool is_master_task, bool do_per_host_booking)
{
   lListElem *cr = nullptr, *cr_config, *dcep;
   int mods = 0;

   DENTER(TOP_LAYER);

   if (ep == nullptr) {
      ERROR("rc_add_job_utilization nullptr object " "(job " sge_u32 " obj %s type %s) slots %d ep %p\n", lGetUlong(jep, JB_job_number), obj_name, type, slots, (void*)ep);
      DRETURN(0);
   }

   if (slots == 0) {
      ERROR("rc_add_job_utilization 0 slot amount " "(job " sge_u32 " obj %s type %s) slots %d ep %p\n", lGetUlong(jep, JB_job_number), obj_name, type, slots, (void*)ep);
      DRETURN(0);
   }

   u_long32 job_id = lGetUlong(jep, JB_job_number);

   for_each_rw (cr_config, lGetList(ep, config_nm)) {
      const char *name = lGetString(cr_config, CE_name);

      /* search default request */  
      if (!(dcep = centry_list_locate(centry_list, name))) {
         ERROR(MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S , name);
         DRETURN(-1);
      }

      u_long32 consumable = lGetUlong(dcep, CE_consumable);

      if (consumable != CONSUMABLE_NO) {
         /* ensure attribute is in actual list */
         if (!(cr = lGetSubStrRW(ep, RUE_name, name, actual_nm))) {
            cr = lAddSubStr(ep, RUE_name, name, actual_nm, RUE_Type);
            /* CE_double is implicitly set to zero */
         }
      }

      if (!consumable_do_booking(consumable, is_master_task, do_per_host_booking)) {
         continue;
      }

      bool did_booking = false;
      int debit_slots = consumable_get_debit_slots(consumable, slots);

      // has contribution from global requests? Then we can do the booking for master and slave task in one step.
      double dval = 0.0;
      if (job_get_contribution_by_scope(jep, nullptr, name, &dval, dcep, JRS_SCOPE_GLOBAL)) {
         if (dval != 0.0) {
            /* update RUE_utilized resource diagram to reflect jobs utilization */
            const lList *binding_to_use = nullptr;
            if (tag == HOST_TAG) {
               binding_to_use = lGetList(gdil, JG_binding_to_use);
            }
            utilization_add(cr, start_time, duration, debit_slots * dval, job_id, task_id, tag,
                            obj_name, type, for_job_scheduling, false, binding_to_use);
            mods++;
            did_booking = true;
         }
      } else if (pe != nullptr) {
         // no global contribution, need to check master and slave
         // we use the original slots value for slave_debit_slots
         // reason: for host consumables, debit_slots is 1, which will be adjusted below to 0
         //         if we have then a slave request for a host consumable, it will not be booked!
         int slave_debit_slots = slots;
         if (is_master_task) {
            // if the master task is part of this booking, check if we have a master request
            dval = 0.0;
            if (job_get_contribution_by_scope(jep, nullptr, name, &dval, dcep, JRS_SCOPE_MASTER)) {
               if (dval != 0.0) {
                  /* update RUE_utilized resource diagram to reflect jobs utilization */
                  // book it for one slot (the master task)
                  utilization_add(cr, start_time, duration, slot_signum(debit_slots) * dval, job_id, task_id, tag,
                                  obj_name, type, for_job_scheduling, false, lGetList(gdil, JG_binding_to_use));
                  mods++;
                  did_booking = true;
               }
            }

            adjust_slave_task_debit_slots(pe, slave_debit_slots);
         }

         // now do booking for the (remaining) slave tasks, if any
         if (slave_debit_slots != 0) {
            bool tmp_ret;
            dval = 0.0;
            // if we are on the master host, and we shall ignore slave requests to booking only for slots
            if (is_master_task && lGetBool(pe, PE_ignore_slave_requests_on_master_host)) {
               tmp_ret = true;
               if (sge_strnullcmp(name, SGE_ATTR_SLOTS) == 0) {
                  dval = 1.0;
               }
            } else {
               tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &dval, dcep, JRS_SCOPE_SLAVE);
            }

            if (tmp_ret && dval != 0.0) {
               /* update RUE_utilized resource diagram to reflect jobs utilization */
               // book it for the remaining slave tasks
               slave_debit_slots = consumable_get_debit_slots(consumable, slave_debit_slots);
               utilization_add(cr, start_time, duration, slave_debit_slots * dval, job_id, task_id, tag,
                               obj_name, type, for_job_scheduling, false, lGetList(gdil, JG_binding_to_use));
               mods++;
               did_booking = true;
            }
         }
      }

      // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
      if (!did_booking && lGetUlong(dcep, CE_relop) == CMPLXEXCL_OP) {
         dval = 1.0;
         /* update RUE_utilized resource diagram to reflect jobs utilization */
         utilization_add(cr, start_time, duration, debit_slots * dval, job_id, task_id, tag,
                         obj_name, type, for_job_scheduling, true, lGetList(gdil, JG_binding_to_use));
         mods++;
      }
   }

   DRETURN(mods);
}

/****** sge_resource_utilization/rqs_add_job_utilization() ********************
*  NAME
*     rqs_add_job_utilization() -- Debit assignment's utilization in a limitation
*                                  rule
*
*  SYNOPSIS
*     static int rqs_add_job_utilization(lListElem *jep, u_long32 task_id, 
*     const char *type, lListElem *rule, dstring rue_name, lList *centry_list, 
*     int slots, const char *obj_name, u_long64 start_time, u_long64 duration,
*     bool is_master_task) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lListElem *jep       - job element (JB_Type)
*     u_long32 task_id     - task id to debit
*     const char *type     - String denoting type of utilization entry 
*     lListElem *rule      - limitation rule (RQR_Type)
*     dstring rue_name     - rue_name where to debit
*     lList *centry_list   - master centry list (CE_Type)
*     int slots            - slots to debit
*     const char *obj_name - name of the object where to debit
*     u_long64 start_time  - start time of utilization
*     u_long64 duration    - end time of utilization
*     bool is_master_task  - is this the master task going to be debit
*
*  RESULT
*     static int - amount of modified limits
*
*  NOTES
*     MT-NOTE: rqs_add_job_utilization() is MT safe 
*
*  SEE ALSO
*     sge_resource_utilization/rc_add_job_utilization()
*     sge_resource_utilization/add_job_utilization()
*******************************************************************************/
static int 
rqs_add_job_utilization(lListElem *jep, const lListElem *pe, u_long32 task_id, const char *type, lListElem *rule,
                        dstring rue_name, const lList *centry_list, int slots, const char *obj_name,
                        u_long64 start_time, u_long64 duration, bool is_master_task, bool do_per_host_booking)
{
   DENTER(TOP_LAYER);

   int mods = 0;

   if (jep != nullptr) {
      u_long32 job_id = lGetUlong(jep, JB_job_number);

      lListElem *limit;
      for_each_rw (limit, lGetListRW(rule, RQR_limit)) {
         lListElem *raw_centry;
         lListElem *rue_elem = nullptr;

         const char *centry_name = lGetString(limit, RQRL_name);
         
         if (!(raw_centry = centry_list_locate(centry_list, centry_name))) {
            /* ignoring not defined centry */
            continue;
         }

         u_long32 consumable = lGetUlong(raw_centry, CE_consumable);

         if (consumable != CONSUMABLE_NO) {
            rue_elem = lGetSubStrRW(limit, RUE_name, sge_dstring_get_string(&rue_name), RQRL_usage);
            if (rue_elem == nullptr) {
               rue_elem = lAddSubStr(limit, RUE_name, sge_dstring_get_string(&rue_name), RQRL_usage, RUE_Type);
               /* RUE_utilized_now is implicitly set to zero */
            }
         }

         if (!consumable_do_booking(consumable, is_master_task, do_per_host_booking)) {
            continue;
         }

         bool did_booking = false;
         int debit_slots = consumable_get_debit_slots(consumable, slots);

         // has contribution from global requests? Then we can do the booking for master and slave task in one step.
         double dval = 0.0;
         if (job_get_contribution_by_scope(jep, nullptr, centry_name, &dval, raw_centry, JRS_SCOPE_GLOBAL)) {
            if (dval != 0.0) {
               /* update RUE_utilized resource diagram to reflect jobs utilization */
               utilization_add(rue_elem, start_time, duration, debit_slots * dval, job_id, task_id,
                               RQS_TAG, obj_name, type, true, false, nullptr);
               mods++;
               did_booking = true;
            }
         } else if (pe != nullptr) {
            // no global contribution, need to check master and slave
            // we use the original slots value for slave_debit_slots
            // reason: for host consumables, debit_slots is 1, which will be adjusted below to 0
            //         if we have then a slave request for a host consumable, it will not be booked!
            int slave_debit_slots = slots;
            if (is_master_task) {
               // if the master task is part of this booking, check if we have a master request
               dval = 0.0;
               if (job_get_contribution_by_scope(jep, nullptr, centry_name, &dval, raw_centry, JRS_SCOPE_MASTER)) {
                  if (dval != 0.0) {
                     /* update RUE_utilized resource diagram to reflect jobs utilization */
                     // book it for one slot (the master task)
                     utilization_add(rue_elem, start_time, duration, slot_signum(debit_slots) * dval, job_id, task_id,
                                     RQS_TAG, obj_name, type, true, false, nullptr);
                     mods++;
                     did_booking = true;
                  }
               }

               // if we did the master task booking
               // adjust the slot count for the slave booking
               adjust_slave_task_debit_slots(pe, slave_debit_slots);
            }

            // now do booking for the (remaining) slave tasks, if any
            if (slave_debit_slots != 0) {
               bool tmp_ret;
               dval = 0.0;
               // if we are on the master host, and we shall ignore slave requests to booking only for slots
               if (is_master_task && lGetBool(pe, PE_ignore_slave_requests_on_master_host)) {
                  tmp_ret = true;
                  if (sge_strnullcmp(centry_name, SGE_ATTR_SLOTS) == 0) {
                     dval = 1.0;
                  }
               } else {
                  tmp_ret = job_get_contribution_by_scope(jep, nullptr, centry_name, &dval, raw_centry, JRS_SCOPE_SLAVE);
                  if (tmp_ret && dval != 0.0) {
                     /* update RUE_utilized resource diagram to reflect jobs utilization */
                     // book it for the remaining slave tasks
                     slave_debit_slots = consumable_get_debit_slots(consumable, slave_debit_slots);
                     utilization_add(rue_elem, start_time, duration, slave_debit_slots * dval, job_id, task_id,
                                     RQS_TAG, obj_name, type, true, false, nullptr);
                     mods++;
                     did_booking = true;
                  }
               }
            }
         }

         // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
         if (!did_booking && lGetUlong(raw_centry, CE_relop) == CMPLXEXCL_OP) {
            dval = 1.0;
            utilization_add(rue_elem, start_time, duration, debit_slots * dval, job_id, task_id,
                            RQS_TAG, obj_name, type, true, true, nullptr);
            mods++;
         }
      }
   }

   DRETURN(mods);
}

static int
add_job_list_to_schedule(const lList *job_list, bool suspended, lList *pe_list, lList *host_list, lList *queue_list, lList *rqs_list,
                         const lList *centry_list, const lList *acl_list, const lList *hgroup_list, lList *ar_list,
                         bool for_job_scheduling, u_long64 now)
{
   lListElem *jep, *ja_task;
   lListElem *gep = host_list_locate(host_list, SGE_GLOBAL_NAME);
   const char *type;
   u_long32 interval = sconf_get_schedule_interval();

   DENTER(TOP_LAYER);

   if (suspended) {
      type = SCHEDULING_RECORD_ENTRY_TYPE_SUSPENDED;
   } else {
      type = SCHEDULING_RECORD_ENTRY_TYPE_RUNNING;
   }   

   for_each_rw (jep, job_list) {
      for_each_rw (ja_task, lGetList(jep, JB_ja_tasks)) {
         sge_assignment_t a = SGE_ASSIGNMENT_INIT;

         assignment_init(&a, jep, ja_task, nullptr);
         assignment_init_ar(&a, ar_list);

         a.start = lGetUlong64(ja_task, JAT_start_time);

         task_get_duration(&a.duration, ja_task);

         a.duration = duration_add_offset(a.duration, sge_gmt32_to_gmt64(sconf_get_duration_offset()));

         /* Prevent jobs that exceed their prospective duration are not reflected 
            in the resource schedules. Note duration enforcement is domain of 
            sge_execd and default_duration is not enforced at all anyways.
            All we can do here is hope the job will be finished in the next interval. */
         if (duration_add_offset(a.start, a.duration) <= now) {
            /* That logging is disabled as it can cause schedd messages file
               be filled up with loggings. There are cases when it can't be 
               considered a misconfiguration if jobs do not complete within the
               time foreseen. If jobs are submitted without -l h_rt limit and 
               aren't cancelled due to default_duration only be in effect */

            if (for_job_scheduling && sconf_get_max_reservations() > 0) {
               WARNING(MSG_SCHEDD_SHOULDHAVEFINISHED_UUU, a.job_id, a.ja_task_id, static_cast<u_long32>(now - a.duration - a.start + 1));
            }
            a.duration = (now - a.start) + interval;
         }

         a.gdil = lGetListRW(ja_task, JAT_granted_destin_identifier_list);
         a.slots = nslots_granted(a.gdil, nullptr);
         a.host_list = host_list;
         a.queue_list = queue_list;
         a.centry_list = centry_list;
         a.rqs_list = rqs_list;
         a.acl_list = acl_list;
         a.hgrp_list = hgroup_list;
         a.ar_list = ar_list;
         a.gep = gep;

         // Step 1/3 We need the current PE for the resource utilization calculation
         const char *pe_name = lGetString(ja_task, JAT_granted_pe);
         a.pe = pe_list_locate(pe_list, pe_name);
         if (pe_name != nullptr && a.pe == nullptr) {
            CRITICAL("===> granted_pe is %s but pe_object is nullptr", pe_name);
         }

         // Step 2/3: adjust current PE with the settings of the PE object of the jatask so that
         //           the utilization is calculated correctly (PE settings might have changed meanwhile)
         //
         // - safe original value of PE_ignore_slave_requests_on_master_host
         // - replace the value in the PE object to that one of the PE object of the jatask
         // - This has to be reset further down below (step 4/4)
         bool org_sromh = false;
         if (a.pe != nullptr) {
            lListElem *old_pe = lGetObject(ja_task, JAT_pe_object);
            bool old_sromh = lGetBool(old_pe, PE_ignore_slave_requests_on_master_host);

            org_sromh = lGetBool(a.pe, PE_ignore_slave_requests_on_master_host);
            lSetBool(a.pe, PE_ignore_slave_requests_on_master_host, old_sromh);
         }

         if (DPRINTF_IS_ACTIVE) {
            DSTRING_STATIC(dstr, 64);
            DPRINTF("Adding job " sge_u32 "." sge_u32 " into schedule start %s duration %.0f\n",
                    lGetUlong(jep, JB_job_number), lGetUlong(ja_task, JAT_task_number),
                    sge_ctime64(a.start, &dstr), sge_gmt64_to_gmt32_double(a.duration));
         }

         // Step 3/4: only update resource utilization schedule RUE_utilized_now is already set through events
         debit_scheduled_job(&a, nullptr, nullptr, false, type, for_job_scheduling);

         // Step 4/4: debiting is done. Reset the PE object with the original value
         if (a.pe != nullptr) {
            lSetBool(a.pe, PE_ignore_slave_requests_on_master_host, org_sromh);
         }
      }
   }

   DRETURN(0);
}

/****** sge_resource_utilization/prepare_resource_schedules() *********************************
*  NAME
*     prepare_resource_schedules() -- Debit non-pending jobs in resource schedule (resource diagram)
*
*  SYNOPSIS
*     static void prepare_resource_schedules(const lList *running_jobs, const 
*     lList *suspended_jobs, lList *pe_list, lList *host_list, lList 
*     *queue_list, lList *centry_list, lList *rqs_list) 
*
*  FUNCTION
*     In order to reflect current and future resource utilization of running 
*     and suspended jobs in the schedule (resource diagram)
*     we iterate through all jobs and debit
*     resources requested by those jobs.
*
*  INPUTS
*     const lList *running_jobs   - The running ones (JB_Type)
*     const lList *suspended_jobs - The susepnded ones (JB_Type)
*     lList *pe_list              - ??? 
*     lList *host_list            - ??? 
*     lList *queue_list           - ??? 
*     lList *rqs_list             - configured resource quota sets
*     lList *centry_list          - ??? 
*     lList *acl_list             - ??? 
*     lList *hgroup_list          - ??? 
*     lList *prepare_resource_schedules - create schedule for job or advance reservation
*                                         scheduling
*     bool for_job_scheduling     - prepare for job or for advance reservation
*     u_long32 now                - now time of assignment
*
*  NOTES
*     MT-NOTE: prepare_resource_schedules() is not MT safe 
*******************************************************************************/
void prepare_resource_schedules(const lList *running_jobs, const lList *suspended_jobs, 
   lList *pe_list, lList *host_list, lList *queue_list, lList *rqs_list, const lList *centry_list,
   const lList *acl_list, const lList *hgroup_list, lList *ar_list, bool for_job_scheduling, u_long64 now)
{
   DENTER(TOP_LAYER);

   add_job_list_to_schedule(running_jobs, false, pe_list, host_list, queue_list,
                            rqs_list, centry_list, acl_list, hgroup_list,
                            ar_list, for_job_scheduling, now);
   add_job_list_to_schedule(suspended_jobs, true, pe_list, host_list, queue_list,
                            rqs_list, centry_list, acl_list, hgroup_list,
                            ar_list, for_job_scheduling, now);
   add_calendar_to_schedule(queue_list, now);

   if (print_resource_utilization) {
      utilization_print_all(pe_list, host_list, queue_list, ar_list);
   }

   DRETURN_VOID;
}

/****** sge_resource_utilization/add_calendar_to_schedule() ***********************************
*  NAME
*     add_calendar_to_schedule() -- addes the queue calendar to the resource
*                                   schedule
*
*  SYNOPSIS
*     static void add_calendar_to_schedule(lList *queue_list) 
*
*  FUNCTION
*     Adds the queue calendars to the resource schedule. It is using
*     the slot entry for simulating and enabled / disabled calendar.
*
*  INPUTS
*     lList *queue_list - all queues, which can posibly run jobs
*     u_long32 now      - now time of assignment
*
*  NOTES
*     MT-NOTE: add_calendar_to_schedule() is MT safe 
*
*  SEE ALSO
*     sge_resource_utilization/set_utilization
*     scheduler/newResourceElem
*     scheduler/prepare_resource_schedules
*******************************************************************************/
static void 
add_calendar_to_schedule(lList *queue_list, u_long64 now)
{
   const lListElem *queue;

   DENTER(TOP_LAYER);

   for_each_ep(queue, queue_list) {
      const lList *queue_states = lGetList(queue, QU_state_changes);
      u_long64 from = now;

      if (queue_states != nullptr) {
      
         const lList *consumable_list = lGetList(queue, QU_consumable_config_list);
         const lListElem *slot_elem = lGetElemStr(consumable_list, CE_name, SGE_ATTR_SLOTS);
         double slot_count = lGetDouble(slot_elem, CE_doubleval); 

         const lList *queue_uti_list = lGetList(queue, QU_resource_utilization);
         lListElem *slot_uti = lGetElemStrRW(queue_uti_list, RUE_name, SGE_ATTR_SLOTS);
         lList *slot_uti_list = lGetListRW(slot_uti, RUE_utilized);
         
         const lListElem *queue_state = nullptr;

         DPRINTF("queue: %s time " sge_u64" \n", lGetString(queue, QU_full_name), from);

         if (slot_uti_list == nullptr) {
            slot_uti_list = lCreateList("slot_uti", RDE_Type);
            lSetList(slot_uti, RUE_utilized, slot_uti_list);
         }

         for_each_ep(queue_state, queue_states) {
            bool is_full = (lGetUlong(queue_state, CQU_state) != QI_DO_NOTHING)?true:false;
            u_long64 till = lGetUlong64(queue_state, CQU_till);
          
            /* check for now, and set it if it is now */
            if (is_full && (from == now)) {
               lSetDouble(slot_uti, RUE_utilized_now, slot_count);
            }
          
            set_utilization(slot_uti_list, from, till, is_full?slot_count:0);
            
            from = till;     
         } /* end for_each */

      }/* end if*/
   }
   
   DRETURN_VOID;
}

/****** sge_resource_utilization/set_utilization() ********************************************
*  NAME
*     set_utilization() -- adds one specific calendar entry to the resource schedule
*
*  SYNOPSIS
*     static void set_utilization(lList *uti_list, u_long32 from, u_long32 
*     till, double uti) 
*
*  FUNCTION
*     This set utilization function is unique for calendars. It removes all other
*     uti settings in the given time interval and replaces it with the given one.
*
*  INPUTS
*     lList *uti_list - the uti list for a specifiy resource and queue
*     u_long32 from   - starting time for this uti
*     u_long32 till   - endtime for this uti.
*     double uti      - utilization (needs to bigger than 1 (schould be max)
*
*  NOTES
*     MT-NOTE: set_utilization() is MT safe 
*
*  SEE ALSO
*     sge_resource_utilization/add_calendar_to_schedule
*     sge_resource_utilization/newResourceElem
*     sge_resource_utilizationscheduler/prepare_resource_schedules
*******************************************************************************/
static void 
set_utilization(lList *uti_list, u_long64 from, u_long64 till, double uti)
{
   DENTER(TOP_LAYER);

   if (uti > 0) {
      bool is_from_added = false;
      bool is_till_added = false;
      double past_uti = 0;
      lListElem *uti_elem_next = nullptr;

      if (till == 0) {
         till = DISPATCH_TIME_QUEUE_END;
      }

      DPRINTF("queue cal. schedule entry time " sge_u64 " till " sge_u64 " util: %f\n", from, till, uti);

      uti_elem_next = lFirstRW(uti_list);
     
      /* search for the starting point */
      while (uti_elem_next != nullptr) {
         u_long64 rde_time = lGetUlong64(uti_elem_next, RDE_time);
         if (rde_time > from) { /*insert before this elem */
            lInsertElem(uti_list, lPrevRW(uti_elem_next), newResourceElem(from, uti));
            past_uti = lGetDouble(uti_elem_next, RDE_amount);
            is_from_added = true; 
            break;
         } else if (rde_time == from) { /* modify found elem */
            /* override utilization is maximun */
            past_uti = lGetDouble(uti_elem_next, RDE_amount);
            lSetDouble(uti_elem_next, RDE_amount, uti);
            is_from_added = true;
            break;
         } else { /* did not find it, continue */
            uti_elem_next = lNextRW(uti_elem_next);
         }
      }

      if (is_from_added) { /* search for the endpoint */
          while (uti_elem_next != nullptr) {
             u_long64 rde_time = lGetUlong64(uti_elem_next, RDE_time);
            if (rde_time > till) { /*insert before this elem */
               lInsertElem(uti_list, lPrevRW(uti_elem_next), newResourceElem(till, past_uti));
               is_till_added = true; 
               break;
            } else if (rde_time == till) { /* do not override utilization is maximun */
               is_till_added = true;
               break;
            } else { /* did not find it, remove the current elem and continue*/
               lListElem *next = lNextRW(uti_elem_next);
               past_uti = lGetDouble(uti_elem_next, RDE_amount);
               lRemoveElem(uti_list, &uti_elem_next);
               uti_elem_next = next;
            }
         }
      } else {
         lAppendElem(uti_list, newResourceElem(from, uti));
      }

      if (!is_till_added) {
         lAppendElem(uti_list, newResourceElem(till, 0));
      }   
   }

   DRETURN_VOID;
}

/****** sge_resource_utilization/newResourceElem() ********************************************
*  NAME
*     newResourceElem() -- creates new resource schedule entry
*
*  SYNOPSIS
*     static lListElem* newResourceElem(u_long32 time, double amount) 
*
*  FUNCTION
*     creates new resource schedule entry and returns it
*
*  INPUTS
*     u_long32 time - specific time
*     double amount - the utilized amount
*
*  RESULT
*     static lListElem* - new resource schedule entry
*
*  NOTES
*     MT-NOTE: newResourceElem() is MT safe 
*
*  SEE ALSO
*     sge_resource_utilization/add_calendar_to_schedule
*     sge_resource_utilization/set_utilization
*     sge_resource_utilization/prepare_resource_schedules
*******************************************************************************/

static lListElem *newResourceElem(u_long64 time, double amount)
{
   lListElem *elem = nullptr;

   elem = lCreateElem(RDE_Type);
   if (elem != nullptr) {
      lSetUlong64(elem, RDE_time, time);
      lSetDouble(elem, RDE_amount, amount);    
   }

   return elem;
}
