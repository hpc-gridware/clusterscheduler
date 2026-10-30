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
 * @brief The qmaster half of the qconf write procedure
 *
 * The model that applies a set of objects, the view and controller the
 * framework expects, and the provider through which the procedure library
 * reaches the writing code in the daemon. Only qmaster includes this; the
 * client side is ocs_QConfWriteModelClient.h and needs none of it - in
 * particular not the monitoring structure, which exists for the master alone.
 *
 * What is written and why it is text rather than fields is explained at
 * ocs::QConfWriteParameter.
 */

#include <ostream>
#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "uti/sge_monitor.h"
#include "uti/sge_rmon_macros.h"

#include "../ocs_QConfObject.h"
#include "ocs_QConfWriteParameter.h"

namespace ocs {

   /** @brief What a write did, for the caller's own report
    *
    * Counted by the provider, handed back through the response bundle. Two
    * numbers rather than one, because `-strict` distinguishes the phases: see
    * ProcedureParameter::WRITE_APPLIED for why the caller cannot work them out
    * from the answer list.
    *
    * @ingroup libprocedure
    */
   struct QConfWriteResult {
      uint32_t applied = 0;       ///< items written, or that would be written in a dry run
      uint32_t parse_failed = 0;  ///< items that could not be read, so were never applied
      uint32_t elements = 0;      ///< objects written, which for a type whose text holds a
                                  ///< list is more than one per item (CS-2823). For every
                                  ///< other type it equals @ref applied, so a caller that
                                  ///< counts objects can use it without asking which kind
                                  ///< of type it is writing
   };

   /** @brief How the procedure library reaches the writing code in qmaster
    *
    * `sge_gdi_add_mod_generic()` lives in `daemons/qmaster` and must stay
    * there: this library is linked into the clients as well, so it cannot call
    * into the daemon. The same wall as the thread pool list of CS-2820, and the
    * same answer - qmaster installs a provider at startup and the model calls
    * through it.
    *
    * The object type is passed in rather than derived from the request's
    * target, because a target does not identify it: `HGRP_LIST` and `US_LIST`
    * each carry three rows of the descriptor table - the plain host group plus
    * the reserved `@admin_hosts`/`@submit_hosts`, the plain userset plus the
    * reserved manager/operator lists. The dispatch entry names the row, and the
    * target is derived from *it*, so there is one source and no ambiguity.
    *
    * @param packet the client request, for the user and host the change is logged under
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param obj which object type is written
    * @param parameter the items and the batch policy
    * @param answer_list receives one entry per item, plus whatever the writes produced
    * @param result receives how many items were applied and how many could not be read
    * @return true when the batch was applied as the policy demands
    */
   using qconf_write_provider = bool (*)(gdi::Packet *packet, gdi::Task *task, monitoring_t *monitor,
                                         QConfObj obj, const QConfWriteParameter &parameter,
                                         lList **answer_list, QConfWriteResult *result);

   /** @brief Install the writing code, called once by qmaster at startup
    * @param provider the function that applies a set of objects
    */
   void set_qconf_write_provider(qconf_write_provider provider);

   /** @brief Applies a set of objects inside qmaster
    *
    * `make_snapshot()` performs the change rather than reading anything. The
    * name comes from the framework, whose seam is "do the server side work,
    * then render", and a write is work: returning false skips the rendering
    * exactly as a failed read does, and the answer list carries the reasons
    * either way.
    *
    * @ingroup libprocedure
    */
   class QConfWriteModelServer {
   private:
      gdi::Packet *packet_;             ///< the client request
      gdi::Task *task_;                 ///< the GDI task being answered
      monitoring_t *monitor_ = nullptr; ///< for monitoring qmaster threads, see set_monitor()
      QConfObj obj_ = QConfObj::CAL;    ///< which object type, see set_object()
      bool obj_known_ = false;          ///< whether set_object() was called
      QConfWriteResult result_;         ///< what the write did, see get_write_result()

