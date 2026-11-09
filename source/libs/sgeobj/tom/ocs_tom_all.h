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

/*
 * This code was generated from file source/libs/sgeobj/json/all_types.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Every object type of the typed object model, and lists of their tables and sub-types
 */

#include <iterator>

#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_ET.h"
#include "sgeobj/tom/ocs_tom_EV.h"
#include "sgeobj/tom/ocs_tom_EVS.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_OQ.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_JC.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_QAJ.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_REP.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PETR.h"
#include "sgeobj/tom/ocs_tom_FPET.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_LIC.h"
#include "sgeobj/tom/ocs_tom_JL.h"
#include "sgeobj/tom/ocs_tom_JP.h"
#include "sgeobj/tom/ocs_tom_JO.h"
#include "sgeobj/tom/ocs_tom_UPU.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_MA.h"
#include "sgeobj/tom/ocs_tom_TE.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CA.h"
#include "sgeobj/tom/ocs_tom_TMR.h"
#include "sgeobj/tom/ocs_tom_TM.h"
#include "sgeobj/tom/ocs_tom_RT.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_SGEJ.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_PERM.h"
#include "sgeobj/tom/ocs_tom_LS.h"
#include "sgeobj/tom/ocs_tom_RU.h"
#include "sgeobj/tom/ocs_tom_SU.h"
#include "sgeobj/tom/ocs_tom_SPC.h"
#include "sgeobj/tom/ocs_tom_SPR.h"
#include "sgeobj/tom/ocs_tom_SPT.h"
#include "sgeobj/tom/ocs_tom_SPTR.h"
#include "sgeobj/tom/ocs_tom_JJ.h"
#include "sgeobj/tom/ocs_tom_JJAT.h"
#include "sgeobj/tom/ocs_tom_NSV.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_ASTRING.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_FCAT.h"
#include "sgeobj/tom/ocs_tom_CTI.h"
#include "sgeobj/tom/ocs_tom_PARA.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLH.h"
#include "sgeobj/tom/ocs_tom_XMLS.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_QETI.h"
#include "sgeobj/tom/ocs_tom_LDR.h"
#include "sgeobj/tom/ocs_tom_QRL.h"
#include "sgeobj/tom/ocs_tom_CCT.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "sgeobj/tom/ocs_tom_SCT.h"
#include "sgeobj/tom/ocs_tom_REF.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQL.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_ACK.h"
#include "sgeobj/tom/ocs_tom_EVR.h"
#include "sgeobj/tom/ocs_tom_JSV.h"
#include "sgeobj/tom/ocs_tom_RTIC.h"
#include "sgeobj/tom/ocs_tom_PRO.h"
#include "sgeobj/tom/ocs_tom_GR.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_TEST.h"
#include "sgeobj/tom/ocs_tom_PACK.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_HM.h"
#include "sgeobj/tom/ocs_tom_CX.h"

namespace ocs::tom {

