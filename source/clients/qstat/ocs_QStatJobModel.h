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

#include "ocs_QStatModelBase.h"
#include "ocs_QStatParameter.h"

namespace ocs {
   class QStatJobModel : public QStatModelBase {
      bool is_manager_ = false;
      bool fetch_data(lList **alpp, QStatParameter &parameter);
      bool prepare_data(lList **alpp, QStatParameter &parameter);
      void free_data();
   public:
      lList* ilp = nullptr;
      lList* jlp = nullptr;

      QStatJobModel() = default;
      ~QStatJobModel() override { free_data(); }

      bool make_snapshot(lList **answer_list, QStatParameter &parameter) override;

      [[nodiscard]] bool is_manager() const { return is_manager_; }
   };
}