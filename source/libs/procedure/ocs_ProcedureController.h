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

#include <sstream>

#include "ocs_ProcedureParameter.h"
#include "ocs_ProcedureModel.h"
#include "ocs_ProcedureView.h"

/**
 * @defgroup libprocedure Stored Procedure MVC Framework
 *
 * @brief MVC framework for executing CLI commands (qstat, qhost, qquota, qrstat)
 * either on the client side or server side.
 *
 * ## Architecture
 *
 * Each command is implemented as a Model-View-Controller triple:
 *
 * - **ProcedureParameter** — carries the parsed command-line arguments, the
 *   requested output format (Plain/XML/JSON), and the execution context
 *   (CLIENT or SERVER).
 *
 * - **ProcedureModel** — fetches or computes the data needed for the response.
 *   Subcommand-specific models (e.g. QStatModelBase) hold typed CULL lists;
 *   the generic ProcedureModel delegates the entire execution to qmaster via GDI
 *   and stores the pre-rendered text response.
 *
 * - **ProcedureView** — renders the model data to an `std::ostream` in the
 *   requested format. The base implementation writes the pre-rendered text as-is;
 *   subclasses wrap it in XML or JSON structure.
 *
 * - **ProcedureController** — orchestrates the triple: calls `make_snapshot()` on
 *   the model (if not already done) then calls `view.show()`.
 *
 * ## Execution contexts
 *
 * - **CLIENT** (`ExecContext::CLIENT`): the client application fetches the required
 *   data from qmaster via GDI and processes and renders it locally.
 *
 * - **SERVER** (`ExecContext::SERVER`): the model reads directly from in-process
 *   master lists (runs inside qmaster).  The rendered output is sent back to the
 *   client in the GDI response.
 *
 * ## Typical call sequence
 * @code
 *   MyParameter param("qstat", packet);
 *   MyModel     model;
 *   MyView      view(param);
 *   MyController ctrl(std::cout);
 *
 *   lList *answer_list = nullptr;
 *   if (model.make_snapshot(&answer_list, param))
 *      ctrl.process_request(param, model, view);
 * @endcode
 */

namespace ocs {
   /** @brief Controller that coordinates model and view for a stored procedure.
    *
    * The base implementation simply calls `view.show(out_, model.get_output_text())`.
    * Subclasses may override `process_request` to perform additional formatting or
    * error reporting before or after the view renders.
    *
    * @ingroup libprocedure
    */
   class ProcedureController {
   protected:
      std::ostream& out_;
   public:
      explicit ProcedureController(std::ostream &out) : out_(out) {};
      virtual ~ProcedureController() = default;

      /** @brief Render the model data via the view.
       *
       * @param parameter  The parsed procedure parameters (used by subclass overrides).
       * @param model      The model whose data has already been fetched via make_snapshot().
       * @param view       The view that writes formatted output to out_.
       */
      virtual void process_request(ProcedureParameter &parameter, ProcedureModel &model, ProcedureView &view);
   };
}
