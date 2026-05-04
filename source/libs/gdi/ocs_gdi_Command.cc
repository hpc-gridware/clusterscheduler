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

#include "ocs_gdi_Command.h"

std::string ocs::gdi::to_string(const Command cmd) {
   switch (cmd) {
      case Command::GET: return "GET";
      case Command::ADD: return "ADD";
      case Command::DEL: return "DEL";
      case Command::MOD: return "MOD";
      case Command::TRIGGER: return "TRIGGER";
      case Command::PERMCHECK: return "PERMCHECK";
      case Command::SPECIAL: return "SPECIAL";
      case Command::COPY: return "COPY";
      case Command::REPLACE: return "REPLACE";
      case Command::GET_PROCEDURE: return "PROCEDURE";
      case Command::NONE: return "NONE";
   }
   return "UNKNOWN_COMMAND";
}

std::string ocs::gdi::to_string(const SubCommand sub_cmd) {
   switch (sub_cmd) {
      case SubCommand::RETURN_NEW_VERSION: return "SGE_GDI_RETURN_NEW_VERSION";
      case SubCommand::ALL_JOBS: return "SGE_GDI_ALL_JOBS";
      case SubCommand::ALL_USERS: return "SGE_GDI_ALL_USERS";
      case SubCommand::SET: return "SGE_GDI_SET";
      case SubCommand::CHANGE: return "SGE_GDI_CHANGE";
      case SubCommand::APPEND: return "SGE_GDI_APPEND";
      case SubCommand::REMOVE: return "SGE_GDI_REMOVE";
      case SubCommand::SET_ALL: return "SGE_GDI_SET_ALL";
      case SubCommand::EXECD_RESTART: return "SGE_GDI_EXECD_RESTART";
   }
   return "UNKNOWN_SUBCOMMAND";
}

