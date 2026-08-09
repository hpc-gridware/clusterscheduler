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
 * @brief Base of the models: fetching or computing what a command reports
 */

#include "cull/cull.h"

#include "ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief Generic model for a stored procedure invocation.
    *
    * This model delegates the entire procedure execution to qmaster via a single
    * GDI call (`gdi::Target::PROCEDURE`).  The parameter bundle is serialised,
    * sent to qmaster, and the pre-rendered text response is stored internally.
    *
    * Command-specific models (e.g. QStatModelBase) do not use this GDI path;
    * instead they fetch typed CULL lists and perform filtering/rendering locally.
    *
    * @note This object owns `procedure_response` and frees it in its destructor.
    *
    * @ingroup libprocedure
    */
   class ProcedureModel {

#pragma region Data
   private:
      lList *procedure_response = nullptr; ///< CULL list received from qmaster; owned by this object.

   public:
      [[nodiscard]] const char *get_output_text() const;

      /** @brief Write what the model holds to the log
       *
       * Does nothing in the base; a subclass overrides it to dump its own
       * lists when tracing is on.
       */
      virtual void log_details() const {};
#pragma endregion

#pragma region Data Retrieval
   public:
      virtual bool make_snapshot(lList **answer_list, ProcedureParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors
   public:
      ProcedureModel() = default;

      virtual ~ProcedureModel() {
         lFreeList(&procedure_response);
      };
#pragma endregion
   };
}
