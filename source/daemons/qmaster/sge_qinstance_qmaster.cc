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
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_conf.h"

#include "sched/sge_resource_utilization.h"
#include "sched/sge_serf.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_persistence_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "sge_qinstance_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
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

   bool (*has_state)(const lListElem *this_elem);   ///< Reads that state from a queue instance

   bool is;               ///< What #has_state must return for this row to apply

   bool (*set_state)(lListElem *this_elem, bool set, uint64_t gdi_session);   ///< Writes that state

   bool set;              ///< What #set_state is called with
   const char *success_msg;   ///< What to report when the transition worked
} change_state_t;

static bool
qinstance_change_state_on_calender_(lListElem *qi_elem, uint32_t cal_order,
                                    lList **state_change_list, monitoring_t *monitor, uint64_t gdi_session);

/** @brief Resolve one cluster queue attribute down to this queue instance
 *
 * A cluster queue configures an attribute per host or host group, and a queue
 * instance has to end up with exactly one value. Where two host groups both
 * match and disagree, the instance becomes *ambiguous* rather than picking
 * one - that is what @p is_ambiguous reports.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param answer_list receives messages for the caller
 * @param cqueue the cluster queue the value comes from
 * @param attribute_name the queue instance field being set
 * @param cqueue_attibute_name the cluster queue field it comes from
 * @param sub_host_name the field holding the host name in the configured list
 * @param sub_value_name the field holding the value in that list
 * @param subsub_key the key field of a sublist value
 * @param[out] matching_host_or_group receives what the value was matched by
 * @param[out] matching_group receives the host group, when it was one
 * @param[out] is_ambiguous receives whether two groups disagreed
 * @param[out] has_changed_conf_attr receives whether a configuration attribute changed
 * @param[out] has_changed_state_attr receives whether a state attribute changed
 * @param initial_modify whether the queue instance is being created
 * @param[out] need_reinitialize receives whether the instance has to be rebuilt
 * @param monitor for monitoring qmaster threads
 * @param master_hgroup_list the host groups, to resolve `@group`
 * @param master_cqueue_list the cluster queues
 * @param gdi_session the session the change belongs to
 * @return true on success
 */
