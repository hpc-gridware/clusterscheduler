/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_lock.h"
#include "uti/ocs_ThreadName.h"

#include <cinttypes>
#include "sge_object.h"
#include "ocs_DataStore.h"

/// Debug layer the data store traces are written to
#define DATA_STORE_LAYER BASIS_LAYER

namespace ocs {
   // the key to get thread local memory
   static pthread_key_t obj_state_key;
   static pthread_once_t obj_once = PTHREAD_ONCE_INIT;

   /// thread local storage
   struct obj_thread_local_t {
      DataStore::Id ds_id; ///< default data store ID that should be used by a thread
   };

   /// data store structure. hold all master lists.
   struct obj_data_store_t {
      lList *master_list[SGE_TYPE_ALL]; ///< master list
   };

   /// thread shared storage data type.
   struct obj_thread_shared_t {
      obj_data_store_t data_store[DataStore::Id::MAX_ID]; ///< all data stores that are available
   };

   /// thread shared storage. the instance that holds all data stores
   static obj_thread_shared_t obj_thread_shared{};

   /// releases the thread local storage.
   static void
   obj_state_destroy(void *st) {
      auto *tlocal = (obj_thread_local_t *) st;
      sge_free(&tlocal);
   }

   /// one time initializer that create the pthread key and the thread shared storage (== all data stores)
   static void
   obj_thread_local_once_init() {
      pthread_key_create(&obj_state_key, obj_state_destroy);

      // initialize thread shared storage that hold all data stores (== master lists)
      for (int ds_id = DataStore::Id::GLOBAL; ds_id < DataStore::Id::MAX_ID; ds_id++) {
         for (auto &master_list: obj_thread_shared.data_store[ds_id].master_list) {
            master_list = nullptr;
         }
      }
   }

   /// trigger for the one time initializer
   static void obj_mt_init() {
      pthread_once(&obj_once, obj_thread_local_once_init);
   }

   /// Runs the one time initialiser from its constructor, so the pthread key exists before main()
   class ObjectThreadInit {
   public:
      /// Triggers the one time initialiser
      ObjectThreadInit() {
         obj_mt_init();
      }
   };

   // although not used the constructor call has the side effect to initialize the pthread_key => do not delete
   static ObjectThreadInit object_obj{};

   /// initialize thread local storage so that a thread uses the OBJ_STATE_GLOBAL data store if this not changed later on.
   static void
   obj_state_init(obj_thread_local_t *state) {
      DENTER(DATA_STORE_LAYER);

      // as default each thread will access the OBJ_STATE_GLOBAL data store
      // if it does not change to a different one with ocs::DataStore::select_active_ds()
      state->ds_id = DataStore::Id::GLOBAL;
      DRETURN_VOID;
   }

   /**
    * Change the active data store of the calling thread to ds.
    * If this function is not called then OBJ_STATE_GLOBAL will be used as default.
    *
    * @param ds_id new data store that should be used.
    */
   void
   DataStore::select_active_ds(DataStore::Id ds_id) {
      DENTER(DATA_STORE_LAYER);
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);
      obj_state->ds_id = ds_id;
      DRETURN_VOID;
   }

   /**
    * @brief Returns the data store which is active for the calling thread.
    *
    * Needed where code may run in more than one thread context and has to know which data it is
    * about to read - a thread which has its own snapshot selected does not see the master lists.
    *
    * @return the active data store of this thread
    */
   DataStore::Id
   DataStore::get_active_ds() {
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);
      return obj_state->ds_id;
   }

#if defined (ENABLE_DEBUG_CHECKS)
   // the locks are declared in the order of the data stores they guard
   static_assert(static_cast<int>(LOCK_GLOBAL) == static_cast<int>(DataStore::Id::GLOBAL));
   static_assert(static_cast<int>(LOCK_SCHEDULER) == static_cast<int>(DataStore::Id::SCHEDULER));
   static_assert(static_cast<int>(LOCK_LISTENER) == static_cast<int>(DataStore::Id::LISTENER));
   static_assert(static_cast<int>(LOCK_READER) == static_cast<int>(DataStore::Id::READER));

   /**
    * @brief Returns the lock which protects a data store
    *
    * @param ds_id the data store
    * @return the lock guarding it
    */
   static sge_locktype_t
   data_store_lock(DataStore::Id ds_id) {
      return static_cast<sge_locktype_t>(ds_id);
   }

   /**
    * @brief Whether the lock check applies to a thread
    *
    * The main thread is exempt: it reads the spooled objects into the master lists while sge_setup_qmaster()
    * starts the qmaster, and frees them again at shutdown, both while no other thread uses the data stores, so
    * it takes no lock for them. Threads without a qmaster thread name (the commlib threads) never use the master
    * lists.
    *
    * @param thread_name the component name of the calling thread
    * @return true for a known qmaster thread name other than the main thread's
    */
   static bool
   is_checked_thread(const char *thread_name) {
      if (thread_name == nullptr) {
         return false;
      }
      const std::string_view name{thread_name};
      for (int t = 0; t < THREAD_TYPE_COUNT; t++) {
         if (t != MAIN_THREAD && to_string_view(static_cast<ThreadName>(t)) == name) {
            return true;
         }
      }
      return false;
   }
