/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include "uti/sge_log.h"

#include "mir/sge_mirror.h"

#include "evm/sge_event_master.h"

#include "ocs_MirrorDataStore.h"
#include "ocs_MirrorServerDataStore.h"
#include "setup_qmaster.h"

/** @brief Constructor that initializes a data store within the qmaster process
 *
 * @param data_store_id Data store that will be filled with data.
 * @param lock_type Lock that should be used to secure the data store
 */
ocs::MirrorServerDataStore::MirrorServerDataStore(ocs::DataStore::Id data_store_id, sge_locktype_t lock_type) : MirrorDataStore(data_store_id, lock_type) {
   // Additional data stores are not allowed to use the global lock
   SGE_ASSERT(lock_type != LOCK_GLOBAL);
   SGE_ASSERT(data_store_id != DataStore::Id::GLOBAL);
}

/** @brief Initialize the calling thread as event mirror thread in qmaster */
void
ocs::MirrorServerDataStore::init_connection() {
   sge_qmaster_thread_init(QMASTER, EVENT_MIRROR_THREAD, true);
}

/** @brief Initialize the event mirror */
void
ocs::MirrorServerDataStore::init_event_mirror() {
   // Initialize the event mirror as if we are within the qmaster
   sge_mirror_initialize(evc, event_mirror_update_func, &sge_mod_event_client,
                         &sge_add_event_client, &sge_remove_event_client, &sge_handle_event_ack, this);
}
