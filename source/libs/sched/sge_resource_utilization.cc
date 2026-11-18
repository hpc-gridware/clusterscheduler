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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The resource utilization diagram - what is booked, and when
 *
 * A consumable is not just "so much free right now". The scheduler keeps, per
 * resource and per object, a **diagram** over time: a list of points at which
 * the booked amount changes, so it can answer "how much of this is free
 * between t and t+d" - which is what resource reservation and advance
 * reservations need.
 *
 * utilization_add() books an interval into a diagram, utilization_max()
 * answers the maximum booking within an interval, and
 * prepare_resource_schedules() fills the diagrams of a whole scheduling run
 * from the running and suspended jobs.
 */

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_hostname.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_str.h"

#include "debit.h"
#include "sge_job_schedd.h"
#include "sge_resource_utilization.h"
#include "sge_select_queue.h"
#include "sge_serf.h"
#include "uti/sge.h"

#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_List.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_schedd.h"
#include "ocs_BindingSchedd.h"

static void utilization_normalize(ocs::tom::List<ocs::tom::RDE> diagram);
static uint64_t utilization_endtime(uint64_t start, uint64_t duration);

static std::pair<ocs::tom::Element<ocs::tom::RDE>, ocs::tom::Element<ocs::tom::RDE>>
utilization_find_time_or_prevstart_or_prev(ocs::tom::ConstList<ocs::tom::RDE> diagram, uint64_t time);

static int
rqs_add_job_utilization(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::PE> pe,
                        uint32_t task_id, const char *type, ocs::tom::Element<ocs::tom::RQR> rule, dstring rue_name,
                        ocs::tom::ConstList<ocs::tom::CE> centry_list, int slots, const char *obj_name,
                        uint64_t start_time, uint64_t duration, bool is_master_task, bool do_per_host_booking);

static void add_calendar_to_schedule(ocs::tom::List<ocs::tom::QU> queue_list, uint64_t now);

static void set_utilization(ocs::tom::List<ocs::tom::RDE> uti_list, uint64_t from, uint64_t till, double uti);

static ocs::tom::ElementPtr<ocs::tom::RDE> newResourceElem(uint64_t time, double amount);

static void utilization_print(ocs::tom::ConstElement<ocs::tom::RUE> cr, const char *object_name,
                              bool show_binding_inuse);

static bool print_resource_utilization = getenv("SGE_PRINT_RESOURCE_UTILIZATION") == nullptr ? false : true;

/**
 * @brief Print resource utilization to dstring
 *
 * Print resource utlilzation as plain number to dstring.
 *
 * @param this_elem A RUE_Type element
 * @param string The string
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: utilization_print_to_dstring() is MT safe
 */
bool utilization_print_to_dstring(const ocs::tom::ConstElement<ocs::tom::RUE> this_elem, dstring *string) {
   if (this_elem == nullptr || string == nullptr) {
      return true;
   }
   return double_print_to_dstring(ocs::tom::get<RUE_utilized_now>(this_elem), string);
}



/**
 * @brief Writes the utilization diagrams of all objects to the debug output
 *
 * @param[in] pe_list    the parallel environments
 * @param[in] host_list  the execution hosts
 * @param[in] queue_list the queue instances
 * @param[in] ar_list    the advance reservations
 */
static void utilization_print_all(const ocs::tom::ConstList<ocs::tom::PE> pe_list,
                                  const ocs::tom::ConstList<ocs::tom::EH> host_list,
                                  const ocs::tom::ConstList<ocs::tom::QU> queue_list,
                                  const ocs::tom::ConstList<ocs::tom::AR> ar_list) {
   DENTER(TOP_LAYER);

   const char *name;

   /* pe_list */
   for (const auto ep : pe_list) {
      name = ocs::tom::get<PE_name>(ep);
      DPRINTF("-------------------------------------------\n");
      DPRINTF("PARALLEL ENVIRONMENT \"%s\"\n", name);
      for (const auto cr : ocs::tom::get<PE_resource_utilization>(ep)) {
         utilization_print(cr, name, false);
      }
   }

   /* global */
   if (const auto ep = host_list.find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME)); ep != nullptr) {
      DPRINTF("-------------------------------------------\n");
      DPRINTF("GLOBAL HOST RESOURCES\n");
      for (const auto cr : ocs::tom::get<EH_resource_utilization>(ep)) {
         utilization_print(cr, SGE_GLOBAL_NAME, false);
      }
   }

   /* exec hosts */
   for (const auto ep : host_list) {
      name = ocs::tom::get<EH_name>(ep);
      if (sge_hostcmp(name, SGE_GLOBAL_NAME)) {
         DPRINTF("-------------------------------------------\n");
         DPRINTF("EXEC HOST \"%s\"\n", name);
         for (const auto cr : ocs::tom::get<EH_resource_utilization>(ep)) {
            utilization_print(cr, name, true);
         }
      }
   }

   /* queue instances */
   for (const auto ep : queue_list) {
      name = ocs::tom::get<QU_full_name>(ep);
      if (strcmp(name, SGE_TEMPLATE_NAME) != 0) {
         DPRINTF("-------------------------------------------\n");
         DPRINTF("QUEUE \"%s\"\n", name);
         for (const auto cr : ocs::tom::get<QU_resource_utilization>(ep)) {
            utilization_print(cr, name, false);
         }
      }
   }
   DPRINTF("-------------------------------------------\n");

   /* advance reservations */
   for (const auto ep : ar_list) {
      uint32_t ar_id = ocs::tom::get<AR_id>(ep);

      for (const auto host : ocs::tom::get<AR_reserved_hosts>(ep)) {
         name = ocs::tom::get<EH_name>(host);
         DPRINTF("-------------------------------------------\n");
         DPRINTF("AR " sge_u32 " HOST \"%s\"\n", ar_id, name);
         for (const auto cr : ocs::tom::get<EH_resource_utilization>(host)) {
            utilization_print(cr, name, false);
         }
      }
      for (const auto queue : ocs::tom::get<AR_reserved_queues>(ep)) {
         name = ocs::tom::get<QU_full_name>(queue);
         if (strcmp(name, SGE_TEMPLATE_NAME)) {
            DPRINTF("-------------------------------------------\n");
            DPRINTF("AR " sge_u32 " QUEUE \"%s\"\n", ar_id, name);
            for (const auto cr : ocs::tom::get<QU_resource_utilization>(queue)) {
               utilization_print(cr, name, false);
            }
         }
      }
   }
   DPRINTF("-------------------------------------------\n");
   
   DRETURN_VOID;
}

/**
 * @brief Writes one utilization diagram to the debug output
 *
 * @param[in] cr                 the resource utilization entry (`RUE_Type`)
 * @param[in] object_name        name of the object the entry belongs to, may
 *                               be nullptr
 * @param[in] show_binding_inuse whether to also print the topology units in
 *                               use
 */
