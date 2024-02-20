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
 ************************************************************************/
/*___INFO__MARK_END__*/


/* auto generated file */
#include <cstddef>
#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"

const lDescr *
object_get_subtype(int nm)
{
   const lDescr *ret = nullptr;
   switch (nm) {
      case JB_jid_request_list:
         ret = JRE_Type;
         break;
      case JB_jid_predecessor_list:
         ret = JRE_Type;
         break;
      case JB_jid_successor_list:
         ret = JRE_Type;
         break;
      case JB_ja_ad_request_list:
         ret = JRE_Type;
         break;
      case JB_ja_ad_predecessor_list:
         ret = JRE_Type;
         break;
      case JB_ja_ad_successor_list:
         ret = JRE_Type;
         break;
      case JB_shell_list:
         ret = PN_Type;
         break;
      case JB_env_list:
         ret = VA_Type;
         break;
      case JB_context:
         ret = VA_Type;
         break;
      case JB_job_args:
         ret = ST_Type;
         break;
      case JB_stdout_path_list:
         ret = PN_Type;
         break;
      case JB_stderr_path_list:
         ret = PN_Type;
         break;
      case JB_stdin_path_list:
         ret = PN_Type;
         break;
      case JB_hard_resource_list:
         ret = CE_Type;
         break;
      case JB_soft_resource_list:
         ret = CE_Type;
         break;
      case JB_hard_queue_list:
         ret = QR_Type;
         break;
      case JB_soft_queue_list:
         ret = QR_Type;
         break;
      case JB_mail_list:
         ret = MR_Type;
         break;
      case JB_pe_range:
         ret = RN_Type;
         break;
      case JB_master_hard_queue_list:
         ret = QR_Type;
         break;
      case JB_ja_structure:
         ret = RN_Type;
         break;
      case JB_ja_n_h_ids:
         ret = RN_Type;
         break;
      case JB_ja_u_h_ids:
         ret = RN_Type;
         break;
      case JB_ja_s_h_ids:
         ret = RN_Type;
         break;
      case JB_ja_o_h_ids:
         ret = RN_Type;
         break;
      case JB_ja_a_h_ids:
         ret = RN_Type;
         break;
      case JB_ja_z_ids:
         ret = RN_Type;
         break;
      case JB_ja_template:
         ret = JAT_Type;
         break;
      case JB_ja_tasks:
         ret = JAT_Type;
         break;
      case JB_user_list:
         ret = ST_Type;
         break;
      case JB_job_identifier_list:
         ret = ID_Type;
         break;
      case JB_qs_args:
         ret = ST_Type;
         break;
      case JB_path_aliases:
         ret = PA_Type;
         break;
      case JB_binding:
         ret = BN_Type;
         break;
      case QU_ckpt_list:
         ret = ST_Type;
         break;
      case QU_pe_list:
         ret = ST_Type;
         break;
      case QU_owner_list:
         ret = US_Type;
         break;
      case QU_acl:
         ret = US_Type;
         break;
      case QU_xacl:
         ret = US_Type;
         break;
      case QU_projects:
         ret = PR_Type;
         break;
      case QU_xprojects:
         ret = PR_Type;
         break;
      case QU_consumable_config_list:
         ret = CE_Type;
         break;
      case QU_load_thresholds:
         ret = CE_Type;
         break;
      case QU_suspend_thresholds:
         ret = CE_Type;
         break;
      case QU_subordinate_list:
         ret = SO_Type;
         break;
      case QU_resource_utilization:
         ret = RUE_Type;
         break;
      case QU_message_list:
         ret = QIM_Type;
         break;
      case QU_state_changes:
         ret = CCT_Type;
         break;
      case EH_scaling_list:
         ret = HS_Type;
         break;
      case EH_consumable_config_list:
         ret = CE_Type;
         break;
      case EH_usage_scaling_list:
         ret = HS_Type;
         break;
      case EH_load_list:
         ret = HL_Type;
         break;
      case EH_acl:
         ret = US_Type;
         break;
      case EH_xacl:
         ret = US_Type;
         break;
      case EH_prj:
         ret = PR_Type;
         break;
      case EH_xprj:
         ret = PR_Type;
         break;
      case EH_scaled_usage_list:
         ret = UA_Type;
         break;
      case EH_scaled_usage_pct_list:
         ret = UA_Type;
         break;
      case EH_resource_utilization:
         ret = RUE_Type;
         break;
      case EH_cached_complexes:
         ret = CE_Type;
         break;
      case EH_reschedule_unknown_list:
         ret = RU_Type;
         break;
      case EH_report_variables:
         ret = STU_Type;
         break;
      case EH_merged_report_variables:
         ret = STU_Type;
         break;
      case EV_subscribed:
         ret = EVS_Type;
         break;
      case EV_events:
         ret = ET_Type;
         break;
      case CE_resource_map_list:
         ret = RESL_Type;
         break;
      case OR_queuelist:
         ret = OQ_Type;
         break;
      case OR_granted_resources_list:
         ret = GRU_Type;
         break;
      case US_entries:
         ret = UE_Type;
         break;
      case PE_user_list:
         ret = US_Type;
         break;
      case PE_xuser_list:
         ret = US_Type;
         break;
      case PE_resource_utilization:
         ret = RUE_Type;
         break;
      case CONF_entries:
         ret = CF_Type;
         break;
      case SPA_argval_lListT:
         ret = ST_Type;
         break;
      case PR_usage:
         ret = UA_Type;
         break;
      case PR_long_term_usage:
         ret = UA_Type;
         break;
      case PR_project:
         ret = UPP_Type;
         break;
      case PR_acl:
         ret = US_Type;
         break;
      case PR_xacl:
         ret = US_Type;
         break;
      case PR_debited_job_usage:
         ret = UPU_Type;
         break;
      case UU_usage:
         ret = UA_Type;
         break;
      case UU_long_term_usage:
         ret = UA_Type;
         break;
      case UU_project:
         ret = UPP_Type;
         break;
      case UU_debited_job_usage:
         ret = UPU_Type;
         break;
      case GRU_resource_map_list:
         ret = RESL_Type;
         break;
      case STN_children:
         ret = STN_Type;
         break;
      case STN_usage_list:
         ret = UA_Type;
         break;
      case SC_job_load_adjustments:
         ret = CE_Type;
         break;
      case SC_usage_weight_list:
         ret = UA_Type;
         break;
      case PET_granted_destin_identifier_list:
         ret = JG_Type;
         break;
      case PET_usage:
         ret = UA_Type;
         break;
      case PET_scaled_usage:
         ret = UA_Type;
         break;
      case PET_reported_usage:
         ret = UA_Type;
         break;
      case PET_previous_usage:
         ret = UA_Type;
         break;
      case PET_path_aliases:
         ret = PA_Type;
         break;
      case PET_environment:
         ret = VA_Type;
         break;
      case PETR_path_aliases:
         ret = PA_Type;
         break;
      case PETR_environment:
         ret = VA_Type;
         break;
      case JR_usage:
         ret = UA_Type;
         break;
      case JL_OS_job_list:
         ret = JO_Type;
         break;
      case JO_usage_list:
         ret = UA_Type;
         break;
      case JO_pid_list:
         ret = JP_Type;
         break;
      case UPU_old_usage_list:
         ret = UA_Type;
         break;
      case KRB_tgt_list:
         ret = KTGT_Type;
         break;
      case ID_ja_structure:
         ret = RN_Type;
         break;
      case ID_user_list:
         ret = ST_Type;
         break;
      case MA_answers:
         ret = AN_Type;
         break;
      case CAL_parsed_year_calendar:
         ret = CA_Type;
         break;
      case CAL_parsed_week_calendar:
         ret = CA_Type;
         break;
      case CA_yday_range_list:
         ret = TMR_Type;
         break;
      case CA_wday_range_list:
         ret = TMR_Type;
         break;
      case CA_daytime_range_list:
         ret = TMR_Type;
         break;
      case TMR_begin:
         ret = TM_Type;
         break;
      case TMR_end:
         ret = TM_Type;
         break;
      case UPP_usage:
         ret = UA_Type;
         break;
      case UPP_long_term_usage:
         ret = UA_Type;
         break;
      case SME_message_list:
         ret = MES_Type;
         break;
      case SME_global_message_list:
         ret = MES_Type;
         break;
      case MES_job_number_list:
         ret = ULNG_Type;
         break;
      case JAT_granted_destin_identifier_list:
         ret = JG_Type;
         break;
      case JAT_granted_resources_list:
         ret = GRU_Type;
         break;
      case JAT_usage_list:
         ret = UA_Type;
         break;
      case JAT_scaled_usage_list:
         ret = UA_Type;
         break;
      case JAT_reported_usage_list:
         ret = UA_Type;
         break;
      case JAT_task_list:
         ret = PET_Type;
         break;
      case JAT_finished_task_list:
         ret = FPET_Type;
         break;
      case JAT_previous_usage_list:
         ret = UA_Type;
         break;
      case JAT_message_list:
         ret = QIM_Type;
         break;
      case CT_cache:
         ret = CCT_Type;
         break;
      case HGRP_host_list:
         ret = HR_Type;
         break;
      case HGRP_cqueue_list:
         ret = CQ_Type;
         break;
      case LS_incomplete:
         ret = LR_Type;
         break;
      case LS_complete:
         ret = LR_Type;
         break;
      case SPC_rules:
         ret = SPR_Type;
         break;
      case SPC_types:
         ret = SPT_Type;
         break;
      case SPT_rules:
         ret = SPTR_Type;
         break;
      case JJ_finished_tasks:
         ret = JJAT_Type;
         break;
      case JJ_not_yet_finished_ids:
         ret = RN_Type;
         break;
      case JJ_started_task_ids:
         ret = RN_Type;
         break;
      case JJAT_rusage:
         ret = UA_Type;
         break;
      case NSV_strings:
         ret = ST_Type;
         break;
      case ASTRLIST_value:
         ret = ST_Type;
         break;
      case AUSRLIST_value:
         ret = US_Type;
         break;
      case APRJLIST_value:
         ret = PR_Type;
         break;
      case ACELIST_value:
         ret = CE_Type;
         break;
      case ASOLIST_value:
         ret = SO_Type;
         break;
      case CQ_hostlist:
         ret = HR_Type;
         break;
      case CQ_qinstances:
         ret = QU_Type;
         break;
      case XMLH_Stylesheet:
         ret = XMLS_Type;
         break;
      case XMLH_Attribute:
         ret = XMLA_Type;
         break;
      case XMLE_Attribute:
         ret = XMLA_Type;
         break;
      case RDE_resource_map_list:
         ret = RESL_Type;
         break;
      case RUE_utilized_now_resource_map_list:
         ret = RESL_Type;
         break;
      case RUE_utilized:
         ret = RDE_Type;
         break;
      case RUE_utilized_nonexclusive:
         ret = RDE_Type;
         break;
      case LDR_queue_ref_list:
         ret = QR_Type;
         break;
      case CCT_ignore_queues:
         ret = CTI_Type;
         break;
      case CCT_ignore_hosts:
         ret = CTI_Type;
         break;
      case CCT_job_messages:
         ret = MES_Type;
         break;
      case SCT_job_pending_ref:
         ret = REF_Type;
         break;
      case SCT_job_ref:
         ret = REF_Type;
         break;
      case RQS_rule:
         ret = RQR_Type;
         break;
      case RQR_limit:
         ret = RQRL_Type;
         break;
      case RQRF_scope:
         ret = ST_Type;
         break;
      case RQRF_xscope:
         ret = ST_Type;
         break;
      case RQRL_usage:
         ret = RUE_Type;
         break;
      case AR_resource_list:
         ret = CE_Type;
         break;
      case AR_resource_utilization:
         ret = RUE_Type;
         break;
      case AR_queue_list:
         ret = QR_Type;
         break;
      case AR_granted_slots:
         ret = JG_Type;
         break;
      case AR_reserved_queues:
         ret = QU_Type;
         break;
      case AR_mail_list:
         ret = MR_Type;
         break;
      case AR_pe_range:
         ret = RN_Type;
         break;
      case AR_master_queue_list:
         ret = QR_Type;
         break;
      case AR_acl_list:
         ret = ARA_Type;
         break;
      case AR_xacl_list:
         ret = ARA_Type;
         break;
      case EVR_event_list:
         ret = ET_Type;
         break;
      case RTIC_tickets:
         ret = OR_Type;
         break;
      case PRO_groups:
         ret = GR_Type;
         break;
   }
   return ret;
}
