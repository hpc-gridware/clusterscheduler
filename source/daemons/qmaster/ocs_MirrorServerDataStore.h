#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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
 * @brief Base of the read-only data stores kept current by a mirror thread
 */

#include "mir/ocs_MirrorDataStore.h"

namespace ocs {
   /** @brief Base of the read-only data stores kept current by a mirror thread
    *
    * qmaster answers read-only requests from private copies of the master
    * lists rather than from the live ones, so a `qstat` storm never takes a
    * lock a `qsub` is waiting for. Each copy has its own thread applying the
    * event stream to it, and its own lock.
    */
   class MirrorServerDataStore : public MirrorDataStore {
   public:
      MirrorServerDataStore(ocs::DataStore::Id data_store_id, sge_locktype_t lock_type);
      ~MirrorServerDataStore() override = default;
      void init_event_mirror() override;
      void init_connection() override;
#if 0
      void subscribe_events() override = 0;
      void update_sessions_and_move_requests(uint64_t unique_id) override = 0;
#endif
   };
}
