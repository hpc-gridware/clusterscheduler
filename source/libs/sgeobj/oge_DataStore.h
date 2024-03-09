#pragma once

#include "sge_object.h"

// array index in ds array

namespace oge {
   class DataStore {
   private:
      // no private attributes because data is store thread local
   public:
      enum Id {
         GLOBAL = 0,      ///< RW-DS (used by worker threads to handle RO-requests)
         SCHEDULER,       ///< Scheduler Snapshot (used by main scheduler thread)
         AUTH,            ///< Auth Snapshot (used by listener threads)
         READER,          ///< Reader Snapshot (used by worker threads to handle RO-requests)
         MAX_ID = READER, ///< Maximum amount of thread local storages
      };

      static void select_active_ds(oge::DataStore::Id ds_id);
      static lList **get_master_list_rw(sge_object_type type);
      static const lList **get_master_list(sge_object_type type);
      static lListElem *get_master_str_elem_rw(sge_object_type type, int key_nm, const char *key);
      static const lListElem *get_master_str_elem(sge_object_type type, int key_nm, const char *key);
      static bool free_master_list(sge_object_type type);
   };
}
