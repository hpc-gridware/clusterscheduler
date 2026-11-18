#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief The cluster queue attributes as a compile-time table
 *
 * A cluster queue stores most attributes as a list of host specific values (a per host attribute list, see
 * sge_attr.h); the queue instance derived for one host stores the single value that applies to it. Every entry
 * of #ocs_cqueue_attributes describes both ends of that mapping for one attribute, plus how a new value is
 * validated. The entries are of different types, each carrying its fields as template arguments, so a walk
 * over the table (ocs_for_each_cqueue_attr()) is a fold whose body is compiled per entry with that entry's
 * field types; no field number is looked up at run time.
 *
 * The order of the entries is the order in which the attributes are verified, applied and reported.
 */

#include <cstring>
#include <tuple>
#include <type_traits>

#include "uti/sge.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_QU.h"

#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_cqueue_verify.h"

/**
 * @brief The function validating a new value of a cluster queue attribute
 *
 * Called with the cluster queue, the answer list and one entry of the attribute list; a function that checks
 * the value against defined objects gets their master list as well.
 *
 * @tparam A the attribute type of the list
 * @tparam M the object type of the master list, void for none
 */
template <OcsHostAttr A, typename M>
struct ocs_cqueue_verify_fn {
   /// the function pointer type
   using type = bool (*)(ocs::tom::ConstElement<ocs::tom::CQ>, ocs::tom::ListPtr<ocs::tom::AN> *,
                         ocs::tom::ConstElement<A>, ocs::tom::ConstList<M>);
};

/**
 * @brief The function validating a new value of a cluster queue attribute without a master list
 * @tparam A the attribute type of the list
 */
template <OcsHostAttr A>
struct ocs_cqueue_verify_fn<A, void> {
   /// the function pointer type
   using type = bool (*)(ocs::tom::ConstElement<ocs::tom::CQ>, ocs::tom::ListPtr<ocs::tom::AN> *,
                         ocs::tom::ConstElement<A>);
};

/**
 * @brief One cluster queue attribute and how it maps onto a queue instance
 *
 * The fields are template arguments: the list field of the cluster queue and the value field of the queue
 * instance. The attribute type of the list, and with it the host reference, the value and - for a list value -
 * its key, come from the list field's sub-type and #ocs_attr_traits.
 *
 * @tparam CqF the attribute in the cluster queue, holding the list of values (CQ_Type)
 * @tparam QuF the attribute in the queue instance, holding the resolved value (QU_Type)
 * @tparam M the object type of the master list the verify function checks against, void for none
 */
template <auto CqF, auto QuF, typename M = void>
struct CqueueAttr {
   /// the attribute type of the list
   using attr_type = typename ocs::tom::FieldTraits<CqF>::Sub;
   /// the object type of the master list of #verify, void for none
   using master_type = M;
   /// the field of the cluster queue
   static constexpr auto cqueue_field = CqF;
   /// the field of the queue instance
   static constexpr auto qinstance_field = QuF;
   /// the field of a list entry naming the host or host group
   static constexpr auto href_field = ocs_attr_traits<attr_type>::href;
   /// the field of a list entry holding the value
   static constexpr auto value_field = ocs_attr_traits<attr_type>::value;

   static_assert(std::is_same_v<typename ocs::tom::FieldTraits<CqF>::Type, ocs::tom::CQ>,
                 "the attribute list is a field of the cluster queue");
   static_assert(std::is_same_v<typename ocs::tom::FieldTraits<QuF>::Type, ocs::tom::QU>,
                 "the resolved value is a field of the queue instance");
   static_assert(OcsHostAttr<attr_type>, "the attribute list is a per host attribute list");

   const char *name;          ///< the attribute name an administrator writes
   bool is_sgeee_attribute;   ///< the attribute only exists in the ticket based policies
   bool verify_client;        ///< the client verifies the value too, not only qmaster
   /// validates a new value, or nullptr
   typename ocs_cqueue_verify_fn<attr_type, M>::type verify;
};

/* *INDENT-OFF* */

