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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Cluster queues, and the queue instances they expand into
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "uti/ocs_Pattern.h"

#include "comm/lists/cl_errors.h"
#include "comm/cl_commlib.h"

#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/ocs_cqueue_attributes.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "tom/ocs_List.h"

#include "sched/valid_queue_user.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_c_gdi.h"
#include "sge_utility_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_host_qmaster.h"
#include "sge_hgroup_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "evm/sge_event_master.h"
#include "sge_persistence_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"


static bool
cqueue_hgroup_create(ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_name);

static bool
cqueue_hgroup_delete(ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_name);

static bool
cqueue_verify_hostlist(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::List<ocs::tom::HR> href_list,
                       ocs::gdi::SubCommand sub_command);

static bool
cqueue_route_hostlist(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *cqueue_name,
                      ocs::tom::ConstList<ocs::tom::HR> href_list, monitoring_t *monitor);

static bool
cqueue_mod_attributes(ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                      ocs::tom::Element<ocs::tom::CQ> reduced_elem, ocs::gdi::Command command,
                      ocs::gdi::SubCommand sub_command);

static bool
cqueue_mark_qinstances(ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       ocs::tom::ConstList<ocs::tom::HR> del_hosts, uint64_t gdi_session);

static bool
cqueue_add_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::CQ> cqueue,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstList<ocs::tom::HR> add_hosts,
                      monitoring_t *monitor, ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                      ocs::tom::List<ocs::tom::CQ> master_cqueue_list);

static ocs::tom::ElementPtr<ocs::tom::QU>
qinstance_create(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *hostname, bool *is_ambiguous,
                 monitoring_t *monitor, ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                 ocs::tom::List<ocs::tom::CQ> master_cqueue_list);

static void
cqueue_update_categories(ocs::tom::ConstElement<ocs::tom::CQ> new_cq, ocs::tom::ConstElement<ocs::tom::CQ> old_cq,
                         uint64_t gdi_session);

static void
qinstance_check_unknown_state(ocs::tom::Element<ocs::tom::QU> this_elem,
                              ocs::tom::ConstList<ocs::tom::EH> master_exechost_list, uint64_t gdi_session);

/** @brief Create a queue instance of a cluster queue on one host
 *
 * Every attribute is resolved from the cluster queue for the host; the initial
 * state, the subordination state and the version are set.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue the cluster queue
 * @param answer_list receives messages for the caller
 * @param hostname the host of the new queue instance
 * @param[out] is_ambiguous receives whether the configuration is ambiguous for the host
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups
 * @param master_cqueue_list the cluster queues
 * @return the new queue instance
 */
static ocs::tom::ElementPtr<ocs::tom::QU>
qinstance_create(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *hostname, bool *is_ambiguous,
                 monitoring_t *monitor, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                 const ocs::tom::List<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);
   dstring buffer = DSTRING_INIT;
   const char *cqueue_name = cqueue.get<CQ_name>();
   const auto centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   auto ret = ocs::tom::ElementPtr<ocs::tom::QU>::create();

   /*
    * Pre-initialize some fields: hostname, full_name
    */
   ret->set<QU_qhostname>(ocs::tom::Host(hostname));
   ret->set<QU_qname>(cqueue_name);
   sge_dstring_sprintf(&buffer, "%s@%s", cqueue_name, hostname);
   ret->set<QU_full_name>(sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);

   /*
    * Initialize configuration attributes from CQ
    */
   *is_ambiguous = false;
   bool has_changed_conf_attr = false;   // a new instance is written as a whole anyway
   static_cast<void>(qinstance_modify_attributes(*ret, answer_list, cqueue, nullptr, true, true, true, is_ambiguous,
                                                 &has_changed_conf_attr, nullptr, monitor, master_hgroup_list,
                                                 master_cqueue_list, packet->gdi_session));

   qinstance_set_conf_slots_used(*ret);
   qinstance_debit_consumable(*ret, ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::PE>(),
                              centry_list, 0, true, true, nullptr);

   /*
    * Change qinstance state
    */
   sge_qmaster_qinstance_state_set_ambiguous(*ret, *is_ambiguous, packet->gdi_session);
   if (*is_ambiguous) {
      DPRINTF("Qinstance " SFN "@" SFN " has ambiguous configuration\n", cqueue_name, hostname);
   } else {
      DPRINTF("Qinstance " SFN "@" SFN " has non-ambiguous configuration\n", cqueue_name, hostname);
   }

   /*
    * For new qinstances we have to set some internal fields which
    * will be spooled later on:
    *    - state (modification according to initial state)
    *    - qversion
    */
   sge_qmaster_qinstance_state_set_unknown(*ret, true, packet->gdi_session);
   qinstance_check_unknown_state(*ret, master_ehost_list, packet->gdi_session);
   sge_qmaster_qinstance_set_initial_state(*ret, packet->gdi_session);
   qinstance_initialize_sos_attr(*ret, monitor, master_cqueue_list, packet->gdi_session);

   qinstance_increase_qversion(*ret);

   DRETURN(ret);
}

/** @brief Add a queue instance for every host a cluster queue gained
 *
 * An orphaned queue instance on such a host is reused.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue the cluster queue
 * @param answer_list receives messages for the caller
 * @param add_hosts the hosts gained (HR)
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups
 * @param master_cqueue_list the cluster queues
 * @return true
 */
static bool
cqueue_add_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CQ> cqueue,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstList<ocs::tom::HR> add_hosts,
                      monitoring_t *monitor, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                      const ocs::tom::List<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (cqueue != nullptr && add_hosts != nullptr) {
      for (const auto href : add_hosts) {
         const char *hostname = href.get<HR_name>();
         ocs::tom::Element<ocs::tom::QU> qinstance =
                 cqueue.get<CQ_qinstances>().find_first<QU_qhostname>(ocs::tom::Host(hostname));

         if (qinstance != nullptr) {
            if (qinstance_state_is_orphaned(qinstance)) {
               sge_qmaster_qinstance_state_set_orphaned(qinstance, false, packet->gdi_session);
               qinstance.set<QU_tag>(SGE_QI_TAG_MOD);
            } else {
               /*
                * We might already have this QI if it is in orphaned state.
                * If this is not true, than there is a bug!
                */
               ERROR(MSG_QINSTANCE_QIALREADYHERE_S, hostname);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                               ANSWER_QUALITY_ERROR);
            }
         } else {
            bool is_ambiguous = false;

            const ocs::tom::List<ocs::tom::QU> list = cqueue.get_or_create<CQ_qinstances>("");
            auto new_qinstance = qinstance_create(packet, task, cqueue, answer_list,
                                                  hostname, &is_ambiguous, monitor, master_hgroup_list,
                                                  master_cqueue_list);
            if (is_ambiguous) {
               DPRINTF("qinstance %s has ambiguous conf\n", hostname);
            }
            new_qinstance->set<QU_tag>(SGE_QI_TAG_ADD);
            list.push_back(std::move(new_qinstance));
         }
      }
   }
   DRETURN(ret);
}

/** @brief Tag the queue instances of a cluster queue for the hosts it loses
 *
 * A queue instance still running jobs becomes orphaned instead of deleted.
 *
 * @param cqueue the cluster queue
 * @param answer_list receives messages for the caller
 * @param del_hosts the hosts lost (HR)
 * @param gdi_session the session the change belongs to
 * @return true
 */
