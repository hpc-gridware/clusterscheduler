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
 * @brief Queue instances: the state a queue has on one host
 */

#include <cstdio>
#include <cstring>
#include <type_traits>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/ocs_cqueue_attributes.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_conf.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_List.h"

#include "sched/sge_resource_utilization.h"
#include "sched/sge_serf.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_persistence_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "sge_qinstance_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "msg_qmaster.h"

/** @brief One row of the table driving a queue instance state change
 *
 * A `qmod` command is a transition, and each transition is described by which
 * state it looks at, what that state has to be for the command to apply, and
 * what to set it to. Adding a state means adding a row rather than another
 * branch in the command handler.
 */
typedef struct {
   uint32_t transition;   ///< The `qmod` transition this row applies to
   long state_mask;       ///< The state bit it concerns

   bool (*has_state)(ocs::tom::ConstElement<ocs::tom::QU> this_elem);   ///< Reads that state from a queue instance

   bool is;               ///< What #has_state must return for this row to apply

   /// Writes that state
   bool (*set_state)(ocs::tom::Element<ocs::tom::QU> this_elem, bool set, uint64_t gdi_session);

   bool set;              ///< What #set_state is called with
   const char *success_msg;   ///< What to report when the transition worked
} change_state_t;

static bool
qinstance_change_state_on_calender_(ocs::tom::Element<ocs::tom::QU> qi_elem, uint32_t cal_order,
                                    ocs::tom::ListPtr<ocs::tom::CQU> &&state_change_list, monitoring_t *monitor,
                                    uint64_t gdi_session);

/** @brief Resolve one cluster queue attribute down to this queue instance
 *
 * A cluster queue configures an attribute per host or host group, and a queue
 * instance has to end up with exactly one value. Where two host groups both
 * match and disagree, the instance becomes *ambiguous* rather than picking
 * one - that is what @p is_ambiguous reports.
 *
 * The fields are those of one entry of #ocs_cqueue_attributes; the attributes
 * that do more than copy the value are handled by their own branch, compiled
 * for that entry only.
 *
 * @tparam CqF the cluster queue field the value comes from (CQ)
 * @tparam QuF the queue instance field being set (QU)
 * @param this_elem the queue instance (`QU`)
 * @param answer_list receives messages for the caller
 * @param cqueue the cluster queue the value comes from
 * @param[out] matching_host_or_group receives what the value was matched by
 * @param[out] matching_group receives the host group, when it was one
 * @param[out] is_ambiguous receives whether two groups disagreed
 * @param[out] has_changed_conf_attr set to true if the configuration attribute changed
 * @param initial_modify whether the queue instance is being created
 * @param[out] need_reinitialize set to true if the instance has to be rebuilt; may be nullptr
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups, to resolve `@group`
 * @param master_cqueue_list the cluster queues
 * @param gdi_session the session the change belongs to
 * @return true on success
 */
