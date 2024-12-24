#pragma once
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

#include "sge_object.h"

#include "sge_event.h"

namespace ocs {
   class DataStore {
   private:
      // no private attributes because data is store thread local

   public:
      enum Id {
         GLOBAL = 0,   ///< RW-DS (used by worker threads to handle RO-requests)
         SCHEDULER,    ///< Scheduler Snapshot (used by main scheduler thread)
         LISTENER,     ///< Listener Snapshot (used by listener threads)
         READER,       ///< Reader Snapshot (used by worker threads to handle RO-requests)
         MAX_ID,       ///< Maximum amount of thread local storages
      };

      static void
      select_active_ds(ocs::DataStore::Id ds_id);

      static lList **
      get_master_list_rw(sge_object_type type, bool for_read = false);

      /**
       * Returns a master list (RO-access) from the currently active data store of the active threads
       * @param type Type of the master list
       * @return Pointer of the list location. Will never be nullptr.
       */
      static inline const lList **
      get_master_list(sge_object_type type) {
         return const_cast<const lList **>(ocs::DataStore::get_master_list_rw(type, true));
      }

      static void
      free_master_list(sge_object_type type);

      static void
      free_all_master_lists();

      static ev_registration_id
      get_ev_id_for_data_store(ocs::DataStore::Id data_store_id) {
         switch (data_store_id) {
            case DataStore::GLOBAL:
               return EV_ID_ANY;
            case DataStore::SCHEDULER:
               return EV_ID_SCHEDD;
            case DataStore::LISTENER:
               return EV_ID_EVENT_MIRROR_LISTENER;
            case DataStore::READER:
               return EV_ID_EVENT_MIRROR_READER;
            default:
               return EV_ID_ANY;
         }
      }
   };
}
