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
 * @brief The parameters of the qconf write procedure
 *
 * Shared by both halves: the client fills them, qmaster reads them back. The
 * model, the view and the provider belong to qmaster alone and live in
 * ocs_QConfWriteServer.h, so a client that only sends a request pulls neither
 * them nor the monitoring structure they use.
 *
 * One procedure writes every object type and any number of objects, which is
 * why there is one parameter class and not one per type:
 *
 *  - **what** is written comes from the request's GDI target, which the dispatch
 *    table binds to the entry (CS-2822), and the target leads to the object's
 *    row in the qconf descriptor table - descriptor, field list, spooling
 *    instruction, validator.
 *  - **how much** is a list. A single interactive modify is a set of one, a
 *    directory of files is a set of many, and nothing in between needs a second
 *    code path.
 *
 * Each item is a (label, payload) pair. The payload is the object's text, as a
 * read rendered it - not typed fields, because only qmaster knows which values
 * it masked when the output was reduced, and a client that parsed the text
 * would hand a placeholder back as if it were a value.
 *
 * The label is the **client's** and opaque to qmaster, which echoes it in the
 * answer. That is what lets the client report "file X failed because Y" without
 * parsing anything: the file name is the client's business, the object's name is
 * inside the text and the server's.
 */

#include <cstdint>
#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "sgeobj/cull/sge_param_SPP_L.h"
#include "uti/ocs_ProgName.h"
#include "uti/sge_rmon_macros.h"

#include "../ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief What to write, and under which batch policy
    * @ingroup libprocedure
    */
   class QConfWriteParameter : public ProcedureParameter {
   public:
      static constexpr auto OBJECT_WRITE = "object_write";  ///< The sub-procedure name
      static constexpr auto OBJECT_SET = "object_set";      ///< The items, as VA_variable/VA_value pairs
      static constexpr auto WRITE_MODE = "write_mode";      ///< The batch policy, a mask of MODE_*

      /* The policies qconf already has, with the meaning they already have -
       * see the design note. MODE_STRICT is parse level, as -strict is today:
       * one item that cannot be read rejects the batch. MODE_DRY stops after
       * the reading and reports what it would have done. */
      static constexpr uint32_t MODE_NONE = 0;    ///< apply what is usable, report what is not
      static constexpr uint32_t MODE_STRICT = 1;  ///< apply nothing unless every item reads
      static constexpr uint32_t MODE_DRY = 2;     ///< report only, change nothing

   private:
      lList *items_ = nullptr;        ///< VA_Type: VA_variable the label, VA_value the text
      uint32_t mode_ = MODE_NONE;     ///< mask of MODE_*

   public:
      /** @brief Build the parameters on the client side
       *
       * The procedure name has to be the program's: it is the first half of the
       * key the dispatch table is looked up by.
       */
      QConfWriteParameter() : ProcedureParameter(to_cstr(QCONF), nullptr) {
         set_sub_procedure_name(OBJECT_WRITE);
      }

      /** @brief Rebuild the parameters inside qmaster from the request bundle
       * @param bundle the request bundle received over GDI
       * @param packet the client request
       */
      explicit QConfWriteParameter(lList *bundle, gdi::Packet *packet)
         : ProcedureParameter("", packet) {
         QConfWriteParameter::set_bundle(bundle);
      }

      ~QConfWriteParameter() override {
         lFreeList(&items_);
      }

      /** @brief Add one object to write
       * @param label how the caller wants this item named in the answer, e.g. a file path
       * @param text the object in the form a read renders it
       */
      void add_item(const char *label, const std::string &text);

      /// @return the items, as a VA_Type list; owned by this object
      [[nodiscard]] const lList *get_items() const { return items_; }

      /// @return how many objects are to be written
      [[nodiscard]] int get_item_count() const { return lGetNumberOfElem(items_); }

      /** @brief Set the batch policy
       * @param mode a mask of MODE_*
       */
      void set_mode(const uint32_t mode) { mode_ = mode; }

      /** @brief Say which form the item texts are in
       *
       * The same value that decided how a read rendered the object decides how
       * qmaster parses it back, so the caller sets it from its own `-fmt`. The
       * base class takes it from the command line for a read and has no setter
       * for it; a write is told by its caller, which is why this one is here.
       *
       * @param format plain text or JSON, as the texts were rendered
       */
      void set_output_format(const OutputFormat format) { output_format_ = format; }

      /// @return true when one unreadable item must reject the whole batch
      [[nodiscard]] bool is_strict() const { return (mode_ & MODE_STRICT) != 0; }

      /// @return true when nothing may be changed
      [[nodiscard]] bool is_dry() const { return (mode_ & MODE_DRY) != 0; }

      lList *get_bundle() override;

      void set_bundle(const lList *bundle) override;
   };
}
