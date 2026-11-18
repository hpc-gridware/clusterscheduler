#pragma once
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

/** @file
 * @brief A GDI request: a list of tasks sent as one unit
 */

#include <type_traits>

#include "cull/cull.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_Transitional.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Target.h"
#include "gdi/ocs_gdi_Mode.h"
#include "sgeobj/sge_answer.h"

namespace ocs::gdi {
   /**
    * @brief A GDI request: one or more tasks sent to qmaster as a unit
    *
    * Tasks are added with #request in @ref Mode::RECORD and sent together when
    * one is added in @ref Mode::SEND, so a caller pays a single round trip for
    * several operations. #get_response then picks each task's answer out by the
    * id #request returned.
    */
   class Request {
   public:
      Packet *packet;             ///< the tasks, in the form they travel in
      lList *multi_answer_list;   ///< qmaster's answers, one entry per task

      Request();   ///< Build an empty request
      ~Request();  ///< Release the packet and the answers

      /// Wait until the answer to a request sent asynchronously has arrived
      void wait();

      OCS_TRANSITIONAL int request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Mode mode, Target target, Command cmd,
                                   SubCommand, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy);

      template <ocs::tom::ObjectType T>
      int request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Mode mode, Target target, Command cmd, SubCommand sub_cmd,
                  std::type_identity_t<ocs::tom::ConstList<T>> data, const ocs::tom::Where<T> &where,
                  const ocs::tom::What<T> &what);

      template <ocs::tom::ObjectType T>
      int request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Mode mode, Target target, Command cmd, SubCommand sub_cmd,
                  ocs::tom::ListPtr<T> &&data);

      OCS_TRANSITIONAL bool get_response(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Command cmd, SubCommand, Target target,
                                         int id, lList **list) const;

      template <ocs::tom::ObjectType T>
      bool get_response(ocs::tom::ListPtr<ocs::tom::AN> *alpp, Command cmd, SubCommand sub_cmd, Target target, int id,
                        ocs::tom::ListPtr<T> &list) const;
   };

   /**
    * @brief Add a task with typed data, condition and selection to this request, and send it when asked to
    *
    * The data, the condition and the selection are copied; they stay with the caller.
    *
    * @tparam T the object type of the target, deduced from the condition and the selection
    * @param[out] alpp receives errors detected before sending
    * @param mode @ref Mode::RECORD to only collect, @ref Mode::SEND to send now
    * @param target which object list to act on
    * @param cmd what to do
    * @param sub_cmd modifiers refining @p cmd
    * @param data the objects to send, an empty handle for none
    * @param where which objects to act on, an empty condition for all
    * @param what which fields to transfer, an empty selection for all
    * @return the task id to pass to #get_response, or -1 if sending failed
    */
   template <ocs::tom::ObjectType T>
   int Request::request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const Mode mode, const Target target, const Command cmd,
                        const SubCommand sub_cmd, const std::type_identity_t<ocs::tom::ConstList<T>> data,
                        const ocs::tom::Where<T> &where, const ocs::tom::What<T> &what) {
      // with do_copy the task copies the list, the condition and the selection and changes none of them
      lList *lp = const_cast<lList *>(static_cast<const lList *>(data));
      return request(alpp, mode, target, cmd, sub_cmd, &lp, const_cast<lCondition *>(where.get()),
                     const_cast<lEnumeration *>(what.get()), true);
   }

   /**
    * @brief Add a task with typed data to this request without copying the data, and send it when asked to
    *
    * @tparam T the object type of the target
    * @param[out] alpp receives errors detected before sending
    * @param mode @ref Mode::RECORD to only collect, @ref Mode::SEND to send now
    * @param target which object list to act on
    * @param cmd what to do
    * @param sub_cmd modifiers refining @p cmd
    * @param data the objects to send, taken over by the request
    * @return the task id to pass to #get_response, or -1 if sending failed
    */
   template <ocs::tom::ObjectType T>
   int Request::request(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const Mode mode, const Target target, const Command cmd,
                        const SubCommand sub_cmd, ocs::tom::ListPtr<T> &&data) {
      ocs::tom::ListPtr<T> taken = std::move(data);
      lList *lp = taken.release();
      return request(alpp, mode, target, cmd, sub_cmd, &lp, nullptr, nullptr, false);
   }

   /**
    * @brief Take one task's answer out of a sent request, with typed objects
    *
    * @tparam T the object type of the target
    * @param[out] alpp receives the answer list of that task
    * @param cmd the command the task carried
    * @param sub_cmd the modifiers the task carried
    * @param target the target the task addressed
    * @param id the task id #request returned
    * @param[out] list receives the objects read back if the command answers with objects; left as it was
    *             otherwise
    * @return true when the task succeeded
    */
   template <ocs::tom::ObjectType T>
   bool Request::get_response(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const Command cmd, const SubCommand sub_cmd,
                              const Target target, const int id, ocs::tom::ListPtr<T> &list) const {
      lList *const before = list.get();
      lList *lp = before;
      const bool ret = get_response(alpp, cmd, sub_cmd, target, id, &lp);
      if (lp != before) {
         // the answer of a task is a list of the target's type (reduced by the selection) that qmaster packed;
         // it is unpacked with the descriptor it travelled with, so it is taken over unchecked
         list = ocs::tom::ListPtr<T>::from_typed(lp);
      }
      return ret;
   }
}
