#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <string>

namespace ocs {
   class GdiTarget {
      GdiTarget() = default; // prevent instantiation
   public:
      enum Target {
         NO_TARGET = 0,
         SGE_AH_LIST = 1,
         SGE_SH_LIST,
         SGE_EH_LIST,
         SGE_CQ_LIST,
         SGE_JB_LIST,
         SGE_EV_LIST,
         SGE_CE_LIST,
         SGE_ORDER_LIST,
         SGE_MASTER_EVENT,
         SGE_CONF_LIST,
         SGE_UM_LIST,
         SGE_UO_LIST,
         SGE_PE_LIST,
         SGE_SC_LIST,
         SGE_UU_LIST,
         SGE_US_LIST,
         SGE_PR_LIST,
         SGE_STN_LIST,
         SGE_CK_LIST,
         SGE_CAL_LIST,
         SGE_SME_LIST,
         SGE_ZOMBIE_LIST,
         SGE_USER_MAPPING_LIST,
         SGE_HGRP_LIST,
         SGE_RQS_LIST,
         SGE_AR_LIST,
         SGE_DUMMY_LIST
      };

      static std::string targetToString(Target target);
   };
}
