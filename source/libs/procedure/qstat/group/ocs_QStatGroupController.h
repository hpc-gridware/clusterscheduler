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

#include "qstat/ocs_QStatParameter.h"
#include "qstat/ocs_QStatModelClient.h"

#include "qstat/group/ocs_QStatGroupViewBase.h"

namespace ocs {
   class QStatGroupController {
      std::ostream &out_;

      bool cqueue_calculate_summary(const lListElem *cqueue, const lList *exechost_list, const lList *centry_list,
                                    double *load, bool *is_load_available, uint32_t *used, uint32_t *resv, uint32_t *total,
                                    uint32_t *suspend_manual, uint32_t *suspend_threshold, uint32_t *suspend_on_subordinate,
                                    uint32_t *suspend_calendar, uint32_t *unknown, uint32_t *load_alarm,
                                    uint32_t *disabled_manual, uint32_t *disabled_calendar, uint32_t *ambiguous,
                                    uint32_t *orphaned, uint32_t *error, uint32_t *available, uint32_t *temp_disabled,
                                    uint32_t *manual_intervention);

   public:
      explicit QStatGroupController(std::ostream &out) : out_(out) {
      }

      virtual ~QStatGroupController() = default;

      virtual void process_request(QStatParameter &parameter, QStatModelClient &model, QStatGroupViewBase &view);
   };
}
