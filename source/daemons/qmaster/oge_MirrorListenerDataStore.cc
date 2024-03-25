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
   }

}