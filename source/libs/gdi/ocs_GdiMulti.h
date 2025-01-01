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

#include "cull/cull.h"

#include "gdi/ocs_GdiTarget.h"
#include "gdi/ocs_GdiMode.h"

struct sge_gdi_packet_class_t;

namespace ocs {
   class GdiMulti {
   public:
      sge_gdi_packet_class_t *packet;
      lList *multi_answer_list;

      GdiMulti();
      ~GdiMulti();

      void wait();
      int request(lList **alpp, GdiMode::Mode mode, GdiTarget::Target target, u_long32 cmd, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy);
      bool get_response(lList **alpp, u_long32 cmd, GdiTarget::Target target, int id, lList **olpp);
   };
}
