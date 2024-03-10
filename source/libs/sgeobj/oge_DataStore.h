#pragma once
/*___INFO__MARK_BEGIN_CLOSED__*/
/*___INFO__MARK_END_CLOSED__*/

#include "sge_object.h"

namespace oge {
   class DataStore {
   private:
      // no private attributes because data is store thread local

   public:
      enum Id {
         GLOBAL = 0,   ///< RW-DS (used by worker threads to handle RO-requests)
         SCHEDULER,    ///< Scheduler Snapshot (used by main scheduler thread)
         AUTH,         ///< Auth Snapshot (used by listener threads)
         READER,       ///< Reader Snapshot (used by worker threads to handle RO-requests)
         MAX_ID,       ///< Maximum amount of thread local storages
      };

      static void
      select_active_ds(oge::DataStore::Id ds_id);

      static lList **
      get_master_list_rw(sge_object_type type);

      /**
       * Returns a master list (RO-access) from the currently active data store of the active threads
       * @param type Type of the master list
       * @return Pointer of the list location. Will never be nullptr.
       */
      static inline const lList **
      get_master_list(sge_object_type type) {
         return const_cast<const lList **>(oge::DataStore::get_master_list_rw(type));
      }

      static void
      free_master_list(sge_object_type type);

      static void
      free_all_master_lists();
   };
}
