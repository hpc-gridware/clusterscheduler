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
 * @brief The qmaster half of the `qconf -purge` procedure (CS-2948)
 *
 * The same shape as the write procedure in ocs_QConfWriteServer.h: the model
 * calls through a provider that qmaster installs at startup, because the code
 * that changes a cluster queue lives in `daemons/qmaster` and this library is
 * linked into the clients as well.
 */

#include <ostream>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "uti/sge_monitor.h"

#include "ocs_QConfPurgeParameter.h"

namespace ocs {

   /** @brief How the procedure library reaches the purging code in qmaster
    *
    * @param packet the client request, for the user and host the change is logged under
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param parameter what to purge from which queue
    * @param answer_list receives the outcome
    * @return true when the purge did not fail; finding nothing to purge is not a failure
    */
   using qconf_purge_provider = bool (*)(gdi::Packet *packet, gdi::Task *task, monitoring_t *monitor,
                                         const QConfPurgeParameter &parameter, lList **answer_list);

   /** @brief Install the purging code, called once by qmaster at startup
    * @param provider the function that purges a queue
    */
   void set_qconf_purge_provider(qconf_purge_provider provider);

   /** @brief Purges host specific values of one cluster queue inside qmaster
    *
    * `make_snapshot()` performs the change, as the write model's does; see
    * QConfWriteModelServer for why a write goes by that name.
    *
    * @ingroup libprocedure
    */
   class QConfPurgeModelServer {
   private:
      gdi::Packet *packet_;             ///< the client request
      gdi::Task *task_;                 ///< the GDI task being answered
      monitoring_t *monitor_ = nullptr; ///< for monitoring qmaster threads, see set_monitor()

   public:
      /** @brief Bind the model to one request
       * @param packet the client request
       * @param task the GDI task being answered
       */
      QConfPurgeModelServer(gdi::Packet *packet, gdi::Task *task) : packet_(packet), task_(task) {}

      /** @brief Hand the model the thread's monitoring structure
       * @param monitor for monitoring qmaster threads
       */
      void set_monitor(monitoring_t *monitor) { monitor_ = monitor; }

      /** @brief Purge
       * @param answer_list receives the outcome
       * @param parameter what to purge from which queue
       * @return true when the purge did not fail
       */
      bool make_snapshot(lList **answer_list, QConfPurgeParameter &parameter);

      /// @return nothing; a purge has no output, only a verdict
      [[nodiscard]] const char *get_output_text() const { return ""; }
   };

   static_assert(requires(QConfPurgeModelServer model, monitoring_t *monitor) {
                    model.set_monitor(monitor);
                 },
                 "QConfPurgeModelServer must keep set_monitor(monitoring_t *): the procedure "
                 "framework detects it with a requires-expression and silently skips it otherwise");

   /** @brief Renders nothing; what a purge has to say is in the answer list
    * @ingroup libprocedure
    */
   class QConfPurgeView {
   public:
      /** @brief Build the view
       * @param parameter the call's parameters; unused, there is nothing to format
       */
      explicit QConfPurgeView([[maybe_unused]] const ProcedureParameter &parameter) {}
      virtual ~QConfPurgeView() = default;

      /** @brief Write the output, of which there is none
       * @param os where output would go
       * @param output the model's text, always empty for a purge
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
   class QConfPurgeController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind the controller to an output stream
       * @param out the stream the view writes to
       */
      explicit QConfPurgeController(std::ostream &out) : out_(out) {}
      virtual ~QConfPurgeController() = default;

      /** @brief Render the outcome
       * @param parameter the call's parameters
       * @param model the model that performed the change
       * @param view the view to render with
       */
      virtual void process_request([[maybe_unused]] QConfPurgeParameter &parameter, QConfPurgeModelServer &model,
                                   QConfPurgeView &view) {
         view.show(out_, model.get_output_text());
      }
   };
}
