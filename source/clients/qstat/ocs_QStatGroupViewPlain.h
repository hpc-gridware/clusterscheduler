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

#include <ostream>

#include "ocs_QStatGroupViewBase.h"

namespace ocs {
   class QStatGroupViewPlain : public QStatGroupViewBase {
   public:
      QStatGroupViewPlain();
      ~QStatGroupViewPlain() override = default;

      void report_started(std::ostream &os, QStatParameter &parameter) override;
      void report_finished(std::ostream &os, QStatParameter &paraemter) override;
      void report_cqueue(std::ostream &os, const char* qname, Summary *cqueue_summary, QStatParameter &parameter) override;
   };
}