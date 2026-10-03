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
 * @brief The rendering of `qconf -sobjl` (CS-2819)
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "spool/flatfile/ocs_spool_json.h"
#include "uti/sge_dstring.h"

namespace ocs {
   /** @brief `qconf -sobjl`: the names the filter matched, one per line
    *
    * The matching itself happens in the model, because which name a queue entry
    * produces depends on the kind of the entry, not on the object - so what
    * arrives here is already a list of names, not a list of objects.
    *
    * @ingroup libprocedure
    */
   class QConfObjectFilterView : public QConfViewBase {
   public:
      explicit QConfObjectFilterView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfObjectFilterView() override = default;

      /** @brief Not reached: the names are written by show_names() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the names the filter matched (CS-2831)
       *
       * All four object specifications answer under one envelope and one schema
       * id. They differ in what was searched, not in the shape of the answer: in
       * every case it is a list of names, and the qualification of a queue
       * instance or domain (`all.q@host`, `all.q@@group`) is part of the name, not
       * a structural difference. Reusing `cqueue-list` for `-sobjl queue` was
       * rejected for the opposite reason - it would claim that a filtered result
       * is the same document as the full list of `-sql`.
       *
       * @param os    the stream to write to
       * @param names the names the model matched, empty when nothing matched
       */
      void show_names(std::ostream &os, const std::vector<std::string> &names) {
         if (output_format_ != ProcedureParameter::OutputFormat::JSON) {
            for (const auto &name : names) {
               os << name << "\n";
            }
            return;
         }

         /* the shared writer wants name bearing elements; it brings the envelope,
          * the "<name>-list" schema id and the empty array of a result that
          * matched nothing, which is what keeps an empty answer valid JSON */
         lList *name_list = lCreateList("objectfilter", ST_Type);
         for (const auto &name : names) {
            lAddElemStr(&name_list, ST_name, name.c_str(), ST_Type);
         }

         lList *alp = nullptr;
         dstring out = DSTRING_INIT;
         spool_json_write_name_list_ex(&alp, name_list, ST_name, OBJECT_FILTER_TYPE_NAME, &out);
         const char *text = sge_dstring_get_string(&out);
         os << ((text != nullptr) ? text : "");
         sge_dstring_free(&out);
         lFreeList(&alp);
         lFreeList(&name_list);
      }

   private:
      /** @brief The envelope key, and with "-list" appended the schema id */
      static constexpr auto OBJECT_FILTER_TYPE_NAME = "objectfilter";
   };
}