   public:
      /** @brief Bind the model to one request
       *
       * Takes the same two arguments every server side model takes, so the
       * framework constructs it the same way. What only a write needs - the
       * monitor and the object type - arrives through setters for that reason.
       *
       * @param packet the client request
       * @param task the GDI task being answered
       */
      QConfWriteModelServer(gdi::Packet *packet, gdi::Task *task)
         : packet_(packet), task_(task) {}

      /** @brief Hand the model the thread's monitoring structure
       *
       * A write needs it and a read does not: the change reaches the object's
       * on_success hook, and those monitor. A null pointer would work today
       * only by accident.
       *
       * @param monitor for monitoring qmaster threads
       */
      void set_monitor(monitoring_t *monitor) { monitor_ = monitor; }

      /** @brief Hand the model the object type its dispatch entry declares
       * @param obj the row of the qconf descriptor table to write
       */
      void set_object(const QConfObj obj) {
         obj_ = obj;
         obj_known_ = true;
      }

      /** @brief Apply the set
       * @param answer_list receives one entry per item
       * @param parameter the items and the batch policy
       * @return true when the batch was applied as the policy demands
       */
      bool make_snapshot(lList **answer_list, QConfWriteParameter &parameter);

      /// @return nothing; a write has no output, only a verdict
      [[nodiscard]] const char *get_output_text() const { return ""; }

      /** @brief What the write did
       *
       * Read by the procedure framework and put into the response bundle, which
       * is how the client gets at it. Detected with a requires-expression, like
       * the two setters above, so a reading model needs no empty override.
       *
       * @return the counts; all zero until make_snapshot() has run
       */
      [[nodiscard]] const QConfWriteResult &get_write_result() const { return result_; }
   };

   /* The framework hands a model the monitor and the object type only when it
    * has the setters, detected with requires-expressions so the reading models
    * need no empty overrides. Those tests are silent by nature: a setter
    * renamed or given another signature simply would not match, the model would
    * run without what it needs, and the first sign of it would be a null
    * monitor in the event path or a write against the wrong object type. Nailed
    * down here, where the model is defined, so that mistake is a compile error. */
   static_assert(requires(QConfWriteModelServer model, monitoring_t *monitor) {
                    model.set_monitor(monitor);
                 },
                 "QConfWriteModelServer must keep set_monitor(monitoring_t *): the procedure "
                 "framework detects it with a requires-expression and silently skips it otherwise");
   static_assert(requires(QConfWriteModelServer model, QConfObj obj) {
                    model.set_object(obj);
                 },
                 "QConfWriteModelServer must keep set_object(QConfObj): the procedure framework "
                 "detects it with a requires-expression and silently skips it otherwise");
   static_assert(requires(const QConfWriteModelServer model) {
                    model.get_write_result();
                 },
                 "QConfWriteModelServer must keep get_write_result(): the procedure framework "
                 "detects it with a requires-expression and the client would see no counts otherwise");

   /** @brief Renders nothing
    *
    * A write has no output. What it has to say is in the answer list, which is
    * where every other qconf write says it - see qconf_apply_path() and
    * show_answer_list(). A procedure that rendered a confirmation of its own
    * would say the same thing in different words and in a different stream.
    *
    * @ingroup libprocedure
    */
   class QConfWriteView {
   public:
      /** @brief Build the view
       * @param parameter the call's parameters; unused, there is nothing to format
       */
      explicit QConfWriteView(const ProcedureParameter &parameter) {}
      virtual ~QConfWriteView() = default;

      /** @brief Write the output, of which there is none
       * @param os where output would go
       * @param output the model's text, always empty for a write
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
   class QConfWriteController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind the controller to an output stream
       * @param out the stream the view writes to
       */
      explicit QConfWriteController(std::ostream &out) : out_(out) {}
      virtual ~QConfWriteController() = default;

      /** @brief Render the outcome
       * @param parameter the call's parameters
       * @param model the model that performed the change
       * @param view the view to render with
       */
      virtual void process_request(QConfWriteParameter &parameter, QConfWriteModelServer &model,
                                   QConfWriteView &view) {
         view.show(out_, model.get_output_text());
      }
   };
}
