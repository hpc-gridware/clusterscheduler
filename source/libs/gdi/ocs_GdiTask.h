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

#include <string>

#include "basis_types.h"

#include "cull/cull.h"

#include "gdi/ocs_GdiTarget.h"

namespace ocs {
   class GdiTask {
   public:
      /*
       * common parts of a GDI request
       */
      u_long32 command;
      GdiTarget::Target target;
      lList *data_list;
      lList *answer_list;
      lCondition *condition;
      lEnumeration *enumeration;

      /*
       * This flag is used in qmaster to identify if a special
       * optimization can be done. This optimization can only be
       * done for GDI GET requests where the client is
       * an external GDI client (no thread using GDI).
       *
       * In that case it is possible that the lSelectHashPack()
       * function is called with a packbuffer so that the function
       * directly packs into this packbuffer.
       *
       * This avoids a copy operation
       */
      bool do_select_pack_simultaneous;
   public:
      GdiTask(GdiTarget::Target target, u_long32 command, lList **lp,
              lList **a_list, lCondition **condition, lEnumeration **enumeration, bool do_copy);
      GdiTask();
      ~GdiTask();

      void debug_print();

      const char *get_operation_name();
   };
}
