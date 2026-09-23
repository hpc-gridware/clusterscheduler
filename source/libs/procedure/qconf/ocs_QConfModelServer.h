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
 * @brief Reading a `qconf` read operation's objects from the master lists
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"

#include "ocs_QConfModelBase.h"

namespace ocs {
   /** @brief The server side model: read qmaster's own lists
    * @ingroup libprocedure
    */
   class QConfModelServer : public QConfModelBase {
#pragma region Data

   protected:
      gdi::Packet *packet_ = nullptr;   ///< the requesting client, for user and permissions
      gdi::Task *task_ = nullptr;       ///< the request being served
#pragma endregion

   protected:
      bool fetch(lList **answer_list, gdi::Target target,
                 lCondition *where, lEnumeration *what) override;
      bool fetch_centries(lList **answer_list) override;

   public:
      /** @brief Build the server side model
       * @param packet the originating client packet
       * @param task   the request being served
       */
      QConfModelServer(gdi::Packet *packet, gdi::Task *task)
         : packet_(packet), task_(task) {}

      ~QConfModelServer() override = default;
   };
}
