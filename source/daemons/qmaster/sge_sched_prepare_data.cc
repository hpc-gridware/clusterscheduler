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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/ocs_DataStore.h"

#include "sched/msg_schedd.h"
#include "sched/sgeee.h"

#include "evc/sge_event_client.h"

#include "sge.h"
#include "sge_sched_prepare_data.h"
#include "sge_sched_process_events.h"

#include <ocs_CategoryQmaster.h>

static const int cqueue_field_ids[] = {
        CQ_name,
        CQ_hostlist,
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
        CQ_s_vmem,
        NoName
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

        QU_state_changes,

        NoName
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
        NoName
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
        JAT_granted_resources_list,
        NoName
};

static const int pet_nm[] = {
        PET_id,
        PET_status,
        PET_granted_destin_identifier_list,
        PET_usage,
        PET_scaled_usage,
        PET_previous_usage,
        NoName
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
        NoName
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
        PE_ignore_slave_requests_on_master_host,
        NoName
};


void
ensure_valid_what_and_where(sge_where_what_t *where_what) {
   lDescr *tmp_what_descr = nullptr;

   DENTER(GDI_LAYER);

   /* prepare temp data used to create new lists with partial descriptor */
   if (where_what->what_queue2 == nullptr || where_what->where_queue2 == nullptr ||
       where_what->what_queue == nullptr || where_what->where_queue == nullptr) {
      int n = 0;
      int index = 0;
      lEnumeration *tmp_what_queue = nullptr;

      tmp_what_queue = lIntVector2What(QU_Type, qinstance_field_ids);
      n = lCountWhat(tmp_what_queue, QU_Type);
      tmp_what_descr = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1));
      lPartialDescr(tmp_what_queue, QU_Type, tmp_what_descr, &index);

      lFreeWhat(&tmp_what_queue);
   }

   /* acl */
   if (where_what->where_acl == nullptr) {
      where_what->where_acl = lWhere("%T(%I m= %u)", US_Type, US_type, US_ACL);
   }
   if (where_what->what_acldept == nullptr) {
      where_what->what_acldept = lWhat("%T(ALL)", US_Type);
   }

   /* cqueues */
   if (where_what->what_cqueue == nullptr) {
      where_what->what_cqueue = lIntVector2What(CQ_Type, cqueue_field_ids);
   }

   /* departments */
   if (where_what->where_dept == nullptr) {
      where_what->where_dept = lWhere("%T(%I m= %u)", US_Type, US_type, US_DEPT);
   }

   /* configuration */
   if (where_what->where_config == nullptr) {
      const char *hostname = component_get_qualified_hostname();
      where_what->where_config = lWhere("%T(%I c= %s || %I h= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME, CONF_name, hostname);
      where_what->what_config = lWhat("%T(ALL)", CONF_Type);
   }

   /* host */
   if (where_what->where_host == nullptr) {
      where_what->where_host = lWhere("%T(!(%Ic=%s))", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   }
   if (where_what->what_host == nullptr) {
      where_what->what_host = lIntVector2What(EH_Type, eh_nm);
   }

   /* job */
   if (where_what->what_job == nullptr) {
      where_what->what_job = lIntVector2What(JB_Type, job_nm);
   }

   /* jat */
   if (where_what->what_jat == nullptr) {
      where_what->what_jat = lIntVector2What(JAT_Type, jat_nm);
   }

   /* pet */
   if (where_what->what_pet == nullptr) {
      where_what->what_pet = lIntVector2What(PET_Type, pet_nm);
   }

   /* pe */
   if (where_what->what_pe == nullptr) {
      where_what->what_pe = lIntVector2What(PE_Type, pe_nm);
   }

   // qinstances (remaining are available, disabled, calendar disabled, orphaned, alarm, suspend alarm)
   if (where_what->where_queue == nullptr) {
      where_what->where_queue = lWhere("%T("
                                       " !(%I m= %u) &&"
                                       " !(%I m= %u) &&"
                                       " !(%I m= %u) &&"
                                       " !(%I m= %u) &&"
                                       " !(%I m= %u) &&"
                                       " !(%I m= %u))",
                                       tmp_what_descr,
                                       QU_state, QI_SUSPENDED,                // only not suspended queues
                                       QU_state, QI_SUSPENDED_ON_SUBORDINATE, // not suspended on subordinate
                                       QU_state, QI_CAL_SUSPENDED,            // not suspended on calendar
                                       QU_state, QI_ERROR,                    // not in error state
                                       QU_state, QI_UNKNOWN,                  // not in unknown state
                                       QU_state, QI_AMBIGUOUS                 // not in ambiguous state

                                       // @todo: what about the QI_ORPHANED, QI_ALARM, QI_SUSPEND_ALARM ???
      );
   }
   if (where_what->what_queue == nullptr) {
      where_what->what_queue = lIntVector2What(QU_Type, qinstance_field_ids);
   }

   // qinstances (remaining are calendar suspended (possibly also orphaned, alarm, suspend alarm) but not also ...)
   if (where_what->where_queue2 == nullptr) {
      where_what->where_queue2 = lWhere("%T("
                                        "  (%I m= %u) &&"                        // todo: only calendar suspended queues ???
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u) &&"
                                        " !(%I m= %u))",
                                        tmp_what_descr,
                                        QU_state, QI_CAL_SUSPENDED,              // only calendar suspended queues
                                        QU_state, QI_CAL_DISABLED,               // not calendar disabled
                                        QU_state, QI_SUSPENDED,                  // not suspended
                                        QU_state, QI_SUSPENDED_ON_SUBORDINATE,   // not suspended on subordinate
                                        QU_state, QI_ERROR,                      // not in error state
                                        QU_state, QI_UNKNOWN,                    // not in unknown state
                                        QU_state, QI_DISABLED,                   // not disabled
                                        QU_state, QI_AMBIGUOUS                   // not in ambiguous state
      );
   }
   if (where_what->what_queue2 == nullptr) {
      where_what->what_queue2 = lWhat("%T(ALL)", tmp_what_descr);
   }
   if (where_what->where_all_queue == nullptr) {
      where_what->where_all_queue = lWhere("%T(%I!=%s)", QU_Type,
                                           QU_qname, SGE_TEMPLATE_NAME);
   }

   if (tmp_what_descr == nullptr ||
       where_what->where_acl == nullptr || where_what->what_acldept == nullptr ||
       where_what->what_cqueue == nullptr || where_what->where_dept == nullptr ||
       where_what->where_host == nullptr || where_what->what_host == nullptr ||
       where_what->what_job == nullptr || where_what->what_jat == nullptr ||
       where_what->what_pet == nullptr || where_what->where_queue == nullptr ||
       where_what->what_queue == nullptr || where_what->where_queue2 == nullptr ||
       where_what->what_queue2 == nullptr || where_what->where_all_queue == nullptr ||
       where_what->what_pe == nullptr) {
      CRITICAL(SFNMAX, MSG_SCHEDD_UNABLE_TO_SETUP_FILTER);
   }
   /* cleanup tmp data */
   if (tmp_what_descr != nullptr) {
      cull_hash_free_descr(tmp_what_descr);
      sge_free(&tmp_what_descr);
   }
   DRETURN_VOID;
}