bool
qinstance_modify_attribute(lListElem *this_elem, lList **answer_list, const lListElem *cqueue,
                           int attribute_name, int cqueue_attibute_name, int sub_host_name, int sub_value_name,
                           int subsub_key, const char **matching_host_or_group, const char **matching_group,
                           bool *is_ambiguous, bool *has_changed_conf_attr, bool *has_changed_state_attr,
                           const bool initial_modify, bool *need_reinitialize, monitoring_t *monitor,
                           const lList *master_hgroup_list, lList *master_cqueue_list, uint64_t gdi_session) {
#if 0 /* EB: DEBUG: enable debugging for qinstance_modify_attribute() */
#define QINSTANCE_MODIFY_DEBUG
#endif
#ifdef QINSTANCE_MODIFY_DEBUG
   DENTER(TOP_LAYER);
#else
   DENTER(BASIS_LAYER);
#endif

   bool ret = true;
   const lList *master_calendar_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);

   if (this_elem != nullptr && cqueue != nullptr &&
       attribute_name != NoName && cqueue_attibute_name != NoName) {
      const char *hostname = lGetHost(this_elem, QU_qhostname);
      const lList *attr_list = lGetList(cqueue, cqueue_attibute_name);
      const lDescr *descr = lGetElemDescr(this_elem);
      int pos = lGetPosInDescr(descr, attribute_name);
      int type = lGetPosType(descr, pos);
      bool value_found = true;

      switch (cqueue_attibute_name) {
         case CQ_calendar: {
            const char *old_value = lGetString(this_elem, attribute_name);
            const char *new_value;

            str_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                     matching_group, is_ambiguous, master_hgroup_list);
            if (old_value == nullptr || new_value == nullptr || strcmp(old_value, new_value)) {
               const lListElem *calendar = lGetElemStr(master_calendar_list, CAL_name, new_value);

#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                        lNm2Str(attribute_name),
                        old_value ? old_value : "<null>",
                        new_value ? new_value : "<null>"));
#endif
               /* check if the modification is possible or if
                * an existing AR would be violated by the modification
                */
               if (!initial_modify &&
                   sge_ar_list_conflicts_with_calendar(answer_list, lGetString(this_elem, QU_full_name),
                                                       calendar, master_ar_list)) {
                  ret = false;
                  break;
               }

               if (calendar != nullptr) {
                  qinstance_change_state_on_calendar(this_elem, calendar, monitor, gdi_session);
               } else {
                  sge_qmaster_qinstance_state_set_cal_disabled(this_elem, false, gdi_session);
                  sge_qmaster_qinstance_state_set_cal_suspended(this_elem, false,gdi_session);
                  lSetList(this_elem, QU_state_changes, nullptr);
               }
               lSetString(this_elem, attribute_name, new_value);
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_qtype: {
            uint32_t old_value = lGetUlong(this_elem, attribute_name);
            uint32_t new_value;

            qtlist_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                        matching_group, is_ambiguous, master_hgroup_list);
            if (old_value != new_value) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF(("Changed " SFQ " from " sge_u32" to " sge_u32"\n",
                        lNm2Str(attribute_name), old_value, new_value));
#endif
               lSetUlong(this_elem, attribute_name, new_value);
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_s_fsize:
         case CQ_h_fsize:
         case CQ_s_data:
         case CQ_h_data:
         case CQ_s_stack:
         case CQ_h_stack:
         case CQ_s_core:
         case CQ_h_core:
         case CQ_s_rss:
         case CQ_h_rss:
         case CQ_s_vmem:
         case CQ_h_vmem: {
            const char *old_value = lGetString(this_elem, attribute_name);
            const char *new_value = nullptr;

            mem_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                     matching_group, is_ambiguous, master_hgroup_list);
            if (old_value == nullptr || new_value == nullptr ||
                strcmp(old_value, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                        lNm2Str(attribute_name),
                        old_value ? old_value : "<null>",
                        new_value ? new_value : "<null>"));
#endif
               lSetString(this_elem, attribute_name, new_value);
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_s_rt:
         case CQ_h_rt:
         case CQ_s_cpu:
         case CQ_h_cpu: {
            const char *old_value = lGetString(this_elem, attribute_name);
            const char *new_value = nullptr;

            time_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                      matching_group, is_ambiguous, master_hgroup_list);

            if (old_value == nullptr || new_value == nullptr ||
                strcmp(old_value, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                        lNm2Str(attribute_name),
                        old_value ? old_value : "<null>",
                        new_value ? new_value : "<null>"));
#endif
               lSetString(this_elem, attribute_name, new_value);
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_suspend_interval:
         case CQ_min_cpu_interval:
         case CQ_notify: {
            const char *old_value = lGetString(this_elem, attribute_name);
            const char *new_value = nullptr;

            inter_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                       matching_group, is_ambiguous, master_hgroup_list);
            if (old_value == nullptr || new_value == nullptr ||
                strcmp(old_value, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                        lNm2Str(attribute_name),
                        old_value ? old_value : "<null>",
                        new_value ? new_value : "<null>"));
#endif
               if (attribute_name == QU_suspend_interval &&
                   new_value != nullptr) {
                  uint32_t interval;

                  parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME,
                                  new_value, nullptr, 0);
                  if (interval == 0) {
                     /*
                      * Suspend Threshold state will be reset later
                      */
                     lSetUlong(this_elem, QU_gdi_do_later, GDI_DO_LATER);
                  }
               }

               lSetString(this_elem, attribute_name, new_value);
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_ckpt_list:
         case CQ_pe_list: {
            const lList *old_value = lGetList(this_elem, attribute_name);
            lList *new_value = nullptr;

            strlist_attr_list_find_value(attr_list, answer_list, hostname, &new_value,
                                         matching_host_or_group, matching_group, is_ambiguous, master_hgroup_list);
            if (object_list_has_differences(old_value, answer_list, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif
               /*
                * check if the modification is possible or if
                * an existing AR violates that modification
                */
               if (cqueue_attibute_name == CQ_ckpt_list &&
                   ar_list_has_reservation_due_to_ckpt(master_ar_list, answer_list, lGetString(this_elem, QU_full_name),
                                                       new_value)) {
                  ret = false;
                  break;
               } else if (!initial_modify && cqueue_attibute_name == CQ_pe_list &&
                          ar_list_has_reservation_due_to_pe(master_ar_list, answer_list,
                                                            lGetString(this_elem, QU_full_name), new_value)) {
                  ret = false;
                  break;
               }

               lSetList(this_elem, attribute_name, lCopyList("", new_value));
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_owner_list:
         case CQ_acl:
         case CQ_xacl: {
            const lList *old_value = lGetList(this_elem, attribute_name);
            lList *new_value = nullptr;

            usrlist_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                         matching_group, is_ambiguous, master_hgroup_list);
            if (object_list_has_differences(old_value, answer_list, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif
               lSetList(this_elem, attribute_name, lCopyList("", new_value));
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_projects:
         case CQ_xprojects: {
            const lList *old_value = lGetList(this_elem, attribute_name);
            lList *new_value = nullptr;

            prjlist_attr_list_find_value(attr_list, answer_list, hostname, &new_value,
                                         matching_host_or_group, matching_group, is_ambiguous, master_hgroup_list);
            if (object_list_has_differences(old_value, answer_list, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif
               lSetList(this_elem, attribute_name, lCopyList("", new_value));
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_job_slots: {
            uint32_t old_value = lGetUlong(this_elem, attribute_name);
            uint32_t new_value;

            ulng_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                      matching_group, is_ambiguous, master_hgroup_list);
            if (old_value != new_value) {
               uint32_t slots_reserved = qinstance_slots_reserved(this_elem);
               DPRINTF("reserved slots %d\n", slots_reserved);
               if (!initial_modify && new_value < slots_reserved) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_QINSTANCE_SLOTSRESERVED_USS, slots_reserved,
                                          lGetString(this_elem, QU_qname), hostname);
                  ret &= false;
               } else {
#ifdef QINSTANCE_MODIFY_DEBUG
                  DPRINTF("Changed " SFQ " from " sge_u32" to " sge_u32"\n",
                          lNm2Str(attribute_name), old_value, new_value);
#endif
                  lSetUlong(this_elem, attribute_name, new_value);
                  *has_changed_conf_attr = true;
                  if (need_reinitialize != nullptr) {
                     *need_reinitialize = true;
                  }
               }
            }
         }
            break;
         case CQ_consumable_config_list: {
            const lList *old_value = lGetList(this_elem, attribute_name);
            lList *new_value = nullptr;
            bool created_new_value = false;

            celist_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                        matching_group, is_ambiguous, master_hgroup_list);

            if (centry_list_fill_request(new_value, answer_list, master_centry_list,
                                         true, true, false) == 0) {
               lListElem *slots_ce = nullptr;

               /* implicit slots entry */
               if (lGetElemStr(new_value, CE_name, SGE_ATTR_SLOTS) == nullptr) {
                  const lList *cq_slots_attr = lGetList(cqueue, CQ_job_slots);
                  uint32_t slots_value;
                  dstring buffer = DSTRING_INIT;

                  ulng_attr_list_find_value(cq_slots_attr, answer_list, hostname, &slots_value, matching_host_or_group,
                                            matching_group, is_ambiguous, master_hgroup_list);
                  sge_dstring_sprintf(&buffer, sge_u32, slots_value);

                  if (new_value == nullptr) {
                     created_new_value = true;
                  }
                  slots_ce = lAddElemStr(&new_value, CE_name, SGE_ATTR_SLOTS, CE_Type);
                  lSetDouble(slots_ce, CE_doubleval, slots_value);
                  lSetString(slots_ce, CE_stringval, sge_dstring_get_string(&buffer));
                  sge_dstring_free(&buffer);
               }

               if (object_list_has_differences(old_value, answer_list, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
                  DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif
                  if (!initial_modify &&
                      ar_list_has_reservation_due_to_qinstance_complex_attr(master_ar_list, answer_list,
                                                                            this_elem, master_centry_list)) {
                     ret = false;
                  } else {
                     if (need_reinitialize != nullptr) {
                        *need_reinitialize = true;
                     }

                     lSetList(this_elem, attribute_name, lCopyList("", new_value));
                     *has_changed_conf_attr = true;
                  }
               }
               lRemoveElem(new_value, &slots_ce);
            } else {
               ret &= false;
            }
            if (created_new_value) {
               lFreeList(&new_value);
            }
         }
            break;
         case CQ_load_thresholds:
         case CQ_suspend_thresholds: {
            const lList *old_value = lGetList(this_elem, attribute_name);
            lList *new_value = nullptr;

            celist_attr_list_find_value(attr_list, answer_list, hostname, &new_value,
                                        matching_host_or_group, matching_group, is_ambiguous, master_hgroup_list);
            if (object_list_has_differences(old_value, answer_list, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif
               /*
                * Suspend Threshold state will be reset later
                */
               if (attribute_name == QU_suspend_thresholds) {
                  lSetUlong(this_elem, QU_gdi_do_later, GDI_DO_LATER);
               }

               lSetList(this_elem, attribute_name, lCopyList("", new_value));
               *has_changed_conf_attr = true;
            }
         }
            break;
         case CQ_subordinate_list: {
            const lList *old_value = nullptr;          /* SO_Type list */
            lList *new_value = nullptr;          /* SO_Type list */

            old_value = lGetList(this_elem, attribute_name);
            solist_attr_list_find_value(attr_list, answer_list, hostname, &new_value,
                                        matching_host_or_group, matching_group, is_ambiguous, master_hgroup_list);
            if (object_list_has_differences(old_value, answer_list, new_value)) {
               lList *unsuspended_so = nullptr;  /* SO_Type list */
               lList *suspended_so = nullptr;    /* SO_Type list */
               const lListElem *first_old_elem = nullptr;
               const lListElem *first_new_elem = nullptr;
               bool valid_config = false;

               /*
                * check slotwise subordinations
                */
               /*
                * Check if there are loops in the slotwise subordination tree.
                */
               lList *old_value_copy = nullptr;  /* SO_Type list */
               lList *new_value_copy = nullptr;  /* SO_Type list */

               old_value_copy = lCopyList("copy_old", old_value);
               new_value_copy = lCopyList("copy_new", new_value);

               /*
                * Find all queues that were slotwise subordinated before
                * and are no longer subordinated now and trigger them.
                */
               lDiffListStr(SO_name, &old_value_copy, &new_value_copy);
               valid_config = check_new_slotwise_subordinate_tree(this_elem, new_value_copy, answer_list);

               lFreeList(&old_value_copy);
               lFreeList(&new_value_copy);

               if (!valid_config) {
                  ret = false;
                  break;
               }

               /*
                * Detect queues that are no longer subordinated to this queue and
                * trigger recalculation for them.
                */
               hostname = lGetHost(this_elem, QU_qhostname);
               first_old_elem = lFirst(old_value);
               first_new_elem = lFirst(new_value);

               if (first_old_elem != nullptr && lGetUlong(first_old_elem, SO_slots_sum) > 0 &&
                   first_new_elem != nullptr && lGetUlong(first_new_elem, SO_slots_sum) == 0) {
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
#ifdef QINSTANCE_MODIFY_DEBUG
               DPRINTF("Changed " SFQ "\n", lNm2Str(attribute_name));
#endif

               /*
                * Find list of subordinates that are suspended currently
                *
                * This queue can't have any running jobs and thus can't
                * subordinate anything if the queue was freshly added
                */
               if (!initial_modify) {
                  qinstance_find_suspended_subordinates(this_elem, answer_list, &unsuspended_so, master_cqueue_list);
               }

               /*
                * Modify sublist
                */
               lSetList(this_elem, attribute_name, lCopyList("", new_value));
               *has_changed_conf_attr = true;

               if (!initial_modify) {
                  /*
                   * Find list of subordinates that have to be suspended after
                   * the modification of CQ_subordinate_list-sublist
                   */
                  qinstance_find_suspended_subordinates(this_elem, answer_list, &suspended_so, master_cqueue_list);

                  /*
                   * Remove equal entries in both lists
                   */
                  lDiffListStr(SO_name, &suspended_so, &unsuspended_so);

                  /*
                   * (Un)suspend subordinated queue instances
                   */
                  cqueue_list_x_on_subordinate_so(master_cqueue_list, answer_list, false, unsuspended_so, monitor, gdi_session);
                  cqueue_list_x_on_subordinate_so(master_cqueue_list, answer_list, true, suspended_so, monitor, gdi_session);
               }

               /*
                * Cleanup
                */
               lFreeList(&suspended_so);
               lFreeList(&unsuspended_so);
            }
         }
            break;
         default:
            value_found = false;
            break;
      }

      if (!value_found) {
         switch (type) {
            case lStringT: {
               const char *old_value = lGetString(this_elem, attribute_name);
               const char *new_value = nullptr;

               str_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                        matching_group, is_ambiguous, master_hgroup_list);
               if (old_value == nullptr || new_value == nullptr ||
                   strcmp(old_value, new_value)) {
#ifdef QINSTANCE_MODIFY_DEBUG
                  DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                           lNm2Str(attribute_name),
                           old_value ? old_value : "<null>",
                           new_value ? new_value : "<null>"));
#endif
                  lSetString(this_elem, attribute_name, new_value);
                  *has_changed_conf_attr = true;
               }
            }
               break;
            case lUlongT: {
               uint32_t old_value = lGetUlong(this_elem, attribute_name);
               uint32_t new_value;

               ulng_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                         matching_group, is_ambiguous, master_hgroup_list);
               if (old_value != new_value) {
#ifdef QINSTANCE_MODIFY_DEBUG
                  DPRINTF(("Changed " SFQ " from " sge_u32" to " sge_u32"\n", lNm2Str(attribute_name), old_value, new_value));
#endif
                  lSetUlong(this_elem, attribute_name, new_value);
                  *has_changed_conf_attr = true;
                  if (attribute_name == QU_nsuspend &&
                      new_value == 0) {
                     /*
                      * Suspend Threshold state will be reset later
                      */

                     lSetUlong(this_elem, QU_gdi_do_later, GDI_DO_LATER);
                  }
               }
            }
               break;
            case lBoolT: {
               bool old_value = lGetBool(this_elem, attribute_name) ? true : false;
               bool new_value;

               bool_attr_list_find_value(attr_list, answer_list, hostname, &new_value, matching_host_or_group,
                                         matching_group, is_ambiguous, master_hgroup_list);
               if (old_value != new_value) {
#ifdef QINSTANCE_MODIFY_DEBUG
                  DPRINTF(("Changed " SFQ " from " SFQ " to " SFQ "\n",
                           lNm2Str(attribute_name),
                           (old_value ? "true" : "false"),
                           (new_value ? "true" : "false")));
#endif
                  lSetBool(this_elem, attribute_name, new_value);
                  *has_changed_conf_attr = true;
               }
            }
               break;
            default:
               DPRINTF("unhandled attribute\n");
               break;
         }
      }
   }
   DRETURN(ret);
}

