#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "oge_MirrorDataStore.h"

namespace oge {
   class MirrorListenerDataStore : public MirrorDataStore {
   public:
      MirrorListenerDataStore();
      ~MirrorListenerDataStore() override = default;
      void subscribe_events() override;
   };
}
