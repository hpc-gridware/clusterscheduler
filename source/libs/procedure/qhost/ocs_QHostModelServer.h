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

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QHostModelBase.h"

namespace ocs {
   /** @brief QHost model for the server execution context.
    *
    * Implements `fetch_data()` by reading directly from in-process qmaster master
    * lists, avoiding any GDI network round-trip.  Constructed with the originating
    * client's `gdi::Packet` so that permission checks reflect the real caller.
    *
    * @see QHostModelBase for the full pipeline description.
    * @ingroup libprocedure
    */
   class QHostModelServer : public QHostModelBase {
      [[maybe_unused]] gdi::Packet *packet = nullptr;
      [[maybe_unused]] gdi::Task *task = nullptr;
      
   protected:

      bool fetch_data(lList **answer_list, const lList *hostname_list, const lList *user_name_list, uint32_t show) override;

   public:

      QHostModelServer(gdi::Packet *packet, gdi::Task *task) : packet(packet), task(task) {};
      ~QHostModelServer() override = default;
   };
}
