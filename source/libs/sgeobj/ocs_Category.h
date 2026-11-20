#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Job categories, grouping jobs with identical scheduling requirements
 */

#include "cull/cull.h"

#include "uti/sge_dstring.h"

#include "sgeobj/cull/sge_ct_CT_L.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_US.h"

namespace ocs {
   /// Job categories: jobs with identical scheduling relevant requests share one category
   class Category {
      static uint32_t next_id;

   public:
      static uint32_t get_next_id(ocs::tom::ConstList<ocs::tom::CT> master_category_list);
      static void build_string(dstring *category_str, ocs::tom::Element<ocs::tom::JB> job,
                               ocs::tom::ConstList<ocs::tom::US> acl_list, ocs::tom::ConstList<ocs::tom::PR> prj_list,
                               ocs::tom::ConstList<ocs::tom::RQS> rqs_list);
   };
}
