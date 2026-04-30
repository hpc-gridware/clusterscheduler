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


#include "cull/cull_list.h"

#include "ocs_QStatModelBase.h"
#include "ocs_QStatParameter.h"

namespace ocs {
   /** @brief QStat model for the client execution context.
    *
    * Implements `fetch_data()` by issuing GDI requests to qmaster to retrieve
    * the queue, job, host, and auxiliary CULL lists.  All subsequent filtering
    * and rendering happens locally on the client side.
    *
    * @see QStatModelBase for the full pipeline description.
    * @ingroup libprocedure
    */
   class QStatModelClient : public QStatModelBase {
   protected:
      bool fetch_data(lList **answer_list, QStatParameter &parameter) override;
      bool prepare_data(lList **answer_list, QStatParameter &parameter) override;
   public:
      QStatModelClient() : QStatModelBase() {};
      ~QStatModelClient() override = default;
   };
}