   /** @brief The field table of every object type, in the order of all_types.json */
   inline constexpr TypeTable all_type_tables[] = {
      {"JB", JB::descriptor.e, JB::fields, std::size(JB::fields)},
      {"JRS", JRS::descriptor.e, JRS::fields, std::size(JRS::fields)},
      {"QU", QU::descriptor.e, QU::fields, std::size(QU::fields)},
      {"EH", EH::descriptor.e, EH::fields, std::size(EH::fields)},
      {"AN", AN::descriptor.e, AN::fields, std::size(AN::fields)},
      {"HL", HL::descriptor.e, HL::fields, std::size(HL::fields)},
      {"HS", HS::descriptor.e, HS::fields, std::size(HS::fields)},
      {"ET", ET::descriptor.e, ET::fields, std::size(ET::fields)},
      {"EV", EV::descriptor.e, EV::fields, std::size(EV::fields)},
      {"EVS", EVS::descriptor.e, EVS::fields, std::size(EVS::fields)},
      {"CE", CE::descriptor.e, CE::fields, std::size(CE::fields)},
      {"LR", LR::descriptor.e, LR::fields, std::size(LR::fields)},
      {"OR", OR::descriptor.e, OR::fields, std::size(OR::fields)},
      {"OQ", OQ::descriptor.e, OQ::fields, std::size(OQ::fields)},
      {"US", US::descriptor.e, US::fields, std::size(US::fields)},
      {"UE", UE::descriptor.e, UE::fields, std::size(UE::fields)},
      {"RN", RN::descriptor.e, RN::fields, std::size(RN::fields)},
      {"PN", PN::descriptor.e, PN::fields, std::size(PN::fields)},
      {"VA", VA::descriptor.e, VA::fields, std::size(VA::fields)},
      {"MR", MR::descriptor.e, MR::fields, std::size(MR::fields)},
      {"PE", PE::descriptor.e, PE::fields, std::size(PE::fields)},
      {"QR", QR::descriptor.e, QR::fields, std::size(QR::fields)},
      {"JC", JC::descriptor.e, JC::fields, std::size(JC::fields)},
      {"CONF", CONF::descriptor.e, CONF::fields, std::size(CONF::fields)},
      {"CF", CF::descriptor.e, CF::fields, std::size(CF::fields)},
      {"ST", ST::descriptor.e, ST::fields, std::size(ST::fields)},
      {"STU", STU::descriptor.e, STU::fields, std::size(STU::fields)},
      {"JG", JG::descriptor.e, JG::fields, std::size(JG::fields)},
      {"SO", SO::descriptor.e, SO::fields, std::size(SO::fields)},
      {"QAJ", QAJ::descriptor.e, QAJ::fields, std::size(QAJ::fields)},
      {"SPA", SPA::descriptor.e, SPA::fields, std::size(SPA::fields)},
      {"REP", REP::descriptor.e, REP::fields, std::size(REP::fields)},
      {"UA", UA::descriptor.e, UA::fields, std::size(UA::fields)},
      {"PR", PR::descriptor.e, PR::fields, std::size(PR::fields)},
      {"UU", UU::descriptor.e, UU::fields, std::size(UU::fields)},
      {"GRU", GRU::descriptor.e, GRU::fields, std::size(GRU::fields)},
      {"RESL", RESL::descriptor.e, RESL::fields, std::size(RESL::fields)},
      {"STN", STN::descriptor.e, STN::fields, std::size(STN::fields)},
      {"SC", SC::descriptor.e, SC::fields, std::size(SC::fields)},
      {"PET", PET::descriptor.e, PET::fields, std::size(PET::fields)},
      {"PETR", PETR::descriptor.e, PETR::fields, std::size(PETR::fields)},
      {"FPET", FPET::descriptor.e, FPET::fields, std::size(FPET::fields)},
      {"JR", JR::descriptor.e, JR::fields, std::size(JR::fields)},
      {"LIC", LIC::descriptor.e, LIC::fields, std::size(LIC::fields)},
      {"JL", JL::descriptor.e, JL::fields, std::size(JL::fields)},
      {"JP", JP::descriptor.e, JP::fields, std::size(JP::fields)},
      {"JO", JO::descriptor.e, JO::fields, std::size(JO::fields)},
      {"UPU", UPU::descriptor.e, UPU::fields, std::size(UPU::fields)},
      {"CK", CK::descriptor.e, CK::fields, std::size(CK::fields)},
      {"PA", PA::descriptor.e, PA::fields, std::size(PA::fields)},
      {"JRE", JRE::descriptor.e, JRE::fields, std::size(JRE::fields)},
      {"ID", ID::descriptor.e, ID::fields, std::size(ID::fields)},
      {"MA", MA::descriptor.e, MA::fields, std::size(MA::fields)},
      {"TE", TE::descriptor.e, TE::fields, std::size(TE::fields)},
      {"CAL", CAL::descriptor.e, CAL::fields, std::size(CAL::fields)},
      {"CA", CA::descriptor.e, CA::fields, std::size(CA::fields)},
      {"TMR", TMR::descriptor.e, TMR::fields, std::size(TMR::fields)},
      {"TM", TM::descriptor.e, TM::fields, std::size(TM::fields)},
      {"RT", RT::descriptor.e, RT::fields, std::size(RT::fields)},
      {"UPP", UPP::descriptor.e, UPP::fields, std::size(UPP::fields)},
      {"SME", SME::descriptor.e, SME::fields, std::size(SME::fields)},
      {"MES", MES::descriptor.e, MES::fields, std::size(MES::fields)},
      {"JAT", JAT::descriptor.e, JAT::fields, std::size(JAT::fields)},
      {"CT", CT::descriptor.e, CT::fields, std::size(CT::fields)},
      {"SGEJ", SGEJ::descriptor.e, SGEJ::fields, std::size(SGEJ::fields)},
      {"ULNG", ULNG::descriptor.e, ULNG::fields, std::size(ULNG::fields)},
      {"HGRP", HGRP::descriptor.e, HGRP::fields, std::size(HGRP::fields)},
      {"HR", HR::descriptor.e, HR::fields, std::size(HR::fields)},
      {"PERM", PERM::descriptor.e, PERM::fields, std::size(PERM::fields)},
      {"LS", LS::descriptor.e, LS::fields, std::size(LS::fields)},
      {"RU", RU::descriptor.e, RU::fields, std::size(RU::fields)},
      {"SU", SU::descriptor.e, SU::fields, std::size(SU::fields)},
      {"SPC", SPC::descriptor.e, SPC::fields, std::size(SPC::fields)},
      {"SPR", SPR::descriptor.e, SPR::fields, std::size(SPR::fields)},
      {"SPT", SPT::descriptor.e, SPT::fields, std::size(SPT::fields)},
      {"SPTR", SPTR::descriptor.e, SPTR::fields, std::size(SPTR::fields)},
      {"JJ", JJ::descriptor.e, JJ::fields, std::size(JJ::fields)},
      {"JJAT", JJAT::descriptor.e, JJAT::fields, std::size(JJAT::fields)},
      {"NSV", NSV::descriptor.e, NSV::fields, std::size(NSV::fields)},
      {"ASTR", ASTR::descriptor.e, ASTR::fields, std::size(ASTR::fields)},
      {"AULNG", AULNG::descriptor.e, AULNG::fields, std::size(AULNG::fields)},
      {"ABOOL", ABOOL::descriptor.e, ABOOL::fields, std::size(ABOOL::fields)},
      {"ATIME", ATIME::descriptor.e, ATIME::fields, std::size(ATIME::fields)},
      {"AMEM", AMEM::descriptor.e, AMEM::fields, std::size(AMEM::fields)},
      {"AINTER", AINTER::descriptor.e, AINTER::fields, std::size(AINTER::fields)},
      {"ASTRING", ASTRING::descriptor.e, ASTRING::fields, std::size(ASTRING::fields)},
      {"ASTRLIST", ASTRLIST::descriptor.e, ASTRLIST::fields, std::size(ASTRLIST::fields)},
      {"AUSRLIST", AUSRLIST::descriptor.e, AUSRLIST::fields, std::size(AUSRLIST::fields)},
      {"APRJLIST", APRJLIST::descriptor.e, APRJLIST::fields, std::size(APRJLIST::fields)},
      {"ACELIST", ACELIST::descriptor.e, ACELIST::fields, std::size(ACELIST::fields)},
      {"ASOLIST", ASOLIST::descriptor.e, ASOLIST::fields, std::size(ASOLIST::fields)},
      {"AQTLIST", AQTLIST::descriptor.e, AQTLIST::fields, std::size(AQTLIST::fields)},
      {"CQ", CQ::descriptor.e, CQ::fields, std::size(CQ::fields)},
      {"QIM", QIM::descriptor.e, QIM::fields, std::size(QIM::fields)},
      {"FCAT", FCAT::descriptor.e, FCAT::fields, std::size(FCAT::fields)},
      {"CTI", CTI::descriptor.e, CTI::fields, std::size(CTI::fields)},
      {"PARA", PARA::descriptor.e, PARA::fields, std::size(PARA::fields)},
      {"XMLA", XMLA::descriptor.e, XMLA::fields, std::size(XMLA::fields)},
      {"XMLH", XMLH::descriptor.e, XMLH::fields, std::size(XMLH::fields)},
      {"XMLS", XMLS::descriptor.e, XMLS::fields, std::size(XMLS::fields)},
      {"XMLE", XMLE::descriptor.e, XMLE::fields, std::size(XMLE::fields)},
      {"RDE", RDE::descriptor.e, RDE::fields, std::size(RDE::fields)},
      {"RUE", RUE::descriptor.e, RUE::fields, std::size(RUE::fields)},
      {"QETI", QETI::descriptor.e, QETI::fields, std::size(QETI::fields)},
      {"LDR", LDR::descriptor.e, LDR::fields, std::size(LDR::fields)},
      {"QRL", QRL::descriptor.e, QRL::fields, std::size(QRL::fields)},
      {"CCT", CCT::descriptor.e, CCT::fields, std::size(CCT::fields)},
      {"CQU", CQU::descriptor.e, CQU::fields, std::size(CQU::fields)},
      {"SCT", SCT::descriptor.e, SCT::fields, std::size(SCT::fields)},
      {"REF", REF::descriptor.e, REF::fields, std::size(REF::fields)},
      {"RQS", RQS::descriptor.e, RQS::fields, std::size(RQS::fields)},
      {"RQR", RQR::descriptor.e, RQR::fields, std::size(RQR::fields)},
      {"RQRF", RQRF::descriptor.e, RQRF::fields, std::size(RQRF::fields)},
      {"RQRL", RQRL::descriptor.e, RQRL::fields, std::size(RQRL::fields)},
      {"RQL", RQL::descriptor.e, RQL::fields, std::size(RQL::fields)},
      {"AR", AR::descriptor.e, AR::fields, std::size(AR::fields)},
      {"ARA", ARA::descriptor.e, ARA::fields, std::size(ARA::fields)},
      {"ACK", ACK::descriptor.e, ACK::fields, std::size(ACK::fields)},
      {"EVR", EVR::descriptor.e, EVR::fields, std::size(EVR::fields)},
      {"JSV", JSV::descriptor.e, JSV::fields, std::size(JSV::fields)},
      {"RTIC", RTIC::descriptor.e, RTIC::fields, std::size(RTIC::fields)},
      {"PRO", PRO::descriptor.e, PRO::fields, std::size(PRO::fields)},
      {"GR", GR::descriptor.e, GR::fields, std::size(GR::fields)},
      {"BN", BN::descriptor.e, BN::fields, std::size(BN::fields)},
      {"TEST", TEST::descriptor.e, TEST::fields, std::size(TEST::fields)},
      {"PACK", PACK::descriptor.e, PACK::fields, std::size(PACK::fields)},
      {"SPP", SPP::descriptor.e, SPP::fields, std::size(SPP::fields)},
      {"RL", RL::descriptor.e, RL::fields, std::size(RL::fields)},
      {"HM", HM::descriptor.e, HM::fields, std::size(HM::fields)},
      {"CX", CX::descriptor.e, CX::fields, std::size(CX::fields)},
   };

