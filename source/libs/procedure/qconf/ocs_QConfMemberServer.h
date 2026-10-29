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
 * @brief The qmaster half of the access list membership procedure (CS-2959)
 *
 * The same shape as the write procedure in ocs_QConfWriteServer.h: the model
 * calls through a provider that qmaster installs at startup, because the code
 * that changes an access list lives in `daemons/qmaster` and this library is
 * linked into the clients as well.
 */

#include <ostream>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "uti/sge_monitor.h"

#include "ocs_QConfMemberParameter.h"

namespace ocs {

   /** @brief How the procedure library reaches the membership code in qmaster
    *
    * @param packet the client request, for the user and host the change is logged under
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param parameter which users to add to or remove from which lists
    * @param answer_list receives one answer per user and list
    * @return true when every user could be added or removed
    */
   using qconf_member_provider = bool (*)(gdi::Packet *packet, gdi::Task *task, monitoring_t *monitor,
                                         const QConfMemberParameter &parameter, lList **answer_list);

   /** @brief Install the membership code, called once by qmaster at startup
    * @param provider the function that changes the entries of access lists
    */
   void set_qconf_member_provider(qconf_member_provider provider);

   /** @brief Adds users to or removes them from access lists inside qmaster
    *
    * `make_snapshot()` performs the change, as the write model's does; see
    * QConfWriteModelServer for why a write goes by that name.
    *
    * @ingroup libprocedure
    */
   class QConfMemberModelServer {
   private:
      gdi::Packet *packet_;             ///< the client request
      gdi::Task *task_;                 ///< the GDI task being answered
      monitoring_t *monitor_ = nullptr; ///< for monitoring qmaster threads, see set_monitor()

   public:
      /** @brief Bind the model to one request
       * @param packet the client request
       * @param task the GDI task being answered
       */
      QConfMemberModelServer(gdi::Packet *packet, gdi::Task *task) : packet_(packet), task_(task) {}

      /** @brief Hand the model the thread's monitoring structure
       * @param monitor for monitoring qmaster threads
       */
      void set_monitor(monitoring_t *monitor) { monitor_ = monitor; }

      /** @brief Change the entries
       * @param answer_list receives one answer per user and list
       * @param parameter which users to add to or remove from which lists
       * @return true when every user could be added or removed
       */
      bool make_snapshot(lList **answer_list, QConfMemberParameter &parameter);

      /// @return nothing; the change has no output, only answers
      [[nodiscard]] const char *get_output_text() const { return ""; }
   };

   static_assert(requires(QConfMemberModelServer model, monitoring_t *monitor) {
                    model.set_monitor(monitor);
                 },
                 "QConfMemberModelServer must keep set_monitor(monitoring_t *): the procedure "
                 "framework detects it with a requires-expression and silently skips it otherwise");

   /** @brief Renders nothing; what the change has to say is in the answer list
    * @ingroup libprocedure
    */
   class QConfMemberView {
   public:
      /** @brief Build the view
       * @param parameter the call's parameters; unused, there is nothing to format
       */
      explicit QConfMemberView([[maybe_unused]] const ProcedureParameter &parameter) {}
      virtual ~QConfMemberView() = default;

      /** @brief Write the output, of which there is none
       * @param os where output would go
       * @param output the model's text, always empty here
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
   class QConfMemberController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind the controller to an output stream
       * @param out the stream the view writes to
       */
      explicit QConfMemberController(std::ostream &out) : out_(out) {}
      virtual ~QConfMemberController() = default;

      /** @brief Render the outcome
       * @param parameter the call's parameters
       * @param model the model that performed the change
       * @param view the view to render with
       */
      virtual void process_request([[maybe_unused]] QConfMemberParameter &parameter, QConfMemberModelServer &model,
                                   QConfMemberView &view) {
         view.show(out_, model.get_output_text());
      }
   };
}
