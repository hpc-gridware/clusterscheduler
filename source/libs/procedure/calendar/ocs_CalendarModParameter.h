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
 * @brief The parameters of the calendar modify procedure
 *
 * Shared by both halves: the client marshals them into the request bundle, and
 * qmaster reads them back out of it. Split from the server side on purpose -
 * the model, the view and the provider belong to qmaster, and a client that
 * only needs to send a request should not pull them in (ocs_CalendarModServer.h).
 */

#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "sgeobj/cull/sge_param_SPP_L.h"
#include "uti/ocs_ProgName.h"
#include "uti/sge_rmon_macros.h"

#include "../ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief The text of the calendar a modify procedure should apply
    *
    * The text, not a set of fields. That is the whole point of the shape: what
    * a read produces is text, so what a write consumes has to be the same text,
    * or the two halves of the round trip speak about the object differently.
    *
    * It also matters once RBAC reduces output. A reduced read renders
    * placeholders for the values the caller may not see, and only qmaster knows
    * which ones it masked. Typed fields coming back would arrive as real values
    * and overwrite the originals; text parsed in qmaster lets qmaster ignore
    * exactly the fields it masked.
    *
    * The calendar name is not carried separately - it is a field of the text,
    * and a second copy could disagree with it.
    *
    * @ingroup libprocedure
    */
   class CalendarModParameter : public ProcedureParameter {
   public:
      static constexpr auto CALENDAR_MOD = "calendar_mod";  ///< The sub-procedure name
      static constexpr auto OBJECT_TEXT = "object_text";    ///< The object as the read rendered it

   private:
      std::string object_text_;   ///< the calendar in the flat form a read produces

   public:
      /** @brief Build the parameters on the client side
       *
       * The procedure name has to be the program's, because that is the first
       * half of the key the dispatch table is looked up by - `{"qconf",
       * "calendar_mod"}`. Leaving it empty makes qmaster answer "requested
       * stored procedure is not available", which is a confusing way to learn
       * about a missing constructor argument.
       */
      CalendarModParameter() : ProcedureParameter(to_cstr(QCONF), nullptr) {
         set_sub_procedure_name(CALENDAR_MOD);
      }

      /** @brief Rebuild the parameters inside qmaster from the request bundle
       * @param bundle the request bundle received over GDI
       * @param packet the client request
       */
      explicit CalendarModParameter(lList *bundle, gdi::Packet *packet)
         : ProcedureParameter("", packet) {
         CalendarModParameter::set_bundle(bundle);
      }

      /** @brief Hand over the object text to apply
       * @param text the calendar in the form a read renders it
       */
      void set_object_text(std::string text) { object_text_ = std::move(text); }

      /// @return the object text to apply
      [[nodiscard]] const std::string &get_object_text() const { return object_text_; }

      lList *get_bundle() override;

      void set_bundle(const lList *bundle) override;
   };

}
