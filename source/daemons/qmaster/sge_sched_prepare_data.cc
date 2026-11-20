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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The event subscription the scheduler runs with
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "sched/msg_schedd.h"

#include "evc/sge_event_client.h"

#include "sge.h"
#include "sge_sched_prepare_data.h"
#include "sge_sched_process_events.h"

#include <ocs_CategoryQmaster.h>

static const int cqueue_field_ids[] = {
        CQ_name,
        /* CS-2677: CQ_hostlist was subscribed here and read nowhere in the
         * scheduler. The host list of a cluster queue is the member list of
         * its host group now; what the scheduler needs -- which hosts the
         * queue reaches -- it takes from CQ_qinstances, as it always did. */
        CQ_qinstances,
        CQ_consumable_config_list,
        CQ_projects,
        CQ_xprojects,
        CQ_acl,
        CQ_xacl,
        CQ_qtype,
        CQ_pe_list,
        CQ_nsuspend,
        CQ_job_slots,
        CQ_calendar,
        CQ_h_core,
        CQ_h_cpu,
        CQ_h_data,
        CQ_h_fsize,
        CQ_h_rss,
        CQ_h_rt,
        CQ_h_stack,
        CQ_h_vmem,
        CQ_min_cpu_interval,
        CQ_rerun,
        CQ_s_core,
        CQ_s_cpu,
        CQ_s_data,
        CQ_s_fsize,
        CQ_s_rt,
        CQ_s_rss,
        CQ_s_stack,
        CQ_s_vmem
};

static const int qinstance_field_ids[] = {
        QU_full_name,
        QU_qhostname,
        QU_tag,
        QU_qname,
        QU_acl,
        QU_xacl,
        QU_projects,
        QU_xprojects,
        QU_resource_utilization,
        QU_job_slots,
        QU_load_thresholds,
        QU_suspend_thresholds,
        QU_host_seq_no,
        QU_seq_no,
        QU_state,
        QU_tagged4schedule,
        QU_nsuspend,
        QU_suspend_interval,
        QU_consumable_config_list,
        QU_available_at,
        QU_soft_violation,
        QU_version,
        QU_subordinate_list,

        QU_qtype,
        QU_calendar,
        QU_s_rt,
        QU_h_rt,
        QU_s_cpu,
        QU_h_cpu,
        QU_s_fsize,
        QU_h_fsize,
        QU_s_data,
        QU_h_data,
        QU_s_stack,
        QU_h_stack,
        QU_s_core,
        QU_h_core,
        QU_s_rss,
        QU_h_rss,
        QU_s_vmem,
        QU_h_vmem,
        QU_min_cpu_interval,

        QU_suspended_on_subordinate,
        QU_last_suspend_threshold_ckeck,
        QU_pe_list,
        QU_ckpt_list,

        QU_state_changes
};

static const int job_nm[] = {
        JB_job_number,
        JB_category,
        JB_category_id,
        JB_request_set_list,
        JB_owner,
        JB_group,
        JB_grp_list,
        JB_ja_n_h_ids,
        JB_ja_template,
        JB_type,
        JB_ja_u_h_ids,
        JB_ja_s_h_ids,
        JB_ja_o_h_ids,
        JB_ja_a_h_ids,
        JB_pe,
        JB_project,
        JB_department,
        JB_execution_time,
        JB_override_tickets,
        JB_jid_predecessor_list,
        JB_deadline,
        JB_submission_time,
        JB_checkpoint_name,
        JB_version,
        JB_priority,
        JB_host,
        JB_ja_structure,
        JB_jobshare,
        JB_pe_range,
        JB_nppri,
        JB_urg,
        JB_nurg,
        JB_dlcontr,
        JB_wtcontr,
        JB_rrcontr,
        JB_soft_wallclock_gmt,
        JB_hard_wallclock_gmt,
        JB_reserve,
        JB_ja_tasks,
        JB_ar,
        JB_ja_task_concurrency,
        JB_binding
};

static const int jat_nm[] = {
        JAT_task_number,
        JAT_tix,
        JAT_state,
        JAT_fshare,
        JAT_status,
        JAT_granted_pe,
        JAT_pe_object,
        JAT_scaled_usage_list,
        JAT_task_list,
        JAT_start_time,
        JAT_hold,
        JAT_granted_destin_identifier_list,
        JAT_master_queue,
        JAT_oticket,
        JAT_fticket,
        JAT_sticket,
        JAT_share,
        JAT_prio,
        JAT_ntix,
        JAT_wallclock_limit,
        JAT_granted_resources_list
};

static const int pet_nm[] = {
        PET_id,
        PET_status,
        PET_granted_destin_identifier_list,
        PET_usage,
        PET_scaled_usage,
        PET_previous_usage
};

static const int eh_nm[] = {
        EH_name,
        EH_scaling_list,
        EH_consumable_config_list,
        EH_usage_scaling_list,
        EH_load_list,
        EH_acl,
        EH_xacl,
        EH_prj,
        EH_xprj,
        EH_sort_value,
        EH_load_correction_factor,
        EH_seq_no,
        EH_resource_utilization,
        EH_reschedule_unknown_list,
        EH_internal_topology
};

