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
 * @brief TODO describe this file
 */

#include "ocs_MirrorServerDataStore.h"

namespace ocs {
   class MirrorListenerDataStore : public MirrorServerDataStore {
   public:
      MirrorListenerDataStore() : MirrorServerDataStore(DataStore::Id::LISTENER, LOCK_LISTENER) {};
      ~MirrorListenerDataStore() override = default;
      void subscribe_events() override;
      void update_sessions_and_move_requests(uint64_t unique_id) override {};
   };
}
