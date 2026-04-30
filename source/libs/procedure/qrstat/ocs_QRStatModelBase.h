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

#include "qrstat/ocs_QRStatParameterClient.h"

namespace ocs {
   /** @brief Base model for qrstat that holds the advance reservation (AR) list.
    *
    * Fetches the AR list filtered and projected according to the given parameters,
    * then passes it to the view for rendering.
    *
    * ## Client vs. server execution
    *
    * - **QRStatModelClient** (`ExecContext::CLIENT`): issues a GDI request to qmaster
    *   to fetch the filtered AR list.
    * - **QRStatModelServer** (`ExecContext::SERVER`): reads directly from the in-process
    *   qmaster master lists, avoiding any network round-trip.
    *
    * ## Virtual pipeline inside make_snapshot()
    *
    * 1. `fetch_data()` — populates `ar_list_` (overridden per context).
    *
    * @ingroup libprocedure
    */
   class QRStatModelBase {
   public:
      lList *ar_list_ = nullptr; ///< Advance reservation list; populated by fetch_data().

   protected:
      static lEnumeration *get_ar_what(QRStatParameter& parameter);
      static lCondition *get_ar_where(QRStatParameter& parameter);

      /** @brief Fetch the AR list into ar_list_.
       *
       * Overridden by QRStatModelClient (GDI call) and QRStatModelServer (master list).
       */
      virtual bool fetch_data(lList **answer_list, QRStatParameter& parameter);

   public:
      [[nodiscard]] const lList *get_ar_list() const { return ar_list_; }

   public:
      QRStatModelBase() = default;
      virtual ~QRStatModelBase();

      /** @brief Run the full pipeline: fetch_data, then hand off to the view.
       *
       * @param answer_list  Receives error messages on failure.
       * @param parameter    Parsed qrstat parameters.
       * @return true if fetch_data succeeded.
       */
      bool make_snapshot(lList **answer_list, QRStatParameter &parameter);
   };
}
