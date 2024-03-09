#include <pthread.h>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

#include "basis_types.h"
#include "sge_object.h"
#include "oge_DataStore.h"
#include "msg_sgeobjlib.h"

#define DATA_STORE_LAYER TOP_LAYER

// the key to get thread local memory
static pthread_key_t obj_state_key;
static pthread_once_t obj_once = PTHREAD_ONCE_INIT;

/// thread local storage
struct obj_thread_local_t {
   oge::DataStore::Id ds_id; ///< default data store ID that should be used by a thread
};

/// data store structure. hold all master lists.
struct obj_data_store_t {
   lList *master_list[SGE_TYPE_ALL]; ///< master list
};

/// thread shared storage data type.
struct obj_thread_shared_t {
   obj_data_store_t data_store[oge::DataStore::Id::MAX_ID + 1]; ///< all data stores that are available
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
   for (int ds_id = oge::DataStore::Id::GLOBAL; ds_id <= oge::DataStore::Id::MAX_ID; ds_id++) {
      for (int list_id = 0; list_id < SGE_TYPE_ALL; list_id++) {
         obj_thread_shared.data_store[ds_id].master_list[list_id] = nullptr;
      }
   }
}

/// trigger for the one time initializer
static void obj_mt_init() {
   pthread_once(&obj_once, obj_thread_local_once_init);
}

class ObjectThreadInit {
public:
   ObjectThreadInit() {
      obj_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static ObjectThreadInit object_obj{};

/// initialize thread local storage so that a thread uses the OBJ_STATE_GLOBAL data store if this not changed later on.
static void
obj_state_init(obj_thread_local_t *state) {
   DENTER(TOP_LAYER);

   // as default each thread will access the OBJ_STATE_GLOBAL data store
   // if it does not change to a different one with oge::DataStore::select_active_ds()
   state->ds_id = oge::DataStore::Id::GLOBAL;
   DRETURN_VOID;
}

/**
 * Change the active data store of the calling thread to ds.
 * If this function is not called then OBJ_STATE_GLOBAL will be used as default.
 *
 * @param ds_id new data store that should be used.
 */
void
oge::DataStore::select_active_ds(oge::DataStore::Id ds_id) {
   DENTER(TOP_LAYER);
   GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);
   DPRINTF("thread will use data store %d\n", ds_id);
   obj_state->ds_id = ds_id;
   DRETURN_VOID;
}

/**
 * Returns the master list (RW-access) of the currently active data store for the specified type.
 *
 * @param type master list type
 * @return pointer to the master list. will never be nullptr.
 */
lList **
oge::DataStore::get_master_list_rw(sge_object_type type) {
   DENTER(TOP_LAYER);

   if (type < SGE_TYPE_ALL) {
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);

      DPRINTF("ds: %d, type: %d, list: %p\n", obj_state->ds_id, type,
              obj_thread_shared.data_store[obj_state->ds_id].master_list[type]);

      lList **ret;
      ret = &(obj_thread_shared.data_store[obj_state->ds_id].master_list[type]);
#ifdef OBSERVE
      if (*obj_state->object_base[type].list) {
         lObserveChangeListType(*obj_state->object_base[type].list, true, obj_state->object_base[type].type_name);
      }
#endif
      DRETURN(ret);
   } else {
      ERROR(MSG_OBJECT_INVALID_OBJECT_TYPE_SI, __func__, type);
      DRETURN(nullptr);
   }
}

/**
 * Returns the master list (RO-access) of the currently active data store for the specified type.
 *
 * @param type master list type
 * @return pointer to the master list. will never be nullptr.
 */
const lList **oge::DataStore::get_master_list(sge_object_type type) {
   return (const lList **) oge::DataStore::get_master_list_rw(type);
}

// TODO: remove because duplicate functionality. use cull directly!
lListElem *oge::DataStore::get_master_str_elem_rw(sge_object_type type, int key_nm, const char *key) {
   DENTER(DATA_STORE_LAYER);
   lList **master_list = oge::DataStore::get_master_list_rw(type);
// master_list cannot be nullptr
#if 0
   if (master_list == nullptr) {
      DRETURN(nullptr);
   }
#endif
   DRETURN(lGetElemStrRW(*master_list, key_nm, key));
}

// TODO: remove because duplicate functionality. use cull directly!
const lListElem *oge::DataStore::get_master_str_elem(sge_object_type type, int key_nm, const char *key) {
   return oge::DataStore::get_master_str_elem_rw(type, key_nm, key);
}

// TODO: nowhere used. would expect that at least one method should be called that releases the complete DS.
bool oge::DataStore::free_master_list(sge_object_type type) {
   DENTER(DATA_STORE_LAYER);
   if (type < SGE_TYPE_ALL) {
      GET_SPECIFIC(obj_thread_local_t, obj_state, obj_state_init, obj_state_key);

      if (obj_thread_shared.data_store[obj_state->ds_id].master_list[type]) {
         lFreeList(&obj_thread_shared.data_store[obj_state->ds_id].master_list[type]);
      }
      DRETURN(true);
   } else {
      ERROR(MSG_OBJECT_INVALID_OBJECT_TYPE_SI, __func__, type);
      DRETURN(false);
   }
}