/** @brief Apply a `qmod` command to one queue instance
 *
 * Walks the #change_state_t table to find the row for this transition, checks
 * that the caller is allowed to make it, and reports what happened.
 *
 * @param this_elem the queue instance (`QU_Type`)
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
qinstance_change_state_on_command(lListElem *this_elem, lList **answer_list,
                                  uint32_t transition, bool force_transition, const char *user, const char *host,
                                  bool is_operator, bool is_owner, monitoring_t *monitor, uint64_t gdi_session) {
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
                  sge_signal_queue(SGE_SIGSTOP, this_elem, nullptr, nullptr, monitor);
                  did_something = true;
               }
               break;
            case QI_DO_UNSUSPEND :
               if (!qinstance_state_is_susp_on_sub(this_elem) &&
                   !qinstance_state_is_cal_suspended(this_elem)) {
                  sge_signal_queue(SGE_SIGCONT, this_elem, nullptr, nullptr, monitor);
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
            ret &= sge_event_spool(answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, lGetString(this_elem, QU_qname),
                                   lGetHost(this_elem, QU_qhostname), nullptr, this_elem, nullptr, nullptr, true, true, gdi_session);

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
qinstance_change_state_on_calendar(lListElem *this_elem, const lListElem *calendar, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && calendar != nullptr) {
      lList *state_changes_list = nullptr;
      uint32_t state;
      uint64_t when = 0;

      state = calender_state_changes(calendar, &state_changes_list, &when, nullptr);

      ret = qinstance_change_state_on_calender_(this_elem, state, &state_changes_list, monitor, gdi_session);

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
qinstance_change_state_on_calendar_all(const char *cal_name, uint32_t cal_order,
                                       const lList *state_change_list, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for_each_ep_lv(cqueue, *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE)) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw_lv(qinstance, qinstance_list) {
         const char *queue_calendar = lGetString(qinstance, QU_calendar);

         if (queue_calendar != nullptr && !strcmp(queue_calendar, cal_name)) {
            lList *copy_state_change_list = lCopyList("state list", state_change_list);
            ret = qinstance_change_state_on_calender_(qinstance, cal_order, &copy_state_change_list, monitor, gdi_session);
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
 * @param state_change_list qi state list
 *
 * @return true, if it worked
 *
 * @note MT-NOTE: qinstance_change_state_on_calender_() is MT safe
 */
