/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "mir/sge_mirror.h"

#include "oge_MirrorListenerDataStore.h"

oge::MirrorListenerDataStore::MirrorListenerDataStore() : MirrorDataStore(oge::DataStore::Id::LISTENER) {
   ;
}

void oge::MirrorListenerDataStore::subscribe_events() {
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, nullptr, nullptr, nullptr, nullptr, nullptr);
}