template <auto CqF, auto QuF>
static bool
qinstance_modify_attribute(const ocs::tom::Element<ocs::tom::QU> this_elem,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const ocs::tom::ConstElement<ocs::tom::CQ> cqueue, const char **matching_host_or_group,
                           const char **matching_group, bool *is_ambiguous, bool *has_changed_conf_attr,
                           const bool initial_modify, bool *need_reinitialize, monitoring_t *monitor,
                           const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                           const ocs::tom::List<ocs::tom::CQ> master_cqueue_list, const uint64_t gdi_session) {
   DENTER(BASIS_LAYER);

   using A = typename ocs::tom::FieldTraits<CqF>::Sub;
   using V = typename ocs_attr_traits<A>::value_type;
   bool ret = true;

   if (this_elem && cqueue) {
      const char *hostname = ocs::tom::get<QU_qhostname>(this_elem);
      const auto lookup = ocs_attr_list_find(ocs::tom::get<CqF>(cqueue), answer_list, hostname,
                                             matching_host_or_group, matching_group, is_ambiguous,
                                             master_hgroup_list);
      // the value of the entry read last; a lookup that read none leaves the empty value
      V new_value{};
      if (lookup.entry) {
         new_value = ocs::tom::get<ocs_attr_traits<A>::value>(lookup.entry);
      }

      if constexpr (CqF == CQ_calendar) {
         const char *old_value = ocs::tom::get<QuF>(this_elem);

         if (old_value == nullptr || new_value == nullptr || strcmp(old_value, new_value)) {
            const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
            const auto master_calendar_list = ocs::DataStore::master_list<SGE_TYPE_CALENDAR>();
            const ocs::tom::ConstElement<ocs::tom::CAL> calendar =
                    ocs::tom::find_first<CAL_name>(master_calendar_list, new_value);

            /* check if the modification is possible or if
             * an existing AR would be violated by the modification
             */
            if (!initial_modify &&
                sge_ar_list_conflicts_with_calendar(answer_list, ocs::tom::get<QU_full_name>(this_elem),
                                                    calendar, master_ar_list)) {
               ret = false;
            } else {
               if (calendar != nullptr) {
                  qinstance_change_state_on_calendar(this_elem, calendar, monitor, gdi_session);
               } else {
                  sge_qmaster_qinstance_state_set_cal_disabled(this_elem, false, gdi_session);
                  sge_qmaster_qinstance_state_set_cal_suspended(this_elem, false, gdi_session);
                  ocs::tom::set<QU_state_changes>(this_elem, nullptr);
               }
               ocs::tom::set<QuF>(this_elem, new_value);
               *has_changed_conf_attr = true;
            }
         }
      } else if constexpr (CqF == CQ_job_slots) {
         const uint32_t old_value = ocs::tom::get<QuF>(this_elem);

         if (old_value != new_value) {
            uint32_t slots_reserved = qinstance_slots_reserved(this_elem);
            DPRINTF("reserved slots %d\n", slots_reserved);
            if (!initial_modify && new_value < slots_reserved) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_QINSTANCE_SLOTSRESERVED_USS, slots_reserved,
                                       ocs::tom::get<QU_qname>(this_elem), hostname);
               ret = false;
            } else {
               ocs::tom::set<QuF>(this_elem, new_value);
               *has_changed_conf_attr = true;
               if (need_reinitialize != nullptr) {
                  *need_reinitialize = true;
               }
            }
         }
      } else if constexpr (CqF == CQ_ckpt_list || CqF == CQ_pe_list) {
         const auto old_value = ocs::tom::get<QuF>(this_elem);

         if (object_list_has_differences(old_value, answer_list, new_value)) {
            const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

            /*
             * check if the modification is possible or if
             * an existing AR violates that modification
             */
            bool conflict;
            if constexpr (CqF == CQ_ckpt_list) {
               conflict = ar_list_has_reservation_due_to_ckpt(master_ar_list, answer_list,
                                                              ocs::tom::get<QU_full_name>(this_elem), new_value);
            } else {
               conflict = !initial_modify &&
                          ar_list_has_reservation_due_to_pe(master_ar_list, answer_list,
                                                            ocs::tom::get<QU_full_name>(this_elem), new_value);
            }
            if (conflict) {
               ret = false;
            } else {
               ocs::tom::set<QuF>(this_elem, new_value.clone(""));
               *has_changed_conf_attr = true;
            }
         }
      } else if constexpr (CqF == CQ_consumable_config_list) {
         const auto old_value = ocs::tom::get<QuF>(this_elem);
         const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
         const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
         // the list of the cluster queue is completed in place, as it always was: the request data is filled in,
         // and an implicit slots entry is added for the comparison and removed again
         ocs::tom::List<ocs::tom::CE> value_list =
            ocs::tom::List<ocs::tom::CE>::from_typed(const_cast<lList *>(new_value.raw()));
         ocs::tom::ListPtr<ocs::tom::CE> created_value;

         if (centry_list_fill_request(value_list, answer_list, master_centry_list, true, true, false) == 0) {
            ocs::tom::Element<ocs::tom::CE> slots_ce = nullptr;

            /* implicit slots entry */
            if (!value_list.find_first<CE_name>(SGE_ATTR_SLOTS)) {
               const auto slots_lookup = ocs_attr_list_find(ocs::tom::get<CQ_job_slots>(cqueue), answer_list,
                                                            hostname, matching_host_or_group, matching_group,
                                                            is_ambiguous, master_hgroup_list);
               uint32_t slots_value = 0;
               dstring buffer = DSTRING_INIT;

               if (slots_lookup.entry) {
                  slots_value = ocs::tom::get<AULNG_value>(slots_lookup.entry);
               }
               sge_dstring_sprintf(&buffer, sge_u32, slots_value);

               if (!value_list) {
                  created_value = ocs::tom::ListPtr<ocs::tom::CE>::create("");
                  value_list = *created_value;
               }
               slots_ce = value_list.emplace_back<CE_name>(SGE_ATTR_SLOTS);
               ocs::tom::set<CE_doubleval>(slots_ce, slots_value);
               ocs::tom::set<CE_stringval>(slots_ce, sge_dstring_get_string(&buffer));
               sge_dstring_free(&buffer);
            }

            if (object_list_has_differences(old_value, answer_list, value_list)) {
               if (!initial_modify &&
                   ar_list_has_reservation_due_to_qinstance_complex_attr(master_ar_list, answer_list,
                                                                         this_elem, master_centry_list)) {
                  ret = false;
               } else {
                  if (need_reinitialize != nullptr) {
                     *need_reinitialize = true;
                  }

                  ocs::tom::set<QuF>(this_elem, value_list.clone(""));
                  *has_changed_conf_attr = true;
               }
            }
            if (slots_ce) {
               value_list.erase(slots_ce);
            }
         } else {
            ret = false;
         }
         // a list created for the implicit slots entry is freed with created_value
      } else if constexpr (CqF == CQ_subordinate_list) {
         const auto old_value = ocs::tom::get<QuF>(this_elem);

         if (object_list_has_differences(old_value, answer_list, new_value)) {
            ocs::tom::ListPtr<ocs::tom::SO> unsuspended_so;
            ocs::tom::ListPtr<ocs::tom::SO> suspended_so;
            bool valid_config = false;

            /*
             * check slotwise subordinations
             */
            /*
             * Check if there are loops in the slotwise subordination tree.
             */
            {
               auto old_value_copy = old_value.clone("copy_old");
               auto new_value_copy = new_value.clone("copy_new");

               /*
                * Find all queues that were slotwise subordinated before
                * and are no longer subordinated now and trigger them.
                */
               old_value_copy.get().template remove_common<SO_name>(new_value_copy.get());
               valid_config = check_new_slotwise_subordinate_tree(this_elem, new_value_copy.get(), answer_list);
            }

            if (!valid_config) {
               ret = false;
            } else {
               /*
                * Detect queues that are no longer subordinated to this queue and
                * trigger recalculation for them.
                */
               const ocs::tom::ConstElement<ocs::tom::SO> first_old_elem = old_value.front();
               const ocs::tom::ConstElement<ocs::tom::SO> first_new_elem = new_value.front();

               if (first_old_elem != nullptr && ocs::tom::get<SO_slots_sum>(first_old_elem) > 0 &&
                   first_new_elem != nullptr && ocs::tom::get<SO_slots_sum>(first_new_elem) == 0) {
                  /*
                   * If there was slotwise preemption configured before and there is
                   * either no subordination or queue wise subordination configured
                   * now, unsuspend all tasks. Queue wise subordination will be
                   * calculated in the "classic"  section below.
                   */
                  unsuspend_all_tasks_in_slotwise_sub_tree(this_elem, monitor);
               } else {
                  /*
                   * If there was slotwise preemption configured and is still slotwise
                   * preemption configured, unsuspend all task in the dechained queues.
                   * New suspension will be calculated in cqueue_success().
                   */

                  /*
                   * Find all queues that were slotwise subordinated before,
                   * are still slotwise subordinated but have different
                   * seq_no now.
                   * TODO: HP: Make sure these two lDiffListUlong() always
                   *           return the results we expect!
                   */
                  if (do_slotwise_subordinate_lists_differ(old_value, new_value)) {
                     /*
                      * unsuspend all tasks in the whole subtree, new suspends will
                      * be calculated in cqueue_success().
                      */
                     unsuspend_all_tasks_in_slotwise_sub_tree(this_elem, monitor);
                  }
               }

               /*
                * check "classic" queuewise subordinations
                */

               /*
                * Find list of subordinates that are suspended currently
                *
                * This queue can't have any running jobs and thus can't
                * subordinate anything if the queue was freshly added
                */
               if (!initial_modify) {
                  unsuspended_so = qinstance_find_suspended_subordinates(this_elem, answer_list, master_cqueue_list);
               }

               /*
                * Modify sublist
                */
               ocs::tom::set<QuF>(this_elem, new_value.clone(""));
               *has_changed_conf_attr = true;

               if (!initial_modify) {
                  /*
                   * Find list of subordinates that have to be suspended after
                   * the modification of CQ_subordinate_list-sublist
                   */
                  suspended_so = qinstance_find_suspended_subordinates(this_elem, answer_list, master_cqueue_list);

                  /*
                   * Remove equal entries in both lists
                   * (unlike lDiffListStr() an emptied list stays allocated, which makes no difference below)
                   */
                  suspended_so.get().remove_common<SO_name>(unsuspended_so.get());

                  /*
                   * (Un)suspend subordinated queue instances
                   */
                  cqueue_list_x_on_subordinate_so(master_cqueue_list, answer_list, false, unsuspended_so.get(),
                                                  monitor, gdi_session);
                  cqueue_list_x_on_subordinate_so(master_cqueue_list, answer_list, true, suspended_so.get(),
                                                  monitor, gdi_session);
               }
            }
         }
      } else if constexpr (std::is_same_v<V, const char *>) {
         // the string, time, memory and interval attributes
         const char *old_value = ocs::tom::get<QuF>(this_elem);

         if (old_value == nullptr || new_value == nullptr || strcmp(old_value, new_value)) {
            if constexpr (QuF == QU_suspend_interval) {
               if (new_value != nullptr) {
                  uint32_t interval;

                  parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, new_value, nullptr, 0);
                  if (interval == 0) {
                     /*
                      * Suspend Threshold state will be reset later
                      */
                     ocs::tom::set<QU_gdi_do_later>(this_elem, GDI_DO_LATER);
                  }
               }
            }
            ocs::tom::set<QuF>(this_elem, new_value);
            *has_changed_conf_attr = true;
         }
      } else if constexpr (std::is_same_v<V, uint32_t> || std::is_same_v<V, bool>) {
         // the numbers, the queue type and the booleans
         const V old_value = ocs::tom::get<QuF>(this_elem);

         if (old_value != new_value) {
            ocs::tom::set<QuF>(this_elem, new_value);
            *has_changed_conf_attr = true;
            if constexpr (QuF == QU_nsuspend) {
               if (new_value == 0) {
                  /*
                   * Suspend Threshold state will be reset later
                   */
                  ocs::tom::set<QU_gdi_do_later>(this_elem, GDI_DO_LATER);
               }
            }
         }
      } else {
         // the user set, project and threshold lists
         const auto old_value = ocs::tom::get<QuF>(this_elem);

         if (object_list_has_differences(old_value, answer_list, new_value)) {
            if constexpr (QuF == QU_suspend_thresholds) {
               /*
                * Suspend Threshold state will be reset later
                */
               ocs::tom::set<QU_gdi_do_later>(this_elem, GDI_DO_LATER);
            }
            ocs::tom::set<QuF>(this_elem, new_value.clone(""));
            *has_changed_conf_attr = true;
         }
      }
   }
   DRETURN(ret);
}