void
free_what_and_where(sge_where_what_t *where_what) {
   lFreeWhere(&(where_what->where_queue));
   lFreeWhere(&(where_what->where_queue2));
   lFreeWhere(&(where_what->where_all_queue));
   lFreeWhere(&(where_what->where_cqueue));
   lFreeWhere(&(where_what->where_job));
   lFreeWhere(&(where_what->where_host));
   lFreeWhere(&(where_what->where_dept));
   lFreeWhere(&(where_what->where_acl));
   lFreeWhere(&(where_what->where_jat));
   lFreeWhere(&(where_what->where_config));

   lFreeWhat(&(where_what->what_queue));
   lFreeWhat(&(where_what->what_queue2));
   lFreeWhat(&(where_what->what_cqueue));
   lFreeWhat(&(where_what->what_job));
   lFreeWhat(&(where_what->what_host));
   lFreeWhat(&(where_what->what_acldept));
   lFreeWhat(&(where_what->what_jat));
   lFreeWhat(&(where_what->what_pet));
   lFreeWhat(&(where_what->what_pe));
   lFreeWhat(&(where_what->what_config));
}

sge_callback_result
sge_process_schedd_monitor_event(sge_evc_class_t *evc, sge_object_type type,
                                 sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(GDI_LAYER);
   DPRINTF("monitoring next scheduler run\n");
   evc->monitor_next_run = true;
   DRETURN(SGE_EMA_OK);
}

sge_callback_result
sge_process_global_config_event(sge_evc_class_t *evc, sge_object_type type,
                                sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(GDI_LAYER);
   DPRINTF("notification about new global configuration\n");

   st_set_flag_new_global_conf(true);
   DRETURN(SGE_EMA_OK);
}

