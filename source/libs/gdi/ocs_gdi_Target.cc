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

#include "ocs_gdi_Target.h"

std::string ocs::gdi::to_string(const Target target) {
   switch (target) {
      case Target::NO_TARGET: return "NO_TARGET";
      case Target::AH_LIST: return "AH_LIST";
      case Target::SH_LIST: return "SH_LIST";
      case Target::EH_LIST: return "EH_LIST";
      case Target::CQ_LIST: return "CQ_LIST";
      case Target::JB_LIST: return "JB_LIST";
      case Target::EV_LIST: return "EV_LIST";
      case Target::CE_LIST: return "CE_LIST";
      case Target::ORDER_LIST: return "ORDER_LIST";
      case Target::MASTER_EVENT: return "MASTER_EVENT";
      case Target::CONF_LIST: return "CONF_LIST";
      case Target::UM_LIST: return "UM_LIST";
      case Target::UO_LIST: return "UO_LIST";
      case Target::PE_LIST: return "PE_LIST";
      case Target::SC_LIST: return "SC_LIST";
      case Target::UU_LIST: return "UU_LIST";
      case Target::US_LIST: return "US_LIST";
      case Target::PR_LIST: return "PR_LIST";
      case Target::STN_LIST: return "STN_LIST";
      case Target::CK_LIST: return "CK_LIST";
      case Target::HGRP_LIST: return "HGRP_LIST";
      case Target::RQS_LIST: return "RQS_LIST";
      case Target::AR_LIST: return "AR_LIST";
      case Target::DUMMY_LIST: return "DUMMY_LIST";
      case Target::CAT_LIST: return "CAT_LIST";
      case Target::CAL_LIST: return "CAL_LIST";
      case Target::RL_LIST: return "RL_LIST";
      case Target::SME_LIST: return "SME_LIST";
      case Target::PROCEDURE: return "PROCEDURE";
   }
   return "UNKNOWN_TARGET";
}
