#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "lwdb/AttributeStatic.h"
#include "sgeobj/lwdb/ocs_JB_attributes.h"
#include "sgeobj/lwdb/ocs_JRS_attributes.h"
#include "sgeobj/lwdb/ocs_QU_attributes.h"
#include "sgeobj/lwdb/ocs_EH_attributes.h"
#include "sgeobj/lwdb/ocs_AH_attributes.h"
#include "sgeobj/lwdb/ocs_SH_attributes.h"
#include "sgeobj/lwdb/ocs_AN_attributes.h"
#include "sgeobj/lwdb/ocs_HL_attributes.h"
#include "sgeobj/lwdb/ocs_HS_attributes.h"
#include "sgeobj/lwdb/ocs_ET_attributes.h"
#include "sgeobj/lwdb/ocs_EV_attributes.h"
#include "sgeobj/lwdb/ocs_EVS_attributes.h"
#include "sgeobj/lwdb/ocs_CE_attributes.h"
#include "sgeobj/lwdb/ocs_LR_attributes.h"
#include "sgeobj/lwdb/ocs_OR_attributes.h"
#include "sgeobj/lwdb/ocs_OQ_attributes.h"
#include "sgeobj/lwdb/ocs_US_attributes.h"
#include "sgeobj/lwdb/ocs_UE_attributes.h"
#include "sgeobj/lwdb/ocs_RN_attributes.h"
#include "sgeobj/lwdb/ocs_PN_attributes.h"
#include "sgeobj/lwdb/ocs_VA_attributes.h"
#include "sgeobj/lwdb/ocs_MR_attributes.h"
#include "sgeobj/lwdb/ocs_UM_attributes.h"
#include "sgeobj/lwdb/ocs_UO_attributes.h"
#include "sgeobj/lwdb/ocs_PE_attributes.h"
#include "sgeobj/lwdb/ocs_QR_attributes.h"
#include "sgeobj/lwdb/ocs_JC_attributes.h"
#include "sgeobj/lwdb/ocs_CONF_attributes.h"
#include "sgeobj/lwdb/ocs_CF_attributes.h"
#include "sgeobj/lwdb/ocs_ST_attributes.h"
#include "sgeobj/lwdb/ocs_STU_attributes.h"
#include "sgeobj/lwdb/ocs_JG_attributes.h"
#include "sgeobj/lwdb/ocs_SO_attributes.h"
#include "sgeobj/lwdb/ocs_QAJ_attributes.h"
#include "sgeobj/lwdb/ocs_SPA_attributes.h"
#include "sgeobj/lwdb/ocs_REP_attributes.h"
#include "sgeobj/lwdb/ocs_UA_attributes.h"
#include "sgeobj/lwdb/ocs_PR_attributes.h"
#include "sgeobj/lwdb/ocs_UU_attributes.h"
#include "sgeobj/lwdb/ocs_GRU_attributes.h"
#include "sgeobj/lwdb/ocs_RESL_attributes.h"
#include "sgeobj/lwdb/ocs_STN_attributes.h"
#include "sgeobj/lwdb/ocs_SC_attributes.h"
#include "sgeobj/lwdb/ocs_PET_attributes.h"
#include "sgeobj/lwdb/ocs_PETR_attributes.h"
#include "sgeobj/lwdb/ocs_FPET_attributes.h"
#include "sgeobj/lwdb/ocs_JR_attributes.h"
#include "sgeobj/lwdb/ocs_LIC_attributes.h"
#include "sgeobj/lwdb/ocs_JL_attributes.h"
#include "sgeobj/lwdb/ocs_JP_attributes.h"
#include "sgeobj/lwdb/ocs_JO_attributes.h"
#include "sgeobj/lwdb/ocs_UPU_attributes.h"
#include "sgeobj/lwdb/ocs_CK_attributes.h"
#include "sgeobj/lwdb/ocs_KRB_attributes.h"
#include "sgeobj/lwdb/ocs_PA_attributes.h"
#include "sgeobj/lwdb/ocs_JRE_attributes.h"
#include "sgeobj/lwdb/ocs_ID_attributes.h"
#include "sgeobj/lwdb/ocs_MA_attributes.h"
#include "sgeobj/lwdb/ocs_TE_attributes.h"
#include "sgeobj/lwdb/ocs_CAL_attributes.h"
#include "sgeobj/lwdb/ocs_CA_attributes.h"
#include "sgeobj/lwdb/ocs_TMR_attributes.h"
#include "sgeobj/lwdb/ocs_TM_attributes.h"
#include "sgeobj/lwdb/ocs_RT_attributes.h"
#include "sgeobj/lwdb/ocs_UPP_attributes.h"
#include "sgeobj/lwdb/ocs_KTGT_attributes.h"
#include "sgeobj/lwdb/ocs_SME_attributes.h"
#include "sgeobj/lwdb/ocs_MES_attributes.h"
#include "sgeobj/lwdb/ocs_JAT_attributes.h"
#include "sgeobj/lwdb/ocs_CT_attributes.h"
#include "sgeobj/lwdb/ocs_SGEJ_attributes.h"
#include "sgeobj/lwdb/ocs_ULNG_attributes.h"
#include "sgeobj/lwdb/ocs_HGRP_attributes.h"
#include "sgeobj/lwdb/ocs_HR_attributes.h"
#include "sgeobj/lwdb/ocs_PERM_attributes.h"
#include "sgeobj/lwdb/ocs_LS_attributes.h"
#include "sgeobj/lwdb/ocs_RU_attributes.h"
#include "sgeobj/lwdb/ocs_FES_attributes.h"
#include "sgeobj/lwdb/ocs_SU_attributes.h"
#include "sgeobj/lwdb/ocs_SPC_attributes.h"
#include "sgeobj/lwdb/ocs_SPR_attributes.h"
#include "sgeobj/lwdb/ocs_SPT_attributes.h"
#include "sgeobj/lwdb/ocs_SPTR_attributes.h"
#include "sgeobj/lwdb/ocs_JJ_attributes.h"
#include "sgeobj/lwdb/ocs_JJAT_attributes.h"
#include "sgeobj/lwdb/ocs_NSV_attributes.h"
#include "sgeobj/lwdb/ocs_ASTR_attributes.h"
#include "sgeobj/lwdb/ocs_AULNG_attributes.h"
#include "sgeobj/lwdb/ocs_ABOOL_attributes.h"
#include "sgeobj/lwdb/ocs_ATIME_attributes.h"
#include "sgeobj/lwdb/ocs_AMEM_attributes.h"
#include "sgeobj/lwdb/ocs_AINTER_attributes.h"
#include "sgeobj/lwdb/ocs_ASTRING_attributes.h"
#include "sgeobj/lwdb/ocs_ASTRLIST_attributes.h"
#include "sgeobj/lwdb/ocs_AUSRLIST_attributes.h"
#include "sgeobj/lwdb/ocs_APRJLIST_attributes.h"
#include "sgeobj/lwdb/ocs_ACELIST_attributes.h"
#include "sgeobj/lwdb/ocs_ASOLIST_attributes.h"
#include "sgeobj/lwdb/ocs_AQTLIST_attributes.h"
#include "sgeobj/lwdb/ocs_CQ_attributes.h"
#include "sgeobj/lwdb/ocs_QIM_attributes.h"
#include "sgeobj/lwdb/ocs_FCAT_attributes.h"
#include "sgeobj/lwdb/ocs_CTI_attributes.h"
#include "sgeobj/lwdb/ocs_PARA_attributes.h"
#include "sgeobj/lwdb/ocs_XMLA_attributes.h"
#include "sgeobj/lwdb/ocs_XMLH_attributes.h"
#include "sgeobj/lwdb/ocs_XMLS_attributes.h"
#include "sgeobj/lwdb/ocs_XMLE_attributes.h"
#include "sgeobj/lwdb/ocs_RDE_attributes.h"
#include "sgeobj/lwdb/ocs_RUE_attributes.h"
#include "sgeobj/lwdb/ocs_QETI_attributes.h"
#include "sgeobj/lwdb/ocs_LDR_attributes.h"
#include "sgeobj/lwdb/ocs_QRL_attributes.h"
#include "sgeobj/lwdb/ocs_CCT_attributes.h"
#include "sgeobj/lwdb/ocs_CQU_attributes.h"
#include "sgeobj/lwdb/ocs_SCT_attributes.h"
#include "sgeobj/lwdb/ocs_REF_attributes.h"
#include "sgeobj/lwdb/ocs_RQS_attributes.h"
#include "sgeobj/lwdb/ocs_RQR_attributes.h"
#include "sgeobj/lwdb/ocs_RQRF_attributes.h"
#include "sgeobj/lwdb/ocs_RQRL_attributes.h"
#include "sgeobj/lwdb/ocs_RQL_attributes.h"
#include "sgeobj/lwdb/ocs_AR_attributes.h"
#include "sgeobj/lwdb/ocs_ARA_attributes.h"
#include "sgeobj/lwdb/ocs_ACK_attributes.h"
#include "sgeobj/lwdb/ocs_EVR_attributes.h"
#include "sgeobj/lwdb/ocs_JSV_attributes.h"
#include "sgeobj/lwdb/ocs_RTIC_attributes.h"
#include "sgeobj/lwdb/ocs_PRO_attributes.h"
#include "sgeobj/lwdb/ocs_GR_attributes.h"
#include "sgeobj/lwdb/ocs_BN_attributes.h"
#include "sgeobj/lwdb/ocs_TEST_attributes.h"
#include "sgeobj/lwdb/ocs_PACK_attributes.h"