sge_callback_result
sge_process_job_event_after(sge_evc_class_t *evc, sge_object_type type, sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(TOP_LAYER);

   // Find job ID and job where we received an event (add, modify)
   u_long32 job_id = 0;
   lListElem *job = nullptr;
   if (action == SGE_EMA_ADD || action == SGE_EMA_MOD) {
      job_id = lGetUlong(event, ET_intkey);
      job = lGetElemUlongRW(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), JB_job_number, job_id);

      if (job == nullptr) {
         DSTRING_STATIC(id_dstring, 64);
         ERROR(MSG_CANTFINDJOBINMASTERLIST_S, job_get_id_string(job_id, 0, nullptr, &id_dstring));
         DRETURN(SGE_EMA_FAILURE);
      }
   }

   switch (action) {
      case SGE_EMA_MOD:
         switch (lGetUlong(event, ET_type)) {
            case sgeE_JOB_FINAL_USAGE: {
               const char *pe_task_id = lGetString(event, ET_strkey);

               if (pe_task_id == nullptr) {
                  // ignore FINAL_USAGE for a pe task here
                  u_long32 ja_task_id = lGetUlong(event, ET_intkey2);
                  lListElem *ja_task = job_search_task(job, nullptr, ja_task_id);

                  if (ja_task == nullptr) {
                     ERROR(MSG_CANTFINDTASKINJOB_UU, ja_task_id, job_id);
                     DRETURN(SGE_EMA_FAILURE);
                  }

                  lSetUlong(ja_task, JAT_status, JFINISHED);
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

#if 0
/****** sge_process_events/sge_process_userset_event_before() ******************
*  NAME
*     sge_process_userset_event_before() -- ???
*
*  SYNOPSIS
*     bool sge_process_userset_event_before(sge_object_type type,
*     sge_event_action action, lListElem *event, void *clientdata)
*
*  FUNCTION
*     Determine whether categories need to be rebuilt. Rebuilding
*     categories is necessary, if a userset (a) gets used first
*     time as ACL or (b) is no longer used as ACL. Also categories
*     must be rebuild if entries change with a userset is used as ACL.
*
*  NOTES
*     MT-NOTE: sge_process_userset_event_before() is not MT safe
*******************************************************************************/
sge_callback_result
sge_process_userset_event_before(sge_evc_class_t *evc, sge_object_type type, sge_event_action action, lListElem *event,
                                 void *clientdata) {
   const lListElem *new_ep, *old_ep;
   const char *u;

   DENTER(GDI_LAYER);

   if (action != SGE_EMA_ADD &&
       action != SGE_EMA_MOD &&
       action != SGE_EMA_DEL) {
      DRETURN(SGE_EMA_OK);
   }

   u = lGetString(event, ET_strkey);
   new_ep = lFirst(lGetList(event, ET_new_version));
   old_ep = lGetElemStrRW(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET), US_name, u);

   switch (action) {
      case SGE_EMA_ADD:
         if (lGetBool(new_ep, US_consider_with_categories)) {
            set_rebuild_categories(true);
            DPRINTF("callback before userset event: rebuild categories due to SGE_EMA_ADD(%s)\n", u);
         }
         break;
      case SGE_EMA_MOD:
         /* need to redo categories if certain changes occur:
            --> it gets used or was used as ACL with queue_conf(5)/host_conf(5)/sge_pe(5)
            --> it is in use as ACL with queue_conf(5)/host_conf(5)/sge_pe(5)
                and a change with users/groups occurred */

         if ((lGetBool(new_ep, US_consider_with_categories) != lGetBool(old_ep, US_consider_with_categories))
             || (lGetBool(old_ep, US_consider_with_categories) &&
                 object_list_has_differences(lGetList(old_ep, US_entries), nullptr, lGetList(new_ep, US_entries)))) {
            set_rebuild_categories(true);
            DPRINTF("callback before userset event: rebuild categories due to SGE_EMA_MOD(%s)\n", u);
         }

         break;
      case SGE_EMA_DEL:
         if (lGetBool(old_ep, US_consider_with_categories)) {
            set_rebuild_categories(true);
            DPRINTF("callback before userset event: rebuild categories due to SGE_EMA_DEL(%s)\n", u);
         }
         break;
      default:
         break;
   }

   DRETURN(SGE_EMA_OK);
}
#endif