static const int pe_nm[] = {
        PE_name,
        PE_slots,
        PE_user_list,
        PE_xuser_list,
        PE_allocation_rule,
        PE_control_slaves,
        PE_job_is_first_task,
        PE_resource_utilization,
        PE_urgency_slots,
        PE_ignore_slave_requests_on_master_host
};


/** @brief Build whatever of the subscription is not built yet
 *
 * A selection that cannot be built (no memory) stays empty; the missing ones are reported as critical, as before.
 *
 * @param where_what the subscription, filled in place
 */
void
ensure_valid_what_and_where(sge_where_what_t *where_what) {
   DENTER(GDI_LAYER);

   try {
      /* acl */
      if (!where_what->where_acl) {
         where_what->where_acl = ocs::tom::field<US_type>().has_all_bits(US_ACL);
      }
      if (!where_what->what_acldept) {
         where_what->what_acldept = ocs::tom::what_all<ocs::tom::US>();
      }

      /* cqueues */
      if (!where_what->what_cqueue) {
         where_what->what_cqueue = ocs::tom::what<ocs::tom::CQ>(cqueue_field_ids);
      }

      /* departments */
      if (!where_what->where_dept) {
         where_what->where_dept = ocs::tom::field<US_type>().has_all_bits(US_DEPT);
      }

      /* configuration */
      if (!where_what->where_config) {
         const char *hostname = component_get_qualified_hostname();
         where_what->where_config = ocs::tom::field<CONF_name>().equals_nocase(SGE_GLOBAL_NAME) ||
                                    ocs::tom::field<CONF_name>().same_host(hostname);
         where_what->what_config = ocs::tom::what_all<ocs::tom::CONF>();
      }

      /* host */
      if (!where_what->where_host) {
         where_what->where_host = !ocs::tom::field<EH_name>().equals_nocase(SGE_TEMPLATE_NAME);
      }
      if (!where_what->what_host) {
         where_what->what_host = ocs::tom::what<ocs::tom::EH>(eh_nm);
      }

      /* job */
      if (!where_what->what_job) {
         where_what->what_job = ocs::tom::what<ocs::tom::JB>(job_nm);
      }

      /* jat */
      if (!where_what->what_jat) {
         where_what->what_jat = ocs::tom::what<ocs::tom::JAT>(jat_nm);
      }

      /* pet */
      if (!where_what->what_pet) {
         where_what->what_pet = ocs::tom::what<ocs::tom::PET>(pet_nm);
      }

      /* pe */
      if (!where_what->what_pe) {
         where_what->what_pe = ocs::tom::what<ocs::tom::PE>(pe_nm);
      }

      // qinstances (remaining are available, disabled, calendar disabled, orphaned, alarm, suspend alarm)
      // A reduced descriptor keeps the positions of the full type, so the conditions on QU_state built on the
      // full type hold for the reduced queue instances of the subscription as well.
      if (!where_what->where_queue) {
         where_what->where_queue =
            !ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED) &&                // only not suspended queues
            !ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED_ON_SUBORDINATE) && // not suspended on subordinate
            !ocs::tom::field<QU_state>().has_all_bits(QI_CAL_SUSPENDED) &&            // not suspended on calendar
            !ocs::tom::field<QU_state>().has_all_bits(QI_ERROR) &&                    // not in error state
            !ocs::tom::field<QU_state>().has_all_bits(QI_UNKNOWN) &&                  // not in unknown state
            !ocs::tom::field<QU_state>().has_all_bits(QI_AMBIGUOUS);                  // not in ambiguous state
            // @todo: what about the QI_ORPHANED, QI_ALARM, QI_SUSPEND_ALARM ???
      }
      if (!where_what->what_queue) {
         where_what->what_queue = ocs::tom::what<ocs::tom::QU>(qinstance_field_ids);
      }

      // qinstances (remaining are calendar suspended (possibly also orphaned, alarm, suspend alarm) but not also ...)
      if (!where_what->where_queue2) {
         where_what->where_queue2 =
            ocs::tom::field<QU_state>().has_all_bits(QI_CAL_SUSPENDED) &&             // todo: only calendar suspended?
            !ocs::tom::field<QU_state>().has_all_bits(QI_CAL_DISABLED) &&             // not calendar disabled
            !ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED) &&                // not suspended
            !ocs::tom::field<QU_state>().has_all_bits(QI_SUSPENDED_ON_SUBORDINATE) && // not suspended on subordinate
            !ocs::tom::field<QU_state>().has_all_bits(QI_ERROR) &&                    // not in error state
            !ocs::tom::field<QU_state>().has_all_bits(QI_UNKNOWN) &&                  // not in unknown state
            !ocs::tom::field<QU_state>().has_all_bits(QI_DISABLED) &&                 // not disabled
            !ocs::tom::field<QU_state>().has_all_bits(QI_AMBIGUOUS);                  // not in ambiguous state
      }
      if (!where_what->what_queue2) {
         where_what->what_queue2 = ocs::tom::what_all<ocs::tom::QU>();
      }
      if (!where_what->where_all_queue) {
         where_what->where_all_queue = ocs::tom::field<QU_qname>() != SGE_TEMPLATE_NAME;
      }
   } catch (const std::exception &) {
      // no memory: what could not be built stays empty and is reported below, as lWhere()/lWhat() failing was
   }

   if (!where_what->where_acl || !where_what->what_acldept ||
       !where_what->what_cqueue || !where_what->where_dept ||
       !where_what->where_host || !where_what->what_host ||
       !where_what->what_job || !where_what->what_jat ||
       !where_what->what_pet || !where_what->where_queue ||
       !where_what->what_queue || !where_what->where_queue2 ||
       !where_what->what_queue2 || !where_what->where_all_queue ||
       !where_what->what_pe) {
      CRITICAL(SFNMAX, MSG_SCHEDD_UNABLE_TO_SETUP_FILTER);
   }
   DRETURN_VOID;
}

