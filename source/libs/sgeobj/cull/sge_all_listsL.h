#pragma once
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
#include <cinttypes>
#include "cull/cull.h"

/* Definition of new names */

#include "sgeobj/cull/sge_job_JB_L.h"
#include "sgeobj/cull/sge_job_JRS_L.h"
#include "sgeobj/cull/sge_qinstance_QU_L.h"
#include "sgeobj/cull/sge_host_EH_L.h"
#include "sgeobj/cull/sge_answer_AN_L.h"
#include "sgeobj/cull/sge_host_HL_L.h"
#include "sgeobj/cull/sge_host_HS_L.h"
#include "sgeobj/cull/sge_event_ET_L.h"
#include "sgeobj/cull/sge_event_EV_L.h"
#include "sgeobj/cull/sge_event_EVS_L.h"
#include "sgeobj/cull/sge_centry_CE_L.h"
#include "sgeobj/cull/sge_report_LR_L.h"
#include "sgeobj/cull/sge_order_OR_L.h"
#include "sgeobj/cull/sge_order_OQ_L.h"
#include "sgeobj/cull/sge_userset_US_L.h"
#include "sgeobj/cull/sge_userset_UE_L.h"
#include "sgeobj/cull/sge_range_RN_L.h"
#include "sgeobj/cull/sge_job_PN_L.h"
#include "sgeobj/cull/sge_var_VA_L.h"
#include "sgeobj/cull/sge_mailrec_MR_L.h"
#include "sgeobj/cull/sge_pe_PE_L.h"
#include "sgeobj/cull/sge_qref_QR_L.h"
#include "sgeobj/cull/sge_userset_JC_L.h"
#include "sgeobj/cull/sge_conf_CONF_L.h"
#include "sgeobj/cull/sge_conf_CF_L.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "sgeobj/cull/sge_str_STU_L.h"
#include "sgeobj/cull/sge_job_JG_L.h"
#include "sgeobj/cull/sge_subordinate_SO_L.h"
#include "sgeobj/cull/sge_helper_QAJ_L.h"
#include "sgeobj/cull/sge_parse_SPA_L.h"
#include "sgeobj/cull/sge_report_REP_L.h"
#include "sgeobj/cull/sge_usage_UA_L.h"
#include "sgeobj/cull/sge_userprj_PR_L.h"
#include "sgeobj/cull/sge_userprj_UU_L.h"
#include "sgeobj/cull/sge_grantedres_GRU_L.h"
#include "sgeobj/cull/sge_host_RESL_L.h"
#include "sgeobj/cull/sge_sharetree_STN_L.h"
#include "sgeobj/cull/sge_schedd_conf_SC_L.h"
#include "sgeobj/cull/sge_pe_task_PET_L.h"
#include "sgeobj/cull/sge_pe_task_PETR_L.h"
#include "sgeobj/cull/sge_pe_task_FPET_L.h"
#include "sgeobj/cull/sge_report_JR_L.h"
#include "sgeobj/cull/sge_report_LIC_L.h"
#include "sgeobj/cull/sge_ptf_JL_L.h"
#include "sgeobj/cull/sge_ptf_JP_L.h"
#include "sgeobj/cull/sge_ptf_JO_L.h"
#include "sgeobj/cull/sge_userprj_UPU_L.h"
#include "sgeobj/cull/sge_ckpt_CK_L.h"
#include "sgeobj/cull/sge_path_alias_PA_L.h"
#include "sgeobj/cull/sge_job_ref_JRE_L.h"
#include "sgeobj/cull/sge_id_ID_L.h"
#include "sgeobj/cull/sge_multi_MA_L.h"
#include "sgeobj/cull/sge_time_event_TE_L.h"
#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "sgeobj/cull/sge_calendar_CA_L.h"
#include "sgeobj/cull/sge_calendar_TMR_L.h"
#include "sgeobj/cull/sge_calendar_TM_L.h"
#include "sgeobj/cull/sge_qexec_RT_L.h"
#include "sgeobj/cull/sge_userprj_UPP_L.h"
#include "sgeobj/cull/sge_message_SME_L.h"
#include "sgeobj/cull/sge_message_MES_L.h"
#include "sgeobj/cull/sge_ja_task_JAT_L.h"
#include "sgeobj/cull/sge_ct_CT_L.h"
#include "sgeobj/cull/sge_eejob_SGEJ_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"
#include "sgeobj/cull/sge_hgroup_HGRP_L.h"
#include "sgeobj/cull/sge_href_HR_L.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"
#include "sgeobj/cull/sge_loadsensor_LS_L.h"
#include "sgeobj/cull/sge_host_RU_L.h"
#include "sgeobj/cull/sge_suser_SU_L.h"
#include "sgeobj/cull/sge_spooling_SPC_L.h"
#include "sgeobj/cull/sge_spooling_SPR_L.h"
#include "sgeobj/cull/sge_spooling_SPT_L.h"
#include "sgeobj/cull/sge_spooling_SPTR_L.h"
#include "sgeobj/cull/sge_japi_JJ_L.h"
#include "sgeobj/cull/sge_japi_JJAT_L.h"
#include "sgeobj/cull/sge_japi_NSV_L.h"
#include "sgeobj/cull/sge_attr_ASTR_L.h"
#include "sgeobj/cull/sge_attr_AULNG_L.h"
#include "sgeobj/cull/sge_attr_ABOOL_L.h"
#include "sgeobj/cull/sge_attr_ATIME_L.h"
#include "sgeobj/cull/sge_attr_AMEM_L.h"
#include "sgeobj/cull/sge_attr_AINTER_L.h"
#include "sgeobj/cull/sge_attr_ASTRING_L.h"
#include "sgeobj/cull/sge_attr_ASTRLIST_L.h"
#include "sgeobj/cull/sge_attr_AUSRLIST_L.h"
#include "sgeobj/cull/sge_attr_APRJLIST_L.h"
#include "sgeobj/cull/sge_attr_ACELIST_L.h"
#include "sgeobj/cull/sge_attr_ASOLIST_L.h"
#include "sgeobj/cull/sge_attr_AQTLIST_L.h"
#include "sgeobj/cull/sge_cqueue_CQ_L.h"
#include "sgeobj/cull/sge_mesobj_QIM_L.h"
#include "sgeobj/cull/sge_eejob_FCAT_L.h"
#include "sgeobj/cull/sge_ct_CTI_L.h"
#include "sgeobj/cull/sge_schedd_conf_PARA_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLA_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLH_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLS_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/cull/sge_qeti_QETI_L.h"
#include "sgeobj/cull/sge_select_queue_LDR_L.h"
#include "sgeobj/cull/sge_select_queue_QRL_L.h"
#include "sgeobj/cull/sge_ct_CCT_L.h"
#include "sgeobj/cull/sge_calendar_CQU_L.h"
#include "sgeobj/cull/sge_ct_SCT_L.h"
#include "sgeobj/cull/sge_ct_REF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQS_L.h"
#include "sgeobj/cull/sge_resource_quota_RQR_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRL_L.h"
#include "sgeobj/cull/sge_resource_quota_RQL_L.h"
#include "sgeobj/cull/sge_advance_reservation_AR_L.h"
#include "sgeobj/cull/sge_advance_reservation_ARA_L.h"
#include "sgeobj/cull/sge_ack_ACK_L.h"
#include "sgeobj/cull/sge_event_request_EVR_L.h"
#include "sgeobj/cull/sge_jsv_JSV_L.h"
#include "sgeobj/cull/sge_order_RTIC_L.h"
#include "sgeobj/cull/sge_proc_PRO_L.h"
#include "sgeobj/cull/sge_proc_GR_L.h"
#include "sgeobj/cull/sge_binding_BN_L.h"
#include "sgeobj/cull/sge_test_TEST_L.h"
#include "sgeobj/cull/sge_pack_PACK_L.h"
#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/cull/sge_role_RL_L.h"
#include "sgeobj/cull/sge_matchcache_HM_L.h"
#include "sgeobj/cull/sge_job_CX_L.h"
#if defined(__SGE_GDI_LIBRARY_HOME_OBJECT_FILE__)

