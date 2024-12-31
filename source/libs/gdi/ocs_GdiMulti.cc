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
#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_gdi_packet_internal.h"
#include "gdi/ocs_GdiTarget.h"
#include "gdi/msg_gdilib.h"

#include "msg_common.h"
#include "ocs_GdiMulti.h"

#define GDI_MULTI_LAYER GDI_LAYER

ocs::GdiMulti::GdiMulti() : packet(nullptr), multi_answer_list(nullptr) {
   ;
}

ocs::GdiMulti::~GdiMulti() {
   sge_gdi_packet_free(&packet);
   lFreeList(&multi_answer_list);
}

void
ocs::GdiMulti::wait() {
   DENTER(GDI_MULTI_LAYER);

   if (packet != nullptr) {
      if (component_is_qmaster_internal()) {
         sge_gdi_packet_wait_for_result_internal(&packet, &multi_answer_list);
      } else {
         sge_gdi_packet_wait_for_result_external(&packet, &multi_answer_list);
      }
      packet = nullptr;
   }
   DRETURN_VOID;
}

int
ocs::GdiMulti::request(lList **alpp, GdiMode::Mode mode, GdiTarget::Target target, u_long32 cmd, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy) {
   DENTER(GDI_MULTI_LAYER);
   int ret;

   /*
    * Create a new packet (if it not already exist) and store it
    * in state_gdi_multi structure
    */
   if (packet == nullptr) {
      packet = sge_gdi_packet_create(alpp);
   }

   /*
    * Add a task to the packet and if it is the last task of a
    * multi GDI request (mode == ocs::GdiMode::SEND) then execute it
    */
   if (packet != nullptr) {
      sge_gdi_packet_append_task(packet, alpp, target, cmd, lp, nullptr, &cp, &enp, do_copy);
      ret = sge_gdi_packet_get_last_task_id(packet);
      if (mode == GdiMode::SEND) {
         int local_ret;

         if (component_is_qmaster_internal()) {
            local_ret = sge_gdi_packet_execute_internal(alpp, packet);
         } else {
            local_ret = sge_gdi_packet_execute_external(alpp, packet);
         }
         if (!local_ret) {
            /* answer has been written in ctx->sge_gdi_packet_execute() */
            sge_gdi_packet_free(&packet);
            packet = nullptr;
            ret = -1;
         }
      }
   } else {
      /* answer list has been filled by sge_gdi_packet_create() */
      ret = -1;
   }
   DRETURN(ret);
}

bool
ocs::GdiMulti::get_response(lList **alpp, u_long32 cmd, GdiTarget::Target target, int id, lList **olpp) {
   DENTER(GDI_MULTI_LAYER);
   int operation = SGE_GDI_GET_OPERATION(cmd);
   int sub_command = SGE_GDI_GET_SUBCOMMAND(cmd);

   if (multi_answer_list == nullptr || id < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lListElem *map = lGetElemUlongRW(multi_answer_list, MA_id, id);
   if (!map) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SGEGDIFAILED_S, ocs::GdiTarget::targetToString(target).c_str());
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if ((operation == SGE_GDI_GET) || (operation == SGE_GDI_PERMCHECK) ||
       (operation == SGE_GDI_ADD && sub_command == SGE_GDI_RETURN_NEW_VERSION)) {
      if (!olpp) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }
      lXchgList(map, MA_objects, olpp);
   }

   lXchgList(map, MA_answers, alpp);
   DRETURN(true);
}
