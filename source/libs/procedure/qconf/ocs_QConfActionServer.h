#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief The qmaster half of the qconf action procedures (CS-2962)
 *
 * One model, one view and one controller for every action. What differs per
 * action is the function that changes the objects, and that function lives in
 * `daemons/qmaster`, because it goes through `sge_gdi_add_mod_generic()` and
 * this library is linked into the clients as well. So qmaster registers one
 * provider per action at startup - the same wall and the same answer as the
 * write procedure in ocs_QConfWriteServer.h - and the model looks the provider
 * up by the name of the action it was asked to run.
 */

#include <ostream>

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "uti/sge_monitor.h"

#include "ocs_QConfActionParameter.h"

namespace ocs {

   /** @brief The qmaster function that carries out one action
    *
    * @param packet the client request, for the user and host the change is logged under
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param parameter the action's arguments
    * @param answer_list receives what the action has to say
    * @return true when the action did not fail
    */
   using qconf_action_provider = bool (*)(gdi::Packet *packet, gdi::Task *task, monitoring_t *monitor,
                                          const QConfActionParameter &parameter, lList **answer_list);

   /** @brief Register the function of one action, called by qmaster at startup
    *
    * Before any worker can answer a request, so the registry is read only once
    * requests arrive and needs no lock.
    *
    * @param action the action, one of the QConfActionParameter constants
    * @param provider the function that carries it out
    */
   void set_qconf_action_provider(const char *action, qconf_action_provider provider);

   /** @brief Carries out one action inside qmaster
    *
    * `make_snapshot()` performs the change rather than reading anything; see
    * QConfWriteModelServer for why a write goes by that name.
    *
    * @ingroup libprocedure
    */
   class QConfActionModelServer {
   private:
      gdi::Packet *packet_;             ///< the client request
      gdi::Task *task_;                 ///< the GDI task being answered
      monitoring_t *monitor_ = nullptr; ///< for monitoring qmaster threads, see set_monitor()

   public:
      /** @brief Bind the model to one request
       * @param packet the client request
       * @param task the GDI task being answered
       */
      QConfActionModelServer(gdi::Packet *packet, gdi::Task *task) : packet_(packet), task_(task) {}

      /** @brief Hand the model the thread's monitoring structure
       * @param monitor for monitoring qmaster threads
       */
      void set_monitor(monitoring_t *monitor) { monitor_ = monitor; }

      /** @brief Carry out the action the parameters name
       * @param answer_list receives what the action has to say
       * @param parameter the action and its arguments
       * @return true when the action did not fail
       */
      bool make_snapshot(lList **answer_list, QConfActionParameter &parameter);

      /// @return nothing; an action has no output, only answers
      [[nodiscard]] const char *get_output_text() const { return ""; }
   };

   static_assert(requires(QConfActionModelServer model, monitoring_t *monitor) {
                    model.set_monitor(monitor);
                 },
                 "QConfActionModelServer must keep set_monitor(monitoring_t *): the procedure "
                 "framework detects it with a requires-expression and silently skips it otherwise");

   /** @brief Renders nothing; what an action has to say is in the answer list
    * @ingroup libprocedure
    */
   class QConfActionView {
   public:
      /** @brief Build the view
       * @param parameter the call's parameters; unused, there is nothing to format
       */
      explicit QConfActionView([[maybe_unused]] const ProcedureParameter &parameter) {}
      virtual ~QConfActionView() = default;

      /** @brief Write the output, of which there is none
       * @param os where output would go
       * @param output the model's text, always empty for an action
       */
      virtual void show(std::ostream &os, const char *output) {
         if (output != nullptr && *output != '\0') {
            os << output << "\n";
         }
      }
   };

   /** @brief Hands the model's (empty) output to the view
    * @ingroup libprocedure
    */
   class QConfActionController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind the controller to an output stream
       * @param out the stream the view writes to
       */
      explicit QConfActionController(std::ostream &out) : out_(out) {}
      virtual ~QConfActionController() = default;

      /** @brief Render the outcome
       * @param parameter the call's parameters
       * @param model the model that performed the change
       * @param view the view to render with
       */
      virtual void process_request([[maybe_unused]] QConfActionParameter &parameter, QConfActionModelServer &model,
                                   QConfActionView &view) {
         view.show(out_, model.get_output_text());
      }
   };
}
