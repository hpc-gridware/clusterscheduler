#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "sgeobj/cull/sge_ct_CT_L.h"

namespace ocs {
   class Category {
      static u_long32 next_id;

   public:
      static u_long32 get_next_id(lList *master_category_list) {
         u_long32 id;

         // do not use 0 as ID or IDs that already have been used
         do {
            id = next_id++;
         } while (id == 0 || lGetElemUlong(master_category_list, CT_id, id) != nullptr);
         return id;
      }
      static void build_string(dstring *category_str, lListElem *job, const lList *acl_list, const lList *prj_list, const lList *rqs_list);
   };
}
