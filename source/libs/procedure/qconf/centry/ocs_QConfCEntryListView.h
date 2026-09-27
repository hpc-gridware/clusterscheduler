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
 * @brief The table rendering of `qconf -sc` (CS-2817)
 */

#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -sc`: every complex entry as an aligned table
    *
    * Neither of the two renderings the descriptor table offers fits. It is not a
    * name list, and it is not one object - it is the whole list written through
    * a flatfile instruction with a header, aligned columns and a footer
    * (`qconf_ce_list_sfi`). So it is a sub-procedure with a view of its own.
    *
    * It honours `-fmt json`, which the hand written code did by passing
    * qconf_opt_format through. One view serves both forms, as the flatfile
    * writer switches on the format itself.
    *
    * The alignment is the reason this cannot simply reuse the shared field list:
    * spool_flatfile_align_list() writes the computed widths back into the array
    * it is given, and the providers hand out a static table. See
    * qconf_copy_fields().
    *
    * @ingroup libprocedure
    */
   class QConfCEntryListView : public QConfViewBase {
   public:
      explicit QConfCEntryListView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfCEntryListView() override = default;

      /** @brief Not reached: the table is written by show_table() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm) override {
      }

      void show_table(std::ostream &os, const lList *list, const qconf_object_desc &desc);
   };
}