/** @brief Release the subscription
 *
 * Every selection is freed and its handle left empty, so the subscription can be built again.
 *
 * @param where_what the subscription
 */
void
free_what_and_where(sge_where_what_t *where_what) {
   *where_what = sge_where_what_t{};
}

/** @brief React to a request that the next scheduling run be profiled
 *
 * @param evc the event client
 * @param type the object type the event is about
 * @param action what happened to it
 * @param event the event (ET)
 * @param clientdata the scheduler's snapshot
 * @return whether the mirror should go on to apply the event
 */
sge_callback_result
sge_process_schedd_monitor_event(sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                                 [[maybe_unused]] const sge_event_action action,
                                 [[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::ET> event,
                                 [[maybe_unused]] void *clientdata) {
   DENTER(GDI_LAYER);
   DPRINTF("monitoring next scheduler run\n");
   evc->monitor_next_run = true;
   DRETURN(SGE_EMA_OK);
}

/** @brief React to a change in the global configuration
 *
 * @param evc the event client
 * @param type the object type the event is about
 * @param action what happened to it
 * @param event the event (ET)
 * @param clientdata the scheduler's snapshot
 * @return whether the mirror should go on to apply the event
 */
sge_callback_result
sge_process_global_config_event([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                                [[maybe_unused]] const sge_event_action action,
                                [[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::ET> event,
                                [[maybe_unused]] void *clientdata) {
   DENTER(GDI_LAYER);
   DPRINTF("notification about new global configuration\n");
   st_set_flag_new_global_conf(true);
   DRETURN(SGE_EMA_OK);
}

/** @brief React to a job change once the mirror has applied it
 *
 * @param evc the event client
 * @param type the object type the event is about
 * @param action what happened to it
 * @param event the event (ET)
 * @param clientdata the scheduler's snapshot
 * @return whether the event was handled
 */
sge_callback_result
sge_process_job_event_after([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                            const sge_event_action action, const ocs::tom::ConstElement<ocs::tom::ET> event,
                            [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   uint32_t job_id = 0;
   ocs::tom::Element<ocs::tom::JB> job;
   if (action == SGE_EMA_ADD || action == SGE_EMA_MOD) {

      // Find the job for which we received an event (ADD and MOD)
      job_id = ocs::tom::get<ET_intkey>(event);
      // the job is modified (JAT_status), so the list is taken writable; for_read keeps the read lock semantics
      const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true);
      job = master_job_list.find_first<JB_job_number>(job_id);
      if (!job) {
         DSTRING_STATIC(id_dstring, 64);
         ERROR(MSG_CANTFINDJOBINMASTERLIST_S, job_get_id_string(job_id, 0, nullptr, &id_dstring));
         DRETURN(SGE_EMA_FAILURE);
      }
   }

   switch (action) {
      case SGE_EMA_MOD:
         switch (ocs::tom::get<ET_type>(event)) {
            case sgeE_JOB_FINAL_USAGE: {
               const char *pe_task_id = ocs::tom::get<ET_strkey>(event);

               if (pe_task_id == nullptr) {
                  // ignore FINAL_USAGE for a pe task here
                  uint32_t ja_task_id = ocs::tom::get<ET_intkey2>(event);
                  const auto ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(job, nullptr, ja_task_id));

                  if (!ja_task) {
                     ERROR(MSG_CANTFINDTASKINJOB_UU, ja_task_id, job_id);
                     DRETURN(SGE_EMA_FAILURE);
                  }

                  ja_task.set<JAT_status>(JFINISHED);
               }
               break;
            }

            case sgeE_JOB_MOD:
            default:
               break;
         }
         break;

      case SGE_EMA_ADD:
      case SGE_EMA_LIST:
      default:
         break;
   }

   DRETURN(SGE_EMA_OK);
}
