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
 * @brief The rendering of `qconf -secl` (CS-2820)
 */

#include <format>

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

#include "sgeobj/cull/sge_event_EV_L.h"
#include "spool/flatfile/ocs_spool_json.h"
#include "uti/sge_dstring.h"

#include "../msg_qconf.h"

namespace ocs {
   /** @brief `qconf -secl`: the registered event clients as a table
    *
    * Not the table sub-procedure of `-sc`: that one derives its column widths
    * from the data through a flatfile spooling instruction, while this table has
    * a hand written header, a fixed rule line and fixed widths. Re-deriving
    * those would be the one change a characterisation test cannot forgive, so
    * the format strings are carried over character for character.
    *
    * @ingroup libprocedure
    */
   class QConfEventClientView : public QConfViewBase {
   public:
      explicit QConfEventClientView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfEventClientView() override = default;

      /** @brief Not reached: the clients are written by show_clients() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the event clients, one line each or as a JSON array
       *
       * The host of a client that has not reported one is shown as `-`, and a
       * missing name as `(null)` - both are what the hand written switch did,
       * and the second one is also what keeps `operator<<` away from a null
       * pointer.
       *
       * @param os   the stream to write to
       * @param list the clients, already sorted by id
       */
      void show_clients(std::ostream &os, const lList *list) {
         if (output_format_ == ProcedureParameter::OutputFormat::JSON) {
            /* CS-2313a: an array of { id, name, host } objects; an empty list
             * still yields the enveloped empty array */
            static spooling_field ev_fields[] = {
               { EV_id,   0, "id",   false, nullptr, false, nullptr, nullptr, nullptr },
               { EV_name, 0, "name", false, nullptr, false, nullptr, nullptr, nullptr },
               { EV_host, 0, "host", false, nullptr, false, nullptr, nullptr, nullptr },
               { NoName,  0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr }
            };
            lList *alp = nullptr;
            dstring out = DSTRING_INIT;
            spool_json_write_typed_list(&alp, list, ev_fields, "event_clients", &out);
            const char *text = sge_dstring_get_string(&out);
            os << ((text != nullptr) ? text : "");
            sge_dstring_free(&out);
            lFreeList(&alp);
            return;
         }

         os << std::format("{:>8} {:<15} {:<25}\n", MSG_TABLE_EV_ID, MSG_TABLE_EV_NAME, MSG_TABLE_HOST);
         os << "--------------------------------------------------\n";

         const lListElem *ep;
         for_each_ep(ep, list) {
            const char *name = lGetString(ep, EV_name);
            const char *host = lGetHost(ep, EV_host);

            os << std::format("{:>8} ", static_cast<int>(lGetUlong(ep, EV_id)));
            os << std::format("{:<15} ", (name != nullptr) ? name : "(null)");
            os << std::format("{:<25}\n", (host != nullptr) ? host : "-");
         }
      }
   };
}