/** @brief Resolve the cluster queue attributes down to a queue instance
 *
 * Walks #ocs_cqueue_attributes and resolves every attribute the reduced
 * element holds, or every attribute if @p all_attributes is set, with
 * qinstance_modify_attribute().
 *
 * Creating an instance (@p is_create) resolves every attribute even after
 * one failed. Modifying one stops at the first attribute that failed, and
 * adds a message of type QI_AMBIGUOUS for each attribute that is ambiguous.
 *
 * @param this_elem the queue instance (`QU`)
 * @param answer_list receives messages for the caller
 * @param cqueue the cluster queue the values come from
 * @param reduced_elem the reduced element the client sent; it may be of another type (a host group from
 *        hgroup_mod()), then it holds no cluster queue attribute; nullptr with @p all_attributes
 * @param all_attributes resolve every attribute, not only those @p reduced_elem holds
 * @param is_create whether the queue instance is being created, see above
 * @param initial_modify whether the queue instance is new, so nothing can conflict with running jobs or
 *        advance reservations
 * @param[in,out] is_ambiguous set to true if an attribute is ambiguous, left as it is otherwise
 * @param[in,out] has_changed_conf_attr set to true if a configuration attribute changed, left as it is otherwise
 * @param[out] need_reinitialize set to true if the instance has to be rebuilt; may be nullptr
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups, to resolve `@group`
 * @param master_cqueue_list the cluster queues
 * @param gdi_session the session the change belongs to
 * @return true on success; on creation, whether the last attribute succeeded is not reported
 */