static bool qinstance_change_state_on_calender_(lListElem *this_elem, uint32_t cal_order,
                                                lList **state_change_list, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;
   bool old_cal_disabled = qinstance_state_is_cal_disabled(this_elem);
   bool old_cal_suspended = qinstance_state_is_cal_suspended(this_elem);
   bool new_cal_disabled = (cal_order == QI_DO_CAL_DISABLE) ? true : false;
   bool new_cal_suspended = (cal_order == QI_DO_CAL_SUSPEND) ? true : false;

   lSetList(this_elem, QU_state_changes, *state_change_list);
   *state_change_list = nullptr;

   if (old_cal_disabled != new_cal_disabled) {
      sge_qmaster_qinstance_state_set_cal_disabled(this_elem, new_cal_disabled, gdi_session);
   }

   if (old_cal_suspended != new_cal_suspended) {
      const char *name = lGetString(this_elem, QU_full_name);

      sge_qmaster_qinstance_state_set_cal_suspended(this_elem, new_cal_suspended, gdi_session);
      if (new_cal_suspended) {
         if (qinstance_state_is_susp_on_sub(this_elem)) {
            INFO(MSG_QINSTANCE_NOUSSOS_S, name);
         } else if (qinstance_state_is_manual_suspended(this_elem)) {
            INFO(MSG_QINSTANCE_NOUSADM_S, name);
         } else {
            sge_signal_queue(SGE_SIGSTOP, this_elem, nullptr, nullptr, monitor);
         }
      } else {
         if (qinstance_state_is_susp_on_sub(this_elem)) {
            INFO(MSG_QINSTANCE_NOSSOS_S, name);
         } else if (qinstance_state_is_manual_suspended(this_elem)) {
            INFO(MSG_QINSTANCE_NOSADM_S, name);
         } else {
            sge_signal_queue(SGE_SIGCONT, this_elem, nullptr, nullptr, monitor);
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
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_manual_disabled(lListElem *this_elem, bool set_state, uint64_t gdi_session) {
   bool changed;
   changed = qinstance_state_set_manual_disabled(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                  lGetString(this_elem, QU_full_name), QI_DISABLED, set_state, gdi_session);
   }

   return changed;
}

/** @brief Mark a queue instance as suspended by an administrator, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_manual_suspended(lListElem *this_elem, bool set_state, uint64_t gdi_session) {
   bool changed;
   changed = qinstance_state_set_manual_suspended(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                  lGetString(this_elem, QU_full_name), QI_SUSPENDED, set_state, gdi_session);
   }


   return changed;
}

/** @brief Mark a queue instance as unreachable, because its execution host stopped answering, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_unknown(lListElem *this_elem, bool set_state, uint64_t gdi_session) {
   bool changed;
   changed = qinstance_state_set_unknown(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      if (mconf_get_simulate_execds()) {
         sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                     lGetString(this_elem, QU_full_name), QI_UNKNOWN, false, gdi_session);
      } else {
         sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                     lGetString(this_elem, QU_full_name), QI_UNKNOWN, set_state, gdi_session);
      }
   }


   return changed;
}

/** @brief Mark a queue instance as in error state, because a job could not be started there, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gdi_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_error(lListElem *this_elem, bool set_state, uint64_t gdi_session) {
   bool changed;
   changed = qinstance_state_set_error(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                  lGetString(this_elem, QU_full_name), QI_ERROR, set_state, gdi_session);
   }


   return changed;
}

/** @brief Mark a queue instance as suspended by a subordinate relationship, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_susp_on_sub(lListElem *this_elem, bool set_state, uint64_t gid_session) {
   bool changed;
   changed = qinstance_state_set_susp_on_sub(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   return changed;
}

/** @brief Mark a queue instance as disabled by its calendar, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_cal_disabled(lListElem *this_elem, bool set_state, uint64_t gid_session) {
   bool changed;
   changed = qinstance_state_set_cal_disabled(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   return changed;
}

/** @brief Mark a queue instance as suspended by its calendar, or no longer so
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_cal_suspended(lListElem *this_elem, bool set_state, uint64_t gid_session) {
   bool changed;
   changed = qinstance_state_set_cal_suspended(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   return changed;
}

/** @brief Mark a queue instance as orphaned - deleted but still holding jobs
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_orphaned(lListElem *this_elem, bool set_state, uint64_t gid_session) {
   bool changed;
   changed = qinstance_state_set_orphaned(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
   }

   return changed;
}

/** @brief Mark a queue instance as ambiguously configured, so it cannot be used, or no longer so
 *
 * Announces the change to the event clients when the state actually moved.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param set_state true to set the state, false to clear it
 * @param gid_session the session the change belongs to
 * @return true when the state changed
 */
bool
sge_qmaster_qinstance_state_set_ambiguous(lListElem *this_elem, bool set_state, uint64_t gid_session) {
   bool changed;
   changed = qinstance_state_set_ambiguous(this_elem, set_state);
   if (changed) {
      ocs::ReportingFileWriter::create_queue_records(nullptr, this_elem, sge_get_gmt64());
      sge_ar_list_set_error_state(*ocs::DataStore::get_master_list_rw(SGE_TYPE_AR),
                                  lGetString(this_elem, QU_full_name), QI_AMBIGUOUS, set_state, gid_session);
   }


   return changed;
}

/* ret: did the state change */
/** @brief Put a newly created queue instance into the state its configuration asks for
 *
 * A queue may be configured to start out disabled, and a calendar may already
 * apply to it, so the state is not simply "enabled" at creation.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param gdi_session the session the change belongs to
 *
 * @return true if the initial configuration changed the state
 */
bool
sge_qmaster_qinstance_set_initial_state(lListElem *this_elem, uint64_t gdi_session) {
#ifdef QINSTANCE_MODIFY_DEBUG
   DENTER(TOP_LAYER);
#else
   DENTER(BASIS_LAYER);
#endif

   bool ret = false;
   const char *state_string = lGetString(this_elem, QU_initial_state);

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
 * @param this_elem QU_Type element
 * @param answer_list AN_Type element
 *
 * @return error result true  - success false - error
 *
 * @note MT-NOTE: qinstance_reinit_consumable_actual_list() is MT safe
 */
bool
qinstance_reinit_consumable_actual_list(lListElem *this_elem,
                                        lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      const char *name = lGetString(this_elem, QU_full_name);
      const lList *job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
      const lList *centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
      const lList *ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
      const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);

      lSetList(this_elem, QU_resource_utilization, nullptr);
      qinstance_set_conf_slots_used(this_elem);
      qinstance_debit_consumable(this_elem, nullptr, nullptr, centry_list, 0, true, true, nullptr);

      for_each_rw_lv(ep, job_list) {
         const lList *ja_task_list = lGetList(ep, JB_ja_tasks);

         for_each_ep_lv(ja_task, ja_task_list) {
            const lList *gdil = lGetList(ja_task, JAT_granted_destin_identifier_list);
            const lListElem *gdil_ep = lGetElemStr(gdil, JG_qname, name);
            const lListElem *pe = lGetObject(ja_task, JAT_pe_object);

            if (gdil_ep != nullptr) {
               int slots = lGetUlong(gdil_ep, JG_slots);

               bool is_master_task = false;
               if (gdil_ep == lFirst(gdil)) {
                  is_master_task = true;
               }

               bool do_per_host_booking = true;
               const lListElem *prev = lPrev(gdil_ep);
               if (prev != nullptr &&
                   sge_strnullcmp(lGetHost(gdil_ep, JG_qhostname), lGetHost(prev, JG_qhostname)) == 0) {
                  do_per_host_booking = false;
               }

               if (slots > 0) {
                  qinstance_debit_consumable(this_elem, ep, pe, centry_list, slots, is_master_task,
                                             do_per_host_booking, nullptr);
               }
            }
         }
      }
      for_each_rw_lv(ep, ar_list) {
         const lList *gdil = lGetList(ep, AR_granted_slots);
         const lListElem *gdil_ep = lGetElemStr(gdil, JG_qname, name);

         if (gdil_ep != nullptr) {
            bool is_master_task = false;
            lListElem *dummy_job = lCreateElem(JB_Type);

            if (gdil_ep == lFirst(gdil)) {
               is_master_task = true;
            }

            const char *pe_name = lGetString(ep, AR_granted_pe);
            const lListElem *pe = nullptr;
            if (pe_name != nullptr) {
               pe = pe_list_locate(master_pe_list, pe_name);
            }

            job_set_hard_resource_list(dummy_job, lCopyList(nullptr, lGetList(ep, AR_resource_list)));

            rc_add_job_utilization(nullptr, dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                   this_elem, centry_list, lGetUlong(gdil_ep, JG_slots),
                                   QU_consumable_config_list, QU_resource_utilization, name,
                                   lGetUlong64(ep, AR_start_time), lGetUlong64(ep, AR_duration),
                                   QUEUE_TAG, false, is_master_task, false);
            lFreeElem(&dummy_job);
         }
      }
   }

   DRETURN(ret);
}
