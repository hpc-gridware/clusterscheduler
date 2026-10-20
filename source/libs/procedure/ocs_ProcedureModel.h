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
#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_Target.h"

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
   protected:
      /** @brief CULL list received from qmaster; owned by this object
       *
       * Protected rather than private so that a unit test can hand in a response
       * it built itself and check the readers against it (CS-2821). Ownership is
       * unchanged - the destructor still frees whatever is here.
       */
      lList *procedure_response = nullptr;

   public:
      [[nodiscard]] const char *get_output_text() const;

      /** @brief What RBAC removed from this response (CS-2821)
       *
       * A mask of `ProcedureParameter::REDUCTION_*`, zero when nothing was
       * removed - which is every response today, because reduction does not
       * exist yet. The channel is here so that the write guard of CS-2823 and
       * the redaction work have something to agree on.
       *
       * An absent entry reads as zero, so a response from an older qmaster is
       * treated as not reduced. That is the right way round: an old daemon does
       * not reduce.
       *
       * @return the reduction mask
       */
      [[nodiscard]] uint32_t get_reduction() const;

      /** @brief What a write procedure did (CS-2822)
       *
       * The two counts the caller of a set write needs and cannot derive: how
       * many items were applied, and how many of them never got that far
       * because they could not be read. The reasons are in the answer list; what
       * is here is the arithmetic, which only the writing side can do - see
       * ProcedureParameter::WRITE_APPLIED.
       *
       * A reading procedure sends neither entry, so false means "this was not a
       * write" and the two values are untouched. Both are then also left
       * untouched when only one entry arrived, because a half-filled result
       * would be indistinguishable from a complete one.
       *
       * @param applied receives how many items were written
       * @param parse_failed receives how many could not be read
       * @param elements receives how many objects that was, which for a type whose text holds
       *                 a list is more than one per item (CS-2823); nullptr to ignore it, and
       *                 an older response without the entry leaves it untouched
       * @return true when the response carried a write result
       */
      [[nodiscard]] bool get_write_result(uint32_t &applied, uint32_t &parse_failed,
                                          uint32_t *elements = nullptr) const;

      /** @brief What an export procedure rendered (CS-2826)
       *
       * The exported objects as name/value pairs, `VA_variable` carrying the
       * object name and `VA_value` the text of its file, in the order qmaster
       * fetched them. The caller writes one file per entry and needs the pairing
       * to name them, see ProcedureParameter::EXPORT_OBJECTS.
       *
       * The entry's presence is the answer and the list is the content, the way
       * get_write_result() reports a write: an export that matched no object
       * sends the entry with an empty list, and CULL stores an empty list as
       * none at all. Returning the list alone could not tell that legitimate
       * "nothing to export" from a response that never carried the entry - and
       * the second one would be a broken channel wearing the face of an empty
       * cluster.
       *
       * The list belongs to this object and lives as long as it does.
       *
       * @param objects receives the pairs, left untouched when there is no entry
       * @return true when the response carried an export result
       */
      [[nodiscard]] bool get_export_objects(const lList **objects) const;

      /** @brief Write what the model holds to the log
       *
       * Does nothing in the base; a subclass overrides it to dump its own
       * lists when tracing is on.
       */
      virtual void log_details() const {};
#pragma endregion

#pragma region Data Retrieval
   public:
      /** @brief The GDI command this model's procedure runs under
       *
       * A reading procedure arrives as `GET_PROCEDURE`, which is checked like a
       * plain read; one that may change state arrives as `MOD_PROCEDURE` and is
       * checked like a MOD (CS-2822). The two are not interchangeable: qmaster
       * decides the permission checks from the command and then refuses a
       * procedure whose dispatch entry declares the other one, so a model that
       * writes must say so here or its request is turned down.
       *
       * @return the command to send
       */
      [[nodiscard]] virtual gdi::Command get_command() const {
         return gdi::Command::GET_PROCEDURE;
      }

      /** @brief The GDI target this model's procedure is addressed to
       *
       * A read is addressed to `Target::PROCEDURE`, which owns no permission of
       * its own - right for a read, which is checked by host only. A procedure
       * that may change state must name the object list it writes instead, so
       * that the modify permission and admin host checks ask about that list
       * and so that qmaster's dispatch is handed the row carrying its write
       * hooks (CS-2822). qmaster refuses a request whose target disagrees with
       * what the procedure's dispatch entry declares, so this is checked, not
       * trusted.
       *
       * @return the target to address
       */
      [[nodiscard]] virtual gdi::Target get_target() const {
         return gdi::Target::PROCEDURE;
      }

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