bool
qinstance_modify_attributes(const ocs::tom::Element<ocs::tom::QU> this_elem,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const ocs::tom::ConstElement<ocs::tom::CQ> cqueue, const lListElem *reduced_elem,
                            const bool all_attributes, const bool is_create, const bool initial_modify,
                            bool *is_ambiguous, bool *has_changed_conf_attr, bool *need_reinitialize,
                            monitoring_t *monitor, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                            const ocs::tom::List<ocs::tom::CQ> master_cqueue_list, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   ocs_for_each_cqueue_attr_while([&](const auto &attr) {
      using E = ocs_cqueue_attr_t<decltype(attr)>;

      // has() of the raw element is false for an element of another type
      if (all_attributes || ocs::tom::has<E::cqueue_field>(reduced_elem)) {
         const char *matching_host_or_group = nullptr;
         const char *matching_group = nullptr;
         bool tmp_is_ambiguous = false;

         const bool attr_ret = qinstance_modify_attribute<E::cqueue_field, E::qinstance_field>(
            this_elem, answer_list, cqueue, &matching_host_or_group, &matching_group, &tmp_is_ambiguous,
            has_changed_conf_attr, initial_modify, need_reinitialize, monitor, master_hgroup_list,
            master_cqueue_list, gdi_session);

         if (!is_create) {
            ret &= attr_ret;
            if (tmp_is_ambiguous) {
               /*
                * Add a message which explains the reason for
                * ambiguous state
                */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_HASAMBVAL_SSS, attr.name, matching_host_or_group,
                        matching_group);
               qinstance_message_add(this_elem, QI_AMBIGUOUS, SGE_EVENT);
            }
         }
         *is_ambiguous |= tmp_is_ambiguous;
      }
      return ret;
   });
   DRETURN(ret);
}

