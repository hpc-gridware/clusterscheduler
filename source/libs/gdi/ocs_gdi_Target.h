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

/** @file
 * @brief The object lists a GDI request can address
 */

#include <cstdint>
#include <string>

namespace ocs::gdi {
   /**
    * @brief The object list a GDI request addresses
    *
    * Every request names exactly one target; qmaster maps it to the cull type,
    * the spooling function and the permission check for that object kind. The
    * table in `daemons/qmaster/sge_c_gdi.cc` is where that mapping lives.
    */
   enum class Target : uint32_t {
      NO_TARGET = 0, ///< no target; an unset or invalid request
      // CS-2438: AH_LIST/SH_LIST retired -- admin and submit hosts are stored in
      // the reserved "@admin_hosts"/"@submit_hosts" host groups (HGRP_LIST)
      EH_LIST = 1, ///< execution hosts
      CQ_LIST, ///< cluster queues
      JB_LIST, ///< jobs
      EV_LIST, ///< event clients
      CE_LIST, ///< complex entries
      ORDER_LIST, ///< scheduler orders
      MASTER_EVENT, ///< the event master itself
      CONF_LIST, ///< host configurations
      PE_LIST, ///< parallel environments
      SC_LIST, ///< the scheduler configuration
      UU_LIST, ///< users
      US_LIST, ///< user sets
      PR_LIST, ///< projects
      STN_LIST, ///< the share tree
      CK_LIST, ///< checkpointing interfaces
      CAL_LIST, ///< calendars
      RL_LIST, ///< RBAC roles
      SME_LIST, ///< scheduler information about why a job is not running
      HGRP_LIST, ///< host groups
      RQS_LIST, ///< resource quota sets
      AR_LIST, ///< advance reservations
      DUMMY_LIST, ///< a general request that addresses no list
      CAT_LIST, ///< job categories
      PROCEDURE, ///< procedures
   };

   /**
    * @brief The name of a target, for logging and error messages
    * @param target the target to name
    * @return its name, or `"UNKNOWN_TARGET"`
    */
   std::string to_string(Target target);
}
