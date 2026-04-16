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
   enum class Command : __uint32_t {
      NONE = 0,
      GET,
      ADD,
      DEL,
      MOD,
      TRIGGER,
      PERMCHECK,
      SPECIAL,
      COPY,
      REPLACE,
      GET_PROCEDURE,
   };

   std::string to_string(Command cmd);

   inline Command operator|(Command a, Command b) {
      return static_cast<Command>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
   }

   inline Command operator&(Command a, Command b) {
      return static_cast<Command>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
   }

   enum class SubCommand : uint32_t {
      NONE                 = 0,

      RETURN_NEW_VERSION   = (1<<8),   ///< used for ADD-JOB-requests so that the created job is returned

      // delete or modify all jobs
      ALL_JOBS             = (1<<9),
      ALL_USERS            = (1<<10),

      // for queues and hosts to define how to handle sublists
      SET                  = NONE,        ///< overwrite the sublist with given values
      CHANGE               = (1<<11),  ///< change the given elements
      APPEND               = (1<<12),  ///< add some elements into a sublist
      REMOVE               = (1<<13),  ///< remove some elements from a sublist
      SET_ALL              = (1<<14),  ///< overwrite the sublist with given values and erase all domain/host specific values not given with the current request

      EXECD_RESTART        = (1<<15)
   };

   std::string to_string(SubCommand sub_cmd);

   inline SubCommand operator|(SubCommand a, SubCommand b) {
      return static_cast<SubCommand>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
   }

   inline SubCommand operator&(SubCommand a, SubCommand b) {
      return static_cast<SubCommand>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
   }

   inline bool get_response_from_master(Command cmd, SubCommand sub_cmd) {
      return cmd == Command::GET || cmd == Command::PERMCHECK || cmd == Command::GET_PROCEDURE
             || (cmd == Command::ADD && sub_cmd == SubCommand::RETURN_NEW_VERSION);
   }
}
