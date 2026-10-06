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
 * @brief The qmaster half of the calendar modify procedure
 *
 * The model that performs the change, the view and controller that report it,
 * and the provider through which the procedure library reaches the writing code
 * in the daemon. Only qmaster includes this; the client side is
 * ocs_CalendarModModelClient.h and needs none of it - in particular not the
 * monitoring structure, which exists for the master alone.
 *
 * Why the procedure exists at all, and why it takes text rather than fields,
 * is explained at ocs::CalendarModParameter.
 */

#include <ostream>
#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "uti/sge_monitor.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_CalendarModParameter.h"

namespace ocs {

   /** @brief How the procedure library reaches the writing code in qmaster
    *
    * `sge_gdi_add_mod_generic()` lives in `daemons/qmaster` and must stay there:
    * the procedure library is linked into the clients as well, so it cannot call
    * into the daemon. The same wall as the thread pool list in CS-2820, and the
    * same answer - qmaster installs a provider at startup, and the model calls
    * through it.
    *
    * @param packet the client request, for the user and host the change is logged under
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param parameter what to change
    * @param answer_list receives the messages the write produced
    * @return true when the calendar was modified
    */
   using calendar_mod_provider = bool (*)(gdi::Packet *packet, gdi::Task *task, monitoring_t *monitor,
                                          const CalendarModParameter &parameter, lList **answer_list);

   /** @brief Install the writing code, called once by qmaster at startup
    * @param provider the function that performs the modification
    */
   void set_calendar_mod_provider(calendar_mod_provider provider);

   /** @brief Modifies one calendar inside qmaster
    *
    * `make_snapshot()` performs the change rather than reading anything. The
    * name comes from the framework, whose seam is "do the server side work,
    * then render", and a write is work: returning false skips the rendering
    * exactly as a failed read does, and the answer list carries the reason
    * either way.
    *
    * @ingroup libprocedure
    */
   class CalendarModModelServer {
   private:
      gdi::Packet *packet_;      ///< the client request
      gdi::Task *task_;          ///< the GDI task being answered
      monitoring_t *monitor_ = nullptr; ///< for monitoring qmaster threads, see set_monitor()
      std::string message_;      ///< what to report to the caller

   public:
      /** @brief Bind the model to one request
       *
       * Takes the same two arguments every server side model takes, so the
       * framework can construct it the same way. The monitor arrives through
       * set_monitor() instead of the constructor for that reason.
       *
       * @param packet the client request
       * @param task the GDI task being answered
       */
      CalendarModModelServer(gdi::Packet *packet, gdi::Task *task)
         : packet_(packet), task_(task) {}

      /** @brief Hand the model the thread's monitoring structure
       *
       * A write needs it and a read does not: the modification reaches
       * qinstance_change_state_on_calendar_all() through the object's
       * on_success hook, and that one monitors. Passing a null pointer instead
       * would work today only by accident.
       *
       * @param monitor for monitoring qmaster threads
       */
      void set_monitor(monitoring_t *monitor) { monitor_ = monitor; }

      /** @brief Apply the modification
       * @param answer_list receives errors and the confirmation
       * @param parameter the object text to apply
       * @return true when the calendar was modified
       */
      bool make_snapshot(lList **answer_list, CalendarModParameter &parameter);

      /// @return what to report to the caller
      [[nodiscard]] const char *get_output_text() const { return message_.c_str(); }
   };

   /* The framework hands a model the thread's monitoring structure only when it
    * has this setter, which exec_procedure() detects with a requires-expression
    * so the 22 reading models need no empty override. That test is silent by
    * nature: a setter renamed or given another signature simply would not
    * match, the model would run without a monitor, and the first sign of it
    * would be a null pointer in the event path at run time. Nailed down here,
    * where the model is defined, so that mistake is a compile error instead. */
   static_assert(requires(CalendarModModelServer model, monitoring_t *monitor) {
                    model.set_monitor(monitor);
                 },
                 "CalendarModModelServer must keep set_monitor(monitoring_t *): the procedure "
                 "framework detects it with a requires-expression and silently skips it otherwise");

   /** @brief Renders what the modification reported
    *
    * One class for all three output formats. The response is a single
    * confirmation line with nothing to structure, so an XML or JSON wrapper
    * would carry no information that the plain text does not - the same
    * decision the read procedures took wherever their output is one value.
    *
    * @ingroup libprocedure
    */
   class CalendarModView {
   public:
      /** @brief Build the view
       * @param parameter the call's parameters; unused, the output has no format of its own
       */
      explicit CalendarModView(const ProcedureParameter &parameter) {}
      virtual ~CalendarModView() = default;

      /** @brief Write the confirmation
       * @param os where to write
       * @param output the text the model produced
       */
      virtual void show(std::ostream &os, const char *output) {
         if (output != nullptr && *output != '\0') {
            os << output << "\n";
         }
      }
   };

   /** @brief Hands the model's report to the view
    * @ingroup libprocedure
    */
   class CalendarModController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind the controller to an output stream
       * @param out the stream the view writes to
       */
      explicit CalendarModController(std::ostream &out) : out_(out) {}
      virtual ~CalendarModController() = default;

      /** @brief Render the outcome of the modification
       * @param parameter the call's parameters
       * @param model the model that performed the change
       * @param view the view to render with
       */
      virtual void process_request(CalendarModParameter &parameter, CalendarModModelServer &model,
                                   CalendarModView &view) {
         view.show(out_, model.get_output_text());
      }
   };
}
