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
/*___INFO__MARK_END_NEW__*/

#include "basis_types.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "gdi/ocs_gdi_Command.h"
#include "ocs_gdi_Task.h"

ocs::gdi::Task::Task(Target::TargetValue target, Command::Cmd command, SubCommand::SubCmd sub_cmd,
                     lList **lp, lList **a_list, lCondition **condition, lEnumeration **enumeration, bool do_copy)
      : command(command), sub_command(sub_cmd), target(target), data_list(nullptr), answer_list(nullptr), condition(nullptr),
        enumeration(nullptr), do_select_pack_simultaneous(false) {
   DENTER(TOP_LAYER);

   if (do_copy) {
      if (enumeration != nullptr && *enumeration != nullptr) {
         this->data_list = (((lp != nullptr) && (*lp != nullptr)) ? lSelect("", *lp, nullptr, *enumeration) : nullptr);
      } else {
         this->data_list = (((lp != nullptr) && (*lp != nullptr)) ? lCopyList("", *lp) : nullptr);
      }
      this->answer_list = (((a_list != nullptr) && (*a_list != nullptr)) ? lCopyList("", *a_list) : nullptr);
      this->condition = (((condition != nullptr) && (*condition != nullptr)) ? lCopyWhere(*condition) : nullptr);
      this->enumeration = (((enumeration != nullptr) && (*enumeration != nullptr)) ? lCopyWhat(*enumeration) : nullptr);
   } else {
      if ((lp != nullptr) && (*lp != nullptr)) {
         this->data_list = *lp;
         *lp = nullptr;
      } else {
         this->data_list = nullptr;
      }
      if ((a_list != nullptr) && (*a_list != nullptr)) {
         this->answer_list = *a_list;
         *a_list = nullptr;
      } else {
         this->answer_list = nullptr;
      }
      if ((condition != nullptr) && (*condition != nullptr)) {
         this->condition = *condition;
         *condition = nullptr;
      } else {
         this->condition = nullptr;
      }
      if ((enumeration != nullptr) && (*enumeration != nullptr)) {
         this->enumeration = *enumeration;
         *enumeration = nullptr;
      } else {
         this->enumeration = nullptr;
      }
   }
}

ocs::gdi::Task::Task()
       : command(gdi::Command::SGE_GDI_NONE), target(gdi::Target::TargetValue::NO_TARGET), data_list(nullptr),
         answer_list(nullptr), condition(nullptr), enumeration(nullptr), do_select_pack_simultaneous(false) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

ocs::gdi::Task::~Task() {
   DENTER(TOP_LAYER);
   lFreeList(&data_list);
   lFreeList(&answer_list);
   lFreeWhat(&enumeration);
   lFreeWhere(&condition);
   DRETURN_VOID;
}

void
ocs::gdi::Task::debug_print() {
   DENTER(TOP_LAYER);
   DPRINTF("command = " sge_u32 "\n", static_cast<u_long32>(command));
   DPRINTF("target = " sge_u32 "\n", static_cast<u_long32>(target));
   DPRINTF("data_list = %p\n", data_list);
   DPRINTF("answer_list = %p\n", answer_list);
   DPRINTF("condition = %p\n", condition);
   DPRINTF("enumeration = %p\n", enumeration);
   DRETURN_VOID;
}