/** @brief Apply a `qmod` command to one queue instance
 *
 * Walks the #change_state_t table to find the row for this transition, checks
 * that the caller is allowed to make it, and reports what happened.
 *
 * @param this_elem the queue instance (`QU`)
 * @param answer_list receives messages for the caller
 * @param transition which `qmod` transition was asked for
 * @param force_transition whether to force it through
 * @param user the requesting user
 * @param host the requesting host
 * @param is_operator whether the user is an operator
 * @param is_owner whether the user owns the queue
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 * @return true when the transition was made
 */
bool
qinstance_change_state_on_command(const ocs::tom::Element<ocs::tom::QU> this_elem,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  const uint32_t transition, const bool force_transition, const char *user,
                                  const char *host, const bool is_operator, const bool is_owner,
                                  monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;
   dstring buffer = DSTRING_INIT;
   const char *qinstance_name = qinstance_get_name(this_elem, &buffer);
   change_state_t transitions[] = {
           {QI_DO_CLEARERROR, ~QI_ERROR, qinstance_state_is_error, true, sge_qmaster_qinstance_state_set_error, false},
           {QI_DO_ENABLE, ~QI_DISABLED, qinstance_state_is_manual_disabled, true,
            sge_qmaster_qinstance_state_set_manual_disabled, false},
           {QI_DO_DISABLE, QI_DISABLED, qinstance_state_is_manual_disabled, false,
            sge_qmaster_qinstance_state_set_manual_disabled, true},
           {QI_DO_SUSPEND, QI_SUSPENDED, qinstance_state_is_manual_suspended, false,
            sge_qmaster_qinstance_state_set_manual_suspended, true},
           {QI_DO_UNSUSPEND, ~QI_SUSPENDED, qinstance_state_is_manual_suspended, true,
            sge_qmaster_qinstance_state_set_manual_suspended, false},
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
           { QI_DO_SETERROR,       QI_ERROR,      qinstance_state_is_error,            false, sge_qmaster_qinstance_state_set_error,            true},
           { QI_DO_SETORPHANED,    QI_ORPHANED,   qinstance_state_is_orphaned,         false, sge_qmaster_qinstance_state_set_orphaned,         true},
           { QI_DO_CLEARORPHANED,  ~QI_ORPHANED,  qinstance_state_is_orphaned,         true,  sge_qmaster_qinstance_state_set_orphaned,         false},
           { QI_DO_SETUNKNOWN,     QI_UNKNOWN,    qinstance_state_is_unknown,          false, sge_qmaster_qinstance_state_set_unknown,          true},
           { QI_DO_CLEARUNKNOWN,   ~QI_UNKNOWN,   qinstance_state_is_unknown,          true,  sge_qmaster_qinstance_state_set_unknown,          false},
           { QI_DO_SETAMBIGUOUS,   QI_AMBIGUOUS,  qinstance_state_is_ambiguous,        false, sge_qmaster_qinstance_state_set_ambiguous,        true},
           { QI_DO_CLEARAMBIGUOUS, ~QI_AMBIGUOUS, qinstance_state_is_ambiguous,        true,  sge_qmaster_qinstance_state_set_ambiguous,        false},
#endif
           {QI_DO_NOTHING, 0, nullptr, true, nullptr, true}
   };

   if (is_owner || is_operator) {
      int i = 0;

      while (transitions[i].transition != QI_DO_NOTHING) {
         if (transitions[i].transition == transition) {
            break;
         }
         i++;
      }

      /*
       * Verify current state
       */
      if (transitions[i].has_state(this_elem) == transitions[i].is ||
          force_transition) {
         bool did_something = false;

         DTRACE;

         /*
          * Some transitions need extra work
          */
         switch (transition) {
            case QI_DO_SUSPEND :
               if ((!qinstance_state_is_susp_on_sub(this_elem) &&
                    !qinstance_state_is_cal_suspended(this_elem)) ||
                   force_transition) {
                  sge_signal_queue(SGE_SIGSTOP, this_elem, ocs::tom::Element<ocs::tom::JB>(),
                                   ocs::tom::Element<ocs::tom::JAT>(), monitor);
                  did_something = true;
               }
               break;
            case QI_DO_UNSUSPEND :
               if (!qinstance_state_is_susp_on_sub(this_elem) &&
                   !qinstance_state_is_cal_suspended(this_elem)) {
                  sge_signal_queue(SGE_SIGCONT, this_elem, ocs::tom::Element<ocs::tom::JB>(),
                                   ocs::tom::Element<ocs::tom::JAT>(), monitor);
                  did_something = true;
               }
               break;
            case QI_DO_CLEARERROR :
               qinstance_message_trash_all_of_type_X(this_elem, QI_ERROR);
               did_something = true;
               break;
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
               case QI_DO_SETERROR :
                    qinstance_message_add(this_elem, QI_ERROR, "this is a debug message\n");
                    did_something = true;
                  break;
#endif
            default:
               did_something = true;
         }

         /*
          * Change state
          */
         if (did_something) {
            transitions[i].set_state(this_elem, transitions[i].set, gdi_session);
         }

         /*
          * Make changes persistent
          */
         if (did_something) {
            qinstance_increase_qversion(this_elem);
            ret &= sge_event_spool(answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, ocs::tom::get<QU_qname>(this_elem),
                                   ocs::tom::get<QU_qhostname>(this_elem), nullptr, this_elem, nullptr, nullptr, true, true, gdi_session);

            if (ret) {
               if (force_transition) {
                  INFO(MSG_QINSTANCE_FORCEDSTATE_SSSS, user, host, qinstance_name, qinstance_state_as_string( transitions[i].state_mask));
               } else {
                  INFO(MSG_QINSTANCE_CHANGEDST_SSSS, user, host, qinstance_name, qinstance_state_as_string( transitions[i].state_mask));
               }
               answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
            } else {
               ERROR(MSG_QINSTANCE_STATENOTMOD_S, qinstance_name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

               /*
                * Rollback
                */
               if (!force_transition) {
                  transitions[i].set_state(this_elem, transitions[i].set ? false : true, gdi_session);
               }
            }
         }
      } else {
         INFO(MSG_QINSTANCE_HASSTATE_SS, qinstance_name, qinstance_state_as_string(transitions[i].state_mask));
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
      }
   } else {
      WARNING(MSG_QINSTANCE_STATENOTMODPERM_S, qinstance_name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
   }
   sge_dstring_free(&buffer);
   DRETURN(ret);
}


/**
 * @brief - changes the state of a given qi (wraper)
 *
 * Changes the state of a given qi based on its calendar.
 *
 * @param this_elem quinstance
 * @param calendar calendar
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 *
 * @return state got changed or not
 *
 * @note MT-NOTE: qinstance_change_state_on_calendar() is MT safe
 */
bool
qinstance_change_state_on_calendar(const ocs::tom::Element<ocs::tom::QU> this_elem,
                                   const ocs::tom::ConstElement<ocs::tom::CAL> calendar, monitoring_t *monitor,
                                   const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && calendar != nullptr) {
      auto [state, when, state_changes_list] = calender_state_changes(calendar, nullptr);

      ret = qinstance_change_state_on_calender_(this_elem, state, std::move(state_changes_list), monitor,
                                                gdi_session);

      if (when != 0) {
         /* Arm the timer for the next state change of the calendar. A calendar gets a timer of its
            own whenever it is added or modified and whenever one of its timed events is delivered,
            but a queue instance that just got the calendar assigned would otherwise keep the state
            computed above until something else re-evaluates the calendar. */
         const char *cal_name = ocs::tom::get<CAL_name>(calendar);

         DPRINTF("queue %s: calendar %s, next state change at " sge_u64 "\n",
                 ocs::tom::get<QU_full_name>(this_elem), cal_name, when);
         calendar_arm_timer(cal_name, when);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Changes the state of all qis (wraper)
 *
 * @param cal_name calendar name
 * @param cal_order calendar state (todo)
 * @param state_change_list state list for the qis
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 *
 * @return true, if it worked
 *
 * @note MT-NOTE: qinstance_change_state_on_calendar_all() is not MT safe
 *       Directly access the cluster queue list
 */
bool
qinstance_change_state_on_calendar_all(const char *cal_name, const uint32_t cal_order,
                                       const ocs::tom::ConstList<ocs::tom::CQU> state_change_list,
                                       monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   bool ret = true;
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   for (const auto cqueue : master_cqueue_list) {
      for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cqueue)) {
         const char *queue_calendar = ocs::tom::get<QU_calendar>(qinstance);

         if (queue_calendar != nullptr && !strcmp(queue_calendar, cal_name)) {
            ret = qinstance_change_state_on_calender_(qinstance, cal_order, state_change_list.clone("state list"),
                                                      monitor, gdi_session);
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Changes qi state based on calendar
 *
 * @param this_elem qi
 * @param cal_order next state (order)
 * @param state_change_list qi state list, stored in the queue instance (always consumed)
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 *
 * @return true, if it worked
 *
 * @note MT-NOTE: qinstance_change_state_on_calender_() is MT safe
 */
static bool qinstance_change_state_on_calender_(const ocs::tom::Element<ocs::tom::QU> this_elem,
                                                const uint32_t cal_order,
                                                ocs::tom::ListPtr<ocs::tom::CQU> &&state_change_list,
                                                monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::CQU> state_changes = std::move(state_change_list);
   bool ret = true;
   bool old_cal_disabled = qinstance_state_is_cal_disabled(this_elem);
   bool old_cal_suspended = qinstance_state_is_cal_suspended(this_elem);
   bool new_cal_disabled = (cal_order == QI_DO_CAL_DISABLE) ? true : false;
   bool new_cal_suspended = (cal_order == QI_DO_CAL_SUSPEND) ? true : false;

   ocs::tom::set<QU_state_changes>(this_elem, std::move(state_changes));

   if (old_cal_disabled != new_cal_disabled || old_cal_suspended != new_cal_suspended) {
      // together with the calendar timer messages this shows whether a state change that the
      // calendar announced has really reached the queue instance
      INFO(MSG_CALENDAR_QINSTANCESTATE_SU, ocs::tom::get<QU_full_name>(this_elem), cal_order);
   }

   if (old_cal_disabled != new_cal_disabled) {
      sge_qmaster_qinstance_state_set_cal_disabled(this_elem, new_cal_disabled, gdi_session);
   }

   if (old_cal_suspended != new_cal_suspended) {
      const char *name = ocs::tom::get<QU_full_name>(this_elem);

      sge_qmaster_qinstance_state_set_cal_suspended(this_elem, new_cal_suspended, gdi_session);
      if (new_cal_suspended) {
         if (qinstance_state_is_susp_on_sub(this_elem)) {
            INFO(MSG_QINSTANCE_NOUSSOS_S, name);
         } else if (qinstance_state_is_manual_suspended(this_elem)) {
            INFO(MSG_QINSTANCE_NOUSADM_S, name);
         } else {
            sge_signal_queue(SGE_SIGSTOP, this_elem, ocs::tom::Element<ocs::tom::JB>(),
                             ocs::tom::Element<ocs::tom::JAT>(), monitor);
         }
      } else {
         if (qinstance_state_is_susp_on_sub(this_elem)) {
            INFO(MSG_QINSTANCE_NOSSOS_S, name);
         } else if (qinstance_state_is_manual_suspended(this_elem)) {
            INFO(MSG_QINSTANCE_NOSADM_S, name);
         } else {
            sge_signal_queue(SGE_SIGCONT, this_elem, ocs::tom::Element<ocs::tom::JB>(),
                             ocs::tom::Element<ocs::tom::JAT>(), monitor);
         }
      }
   }

   qinstance_add_event(this_elem, sgeE_QINSTANCE_MOD, gdi_session);

   DRETURN(ret);
}

/** @brief Mark a queue instance as disabled by an administrator, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_manual_disabled(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                                const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_manual_disabled(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                  ocs::tom::get<QU_full_name>(this_elem), QI_DISABLED, set_state, gdi_session);
   }

   DRETURN(changed);
}

/** @brief Mark a queue instance as suspended by an administrator, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_manual_suspended(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                                 const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_manual_suspended(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                  ocs::tom::get<QU_full_name>(this_elem), QI_SUSPENDED, set_state, gdi_session);
   }


   DRETURN(changed);
}

/** @brief Mark a queue instance as unreachable, because its execution host stopped answering, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_unknown(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                        const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_unknown(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      if (mconf_get_simulate_execds()) {
         sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                     ocs::tom::get<QU_full_name>(this_elem), QI_UNKNOWN, false, gdi_session);
      } else {
         sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                     ocs::tom::get<QU_full_name>(this_elem), QI_UNKNOWN, set_state, gdi_session);
      }
   }


   DRETURN(changed);
}

/** @brief Mark a queue instance as in error state, because a job could not be started there, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_error(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                      const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_error(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                  ocs::tom::get<QU_full_name>(this_elem), QI_ERROR, set_state, gdi_session);
   }


   DRETURN(changed);
}

/** @brief Mark a queue instance as suspended by a subordinate relationship, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_susp_on_sub(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                            const uint64_t gid_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_susp_on_sub(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   DRETURN(changed);
}

/** @brief Mark a queue instance as disabled by its calendar, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_cal_disabled(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                             const uint64_t gid_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_cal_disabled(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   DRETURN(changed);
}

/** @brief Mark a queue instance as suspended by its calendar, or no longer so
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_cal_suspended(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                              const uint64_t gid_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_cal_suspended(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   DRETURN(changed);
}

/** @brief Mark a queue instance as orphaned - deleted but still holding jobs
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_orphaned(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                         const uint64_t gid_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_orphaned(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   DRETURN(changed);
}

/** @brief Mark a queue instance as ambiguously configured, so it cannot be used, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_ambiguous(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool set_state,
                                          const uint64_t gid_session) {
   DENTER(TOP_LAYER);

   bool changed;
   changed = qinstance_state_set_ambiguous(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(ocs::DataStore::master_list_rw<SGE_TYPE_AR>(),
                                  ocs::tom::get<QU_full_name>(this_elem), QI_AMBIGUOUS, set_state, gid_session);
   }


   DRETURN(changed);
}

/* ret: did the state change */
/** @brief Put a newly created queue instance into the state its configuration asks for
 *
 * A queue may be configured to start out disabled, and a calendar may already
 * apply to it, so the state is not simply "enabled" at creation.
 *
 * @param this_elem the queue instance (`QU`)
 * @param gdi_session the session the change belongs to
 *
 * @return true if the initial configuration changed the state
 */
bool
sge_qmaster_qinstance_set_initial_state(const ocs::tom::Element<ocs::tom::QU> this_elem, const uint64_t gdi_session) {
#ifdef QINSTANCE_MODIFY_DEBUG
   DENTER(TOP_LAYER);
#else
   DENTER(BASIS_LAYER);
#endif

   bool ret = false;
   const char *state_string = ocs::tom::get<QU_initial_state>(this_elem);

   if (state_string != nullptr && strcmp(state_string, "default")) {
      bool do_disable = strcmp(state_string, "disabled") == 0 ? true : false;

      if (do_disable != qinstance_state_is_manual_disabled(this_elem)) {
         ret = true;
         sge_qmaster_qinstance_state_set_manual_disabled(this_elem, do_disable, gdi_session);
      }
   }
   DRETURN(ret);
}

/**
 * @brief As it says
 *
 * Reinitialize the consumable actual values.
 *
 * @param this_elem QU element
 * @param answer_list AN element
 *
 * @return error result true  - success false - error
 *
 * @note MT-NOTE: qinstance_reinit_consumable_actual_list() is MT safe
 */
bool
qinstance_reinit_consumable_actual_list(const ocs::tom::Element<ocs::tom::QU> this_elem,
                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      const char *name = ocs::tom::get<QU_full_name>(this_elem);
      const auto job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
      const auto centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
      const auto ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
      const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();

      ocs::tom::set<QU_resource_utilization>(this_elem, nullptr);
      qinstance_set_conf_slots_used(this_elem);
      qinstance_debit_consumable(this_elem, ocs::tom::ConstElement<ocs::tom::JB>(),
                                 ocs::tom::ConstElement<ocs::tom::PE>(), centry_list, 0, true, true, nullptr);

      for (const auto ep : job_list) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(ep)) {
            ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task);
            const ocs::tom::ConstElement<ocs::tom::JG> gdil_ep = ocs::tom::find_first<JG_qname>(gdil, name);
            const ocs::tom::ConstElement<ocs::tom::PE> pe = ocs::tom::get<JAT_pe_object>(ja_task);

            if (gdil_ep != nullptr) {
               int slots = ocs::tom::get<JG_slots>(gdil_ep);

               bool is_master_task = false;
               if (gdil_ep == gdil.front()) {
                  is_master_task = true;
               }

               bool do_per_host_booking = true;
               const ocs::tom::ConstElement<ocs::tom::JG> prev = gdil_ep.prev();
               if (prev != nullptr &&
                   sge_strnullcmp(ocs::tom::get<JG_qhostname>(gdil_ep), ocs::tom::get<JG_qhostname>(prev)) == 0) {
                  do_per_host_booking = false;
               }

               if (slots > 0) {
                  qinstance_debit_consumable(this_elem, ep, pe, centry_list, slots, is_master_task,
                                             do_per_host_booking, nullptr);
               }
            }
         }
      }
      for (const auto ep : ar_list) {
         ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<AR_granted_slots>(ep);
         const ocs::tom::ConstElement<ocs::tom::JG> gdil_ep = ocs::tom::find_first<JG_qname>(gdil, name);

         if (gdil_ep != nullptr) {
            bool is_master_task = false;
            auto dummy_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();

            if (gdil_ep == gdil.front()) {
               is_master_task = true;
            }

            const char *pe_name = ocs::tom::get<AR_granted_pe>(ep);
            ocs::tom::ConstElement<ocs::tom::PE> pe = nullptr;
            if (pe_name != nullptr) {
               pe = pe_list_locate(master_pe_list, pe_name);
            }

            job_set_hard_resource_list(*dummy_job, ocs::tom::get<AR_resource_list>(ep).clone());

            rc_add_job_utilization(ocs::tom::ConstElement<ocs::tom::JG>(), *dummy_job,
                                   pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                   this_elem, centry_list,
                                   ocs::tom::get<JG_slots>(gdil_ep), name, ocs::tom::get<AR_start_time>(ep),
                                   ocs::tom::get<AR_duration>(ep), QUEUE_TAG, false, is_master_task, false,
                                   ocs::tom::ConstList<ocs::tom::GRU>());
         }
      }
   }

   DRETURN(ret);
}