#endif

   /**
    * @brief Returns the master list (RW-access) of the currently active data store for the specified type.
    *
    * @param type master list type
    * @param for_read true when the caller only reads, so a snapshot store may answer
    * @return pointer to the master list. will never be nullptr.
    */
   lList **
   DataStore::get_master_list_rw(sge_object_type type, bool for_read) {
      DENTER(DATA_STORE_LAYER);
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);

#if defined (ENABLE_DEBUG_CHECKS)
      auto ds_id = obj_state->ds_id;
      const char *thread_name = component_get_thread_name();
      if (thread_name != nullptr) {
         if (strcmp(thread_name, "worker") == 0 && ds_id != DataStore::Id::GLOBAL) {
            CRITICAL("Worker thread is trying to access data store %d for list %d", ds_id, type);
            ocs::TerminationManager::trigger_abort();
         }

         if (strcmp(thread_name, "reader") == 0) {
            if (ds_id != DataStore::Id::READER) {
               CRITICAL("Reader thread is trying to access data store %d for list %d", ds_id, type);
               ocs::TerminationManager::trigger_abort();
            }
            // @todo enable once CS-825 is fixed
#if 0
            if (!for_read) {
               CRITICAL("Reader thread is trying to get master list with write access");
               ocs::TerminationManager::trigger_abort();
            }
#endif
         }
      }

      // A qmaster thread must hold the lock of the data store whose master list it reads. Only the threads
      // with a known qmaster thread name are checked: the main thread loads and frees the lists while no
      // other thread runs, and threads without such a name (commlib) never touch them. The scheduler data
      // store belongs to the scheduler thread alone and has no lock.
      if (component_get_component_id() == QMASTER && is_checked_thread(thread_name)
          && ds_id != DataStore::Id::SCHEDULER) {
         auto lock = data_store_lock(ds_id);
         // the configuration list of the global data store is guarded by the master configuration lock
         const bool held = ocs_lock_is_held(lock)
                           || (ds_id == DataStore::Id::GLOBAL && type == SGE_TYPE_CONFIG && ocs_lock_is_held(LOCK_MASTER_CONF));
         if (!held) {
            CRITICAL("Thread %s is trying to access list %d of data store %d without holding its lock",
                     thread_name, type, ds_id);
            ocs::TerminationManager::trigger_abort();
         }
         // @todo enable once CS-825 is fixed and a full testsuite run shows no violation
#if 0
         if (!for_read && !ocs_lock_is_held_for_write(lock)) {
            CRITICAL("Thread %s is trying to get list %d of data store %d with write access holding only a read lock",
                     thread_name, type, ds_id);
            ocs::TerminationManager::trigger_abort();
         }
#endif
      }
#endif

      lList **ret;
      ret = &(obj_thread_shared.data_store[obj_state->ds_id].master_list[type]);
#ifdef OBSERVE
      DPRINTF("ds: %d, type: %d, list: %p\n", obj_state->ds_id, type, *ret);
      if (*obj_state->object_base[type].list) {
         lObserveChangeListType(*obj_state->object_base[type].list, true, obj_state->object_base[type].type_name);
      }
#endif
      DRETURN(ret);
   }

   /**
    * Free the master list which has the given type.
    *
    * @param type master list type.
    */
   void
   DataStore::free_master_list(sge_object_type type) {
      DENTER(DATA_STORE_LAYER);
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);
      lFreeList(get_master_list_rw(type));
      DRETURN_VOID;
   }

   /**
    * Free all master lists in the currently active data store of the thread.
    *
    * @todo nowhere used. would expect that at least one method should be called that releases the complete DS.
    * @todo correct place is either the event client (e.g. the scheduler) or the mirror thread that we will introduce.
    */
   void
   DataStore::free_all_master_lists() {
      DENTER(DATA_STORE_LAYER);
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);
      for (int type = SGE_TYPE_FIRST; type < SGE_TYPE_ALL; type++) {
         lFreeList(get_master_list_rw(static_cast<sge_object_type>(type)));
      }
      DRETURN_VOID;
   }
}
