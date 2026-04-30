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
      /** @brief Return the pre-rendered output text received from qmaster.
       *
       * Returns an empty string if the response list is absent or malformed.
       * The returned pointer is valid for the lifetime of this object.
       */
      [[nodiscard]] const char *get_output_text() const;
      virtual void log_details() const {};
#pragma endregion

#pragma region Data Retrieval
   public:
      /** @brief Send the procedure request to qmaster and store the response.
       *
       * Serialises @p parameter into a CULL bundle, issues a GDI GET_PROCEDURE
       * request, and stores the resulting response list.  On success the rendered
       * output is accessible via get_output_text().
       *
       * @param answer_list  Receives error messages on failure.
       * @param parameter    The parsed procedure parameters to send.
       * @return true on success, false if the GDI call returned an error.
       */
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
