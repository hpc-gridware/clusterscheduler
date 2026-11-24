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

/** @file
 * @brief The per-thread copies of the cluster's object lists
 */

#include <utility>

#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"

#include "sge_object.h"
#include "sge_event.h"
#include "ocs_object_type.h"

namespace ocs {
   /**
    * @brief Thread local access to the master lists, one snapshot per reader kind
    *
    * qmaster keeps several copies of the object model: one read-write store the
    * worker threads modify, and snapshots the scheduler, the listeners and the
    * readers work on without blocking writers. Each thread selects the store it
    * belongs to once via #select_active_ds; every later master list access
    * on that thread then resolves against it, so callers never pass a store
    * around.
    */
   class DataStore {
   private:
      // no private attributes because data is store thread local

      static ocs::tom::ListData **
      get_master_list_rw(sge_object_type type, bool for_read = false);

      /**
       * @brief The slot of a master list (read access) in the active data store of the calling thread
       *
       * Private: callers use the typed accessors (master_list() & co.) for a type known at compile time and
       * master_list_raw() & co. for a type given at run time; master_list_slot() remains for the readers that
       * fill a slot in place.
       *
       * @param type the type of the master list
       * @return the location of the list, never nullptr
       */
      static const ocs::tom::ListData **
      get_master_list(const sge_object_type type) {
         return const_cast<const ocs::tom::ListData **>(ocs::DataStore::get_master_list_rw(type, true));
      }

   public:
      /// The data stores a thread can be attached to
      enum Id {
         GLOBAL = 0,   ///< RW-DS (used by worker threads to handle RO-requests)
         SCHEDULER,    ///< Scheduler Snapshot (used by main scheduler thread)
         LISTENER,     ///< Listener Snapshot (used by listener threads)
         READER,       ///< Reader Snapshot (used by worker threads to handle RO-requests)
         MAX_ID,       ///< Maximum amount of thread local storages
      };

      static void
      select_active_ds(ocs::DataStore::Id ds_id);

      static ocs::DataStore::Id
      get_active_ds();

      static ocs::tom::ConstRawList
      master_list_raw(sge_object_type type);

      static ocs::tom::RawList
      master_list_raw_rw(sge_object_type type, bool for_read = false);

      static ocs::tom::RawListPtr
      replace_master_list_raw(sge_object_type type, ocs::tom::RawListPtr &&list);

      static ocs::tom::ListData **
      master_list_slot(sge_object_type type);

      /// The object type of the elements of the master list of #sge_object_type value V, e.g. ocs::tom::JB
      template <sge_object_type V>
      using master_type_t = typename ocs_object_type_t<V>::type;

      /**
       * @brief The master list of a type known at compile time (read access), from the active data store
       *
       * Calls get_master_list(), so the lock checks of debug builds stay in place. The handle is only valid
       * inside the lock scope of the data store it was obtained in; it must not be stored beyond it. A slot
       * that holds no list gives an empty handle, which iterates over nothing and reads like an empty list.
       *
       * @tparam V the #sge_object_type value of the master list; it must carry an object
       * @return the read-only handle on the list held in the slot, empty if the slot holds nullptr
       */
      template <sge_object_type V>
      static ocs::tom::ConstList<master_type_t<V>>
      master_list() {
         static_assert(ocs_object_type_t<V>::has_object, "the master list of a type that carries objects");
         // The owners create the list of a slot with the descriptor of its type, mirrors with a reduced
         // descriptor of that type: no check per access, this is the hot path of every request and the scheduler.
         return ocs::tom::ConstList<master_type_t<V>>::from_typed(*get_master_list(V));
      }

      /**
       * @brief The master list of a type known at compile time (write access), from the active data store
       *
       * Calls get_master_list_rw(), so the lock checks of debug builds stay in place. The handle is only valid
       * inside the lock scope of the data store it was obtained in; it must not be stored beyond it. The handle
       * allows to change the list held in the slot, not to put another list into the slot: use
       * replace_master_list() for that. A slot that holds no list gives an empty handle.
       *
       * @tparam V the #sge_object_type value of the master list; it must carry an object
       * @param for_read true when the caller only reads, as for get_master_list_rw()
       * @return the handle on the list held in the slot, empty if the slot holds nullptr
       */
      template <sge_object_type V>
      static ocs::tom::List<master_type_t<V>>
      master_list_rw(const bool for_read = false) {
         static_assert(ocs_object_type_t<V>::has_object, "the master list of a type that carries objects");
         // The owners create the list of a slot with the descriptor of its type, mirrors with a reduced
         // descriptor of that type: no check per access, this is the hot path of every request and the scheduler.
         return ocs::tom::List<master_type_t<V>>::from_typed(*get_master_list_rw(V, for_read));
      }

      /**
       * @brief The master list of a type known at compile time (write access), created if the slot holds none
       *
       * Calls get_master_list_rw(), so the lock checks of debug builds stay in place. The caller must hold the
       * lock of the data store for writing. The handle is only valid inside the lock scope of the data store it
       * was obtained in; it must not be stored beyond it.
       *
       * @tparam V the #sge_object_type value of the master list; it must carry an object
       * @param name the name of the list if it is created
       * @return the handle on the list held in the slot, never empty
       */
      template <sge_object_type V>
      static ocs::tom::List<master_type_t<V>>
      master_list_create(const char *name) {
         static_assert(ocs_object_type_t<V>::has_object, "the master list of a type that carries objects");
         ocs::tom::ListData **slot = get_master_list_rw(V);
         if (*slot == nullptr) {
            *slot = ocs::tom::ListPtr<master_type_t<V>>::create(name).release();
         }
         // the list in the slot was created as a list of this type, by its owner or just above
         return ocs::tom::List<master_type_t<V>>::from_typed(*slot);
      }

      /**
       * @brief Put a list into the slot of a master list of a type known at compile time
       *
       * Calls get_master_list_rw(), so the lock checks of debug builds stay in place. The caller must hold the
       * lock of the data store for writing.
       *
       * @tparam V the #sge_object_type value of the master list; it must carry an object
       * @param list the list to put into the slot, may be empty; it is consumed
       * @return the list the slot held before, owned by the caller; empty if the slot held nullptr
       */
      template <sge_object_type V>
      static ocs::tom::ListPtr<master_type_t<V>>
      replace_master_list(ocs::tom::ListPtr<master_type_t<V>> &&list) {
         static_assert(ocs_object_type_t<V>::has_object, "the master list of a type that carries objects");
         ocs::tom::ListPtr<master_type_t<V>> new_list(std::move(list));
         ocs::tom::ListData **slot = get_master_list_rw(V);
         // the list in the slot was put there as a list of this type, by its owner or by this function
         return ocs::tom::ListPtr<master_type_t<V>>::from_typed(std::exchange(*slot, new_list.release()));
      }

      static void
      free_master_list(sge_object_type type);

      static void
      free_all_master_lists();

      /**
       * @brief The event client id feeding a snapshot store
       * @param data_store_id the store to ask about
       * @return the event client registration id that keeps it up to date
       */
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
