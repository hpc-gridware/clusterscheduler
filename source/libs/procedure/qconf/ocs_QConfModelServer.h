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
   /** @brief What produces the qmaster thread pool list
    *
    * The thread pools are the running daemon's own state, so the function that
    * builds the list lives in `daemons/qmaster`. A library must not call into a
    * daemon, so qmaster hands it in at startup and the server side model calls
    * back through this (CS-2820). It is the only read of the whole qconf set
    * whose data is not a configuration object - which is also why RBAC output
    * reduction has nothing to do here: there is no object and no owner, only the
    * question whether a caller may see the daemon's thread configuration at all.
    *
    * @return a newly created ST_Type list the caller owns
    */
   using qconf_thread_pool_provider = lList *(*)();

   /** @brief Tell the server side model how to reach the thread pools
    * @param provider the function qmaster offers, or nullptr to clear it
    */
   void qconf_set_thread_pool_provider(qconf_thread_pool_provider provider);

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
      bool fetch_thread_pools(lList **answer_list) override;
      bool fetch_centries(lList **answer_list) override;
      bool fetch_hgroups(lList **answer_list) override;

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
