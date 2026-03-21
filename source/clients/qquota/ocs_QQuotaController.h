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
#include "ocs_QQuotaModel.h"

namespace ocs {
   class QQuotaController {
   public:
      struct qquota_filter_t {
         const char* user;
         const char* project;
         const char* pe;
         const char* queue;
         const char* host;
      };
   private:
      char *qquota_get_next_filter(stringT filter, const char *cp);
      void qquota_print_out_rule(std::ostream &os, lListElem *rule, dstring rule_name, const char *limit_name,
                                        const char *usage_value, const char *limit_value, qquota_filter_t qfilter,
                                        lList *printed_rules, QQuotaViewBase &view);
      void qquota_print_out_filter(std::ostream &os, lListElem *filter, const char *name, const char *value, dstring *buffer, QQuotaViewBase &view);
   public:

      QQuotaController();
      virtual ~QQuotaController();

      virtual void process_request(QQuotaParameter &parameter, QQuotaModel &model, QQuotaViewBase &view);

   };
}