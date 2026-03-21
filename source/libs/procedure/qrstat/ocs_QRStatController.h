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

#include "ocs_QRStatParameter.h"
#include "ocs_QRStatModel.h"
#include "ocs_QRStatViewBase.h"

namespace ocs {
   class QRStatController {
   public:
      QRStatController() = default;
      virtual ~QRStatController() = default;

      virtual void process_request(QRStatParameter &parameter, QRStatModel &model, QRStatViewBase &view);

   };
}