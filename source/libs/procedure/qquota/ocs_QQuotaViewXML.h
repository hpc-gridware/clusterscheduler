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

#include "cull/cull.h"

#include "ocs_QQuotaParameter.h"
#include "ocs_QQuotaViewBase.h"

namespace ocs {
   class QQuotaViewXML : public QQuotaViewBase {
   public:
      explicit QQuotaViewXML(const QQuotaParameter &parameter);
      ~QQuotaViewXML() override;

      void report_started(std::ostream &os) override;
      void report_finished(std::ostream &os) override;
      void report_limit_rule_begin(std::ostream &os, const char* limit_name) override;
      void report_limit_string_value(std::ostream &os, const char *name, const char *value, bool exclude) override;
      void report_limit_rule_finished(std::ostream &os, const char *limit_name) override;
      void report_resource_value(std::ostream &os, const char* resource, const char* limit, const char *value) override;
   };
}