lNameSpace nmv[] = {

/*
   1. unique keq of the first element in the descriptor
   2. number of elements in the descriptor
   3. array with names describing the fields of the descriptor
   4. pointer to the descriptor
      1.              2.   3.   4.
*/

   {JB_LOWERBOUND, static_cast<int>(ocs::tom::JB::names.size()), ocs::tom::JB::names.data(), JB_Type},
   {JRS_LOWERBOUND, static_cast<int>(ocs::tom::JRS::names.size()), ocs::tom::JRS::names.data(), JRS_Type},
   {QU_LOWERBOUND, static_cast<int>(ocs::tom::QU::names.size()), ocs::tom::QU::names.data(), QU_Type},
   {EH_LOWERBOUND, static_cast<int>(ocs::tom::EH::names.size()), ocs::tom::EH::names.data(), EH_Type},
   {AN_LOWERBOUND, static_cast<int>(ocs::tom::AN::names.size()), ocs::tom::AN::names.data(), AN_Type},
   {HL_LOWERBOUND, static_cast<int>(ocs::tom::HL::names.size()), ocs::tom::HL::names.data(), HL_Type},
   {HS_LOWERBOUND, static_cast<int>(ocs::tom::HS::names.size()), ocs::tom::HS::names.data(), HS_Type},
   {ET_LOWERBOUND, static_cast<int>(ocs::tom::ET::names.size()), ocs::tom::ET::names.data(), ET_Type},
   {EV_LOWERBOUND, static_cast<int>(ocs::tom::EV::names.size()), ocs::tom::EV::names.data(), EV_Type},
   {EVS_LOWERBOUND, static_cast<int>(ocs::tom::EVS::names.size()), ocs::tom::EVS::names.data(), EVS_Type},
   {CE_LOWERBOUND, static_cast<int>(ocs::tom::CE::names.size()), ocs::tom::CE::names.data(), CE_Type},
   {LR_LOWERBOUND, static_cast<int>(ocs::tom::LR::names.size()), ocs::tom::LR::names.data(), LR_Type},
   {OR_LOWERBOUND, static_cast<int>(ocs::tom::OR::names.size()), ocs::tom::OR::names.data(), OR_Type},
   {OQ_LOWERBOUND, static_cast<int>(ocs::tom::OQ::names.size()), ocs::tom::OQ::names.data(), OQ_Type},
   {US_LOWERBOUND, static_cast<int>(ocs::tom::US::names.size()), ocs::tom::US::names.data(), US_Type},
   {UE_LOWERBOUND, static_cast<int>(ocs::tom::UE::names.size()), ocs::tom::UE::names.data(), UE_Type},
   {RN_LOWERBOUND, static_cast<int>(ocs::tom::RN::names.size()), ocs::tom::RN::names.data(), RN_Type},
   {PN_LOWERBOUND, static_cast<int>(ocs::tom::PN::names.size()), ocs::tom::PN::names.data(), PN_Type},
   {VA_LOWERBOUND, static_cast<int>(ocs::tom::VA::names.size()), ocs::tom::VA::names.data(), VA_Type},
   {MR_LOWERBOUND, static_cast<int>(ocs::tom::MR::names.size()), ocs::tom::MR::names.data(), MR_Type},
   {PE_LOWERBOUND, static_cast<int>(ocs::tom::PE::names.size()), ocs::tom::PE::names.data(), PE_Type},
   {QR_LOWERBOUND, static_cast<int>(ocs::tom::QR::names.size()), ocs::tom::QR::names.data(), QR_Type},
   {JC_LOWERBOUND, static_cast<int>(ocs::tom::JC::names.size()), ocs::tom::JC::names.data(), JC_Type},
   {CONF_LOWERBOUND, static_cast<int>(ocs::tom::CONF::names.size()), ocs::tom::CONF::names.data(), CONF_Type},
   {CF_LOWERBOUND, static_cast<int>(ocs::tom::CF::names.size()), ocs::tom::CF::names.data(), CF_Type},
   {ST_LOWERBOUND, static_cast<int>(ocs::tom::ST::names.size()), ocs::tom::ST::names.data(), ST_Type},
   {STU_LOWERBOUND, static_cast<int>(ocs::tom::STU::names.size()), ocs::tom::STU::names.data(), STU_Type},
   {JG_LOWERBOUND, static_cast<int>(ocs::tom::JG::names.size()), ocs::tom::JG::names.data(), JG_Type},
   {SO_LOWERBOUND, static_cast<int>(ocs::tom::SO::names.size()), ocs::tom::SO::names.data(), SO_Type},
   {QAJ_LOWERBOUND, static_cast<int>(ocs::tom::QAJ::names.size()), ocs::tom::QAJ::names.data(), QAJ_Type},
   {SPA_LOWERBOUND, static_cast<int>(ocs::tom::SPA::names.size()), ocs::tom::SPA::names.data(), SPA_Type},
   {REP_LOWERBOUND, static_cast<int>(ocs::tom::REP::names.size()), ocs::tom::REP::names.data(), REP_Type},
   {UA_LOWERBOUND, static_cast<int>(ocs::tom::UA::names.size()), ocs::tom::UA::names.data(), UA_Type},
   {PR_LOWERBOUND, static_cast<int>(ocs::tom::PR::names.size()), ocs::tom::PR::names.data(), PR_Type},
   {UU_LOWERBOUND, static_cast<int>(ocs::tom::UU::names.size()), ocs::tom::UU::names.data(), UU_Type},
   {GRU_LOWERBOUND, static_cast<int>(ocs::tom::GRU::names.size()), ocs::tom::GRU::names.data(), GRU_Type},
   {RESL_LOWERBOUND, static_cast<int>(ocs::tom::RESL::names.size()), ocs::tom::RESL::names.data(), RESL_Type},
   {STN_LOWERBOUND, static_cast<int>(ocs::tom::STN::names.size()), ocs::tom::STN::names.data(), STN_Type},
   {SC_LOWERBOUND, static_cast<int>(ocs::tom::SC::names.size()), ocs::tom::SC::names.data(), SC_Type},
   {PET_LOWERBOUND, static_cast<int>(ocs::tom::PET::names.size()), ocs::tom::PET::names.data(), PET_Type},
   {PETR_LOWERBOUND, static_cast<int>(ocs::tom::PETR::names.size()), ocs::tom::PETR::names.data(), PETR_Type},
   {FPET_LOWERBOUND, static_cast<int>(ocs::tom::FPET::names.size()), ocs::tom::FPET::names.data(), FPET_Type},
   {JR_LOWERBOUND, static_cast<int>(ocs::tom::JR::names.size()), ocs::tom::JR::names.data(), JR_Type},
   {LIC_LOWERBOUND, static_cast<int>(ocs::tom::LIC::names.size()), ocs::tom::LIC::names.data(), LIC_Type},
   {JL_LOWERBOUND, static_cast<int>(ocs::tom::JL::names.size()), ocs::tom::JL::names.data(), JL_Type},
   {JP_LOWERBOUND, static_cast<int>(ocs::tom::JP::names.size()), ocs::tom::JP::names.data(), JP_Type},
   {JO_LOWERBOUND, static_cast<int>(ocs::tom::JO::names.size()), ocs::tom::JO::names.data(), JO_Type},
   {UPU_LOWERBOUND, static_cast<int>(ocs::tom::UPU::names.size()), ocs::tom::UPU::names.data(), UPU_Type},
   {CK_LOWERBOUND, static_cast<int>(ocs::tom::CK::names.size()), ocs::tom::CK::names.data(), CK_Type},
   {PA_LOWERBOUND, static_cast<int>(ocs::tom::PA::names.size()), ocs::tom::PA::names.data(), PA_Type},
   {JRE_LOWERBOUND, static_cast<int>(ocs::tom::JRE::names.size()), ocs::tom::JRE::names.data(), JRE_Type},
   {ID_LOWERBOUND, static_cast<int>(ocs::tom::ID::names.size()), ocs::tom::ID::names.data(), ID_Type},
   {MA_LOWERBOUND, static_cast<int>(ocs::tom::MA::names.size()), ocs::tom::MA::names.data(), MA_Type},
   {TE_LOWERBOUND, static_cast<int>(ocs::tom::TE::names.size()), ocs::tom::TE::names.data(), TE_Type},
   {CAL_LOWERBOUND, static_cast<int>(ocs::tom::CAL::names.size()), ocs::tom::CAL::names.data(), CAL_Type},
   {CA_LOWERBOUND, static_cast<int>(ocs::tom::CA::names.size()), ocs::tom::CA::names.data(), CA_Type},
   {TMR_LOWERBOUND, static_cast<int>(ocs::tom::TMR::names.size()), ocs::tom::TMR::names.data(), TMR_Type},
   {TM_LOWERBOUND, static_cast<int>(ocs::tom::TM::names.size()), ocs::tom::TM::names.data(), TM_Type},
   {RT_LOWERBOUND, static_cast<int>(ocs::tom::RT::names.size()), ocs::tom::RT::names.data(), RT_Type},
   {UPP_LOWERBOUND, static_cast<int>(ocs::tom::UPP::names.size()), ocs::tom::UPP::names.data(), UPP_Type},
   {SME_LOWERBOUND, static_cast<int>(ocs::tom::SME::names.size()), ocs::tom::SME::names.data(), SME_Type},
   {MES_LOWERBOUND, static_cast<int>(ocs::tom::MES::names.size()), ocs::tom::MES::names.data(), MES_Type},
   {JAT_LOWERBOUND, static_cast<int>(ocs::tom::JAT::names.size()), ocs::tom::JAT::names.data(), JAT_Type},
   {CT_LOWERBOUND, static_cast<int>(ocs::tom::CT::names.size()), ocs::tom::CT::names.data(), CT_Type},
   {SGEJ_LOWERBOUND, static_cast<int>(ocs::tom::SGEJ::names.size()), ocs::tom::SGEJ::names.data(), SGEJ_Type},
   {ULNG_LOWERBOUND, static_cast<int>(ocs::tom::ULNG::names.size()), ocs::tom::ULNG::names.data(), ULNG_Type},
   {HGRP_LOWERBOUND, static_cast<int>(ocs::tom::HGRP::names.size()), ocs::tom::HGRP::names.data(), HGRP_Type},
   {HR_LOWERBOUND, static_cast<int>(ocs::tom::HR::names.size()), ocs::tom::HR::names.data(), HR_Type},
   {PERM_LOWERBOUND, static_cast<int>(ocs::tom::PERM::names.size()), ocs::tom::PERM::names.data(), PERM_Type},
   {LS_LOWERBOUND, static_cast<int>(ocs::tom::LS::names.size()), ocs::tom::LS::names.data(), LS_Type},
   {RU_LOWERBOUND, static_cast<int>(ocs::tom::RU::names.size()), ocs::tom::RU::names.data(), RU_Type},
   {SU_LOWERBOUND, static_cast<int>(ocs::tom::SU::names.size()), ocs::tom::SU::names.data(), SU_Type},
   {SPC_LOWERBOUND, static_cast<int>(ocs::tom::SPC::names.size()), ocs::tom::SPC::names.data(), SPC_Type},
   {SPR_LOWERBOUND, static_cast<int>(ocs::tom::SPR::names.size()), ocs::tom::SPR::names.data(), SPR_Type},
   {SPT_LOWERBOUND, static_cast<int>(ocs::tom::SPT::names.size()), ocs::tom::SPT::names.data(), SPT_Type},
   {SPTR_LOWERBOUND, static_cast<int>(ocs::tom::SPTR::names.size()), ocs::tom::SPTR::names.data(), SPTR_Type},
   {JJ_LOWERBOUND, static_cast<int>(ocs::tom::JJ::names.size()), ocs::tom::JJ::names.data(), JJ_Type},
   {JJAT_LOWERBOUND, static_cast<int>(ocs::tom::JJAT::names.size()), ocs::tom::JJAT::names.data(), JJAT_Type},
   {NSV_LOWERBOUND, static_cast<int>(ocs::tom::NSV::names.size()), ocs::tom::NSV::names.data(), NSV_Type},
   {ASTR_LOWERBOUND, static_cast<int>(ocs::tom::ASTR::names.size()), ocs::tom::ASTR::names.data(), ASTR_Type},
   {AULNG_LOWERBOUND, static_cast<int>(ocs::tom::AULNG::names.size()), ocs::tom::AULNG::names.data(), AULNG_Type},
   {ABOOL_LOWERBOUND, static_cast<int>(ocs::tom::ABOOL::names.size()), ocs::tom::ABOOL::names.data(), ABOOL_Type},
   {ATIME_LOWERBOUND, static_cast<int>(ocs::tom::ATIME::names.size()), ocs::tom::ATIME::names.data(), ATIME_Type},
   {AMEM_LOWERBOUND, static_cast<int>(ocs::tom::AMEM::names.size()), ocs::tom::AMEM::names.data(), AMEM_Type},
   {AINTER_LOWERBOUND, static_cast<int>(ocs::tom::AINTER::names.size()), ocs::tom::AINTER::names.data(), AINTER_Type},
   {ASTRING_LOWERBOUND, static_cast<int>(ocs::tom::ASTRING::names.size()), ocs::tom::ASTRING::names.data(), ASTRING_Type},
   {ASTRLIST_LOWERBOUND, static_cast<int>(ocs::tom::ASTRLIST::names.size()), ocs::tom::ASTRLIST::names.data(), ASTRLIST_Type},
   {AUSRLIST_LOWERBOUND, static_cast<int>(ocs::tom::AUSRLIST::names.size()), ocs::tom::AUSRLIST::names.data(), AUSRLIST_Type},
   {APRJLIST_LOWERBOUND, static_cast<int>(ocs::tom::APRJLIST::names.size()), ocs::tom::APRJLIST::names.data(), APRJLIST_Type},
   {ACELIST_LOWERBOUND, static_cast<int>(ocs::tom::ACELIST::names.size()), ocs::tom::ACELIST::names.data(), ACELIST_Type},
   {ASOLIST_LOWERBOUND, static_cast<int>(ocs::tom::ASOLIST::names.size()), ocs::tom::ASOLIST::names.data(), ASOLIST_Type},
   {AQTLIST_LOWERBOUND, static_cast<int>(ocs::tom::AQTLIST::names.size()), ocs::tom::AQTLIST::names.data(), AQTLIST_Type},
   {CQ_LOWERBOUND, static_cast<int>(ocs::tom::CQ::names.size()), ocs::tom::CQ::names.data(), CQ_Type},
   {QIM_LOWERBOUND, static_cast<int>(ocs::tom::QIM::names.size()), ocs::tom::QIM::names.data(), QIM_Type},
   {FCAT_LOWERBOUND, static_cast<int>(ocs::tom::FCAT::names.size()), ocs::tom::FCAT::names.data(), FCAT_Type},
   {CTI_LOWERBOUND, static_cast<int>(ocs::tom::CTI::names.size()), ocs::tom::CTI::names.data(), CTI_Type},
   {PARA_LOWERBOUND, static_cast<int>(ocs::tom::PARA::names.size()), ocs::tom::PARA::names.data(), PARA_Type},
   {XMLA_LOWERBOUND, static_cast<int>(ocs::tom::XMLA::names.size()), ocs::tom::XMLA::names.data(), XMLA_Type},
   {XMLH_LOWERBOUND, static_cast<int>(ocs::tom::XMLH::names.size()), ocs::tom::XMLH::names.data(), XMLH_Type},
   {XMLS_LOWERBOUND, static_cast<int>(ocs::tom::XMLS::names.size()), ocs::tom::XMLS::names.data(), XMLS_Type},
   {XMLE_LOWERBOUND, static_cast<int>(ocs::tom::XMLE::names.size()), ocs::tom::XMLE::names.data(), XMLE_Type},
   {RDE_LOWERBOUND, static_cast<int>(ocs::tom::RDE::names.size()), ocs::tom::RDE::names.data(), RDE_Type},
   {RUE_LOWERBOUND, static_cast<int>(ocs::tom::RUE::names.size()), ocs::tom::RUE::names.data(), RUE_Type},
   {QETI_LOWERBOUND, static_cast<int>(ocs::tom::QETI::names.size()), ocs::tom::QETI::names.data(), QETI_Type},
   {LDR_LOWERBOUND, static_cast<int>(ocs::tom::LDR::names.size()), ocs::tom::LDR::names.data(), LDR_Type},
   {QRL_LOWERBOUND, static_cast<int>(ocs::tom::QRL::names.size()), ocs::tom::QRL::names.data(), QRL_Type},
   {CCT_LOWERBOUND, static_cast<int>(ocs::tom::CCT::names.size()), ocs::tom::CCT::names.data(), CCT_Type},
   {CQU_LOWERBOUND, static_cast<int>(ocs::tom::CQU::names.size()), ocs::tom::CQU::names.data(), CQU_Type},
   {SCT_LOWERBOUND, static_cast<int>(ocs::tom::SCT::names.size()), ocs::tom::SCT::names.data(), SCT_Type},
   {REF_LOWERBOUND, static_cast<int>(ocs::tom::REF::names.size()), ocs::tom::REF::names.data(), REF_Type},
   {RQS_LOWERBOUND, static_cast<int>(ocs::tom::RQS::names.size()), ocs::tom::RQS::names.data(), RQS_Type},
   {RQR_LOWERBOUND, static_cast<int>(ocs::tom::RQR::names.size()), ocs::tom::RQR::names.data(), RQR_Type},
   {RQRF_LOWERBOUND, static_cast<int>(ocs::tom::RQRF::names.size()), ocs::tom::RQRF::names.data(), RQRF_Type},
   {RQRL_LOWERBOUND, static_cast<int>(ocs::tom::RQRL::names.size()), ocs::tom::RQRL::names.data(), RQRL_Type},
   {RQL_LOWERBOUND, static_cast<int>(ocs::tom::RQL::names.size()), ocs::tom::RQL::names.data(), RQL_Type},
   {AR_LOWERBOUND, static_cast<int>(ocs::tom::AR::names.size()), ocs::tom::AR::names.data(), AR_Type},
   {ARA_LOWERBOUND, static_cast<int>(ocs::tom::ARA::names.size()), ocs::tom::ARA::names.data(), ARA_Type},
   {ACK_LOWERBOUND, static_cast<int>(ocs::tom::ACK::names.size()), ocs::tom::ACK::names.data(), ACK_Type},
   {EVR_LOWERBOUND, static_cast<int>(ocs::tom::EVR::names.size()), ocs::tom::EVR::names.data(), EVR_Type},
   {JSV_LOWERBOUND, static_cast<int>(ocs::tom::JSV::names.size()), ocs::tom::JSV::names.data(), JSV_Type},
   {RTIC_LOWERBOUND, static_cast<int>(ocs::tom::RTIC::names.size()), ocs::tom::RTIC::names.data(), RTIC_Type},
   {PRO_LOWERBOUND, static_cast<int>(ocs::tom::PRO::names.size()), ocs::tom::PRO::names.data(), PRO_Type},
   {GR_LOWERBOUND, static_cast<int>(ocs::tom::GR::names.size()), ocs::tom::GR::names.data(), GR_Type},
   {BN_LOWERBOUND, static_cast<int>(ocs::tom::BN::names.size()), ocs::tom::BN::names.data(), BN_Type},
   {TEST_LOWERBOUND, static_cast<int>(ocs::tom::TEST::names.size()), ocs::tom::TEST::names.data(), TEST_Type},
   {PACK_LOWERBOUND, static_cast<int>(ocs::tom::PACK::names.size()), ocs::tom::PACK::names.data(), PACK_Type},
   {SPP_LOWERBOUND, static_cast<int>(ocs::tom::SPP::names.size()), ocs::tom::SPP::names.data(), SPP_Type},
   {RL_LOWERBOUND, static_cast<int>(ocs::tom::RL::names.size()), ocs::tom::RL::names.data(), RL_Type},
   {HM_LOWERBOUND, static_cast<int>(ocs::tom::HM::names.size()), ocs::tom::HM::names.data(), HM_Type},
   {CX_LOWERBOUND, static_cast<int>(ocs::tom::CX::names.size()), ocs::tom::CX::names.data(), CX_Type},
   {0, 0, nullptr, nullptr}
};

#else

extern lNameSpace nmv[];

#endif /* __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__ */

