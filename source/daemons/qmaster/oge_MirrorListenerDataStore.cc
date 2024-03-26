/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "mir/sge_mirror.h"

#include "oge_MirrorListenerDataStore.h"

namespace oge {
   MirrorListenerDataStore::MirrorListenerDataStore() : MirrorDataStore(DataStore::Id::LISTENER, LOCK_LISTENER) {
      ;
   }

   void MirrorListenerDataStore::subscribe_events() {
      sge_mirror_subscribe(evc, SGE_TYPE_ADMINHOST, nullptr, nullptr, nullptr, nullptr, nullptr);
      sge_mirror_subscribe(evc, SGE_TYPE_SUBMITHOST, nullptr, nullptr, nullptr, nullptr, nullptr);
      sge_mirror_subscribe(evc, SGE_TYPE_HGROUP, nullptr, nullptr, nullptr, nullptr, nullptr);
      sge_mirror_subscribe(evc, SGE_TYPE_MANAGER, nullptr, nullptr, nullptr, nullptr, nullptr);
      sge_mirror_subscribe(evc, SGE_TYPE_OPERATOR, nullptr, nullptr, nullptr, nullptr, nullptr);

      evc->ec_set_flush(evc, sgeE_ADMINHOST_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_DEL, true, 0);

      evc->ec_set_flush(evc, sgeE_SUBMITHOST_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_DEL, true, 0);

      evc->ec_set_flush(evc, sgeE_HGROUP_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_DEL, true, 0);

      evc->ec_set_flush(evc, sgeE_MANAGER_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_DEL, true, 0);

      evc->ec_set_flush(evc, sgeE_OPERATOR_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_DEL, true, 0);

      evc->ec_set_edtime(evc, 15);

      // no need to call evc->ec_commit(). This is done directly after this method returns.
   }
}