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
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Queue instances: one queue on one host
 *
 * A cluster queue is a template; the queue instance is what a job actually
 * runs in. Its name is `cqueue@host`, and most of its attributes were resolved
 * from the cluster queue's per host lists when the instance was created.
 *
 * This is also where consumables are booked and unbooked: #rc_debit_consumable
 * is the single place that changes a resource container's actual values, for
 * queues, hosts and the global host alike.
 *
 * @see sge_qinstance.h
 */

#include <cstring>
#include <type_traits>

#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_hostname.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_RUE.h"

#include "msg_common.h"
#include "ocs_TopologyString.h"
#include "sched/sge_resource_utilization.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge.h"
#include "uti/sge_time.h"

/// Debug layer the queue instance traces are written to
#define QINSTANCE_LAYER BASIS_LAYER

/**
 * @brief Find a qinstance
 *
 * Find a qinstance in "this_list" which is part of the cluster queue
 * with the name "cqueue_name" and resides on the host with the name
 * "hostname".
 *
 * @param this_list QU_Type list, may be an empty handle
 * @param hostname hostname
 * @param cqueue_name cluster queue name; nullptr looks up the host alone
 *
 * @return QU_Type element, or an empty handle
 *
 * @note MT-NOTE: qinstance_list_locate() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> qinstance_list_locate(const ocs::tom::ConstList<ocs::tom::QU> this_list,
                                                      const char *hostname, const char *cqueue_name) {
   ocs::tom::Element<ocs::tom::QU> ret = nullptr;

   if (cqueue_name == nullptr) {
      ret = ocs::tom::find_first_rw<QU_qhostname>(this_list, hostname);
   } else {
      for (const auto qinstance : this_list) {
         const char *qname = ocs::tom::get<QU_qname>(qinstance);
         const char *hname = ocs::tom::get<QU_qhostname>(qinstance);

         /* use qinstance expression */
         if (!sge_eval_expression(ocs::CEntry::Type::CSTR, cqueue_name, qname, nullptr)) {
            if (!sge_eval_expression(ocs::CEntry::Type::HOST, hostname, hname, nullptr)) {
               // the list is read-only for the search only; the caller gets the element writable, as before
               ret = ocs::tom::Element<ocs::tom::QU>::from_typed(const_cast<lListElem *>(qinstance.raw()));
               break;
            }
         }
      }
   }
   return ret;
}

/**
 * @brief Find a qinstance using the fullname
 *
 * find a qinstance using the fullname
 *
 * @param queue_list QU_Type list, may be an empty handle
 * @param full_name fullname of the qinstance (`cqueue@hostname`)
 *
 * @return QU_type element, or an empty handle
 *
 * @note MT-NOTE: qinstance_list_locate2() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> qinstance_list_locate2(const ocs::tom::ConstList<ocs::tom::QU> queue_list,
                                                       const char *full_name) {
   return ocs::tom::find_first_rw<QU_full_name>(queue_list, full_name);
}

/**
 * @brief Returns the fullname of a qinstance object
 *
 * @param this_elem QU_Type element, may be an empty handle
 * @param string_buffer dynamic string buffer
 *
 * @return pointer to the internal string buffer of "string_buffer", or nullptr if an argument is missing
 *
 * @note MT-NOTE: qinstance_get_name() is MT safe
 */
const char *qinstance_get_name(const ocs::tom::ConstElement<ocs::tom::QU> this_elem, dstring *string_buffer) {
   const char *ret = nullptr;

   if (this_elem && string_buffer != nullptr) {
      ret = sge_dstring_sprintf(string_buffer, SFN "@" SFN,
                                ocs::tom::get<QU_qname>(this_elem),
                                static_cast<const char *>(ocs::tom::get<QU_qhostname>(this_elem)));
   }
   return ret;
}

/**
 * @brief qinstance_get_name() for an untyped caller
 *
 * @param this_elem QU_Type element, checked; may be nullptr
 * @param string_buffer dynamic string buffer
 *
 * @return as qinstance_get_name()
 */
const char *qinstance_get_name(const lListElem *this_elem, dstring *string_buffer) {
   return qinstance_get_name(ocs::tom::ConstElement<ocs::tom::QU>(this_elem), string_buffer);
}

/**
 * @brief Increase the qinstance queue version
 *
 * Increase the queue version of the given qinstance "this_elem".
 *
 * @param this_elem QU_Type element
 *
 * @note MT-NOTE: qinstance_increase_qversion() is MT safe
 */
void qinstance_increase_qversion(const ocs::tom::Element<ocs::tom::QU> this_elem) {
   DENTER(TOP_LAYER);
   ocs::tom::add<QU_version>(this_elem, 1);
   DRETURN_VOID;
}

/**
 * @brief qinstance_increase_qversion() for an untyped caller
 *
 * @param this_elem QU_Type element, checked
 */
void qinstance_increase_qversion(lListElem *this_elem) {
   qinstance_increase_qversion(ocs::tom::Element<ocs::tom::QU>(this_elem));
}

/**
 * @brief check if the packet user is the owner of the qinstance
 *
 * This function checks if the user of the given packet is the owner of the
 * qinstance element "this_elem".
 *
 * @param packet GDI packet containing the ownership information of the request creator
 * @param this_elem qinstance element to check (QU_Type), may be an empty handle
 * @return true if the user is in the owner list, false otherwise or if an argument is missing
 */
