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
 * @brief A GDI request: a list of tasks sent as one unit
 */

#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_MA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

#include "gdi/ocs_gdi_Target.h"
#include "gdi/msg_gdilib.h"

#include "msg_common.h"
#include "ocs_gdi_Request.h"

/// rmon layer the multi GDI code logs under
#define GDI_MULTI_LAYER GDI_LAYER

ocs::gdi::Request::Request() : packet(nullptr) {
}

ocs::gdi::Request::~Request() {
   delete packet;
}

void
ocs::gdi::Request::wait() {
   DENTER(GDI_MULTI_LAYER);

   // wait for the result of the packet
   if (packet != nullptr) {
      if (component_is_qmaster_internal()) {
         packet->wait_for_result_internal(multi_answer_list);
      } else {
         packet->wait_for_result_external(multi_answer_list);
      }
      delete packet;
      packet = nullptr;
   }
   DRETURN_VOID;
}

/**
 * @brief Copy the objects a task sends, for a caller that keeps its own
 *
 * As `lSelect("", lp, nullptr, enp)` and `lCopyList("", lp)` did before: the copy is named "" - the name goes
 * over the wire -, a selection that keeps no element gives no list, and a failure gives no list instead of an
 * exception. ocs::tom::select() and ocs::tom::clone() keep the name of the source and return an empty list, so
 * the core functions are called here.
 *
 * @param lp the objects to send, or nullptr
 * @param enp which fields to send, or nullptr for all
 * @return the owner of the copy, empty for nullptr, for no selected element or on failure
 */
static ocs::tom::RawListPtr
ocs_gdi_copy_request_data(const lList *lp, const lEnumeration *enp) {
   if (lp == nullptr) {
      return {};
   }
   try {
      if (enp != nullptr) {
         ocs::tom::RawListPtr copy(ocs::tom::select_list("", lp, nullptr, enp, true, false));
         if (copy.get().size() == 0) {
            // an empty result of a hashed selection is no list, as lSelect() gave
            copy.reset();
         }
         return copy;
      }
      return ocs::tom::RawListPtr(ocs::tom::copy_list("", lp, true, false));
   } catch (const ocs::tom::FieldException &) {
      // a selection the list's type does not hold: no list, as lSelect() gave
   } catch (const ocs::tom::ListCoreError &) {
      // no memory: no list, as lSelect() and lCopyList() gave
   }
   return {};
}

/**
 * @brief Copy a condition or a field selection without throwing, as lCopyWhere() and lCopyWhat() did
 * @tparam P lCondition or lEnumeration
 * @param p the condition or selection, or nullptr
 * @return the owner of the copy, empty for nullptr or on failure
 */
template <typename P>
static auto
ocs_gdi_soft_clone(const P *p) -> decltype(ocs::tom::clone(p)) {
   try {
      return ocs::tom::clone(p);
   } catch (const ocs::tom::FieldException &) {
      // nothing to copy
   } catch (const ocs::tom::ListCoreError &) {
      // no memory or a corrupt condition: none, as before
   }
   return {};
}

/**
 * @brief Add a task to this request, and send it when asked to
 *
 * @param[out] alpp receives errors detected before sending
 * @param mode @ref Mode::RECORD to only collect, @ref Mode::SEND to send now
 * @param target which object list to act on
 * @param cmd what to do
 * @param sub_cmd modifiers refining @p cmd
 * @param lp the objects to send
 * @param cp which objects to act on, from `lWhere()`
 * @param enp which fields to transfer, from `lWhat()`
 * @param do_copy true to copy the arguments instead of taking them over
 * @return the task id to pass to #get_response, or 0 on error
 */
int
ocs::gdi::Request::request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Mode mode, Target target, const Command cmd,
                           SubCommand sub_cmd, lList **lp, lCondition *cp, lEnumeration *enp, const bool do_copy) {
   DENTER(GDI_MULTI_LAYER);

   // create a new packet if it does not exist
   if (packet == nullptr) {
      packet = new Packet();
   }

   // create a new task and append it to the packet; with do_copy it gets copies, otherwise the parts themselves
   Task *task;
   if (do_copy) {
      task = new Task(target, cmd, sub_cmd, ocs_gdi_copy_request_data(lp != nullptr ? *lp : nullptr, enp), {},
                      ocs_gdi_soft_clone(cp), ocs_gdi_soft_clone(enp));
   } else {
      task = new Task(target, cmd, sub_cmd, ocs::tom::RawListPtr(lp != nullptr ? std::exchange(*lp, nullptr) : nullptr),
                      {}, ocs::tom::RawWherePtr(cp), ocs::tom::RawWhatPtr(enp));
   }
   int id = packet->append_task(task);

   // execute the packet if it is the last task (mode == ocs::gdi::Mode::SEND)
   if (mode == Mode::SEND) {
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

/**
 * @brief Take one task's answer out of a sent request
 *
 * @param[out] alpp receives the answer list of that task
 * @param cmd the command the task carried
 * @param sub_cmd the modifiers the task carried
 * @param target the target the task addressed
 * @param id the task id #request returned
 * @param[out] list receives the objects read back, if any
 * @return true when the task succeeded
 */
bool
ocs::gdi::Request::get_response(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const Command cmd, const SubCommand sub_cmd,
                                const Target target, const int id, lList **list) const {
   DENTER(GDI_MULTI_LAYER);

   // check parameters. list can be nullptr if no response data is expected
   SGE_ASSERT(alpp != nullptr);

   // still no response available? should not happen unless wait() was not called.
   if (multi_answer_list == nullptr || id < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // get the response for the given id
   ocs::tom::Element<ocs::tom::MA> map = multi_answer_list.get().find_first<MA_id>(id);
   if (!map) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SGEGDIFAILED_S, to_string(target).c_str());
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // get the response data for commands where we expect a response
   if (list != nullptr && get_response_from_master(cmd, sub_cmd)) {
      // MA_objects holds lists of any type; the caller takes the list over as a raw pointer
      *list = map.exchange<MA_objects>(ocs::tom::RawListPtr()).release();
   }

   // get the answer list for the given id. Use answer_list_replace so that any
   // answer_list the caller has accumulated from a previous get_response() call
   // on the same alpp gets freed instead of orphaned; every prior request's
   // per-task answer_list used to leak silently because success/info entries
   // never tripped answer_list_has_error().
   auto answer_list = map.exchange<MA_answers>(ocs::tom::ListPtr<ocs::tom::AN>());
   answer_list_replace(alpp, &answer_list);

   DRETURN(true);
}