static void utilization_print(const ocs::tom::ConstElement<ocs::tom::RUE> cr, const char *object_name,
                              bool show_binding_inuse) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(dstr, 64);

   if (object_name == nullptr) {
      object_name = "<unknown_object>";
   }
   const char *name = ocs::tom::get<RUE_name>(cr);
   double utilized_now = ocs::tom::get<RUE_utilized_now>(cr);

   DPRINTF("resource utilization: %s: utilized-now: %s=%f\n", object_name, name, utilized_now);

   for (const auto rde : ocs::tom::get<RUE_utilized>(cr)) {
      uint64_t time = ocs::tom::get<RDE_time>(rde);
      double amount = ocs::tom::get<RDE_amount>(rde);
      const char *time_str = sge_ctime64(time, &dstr);

      if (show_binding_inuse) {
         const char *binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(rde);
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
   DPRINTF("resource utilization: %s: utilized-now-non-exclusive: %s=%f\n", object_name, name, ocs::tom::get<RUE_utilized_now_nonexclusive>(cr));
   for (const auto rde : ocs::tom::get<RUE_utilized_nonexclusive>(cr)) {
      DPRINTF("\t%s  %f\n", sge_ctime64(ocs::tom::get<RDE_time>(rde), &dstr), ocs::tom::get<RDE_amount>(rde));
   }

   DRETURN_VOID;
}

static uint64_t utilization_endtime(uint64_t start, uint64_t duration) {
   DENTER(BASIS_LAYER);

   uint64_t end_time;

   if (((double)start + (double)duration) < ((double)std::numeric_limits<uint64_t>::max())) {
      end_time = start + duration;
   } else {
      end_time = std::numeric_limits<uint64_t>::max();
   }

   DRETURN(end_time);
}

/**
 * @brief set the instances a diagram entry carries, from a previous entry plus or minus a change
 *
 * The resource map counterpart of the topology string a diagram entry carries for core
 * bindings: an entry says how much of each identifier is in use from its time onwards, and an
 * entry is built from the one before it plus what starts there, or minus what ends there.
 *
 * An identifier which falls to nothing is dropped rather than kept at zero, so that two entries
 * describing the same state compare equal and utilization_normalize() can merge them.
 *
 * @param rde      the diagram entry to fill
 * @param previous the instances in use up to this point, may be nullptr
 * @param change   the instances starting or ending here
 * @param add      true when they start here, false when they end here
 */
static void
utilization_rsmap_set(const ocs::tom::Element<ocs::tom::RDE> rde, const ocs::tom::ConstList<ocs::tom::RESL> previous,
                      const ocs::tom::ConstList<ocs::tom::RESL> change, bool add) {
   auto ids = previous != nullptr ? previous.clone("rsmap_inuse") : ocs::tom::ListPtr<ocs::tom::RESL>();

   for (const auto ep : change) {
      const char *id = ocs::tom::get<RESL_value>(ep);
      const uint32_t amount = ocs::tom::get<RESL_amount>(ep);

      auto dst = ids != nullptr ? ids->find_first<RESL_value>(id) : ocs::tom::Element<ocs::tom::RESL>();
      if (dst == nullptr) {
         if (!add) {
            // nothing of this identifier was in use, so there is nothing to give back. The
            // booking and the unbooking have disagreed, which the amount side would report
            // as a negative utilization.
            continue;
         }
         if (ids == nullptr) {
            ids = ocs::tom::ListPtr<ocs::tom::RESL>::create("rsmap_inuse");
         }
         dst = ids->emplace_back<RESL_value>(id);
         if (dst == nullptr) {
            continue;
         }
      }

      const uint32_t in_use = ocs::tom::get<RESL_amount>(dst);
      if (add) {
         ocs::tom::set<RESL_amount>(dst, in_use + amount);
      } else if (in_use > amount) {
         ocs::tom::set<RESL_amount>(dst, in_use - amount);
      } else {
         ids->erase(dst);
      }
   }

   ocs::tom::set<RDE_resource_map_list>(rde, std::move(ids));
}

/**
 * @brief do two diagram entries have the same resource map instances in use
 *
 * Used by utilization_normalize() to decide whether an entry says anything the one before it
 * did not. Order is not significant, only which identifiers are present and with what count.
 *
 * @param a the instances in use of the one entry, may be an empty handle
 * @param b the instances in use of the other entry, may be an empty handle
 * @return true if both name the same identifiers with the same counts
 */
static bool
utilization_rsmap_equal(const ocs::tom::ConstList<ocs::tom::RESL> a, const ocs::tom::ConstList<ocs::tom::RESL> b) {
   DENTER(BASIS_LAYER);

   if (a.size() != b.size()) {
      DRETURN(false);
   }

   for (const auto ep : a) {
      const auto other = ocs::tom::find_first<RESL_value>(b, ocs::tom::get<RESL_value>(ep));
      if (other == nullptr || ocs::tom::get<RESL_amount>(other) != ocs::tom::get<RESL_amount>(ep)) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}

/**
 * @brief how much of each resource map instance is taken anywhere in a time window
 *
 * The instance counterpart of utilization_max(): that says how much of the amount is taken at
 * its worst point in the window, this says the same per identifier. A job is granted particular
 * instances and holds them for as long as it runs, so the question an id aware selection has to
 * ask is not what is free at one moment but what is free throughout.
 *
 * Two sources are combined, and combined by taking the larger of the two rather than by adding
 * them, because a job which is running now appears in both:
 *
 *   what is held right now, which is the whole picture when no reservation exists and nothing
 *   reserves ahead, because then no diagram is built at all
 *
 *   the diagram over the window, which is the whole picture when one is built - the scheduler
 *   puts the running jobs into it along with the reservations
 *
 * What is held right now is left out for a window which begins in the future: an instance a job
 * holds at this moment says nothing about a window starting after that job will have ended, and
 * a window in the future is only ever asked about when the diagram is being maintained.
 *
 * @param cr         the resource utilization of the map (RUE_Type)
 * @param now        this moment
 * @param start_time when the window begins, DISPATCH_TIME_NOW for this moment
 * @param duration   how long it lasts
 * @return           the identifiers and how many of each are taken, owned by the caller
 */
ocs::tom::ListPtr<ocs::tom::RESL>
utilization_rsmap_max(const ocs::tom::ConstElement<ocs::tom::RUE> cr, uint64_t now, uint64_t start_time,
                      uint64_t duration) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::RESL> taken;
   if (cr == nullptr) {
      DRETURN(taken);
   }

   // At queue end every job holding something at this moment has ended - that is what queue
   // end means - so only the diagram beyond them counts there, and the diagram's last entry is
   // where that is. The window reaches to the end either way.
   const bool at_queue_end = (start_time == DISPATCH_TIME_QUEUE_END);
   if (start_time == DISPATCH_TIME_NOW) {
      start_time = now;
   }

   // what is held at this moment
   if (!at_queue_end && start_time <= now) {
      const auto in_use = ocs::tom::get<RUE_utilized_now_resource_map_list>(cr);
      if (in_use != nullptr) {
         taken = in_use.clone("rsmap_taken");
      }
   }

   // and what the diagram says about the window
   const auto diagram = ocs::tom::get<RUE_utilized>(cr);
   if (diagram != nullptr) {
      const uint64_t end_time = utilization_endtime(start_time, duration);

      const auto [hit, before] = utilization_find_time_or_prevstart_or_prev(diagram, start_time);

      // the entry in force when the window opens, then every entry which begins inside it
      ocs::tom::ConstElement<ocs::tom::RDE> rde = (hit != nullptr) ? hit : before;
      if (rde == nullptr) {
         // the window opens before the diagram does
         rde = diagram.front();
         if (rde != nullptr && ocs::tom::get<RDE_time>(rde) >= end_time) {
            rde = nullptr;
         }
      }
      while (rde != nullptr) {
         for (const auto ep : ocs::tom::get<RDE_resource_map_list>(rde)) {
            const char *id = ocs::tom::get<RESL_value>(ep);
            const uint32_t amount = ocs::tom::get<RESL_amount>(ep);

            if (taken == nullptr) {
               taken = ocs::tom::ListPtr<ocs::tom::RESL>::create("rsmap_taken");
            }
            auto dst = taken->find_first<RESL_value>(id);
            if (dst == nullptr) {
               dst = taken->emplace_back<RESL_value>(id);
            }
            if (dst != nullptr && ocs::tom::get<RESL_amount>(dst) < amount) {
               ocs::tom::set<RESL_amount>(dst, amount);
            }
         }

         rde = rde.next();
         if (rde != nullptr && ocs::tom::get<RDE_time>(rde) >= end_time) {
            break;
         }
      }
   }

   DRETURN(taken);
}


/**
 * @brief the earliest time one group of a resource map has a number of instances free
 *
 * The per group counterpart of utilization_below(). That one asks when the amount of a
 * consumable drops below a threshold and stays there; this asks when one group of a resource
 * map - one identifier for same=id, the instances sharing a characteristic for
 * same=<characteristic> - has enough instances free and keeps them.
 *
 * The distinction matters and is the reason this cannot be answered from the aggregate. "Four
 * free shares" and "four free shares of one card" are different questions, and the second does
 * not follow from the first at any level of approximation worth having. It can be answered here
 * because each entry of the diagram carries what is in use per identifier at its time, so a
 * group's free count is available at every point the diagram describes.
 *
 * The same group has to serve the request for the whole time, so this is a search per group and
 * the answer is the earliest of them - not the earliest point at which some group or other
 * happens to fit, which a job could not run on.
 *
 * Like utilization_below() it searches from the end of the diagram backwards, so the time it
 * returns is one from which the group stays free rather than a gap which closes again.
 *
 * @param definition the resource map on the host, from EH_consumable_config_list, may be an empty handle
 * @param cr         the resource utilization of the map (RUE_Type), may be an empty handle
 * @param key_name   "id" or the name of a characteristic
 * @param amount     how many instances of one group are needed
 * @param id_expr    nullptr when the request carries no id= parameter, otherwise the expression
 *                   an instance's identifier has to satisfy
 * @param required_props an empty handle when the request names no characteristic, otherwise the
 *                   resolved requirements an instance has to carry
 * @return           the time from which a group can serve the request, DISPATCH_TIME_NOW when
 *                   one can already, std::numeric_limits<uint64_t>::max() when no group ever can
 */
uint64_t
utilization_rsmap_below(const ocs::tom::ConstElement<ocs::tom::CE> definition,
                        const ocs::tom::ConstElement<ocs::tom::RUE> cr, const char *key_name, const uint32_t amount,
                        const char *id_expr, const ocs::tom::ConstList<ocs::tom::CE> required_props) {
   DENTER(TOP_LAYER);

   if (definition == nullptr || amount == 0) {
      DRETURN(DISPATCH_TIME_NOW);
   }

   const auto diagram = (cr != nullptr) ? ocs::tom::get<RUE_utilized>(cr) : ocs::tom::ConstList<ocs::tom::RDE>();
   if (diagram.empty()) {
      // nothing is booked over any period, so whether a group can serve the request is a
      // question about the configuration alone and the answer does not change with time
      DRETURN(DISPATCH_TIME_NOW);
   }

   // With same= the request has to be served by one group, so every group is asked and the
   // earliest of them is the answer. Without it the instances need not agree in anything and
   // there is a single set to ask about - the one an id= expression leaves, or the whole map.
   // A single element list keeps that as one walk rather than a second copy of it.
   ocs::tom::ListPtr<ocs::tom::ST> keys;
   if (key_name != nullptr) {
      keys = centry_rsmap_group_keys(definition, key_name);
   } else {
      keys = ocs::tom::ListPtr<ocs::tom::ST>::create("");
      keys->emplace_back<ST_name>("");
   }
   uint64_t earliest = std::numeric_limits<uint64_t>::max();
   bool found = false;

   for (const auto key_ep : keys.get()) {
      const char *key = (key_name != nullptr) ? ocs::tom::get<ST_name>(key_ep) : nullptr;

      // Walk this group's history backwards. The entry which does not fit ends the search: the
      // one after it is the point from which the group is free to the end of the diagram.
      //
      // The last entry is the state from its time onwards, so a group which does not fit there
      // never becomes free and is no use to this request. That is not the same as the map being
      // full - another group may well serve it - so this group is passed over rather than the
      // search being given up.
      uint64_t when = DISPATCH_TIME_NOW;
      bool never = false;
      for (auto rde = diagram.back(); rde != nullptr; rde = rde.prev()) {
         ocs::tom::ConstList<ocs::tom::RESL> rde_taken = ocs::tom::get<RDE_resource_map_list>(rde);
         const uint32_t free = (key != nullptr)
               ? centry_rsmap_group_free(definition, rde_taken, key_name, key, id_expr,
                                         required_props)
               : centry_rsmap_free(definition, rde_taken, id_expr, required_props);
         if (free >= amount) {
            continue;
         }
         const auto next = rde.next();
         if (next == nullptr) {
            never = true;
         } else {
            when = ocs::tom::get<RDE_time>(next);
         }
         break;
      }
      if (never) {
         continue;
      }

      if (when == DISPATCH_TIME_NOW) {
         // this group is free throughout, so there is nothing to wait for
         DRETURN(DISPATCH_TIME_NOW);
      }
      if (!found || when < earliest) {
         earliest = when;
         found = true;
      }
   }

   DRETURN(found ? earliest : std::numeric_limits<uint64_t>::max());
}

/**
 * @brief Debit a jobs resource utilization
 *
 * A jobs resource utilization is debited into the resource
 * utilization diagram at the given time for the given duration.
 *
 * @param cr Resource utilization entry (RUE_Type)
 * @param start_time Start time of utilization
 * @param duration Duration
 * @param utilization Amount
 * @param job_id Job id
 * @param ja_taskid Task id
 * @param level *_TAG
 * @param object_name The objects name
 * @param type String denoting type of utilization entry.
 * @param for_job reserve for a job (true) or for an advance reservation (false)
 * @param implicit_non_exclusive add implicit entry for non-exclusive jobs requesting a exclusive centry
 * @param binding_touse the topology units the job will use, booked along with
 *                      the amount, or an empty handle
 * @param rsmap_inuse the resource map instances the job holds over the window, or an empty handle
 *
 * @return 0 on success
 *
 * @note MT-NOTE: utilization_add() is not MT safe
 */
int utilization_add(const ocs::tom::Element<ocs::tom::RUE> cr, uint64_t start_time, uint64_t duration,
                    double utilization, uint32_t job_id, uint32_t ja_taskid, uint32_t level, const char *object_name,
                    const char *type, bool for_job, bool implicit_non_exclusive,
                    const ocs::tom::ConstList<ocs::tom::ST> binding_touse,
                    const ocs::tom::ConstList<ocs::tom::RESL> rsmap_inuse) {
   DENTER(TOP_LAYER);
   const char *name = ocs::tom::get<RUE_name>(cr);
   uint64_t end_time;

   // the diagram of the exclusive or of the non-exclusive utilization
   auto resource_diagram = implicit_non_exclusive ? ocs::tom::get<RUE_utilized_nonexclusive>(cr)
                                                  : ocs::tom::get<RUE_utilized>(cr);

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

   bool handle_binding = false;
   ocs::TopologyString binding_to_use_obj;
   if (level == HOST_TAG && strcmp(name, SGE_ATTR_SLOTS) == 0 && binding_touse != nullptr) {
      handle_binding = true;

      // we have at least one binding_to_use element. copy the first one to the binding_touse_dstr
      auto to_use_elem = binding_touse.front();
      ocs::TopologyString binding_to_add(ocs::tom::get<ST_name>(to_use_elem));
      binding_to_use_obj.mark_nodes_as_used_or_unused(binding_to_add, true);

      // add all cores/threads of additional elements to the binding_to_use_dstr
      to_use_elem = to_use_elem.next();
      while (to_use_elem != nullptr) {
         binding_to_add.reset_topology(ocs::tom::get<ST_name>(to_use_elem));
         binding_to_use_obj.mark_nodes_as_used_or_unused(binding_to_add, true);
         to_use_elem = to_use_elem.next();
      }
   }
   DPRINTF("utilization_add: binding to add is %s\n", binding_to_use_obj.to_product_topology_string().c_str());

   // A resource map is held as particular instances, and which ones is as much part of the
   // diagram as the amount: a job scheduled into a window has to know which instances are
   // taken over that window, not only how many. The instances ride on the entry of the map
   // they belong to, the way a core binding rides on the slots entry.
   // a map is configured on an exec host or on the global host, never on a queue
   const bool handle_rsmap = ((level == HOST_TAG || level == GLOBAL_TAG) && !rsmap_inuse.empty());

   // The same call books and gives back, the second with a negated amount - an advance
   // reservation being removed, a job being undebited. The instances follow the amount: they
   // are taken from the start of the window and given back at its end, or the other way round.
   const bool rsmap_taken = (utilization >= 0.0);

   /* ensure the resource diagram is initialized */
   if (resource_diagram == nullptr) {
      auto created = ocs::tom::ListPtr<ocs::tom::RDE>::create(name);
      resource_diagram = created.get();
      if (implicit_non_exclusive) {
         ocs::tom::set<RUE_utilized_nonexclusive>(cr, std::move(created));
      } else {
         ocs::tom::set<RUE_utilized>(cr, std::move(created));
      }
   }

   auto [start, prev] = utilization_find_time_or_prevstart_or_prev(resource_diagram, start_time);

   double util_prev = 0.0;
   const char *binding_prev = nullptr;
   if (start) {
      // if the start element is already there, we can just add the utilization to it
      ocs::tom::add<RDE_amount>(start, utilization);

      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (ocs::tom::get<RDE_binding_inuse>(start) != nullptr) {
            topo_binding_now.reset_topology(ocs::tom::get<RDE_binding_inuse>(start));
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RDE_binding_inuse>(start, topo_binding_now,
                                                                                   binding_to_use_obj, true);
      }
      if (handle_rsmap) {
         utilization_rsmap_set(start, ocs::tom::get<RDE_resource_map_list>(start), rsmap_inuse,
         rsmap_taken);
      }
   } else {
      // no start element found, so we need to create one
      // if there is a previous element, we can add its amount and binding_inuse to the new element
      // otherwise we just create a new element with the utilization. it is the new list beginning
      ocs::tom::ConstList<ocs::tom::RESL> rsmap_prev;
      if (prev != nullptr) {
         util_prev = ocs::tom::get<RDE_amount>(prev);
         binding_prev = ocs::tom::get<RDE_binding_inuse>(prev);
         rsmap_prev = ocs::tom::get<RDE_resource_map_list>(prev);
      }

      // create a new start element with the utilization and binding_inuse
      auto new_start = ocs::tom::ElementPtr<ocs::tom::RDE>::create();
      start = new_start.get();
      ocs::tom::set<RDE_time>(start, start_time);
      ocs::tom::set<RDE_amount>(start, utilization + util_prev);

      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (binding_prev != nullptr) {
            topo_binding_now.reset_topology(binding_prev);
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RDE_binding_inuse>(start, topo_binding_now,
                                                                                   binding_to_use_obj, true);
      }
      if (handle_rsmap) {
         utilization_rsmap_set(start, rsmap_prev, rsmap_inuse, rsmap_taken);
      }

      // Insert the new element with our start time after the previous element that has an earlier start time
      if (prev != nullptr) {
         resource_diagram.insert_after(prev, std::move(new_start));
      } else {
         resource_diagram.push_front(std::move(new_start));
      }
   }

   ocs::tom::Element<ocs::tom::RDE> end;
   prev = start;
   auto thiz = start.next();

   /* find existing end element or the element before */
   while (thiz != nullptr) {
      if (end_time == ocs::tom::get<RDE_time>(thiz)) {
         end = thiz;
         break;
      }
      if (end_time < ocs::tom::get<RDE_time>(thiz)) {
         break;
      }

      /* increment amount of elements in-between */
      ocs::tom::add<RDE_amount>(thiz, utilization);

      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (ocs::tom::get<RDE_binding_inuse>(thiz) != nullptr) {
            topo_binding_now.reset_topology(ocs::tom::get<RDE_binding_inuse>(thiz));
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RDE_binding_inuse>(thiz, topo_binding_now,
                                                                                   binding_to_use_obj, true);
      }
      if (handle_rsmap) {
         utilization_rsmap_set(thiz, ocs::tom::get<RDE_resource_map_list>(thiz), rsmap_inuse,
         rsmap_taken);
      }
      prev = thiz;
      thiz = thiz.next();
   }

   if (end == nullptr) {
      util_prev = ocs::tom::get<RDE_amount>(prev);
      binding_prev = ocs::tom::get<RDE_binding_inuse>(prev);
      const auto rsmap_prev_end = ocs::tom::get<RDE_resource_map_list>(prev);

      auto new_end = ocs::tom::ElementPtr<ocs::tom::RDE>::create();
      ocs::tom::set<RDE_time>(new_end, end_time);
      ocs::tom::set<RDE_amount>(new_end, util_prev - utilization);
      if (handle_binding) {
         ocs::TopologyString topo_binding_now;
         if (binding_prev != nullptr) {
            topo_binding_now.reset_topology(binding_prev);
         }
         ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RDE_binding_inuse>(new_end.get(), topo_binding_now,
                                                                                   binding_to_use_obj, false);
      }
      if (handle_rsmap) {
         utilization_rsmap_set(new_end.get(), rsmap_prev_end, rsmap_inuse, !rsmap_taken);
      }

      resource_diagram.insert_after(prev, std::move(new_end));
   }

   utilization_normalize(resource_diagram);

#if 0
   DSTRING_STATIC(combined_name, 1024);
   sge_dstring_sprintf(&combined_name, "after normalize %s-%s", object_name, name);
   utilization_print(cr, sge_dstring_get_string(&combined_name), handle_binding);
#endif

   DRETURN(0);
}

/**
 * @brief Debit a jobs resource utilization, for a caller with untyped objects (N-X-2)
 *
 * @param cr Resource utilization entry (RUE_Type), checked
 * @param start_time Start time of utilization
 * @param duration Duration
 * @param utilization Amount
 * @param job_id Job id
 * @param ja_taskid Task id
 * @param level *_TAG
 * @param object_name The objects name
 * @param type String denoting type of utilization entry.
 * @param for_job reserve for a job (true) or for an advance reservation (false)
 * @param implicit_non_exclusive add implicit entry for non-exclusive jobs requesting a exclusive centry
 * @param binding_touse the topology units the job will use (ST_Type), checked, or nullptr
 * @param rsmap_inuse the resource map instances the job holds (RESL_Type), checked, or nullptr
 *
 * @return 0 on success
 */
int utilization_add(lListElem *cr, uint64_t start_time, uint64_t duration, double utilization,
                    uint32_t job_id, uint32_t ja_taskid, uint32_t level, const char *object_name,
                    const char *type, bool for_job, bool implicit_non_exclusive,
                    const lList *binding_touse, const lList *rsmap_inuse) {
   return utilization_add(ocs::tom::Element<ocs::tom::RUE>(cr), start_time, duration, utilization, job_id, ja_taskid,
                          level, object_name, type, for_job, implicit_non_exclusive,
                          ocs::tom::ConstList<ocs::tom::ST>(binding_touse),
                          ocs::tom::ConstList<ocs::tom::RESL>(rsmap_inuse));
}

/** @brief finds the element with the specified time or the element before it
 *
 * This function searches for an element in the utilization diagram
 * that matches the specified time. If an element with the exact time is found,
 * that element is the first of the result. If no such element exists,
 * the second is the element before the specified time, or an empty handle if
 * no such element exists.
 *
 *  @param diagram the utilization diagram to search in, may be an empty handle
 *  @param time the time to search for
 *  @return the element with that time and the element before it, each an empty handle if there is none; the
 *          elements can be changed by the caller, as the diagram belongs to the object being booked
 */
static std::pair<ocs::tom::Element<ocs::tom::RDE>, ocs::tom::Element<ocs::tom::RDE>>
utilization_find_time_or_prevstart_or_prev(const ocs::tom::ConstList<ocs::tom::RDE> diagram, const uint64_t time) {
   DENTER(BASIS_LAYER);

   ocs::tom::Element<ocs::tom::RDE> prev;

   // lFirstRW() of a handle gives the mutable element of a read-only list the caller may change
   for (auto thiz = lFirstRW(diagram); thiz != nullptr; thiz = thiz.next()) {
      uint64_t rde_time = ocs::tom::get<RDE_time>(thiz);
      if (time == rde_time) {
         DRETURN(std::make_pair(thiz, prev));
      }
      if (time < rde_time) {
         break;
      }
      prev = thiz;
   }

   DRETURN(std::make_pair(ocs::tom::Element<ocs::tom::RDE>(), prev));
}

/**
 * @brief Normalizes a utilization diagram
 *
 * Leading entries without utilization are removed, and so is every entry that
 * says the same as the one before it: the same amount, the same binding and
 * the same resource map instances.
 *
 * @note The diagram does not need to vanish (nullptr list) as long as
 *       utilization is only added.
 *
 * @param diagram the utilization diagram
 */
static void utilization_normalize(const ocs::tom::List<ocs::tom::RDE> diagram) {
   DENTER(BASIS_LAYER);

   auto thiz = diagram.front();

   while (thiz != nullptr && ocs::tom::get<RDE_amount>(thiz) == 0.0) {
      diagram.erase(thiz);
      thiz = diagram.front();
   }

   if (thiz == nullptr) {
      DRETURN_VOID;
   }

   auto next = thiz.next();
   if (next == nullptr) {
      DRETURN_VOID;
   }

   double util_prev = ocs::tom::get<RDE_amount>(thiz);
   const char *bind_prev = ocs::tom::get<RDE_binding_inuse>(thiz);
   auto rsmap_prev = ocs::tom::get<RDE_resource_map_list>(thiz);

   while ((thiz = next) != nullptr) {
      next = thiz.next();

      double util = ocs::tom::get<RDE_amount>(thiz);
      const char *bind = ocs::tom::get<RDE_binding_inuse>(thiz);
      const auto rsmap = ocs::tom::get<RDE_resource_map_list>(thiz);

      // values need to be the same, and binding and the resource map instances also need to be
      // the same so that we can remove an entry
      if (util_prev == util &&
          ((bind_prev == nullptr && bind == nullptr) ||
           (bind_prev != nullptr && bind != nullptr && strcmp(bind_prev, bind) == 0)) &&
          utilization_rsmap_equal(rsmap_prev, rsmap)) {
         diagram.erase(thiz);
      } else {
         util_prev = util;
         bind_prev = bind;
         rsmap_prev = rsmap;
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Raises a utilization to what core binding actually blocks
 *
 * A job bound to topology units can make a resource unusable beyond the
 * amount it booked - the remaining slots exist but have no units left to bind
 * to. This returns the larger of the booked utilization and the one implied
 * by the binding.
 *
 * @param[in]     a             the assignment, or nullptr to leave `util` as
 *                              it is
 * @param[in]     host          the execution host, or nullptr as above
 * @param[in,out] binding_inuse the topology units already taken
 * @param[in]     util          the utilization booked so far
 * @param[in]     total         the total capacity of the resource
 * @param[in]     slots         the number of slots in question
 *
 * @return the utilization to work with
 */
static double increase_util_depending_on_binding(const sge_assignment_t *a,
                                                 const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                 ocs::TopologyString &binding_inuse, double util, double total,
                                                 double slots) {
   // no binding string => no util change
   if (a != nullptr && host != nullptr) {
      double util_candidate = total - ocs::BindingSchedd::test_strategy(a, host, slots, binding_inuse);
      util = std::max(util, util_candidate);
   }
   return util;
}

/**
 * @brief Determine utilization at queue end time
 *
 * Determine utilization at queue end time. Jobs that last until
 * ever can cause a non-zero utilization.
 *
 * @param a                the assignment, or nullptr when no binding has to
 *                         be taken into account
 * @param host             the execution host, or nullptr as above
 * @param cr               Resource utilization entry (`RUE_utilized`)
 * @param total            the total capacity of the resource
 * @param request          the amount the job requests
 * @param slots            the number of slots in question
 * @param for_excl_request For exclusive request
 * @param[out] binding_inuse receives the topology units in use at that time
 *
 * @return queue end utilization
 *
 * @note MT-NOTE: utilization_queue_end() is MT safe
 */
static double utilization_queue_end(const sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::EH> host,
                                    const ocs::tom::ConstElement<ocs::tom::RUE> cr, double total, double request,
                                    double slots, bool for_excl_request,
                                    ocs::TopologyString &binding_inuse) {
   DENTER(TOP_LAYER);

#if 1
   utilization_print(cr, "the object", false);
#endif

   double max = 0.0;
   const char *binding_inuse_str = nullptr;
   const auto ep = ocs::tom::get<RUE_utilized>(cr).back();
   if (ep != nullptr) {
      if (ocs::tom::get<RDE_time>(ep) != std::numeric_limits<uint64_t>::max()) {
         max = ocs::tom::get<RDE_amount>(ep);
         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(ep);
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
         }
      } else {
         max = ocs::tom::get<RDE_amount>(ep.prev());
         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(ep.prev());
         if (binding_inuse_str != nullptr) {
            ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
         }
      }
   }

   if (for_excl_request) {
      double max_nonexclusive;
      const char *binding_inuse_nonexclusive_str = nullptr;
      const auto ep = ocs::tom::get<RUE_utilized_nonexclusive>(cr).back();
      if (ep != nullptr) {
         if (ocs::tom::get<RDE_time>(ep) != std::numeric_limits<uint64_t>::max()) {
            max_nonexclusive = ocs::tom::get<RDE_amount>(ep);
            binding_inuse_nonexclusive_str = ocs::tom::get<RDE_binding_inuse>(ep);
            if (binding_inuse_nonexclusive_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_nonexclusive_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, tmp_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else {
            max_nonexclusive = ocs::tom::get<RDE_amount>(ep.prev());
            binding_inuse_nonexclusive_str = ocs::tom::get<RDE_binding_inuse>(ep.prev());
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
 * @param a               the assignment, or nullptr when no binding has to be
 *                        taken into account
 * @param host            the execution host, or nullptr as above
 * @param cr              Resource utilization entry (`RUE_utilized`)
 * @param start_time      Start time of the timeframe
 * @param duration        Duration of timeframe
 * @param total           the total capacity of the resource
 * @param request         the amount the job requests
 * @param slots           the number of slots in question
 * @param for_excl_request Whether to check for exclusive requests
 * @param[out] combined_binding_inuse the topology units in use over the
 *                        timeframe; only filled for slots at host level
 *
 * @return Maximum utilization value
 */
double
utilization_max(const sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::EH> host,
                const ocs::tom::ConstElement<ocs::tom::RUE> cr, uint64_t start_time, uint64_t duration, double total,
                double request, double slots,
                bool for_excl_request, ocs::TopologyString &combined_binding_inuse) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::RDE> rde;
   double max = 0.0;
   uint64_t end_time = utilization_endtime(start_time, duration);
   const char *binding_inuse_str = nullptr;
   bool found_requested_max = false;

   // ----------------------------------------------------------------------------------------------------------------
   // find current utilization
   if (start_time == DISPATCH_TIME_NOW) {
      max = ocs::tom::get<RUE_utilized_now>(cr);
      binding_inuse_str = ocs::tom::get<RUE_utilized_now_binding_inuse>(cr);
      if (binding_inuse_str != nullptr) {
         ocs::TopologyString tmp_binding_inuse(binding_inuse_str);
         max = increase_util_depending_on_binding(a, host, tmp_binding_inuse, max, total, slots);
      }

      if (for_excl_request) {
         max = std::max(ocs::tom::get<RUE_utilized_now_nonexclusive>(cr), max);
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

      const auto [start, prev] =
         utilization_find_time_or_prevstart_or_prev(ocs::tom::get<RUE_utilized>(cr), start_time);
      uint64_t time = 0;
      if (start != nullptr) {
         rde = start.next();
         max = ocs::tom::get<RDE_amount>(start);
         time = ocs::tom::get<RDE_time>(start);
         DPRINTF("utilization_max: found entry with exact start time  %s (" sge_u32 ")\n", sge_ctime64(time, &dstr), time);

         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(start);
         if (binding_inuse_str != nullptr) {
            combined_binding_inuse.reset_topology(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, combined_binding_inuse, max, total, slots);
         }
      } else if (prev != nullptr) {
         rde = prev.next();
         max = ocs::tom::get<RDE_amount>(prev);
         time = ocs::tom::get<RDE_time>(prev);
         DPRINTF("utilization_max: found entry before start time  %s (" sge_u32 ")\n", sge_ctime64(time, &dstr), time);

         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(prev);
         if (binding_inuse_str != nullptr) {
            combined_binding_inuse.reset_topology(binding_inuse_str);
            max = increase_util_depending_on_binding(a, host, combined_binding_inuse, max, total, slots);
         }
      } else {
         rde = ocs::tom::get<RUE_utilized>(cr).front();
      }

      /* now watch out for the maximum before end time */
      while (rde != nullptr && end_time > ocs::tom::get<RDE_time>(rde)) {
         double candidate_max = ocs::tom::get<RDE_amount>(rde);
         uint64_t candidate_time = ocs::tom::get<RDE_time>(rde);

         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(rde);
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
         rde = rde.next();
      }

      if (for_excl_request) {
         double max_nonexclusive = 0.0;
         uint64_t time_nonexclusive = 0;
         ocs::TopologyString combined_binding_inuse_nonexclusive;

         const auto [start_ne, prev_ne] =
            utilization_find_time_or_prevstart_or_prev(ocs::tom::get<RUE_utilized_nonexclusive>(cr), start_time);
         if (start_ne != nullptr) {
            rde = start_ne.next();
            max_nonexclusive = ocs::tom::get<RDE_amount>(start_ne);
            time_nonexclusive = ocs::tom::get<RDE_time>(start_ne);
            binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(start_ne);
            if (binding_inuse_str != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else if (prev_ne != nullptr) {
            rde = prev_ne.next();

            max_nonexclusive = ocs::tom::get<RDE_amount>(prev_ne);
            time_nonexclusive = ocs::tom::get<RDE_time>(prev_ne);
            binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(prev_ne);
            if (binding_inuse_str != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str);
               max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, max_nonexclusive, total, slots);
            }
         } else {
            rde = ocs::tom::get<RUE_utilized_nonexclusive>(cr).front();
         }

         /* now watch out for the maximum before end time */
         while (rde != nullptr && end_time > ocs::tom::get<RDE_time>(rde)) {
            double candidate_max_nonexclusive = ocs::tom::get<RDE_amount>(rde);

            binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(rde);
            if (binding_inuse_str != nullptr) {
               ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_str);
               combined_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(tmp_binding_inuse_nonexclusive, true);
               candidate_max_nonexclusive = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, candidate_max_nonexclusive, total, slots);
            }

            if (candidate_max_nonexclusive > max_nonexclusive) {
               max_nonexclusive = candidate_max_nonexclusive;
               time_nonexclusive = ocs::tom::get<RDE_time>(rde);
            }
            rde = rde.next();
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

/** @brief Whether the resource stays below max_util for as long as the job would run
 *
 * The question a reservation has to answer is not whether a resource is free from some moment
 * onwards for good, but whether it is free long enough for the job to run. Fills in the binding
 * pattern which fits in that window when one does.
 */
static bool
utilization_fits_duration(const sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::EH> host,
                          const ocs::tom::ConstElement<ocs::tom::RUE> cr,
                          uint64_t start, double max_util, double total, double slots,
                          bool for_excl_request, ocs::TopologyString &binding_inuse) {
   ocs::TopologyString candidate_binding;

   const double peak = utilization_max(a, host, cr, start, a->duration, total, total - max_util,
                                       slots, for_excl_request, candidate_binding);
   if (peak > max_util) {
      return false;
   }

   binding_inuse.reset_topology(candidate_binding.to_string(true, true, true));
   return true;
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
 * @note The backward search finds a time from which the utilization stays below max_util until the end
 * of the diagram. That is more than a job needs, and it passes over a free window which the utilization
 * closes again later - a queue calendar creates exactly such a window (CS-1000). When the assignment
 * carries a duration, the result is therefore refined afterwards to the earliest window which is long
 * enough for the job, and the binding pattern reported belongs to that window.
 *
 * Without an assignment, or for a job whose duration reaches the end of the diagram anyway, the older
 * guarantee is what remains: between the returned time and the end of the diagram the utilization is
 * below max_util, and for slots binding on hosts one binding pattern fits there.
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
uint64_t
utilization_below(const sge_assignment_t *a, const ocs::tom::ConstElement<ocs::tom::EH> host,
                  const ocs::tom::ConstElement<ocs::tom::RUE> cr, double max_util, double total, double slots,
                  const char *object_name, bool for_excl_request, ocs::TopologyString& combined_binding_inuse) {
   DENTER(TOP_LAYER);
   double util = 0;
   uint64_t when = DISPATCH_TIME_NOW;
   const char *binding_inuse_str = nullptr;
   DSTRING_STATIC(dstr, 64);

#if 1
   DPRINTF("utilization_below:\n");
   utilization_print(cr, object_name, true);
#endif

   // search backward starting from the diagram's end
   bool reuse_prev_util = false;
   for (auto rde = ocs::tom::get<RUE_utilized>(cr).back(); rde != nullptr; rde = rde.prev()) {

      // avoid doing work twice: reuse fetched data once and look at each binding pattern also only once
      if (!reuse_prev_util) {
         util = ocs::tom::get<RDE_amount>(rde);
         binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(rde);
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
         const auto p = rde.prev();
         if (p != nullptr) {

            util = ocs::tom::get<RDE_amount>(p);
            binding_inuse_str = ocs::tom::get<RDE_binding_inuse>(p);
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
               DPRINTF("utilization_below: found max utilization at due to combined binding in resource diagram %s\n", sge_ctime64(ocs::tom::get<RDE_time>(p), &dstr));
               when = ocs::tom::get<RDE_time>(rde);
               break;
            }
         }
      }
   }

   // repeat the same from above for non-exclusive entries if requested
   ocs::TopologyString combined_binding_inuse_nonexclusive;
   reuse_prev_util = false;
   if (for_excl_request) {
      uint64_t when_nonexclusive = DISPATCH_TIME_NOW;
      const char *binding_inuse_str_nonexclusive = nullptr;

      for (auto rde = ocs::tom::get<RUE_utilized_nonexclusive>(cr).back(); rde != nullptr; rde = rde.prev()) {
         if (!reuse_prev_util) {
            util = ocs::tom::get<RDE_amount>(rde);
            binding_inuse_str_nonexclusive = ocs::tom::get<RDE_binding_inuse>(rde);
            if (binding_inuse_str_nonexclusive != nullptr) {
               combined_binding_inuse_nonexclusive.reset_topology(binding_inuse_str_nonexclusive);
               DPRINTF("combined_binding_inuse: %s\n", combined_binding_inuse_nonexclusive.to_product_topology_string().c_str());
               util = increase_util_depending_on_binding(a, host, combined_binding_inuse_nonexclusive, util, total, slots);
            }
         } else {
            if (binding_inuse_str_nonexclusive != nullptr) {
               ocs::TopologyString combined_binding_to_add(binding_inuse_str_nonexclusive);
               combined_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(combined_binding_to_add, true);
               DPRINTF("combined_binding_inuse: %s\n", combined_binding_inuse_nonexclusive.to_product_topology_string().c_str());
            }
         }

         if (util <= max_util) {
            const auto p = rde.prev();
            if (p != nullptr) {

               util = ocs::tom::get<RDE_amount>(p);
               binding_inuse_str_nonexclusive = ocs::tom::get<RDE_binding_inuse>(p);
               if (binding_inuse_str_nonexclusive != nullptr) {
                  ocs::TopologyString tmp_binding_inuse_nonexclusive(binding_inuse_str_nonexclusive);
                  tmp_binding_inuse_nonexclusive.mark_nodes_as_used_or_unused(combined_binding_inuse_nonexclusive, true);
                  util = increase_util_depending_on_binding(a, host, tmp_binding_inuse_nonexclusive, util, total, slots);
               }
               reuse_prev_util = true;

               if (util > max_util) {
                  DPRINTF("utilization_below: found max utilization-nonexclusive at due to combined binding in resource diagram %s\n", sge_ctime64(ocs::tom::get<RDE_time>(p), &dstr));
                  when_nonexclusive = ocs::tom::get<RDE_time>(rde);
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

   // CS-1000: the search above answers a stronger question than a job asks. It finds a time from
   // which the resource stays free until the end of the diagram, while a job only needs a window
   // as long as it runs. A calendar which closes the queue later on books the whole queue for
   // that period, so the end of the diagram lies behind the closed period and every free window
   // in front of it is passed over - the reservation is then made for the moment the queue
   // reopens, far too late to keep anything free.
   //
   // A job whose duration reaches the end of the diagram from wherever it starts asks the same
   // question as the search above, so for it this finds nothing new.
   if (when != DISPATCH_TIME_NOW && a != nullptr && a->duration > 0) {
      uint64_t earlier = when;

      if (utilization_fits_duration(a, host, cr, a->now, max_util, total, slots, for_excl_request,
                                    combined_binding_inuse)) {
         earlier = a->now;
      } else {
         for (const auto candidate_rde : ocs::tom::get<RUE_utilized>(cr)) {
            const uint64_t candidate = ocs::tom::get<RDE_time>(candidate_rde);

            // a window can only open where an entry of the diagram begins, so those are the
            // only moments worth asking about
            if (candidate <= a->now) {
               continue;
            }
            if (candidate >= when) {
               break;
            }
            if (utilization_fits_duration(a, host, cr, candidate, max_util, total, slots,
                                          for_excl_request, combined_binding_inuse)) {
               earlier = candidate;
               break;
            }
         }
      }

      if (earlier < when) {
         DPRINTF("utilization_below: a window of " sge_u64 " fits already at %s, before " sge_u64 "\n",
                 a->duration, sge_ctime64(earlier, &dstr), when);
         when = earlier;
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

/**
 * @brief Debit assignements' utilization to all schedules
 *
 * The resouce utilization of an assignment is debited into the schedules
 * of global, host and queue instance resource containers and limitation
 * rule sets. For parallel jobs debitation is made also with the parallel
 * environement schedule.
 *
 * @param a The assignment
 * @param type A string that is used to monitor assignment type
 * @param for_job_scheduling utilize for job or for advance reservation
 *
 * @return always 0
 *
 * @note MT-NOTE: add_job_utilization() is MT safe
 */
int add_job_utilization(const sge_assignment_t *a, const char *type, bool for_job_scheduling) {
   // the resource map instances this assignment holds, so that the diagram records which ones
   // are taken over its window and not only how many
   const auto job_granted_resources = (a->ja_task != nullptr)
                                      ? ocs::tom::get<JAT_granted_resources_list>(a->ja_task)
                                      : ocs::tom::List<ocs::tom::GRU>();

   DENTER(TOP_LAYER);

   if (a->ar_id == 0) {
      /* debit non-AR-job */

      dstring rue_name = DSTRING_INIT;
      /* parallel environment  */
      if (a->pe) {
         utilization_add(ocs::tom::get<PE_resource_utilization>(a->pe).front(), a->start, a->duration, a->slots,
                         a->job_id, a->ja_task_id, PE_TAG, ocs::tom::get<PE_name>(a->pe), type,
                         for_job_scheduling, false, ocs::tom::ConstList<ocs::tom::ST>(),
                         ocs::tom::ConstList<ocs::tom::RESL>());
      }

      bool is_master_task = true;
      const char *last_eh_name = nullptr;
      bool do_per_global_host_booking = true;
      for (const auto gdil_ep : ocs::tom::ConstList<ocs::tom::JG>(*a->gdil)) {
         int slots = ocs::tom::get<JG_slots>(gdil_ep);
         const char *eh_name = ocs::tom::get<JG_qhostname>(gdil_ep);
         const char *qname = ocs::tom::get<JG_qname>(gdil_ep);
         const char* pe = (a->pe)?ocs::tom::get<PE_name>(a->pe):nullptr;
         const char *queue_instance = ocs::tom::get<JG_qname>(gdil_ep);
         char *queue = cqueue_get_name_from_qinstance(queue_instance);
         bool do_per_host_booking = host_do_per_host_booking(&last_eh_name, eh_name);

         // global
         // we really need to do it per gdil_ep, because we have to consider is_master_task and ign_sreq_on_mhost
         rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, a->gep, a->centry_list, slots,
                                SGE_GLOBAL_NAME,
                                a->start, a->duration, GLOBAL_TAG, for_job_scheduling,
                                is_master_task,
                                do_per_global_host_booking, job_granted_resources);

         // host
         if (const auto hep = a->host_list.find_first<EH_name>(ocs::tom::Host(eh_name)); hep != nullptr) {
            rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, hep, a->centry_list, slots,
                                   eh_name, a->start,
                                   a->duration, HOST_TAG, for_job_scheduling, is_master_task,
                                   do_per_host_booking,
                                   job_granted_resources);
         }

         // queue
         if (const auto qep = a->queue_list.find_first<QU_full_name>(qname); qep != nullptr) {
            /* 
             * The nullptr case happens in case of queues that were sorted out b/c they
             * are unknown, in some suspend state or in calendar disable state. As long 
             * as we do not intend to schedule future resource utilization for those 
             * queues it's valid to simply ignore resource utilizations decided in former 
             * schedule runs: running/suspneded/migrating jobs.
             * 
             */
            rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, qep, a->centry_list, slots,
                                   qname, a->start,
                                   a->duration, QUEUE_TAG, for_job_scheduling, is_master_task,
                                   false,
                                   job_granted_resources);
         }

         /* resource quotas */
         for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(a->rqs_list)) {
            if (!ocs::tom::get<RQS_enabled>(rqs)) {
               continue;
            }

            const auto rule = rqs_get_matching_rule(rqs, a->user, a->group, a->grp_list, a->project, pe, eh_name,
                                                    queue, a->acl_list, a->hgrp_list, nullptr);
            if (rule != nullptr) {

               rqs_get_rue_string(&rue_name, rule, a->user, a->project, eh_name, queue, pe);

               rqs_add_job_utilization(a->job, a->pe, a->ja_task_id, type, rule, rue_name,
                                       a->centry_list, slots, ocs::tom::get<RQS_name>(rqs),
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
         const auto ar_global_host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(a->ar, SGE_GLOBAL_NAME);

         bool is_master_task = true;
         bool do_per_global_host_booking = true;
         const char *last_eh_name = nullptr;
         for (const auto gdil_ep : ocs::tom::ConstList<ocs::tom::JG>(*a->gdil)) {
            int slots = ocs::tom::get<JG_slots>(gdil_ep);
            const char *qname = ocs::tom::get<JG_qname>(gdil_ep);
            const char *eh_name = ocs::tom::get<JG_qhostname>(gdil_ep);
            bool do_per_host_booking = host_do_per_host_booking(&last_eh_name, eh_name);

            // CS-1745: what about the parallel environment utilization for AR jobs?

            if (const auto qep = ocs::tom::find_first_in_rw<AR_reserved_queues, QU_full_name>(a->ar, qname);
                qep != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, qep, a->centry_list, slots,
                                      qname, a->start,
                                      a->duration, QUEUE_TAG, for_job_scheduling, is_master_task,
                                      do_per_host_booking,
                                      job_granted_resources);
            }
            if (ar_global_host != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, ar_global_host, a->centry_list, slots,
                                      SGE_GLOBAL_NAME, a->start,
                                      a->duration, HOST_TAG, for_job_scheduling, is_master_task,
                                      do_per_global_host_booking, job_granted_resources);
            }
            const auto host = ocs::tom::find_first_in_rw<AR_reserved_hosts, EH_name>(a->ar, eh_name);
            if (host != nullptr) {
               rc_add_job_utilization(gdil_ep, a->job, a->pe, a->ja_task_id, type, host, a->centry_list, slots,
                                      eh_name, a->start,
                                      a->duration, HOST_TAG, for_job_scheduling, is_master_task,
                                      do_per_host_booking,
                                      job_granted_resources);
            }
            is_master_task = false;
            do_per_global_host_booking = false;
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Books a job's consumables into the diagrams of one object
 *
 * The object is a host (the global one included) or a queue instance. Its
 * configuration and utilization fields are template arguments, which is why
 * the same implementation serves both levels; the public overloads of
 * rc_add_job_utilization() name them.
 *
 * @tparam T       the object type, ocs::tom::EH or ocs::tom::QU
 * @tparam ConfigF the field of the object holding its consumable configuration
 * @tparam ActualF the field of the object holding its utilization
 * @param[in]     gdil                the granted destination identifier the
 *                                    booking is made for
 * @param[in]     jep                 the job (`JB_Type`)
 * @param[in]     pe                  the granted parallel environment, or an
 *                                    empty handle
 * @param[in]     task_id             id of the array task
 * @param[in]     type                why the utilization is booked, one of
 *                                    the SCHEDULING_RECORD_ENTRY_TYPE_*
 *                                    strings
 * @param[in,out] ep                  the object to book on, or an empty
 *                                    handle, which is an error
 * @param[in]     centry_list         the system wide attribute configuration
 * @param[in]     slots               number of slots granted here
 * @param[in]     obj_name            name of the object, for messages
 * @param[in]     start_time          start of the utilization
 * @param[in]     duration            duration of the utilization
 * @param[in]     tag                 the level tag - queue, host or global
 * @param[in]     for_job_scheduling  book for a job (true) or for an advance
 *                                    reservation (false)
 * @param[in]     is_master_task      whether the master task runs here
 * @param[in]     do_per_host_booking whether the per host consumables have to
 *                                    be booked in this call
 * @param[in]     granted_resources_list the granted resources of the task
 *                                    (`GRU_Type`), or an empty handle
 *
 * @return the number of modifications made, -1 if a configured attribute is
 *         missing in the complexes
 */
template <typename T, auto ConfigF, auto ActualF>
static int
rc_add_job_utilization_impl(const ocs::tom::ConstElement<ocs::tom::JG> gdil,
                            const ocs::tom::ConstElement<ocs::tom::JB> jep,
                            const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint32_t task_id, const char *type,
                            const ocs::tom::Element<T> ep, const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                            const int slots, const char *obj_name, const uint64_t start_time, const uint64_t duration,
                            const uint32_t tag, const bool for_job_scheduling, const bool is_master_task,
                            const bool do_per_host_booking,
                            const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::RUE> cr;
   int mods = 0;

   if (ep == nullptr) {
      ERROR("rc_add_job_utilization nullptr object " "(job " sge_u32 " obj %s type %s) slots %d ep %p\n",
            ocs::tom::get<JB_job_number>(jep), obj_name, type, slots, static_cast<void *>(ep.raw()));
      DRETURN(0);
   }

   if (slots == 0) {
      ERROR("rc_add_job_utilization 0 slot amount " "(job " sge_u32 " obj %s type %s) slots %d ep %p\n",
            ocs::tom::get<JB_job_number>(jep), obj_name, type, slots, static_cast<void *>(ep.raw()));
      DRETURN(0);
   }

   uint32_t job_id = ocs::tom::get<JB_job_number>(jep);

   for (const auto cr_config : ocs::tom::get<ConfigF>(ep)) {
      const char *name = ocs::tom::get<CE_name>(cr_config);

      /* search default request */  
      const auto dcep = ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, name));
      if (dcep == nullptr) {
         ERROR(MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S , name);
         DRETURN(-1);
      }

      uint32_t consumable = ocs::tom::get<CE_consumable>(dcep);

      if (consumable != CONSUMABLE_NO) {
         /* ensure attribute is in actual list */
         cr = ocs::tom::find_first_in_rw<ActualF, RUE_name>(ep, name);
         if (cr == nullptr) {
            cr = ocs::tom::emplace_back_in<ActualF, RUE_name>(ep, name);
            /* CE_double is implicitly set to zero */
         }
      }

      // The instances of a resource map which this booking takes, so that the diagram says
      // which ones are held over the window and not only how many. They were chosen when the
      // assignment was granted and are looked up here by the map and the object they were
      // granted on.
      ocs::tom::ConstList<ocs::tom::RESL> rsmap_inuse;
      if ((tag == HOST_TAG || tag == GLOBAL_TAG) && static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(dcep)) == ocs::CEntry::Type::RSMAP) {
         const auto gru = gru_list_search(granted_resources_list, name, obj_name);
         if (gru != nullptr && static_cast<ocs::GrantedResources::Type>(ocs::tom::get<GRU_type>(gru)) == ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
            rsmap_inuse = ocs::tom::get<GRU_resource_map_list>(gru);
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
            ocs::tom::ConstList<ocs::tom::ST> binding_to_use;
            if (tag == HOST_TAG) {
               binding_to_use = ocs::tom::get<JG_binding_to_use>(gdil);
            }
            utilization_add(cr, start_time, duration, debit_slots * dval, job_id, task_id, tag,
                            obj_name, type, for_job_scheduling, false, binding_to_use, rsmap_inuse);
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
                                  obj_name, type, for_job_scheduling, false, ocs::tom::get<JG_binding_to_use>(gdil),
                               rsmap_inuse);
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
            if (is_master_task && ocs::tom::get<PE_ignore_slave_requests_on_master_host>(pe)) {
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
                               obj_name, type, for_job_scheduling, false, ocs::tom::get<JG_binding_to_use>(gdil),
                               rsmap_inuse);
               mods++;
               did_booking = true;
            }
         }
      }

      // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
      if (!did_booking && ocs::tom::get<CE_relop>(dcep) == CMPLXEXCL_OP) {
         dval = 1.0;
         /* update RUE_utilized resource diagram to reflect jobs utilization */
         utilization_add(cr, start_time, duration, debit_slots * dval, job_id, task_id, tag,
                         obj_name, type, for_job_scheduling, true, ocs::tom::get<JG_binding_to_use>(gdil),
                         ocs::tom::ConstList<ocs::tom::RESL>());
         mods++;
      }
   }

   DRETURN(mods);
}

/**
 * @brief Books a job's consumables into the diagrams of a host, the global host included
 *
 * @param[in]     gdil                the granted destination identifier the booking is made for
 * @param[in]     jep                 the job
 * @param[in]     pe                  the granted parallel environment, or an empty handle
 * @param[in]     task_id             id of the array task
 * @param[in]     type                why the utilization is booked
 * @param[in,out] host                the host to book on
 * @param[in]     centry_list         the system wide attribute configuration
 * @param[in]     slots               number of slots granted here
 * @param[in]     obj_name            name of the object, for messages
 * @param[in]     start_time          start of the utilization
 * @param[in]     duration            duration of the utilization
 * @param[in]     tag                 the level tag
 * @param[in]     for_job_scheduling  book for a job (true) or for an advance reservation (false)
 * @param[in]     is_master_task      whether the master task runs here
 * @param[in]     do_per_host_booking whether the per host consumables have to be booked in this call
 * @param[in]     granted_resources_list the granted resources of the task, or an empty handle
 *
 * @return the number of modifications made, -1 if a configured attribute is missing in the complexes
 */
int
rc_add_job_utilization(const ocs::tom::ConstElement<ocs::tom::JG> gdil, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                       const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint32_t task_id, const char *type,
                       const ocs::tom::Element<ocs::tom::EH> host, const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                       const int slots, const char *obj_name, const uint64_t start_time, const uint64_t duration,
                       const uint32_t tag, const bool for_job_scheduling, const bool is_master_task,
                       const bool do_per_host_booking,
                       const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list) {
   return rc_add_job_utilization_impl<ocs::tom::EH, EH_consumable_config_list, EH_resource_utilization>(
      gdil, jep, pe, task_id, type, host, centry_list, slots, obj_name, start_time, duration, tag, for_job_scheduling,
      is_master_task, do_per_host_booking, granted_resources_list);
}

/**
 * @brief Books a job's consumables into the diagrams of a queue instance
 *
 * @param[in]     gdil                the granted destination identifier the booking is made for
 * @param[in]     jep                 the job
 * @param[in]     pe                  the granted parallel environment, or an empty handle
 * @param[in]     task_id             id of the array task
 * @param[in]     type                why the utilization is booked
 * @param[in,out] queue               the queue instance to book on
 * @param[in]     centry_list         the system wide attribute configuration
 * @param[in]     slots               number of slots granted here
 * @param[in]     obj_name            name of the object, for messages
 * @param[in]     start_time          start of the utilization
 * @param[in]     duration            duration of the utilization
 * @param[in]     tag                 the level tag
 * @param[in]     for_job_scheduling  book for a job (true) or for an advance reservation (false)
 * @param[in]     is_master_task      whether the master task runs here
 * @param[in]     do_per_host_booking whether the per host consumables have to be booked in this call
 * @param[in]     granted_resources_list the granted resources of the task, or an empty handle
 *
 * @return the number of modifications made, -1 if a configured attribute is missing in the complexes
 */
int
rc_add_job_utilization(const ocs::tom::ConstElement<ocs::tom::JG> gdil, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                       const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint32_t task_id, const char *type,
                       const ocs::tom::Element<ocs::tom::QU> queue, const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                       const int slots, const char *obj_name, const uint64_t start_time, const uint64_t duration,
                       const uint32_t tag, const bool for_job_scheduling, const bool is_master_task,
                       const bool do_per_host_booking,
                       const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list) {
   return rc_add_job_utilization_impl<ocs::tom::QU, QU_consumable_config_list, QU_resource_utilization>(
      gdil, jep, pe, task_id, type, queue, centry_list, slots, obj_name, start_time, duration, tag, for_job_scheduling,
      is_master_task, do_per_host_booking, granted_resources_list);
}

/**
 * @brief Debit assignment's utilization in a limitation
 *
 * @param jep job element (JB_Type), or an empty handle
 * @param pe the granted parallel environment, or an empty handle
 * @param task_id task id to debit
 * @param type String denoting type of utilization entry
 * @param rule limitation rule (RQR_Type)
 * @param rue_name rue_name where to debit
 * @param centry_list master centry list (CE_Type)
 * @param slots slots to debit
 * @param obj_name name of the object where to debit
 * @param start_time start time of utilization
 * @param duration end time of utilization
 * @param is_master_task is this the master task going to be debit
 * @param do_per_host_booking whether the per host consumables have to be booked in this call
 *
 * @return amount of modified limits
 *
 * @note MT-NOTE: rqs_add_job_utilization() is MT safe
 *
 * @see #rc_add_job_utilization, #add_job_utilization
 */
static int
rqs_add_job_utilization(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::PE> pe,
                        const uint32_t task_id, const char *type, const ocs::tom::Element<ocs::tom::RQR> rule,
                        const dstring rue_name, const ocs::tom::ConstList<ocs::tom::CE> centry_list, const int slots,
                        const char *obj_name, const uint64_t start_time, const uint64_t duration,
                        const bool is_master_task, const bool do_per_host_booking) {
   DENTER(TOP_LAYER);

   int mods = 0;

   if (jep != nullptr) {
      uint32_t job_id = ocs::tom::get<JB_job_number>(jep);

      for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {
         ocs::tom::Element<ocs::tom::RUE> rue_elem;

         const char *centry_name = ocs::tom::get<RQRL_name>(limit);
         
         const auto raw_centry =
            ocs::tom::Element<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, centry_name));
         if (raw_centry == nullptr) {
            /* ignoring not defined centry */
            continue;
         }

         uint32_t consumable = ocs::tom::get<CE_consumable>(raw_centry);

         if (consumable != CONSUMABLE_NO) {
            rue_elem = ocs::tom::find_first_in_rw<RQRL_usage, RUE_name>(limit, sge_dstring_get_string(&rue_name));
            if (rue_elem == nullptr) {
               rue_elem = ocs::tom::emplace_back_in<RQRL_usage, RUE_name>(limit, sge_dstring_get_string(&rue_name));
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
                               RQS_TAG, obj_name, type, true, false, ocs::tom::ConstList<ocs::tom::ST>(),
                               ocs::tom::ConstList<ocs::tom::RESL>());
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
                                     RQS_TAG, obj_name, type, true, false, ocs::tom::ConstList<ocs::tom::ST>(),
                                     ocs::tom::ConstList<ocs::tom::RESL>());
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
               if (is_master_task && ocs::tom::get<PE_ignore_slave_requests_on_master_host>(pe)) {
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
                                     RQS_TAG, obj_name, type, true, false, ocs::tom::ConstList<ocs::tom::ST>(),
                                     ocs::tom::ConstList<ocs::tom::RESL>());
                     mods++;
                     did_booking = true;
                  }
               }
            }
         }

         // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
         if (!did_booking && ocs::tom::get<CE_relop>(raw_centry) == CMPLXEXCL_OP) {
            dval = 1.0;
            utilization_add(rue_elem, start_time, duration, debit_slots * dval, job_id, task_id,
                            RQS_TAG, obj_name, type, true, true, ocs::tom::ConstList<ocs::tom::ST>(),
                            ocs::tom::ConstList<ocs::tom::RESL>());
            mods++;
         }
      }
   }

   DRETURN(mods);
}

/**
 * @brief Debit the tasks of a list of running or suspended jobs in the resource schedules
 * @param job_list the jobs
 * @param suspended whether the jobs are suspended, for the scheduling record
 * @param pe_list the parallel environments
 * @param host_list the execution hosts
 * @param queue_list the queue instances
 * @param rqs_list the resource quota sets
 * @param centry_list the complex entries
 * @param acl_list the access lists
 * @param hgroup_list the host groups
 * @param ar_list the advance reservations
 * @param for_job_scheduling prepare for job or for advance reservation scheduling
 * @param now now time of assignment
 * @return always 0
 */
static int
add_job_list_to_schedule(const ocs::tom::List<ocs::tom::JB> job_list, const bool suspended,
                         const ocs::tom::List<ocs::tom::PE> pe_list, const ocs::tom::List<ocs::tom::EH> host_list,
                         const ocs::tom::List<ocs::tom::QU> queue_list, const ocs::tom::List<ocs::tom::RQS> rqs_list,
                         const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         const ocs::tom::ConstList<ocs::tom::US> acl_list,
                         const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                         const ocs::tom::List<ocs::tom::AR> ar_list, const bool for_job_scheduling,
                         const uint64_t now) {
   DENTER(TOP_LAYER);

   auto gep = ocs::tom::find_first_rw<EH_name>(host_list, SGE_GLOBAL_NAME);
   const char *type;
   uint32_t interval = sconf_get_schedule_interval();

   if (suspended) {
      type = SCHEDULING_RECORD_ENTRY_TYPE_SUSPENDED;
   } else {
      type = SCHEDULING_RECORD_ENTRY_TYPE_RUNNING;
   }   

   for (const auto jep : job_list) {
      for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(jep)) {
         sge_assignment_t a{};

         assignment_init(&a, jep, ja_task, nullptr);
         assignment_init_ar(&a, ar_list);

         a.start = ocs::tom::get<JAT_start_time>(ja_task);

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
               WARNING(MSG_SCHEDD_SHOULDHAVEFINISHED_UUU, a.job_id, a.ja_task_id, static_cast<uint32_t>(now - a.duration - a.start + 1));
            }
            a.duration = (now - a.start) + interval;
         }

         // The granted list belongs to the task, but a.gdil owns what it holds: the loan gives it back before the
         // assignment ends, also when an exception leaves the loop, so the task keeps its list
         a.gdil = ocs::tom::ListPtr<ocs::tom::JG>::from_typed(
               ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task));
         struct GdilLoan {
            ocs::tom::ListPtr<ocs::tom::JG> &gdil; ///< the owning member holding the borrowed list

            /** @brief Give the borrowed list back without freeing it */
            ~GdilLoan() {
               [[maybe_unused]] auto borrowed = gdil.release();
            }
         } gdil_loan{a.gdil};
         a.slots = nslots_granted(*a.gdil, nullptr);
         a.host_list = host_list;
         a.queue_list = queue_list;
         a.centry_list = centry_list;
         a.rqs_list = rqs_list;
         a.acl_list = acl_list;
         a.hgrp_list = hgroup_list;
         a.ar_list = ar_list;
         a.gep = gep;

         // Step 1/3 We need the current PE for the resource utilization calculation
         const char *pe_name = ocs::tom::get<JAT_granted_pe>(ja_task);
         a.pe = ocs::tom::Element<ocs::tom::PE>(pe_list_locate(pe_list, pe_name));
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
            const auto old_pe = ocs::tom::get<JAT_pe_object>(ja_task);
            bool old_sromh = ocs::tom::get<PE_ignore_slave_requests_on_master_host>(old_pe);

            org_sromh = ocs::tom::get<PE_ignore_slave_requests_on_master_host>(a.pe);
            ocs::tom::set<PE_ignore_slave_requests_on_master_host>(a.pe, old_sromh);
         }

         if (DPRINTF_IS_ACTIVE) {
            DSTRING_STATIC(dstr, 64);
            DPRINTF("Adding job " sge_u32 "." sge_u32 " into schedule start %s duration %.0f\n",
                    ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(ja_task),
                    sge_ctime64(a.start, &dstr), sge_gmt64_to_gmt32_double(a.duration));
         }

         // Step 3/4: only update resource utilization schedule RUE_utilized_now is already set through events
         debit_scheduled_job(&a, nullptr, nullptr, false, type, for_job_scheduling);

         // Step 4/4: debiting is done. Reset the PE object with the original value
         if (a.pe != nullptr) {
            ocs::tom::set<PE_ignore_slave_requests_on_master_host>(a.pe, org_sromh);
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Debit non-pending jobs in resource schedule (resource diagram)
 *
 * In order to reflect current and future resource utilization of running
 * and suspended jobs in the schedule (resource diagram)
 * we iterate through all jobs and debit
 * resources requested by those jobs.
 *
 * @param running_jobs The running ones (JB_Type)
 * @param suspended_jobs The susepnded ones (JB_Type)
 * @param pe_list the parallel environments, whose diagrams are filled
 * @param host_list the execution hosts, whose diagrams are filled
 * @param queue_list the queue instances, whose diagrams are filled
 * @param rqs_list configured resource quota sets, may be an empty handle
 * @param centry_list the system wide attribute configuration
 * @param acl_list the access lists, needed to evaluate the resource quotas
 * @param hgroup_list the host groups, needed as above
 * @param ar_list the advance reservations, whose diagrams are filled, may be an empty handle
 * @param for_job_scheduling prepare for job or for advance reservation
 * @param now now time of assignment
 *
 * @note MT-NOTE: prepare_resource_schedules() is not MT safe
 */
void
prepare_resource_schedules(const ocs::tom::List<ocs::tom::JB> running_jobs,
                           const ocs::tom::List<ocs::tom::JB> suspended_jobs,
                           const ocs::tom::List<ocs::tom::PE> pe_list,
                           const ocs::tom::List<ocs::tom::EH> host_list, const ocs::tom::List<ocs::tom::QU> queue_list,
                           const ocs::tom::List<ocs::tom::RQS> rqs_list,
                           const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                           const ocs::tom::ConstList<ocs::tom::US> acl_list,
                           const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                           const ocs::tom::List<ocs::tom::AR> ar_list, const bool for_job_scheduling,
                           const uint64_t now) {
   DENTER(TOP_LAYER);

   // the tasks of the jobs are booked, so the job lists are changed
   add_job_list_to_schedule(running_jobs, false, pe_list, host_list, queue_list, rqs_list, centry_list, acl_list,
                            hgroup_list, ar_list, for_job_scheduling, now);
   add_job_list_to_schedule(suspended_jobs, true, pe_list, host_list, queue_list, rqs_list, centry_list, acl_list,
                            hgroup_list, ar_list, for_job_scheduling, now);
   add_calendar_to_schedule(queue_list, now);

   if (print_resource_utilization) {
      utilization_print_all(pe_list, host_list, queue_list, ar_list);
   }

   DRETURN_VOID;
}


/**
 * @brief Addes the queue calendar to the resource
 *
 * Adds the queue calendars to the resource schedule. It is using
 * the slot entry for simulating and enabled / disabled calendar.
 *
 * @param queue_list all queues, which can posibly run jobs
 * @param now now time of assignment
 *
 * @note MT-NOTE: add_calendar_to_schedule() is MT safe
 */
static void
add_calendar_to_schedule(const ocs::tom::List<ocs::tom::QU> queue_list, uint64_t now) {
   DENTER(TOP_LAYER);

   for (const auto queue : queue_list) {
      const auto queue_states = ocs::tom::get<QU_state_changes>(queue);
      uint64_t from = now;

      if (queue_states != nullptr) {
      
         const auto consumable_list = ocs::tom::get<QU_consumable_config_list>(queue);
         const auto slot_elem = ocs::tom::find_first<CE_name>(consumable_list, SGE_ATTR_SLOTS);
         double slot_count = ocs::tom::get<CE_doubleval>(slot_elem); 

         const auto queue_uti_list = ocs::tom::get<QU_resource_utilization>(queue);
         const auto slot_uti = ocs::tom::find_first_rw<RUE_name>(queue_uti_list, SGE_ATTR_SLOTS);
         auto slot_uti_list = ocs::tom::get<RUE_utilized>(slot_uti);
         
         DPRINTF("queue: %s time " sge_u64" \n", ocs::tom::get<QU_full_name>(queue), from);

         if (slot_uti_list == nullptr) {
            auto created = ocs::tom::ListPtr<ocs::tom::RDE>::create("slot_uti");
            slot_uti_list = created.get();
            ocs::tom::set<RUE_utilized>(slot_uti, std::move(created));
         }

         for (const auto queue_state : queue_states) {
            bool is_full = (ocs::tom::get<CQU_state>(queue_state) != QI_DO_NOTHING)?true:false;
            uint64_t till = ocs::tom::get<CQU_till>(queue_state);
          
            /* check for now, and set it if it is now */
            if (is_full && (from == now)) {
               ocs::tom::set<RUE_utilized_now>(slot_uti, slot_count);
            }
          
            set_utilization(slot_uti_list, from, till, is_full?slot_count:0);
            
            from = till;     
         } /* end for_each */

      }/* end if*/
   }
   
   DRETURN_VOID;
}

/**
 * @brief Adds one specific calendar entry to the resource schedule
 *
 * This set utilization function is unique for calendars. It removes all other
 * uti settings in the given time interval and replaces it with the given one.
 *
 * @param uti_list the uti list for a specifiy resource and queue
 * @param from starting time for this uti
 * @param till endtime for this uti.
 * @param uti utilization (needs to bigger than 1 (schould be max)
 *
 * @note MT-NOTE: set_utilization() is MT safe
 */
static void
set_utilization(const ocs::tom::List<ocs::tom::RDE> uti_list, uint64_t from, uint64_t till, double uti) {
   DENTER(TOP_LAYER);

   if (uti > 0) {
      bool is_from_added = false;
      bool is_till_added = false;
      double past_uti = 0;

      if (till == 0) {
         till = DISPATCH_TIME_QUEUE_END;
      }

      DPRINTF("queue cal. schedule entry time " sge_u64 " till " sge_u64 " util: %f\n", from, till, uti);

      auto uti_elem_next = uti_list.front();
     
      /* search for the starting point */
      while (uti_elem_next != nullptr) {
         uint64_t rde_time = ocs::tom::get<RDE_time>(uti_elem_next);
         if (rde_time > from) { /*insert before this elem */
            if (const auto before = uti_elem_next.prev(); before != nullptr) {
               uti_list.insert_after(before, newResourceElem(from, uti));
            } else {
               uti_list.push_front(newResourceElem(from, uti));
            }
            past_uti = ocs::tom::get<RDE_amount>(uti_elem_next);
            is_from_added = true; 
            break;
         } else if (rde_time == from) { /* modify found elem */
            /* override utilization is maximun */
            past_uti = ocs::tom::get<RDE_amount>(uti_elem_next);
            ocs::tom::set<RDE_amount>(uti_elem_next, uti);
            is_from_added = true;
            break;
         } else { /* did not find it, continue */
            uti_elem_next = uti_elem_next.next();
         }
      }

      if (is_from_added) { /* search for the endpoint */
          while (uti_elem_next != nullptr) {
             uint64_t rde_time = ocs::tom::get<RDE_time>(uti_elem_next);
            if (rde_time > till) { /*insert before this elem */
               if (const auto before = uti_elem_next.prev(); before != nullptr) {
                  uti_list.insert_after(before, newResourceElem(till, past_uti));
               } else {
                  uti_list.push_front(newResourceElem(till, past_uti));
               }
               is_till_added = true; 
               break;
            } else if (rde_time == till) { /* do not override utilization is maximun */
               is_till_added = true;
               break;
            } else { /* did not find it, remove the current elem and continue*/
               const auto next = uti_elem_next.next();
               past_uti = ocs::tom::get<RDE_amount>(uti_elem_next);
               uti_list.erase(uti_elem_next);
               uti_elem_next = next;
            }
         }
      } else {
         uti_list.push_back(newResourceElem(from, uti));
      }

      if (!is_till_added) {
         uti_list.push_back(newResourceElem(till, 0));
      }   
   }

   DRETURN_VOID;
}

/**
 * @brief Creates new resource schedule entry
 *
 * creates new resource schedule entry and returns it
 *
 * @param time specific time
 * @param amount the utilized amount
 *
 * @return new resource schedule entry, owned by the caller
 *
 * @note MT-NOTE: newResourceElem() is MT safe
 */
static ocs::tom::ElementPtr<ocs::tom::RDE> newResourceElem(uint64_t time, double amount) {
   auto elem = ocs::tom::ElementPtr<ocs::tom::RDE>::create();
   ocs::tom::set<RDE_time>(elem, time);
   ocs::tom::set<RDE_amount>(elem, amount);
   return elem;
}

/**
 * @brief How many slots of a queue instance are reserved at this moment?
 *
 * @param[in] this_elem the queue instance (`QU_Type`)
 *
 * @return the number of slots currently reserved, 0 if the queue instance has
 *         no slot utilization entry
 */
int qinstance_slots_reserved_now(const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(TOP_LAYER);
   int ret = 0;
   if (const auto slots = ocs::tom::find_first_in<QU_resource_utilization, RUE_name>(this_elem, SGE_ATTR_SLOTS);
       slots != nullptr) {
      const uint64_t now = sge_get_gmt64();
      ocs::TopologyString binding_in_use;
      ret = static_cast<int>(utilization_max(nullptr, ocs::tom::ConstElement<ocs::tom::EH>(), slots, now, 0, 0.0, 0.0,
                                             0.0, false, binding_in_use));
   }
   DRETURN(ret);
}

