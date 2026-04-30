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

#include "gdi/ocs_gdi_Request.h"

#include "ocs_QQuotaModelBase.h"

namespace ocs {
   /** @brief QQuota model for the server execution context.
    *
    * Implements `fetch_data()` by reading directly from in-process qmaster master
    * lists, avoiding any GDI network round-trip.  Constructed with the originating
    * client's `gdi::Packet` so that permission checks reflect the real caller.
    *
    * @see QQuotaModelBase for the full pipeline description.
    * @ingroup libprocedure
    */
   class QQuotaModelServer : public QQuotaModelBase {
      [[maybe_unused]] gdi::Packet *packet = nullptr;
      [[maybe_unused]] gdi::Task *task = nullptr;
   public:
      QQuotaModelServer(gdi::Packet *packet, gdi::Task *task) : packet(packet), task(task) {};
      ~QQuotaModelServer() override = default;

      bool fetch_data(lList **answer_list, const lList *host_list) override;
   };
}
