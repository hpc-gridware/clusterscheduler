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
 * @brief The rendering of `qconf -sds` (CS-2819)
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -sds`: the queues whose configuration cannot take effect
    *
    * The model has already produced the whole report, because building it needs
    * the answer list, so this only writes it. It is a sub-procedure rather than a
    * descriptor row because the output is neither a name list nor an object: it
    * is a finding about several objects at once.
    *
    * @ingroup libprocedure
    */
   class QConfSickView : public QConfViewBase {
   public:
      explicit QConfSickView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfSickView() override = default;

      /** @brief Not reached: the report is written by show_report() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the report the model built
       * @param os     the stream to write to
       * @param report the report, empty when every queue is sound
       */
      void show_report(std::ostream &os, const std::string &report) { os << report; }
   };
}