bool qinstance_is_owner(const ocs::gdi::Packet *packet, const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (packet == nullptr) {
      DRETURN(false);
   }
   if (this_elem) {
      const auto owner_list = ocs::tom::get<QU_owner_list>(this_elem);
      if (ocs::tom::find_first<US_name>(owner_list, packet->user)) {
         ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief qinstance_is_owner() for an untyped caller
 *
 * @param packet GDI packet containing the ownership information of the request creator
 * @param this_elem qinstance element to check (QU_Type), checked; may be nullptr
 * @return as qinstance_is_owner()
 */
bool qinstance_is_owner(const ocs::gdi::Packet *packet, const lListElem *this_elem) {
   return qinstance_is_owner(packet, ocs::tom::ConstElement<ocs::tom::QU>(this_elem));
}

/**
 * @brief Is the PE object referenced
 *
 * Is the given PE ("pe") referenced in the qinstance element "this_elem".
 *
 * @param this_elem QU_Type element
 * @param pe PE_Type element
 *
 * @return test result true  - is referenced false - is not referenced
 *
 * @note MT-NOTE: qinstance_is_pe_referenced() is MT safe
 */
bool qinstance_is_pe_referenced(const ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                const ocs::tom::ConstElement<ocs::tom::PE> pe) {
   DENTER(TOP_LAYER);
   for (const auto pe_name_elem : ocs::tom::get<QU_pe_list>(this_elem)) {
      if (strcmp(ocs::tom::get<PE_name>(pe), ocs::tom::get<ST_name>(pe_name_elem)) == 0) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/**
 * @brief Is a PE referenced
 *
 * Test is at least one PE is referenced by qinstance "this_elem"
 *
 * @param this_elem QU_Type
 *
 * @return test result true  - an PE is referenced false - no PE is referenced ("NONE")
 *
 * @note MT-NOTE: qinstance_is_a_pe_referenced() is MT safe
 */
bool qinstance_is_a_pe_referenced(const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (!ocs::tom::get<QU_pe_list>(this_elem).empty()) {
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Is the CKTP referenced
 *
 * Tests if the given CKPT object ("ckpt") is referenced in
 * the qinstance "this_elem".
 *
 * @param this_elem QU_Type element
 * @param ckpt CK_Type element
 *
 * @return test result true  - CKPT is referenced false - CKPT is not referenced
 *
 * @note MT-NOTE: qinstance_is_ckpt_referenced() is MT safe
 */
bool qinstance_is_ckpt_referenced(const ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                  const ocs::tom::ConstElement<ocs::tom::CK> ckpt) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const auto ckpt_list = ocs::tom::get<QU_ckpt_list>(this_elem);

   if (ocs::tom::find_first<ST_name>(ckpt_list, ocs::tom::get<CK_name>(ckpt))) {
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Is an CKPT object referenced
 *
 * Is an CKPT object referenced in "this_elem".
 *
 * @param this_elem QU_Type element
 *
 * @return test result true  - a CKPT is referenced false - no CKPT is referenced
 *
 * @note MT-NOTE: qinstance_is_a_ckpt_referenced() is MT safe
 */
bool qinstance_is_a_ckpt_referenced(const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (!ocs::tom::get<QU_ckpt_list>(this_elem).empty()) {
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Is it a complex_value
 *
 * Is the given "centry" in the list of complex_values of "this_elem".
 *
 * @param this_elem QU_Type element, may be an empty handle
 * @param centry CE_Type element
 *
 * @return test result true  - it is a complex value false - no complex value
 *
 * @note MT-NOTE: qinstance_is_centry_a_complex_value() is MT safe
 */
bool qinstance_is_centry_a_complex_value(const ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                         const ocs::tom::ConstElement<ocs::tom::CE> centry) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (this_elem) {
      const char *name = ocs::tom::get<CE_name>(centry);
      const auto centry_list = ocs::tom::get<QU_consumable_config_list>(this_elem);
      const auto centry_ref = ocs::tom::find_first<CE_name>(centry_list, name);

      if (centry_ref || get_rsrc(name, true, nullptr, nullptr, nullptr, nullptr) == 0) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find certain qinstances
 *
 * Finds all qinstances in "this_list" whose hostname part matches
 * the "hostname_pattern" (fnmatch pattern) and returns their
 * qinstance names as queue references. In case of any error "answer_list"
 * will be filled.
 *
 * @param this_list QU_Type list, may be an empty handle
 * @param answer_list AN_Type list
 * @param hostname_pattern fnmatch hostname pattern
 *
 * @return QR_Type list of the matching qinstance names; an empty handle if nothing matched or an argument is missing
 *
 * @note MT-NOTE: qinstance_list_find_matching() is MT safe
 */
ocs::tom::ListPtr<ocs::tom::QR> qinstance_list_find_matching(const ocs::tom::ConstList<ocs::tom::QU> this_list,
                                                             lList **answer_list, const char *hostname_pattern) {
   DENTER(QINSTANCE_LAYER);

   // created by the first match, as the old out-parameter was
   ocs::tom::ListPtr<ocs::tom::QR> qref_list;

   if (this_list && hostname_pattern != nullptr) {
      char host[CL_MAXHOSTNAMELEN];

      if ((getuniquehostname(hostname_pattern, host, 0)) == CL_RETVAL_OK) {
         hostname_pattern = host;
      }

      const bool hostname_pattern_is_expression = ocs::is_expression(hostname_pattern);
      for (const auto qinstance : this_list) {
         const char *hostname = ocs::tom::get<QU_qhostname>(qinstance);
         /* use qinstance expression */
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, hostname_pattern, hostname, answer_list, true,
                                  hostname_pattern_is_expression)) {
            // a qinstance without a full name is skipped, as the free emplace_back() took the soft path for it
            const char *full_name = ocs::tom::get<QU_full_name>(qinstance);
            if (full_name != nullptr) {
               if (!qref_list) {
                  qref_list = ocs::tom::ListPtr<ocs::tom::QR>::create("");
               }
               qref_list->emplace_back<QR_name>(full_name);
            }
         }
      }
   }
   DRETURN(qref_list);
}

/**
 * @brief Returns the number of currently used slots
 *
 * Returns the number of currently used slots.
 *
 * @param this_elem QU_Type element
 *
 * @return number of slots
 *
 * @note MT-NOTE: qinstance_slots_used() is MT safe
 */
int qinstance_slots_used(const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(QINSTANCE_LAYER);
   const auto slots = ocs::tom::find_first_in<QU_resource_utilization, RUE_name>(this_elem, SGE_ATTR_SLOTS);
   int ret = 1000000; /* when slots is unknown */

   if (slots) {
      ret = ocs::tom::get<RUE_utilized_now>(slots);
   } else {
      /* this happens on qinstance_create when a queue instance is created
         before others queue instances in the subordinate lists
         are created */
      CRITICAL(MSG_QINSTANCE_MISSLOTS_S, ocs::tom::get<QU_full_name>(this_elem));
   }
   DRETURN(ret);
}

/**
 * @brief qinstance_slots_used() for an untyped caller
 *
 * @param this_elem QU_Type element, checked
 *
 * @return as qinstance_slots_used()
 */
int qinstance_slots_used(const lListElem *this_elem) {
   return qinstance_slots_used(ocs::tom::ConstElement<ocs::tom::QU>(this_elem));
}

/**
 * @brief The number of maximal reserved slots
 *
 * Returns the number of maximal reserved slots by all advance reservations
 *
 * @param this_elem QU_Type element
 *
 * @return number of slots
 *
 * @note MT-NOTE: qinstance_slots_reserved() is MT safe
 */
uint32_t qinstance_slots_reserved(const ocs::tom::ConstElement<ocs::tom::QU> this_elem) {
   DENTER(QINSTANCE_LAYER);
   uint32_t ret = 0;
   const auto slots = ocs::tom::find_first_in<QU_resource_utilization, RUE_name>(this_elem, SGE_ATTR_SLOTS);
   if (slots) {
      for (const auto utilized : ocs::tom::get<RUE_utilized>(slots)) {
         ret = std::max(static_cast<double>(ret), ocs::tom::get<RDE_amount>(utilized));
      }
   }

   DRETURN(ret);
}

/**
 * @brief qinstance_slots_reserved() for an untyped caller
 *
 * @param this_elem QU_Type element, checked
 *
 * @return as qinstance_slots_reserved()
 */
uint32_t qinstance_slots_reserved(const lListElem *this_elem) {
   return qinstance_slots_reserved(ocs::tom::ConstElement<ocs::tom::QU>(this_elem));
}

/**
 * @brief Modifies the number of used slots
 *
 * @param this_elem QU_Type
 * @param new_slots new slot value
 *
 * @note MT-NOTE: qinstance_set_slots_used() is MT safe
 */
void qinstance_set_slots_used(const ocs::tom::Element<ocs::tom::QU> this_elem, const int new_slots) {
   DENTER(QINSTANCE_LAYER);
   const auto slots = ocs::tom::find_first_in_rw<QU_resource_utilization, RUE_name>(this_elem, "slots");
   if (slots) {
      ocs::tom::set<RUE_utilized_now>(slots, new_slots);
   } else {
      /* because this should never happen and an critical error */
      CRITICAL(MSG_QINSTANCE_MISSLOTS_S, ocs::tom::get<QU_full_name>(this_elem));
   }
   DRETURN_VOID;
}

/**
 * @brief Debits/Undebits consumables
 *
 * Checks if there are nonstatic load values avaialable for the
 * qinstance. If this is the case, then then the "unknown" state
 * of that machine will be released.
 *
 * @param qep Qinstance resource container
 * @param jep The job (JB_Type) defining which resources and how much of them need to be (un)debited;
 *            an empty handle only initializes the booking
 * @param pe The job's parallel environment, needed to split master and slave requests; may be an empty handle
 * @param centry_list The global complex list that is needed to interpret the jobs' resource requests.
 * @param slots The number of slots for which we are debiting. Positive slots numbers cause debiting, negative ones cause undebiting.
 * @param is_master_task true when this booking includes the job's master task
 * @param do_per_host_booking true when a per host consumable still has to be booked on this host
 * @param just_check If != nullptr do not debit, only check whether debiting would exceed the resources
 *
 * @return -1 in case of an error. Otherwise the number of (un)debitations that actually took place;
 *         0 means the consumable resources of the 'ep' object have not changed.
 *
 * @note MT-NOTE: qinstance_debit_consumable() is MT safe
 */
int qinstance_debit_consumable(const ocs::tom::Element<ocs::tom::QU> qep,
                               const ocs::tom::ConstElement<ocs::tom::JB> jep,
                               const ocs::tom::ConstElement<ocs::tom::PE> pe,
                               const ocs::tom::ConstList<ocs::tom::CE> centry_list, const int slots,
                               const bool is_master_task, const bool do_per_host_booking, bool *just_check) {
   return rc_debit_consumable<ocs::tom::QU, QU_consumable_config_list, QU_resource_utilization>(
      jep, pe, qep, centry_list, slots, ocs::tom::get<QU_qname>(qep), is_master_task, do_per_host_booking, just_check);
}

/**
 * @brief qinstance_debit_consumable() for an untyped caller
 *
 * @param qep Qinstance resource container (QU_Type), checked
 * @param jep The job (JB_Type), checked; nullptr only initializes the booking
 * @param pe The job's parallel environment (PE_Type), checked; may be nullptr
 * @param centry_list The global complex list (CE_Type), checked
 * @param slots The number of slots for which we are debiting; negative ones undebit
 * @param is_master_task true when this booking includes the job's master task
 * @param do_per_host_booking true when a per host consumable still has to be booked on this host
 * @param just_check If != nullptr do not debit, only check whether debiting would exceed the resources
 *
 * @return as qinstance_debit_consumable()
 */
int qinstance_debit_consumable(lListElem *qep, const lListElem *jep, const lListElem *pe, const lList *centry_list,
                               const int slots, const bool is_master_task, const bool do_per_host_booking,
                               bool *just_check) {
   return qinstance_debit_consumable(ocs::tom::Element<ocs::tom::QU>(qep), ocs::tom::ConstElement<ocs::tom::JB>(jep),
                                     ocs::tom::ConstElement<ocs::tom::PE>(pe),
                                     ocs::tom::ConstList<ocs::tom::CE>(centry_list), slots, is_master_task,
                                     do_per_host_booking, just_check);
}

/**
 * @brief Adds a message to the qinstance structure
 *
 * @param this_elem QU_Type
 * @param type message type
 * @param message message
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: qinstance_message_add() is MT safe
 */
bool qinstance_message_add(const ocs::tom::Element<ocs::tom::QU> this_elem, const uint32_t type,
                           const char *message) {
   DENTER(TOP_LAYER);

   bool ret = true;

   object_message_add<QU_message_list>(this_elem, type, message);
   DRETURN(ret);
}

/**
 * @brief qinstance_message_add() for an untyped caller
 *
 * @param this_elem QU_Type, checked
 * @param type message type
 * @param message message
 *
 * @return as qinstance_message_add()
 */
bool qinstance_message_add(lListElem *this_elem, const uint32_t type, const char *message) {
   return qinstance_message_add(ocs::tom::Element<ocs::tom::QU>(this_elem), type, message);
}

/**
 * @brief Trash messages
 *
 * Removes all messages with the message "type" id.
 *
 * @param this_elem QU_Type
 * @param type message type
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: qinstance_message_trash_all_of_type_X() is MT safe
 */
bool qinstance_message_trash_all_of_type_X(const ocs::tom::Element<ocs::tom::QU> this_elem, const uint32_t type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   object_message_trash_all_of_type_X<QU_message_list>(this_elem, type);
   DRETURN(ret);
}

/**
 * @brief qinstance_message_trash_all_of_type_X() for an untyped caller
 *
 * @param this_elem QU_Type, checked
 * @param type message type
 *
 * @return as qinstance_message_trash_all_of_type_X()
 */
bool qinstance_message_trash_all_of_type_X(lListElem *this_elem, const uint32_t type) {
   return qinstance_message_trash_all_of_type_X(ocs::tom::Element<ocs::tom::QU>(this_elem), type);
}

/**
 * @brief Set the full name of the qinstance
 *
 * Set the full name of the qinstance. The QU_qname name attribute
 * will be used as input for the cqueue part and QU_qhostname
 * as hostname part of the full name.
 *
 * @param this_elem QU_Type
 *
 * @note MT-NOTE: qinstance_set_full_name() is MT safe
 */
void qinstance_set_full_name(const ocs::tom::Element<ocs::tom::QU> this_elem) {
   dstring buffer = DSTRING_INIT;
   const char *cqueue_name = ocs::tom::get<QU_qname>(this_elem);
   const char *hostname = ocs::tom::get<QU_qhostname>(this_elem);

   sge_dstring_sprintf(&buffer, "%s@%s", cqueue_name, hostname);
   ocs::tom::set<QU_full_name>(this_elem, sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);
}

/**
 * @brief qinstance_set_full_name() for an untyped caller
 *
 * @param this_elem QU_Type, checked
 */
void qinstance_set_full_name(lListElem *this_elem) {
   qinstance_set_full_name(ocs::tom::Element<ocs::tom::QU>(this_elem));
}

/**
 * @brief Validates and initializes qinstances
 *
 * Validates qinstance attributes and initializes them if necessary.
 *
 * @param this_elem QU_Type
 * @param answer_list AN_Type
 * @param master_exechost_list the execution hosts, so the queue's host can be checked
 * @param master_centry_list the complex entries, so the queue's resources can be checked
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: qinstance_validate() is MT safe
 */
bool qinstance_validate(const ocs::tom::Element<ocs::tom::QU> this_elem, lList **answer_list,
                        const ocs::tom::ConstList<ocs::tom::EH> master_exechost_list,
                        const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /* QU_full_name isn't spooled, if it is not set, create it */
   if (ocs::tom::get<QU_full_name>(this_elem) == nullptr) {
      qinstance_set_full_name(this_elem);
   }

   /* handle slots from now on as a consumble attribute of queue */
   qinstance_set_conf_slots_used(this_elem);

   /* remove all queue message, which are regenerated during the unspooling
      the queue */
   qinstance_message_trash_all_of_type_X(this_elem, ~QI_ERROR);

   /* setup actual list of queue; typed empty handles, as the raw overload takes nullptr as well */
   qinstance_debit_consumable(this_elem, ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::PE>(),
                              master_centry_list, 0, true, true, nullptr);

   /* init double values of consumable configuration */
   if (centry_list_fill_request(ocs::tom::get<QU_consumable_config_list>(this_elem), answer_list, master_centry_list,
                                true, false, true) != 0) {
      ret = false;
   }

   if (ret) {
      if (ensure_attrib_available(nullptr, this_elem, QU_load_thresholds, master_centry_list) ||
          ensure_attrib_available(nullptr, this_elem, QU_suspend_thresholds, master_centry_list) ||
          ensure_attrib_available(nullptr, this_elem, QU_consumable_config_list, master_centry_list)) {
         ret = false;
      }
   }

   /* qinstance state */
   if (ret) {
      qinstance_state_set_unknown(this_elem, true);
      qinstance_state_set_cal_disabled(this_elem, false);
      qinstance_state_set_cal_suspended(this_elem, false);
      qinstance_set_slots_used(this_elem, 0);

      if (host_list_locate(master_exechost_list,
                           ocs::tom::get<QU_qhostname>(this_elem)) == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_QINSTANCE_HOSTFORQUEUEDOESNOTEXIST_SS,
                                 ocs::tom::get<QU_qname>(this_elem),
                                 static_cast<const char *>(ocs::tom::get<QU_qhostname>(this_elem)));
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief qinstance_validate() for an untyped caller
 *
 * @param this_elem QU_Type, checked
 * @param answer_list AN_Type
 * @param master_exechost_list the execution hosts (EH_Type), checked
 * @param master_centry_list the complex entries (CE_Type), checked
 *
 * @return as qinstance_validate()
 */
bool qinstance_validate(lListElem *this_elem, lList **answer_list, const lList *master_exechost_list,
                        const lList *master_centry_list) {
   return qinstance_validate(ocs::tom::Element<ocs::tom::QU>(this_elem), answer_list,
                             ocs::tom::ConstList<ocs::tom::EH>(master_exechost_list),
                             ocs::tom::ConstList<ocs::tom::CE>(master_centry_list));
}

/**
 * @brief Validates and initializes qinstances
 *
 * Validates qinstances attributes and initializes them if necessary.
 *
 * @param this_list QU_Type list, may be an empty handle
 * @param answer_list AN_Type
 * @param master_exechost_list the execution hosts, so each queue's host can be checked
 * @param master_centry_list the complex entries, so each queue's resources can be checked
 *
 * @return true on success, false at the first queue that failed
 *
 * @note MT-NOTE: qinstance_list_validate() is MT safe
 */
bool qinstance_list_validate(const ocs::tom::List<ocs::tom::QU> this_list, lList **answer_list,
                             const ocs::tom::ConstList<ocs::tom::EH> master_exechost_list,
                             const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for (const auto qinstance : this_list) {
      if (!qinstance_validate(qinstance, answer_list, master_exechost_list, master_centry_list)) {
         ret = false;
         break;
      }
   }

   DRETURN(ret);
}

/**
 * @brief qinstance_list_validate() for an untyped caller
 *
 * @param this_list QU_Type list, checked; may be nullptr
 * @param answer_list AN_Type
 * @param master_exechost_list the execution hosts (EH_Type), checked
 * @param master_centry_list the complex entries (CE_Type), checked
 *
 * @return as qinstance_list_validate()
 */
bool qinstance_list_validate(lList *this_list, lList **answer_list, const lList *master_exechost_list,
                             const lList *master_centry_list) {
   return qinstance_list_validate(ocs::tom::List<ocs::tom::QU>(this_list), answer_list,
                                  ocs::tom::ConstList<ocs::tom::EH>(master_exechost_list),
                                  ocs::tom::ConstList<ocs::tom::CE>(master_centry_list));
}

/**
 * @brief Book an exclusive consumable that the job does not request
 *
 * The job doesn't request the exclusive complex, but it still has effect:
 * A job actually requesting to run exclusive will not be dispatched
 * to this host - scheduler checks if there is a job running either
 * requesting the exclusive resource or a job not requesting it is
 * blocking the host.
 *
 * For the consumable check (just_check != nullptr) this means that
 * we check if the resource is actually in use (RU_utilized_now).
 *
 * @param name the consumable's name
 * @param obj_type "queue" or "host", for the messages
 * @param obj_name the name of the resource container, for the messages
 * @param slots the number of slots to book
 * @param booking_ep the booking of the consumable (RUE_Type); may be an empty handle when only checking
 * @param just_check If != nullptr do not book, only check; set to false when the check fails
 *
 * @return true, false in case of just_check and the check fails
 */
static bool
rc_debit_consumable_implicit_exclusive(const char *name, const char *obj_type, const char *obj_name, const int slots,
                                       const ocs::tom::Element<ocs::tom::RUE> booking_ep, bool *just_check) {
   DENTER(TOP_LAYER);
   bool ret = true;

   double request = 1.0;
   if (just_check == nullptr) {
      DPRINTF("debiting (implicit exclusive) %f of %s on %s %s for %d slots\n", request, name,
              obj_type, obj_name, slots);
      ocs::tom::add<RUE_utilized_now_nonexclusive>(booking_ep, slots * request);
   } else {
      double actual_value = !booking_ep ? 0 : ocs::tom::get<RUE_utilized_now>(booking_ep);
      if (actual_value > 0) {
         ERROR(MSG_EXCLCAPACITYEXCEEDED_FSSSI, request, name, obj_type, obj_name, slots);
         *just_check = false;
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Book or check an explicit request of a consumable
 *
 * @param name the consumable's name
 * @param obj_type "queue" or "host", for the messages
 * @param obj_name the name of the resource container, for the messages
 * @param slots the number of slots to book; ignored for an exclusive consumable when checking
 * @param request the amount requested per slot
 * @param additional_request an amount already booked on this container in the same check (master task)
 * @param capacity_ep the configured capacity (CE_Type)
 * @param booking_ep the booking of the consumable (RUE_Type); may be an empty handle when only checking
 * @param is_exclusive whether the consumable is exclusive
 * @param just_check If != nullptr do not book, only check; set to false when the check fails
 *
 * @return true, false in case of just_check and the check fails
 */
static bool
rc_debit_consumable_explicit_request(const char *name, const char *obj_type, const char *obj_name, int slots,
                                     const double request, const double additional_request,
                                     const ocs::tom::ConstElement<ocs::tom::CE> capacity_ep,
                                     const ocs::tom::Element<ocs::tom::RUE> booking_ep, const bool is_exclusive,
                                     bool *just_check) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (just_check == nullptr) {
      DPRINTF("debiting %f of %s on %s %s for %d slots\n", request, name, obj_type, obj_name, slots);
      ocs::tom::add<RUE_utilized_now>(booking_ep, slots * request);
   } else {
      DPRINTF("verifying %f of %s on %s %s for %d slots + %f\n", request, name, obj_type, obj_name, slots,
              additional_request);
      double actual_value = !booking_ep ? 0 : ocs::tom::get<RUE_utilized_now>(booking_ep);
      double config_value = ocs::tom::get<CE_doubleval>(capacity_ep);
      /* for exclusive consumables ignore the number of slots */
      if (is_exclusive) {
         slots = 1;
      }
      if ((config_value - actual_value - slots * request - additional_request) < 0) {
         ERROR(MSG_CAPACITYEXCEEDED_FSSSIFF, request, name, obj_type, obj_name, slots, additional_request,
               config_value - actual_value);
         *just_check = false;
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Debit/Undebit consumables from resource container
 *
 * Updates all consumable actual values of a resource container
 * for 'slots' slots of the given job. Positive slots numbers
 * cause debiting, negative ones cause undebiting.
 *
 * @param jep The job (JB_Type) defining which resources and how much of them need to be (un)debited;
 *            an empty handle only initializes the booking
 * @param pe The job's parallel environment, needed to split master and slave requests; may be an empty handle
 * @tparam T the type of the resource container: ocs::tom::QU, or ocs::tom::EH for the global and the other hosts
 * @tparam CONFIG_NM the field of T that contains a CE_Type list of configured complex values
 * @tparam ACTUAL_NM the field of T that contains a RUE_Type list of actual complex values
 * @param ep The resource container (global/host/queue) that owns the resources, may be an empty handle
 * @param centry_list The global complex list that is needed to interpret the jobs' resource requests.
 * @param slots The number of slots for which we are debiting. Positive slots numbers cause debiting, negative ones cause undebiting.
 * @param obj_name The name of the object we are debiting from. This is only used for monitoring/diagnosis purposes.
 * @param is_master_task true when this booking includes the job's master task
 * @param do_per_host_booking true when a per host consumable still has to be booked on this host
 * @param just_check If != nullptr do not do the actual debiting, but just check if debiting would exceed resources. Only makes sense for debiting (slots > 0).
 *
 * @return -1 in case of an error. Otherwise the number of (un)debitations that actually took place;
 *         0 means the consumable resources of the 'ep' object have not changed.
 */
template <typename T, auto CONFIG_NM, auto ACTUAL_NM>
int rc_debit_consumable(const ocs::tom::ConstElement<ocs::tom::JB> jep, const ocs::tom::ConstElement<ocs::tom::PE> pe,
                        const ocs::tom::Element<T> ep, const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                        const int slots, const char *obj_name, const bool is_master_task,
                        const bool do_per_host_booking, bool *just_check) {
   DENTER(TOP_LAYER);
   int mods = 0;

   if (!ep) {
      DRETURN(0);
   }

   /* assume debiting would work */
   if (just_check != nullptr) {
      *just_check = true;
   }

   // loop over all queue/exechost complex_values (QU_consumable_config_list, EH_consumable_config_list)
   for (const auto cr_config : ocs::tom::get<CONFIG_NM>(ep)) {
      ocs::tom::Element<ocs::tom::RUE> cr;
      const char *name = ocs::tom::get<CE_name>(cr_config);

      // search complex definition (with default request); found in a CE_Type list, so no check is needed
      const auto dcep = ocs::tom::ConstElement<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, name));
      if (!dcep) {
         ERROR(MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S , name);
         DRETURN(-1);
      }

      uint32_t consumable = ocs::tom::get<CE_consumable>(dcep);

      // ensure attribute is in actual list
      // @todo we could do this only if jep == nullptr - this is the call to initialize booking
      if (consumable != CONSUMABLE_NO) {
         cr = ocs::tom::find_first_in_rw<ACTUAL_NM, RUE_name>(ep, name);
         if (just_check == nullptr && !cr) {
            cr = ocs::tom::emplace_back_in<ACTUAL_NM, RUE_name>(ep, name);
            /* RUE_utilized_now is implicitly set to zero */
         }
      }

      if (jep) {
         if (!consumable_do_booking(consumable, is_master_task, do_per_host_booking)) {
            continue;
         }

         bool did_booking = false;
         bool is_exclusive = ocs::tom::get<CE_relop>(dcep) == CMPLXEXCL_OP;
         const char *obj_type = std::is_same_v<T, ocs::tom::QU> ? "queue" : "host";

         int debit_slots = consumable_get_debit_slots(consumable, slots);
         DPRINTF("===> rc_debit_consumable(): %s: %s: debit_slots: %d\n", obj_name, name, debit_slots);

         // has contribution from global requests? Then we can do the booking for master and slave task in one step.
         double dval = 0.0;
         bool tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &dval, dcep, JRS_SCOPE_GLOBAL);
         if (tmp_ret) {
            // the resource was requested
            DPRINTF("===> rc_debit_consumable(): %s: we have GLOBAL %s request: %f for %d slots\n", obj_name, name,
                    dval, debit_slots);
            if (dval != 0.0) {
               if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, debit_slots, dval, 0.0, cr_config,
                                                         cr, is_exclusive, just_check)) {
                  break;
               }
               mods++;
               did_booking = true;
            }
         } else if (pe) {
            // no global contribution, need to check master and slave
            // we use the original slots value for slave_debit_slots
            // reason: for host consumables, debit_slots is 1, which will be adjusted below to 0
            //         if we have then a slave request for a host consumable, it will not be booked!
            int slave_debit_slots = slots;
            double master_dval = 0.0;
            if (is_master_task) {
               // if the master task is part of this booking, check if we have a master request
               tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &master_dval, dcep, JRS_SCOPE_MASTER);
               if (tmp_ret) {
                  // we have a master request
                  DPRINTF("===> rc_debit_consumable(): %s: we have MASTER %s request: %f for %d slots\n", obj_name,
                          name, master_dval, slot_signum(debit_slots));
                  if (master_dval != 0.0) {
                     // book it for one slot
                     if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, slot_signum(debit_slots),
                                                               master_dval, 0.0, cr_config, cr, is_exclusive,
                                                               just_check)) {
                        break;
                     }
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
               double slave_dval = 0.0;
               // if we are on the master host, and we shall ignore slave requests to booking only for slots
               if (is_master_task && ocs::tom::get<PE_ignore_slave_requests_on_master_host>(pe)) {
                  tmp_ret = true;
                  if (sge_strnullcmp(name, SGE_ATTR_SLOTS) == 0) {
                     slave_dval = 1.0;
                  }
               } else {
                  // we consider the slave requests
                  tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &slave_dval, dcep, JRS_SCOPE_SLAVE);
               }
               if (tmp_ret && slave_dval != 0.0) {
                  // we have a slave request
                  slave_debit_slots = consumable_get_debit_slots(consumable, slave_debit_slots);
                  DPRINTF("===> rc_debit_consumable(): %s: we have SLAVE %s request: %f for %d slots\n", obj_name,
                          name, slave_dval, slave_debit_slots);
                  if (slave_dval != 0.0) {
                     // in case of just_check: need to pass the master_dval as additional usage on this host/queue
                     if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, slave_debit_slots,
                                                               slave_dval, just_check != nullptr ? master_dval : 0.0,
                                                               cr_config, cr, is_exclusive, just_check)) {
                        break;
                     }
                     mods++;
                     did_booking = true;
                  }
               }
            }
         }

         // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
         if (!did_booking && is_exclusive) {
            if (!rc_debit_consumable_implicit_exclusive(name, obj_type, obj_name, debit_slots, cr, just_check)) {
               break;
            }
            mods++;
         }
      }
   }

   DRETURN(mods);
}

// the two kinds of resource container; the template is defined here only
template int rc_debit_consumable<ocs::tom::QU, QU_consumable_config_list, QU_resource_utilization>(
   ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::PE> pe,
   ocs::tom::Element<ocs::tom::QU> ep, ocs::tom::ConstList<ocs::tom::CE> centry_list, int slots, const char *obj_name,
   bool is_master_task, bool do_per_host_booking, bool *just_check);
template int rc_debit_consumable<ocs::tom::EH, EH_consumable_config_list, EH_resource_utilization>(
   ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::PE> pe,
   ocs::tom::Element<ocs::tom::EH> ep, ocs::tom::ConstList<ocs::tom::CE> centry_list, int slots, const char *obj_name,
   bool is_master_task, bool do_per_host_booking, bool *just_check);

/**
 * @brief Mirror the configured slot count into the consumable configuration
 *
 * `slots` is configured as a plain queue attribute but booked like any other
 * consumable, so the value has to exist in both places. The entry is created
 * when it does not exist yet.
 *
 * @param[in,out] this_elem the queue instance to update
 */
void qinstance_set_conf_slots_used(const ocs::tom::Element<ocs::tom::QU> this_elem) {
   DENTER(QINSTANCE_LAYER);
   auto slots = ocs::tom::find_first_in_rw<QU_consumable_config_list, CE_name>(this_elem, "slots");
   if (!slots) {
      slots = ocs::tom::emplace_back_in<QU_consumable_config_list, CE_name>(this_elem, "slots");
   }
   if (slots) {
      DSTRING_STATIC(buffer, 32);
      uint32_t slots_value = ocs::tom::get<QU_job_slots>(this_elem);

      ocs::tom::set<CE_doubleval>(slots, slots_value);
      ocs::tom::set<CE_stringval>(slots, sge_dstring_sprintf(&buffer, sge_u32, slots_value));
   }
   DRETURN_VOID;
}

/**
 * @brief qinstance_set_conf_slots_used() for an untyped caller
 *
 * @param[in,out] this_elem the queue instance to update (QU_Type), checked
 */
void qinstance_set_conf_slots_used(lListElem *this_elem) {
   qinstance_set_conf_slots_used(ocs::tom::Element<ocs::tom::QU>(this_elem));
}

/**
 * @brief Verify a queue instance in execd
 *
 * Verify a single queue instance, that has been sent by qmaster to execd
 * as part of a job start order.
 *
 * @param qep the queue instance to verify, may be an empty handle
 * @param answer_list answer list to pass back error messages
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: qinstance_verify() is MT safe
 */
bool qinstance_verify(const ocs::tom::ConstElement<ocs::tom::QU> qep, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!qep) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      ret = false;
   }

   if (ret) {
      ret = verify_host_name(answer_list, ocs::tom::get<QU_qhostname>(qep));
   }

   if (ret) {
      if (verify_obj_name(answer_list, ocs::tom::get<QU_qname>(qep),
                          MAX_VERIFY_STRING, lNm2Str(QU_qname)) != STATUS_OK) {
         ret = false;
      }
   }

   if (ret) {
      ret = qinstance_verify_full_name(answer_list, ocs::tom::get<QU_full_name>(qep));
   }

   if (ret) {
      ret = path_verify(ocs::tom::get<QU_shell>(qep), answer_list, "shell", true);
   }

   DRETURN(ret);
}

/**
 * @brief Verify a queue instance full name
 *
 * Verifies, if a queue instance full name is correct (form `cqueue@host`).
 *
 * @param answer_list answer list to pass back error messages
 * @param full_name the queue instance name to verify
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: qinstance_verify_full_name() is MT safe
 */
bool
qinstance_verify_full_name(lList **answer_list, const char *full_name) {
   bool ret = true;

   dstring cqueue_name = DSTRING_INIT;
   dstring host_domain = DSTRING_INIT;
   bool has_hostname, has_domain;

   if (full_name == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_QINSTANCE_NAME_S, "<null>");
      ret = false;
   }

   /* split the queue instance name and verify its components */
   if (ret) {
      if (!cqueue_name_split(full_name, &cqueue_name, &host_domain,
                             &has_hostname, &has_domain)) {
         /* report syntax error */
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_INVALID_QINSTANCE_NAME_S, full_name);
         ret = false;
      }
   }

   /* the cqueue name */
   if (ret) {
      if (verify_obj_name(answer_list, sge_dstring_get_string(&cqueue_name),
                          MAX_VERIFY_STRING, "cluster queue") != STATUS_OK) {
         ret = false;
      }
   }

   /* the hostname or host group */
   if (ret) {
      if (has_hostname) {
         ret = verify_host_name(answer_list, sge_dstring_get_string(&host_domain));
      } else if (has_domain) {
         if (verify_obj_name(answer_list, sge_dstring_get_string(&host_domain) + 1,
                             MAX_VERIFY_STRING, "host domain") != STATUS_OK) {
            ret = false;
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_INVALID_QINSTANCE_NAME_S, full_name);
         ret = false;
      }
   }

   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_domain);

   return ret;
}

/**
 * @brief Set/unset qinstance into state
 *
 * Sets or Unsets a qinstance into error state and adds or removes the given
 * error message
 *
 * @param qinstance qinstance object (QU_Type)
 * @param type new state
 * @param message error message to set
 * @param set_error set or unset
 *
 * @note MT-NOTE: qinstance_set_error() is MT safe
 */
void qinstance_set_error(const ocs::tom::Element<ocs::tom::QU> qinstance, const uint32_t type, const char *message,
                         const bool set_error) {
   qinstance_set_state(qinstance, set_error, type);
   if (set_error) {
      qinstance_message_add(qinstance, type, message);
   } else {
      qinstance_message_trash_all_of_type_X(qinstance, type);
   }
}

/**
 * @brief qinstance_set_error() for an untyped caller
 *
 * @param qinstance qinstance object (QU_Type), checked
 * @param type new state
 * @param message error message to set
 * @param set_error set or unset
 */
void qinstance_set_error(lListElem *qinstance, const uint32_t type, const char *message, const bool set_error) {
   qinstance_set_error(ocs::tom::Element<ocs::tom::QU>(qinstance), type, message, set_error);
}