   /** @brief The sub-type of every list and object field that has one */
   inline constexpr SubTypeEntry all_sub_types[] = {
      {JB_jid_request_list, JRE::descriptor.e},
      {JB_jid_predecessor_list, JRE::descriptor.e},
      {JB_jid_successor_list, JRE::descriptor.e},
      {JB_ja_ad_request_list, JRE::descriptor.e},
      {JB_ja_ad_predecessor_list, JRE::descriptor.e},
      {JB_ja_ad_successor_list, JRE::descriptor.e},
      {JB_shell_list, PN::descriptor.e},
      {JB_env_list, VA::descriptor.e},
      {JB_context, VA::descriptor.e},
      {JB_job_args, ST::descriptor.e},
      {JB_stdout_path_list, PN::descriptor.e},
      {JB_stderr_path_list, PN::descriptor.e},
      {JB_stdin_path_list, PN::descriptor.e},
      {JB_request_set_list, JRS::descriptor.e},
      {JB_mail_list, MR::descriptor.e},
      {JB_pe_range, RN::descriptor.e},
      {JB_ja_structure, RN::descriptor.e},
      {JB_ja_n_h_ids, RN::descriptor.e},
      {JB_ja_u_h_ids, RN::descriptor.e},
      {JB_ja_s_h_ids, RN::descriptor.e},
      {JB_ja_o_h_ids, RN::descriptor.e},
      {JB_ja_a_h_ids, RN::descriptor.e},
      {JB_ja_template, JAT::descriptor.e},
      {JB_ja_tasks, JAT::descriptor.e},
      {JB_user_list, ST::descriptor.e},
      {JB_job_identifier_list, ID::descriptor.e},
      {JB_path_aliases, PA::descriptor.e},
      {JB_binding, BN::descriptor.e},
      {JB_grp_list, ST::descriptor.e},
      {JB_joker, VA::descriptor.e},
      {JB_context_change, CX::descriptor.e},
      {JRS_hard_resource_list, CE::descriptor.e},
      {JRS_soft_resource_list, CE::descriptor.e},
      {JRS_hard_queue_list, QR::descriptor.e},
      {JRS_soft_queue_list, QR::descriptor.e},
      {QU_ckpt_list, ST::descriptor.e},
      {QU_pe_list, ST::descriptor.e},
      {QU_owner_list, US::descriptor.e},
      {QU_acl, US::descriptor.e},
      {QU_xacl, US::descriptor.e},
      {QU_projects, PR::descriptor.e},
      {QU_xprojects, PR::descriptor.e},
      {QU_consumable_config_list, CE::descriptor.e},
      {QU_load_thresholds, CE::descriptor.e},
      {QU_suspend_thresholds, CE::descriptor.e},
      {QU_subordinate_list, SO::descriptor.e},
      {QU_resource_utilization, RUE::descriptor.e},
      {QU_message_list, QIM::descriptor.e},
      {QU_state_changes, CCT::descriptor.e},
      {QU_joker, VA::descriptor.e},
      {EH_scaling_list, HS::descriptor.e},
      {EH_consumable_config_list, CE::descriptor.e},
      {EH_usage_scaling_list, HS::descriptor.e},
      {EH_load_list, HL::descriptor.e},
      {EH_acl, US::descriptor.e},
      {EH_xacl, US::descriptor.e},
      {EH_prj, PR::descriptor.e},
      {EH_xprj, PR::descriptor.e},
      {EH_resource_utilization, RUE::descriptor.e},
      {EH_cached_complexes, CE::descriptor.e},
      {EH_reschedule_unknown_list, RU::descriptor.e},
      {EH_report_variables, STU::descriptor.e},
      {EH_merged_report_variables, STU::descriptor.e},
      {EH_joker, VA::descriptor.e},
      {EV_subscribed, EVS::descriptor.e},
      {EV_events, ET::descriptor.e},
      {CE_resource_map_list, RESL::descriptor.e},
      {OR_queuelist, OQ::descriptor.e},
      {OR_granted_resources_list, GRU::descriptor.e},
      {OQ_binding_to_use, ST::descriptor.e},
      {OQ_granted_rsmaps, RESL::descriptor.e},
      {US_entries, UE::descriptor.e},
      {US_joker, VA::descriptor.e},
      {PE_user_list, US::descriptor.e},
      {PE_xuser_list, US::descriptor.e},
      {PE_resource_utilization, RUE::descriptor.e},
      {PE_joker, VA::descriptor.e},
      {CONF_entries, CF::descriptor.e},
      {JG_binding_to_use, ST::descriptor.e},
      {JG_granted_rsmaps, RESL::descriptor.e},
      {SPA_argval_lListT, ST::descriptor.e},
      {PR_usage, UA::descriptor.e},
      {PR_long_term_usage, UA::descriptor.e},
      {PR_project, UPP::descriptor.e},
      {PR_acl, US::descriptor.e},
      {PR_xacl, US::descriptor.e},
      {PR_debited_job_usage, UPU::descriptor.e},
      {PR_joker, VA::descriptor.e},
      {UU_usage, UA::descriptor.e},
      {UU_long_term_usage, UA::descriptor.e},
      {UU_project, UPP::descriptor.e},
      {UU_debited_job_usage, UPU::descriptor.e},
      {UU_joker, VA::descriptor.e},
      {GRU_resource_map_list, RESL::descriptor.e},
      {GRU_binding_inuse, ST::descriptor.e},
      {RESL_properties, CE::descriptor.e},
      {STN_children, STN::descriptor.e},
      {STN_usage_list, UA::descriptor.e},
      {STN_joker, VA::descriptor.e},
      {SC_job_load_adjustments, CE::descriptor.e},
      {SC_usage_weight_list, UA::descriptor.e},
      {PET_granted_destin_identifier_list, JG::descriptor.e},
      {PET_usage, UA::descriptor.e},
      {PET_scaled_usage, UA::descriptor.e},
      {PET_reported_usage, UA::descriptor.e},
      {PET_previous_usage, UA::descriptor.e},
      {PET_path_aliases, PA::descriptor.e},
      {PET_environment, VA::descriptor.e},
      {PET_joker, VA::descriptor.e},
      {PETR_path_aliases, PA::descriptor.e},
      {PETR_environment, VA::descriptor.e},
      {JR_usage, UA::descriptor.e},
      {JL_OS_job_list, JO::descriptor.e},
      {JO_usage_list, UA::descriptor.e},
      {JO_pid_list, JP::descriptor.e},
      {UPU_old_usage_list, UA::descriptor.e},
      {CK_joker, VA::descriptor.e},
      {ID_ja_structure, RN::descriptor.e},
      {ID_user_list, ST::descriptor.e},
      {MA_answers, AN::descriptor.e},
      {CAL_parsed_year_calendar, CA::descriptor.e},
      {CAL_parsed_week_calendar, CA::descriptor.e},
      {CAL_joker, VA::descriptor.e},
      {CA_yday_range_list, TMR::descriptor.e},
      {CA_wday_range_list, TMR::descriptor.e},
      {CA_daytime_range_list, TMR::descriptor.e},
      {TMR_begin, TM::descriptor.e},
      {TMR_end, TM::descriptor.e},
      {UPP_usage, UA::descriptor.e},
      {UPP_long_term_usage, UA::descriptor.e},
      {SME_message_list, MES::descriptor.e},
      {SME_global_message_list, MES::descriptor.e},
      {MES_job_number_list, ULNG::descriptor.e},
      {JAT_granted_destin_identifier_list, JG::descriptor.e},
      {JAT_granted_resources_list, GRU::descriptor.e},
      {JAT_usage_list, UA::descriptor.e},
      {JAT_scaled_usage_list, UA::descriptor.e},
      {JAT_reported_usage_list, UA::descriptor.e},
      {JAT_task_list, PET::descriptor.e},
      {JAT_finished_task_list, FPET::descriptor.e},
      {JAT_previous_usage_list, UA::descriptor.e},
      {JAT_pe_object, PE::descriptor.e},
      {JAT_message_list, QIM::descriptor.e},
      {JAT_joker, VA::descriptor.e},
      {CT_cache, CCT::descriptor.e},
      {HGRP_host_list, HR::descriptor.e},
      {HGRP_cqueue_list, CQ::descriptor.e},
      {HGRP_joker, VA::descriptor.e},
      {HGRP_cached_hosts, HR::descriptor.e},
      {HGRP_match_cache, HM::descriptor.e},
      {LS_incomplete, LR::descriptor.e},
      {LS_complete, LR::descriptor.e},
      {SPC_rules, SPR::descriptor.e},
      {SPC_types, SPT::descriptor.e},
      {SPT_rules, SPTR::descriptor.e},
      {JJ_finished_tasks, JJAT::descriptor.e},
      {JJ_not_yet_finished_ids, RN::descriptor.e},
      {JJ_started_task_ids, RN::descriptor.e},
      {JJAT_rusage, UA::descriptor.e},
      {NSV_strings, ST::descriptor.e},
      {ASTRLIST_value, ST::descriptor.e},
      {AUSRLIST_value, US::descriptor.e},
      {APRJLIST_value, PR::descriptor.e},
      {ACELIST_value, CE::descriptor.e},
      {ASOLIST_value, SO::descriptor.e},
      {CQ_hostlist, HR::descriptor.e},
      {CQ_qinstances, QU::descriptor.e},
      {CQ_joker, VA::descriptor.e},
      {XMLH_Stylesheet, XMLS::descriptor.e},
      {XMLH_Attribute, XMLA::descriptor.e},
      {XMLE_Attribute, XMLA::descriptor.e},
      {RDE_resource_map_list, RESL::descriptor.e},
      {RUE_utilized_now_resource_map_list, RESL::descriptor.e},
      {RUE_utilized, RDE::descriptor.e},
      {RUE_utilized_nonexclusive, RDE::descriptor.e},
      {LDR_queue_ref_list, QR::descriptor.e},
      {CCT_ignore_queues, CTI::descriptor.e},
      {CCT_ignore_hosts, CTI::descriptor.e},
      {CCT_job_messages, MES::descriptor.e},
      {SCT_job_pending_ref, REF::descriptor.e},
      {SCT_job_ref, REF::descriptor.e},
      {RQS_rule, RQR::descriptor.e},
      {RQS_joker, VA::descriptor.e},
      {RQR_limit, RQRL::descriptor.e},
      {RQRF_scope, ST::descriptor.e},
      {RQRF_xscope, ST::descriptor.e},
      {RQRL_usage, RUE::descriptor.e},
      {AR_resource_list, CE::descriptor.e},
      {AR_resource_utilization, RUE::descriptor.e},
      {AR_queue_list, QR::descriptor.e},
      {AR_granted_slots, JG::descriptor.e},
      {AR_reserved_hosts, EH::descriptor.e},
      {AR_reserved_queues, QU::descriptor.e},
      {AR_mail_list, MR::descriptor.e},
      {AR_pe_range, RN::descriptor.e},
      {AR_master_queue_list, QR::descriptor.e},
      {AR_acl_list, ARA::descriptor.e},
      {AR_xacl_list, ARA::descriptor.e},
      {AR_request_set_list, JRS::descriptor.e},
      {AR_joker, VA::descriptor.e},
      {AR_granted_resources_list, GRU::descriptor.e},
      {AR_binding, BN::descriptor.e},
      {AR_pe_object, PE::descriptor.e},
      {EVR_event_list, ET::descriptor.e},
      {RTIC_tickets, OR::descriptor.e},
      {PRO_groups, GR::descriptor.e},
      {BN_specific_binding_list, ST::descriptor.e},
      {BN_joker, VA::descriptor.e},
      {TEST_obj_jb, JB::descriptor.e},
      {RL_user_list, US::descriptor.e},
      {RL_parent_role_list, ST::descriptor.e},
      {RL_joker, VA::descriptor.e},
   };

} // namespace ocs::tom