static bool
cqueue_mark_qinstances(const ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const ocs::tom::ConstList<ocs::tom::HR> del_hosts, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (cqueue != nullptr) {
      for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
         const char *hostname = qinstance.get<QU_qhostname>();

         if (del_hosts.find_first<HR_name>(ocs::tom::Host(hostname)) != nullptr) {
            if (qinstance_slots_used(qinstance) > 0 || qinstance_slots_reserved(qinstance) > 0) {
               /*
                * Jobs are currently running in this queue. Therefore
                * it is not possible to delete the queue but we
                * will set it into the "orphaned" state 
                */
               sge_qmaster_qinstance_state_set_orphaned(qinstance, true, gdi_session);
               qinstance.set<QU_tag>(SGE_QI_TAG_MOD);
            } else {
               qinstance.set<QU_tag>(SGE_QI_TAG_DEL);
            }
         } else {
            qinstance.set<QU_tag>(SGE_QI_TAG_DEFAULT);
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Modify one attribute list of a cluster queue as a request says
 *
 * The request's list says, per host or host group, what to do with the cluster queue's list. "sub_cmd" says
 * how: with SGE_GDI_SET_ALL the entries the request does not name are removed first; a removal applies to
 * entries the queue holds; otherwise an entry the queue lacks is created. A value that is a keyed list is
 * modified key by key (attr_mod_sub_list()), any other value is replaced.
 *
 * @tparam CqF the attribute list field of the cluster queue
 * @param this_elem the cluster queue to modify
 * @param answer_list AN list
 * @param reduced_elem the reduced request element; does nothing if it lacks the attribute
 * @param sub_cmd the GDI sub-command
 * @param attribute_name_str the attribute's name, for the messages
 * @param object_name_str the cluster queue's name, for the messages
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: cqueue_mod_sublist() is MT safe
 */
template <auto CqF>
static bool
cqueue_mod_sublist(const ocs::tom::Element<ocs::tom::CQ> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                   const ocs::tom::Element<ocs::tom::CQ> reduced_elem, const ocs::gdi::SubCommand sub_cmd,
                   const char *attribute_name_str, const char *object_name_str) {
   DENTER(TOP_LAYER);

   using A = typename ocs::tom::FieldTraits<CqF>::Sub;
   constexpr auto href_f = ocs_attr_traits<A>::href;
   constexpr auto value_f = ocs_attr_traits<A>::value;
   bool ret = true;

   if (reduced_elem.has<CqF>()) {
      const auto mod_list = reduced_elem.get<CqF>();
      auto org_list = this_elem.get<CqF>();

      /* 
       * Delete all configuration lists except the default-configuration
       * if sub_command is SGE_GDI_SET_ALL
       */
      if ((sub_cmd & ocs::gdi::SubCommand::SET_ALL) == ocs::gdi::SubCommand::SET_ALL) {
         auto next_elem = org_list.front();
         while (const auto elem = next_elem) {
            const char *name = elem.template get<href_f>();

            next_elem = elem.next();
            if (!mod_list.template find_first<href_f>(ocs::tom::Host(name))) {
               DPRINTF("Removing attribute list for " SFQ "\n", name);
               org_list.erase(elem);
            }
         }
      }

      /*
       * Do modifications for all given elements of 
       * domain/host-configuration list
       */
      for (const auto mod_elem : mod_list) {
         const char *name = mod_elem.template get<href_f>();
         char resolved_name[CL_MAXHOSTNAMELEN + 1];

         if (name == nullptr) {
            ERROR(MSG_SGETEXT_INVALIDHOST_S, "");
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         /* Don't try to resolve hostgroups */
         if (name[0] != '@') {
            int back = getuniquehostname(name, resolved_name, 0);

            if (back == CL_RETVAL_OK) {
               /* 
                * This assignment is ok because preious name contained a const
                * string from the mod_elem that we didn't need to free.  
                * Now it will contain a string that's on the stack, 
                * so we still don't have to free it. 
                */
               name = resolved_name;
            } else {
               /*
                * Due to CR 6319231, IZ 1760 this is allowed
                */
            }
         }

         auto org_elem = org_list.template find_first<href_f>(ocs::tom::Host(name));

         /*
          * Create element if it does not exist
          */
         if (!org_elem && (sub_cmd & ocs::gdi::SubCommand::REMOVE) != ocs::gdi::SubCommand::REMOVE) {
            if (!org_list) {
               org_list = this_elem.template get_or_create<CqF>("");
            }
            auto new_elem = ocs::tom::ElementPtr<A>::create();
            new_elem->template set<href_f>(ocs::tom::Host(name));
            org_elem = org_list.push_back(std::move(new_elem));
         }

         /*
          * Modify sublist according to subcommand
          */
         if (org_elem) {
            if constexpr (OcsHostAttrKeyedList<A>) {
               static_cast<void>(attr_mod_sub_list<value_f, ocs_attr_traits<A>::key>(
                  answer_list, org_elem, mod_elem, sub_cmd, attribute_name_str, object_name_str));
            } else {
               // a number, a boolean or a string, which is copied
               org_elem.template set<value_f>(mod_elem.template get<value_f>());
            }
         }
      }
   }

   DRETURN(ret);
}

/** @brief Apply the attributes the client sent to a cluster queue
 *
 * The attributes are walked through #ocs_cqueue_attributes, as long as no
 * attribute failed.
 *
 * @param cqueue the cluster queue
 * @param answer_list receives messages for the caller
 * @param reduced_elem the reduced element the client sent
 * @param command the command being executed, unused
 * @param sub_command what kind of modification this is
 * @return true on success
 */
static bool
cqueue_mod_attributes(const ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                      const ocs::tom::Element<ocs::tom::CQ> reduced_elem,
                      [[maybe_unused]] const ocs::gdi::Command command,
                      const ocs::gdi::SubCommand sub_command) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (cqueue != nullptr && reduced_elem != nullptr) {
      const char *cqueue_name = cqueue.get<CQ_name>();

      ret = ocs_for_each_cqueue_attr_while([&](const auto &attr) {
         using E = ocs_cqueue_attr_t<decltype(attr)>;
         return cqueue_mod_sublist<E::cqueue_field>(cqueue, answer_list, reduced_elem, sub_command, attr.name,
                                                    cqueue_name);
      });
   }
   DRETURN(ret);
}

/** @brief Create the host group carrying a cluster queue's host list
 *
 * CS-2677, N-R-1..3. A cluster queue does not define a set of hosts any more;
 * it owns one, and this creates it: `@@<queue>`, empty, spooled, announced.
 *
 * Called from the add path before anything reads the queue's host list, and
 * before the queue instances are computed -- with the group missing, a host
 * list arriving in the same request would have nowhere to go.
 *
 * The name check is switched off deliberately (`is_name_validate = false`):
 * `@@` is exactly what hgroup_check_name() refuses, which is what reserves the
 * prefix for the qmaster in the first place.
 *
 * @param packet the client request
 * @param answer_list receives messages for the caller
 * @param cqueue_name the cluster queue the group belongs to
 * @return true on success
 */
static bool
cqueue_hgroup_create(ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_name) {
   DENTER(TOP_LAYER);

   dstring name_buffer = DSTRING_INIT;
   const char *name = hgroup_queue_group_name(cqueue_name, &name_buffer);
   bool ret = true;

   if (ocs::DataStore::master_list<SGE_TYPE_HGROUP>().find_first<HGRP_name>(ocs::tom::Host(name)) != nullptr) {
      /* the queue is new, so its group cannot be -- unless one was left behind */
      ERROR(MSG_SGETEXT_ALREADYEXISTS_SS, "host group", name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   ocs::tom::ElementPtr<ocs::tom::HGRP> new_hgroup;

   if (ret) {
      new_hgroup = hgroup_create(answer_list, name, ocs::tom::ListPtr<ocs::tom::HR>(), false);
      if (new_hgroup == nullptr) {
         ret = false;
      }
   }

   if (ret && !spool_write_object(answer_list, spool_get_default_context(), *new_hgroup, name,
                                  SGE_TYPE_HGROUP)) {
      ERROR(MSG_CANTSPOOL_SS, "host group entry", name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      new_hgroup.reset();
      ret = false;
   }

   if (ret) {
      // the master list is a process-lifetime global, it is created here if missing, as before
      const auto hgroup_list = ocs::DataStore::master_list_create<SGE_TYPE_HGROUP>("host group");
      const ocs::tom::Element<ocs::tom::HGRP> hgroup = hgroup_list.push_back(std::move(new_hgroup));

      /* CS-2451: an element without a cache answers "no" to every membership
       * question, so the cache is established before the event goes out */
      ocs::tom::ListPtr<ocs::tom::HR> referencees;
      hgroup_refresh_caches(hgroup, hgroup_list, referencees);
      sge_add_event(0, sgeE_HGROUP_ADD, 0, 0, name, nullptr, nullptr, hgroup, packet->gdi_session);
      hgroup_send_referencee_events(referencees.get(), hgroup_list, packet->gdi_session);
   }

   sge_dstring_free(&name_buffer);
   DRETURN(ret);
}

/** @brief Remove the host group carrying a cluster queue's host list
 *
 * CS-2677, N-R-3 and N-O-2: the group goes with its queue, and only with it.
 * The counterpart of cqueue_hgroup_create(); also the rollback when a queue
 * add fails after the group was created.
 *
 * A missing group is not an error. The queue may predate the change, or the
 * add may have failed before the group existed.
 *
 * @param packet the client request
 * @param answer_list receives messages for the caller
 * @param cqueue_name the cluster queue the group belongs to
 * @return true on success
 */
static bool
cqueue_hgroup_delete(ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_name) {
   DENTER(TOP_LAYER);

   dstring name_buffer = DSTRING_INIT;
   const char *name = hgroup_queue_group_name(cqueue_name, &name_buffer);
   const auto master_hgroup_list = ocs::DataStore::master_list_rw<SGE_TYPE_HGROUP>();
   const ocs::tom::Element<ocs::tom::HGRP> hgroup = master_hgroup_list.find_first<HGRP_name>(ocs::tom::Host(name));
   bool ret = true;

   if (hgroup != nullptr) {
      if (sge_event_spool(answer_list, 0, sgeE_HGROUP_DEL, 0, 0, name, nullptr, nullptr,
                          nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
         master_hgroup_list.erase(hgroup);
      } else {
         ERROR(MSG_CANTSPOOL_SS, "host group entry", name);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   sge_dstring_free(&name_buffer);
   DRETURN(ret);
}

/** @brief Check a host list instruction before the queue is written
 *
 * CS-2677, N-T-4. The host list of a cluster queue is the member list of its
 * host group, and the write is performed by the host group path -- but that
 * happens in cqueue_success(), once the queue itself is installed and the
 * cascade can find it. This runs earlier, in the modifier, so that an input
 * the host group path would reject anyway rejects the whole request, the way
 * it does today.
 *
 * It checks what can be checked without touching state: that the names
 * resolve, that no queue host group is named (N-O-4), and that a host group
 * named as a member exists. The last one is skipped for a removal, where the
 * instruction names what is being taken away rather than what is being added.
 *
 * @param answer_list receives messages for the caller
 * @param href_list the instruction, an HR list; host names are resolved in place
 * @param sub_command how the instruction is merged with the member list
 * @return true when the input is acceptable
 */
static bool
cqueue_verify_hostlist(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::List<ocs::tom::HR> href_list,
                       const ocs::gdi::SubCommand sub_command) {
   DENTER(TOP_LAYER);

   const bool is_remove = (sub_command & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE;
   bool ret = href_list_resolve_hostnames(href_list, answer_list, true, !is_remove);
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();

   for (const auto href : href_list) {
      const char *name = href.get<HR_name>();

      if (!ret) {
         break;
      }
      if (name == nullptr || !ocs::is_hgroup_name(name)) {
         continue;
      }
      if (hgroup_is_queue_group(name)) {
         ERROR(MSG_HGRP_QUEUEGROUP_NOREF_S, name);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
      if (!is_remove && master_hgroup_list.find_first<HGRP_name>(ocs::tom::Host(name)) == nullptr) {
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, "host group", name);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
   }

   DRETURN(ret);
}

/** @brief Write a cluster queue's host list into its host group
 *
 * CS-2677, N-T-4: every write to a host group is owned by the host group
 * write path, and one that arrives as a change to a cluster queue is no
 * exception. This is the routing, and it is deliberately not a
 * reimplementation -- it hands the instruction to ocs_gdi_add_mod()
 * with the host group's own table entry, so the request travels
 * hgroup_mod() -> hgroup_spool() -> hgroup_success() exactly as
 * `qconf -mhgrp @@<queue>` would. The queue instances the change creates or
 * removes come from the cascade inside that path, not from here.
 *
 * Called from cqueue_success() rather than from the modifier: the cascade
 * looks for the affected queues in the master list, and until the queue is
 * installed there the version it would find is the one being replaced.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue_name the cluster queue whose host list is written
 * @param href_list the instruction, an HR list
 * @param monitor for monitoring qmaster threads
 * @return true on success
 */
static bool
cqueue_route_hostlist(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *cqueue_name,
                      const ocs::tom::ConstList<ocs::tom::HR> href_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   dstring name_buffer = DSTRING_INIT;
   const char *group_name = hgroup_queue_group_name(cqueue_name, &name_buffer);
   gdi_object_t *hgroup_object = get_gdi_object(ocs::gdi::Target::HGRP_LIST);
   auto instructions = ocs::tom::ElementPtr<ocs::tom::HGRP>::create();
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   auto members = href_list.clone("");

   if (members == nullptr) {
      members = ocs::tom::ListPtr<ocs::tom::HR>::create("");   /* see cqueue_mod(): empty is an instruction */
   }
   instructions->set<HGRP_name>(ocs::tom::Host(group_name));
   instructions->set<HGRP_host_list>(std::move(members));

   const int status = ocs_gdi_add_mod<ocs::tom::HGRP>(packet, task, &answer_list, *instructions, 0,
                                                      hgroup_object, packet->user, packet->host,
                                                      task->command, task->sub_command, nullptr, monitor);

   /* the caller's answer list is the queue's; the group's messages name the
    * group, which is not the object the administrator addressed */
   if (status != STATUS_OK) {
      answer_list_log(&answer_list, false, true);
   }
   answer_list.reset();
   instructions.reset();
   sge_dstring_free(&name_buffer);

   DRETURN(status == STATUS_OK);
}

/** @brief Bring a cluster queue's instances into line with its new configuration
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue see the brief above
 * @param answer_list receives messages for the caller
 * @param reduced_elem see the brief above
 * @param refresh_all_values see the brief above
 * @param is_startup see the brief above
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups
 * @param master_cqueue_list the cluster queues
 * @return true on success
 */
bool
cqueue_mod_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CQ> cqueue,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ElementData *reduced_elem,
                      const bool refresh_all_values, const bool is_startup, monitoring_t *monitor,
                      const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                      const ocs::tom::List<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   dstring buffer = DSTRING_INIT;
   bool ret = true;
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

   if (cqueue != nullptr && reduced_elem != nullptr) {
      /*
       * Try to find changes for all qinstances ...
       */
      for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
         const char *qinstance_name = qinstance_get_name(qinstance, &buffer);
         bool is_ambiguous = qinstance_state_is_ambiguous(qinstance);
         bool is_del = (qinstance.get<QU_tag>() == SGE_QI_TAG_DEL) ? true : false;
         bool will_be_ambiguous = false;
         bool state_changed = false;
         bool conf_changed = false;
         bool need_reinitialize = false;

         /*
          * Set full name of QI if it is not set
          */
         if (refresh_all_values &&
             qinstance.get<QU_full_name>() == nullptr) {
            qinstance_set_full_name(qinstance);
         }

         /* 
          * Clear all messages which explain ambiguous state
          */
         qinstance_message_trash_all_of_type_X(qinstance, QI_AMBIGUOUS);

         /*
          * Handle each cqueue attribute as long as there was no error
          * and only if the qinstance won't be deleted afterward.
          */

         /*
          * We try to find changes only for attributes which were 
          * sent by the client: those the reduced element holds. The
          * reduced element may be of another type (a host group from
          * hgroup_mod()); it holds none of them then.
          *
          * There are two situations which make it absolutely necessary
          * to have a look on ALL attributes:
          *
          * 1) refresh_all_values is true
          *    The hostlist of "cqueue" changed. As a result it
          *    might be possible that a value for an attribute is
          *    now ambiguous. 
          * 
          * 2) is_ambiguous is true
          *    The qinstance is currently in the ambiguous state.
          *    It is not enough to test only modified attributes if
          *    they are nonambigous. It is also necesssary to check
          *    if all attributes which are not changed now are
          *    nonambigous to clear the ambigous-state from qinstance. 
          */
         if (ret && !is_del) {
            ret &= qinstance_modify_attributes(qinstance, answer_list, cqueue, reduced_elem,
                                               refresh_all_values || is_ambiguous, false, is_startup,
                                               &will_be_ambiguous, &conf_changed, &need_reinitialize, monitor,
                                               master_hgroup_list, master_cqueue_list, packet->gdi_session);
         }

         if (need_reinitialize) {
            qinstance_reinit_consumable_actual_list(qinstance, answer_list);
         }

         /*
          * Change qinstance state
          */
         sge_qmaster_qinstance_state_set_ambiguous(qinstance, will_be_ambiguous, packet->gdi_session);
         if (will_be_ambiguous && !is_ambiguous) {
            state_changed = true;
            DPRINTF("Qinstance " SFQ " has ambiguous configuration\n", qinstance_name);
         } else if (!will_be_ambiguous && is_ambiguous) {
            state_changed = true;
            DPRINTF("Qinstance " SFQ " has non-ambiguous configuration\n", qinstance_name);
         }

         /*
          * Tag the qinstance as modified if the internal state changed. 
          * This will result in spooling the qinstance. Also mod-events wiil 
          * be sent. If only the configuration changed than it is only 
          * necessary to send mod-events.
          */
         if (state_changed) {
            DPRINTF("Internal state of qinstance " SFQ " has been changed\n", qinstance_name);
            qinstance.set<QU_tag>(SGE_QI_TAG_MOD);
            qinstance_increase_qversion(qinstance);
         } else if (conf_changed) {
            DPRINTF("Only config value of qinstance " SFQ " has been changed\n", qinstance_name);
            qinstance.set<QU_tag>(SGE_QI_TAG_MOD_ONLY_CONFIG);
            qinstance_increase_qversion(qinstance);
         }

         if (ret && !is_startup) {
            for (const auto ar : master_ar_list) {
               if (ar.get<AR_granted_slots>().find_first<JG_qname>(qinstance_name)) {
                  if (!sge_ar_have_users_access(nullptr, ar, qinstance.get<QU_full_name>(),
                                                qinstance.get<QU_acl>(), qinstance.get<QU_xacl>(),
                                                master_userset_list)) {
                     ERROR(MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU, SGE_ATTR_USER_LISTS,  ar.get<AR_id>());
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                     break;
                  }
               }
            }
         }

         if (!ret) {
            /*
             * Skip remaining qinstances if an error occurred.
             */
            break;
         }
      }
   }
   sge_dstring_free(&buffer);

   DRETURN(ret);
}

/** @brief Create and delete queue instances as a cluster queue gains and loses hosts
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue see the brief above
 * @param answer_list receives messages for the caller
 * @param reduced_elem see the brief above
 * @param add_hosts see the brief above
 * @param rem_hosts see the brief above
 * @param refresh_all_values see the brief above
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups
 * @param master_cqueue_list the cluster queues
 * @return true on success
 */
bool
cqueue_handle_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CQ> cqueue,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ElementData *reduced_elem,
                         const ocs::tom::ConstList<ocs::tom::HR> add_hosts,
                         const ocs::tom::ConstList<ocs::tom::HR> rem_hosts, const bool refresh_all_values,
                         monitoring_t *monitor, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                         const ocs::tom::List<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (ret) {
      ret = cqueue_mark_qinstances(cqueue, answer_list, rem_hosts, packet->gdi_session);
   }
   if (ret) {
      ret = cqueue_mod_qinstances(packet, task, cqueue, answer_list, reduced_elem,
                                  refresh_all_values, false, monitor, master_hgroup_list, master_cqueue_list);
   }
   if (ret) {
      ret = cqueue_add_qinstances(packet, task, cqueue, answer_list, add_hosts, monitor, master_hgroup_list, master_cqueue_list);
   }
   DRETURN(ret);
}

/** @brief Apply one attribute change to a cluster queue
 *
 * The modifier of ocs_gdi_hooks for cluster queues; see ocs_gdi_add_mod() for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param cqueue the cluster queue
 * @param reduced_elem the reduced element the client sent
 * @param add 1 for add, 0 for modify
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
cqueue_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
           const ocs::tom::Element<ocs::tom::CQ> cqueue, const ocs::tom::Element<ocs::tom::CQ> reduced_elem, int add,
           const char *remote_user, const char *remote_host, gdi_object_t *object, ocs::gdi::Command cmd,
           ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const auto master_calendar_list = ocs::DataStore::master_list<SGE_TYPE_CALENDAR>();
   const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   if (reduced_elem.has<CQ_name>()) {
      const char *name = reduced_elem.get<CQ_name>();

      if (add) {
         /*
          * CS-2677, N-R-4. The binding limit is not MAX_VERIFY_STRING but the
          * spool file name: classic spooling writes ".<name>", so an object name
          * fits in NAME_MAX - 1, and a cluster queue is two characters shorter
          * still because it owns "@@<queue>", spooled as ".@@<queue>".
          *
          * The check is drawn here, where the name that is too long is the one
          * the administrator typed. Left to the spool it would surface as
          * "unable to spool host group entry" -- a fault reported against an
          * object nobody wrote, which is exactly what N-R-5 exists to prevent.
          */
         if (verify_obj_name(
                 answer_list, name, MAX_CQUEUE_NAME, "cqueue") == STATUS_OK) {
            DTRACE;
            cqueue.set<CQ_name>(name);
         } else {
            if (name != nullptr && strlen(name) > MAX_CQUEUE_NAME) {
               ERROR(MSG_CQUEUE_NAMETOOLONG_U, static_cast<uint32_t>(MAX_CQUEUE_NAME));
            } else {
               ERROR(MSG_CQUEUE_NAMENOTGUILTY_S, name);
            }
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }

         /*
          * N-R-1..3: the queue owns a host group and it comes into being here,
          * before the host list arriving in this same request is routed into it
          * and before the queue instances are computed.
          */
         if (ret) {
            ret = cqueue_hgroup_create(packet, answer_list, name);
         }
      } else {
         const char *old_name = cqueue.get<CQ_name>();

         if (strcmp(old_name, name)) {
            ERROR(SFNMAX, MSG_CQUEUE_NONAMECHANGE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   } else {
      ERROR(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::FieldTraits<CQ_name>::info.name, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /*
    * CS-2677, N-T-4: a host list arriving with a queue change is an
    * instruction, not a value of the queue. It is checked here and carried on
    * the element until cqueue_success() hands it to the host group write path;
    * the master list never holds it. What used to stand here -- 152 lines
    * resolving the list against the host groups -- was the queue's own copy of
    * what that path does, and has nothing left to operate on.
    */
   if (ret) {
      if (reduced_elem.has<CQ_hostlist>()) {
         auto instruction = reduced_elem.get<CQ_hostlist>().clone("");

         /*
          * An empty instruction is an instruction: "qconf -rattr queue hostlist
          * NONE" empties the host list, and the queue instances go with it. It
          * has to reach the host group as an empty list rather than as nothing
          * at all, or emptying a host list would silently do nothing.
          */
         if (instruction == nullptr) {
            instruction = ocs::tom::ListPtr<ocs::tom::HR>::create("");
         }

         ret &= cqueue_verify_hostlist(answer_list, instruction.get(), sub_command);
         if (ret) {
            cqueue.set<CQ_hostlist>(std::move(instruction));
         }
      }
   }

   /*
    * Its time to do the cqueue modifications:
    *    - change the attribute lists in the cqueue object
    *    - verify the attribute lists
    */
   if (ret) {
      ret &= cqueue_mod_attributes(cqueue, answer_list,
                                   reduced_elem, cmd, sub_command);
   }
   if (ret) {
      ret &= cqueue_verify_attributes(cqueue, answer_list,
                                      reduced_elem, true, master_calendar_list, master_ckpt_list,
                                      master_pe_list, master_userset_list, master_project_list,
                                      master_centry_list, master_cqueue_list, master_hgroup_list);
   }

   /*
    * Now we have to mod/del all qinstances the queue already has.
    *
    * Instances the host list change creates or removes are not handled here
    * any more: they come from the cascade inside the host group write path,
    * together with the EH elements the new hosts need
    * (host_list_add_missing_href() is called there for the same reason it was
    * called here). refresh_all_values stays false for the same reason -- the
    * cascade sets it itself for the queues whose membership moved.
    */
   if (ret) {
      ret &= cqueue_handle_qinstances(packet, task, cqueue, answer_list, reduced_elem,
                                      ocs::tom::ConstList<ocs::tom::HR>(), ocs::tom::ConstList<ocs::tom::HR>(), false,
                                      monitor, master_hgroup_list, master_cqueue_list);
   }

   /*
    * CS-2677: an add that failed after its host group was created would leave
    * the group behind -- a "@@<queue>" without a queue, which nothing can then
    * remove, since N-O-2 refuses an administrative delete.
    */
   if (!ret && add) {
      ocs::tom::ListPtr<ocs::tom::AN> rollback_answer_list;

      if (!cqueue_hgroup_delete(packet, &rollback_answer_list, cqueue.get<CQ_name>())) {
         answer_list_output(&rollback_answer_list);
      }
      rollback_answer_list.reset();
   }

   if (ret) {
      DRETURN(0);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

/** @brief Announce a cluster queue change once it is safely spooled
 *
 * The on_success of ocs_gdi_hooks for cluster queues.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cqueue the cluster queue
 * @param old_cqueue the cluster queue as it was, or an empty handle on add
 * @param object the table entry for this object type
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
cqueue_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CQ> cqueue,
               const ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue, gdi_object_t *object,
               [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();

   /*
    * CS-2677: take the host list instruction off the element before anything
    * else happens. It travelled here on the queue (N-T-4) but is no part of
    * it, so neither the event below nor the master list may carry it. It is
    * written into the queue host group at the very end of this function.
    */
   auto hostlist_instruction = cqueue.exchange<CQ_hostlist>(ocs::tom::ListPtr<ocs::tom::HR>());

   /* a copy: the element this name lives on is replaced by the host group
    * path below, and the name is needed after that */
   dstring name_buffer = DSTRING_INIT;
   sge_dstring_copy_string(&name_buffer, cqueue.get<CQ_name>());
   const char *queue_name = sge_dstring_get_string(&name_buffer);

   cqueue_update_categories(cqueue, old_cqueue, packet->gdi_session);

   /*
    * CQ modify or add event
    */
   sge_add_event(0, old_cqueue ? sgeE_CQUEUE_MOD : sgeE_CQUEUE_ADD, 0, 0,
                 cqueue.get<CQ_name>(), nullptr, nullptr, cqueue, packet->gdi_session);

   /*
    * QI modify, add or delete event. Finalize operation.
    */
   cqueue_commit(cqueue, packet->gdi_session);

   /*
    * Handle jobs which were supended due to suspend threshold
    */
   for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
      /* check slotwise subordinate suspends for new qinstance config */
      do_slotwise_x_on_subordinate_check(qinstance, false, false, monitor);
      do_slotwise_x_on_subordinate_check(qinstance, true, false, monitor);

      if (qinstance.get<QU_gdi_do_later>() == GDI_DO_LATER) {
         bool is_qinstance_mod = false;
         const char *full_name = qinstance.get<QU_full_name>();

         qinstance.set<QU_gdi_do_later>(0);

         /* in case the thresholds are set to none, we have to unsuspend all jobs because
            the scheduler is not able to do that. If the suspend threshold is still set; 
            just changed, the scheduler can easily deal with it.*/
         if (qinstance.get<QU_suspend_thresholds>() == nullptr) {
            for (const auto job : master_job_list) {
               for (const auto ja_task : job.get<JB_ja_tasks>()) {
                  uint32_t state = ja_task.get<JAT_state>();

                  if (ISSET(state, JSUSPENDED_ON_THRESHOLD)) {
                     /* this does most likely not work with pe jobs, which run in different queues.
                        Issue: 831*/
                     const char *queue_name = ja_task.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>();

                     if (!strcmp(queue_name, full_name)) {

                        if (!ISSET(state, JSUSPENDED)) {
                           sge_signal_queue(SGE_SIGCONT, qinstance, job, ja_task, monitor);
                           SETBIT(JRUNNING, state);
                           is_qinstance_mod = true;
                        }

                        CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);

                        ja_task.set<JAT_state>(state);

                        sge_event_spool(nullptr, 0, sgeE_JATASK_MOD,
                                        job.get<JB_job_number>(),
                                        ja_task.get<JAT_task_number>(), nullptr, nullptr, nullptr,
                                        job, ja_task, nullptr, true, true, packet->gdi_session);

                     }
                  }
               }
            }
         }

         if (is_qinstance_mod) {
            const char *hostname = qinstance.get<QU_qhostname>();
            const char *cqueue_name = qinstance.get<QU_qname>();
            sge_event_spool(nullptr, 0, sgeE_QINSTANCE_MOD,
                            0, 0, cqueue_name, hostname, nullptr,
                            qinstance, nullptr, nullptr, true, true, packet->gdi_session);
         }
      }
   }

   /*
    * CS-2677, N-T-4: last, because the host group write path replaces the
    * queue element in the master list with its own copy -- the one carrying
    * the queue instances the host list change creates or removes. Nothing may
    * touch "cqueue" after this point.
    */
   if (hostlist_instruction != nullptr) {
      cqueue_route_hostlist(packet, task, queue_name, hostlist_instruction.get(), monitor);
      hostlist_instruction.reset();
   }
   sge_dstring_free(&name_buffer);

   DRETURN(0);
}

/** @brief Make the pending changes to a cluster queue permanent
 *
 * @param cqueue the cluster queue
 * @param gdi_session the session the change belongs to
 */
void
cqueue_commit(const ocs::tom::Element<ocs::tom::CQ> cqueue, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::List<ocs::tom::QU> qinstances = cqueue.get<CQ_qinstances>();
   ocs::tom::Element<ocs::tom::QU> qinstance = nullptr;

   /*
    * QI modify, add or delete event
    */
   ocs::tom::Element<ocs::tom::QU> next_qinstance = qinstances.front();
   while ((qinstance = next_qinstance)) {
      uint32_t tag = qinstance.get<QU_tag>();
      const char *name = qinstance.get<QU_qname>();
      const char *hostname = qinstance.get<QU_qhostname>();

      next_qinstance = qinstance.next();

      /*
       * Reset QI tag
       */
      qinstance.set<QU_tag>(SGE_QI_TAG_DEFAULT);

      if (tag == SGE_QI_TAG_ADD) {
         sge_add_event(0, sgeE_QINSTANCE_ADD, 0, 0, name, hostname, nullptr, qinstance, gdi_session);
      } else if (tag == SGE_QI_TAG_MOD ||
                 tag == SGE_QI_TAG_MOD_ONLY_CONFIG) {
         sge_add_event(0, sgeE_QINSTANCE_MOD, 0, 0, name, hostname, nullptr, qinstance, gdi_session);
      } else if (tag == SGE_QI_TAG_DEL) {
         sge_event_spool(nullptr, 0, sgeE_QINSTANCE_DEL, 0, 0, name, hostname,
                         nullptr, nullptr, nullptr, nullptr, true, true, gdi_session);

         /*
          * Now we can remove the qinstance.
          */
         qinstances.erase(qinstance);
      }
   }
   if (qinstances.empty()) {
      cqueue.set<CQ_qinstances>(nullptr);
   }
   DRETURN_VOID;
}

/** @brief Write a cluster queue to the spool
 *
 * The writer of ocs_gdi_hooks for cluster queues.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param cqueue the cluster queue
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
cqueue_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
             const ocs::tom::Element<ocs::tom::CQ> cqueue, gdi_object_t *object) {
   DENTER(TOP_LAYER);
   int ret = 0;
   const char *name = cqueue.get<CQ_name>();
   dstring key_dstring = DSTRING_INIT;
   bool dbret;
   ocs::tom::ListPtr<ocs::tom::AN> spool_answer_list;

   dbret = spool_write_object(&spool_answer_list, spool_get_default_context(), cqueue, name, SGE_TYPE_CQUEUE);
   answer_list_output(&spool_answer_list);

   if (!dbret) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_PERSISTENCE_WRITE_FAILED_S, name);
      ret = 1;
   }

   for (ocs::tom::Element<ocs::tom::QU> qinstance : cqueue.get<CQ_qinstances>()) {
      uint32_t tag = qinstance.get<QU_tag>();

      if (tag == SGE_QI_TAG_ADD || tag == SGE_QI_TAG_MOD) {
         const char *key = sge_dstring_sprintf(&key_dstring, "%s/%s", name,
                                               qinstance.get<QU_qhostname>().c_str());
         dbret = spool_write_object(&spool_answer_list, spool_get_default_context(), qinstance,
                                    key, SGE_TYPE_QINSTANCE);
         answer_list_output(&spool_answer_list);

         if (!dbret) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_PERSISTENCE_WRITE_FAILED_S,
                                    key);
            ret = 1;
         }
      }
   }

   sge_dstring_free(&key_dstring);

   DRETURN(ret);
}

/** @brief Delete a cluster queue
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param this_elem the object
 * @param answer_list receives messages for the caller
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @return STATUS_OK on success
 */
int
cqueue_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char *remote_user, char *remote_host) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   if (this_elem != nullptr && remote_user != nullptr && remote_host != nullptr) {
      const char *name = this_elem.get<CQ_name>();

      if (name != nullptr) {
         const ocs::tom::Element<ocs::tom::CQ> cqueue = master_cqueue_list.find_first<CQ_name>(name);

         if (cqueue != nullptr) {
            ocs::tom::ConstList<ocs::tom::QU> qinstances = cqueue.get<CQ_qinstances>();
            const char *cq_name = cqueue.get<CQ_name>();
            dstring dir = DSTRING_INIT;
            bool do_del = true;

/* TODO: HP: Trigger recalculation of ssos for all subqueues of this queue */
            /*
             * test if the CQ can be removed
             */
            for (const auto qinstance : qinstances) {
               if (qinstance_slots_used(qinstance) > 0 || qinstance_slots_reserved(qinstance) > 0) {
                  ERROR(SFNMAX, MSG_QINSTANCE_STILLJOBS);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST,
                                  ANSWER_QUALITY_ERROR);
                  do_del = false;
                  break;
               }
            }

            /*
             * check for references of this cqueue in other cqueues subordinate
             * lists
             */
            if (do_del) {
               for (const auto tmp_cqueue : master_cqueue_list) {

                  if (cqueue_is_used_in_subordinate(name, tmp_cqueue)) {
                     ERROR(MSG_CQUEUE_DEL_ISREFASSUBORDINATE_SS, name, tmp_cqueue.get<CQ_name>());
                     answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                     do_del = false;
                     break;
                  }
               }
            }

            if (do_del) {
               /*
                * delete QIs
                */
               dstring key = DSTRING_INIT;
               sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);

               for (const auto qinstance : qinstances) {
                  const char *qi_name = qinstance.get<QU_qhostname>();

                  sge_dstring_sprintf(&key, "%s/%s", cq_name, qi_name);
                  if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL,
                                      0, 0, cq_name, qi_name,
                                      nullptr, nullptr, nullptr, nullptr, true, true, packet->gdi_session)) { ;
                  }
               }
               sge_dstring_free(&key);
               sge_rmdir(sge_dstring_get_string(&dir), nullptr);
               sge_dstring_free(&dir);

               /*
                * delete CQ
                */
               if (sge_event_spool(answer_list, 0, sgeE_CQUEUE_DEL, 0, 0, name, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
                  cqueue_update_categories(nullptr, cqueue, packet->gdi_session);
                  master_cqueue_list.erase(cqueue);

                  /*
                   * CS-2677, N-R-3: the host group carrying this queue's host
                   * list goes with it. After the queue, so that a group that
                   * cannot be removed leaves the queue standing rather than the
                   * other way round.
                   */
                  ret &= cqueue_hgroup_delete(packet, answer_list, name);

                  INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, remote_user, remote_host, name, "cluster queue");
                  answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
               } else {
                  ERROR(MSG_CANTSPOOL_SS, "cluster queue", name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            } else {
               ret = false;
            }
         } else {
            ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, "cluster queue", name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         ERROR(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::FieldTraits<CQ_name>::info.name, __func__);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   } else {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   if (ret) {
      DRETURN(STATUS_OK);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}


/** @brief Delete the orphaned instances of one cluster queue
 *
 * @param this_elem the object
 * @param answer_list receives messages for the caller
 * @param ehname see the brief above
 * @param gdi_session the session the change belongs to
 * @return true when anything was deleted
 */
bool
cqueue_del_all_orphaned(const ocs::tom::Element<ocs::tom::CQ> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const char *ehname, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      dstring dir = DSTRING_INIT;
      const char *cq_name = this_elem.get<CQ_name>();
      ocs::tom::List<ocs::tom::QU> qinstance_list = this_elem.get<CQ_qinstances>();
      ocs::tom::Element<ocs::tom::QU> qinstance = nullptr;

      if (ehname) {
         if ((qinstance = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(ehname))) &&
             qinstance_state_is_orphaned(qinstance) &&
             qinstance_slots_used(qinstance) == 0 &&
             qinstance_slots_reserved(qinstance) == 0) {
            const char *qi_name = qinstance.get<QU_qhostname>();

            /*
             * This qinstance should be deleted. There are not jobs anymore.
             */
            sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);
            if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL, 0, 0, cq_name, qi_name,
                                nullptr, nullptr, nullptr, nullptr, true, true, gdi_session)) {
               qinstance_list.erase(qinstance);
               if (qinstance_list.empty()) {
                  sge_rmdir(sge_dstring_get_string(&dir), nullptr);
               }
            }
         }
      } else {
         ocs::tom::Element<ocs::tom::QU> next_qinstance = qinstance_list.front();
         while ((qinstance = next_qinstance) != nullptr) {
            next_qinstance = qinstance.next();

            if (qinstance_state_is_orphaned(qinstance) &&
                qinstance_slots_used(qinstance) == 0 &&
                qinstance_slots_reserved(qinstance) == 0) {
               const char *qi_name = qinstance.get<QU_qhostname>();

               /*
                * This qinstance should be deleted. There are not jobs anymore.
                */
               sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);
               if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL, 0, 0, cq_name, qi_name,
                                   nullptr, nullptr, nullptr, nullptr, true, true, gdi_session)) {
                  qinstance_list.erase(qinstance);
                  if (qinstance_list.empty()) {
                     sge_rmdir(sge_dstring_get_string(&dir), nullptr);
                  }
               }
            }
         }
      }
      sge_dstring_free(&dir);
   }

   DRETURN(ret);
}

/** @brief Delete every orphaned queue instance in the list
 *
 * @param this_list the list to work on
 * @param answer_list receives messages for the caller
 * @param cqname see the brief above
 * @param ehname see the brief above
 * @param gdi_session the session the change belongs to
 * @return true when anything was deleted
 */
bool
cqueue_list_del_all_orphaned(const ocs::tom::List<ocs::tom::CQ> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             const char *cqname, const char *ehname, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (cqname) {
      ret &= cqueue_del_all_orphaned(this_list.find_first<CQ_name>(cqname), answer_list, ehname, gdi_session);
   } else {
      for (const auto cqueue : this_list) {
         ret &= cqueue_del_all_orphaned(cqueue, answer_list, ehname, gdi_session);
         if (!ret) {
            break;
         }
      }
   }

   DRETURN(ret);
}


/** @brief Mark the instances on one host unknown, or clear that state
 *
 * @param this_list the list to work on
 * @param hostname the host
 * @param send_events whether to announce the change to the event clients
 * @param is_unknown see the brief above
 * @param gdi_session the session the change belongs to
 */
void
cqueue_list_set_unknown_state(const ocs::tom::List<ocs::tom::CQ> this_list, const char *hostname,
                              const bool send_events, const bool is_unknown, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   for (const auto cqueue : this_list) {
      if (hostname != nullptr) {
         const ocs::tom::List<ocs::tom::QU> qinstance_list = cqueue.get<CQ_qinstances>();
         const auto qinstance = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(hostname));

         if (qinstance != nullptr && qinstance_state_is_unknown(qinstance) != is_unknown) {
            sge_qmaster_qinstance_state_set_unknown(qinstance, is_unknown, gdi_session);
            if (send_events) {
               qinstance_add_event(qinstance, sgeE_QINSTANCE_MOD, gdi_session);
            }
         }
      } else {
         for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
            if (qinstance_state_is_unknown(qinstance) != is_unknown) {
               sge_qmaster_qinstance_state_set_unknown(qinstance, is_unknown, gdi_session);
               if (send_events) {
                  qinstance_add_event(qinstance, sgeE_QINSTANCE_MOD, gdi_session);
               }
            }
         }
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Collect the references of the two attribute lists of a cluster queue
 *
 * Every key found in a sub-list of an entry of @p Snm1 or @p Snm2 is added
 * once; the list is created, named "", with the first key.
 *
 * @tparam Snm1 the first cluster queue attribute list field, e.g. CQ_projects
 * @tparam Snm2 the second cluster queue attribute list field, e.g. CQ_xprojects
 * @tparam SubF the reference list field of an attribute list entry, e.g. APRJLIST_value
 * @tparam KeyF the key field of a reference, e.g. PR_name
 * @param cqueue the cluster queue
 * @param refs the references collected so far, extended in place
 */
template <auto Snm1, auto Snm2, auto SubF, auto KeyF>
static void
ocs_cqueue_collect_refs(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                        ocs::tom::ListPtr<typename ocs::tom::FieldTraits<KeyF>::Type> &refs) {
   DENTER(TOP_LAYER);

   for (const auto attr_list : {cqueue.get<Snm1>(), cqueue.get<Snm2>()}) {
      for (const auto qc : attr_list) {
         for (const auto ep : qc.template get<SubF>()) {
            const char *p = ep.template get<KeyF>();
            // a reference without a name was not added, as lAddElemStr() refused a null key
            if (p == nullptr) {
               continue;
            }
            if (!refs) {
               refs = ocs::tom::ListPtr<typename ocs::tom::FieldTraits<KeyF>::Type>::create("");
            }
            if (!refs->template find_first<KeyF>(p)) {
               refs->template emplace_back<KeyF>(p);
            }
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Diff cluster queue sublists
 *
 * Determine new/old references in a cluster queue configuration sublist.
 * Names in both lists are removed from both; a list that becomes empty then
 * is freed, as lDiffListStr() did.
 *
 * @tparam Snm1 the first cluster queue attribute list field
 * @tparam Snm2 the second cluster queue attribute list field
 * @tparam SubF the reference list field of an attribute list entry
 * @tparam KeyF the key field of a reference
 * @param new_cqueue New cluster queue (CQ), or an empty handle
 * @param old_cqueue Old cluster queue (CQ), or an empty handle
 * @param new_sublist List of new references, extended; nullptr to collect none
 * @param old_sublist List of old references, extended; nullptr to collect none
 *
 * @note MT-NOTE: cqueue_diff_sublist() is MT safe
 */
template <auto Snm1, auto Snm2, auto SubF, auto KeyF>
static void
cqueue_diff_sublist(const ocs::tom::ConstElement<ocs::tom::CQ> new_cqueue,
                    const ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue,
                    ocs::tom::ListPtr<typename ocs::tom::FieldTraits<KeyF>::Type> *new_sublist,
                    ocs::tom::ListPtr<typename ocs::tom::FieldTraits<KeyF>::Type> *old_sublist) {
   DENTER(TOP_LAYER);

   /* collect 'old' entries in 'old_sublist' */
   if (old_cqueue && old_sublist) {
      ocs_cqueue_collect_refs<Snm1, Snm2, SubF, KeyF>(old_cqueue, *old_sublist);
   }

   /* collect 'new' entries in 'new_sublist' */
   if (new_cqueue && new_sublist) {
      ocs_cqueue_collect_refs<Snm1, Snm2, SubF, KeyF>(new_cqueue, *new_sublist);
   }

   if (new_sublist != nullptr && old_sublist != nullptr && *new_sublist && *old_sublist) {
      const std::size_t new_size = (*new_sublist)->size();
      (*new_sublist)->template remove_common<KeyF>(**old_sublist);
      if ((*new_sublist)->size() != new_size) {
         // lDiffListStr() freed a list that a removal emptied
         if ((*new_sublist)->empty()) {
            new_sublist->reset();
         }
         if ((*old_sublist)->empty()) {
            old_sublist->reset();
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Diff old/new cluster queue projects
 *
 * A diff new/old is made regarding cluster queue projects/xprojects.
 * Project references are returned in new_prj/old_prj.
 *
 * @param new_cqueue New cluster queue (CQ), or an empty handle
 * @param old_cqueue Old cluster queue (CQ), or an empty handle
 * @param new_prj New project references (PR), extended; nullptr to collect none
 * @param old_prj Old project references (PR), extended; nullptr to collect none
 *
 * @note MT-NOTE: cqueue_diff_projects() is MT safe
 */
void
cqueue_diff_projects(const ocs::tom::ConstElement<ocs::tom::CQ> new_cqueue,
                     const ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue,
                     ocs::tom::ListPtr<ocs::tom::PR> *new_prj, ocs::tom::ListPtr<ocs::tom::PR> *old_prj) {
   DENTER(TOP_LAYER);
   cqueue_diff_sublist<CQ_projects, CQ_xprojects, APRJLIST_value, PR_name>(new_cqueue, old_cqueue, new_prj, old_prj);
   DRETURN_VOID;
}

/**
 * @brief Diff old/new cluster queue usersets
 *
 * A diff new/old is made regarding cluster queue acl/xacl.
 * Userset references are returned in new_acl/old_acl.
 *
 * @param new_cqueue New cluster queue (CQ), or an empty handle
 * @param old_cqueue Old cluster queue (CQ), or an empty handle
 * @param new_acl New userset references (US), extended; nullptr to collect none
 * @param old_acl Old userset references (US), extended; nullptr to collect none
 *
 * @note MT-NOTE: cqueue_diff_usersets() is MT safe
 */
void
cqueue_diff_usersets(const ocs::tom::ConstElement<ocs::tom::CQ> new_cqueue,
                     const ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue,
                     ocs::tom::ListPtr<ocs::tom::US> *new_acl, ocs::tom::ListPtr<ocs::tom::US> *old_acl) {
   DENTER(TOP_LAYER);
   cqueue_diff_sublist<CQ_acl, CQ_xacl, AUSRLIST_value, US_name>(new_cqueue, old_cqueue, new_acl, old_acl);
   DRETURN_VOID;
}


/**
 * @brief Update categories wrts userset/project
 *
 * The userset/project information wrts categories is updated based
 * on new/old cluster queue configuration and events are sent upon
 * changes.
 *
 * @param new_cq New cluster queue (CQ)
 * @param old_cq Old cluster queue (CQ)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: cqueue_update_categories() is not MT safe
 */
static void
cqueue_update_categories(const ocs::tom::ConstElement<ocs::tom::CQ> new_cq,
                         const ocs::tom::ConstElement<ocs::tom::CQ> old_cq, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   {
      ocs::tom::ListPtr<ocs::tom::PR> new_lp;
      ocs::tom::ListPtr<ocs::tom::PR> old_lp;
      cqueue_diff_projects(new_cq, old_cq, &new_lp, &old_lp);
      project_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }

   {
      ocs::tom::ListPtr<ocs::tom::US> new_lp;
      ocs::tom::ListPtr<ocs::tom::US> old_lp;
      cqueue_diff_usersets(new_cq, old_cq, &new_lp, &old_lp);
      userset_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }
   DRETURN_VOID;
}

/**
 * @brief Release the unknown state of a queue instance whose host has reported
 *
 * Checks if there are nonstatic load values available for the
 * qinstance. If this is the case, then then the "unknown" state
 * of that machine will be released.
 *
 * @param this_elem the queue instance
 * @param master_exechost_list the execution hosts
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: qinstance_check_unknown_state() is MT safe
 */
static void
qinstance_check_unknown_state(const ocs::tom::Element<ocs::tom::QU> this_elem,
                              const ocs::tom::ConstList<ocs::tom::EH> master_exechost_list,
                              const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const char *hostname = this_elem.get<QU_qhostname>();
   // host_list_locate() has a raw form only; the list holds execution hosts
   const ocs::tom::ConstElement<ocs::tom::EH> host(host_list_locate(master_exechost_list, hostname));

   if (host != nullptr) {
      uint64_t last_heard = host.get<EH_lt_heard_from>();

      if (last_heard != 0) {
         sge_qmaster_qinstance_state_set_unknown(this_elem, false, gdi_session);
      }
   }
   DRETURN_VOID;
}
