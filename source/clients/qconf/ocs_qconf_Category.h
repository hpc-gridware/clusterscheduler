#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include "sgeobj/ocs_Category.h"

namespace ocs {
   class CategoryQconf {
   public:
      static bool
      show_list(lList **answer_list);

      static bool
      show(lList **answer_list, u_long32 id);

      static lList *
      get_via_gdi(lList **answer_list);

      static lListElem *
      get_via_gdi(lList **answer_list, u_long64 id);
   };
}