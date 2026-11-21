/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/* auto generated file */
#include <cstddef>
#include "tom/ocs_Types.h"
#include "sgeobj/cull/sge_all_listsL.h"

const lDescr *
object_get_subtype(int nm)
{
   const lDescr *ret = nullptr;
   switch (nm) {
      case JB_jid_request_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_jid_predecessor_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_jid_successor_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_ja_ad_request_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_ja_ad_predecessor_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_ja_ad_successor_list:
         ret = ocs::tom::JRE::descriptor.e;
         break;
      case JB_shell_list:
         ret = ocs::tom::PN::descriptor.e;
         break;
      case JB_env_list:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case JB_context:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case JB_job_args:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case JB_checkpoint_object:
         ret = ocs::tom::CK::descriptor.e;
         break;
      case JB_stdout_path_list:
         ret = ocs::tom::PN::descriptor.e;
         break;
      case JB_stderr_path_list:
         ret = ocs::tom::PN::descriptor.e;
         break;
      case JB_stdin_path_list:
         ret = ocs::tom::PN::descriptor.e;
         break;
      case JB_request_set_list:
         ret = ocs::tom::JRS::descriptor.e;
         break;
      case JB_mail_list:
         ret = ocs::tom::MR::descriptor.e;
         break;
      case JB_pe_range:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_structure:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_n_h_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_u_h_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_s_h_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_o_h_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_a_h_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JB_ja_template:
         ret = ocs::tom::JAT::descriptor.e;
         break;
      case JB_ja_tasks:
         ret = ocs::tom::JAT::descriptor.e;
         break;
      case JB_user_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case JB_job_identifier_list:
         ret = ocs::tom::ID::descriptor.e;
         break;
      case JB_path_aliases:
         ret = ocs::tom::PA::descriptor.e;
         break;
      case JB_binding:
         ret = ocs::tom::BN::descriptor.e;
         break;
      case JB_grp_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case JB_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case JB_context_change:
         ret = ocs::tom::CX::descriptor.e;
         break;
      case JRS_hard_resource_list:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case JRS_soft_resource_list:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case JRS_hard_queue_list:
         ret = ocs::tom::QR::descriptor.e;
         break;
      case JRS_soft_queue_list:
         ret = ocs::tom::QR::descriptor.e;
         break;
      case QU_ckpt_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case QU_pe_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case QU_owner_list:
         ret = ocs::tom::US::descriptor.e;
         break;
      case QU_acl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case QU_xacl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case QU_projects:
         ret = ocs::tom::PR::descriptor.e;
         break;
      case QU_xprojects:
         ret = ocs::tom::PR::descriptor.e;
         break;
      case QU_consumable_config_list:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case QU_load_thresholds:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case QU_suspend_thresholds:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case QU_subordinate_list:
         ret = ocs::tom::SO::descriptor.e;
         break;
      case QU_resource_utilization:
         ret = ocs::tom::RUE::descriptor.e;
         break;
      case QU_message_list:
         ret = ocs::tom::QIM::descriptor.e;
         break;
      case QU_state_changes:
         ret = ocs::tom::CQU::descriptor.e;
         break;
      case QU_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case EH_scaling_list:
         ret = ocs::tom::HS::descriptor.e;
         break;
      case EH_consumable_config_list:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case EH_usage_scaling_list:
         ret = ocs::tom::HS::descriptor.e;
         break;
      case EH_load_list:
         ret = ocs::tom::HL::descriptor.e;
         break;
      case EH_acl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case EH_xacl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case EH_prj:
         ret = ocs::tom::PR::descriptor.e;
         break;
      case EH_xprj:
         ret = ocs::tom::PR::descriptor.e;
         break;
      case EH_resource_utilization:
         ret = ocs::tom::RUE::descriptor.e;
         break;
      case EH_cached_complexes:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case EH_reschedule_unknown_list:
         ret = ocs::tom::RU::descriptor.e;
         break;
      case EH_report_variables:
         ret = ocs::tom::STU::descriptor.e;
         break;
      case EH_merged_report_variables:
         ret = ocs::tom::STU::descriptor.e;
         break;
      case EH_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case EV_subscribed:
         ret = ocs::tom::EVS::descriptor.e;
         break;
      case EV_events:
         ret = ocs::tom::ET::descriptor.e;
         break;
      case EVS_what:
         ret = ocs::tom::PACK::descriptor.e;
         break;
      case EVS_where:
         ret = ocs::tom::PACK::descriptor.e;
         break;
      case CE_resource_map_list:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case OR_queuelist:
         ret = ocs::tom::OQ::descriptor.e;
         break;
      case OR_granted_resources_list:
         ret = ocs::tom::GRU::descriptor.e;
         break;
      case OQ_binding_to_use:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case OQ_granted_rsmaps:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case US_entries:
         ret = ocs::tom::UE::descriptor.e;
         break;
      case US_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case PE_user_list:
         ret = ocs::tom::US::descriptor.e;
         break;
      case PE_xuser_list:
         ret = ocs::tom::US::descriptor.e;
         break;
      case PE_resource_utilization:
         ret = ocs::tom::RUE::descriptor.e;
         break;
      case PE_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case CONF_entries:
         ret = ocs::tom::CF::descriptor.e;
         break;
      case JG_binding_to_use:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case JG_granted_rsmaps:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case PR_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PR_long_term_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PR_project:
         ret = ocs::tom::UPP::descriptor.e;
         break;
      case PR_acl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case PR_xacl:
         ret = ocs::tom::US::descriptor.e;
         break;
      case PR_debited_job_usage:
         ret = ocs::tom::UPU::descriptor.e;
         break;
      case PR_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case UU_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case UU_long_term_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case UU_project:
         ret = ocs::tom::UPP::descriptor.e;
         break;
      case UU_debited_job_usage:
         ret = ocs::tom::UPU::descriptor.e;
         break;
      case UU_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case GRU_resource_map_list:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case GRU_binding_inuse:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case RESL_properties:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case STN_children:
         ret = ocs::tom::STN::descriptor.e;
         break;
      case STN_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case STN_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case SC_job_load_adjustments:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case SC_usage_weight_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PET_granted_destin_identifier_list:
         ret = ocs::tom::JG::descriptor.e;
         break;
      case PET_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PET_scaled_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PET_reported_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PET_previous_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case PET_path_aliases:
         ret = ocs::tom::PA::descriptor.e;
         break;
      case PET_environment:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case PET_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case PETR_path_aliases:
         ret = ocs::tom::PA::descriptor.e;
         break;
      case PETR_environment:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case JR_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JL_OS_job_list:
         ret = ocs::tom::JO::descriptor.e;
         break;
      case JO_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JO_pid_list:
         ret = ocs::tom::JP::descriptor.e;
         break;
      case UPU_old_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case CK_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case ID_ja_structure:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case ID_user_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case MA_answers:
         ret = ocs::tom::AN::descriptor.e;
         break;
      case CAL_parsed_year_calendar:
         ret = ocs::tom::CA::descriptor.e;
         break;
      case CAL_parsed_week_calendar:
         ret = ocs::tom::CA::descriptor.e;
         break;
      case CAL_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case CA_yday_range_list:
         ret = ocs::tom::TMR::descriptor.e;
         break;
      case CA_wday_range_list:
         ret = ocs::tom::TMR::descriptor.e;
         break;
      case CA_daytime_range_list:
         ret = ocs::tom::TMR::descriptor.e;
         break;
      case TMR_begin:
         ret = ocs::tom::TM::descriptor.e;
         break;
      case TMR_end:
         ret = ocs::tom::TM::descriptor.e;
         break;
      case UPP_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case UPP_long_term_usage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case SME_message_list:
         ret = ocs::tom::MES::descriptor.e;
         break;
      case SME_global_message_list:
         ret = ocs::tom::MES::descriptor.e;
         break;
      case MES_job_number_list:
         ret = ocs::tom::ULNG::descriptor.e;
         break;
      case JAT_granted_destin_identifier_list:
         ret = ocs::tom::JG::descriptor.e;
         break;
      case JAT_granted_resources_list:
         ret = ocs::tom::GRU::descriptor.e;
         break;
      case JAT_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JAT_scaled_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JAT_reported_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JAT_task_list:
         ret = ocs::tom::PET::descriptor.e;
         break;
      case JAT_finished_task_list:
         ret = ocs::tom::FPET::descriptor.e;
         break;
      case JAT_previous_usage_list:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case JAT_pe_object:
         ret = ocs::tom::PE::descriptor.e;
         break;
      case JAT_message_list:
         ret = ocs::tom::QIM::descriptor.e;
         break;
      case JAT_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case CT_cache:
         ret = ocs::tom::CCT::descriptor.e;
         break;
      case HGRP_host_list:
         ret = ocs::tom::HR::descriptor.e;
         break;
      case HGRP_cqueue_list:
         ret = ocs::tom::CQ::descriptor.e;
         break;
      case HGRP_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case HGRP_cached_hosts:
         ret = ocs::tom::HR::descriptor.e;
         break;
      case HGRP_match_cache:
         ret = ocs::tom::HM::descriptor.e;
         break;
      case LS_incomplete:
         ret = ocs::tom::LR::descriptor.e;
         break;
      case LS_complete:
         ret = ocs::tom::LR::descriptor.e;
         break;
      case SPC_rules:
         ret = ocs::tom::SPR::descriptor.e;
         break;
      case SPC_types:
         ret = ocs::tom::SPT::descriptor.e;
         break;
      case SPT_rules:
         ret = ocs::tom::SPTR::descriptor.e;
         break;
      case JJ_finished_tasks:
         ret = ocs::tom::JJAT::descriptor.e;
         break;
      case JJ_not_yet_finished_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JJ_started_task_ids:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case JJAT_rusage:
         ret = ocs::tom::UA::descriptor.e;
         break;
      case NSV_strings:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case ASTRLIST_value:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case AUSRLIST_value:
         ret = ocs::tom::US::descriptor.e;
         break;
      case APRJLIST_value:
         ret = ocs::tom::PR::descriptor.e;
         break;
      case ACELIST_value:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case ASOLIST_value:
         ret = ocs::tom::SO::descriptor.e;
         break;
      case CQ_hostlist:
         ret = ocs::tom::HR::descriptor.e;
         break;
      case CQ_qinstances:
         ret = ocs::tom::QU::descriptor.e;
         break;
      case CQ_seq_no:
         ret = ocs::tom::AULNG::descriptor.e;
         break;
      case CQ_nsuspend:
         ret = ocs::tom::AULNG::descriptor.e;
         break;
      case CQ_job_slots:
         ret = ocs::tom::AULNG::descriptor.e;
         break;
      case CQ_rerun:
         ret = ocs::tom::ABOOL::descriptor.e;
         break;
      case CQ_s_fsize:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_fsize:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_data:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_data:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_stack:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_stack:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_core:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_core:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_rss:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_rss:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_vmem:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_h_vmem:
         ret = ocs::tom::AMEM::descriptor.e;
         break;
      case CQ_s_rt:
         ret = ocs::tom::ATIME::descriptor.e;
         break;
      case CQ_h_rt:
         ret = ocs::tom::ATIME::descriptor.e;
         break;
      case CQ_s_cpu:
         ret = ocs::tom::ATIME::descriptor.e;
         break;
      case CQ_h_cpu:
         ret = ocs::tom::ATIME::descriptor.e;
         break;
      case CQ_suspend_interval:
         ret = ocs::tom::AINTER::descriptor.e;
         break;
      case CQ_min_cpu_interval:
         ret = ocs::tom::AINTER::descriptor.e;
         break;
      case CQ_notify:
         ret = ocs::tom::AINTER::descriptor.e;
         break;
      case CQ_tmpdir:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_shell:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_calendar:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_priority:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_processors:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_prolog:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_epilog:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_shell_start_mode:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_starter_method:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_suspend_method:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_resume_method:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_terminate_method:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_initial_state:
         ret = ocs::tom::ASTR::descriptor.e;
         break;
      case CQ_pe_list:
         ret = ocs::tom::ASTRLIST::descriptor.e;
         break;
      case CQ_ckpt_list:
         ret = ocs::tom::ASTRLIST::descriptor.e;
         break;
      case CQ_owner_list:
         ret = ocs::tom::AUSRLIST::descriptor.e;
         break;
      case CQ_acl:
         ret = ocs::tom::AUSRLIST::descriptor.e;
         break;
      case CQ_xacl:
         ret = ocs::tom::AUSRLIST::descriptor.e;
         break;
      case CQ_projects:
         ret = ocs::tom::APRJLIST::descriptor.e;
         break;
      case CQ_xprojects:
         ret = ocs::tom::APRJLIST::descriptor.e;
         break;
      case CQ_load_thresholds:
         ret = ocs::tom::ACELIST::descriptor.e;
         break;
      case CQ_suspend_thresholds:
         ret = ocs::tom::ACELIST::descriptor.e;
         break;
      case CQ_consumable_config_list:
         ret = ocs::tom::ACELIST::descriptor.e;
         break;
      case CQ_subordinate_list:
         ret = ocs::tom::ASOLIST::descriptor.e;
         break;
      case CQ_qtype:
         ret = ocs::tom::AQTLIST::descriptor.e;
         break;
      case CQ_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case XMLH_Stylesheet:
         ret = ocs::tom::XMLS::descriptor.e;
         break;
      case XMLH_Attribute:
         ret = ocs::tom::XMLA::descriptor.e;
         break;
      case XMLE_Attribute:
         ret = ocs::tom::XMLA::descriptor.e;
         break;
      case RDE_resource_map_list:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case RUE_utilized_now_resource_map_list:
         ret = ocs::tom::RESL::descriptor.e;
         break;
      case RUE_utilized:
         ret = ocs::tom::RDE::descriptor.e;
         break;
      case RUE_utilized_nonexclusive:
         ret = ocs::tom::RDE::descriptor.e;
         break;
      case LDR_queue_ref_list:
         ret = ocs::tom::QRL::descriptor.e;
         break;
      case CCT_ignore_queues:
         ret = ocs::tom::CTI::descriptor.e;
         break;
      case CCT_ignore_hosts:
         ret = ocs::tom::CTI::descriptor.e;
         break;
      case CCT_job_messages:
         ret = ocs::tom::MES::descriptor.e;
         break;
      case SCT_job_pending_ref:
         ret = ocs::tom::REF::descriptor.e;
         break;
      case SCT_job_ref:
         ret = ocs::tom::REF::descriptor.e;
         break;
      case RQS_rule:
         ret = ocs::tom::RQR::descriptor.e;
         break;
      case RQS_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case RQR_filter_users:
         ret = ocs::tom::RQRF::descriptor.e;
         break;
      case RQR_filter_projects:
         ret = ocs::tom::RQRF::descriptor.e;
         break;
      case RQR_filter_pes:
         ret = ocs::tom::RQRF::descriptor.e;
         break;
      case RQR_filter_queues:
         ret = ocs::tom::RQRF::descriptor.e;
         break;
      case RQR_filter_hosts:
         ret = ocs::tom::RQRF::descriptor.e;
         break;
      case RQR_limit:
         ret = ocs::tom::RQRL::descriptor.e;
         break;
      case RQRF_scope:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case RQRF_xscope:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case RQRL_usage:
         ret = ocs::tom::RUE::descriptor.e;
         break;
      case AR_resource_list:
         ret = ocs::tom::CE::descriptor.e;
         break;
      case AR_resource_utilization:
         ret = ocs::tom::RUE::descriptor.e;
         break;
      case AR_queue_list:
         ret = ocs::tom::QR::descriptor.e;
         break;
      case AR_granted_slots:
         ret = ocs::tom::JG::descriptor.e;
         break;
      case AR_reserved_hosts:
         ret = ocs::tom::EH::descriptor.e;
         break;
      case AR_reserved_queues:
         ret = ocs::tom::QU::descriptor.e;
         break;
      case AR_mail_list:
         ret = ocs::tom::MR::descriptor.e;
         break;
      case AR_pe_range:
         ret = ocs::tom::RN::descriptor.e;
         break;
      case AR_master_queue_list:
         ret = ocs::tom::QR::descriptor.e;
         break;
      case AR_acl_list:
         ret = ocs::tom::ARA::descriptor.e;
         break;
      case AR_xacl_list:
         ret = ocs::tom::ARA::descriptor.e;
         break;
      case AR_request_set_list:
         ret = ocs::tom::JRS::descriptor.e;
         break;
      case AR_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case AR_granted_resources_list:
         ret = ocs::tom::GRU::descriptor.e;
         break;
      case AR_binding:
         ret = ocs::tom::BN::descriptor.e;
         break;
      case AR_pe_object:
         ret = ocs::tom::PE::descriptor.e;
         break;
      case EVR_event_client:
         ret = ocs::tom::EV::descriptor.e;
         break;
      case EVR_event_list:
         ret = ocs::tom::ET::descriptor.e;
         break;
      case RTIC_tickets:
         ret = ocs::tom::OR::descriptor.e;
         break;
      case PRO_groups:
         ret = ocs::tom::GR::descriptor.e;
         break;
      case BN_specific_binding_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case BN_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
      case TEST_obj_jb:
         ret = ocs::tom::JB::descriptor.e;
         break;
      case RL_user_list:
         ret = ocs::tom::US::descriptor.e;
         break;
      case RL_parent_role_list:
         ret = ocs::tom::ST::descriptor.e;
         break;
      case RL_joker:
         ret = ocs::tom::VA::descriptor.e;
         break;
   }
   return ret;
}
