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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "gdi/sge_gdi.h"
#include "gdi/ocs_GdiTarget.h"
#include "gdi/msg_gdilib.h"

#include "msg_common.h"
#include "ocs_GdiMulti.h"

#define GDI_MULTI_LAYER GDI_LAYER

ocs::GdiMulti::GdiMulti() : packet(nullptr), multi_answer_list(nullptr) {
   ;
}

ocs::GdiMulti::~GdiMulti() {
   delete packet;
   lFreeList(&multi_answer_list);
}

void
ocs::GdiMulti::wait() {
   DENTER(GDI_MULTI_LAYER);

   // wait for the result of the packet
   if (packet != nullptr) {
      if (component_is_qmaster_internal()) {
         packet->wait_for_result_internal(&multi_answer_list);
      } else {
         packet->wait_for_result_external(&multi_answer_list);
      }
      packet = nullptr;
   }
   DRETURN_VOID;
}

int
ocs::GdiMulti::request(lList **alpp, GdiMode::Mode mode, GdiTarget::Target target, u_long32 cmd, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy) {
   DENTER(GDI_MULTI_LAYER);
   int id = -1;

   // create a new packet if it does not exist
   if (packet == nullptr) {
      packet = new GdiPacket();
      packet->initialize_auth_info();
   }

   // create a new task and append it to the packet
   auto task = new GdiTask(target, cmd, lp, nullptr, &cp, &enp, do_copy);
   id = packet->append_task(task);

   // execute the packet if it is the last task (mode == ocs::GdiMode::SEND)
   if (mode == GdiMode::SEND) {
      int local_ret;

      // internal execution allows to bypass the communication and authentication
      if (component_is_qmaster_internal()) {
         local_ret = packet->execute_internal(alpp);
      } else {
         local_ret = packet->execute_external(alpp);
      }

      // execution failed. cleanup packet and tasks.
      if (!local_ret) {
         delete packet;
         packet = nullptr;
         id = -1;
      }
   }

   // return the id of the task
   DRETURN(id);
}

bool
ocs::GdiMulti::get_response(lList **alpp, u_long32 cmd, GdiTarget::Target target, int id, lList **olpp) {
   DENTER(GDI_MULTI_LAYER);

   // still no response available? should not happen unless wait() was not called.
   if (multi_answer_list == nullptr || id < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // get the response for the given id
   lListElem *map = lGetElemUlongRW(multi_answer_list, MA_id, id);
   if (!map) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SGEGDIFAILED_S, ocs::GdiTarget::targetToString(target).c_str());
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // get the response data for commands where we expect a response
   int operation = SGE_GDI_GET_OPERATION(cmd);
   int sub_command = SGE_GDI_GET_SUBCOMMAND(cmd);
   if ((operation == SGE_GDI_GET) || (operation == SGE_GDI_PERMCHECK) ||
       (operation == SGE_GDI_ADD && sub_command == SGE_GDI_RETURN_NEW_VERSION)) {
      if (!olpp) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }
      lXchgList(map, MA_objects, olpp);
   }

   // get the answer list for the given id
   lXchgList(map, MA_answers, alpp);
   DRETURN(true);
}
