#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include "ocs_QQuotaParameter.h"

namespace ocs {
   class QQuotaModel {
   public:
      lList *rqs_list = nullptr;
      lList *centry_list = nullptr;
      lList *userset_list = nullptr;
      lList *hgroup_list = nullptr;
      lList *exechost_list = nullptr;
   private:
      void free_data();
      bool fetch_data(lList **answer_list, const QQuotaParameter& parameter);
   public:
      QQuotaModel() = default;
      virtual ~QQuotaModel() { free_data(); }

      bool make_snapshot(lList **answer_list, QQuotaParameter &parameter);
   };
}