namespace ocs {   
constexpr AttributeStatic all_attributes[] = {
      JB_ATTRIBUTES,
      JRS_ATTRIBUTES,
      QU_ATTRIBUTES,
      EH_ATTRIBUTES,
      AH_ATTRIBUTES,
      SH_ATTRIBUTES,
      AN_ATTRIBUTES,
      HL_ATTRIBUTES,
      HS_ATTRIBUTES,
      ET_ATTRIBUTES,
      EV_ATTRIBUTES,
      EVS_ATTRIBUTES,
      CE_ATTRIBUTES,
      LR_ATTRIBUTES,
      OR_ATTRIBUTES,
      OQ_ATTRIBUTES,
      US_ATTRIBUTES,
      UE_ATTRIBUTES,
      RN_ATTRIBUTES,
      PN_ATTRIBUTES,
      VA_ATTRIBUTES,
      MR_ATTRIBUTES,
      UM_ATTRIBUTES,
      UO_ATTRIBUTES,
      PE_ATTRIBUTES,
      QR_ATTRIBUTES,
      JC_ATTRIBUTES,
      CONF_ATTRIBUTES,
      CF_ATTRIBUTES,
      ST_ATTRIBUTES,
      STU_ATTRIBUTES,
      JG_ATTRIBUTES,
      SO_ATTRIBUTES,
      QAJ_ATTRIBUTES,
      SPA_ATTRIBUTES,
      REP_ATTRIBUTES,
      UA_ATTRIBUTES,
      PR_ATTRIBUTES,
      UU_ATTRIBUTES,
      GRU_ATTRIBUTES,
      RESL_ATTRIBUTES,
      STN_ATTRIBUTES,
      SC_ATTRIBUTES,
      PET_ATTRIBUTES,
      PETR_ATTRIBUTES,
      FPET_ATTRIBUTES,
      JR_ATTRIBUTES,
      LIC_ATTRIBUTES,
      JL_ATTRIBUTES,
      JP_ATTRIBUTES,
      JO_ATTRIBUTES,
      UPU_ATTRIBUTES,
      CK_ATTRIBUTES,
      KRB_ATTRIBUTES,
      PA_ATTRIBUTES,
      JRE_ATTRIBUTES,
      ID_ATTRIBUTES,
      MA_ATTRIBUTES,
      TE_ATTRIBUTES,
      CAL_ATTRIBUTES,
      CA_ATTRIBUTES,
      TMR_ATTRIBUTES,
      TM_ATTRIBUTES,
      RT_ATTRIBUTES,
      UPP_ATTRIBUTES,
      KTGT_ATTRIBUTES,
      SME_ATTRIBUTES,
      MES_ATTRIBUTES,
      JAT_ATTRIBUTES,
      CT_ATTRIBUTES,
      SGEJ_ATTRIBUTES,
      ULNG_ATTRIBUTES,
      HGRP_ATTRIBUTES,
      HR_ATTRIBUTES,
      PERM_ATTRIBUTES,
      LS_ATTRIBUTES,
      RU_ATTRIBUTES,
      FES_ATTRIBUTES,
      SU_ATTRIBUTES,
      SPC_ATTRIBUTES,
      SPR_ATTRIBUTES,
      SPT_ATTRIBUTES,
      SPTR_ATTRIBUTES,
      JJ_ATTRIBUTES,
      JJAT_ATTRIBUTES,
      NSV_ATTRIBUTES,
      ASTR_ATTRIBUTES,
      AULNG_ATTRIBUTES,
      ABOOL_ATTRIBUTES,
      ATIME_ATTRIBUTES,
      AMEM_ATTRIBUTES,
      AINTER_ATTRIBUTES,
      ASTRING_ATTRIBUTES,
      ASTRLIST_ATTRIBUTES,
      AUSRLIST_ATTRIBUTES,
      APRJLIST_ATTRIBUTES,
      ACELIST_ATTRIBUTES,
      ASOLIST_ATTRIBUTES,
      AQTLIST_ATTRIBUTES,
      CQ_ATTRIBUTES,
      QIM_ATTRIBUTES,
      FCAT_ATTRIBUTES,
      CTI_ATTRIBUTES,
      PARA_ATTRIBUTES,
      XMLA_ATTRIBUTES,
      XMLH_ATTRIBUTES,
      XMLS_ATTRIBUTES,
      XMLE_ATTRIBUTES,
      RDE_ATTRIBUTES,
      RUE_ATTRIBUTES,
      QETI_ATTRIBUTES,
      LDR_ATTRIBUTES,
      QRL_ATTRIBUTES,
      CCT_ATTRIBUTES,
      CQU_ATTRIBUTES,
      SCT_ATTRIBUTES,
      REF_ATTRIBUTES,
      RQS_ATTRIBUTES,
      RQR_ATTRIBUTES,
      RQRF_ATTRIBUTES,
      RQRL_ATTRIBUTES,
      RQL_ATTRIBUTES,
      AR_ATTRIBUTES,
      ARA_ATTRIBUTES,
      ACK_ATTRIBUTES,
      EVR_ATTRIBUTES,
      JSV_ATTRIBUTES,
      RTIC_ATTRIBUTES,
      PRO_ATTRIBUTES,
      GR_ATTRIBUTES,
      BN_ATTRIBUTES,
      TEST_ATTRIBUTES,
      PACK_ATTRIBUTES,
   };
}