/// Every cluster queue attribute, in the order in which they are verified, applied and reported
inline constexpr std::tuple ocs_cqueue_attributes{
   CqueueAttr<CQ_seq_no,                 QU_seq_no>{SGE_ATTR_SEQ_NO, false, false, nullptr},
   CqueueAttr<CQ_nsuspend,               QU_nsuspend>{SGE_ATTR_NSUSPEND, false, false, nullptr},
   CqueueAttr<CQ_job_slots,              QU_job_slots>{SGE_ATTR_SLOTS, false, false, cqueue_verify_job_slots},

   CqueueAttr<CQ_tmpdir,                 QU_tmpdir>{SGE_ATTR_TMPDIR, false, false, nullptr},
   CqueueAttr<CQ_shell,                  QU_shell>{SGE_ATTR_SHELL, false, true, cqueue_verify_shell},
   CqueueAttr<CQ_calendar,               QU_calendar, ocs::tom::CAL>{SGE_ATTR_CALENDAR, false, false,
                                                                     cqueue_verify_calendar},
   CqueueAttr<CQ_priority,               QU_priority>{SGE_ATTR_PRIORITY, false, true, cqueue_verify_priority},
   CqueueAttr<CQ_processors,             QU_processors>{SGE_ATTR_PROCESSORS, false, true, cqueue_verify_processors},
   CqueueAttr<CQ_prolog,                 QU_prolog>{SGE_ATTR_PROLOG, false, false, nullptr},
   CqueueAttr<CQ_epilog,                 QU_epilog>{SGE_ATTR_EPILOG, false, false, nullptr},
   CqueueAttr<CQ_shell_start_mode,       QU_shell_start_mode>{SGE_ATTR_SHELL_START_MODE, false, true,
                                                              cqueue_verify_shell_start_mode},
   CqueueAttr<CQ_starter_method,         QU_starter_method>{SGE_ATTR_STARTER_METHOD, false, false, nullptr},
   CqueueAttr<CQ_suspend_method,         QU_suspend_method>{SGE_ATTR_SUSPEND_METHOD, false, false, nullptr},
   CqueueAttr<CQ_resume_method,          QU_resume_method>{SGE_ATTR_RESUME_METHOD, false, false, nullptr},
   CqueueAttr<CQ_terminate_method,       QU_terminate_method>{SGE_ATTR_TERMINATE_METHOD, false, false, nullptr},
   CqueueAttr<CQ_initial_state,          QU_initial_state>{SGE_ATTR_INITIAL_STATE, false, true,
                                                           cqueue_verify_initial_state},

   CqueueAttr<CQ_rerun,                  QU_rerun>{SGE_ATTR_RERUN, false, false, nullptr},

   CqueueAttr<CQ_s_fsize,                QU_s_fsize>{SGE_ATTR_S_FSIZE, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_fsize,                QU_h_fsize>{SGE_ATTR_H_FSIZE, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_s_data,                 QU_s_data>{SGE_ATTR_S_DATA, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_data,                 QU_h_data>{SGE_ATTR_H_DATA, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_s_stack,                QU_s_stack>{SGE_ATTR_S_STACK, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_stack,                QU_h_stack>{SGE_ATTR_H_STACK, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_s_core,                 QU_s_core>{SGE_ATTR_S_CORE, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_core,                 QU_h_core>{SGE_ATTR_H_CORE, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_s_rss,                  QU_s_rss>{SGE_ATTR_S_RSS, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_rss,                  QU_h_rss>{SGE_ATTR_H_RSS, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_s_vmem,                 QU_s_vmem>{SGE_ATTR_S_VMEM, false, false, cqueue_verify_memory_value},
   CqueueAttr<CQ_h_vmem,                 QU_h_vmem>{SGE_ATTR_H_VMEM, false, false, cqueue_verify_memory_value},

   CqueueAttr<CQ_s_rt,                   QU_s_rt>{SGE_ATTR_S_RT, false, false, cqueue_verify_time_value},
   CqueueAttr<CQ_h_rt,                   QU_h_rt>{SGE_ATTR_H_RT, false, false, cqueue_verify_time_value},
   CqueueAttr<CQ_s_cpu,                  QU_s_cpu>{SGE_ATTR_S_CPU, false, false, cqueue_verify_time_value},
   CqueueAttr<CQ_h_cpu,                  QU_h_cpu>{SGE_ATTR_H_CPU, false, false, cqueue_verify_time_value},

   CqueueAttr<CQ_suspend_interval,       QU_suspend_interval>{SGE_ATTR_SUSPEND_INTERVAL, false, false, nullptr},
   CqueueAttr<CQ_min_cpu_interval,       QU_min_cpu_interval>{SGE_ATTR_MIN_CPU_INTERVAL, false, false, nullptr},
   CqueueAttr<CQ_notify,                 QU_notify>{SGE_ATTR_NOTIFY, false, false, nullptr},

   CqueueAttr<CQ_qtype,                  QU_qtype>{SGE_ATTR_QTYPE, false, false, nullptr},

   CqueueAttr<CQ_ckpt_list,              QU_ckpt_list, ocs::tom::CK>{SGE_ATTR_CKPT_LIST, false, false,
                                                                     cqueue_verify_ckpt_list},
   CqueueAttr<CQ_pe_list,                QU_pe_list, ocs::tom::PE>{SGE_ATTR_PE_LIST, false, false,
                                                                   cqueue_verify_pe_list},

   CqueueAttr<CQ_owner_list,             QU_owner_list>{SGE_ATTR_OWNER_LIST, false, false, nullptr},
   CqueueAttr<CQ_acl,                    QU_acl, ocs::tom::US>{SGE_ATTR_USER_LISTS, false, false,
                                                               cqueue_verify_user_list},
   CqueueAttr<CQ_xacl,                   QU_xacl, ocs::tom::US>{SGE_ATTR_XUSER_LISTS, false, false,
                                                                cqueue_verify_user_list},

   CqueueAttr<CQ_projects,               QU_projects, ocs::tom::PR>{SGE_ATTR_PROJECTS, true, false,
                                                                    cqueue_verify_project_list},
   CqueueAttr<CQ_xprojects,              QU_xprojects, ocs::tom::PR>{SGE_ATTR_XPROJECTS, true, false,
                                                                     cqueue_verify_project_list},

   CqueueAttr<CQ_consumable_config_list, QU_consumable_config_list, ocs::tom::CE>{SGE_ATTR_COMPLEX_VALUES, false,
                                                                                 false,
                                                                                 cqueue_verify_consumable_config_list},
   CqueueAttr<CQ_load_thresholds,        QU_load_thresholds>{SGE_ATTR_LOAD_THRESHOLD, false, false, nullptr},
   CqueueAttr<CQ_suspend_thresholds,     QU_suspend_thresholds>{SGE_ATTR_SUSPEND_THRESHOLD, false, false, nullptr},

   CqueueAttr<CQ_subordinate_list,       QU_subordinate_list, ocs::tom::CQ>{SGE_ATTR_SUBORDINATE_LIST, false, false,
                                                                            cqueue_verify_subordinate_list},
};

/* *INDENT-ON* */

/**
 * @brief The type of the entry the fold passes to its callable, for its compile-time members
 *
 * Inside a generic lambda `[](const auto &attr)` the entry's fields are constants of its type, not of the
 * parameter: write `ocs_cqueue_attr_t<decltype(attr)>::cqueue_field`.
 *
 * @tparam E the declared type of the lambda parameter
 */
template <typename E>
using ocs_cqueue_attr_t = std::remove_cvref_t<E>;

/**
 * @brief Call a callable with every entry of #ocs_cqueue_attributes, in table order
 *
 * A fold over the table: the callable, usually a generic lambda, is instantiated once per entry with that
 * entry's type. Keep its body short and call shared code for what does not depend on the entry's fields.
 *
 * @tparam F the callable, taking `const CqueueAttr<...> &`
 * @param f the callable
 */
template <typename F>
void ocs_for_each_cqueue_attr(F &&f) {
   std::apply([&f](const auto &...attr) { (f(attr), ...); }, ocs_cqueue_attributes);
}

/**
 * @brief Call a callable with the entries of #ocs_cqueue_attributes, in table order, until it returns false
 *
 * As ocs_for_each_cqueue_attr(), for a walk that stops early: the callable returns true to go on.
 *
 * @tparam F the callable, taking `const CqueueAttr<...> &` and returning bool
 * @param f the callable
 * @return true if the callable returned true for every entry
 */
template <typename F>
bool ocs_for_each_cqueue_attr_while(F &&f) {
   return std::apply([&f](const auto &...attr) { return (static_cast<bool>(f(attr)) && ...); },
                     ocs_cqueue_attributes);
}

/**
 * @brief Call a callable with the entry of #ocs_cqueue_attributes an administrator names, found at run time
 *
 * The name is compared with every entry's name; the callable is instantiated per entry, as in
 * ocs_for_each_cqueue_attr(), and called for the matching one only.
 *
 * @tparam F the callable, taking `const CqueueAttr<...> &`
 * @param name the attribute name, e.g. "slots"; may be nullptr
 * @param f the callable
 * @return true if an entry has the name and the callable was called
 */
template <typename F>
bool ocs_with_cqueue_attr(const char *name, F &&f) {
   if (name == nullptr) {
      return false;
   }
   return !ocs_for_each_cqueue_attr_while([name, &f](const auto &attr) {
      if (std::strcmp(attr.name, name) == 0) {
         f(attr);
         return false;
      }
      return true;
   });
}
