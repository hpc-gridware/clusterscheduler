#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include "ocs_QQuotaModelBase.h"

namespace ocs {
   /** @brief QQuota model for the client execution context.
    *
    * Implements `fetch_data()` by issuing GDI requests to qmaster to retrieve
    * RQS, centry, user set, host group, and exec host CULL lists.
    *
    * @see QQuotaModelBase for the full pipeline description.
    * @ingroup libprocedure
    */
   class QQuotaModelClient : public QQuotaModelBase {
   protected:
      bool fetch_data(lList **answer_list, const lList *host_list) override;
   public:
      QQuotaModelClient() = default ;
      ~QQuotaModelClient() override = default;
   };
}
