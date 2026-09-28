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

/** @file
 * @brief Fetching a `qconf` read operation's objects over GDI
 */

#include "ocs_QConfModelBase.h"

namespace ocs {
   /** @brief The client side model: ask qmaster over GDI
    * @ingroup libprocedure
    */
   class QConfModelClient : public QConfModelBase {
   protected:
      bool fetch(lList **answer_list, gdi::Target target,
                 lCondition *where, lEnumeration *what) override;
      bool fetch_centries(lList **answer_list) override;
      bool fetch_hgroups(lList **answer_list) override;
   public:
      QConfModelClient() = default;
      ~QConfModelClient() override = default;
   };
}
