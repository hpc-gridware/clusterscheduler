/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "mir/sge_mirror.h"

#include "oge_MirrorReaderDataStore.h"

oge::MirrorReaderDataStore::MirrorReaderDataStore() : MirrorDataStore(oge::DataStore::Id::READER) {
   ;
}

void oge::MirrorReaderDataStore::subscribe_events() {
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, nullptr, nullptr, nullptr, nullptr, nullptr);
}
