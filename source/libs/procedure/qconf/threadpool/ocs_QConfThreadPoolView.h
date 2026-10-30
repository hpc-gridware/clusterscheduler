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
 * @brief The rendering of `qconf -stl` (CS-2820)
 */

#include <format>

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

#include "sgeobj/cull/sge_str_ST_L.h"
#include "spool/flatfile/ocs_spool_json.h"
#include "uti/sge_dstring.h"

#include "../msg_qconf.h"

namespace ocs {
   /** @brief `qconf -stl`: the qmaster thread pools and their sizes
    *
    * One view for all three factories, as the event client table is: the plain
    * form is a hand written two column table, the JSON form an array of
    * { pool, size } objects, and both work from the same list.
    *
    * An empty list prints **nothing at all** in the plain form, header included.
    * That is what the hand written switch did, and it is pinned by
    * `object/thread_pool/thread_pool_list`; the JSON form still writes its
    * enveloped empty array, because an empty document is not a document.
    *
    * @ingroup libprocedure
    */
   class QConfThreadPoolView : public QConfViewBase {
   public:
      explicit QConfThreadPoolView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfThreadPoolView() override = default;

      /** @brief Not reached: the pools are written by show_pools() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the thread pools, as a table or as a JSON array
       * @param os   the stream to write to
       * @param list the pools, sorted by name by the provider
       */
      void show_pools(std::ostream &os, const lList *list) {
         if (output_format_ == ProcedureParameter::OutputFormat::JSON) {
            /* CS-2313a: an array of { pool, size } objects */
            static spooling_field st_fields[] = {
               { ST_name, 0, "pool", false, nullptr, false, nullptr, nullptr, nullptr },
               { ST_id,   0, "size", false, nullptr, false, nullptr, nullptr, nullptr },
               { NoName,  0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr }
            };
            lList *alp = nullptr;
            dstring out = DSTRING_INIT;
            spool_json_write_typed_list(&alp, list, st_fields, "thread_pools", &out);
            const char *text = sge_dstring_get_string(&out);
            os << ((text != nullptr) ? text : "");
            sge_dstring_free(&out);
            lFreeList(&alp);
            return;
         }

         if (lGetNumberOfElem(list) == 0) {
            return;
         }
         os << std::format("{:<15} {}\n", MSG_TABLE_EV_POOL, MSG_TABLE_SIZE);
         os << "--------------------\n";
         for_each_ep_lv(pool, list) {
            const char *name = lGetString(pool, ST_name);
            os << std::format("{:<15} {}\n", (name != nullptr) ? name : "", lGetUlong(pool, ST_id));
         }
      }
   };
}
