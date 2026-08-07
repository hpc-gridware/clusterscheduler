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
 * @brief What a GDI request asks to be done
 */

#include <cstdint>
#include <string>

namespace ocs::gdi {
   /// What a GDI request asks qmaster to do with its target list
   enum class Command : __uint32_t {
      NONE = 0,       ///< no command; an unset or invalid request
      GET,            ///< read objects from the target list
      ADD,            ///< create new objects
      DEL,            ///< delete objects
      MOD,            ///< modify existing objects
      TRIGGER,        ///< trigger an action rather than change data, e.g. a scheduling run
      PERMCHECK,      ///< ask which permissions the caller has
      SPECIAL,        ///< a request whose meaning depends on the target
      COPY,           ///< create objects by copying existing ones
      REPLACE,        ///< replace the whole target list
      GET_PROCEDURE,  ///< read a procedure definition
   };

   /**
    * @brief The name of a command, for logging and error messages
    * @param cmd the command to name
    * @return its name
    */
   std::string to_string(Command cmd);

   /**
    * @brief Combine two values bitwise
    * @param a first value
    * @param b second value
    * @return the union of both
    */
   inline Command operator|(Command a, Command b) {
      return static_cast<Command>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
   }

   /**
    * @brief Intersect two values bitwise
    * @param a first value
    * @param b second value
    * @return the bits set in both
    */
   inline Command operator&(Command a, Command b) {
      return static_cast<Command>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
   }

   /**
    * @brief Modifiers that refine a `Command` value
    *
    * A bit mask: several may be combined. The sublist modifiers
    * (`SET` and its neighbours) decide how a request that carries a
    * sublist is merged into the stored object.
    */
   enum class SubCommand : uint32_t {
      NONE                 = 0,        ///< no modifier

      RETURN_NEW_VERSION   = (1<<8),   ///< used for ADD-JOB-requests so that the created job is returned

      // delete or modify all jobs
      ALL_JOBS             = (1<<9),   ///< the request applies to every job, not just the named one
      ALL_USERS            = (1<<10),  ///< the request applies to every user

      // for queues and hosts to define how to handle sublists
      SET                  = NONE,        ///< overwrite the sublist with given values
      CHANGE               = (1<<11),  ///< change the given elements
      APPEND               = (1<<12),  ///< add some elements into a sublist
      REMOVE               = (1<<13),  ///< remove some elements from a sublist
      SET_ALL              = (1<<14),  ///< overwrite the sublist with given values and erase all domain/host specific values not given with the current request

      EXECD_RESTART        = (1<<15)   ///< the change requires the execution daemons to restart
   };

   /**
    * @brief The name of a sub-command, for logging and error messages
    * @param sub_cmd the sub-command to name
    * @return its name
    */
   std::string to_string(SubCommand sub_cmd);

   /**
    * @brief Combine two values bitwise
    * @param a first value
    * @param b second value
    * @return the union of both
    */
   inline SubCommand operator|(SubCommand a, SubCommand b) {
      return static_cast<SubCommand>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
   }

   /**
    * @brief Intersect two values bitwise
    * @param a first value
    * @param b second value
    * @return the bits set in both
    */
   inline SubCommand operator&(SubCommand a, SubCommand b) {
      return static_cast<SubCommand>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
   }

   /**
    * @brief Does this request expect data back from qmaster?
    *
    * Reads always do; a write does only when it asks for the created object to
    * be returned.
    *
    * @param cmd the command
    * @param sub_cmd the modifiers
    * @return true when the caller must wait for a response carrying data
    */
   inline bool get_response_from_master(Command cmd, SubCommand sub_cmd) {
      return cmd == Command::GET || cmd == Command::PERMCHECK || cmd == Command::GET_PROCEDURE
             || (cmd == Command::ADD && sub_cmd == SubCommand::RETURN_NEW_VERSION);
   }
}
