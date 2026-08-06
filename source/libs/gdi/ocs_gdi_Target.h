#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include <cstdint>
#include <string>

namespace ocs::gdi {
   enum class Target : uint32_t {
      NO_TARGET = 0,
      // CS-2438: AH_LIST/SH_LIST retired -- admin and submit hosts are stored in
      // the reserved "@admin_hosts"/"@submit_hosts" host groups (HGRP_LIST)
      EH_LIST = 1,
      CQ_LIST,
      JB_LIST,
      EV_LIST,
      CE_LIST,
      ORDER_LIST,
      MASTER_EVENT,
      CONF_LIST,
      PE_LIST,
      SC_LIST,
      UU_LIST,
      US_LIST,
      PR_LIST,
      STN_LIST,
      CK_LIST,
      CAL_LIST,
      RL_LIST,
      SME_LIST,
      HGRP_LIST,
      RQS_LIST,
      AR_LIST,
      DUMMY_LIST,
      CAT_LIST,
      PROCEDURE,
   };

   std::string to_string(Target target);
}
