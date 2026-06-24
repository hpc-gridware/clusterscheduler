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

#include "ocs_QQuotaParameterClient.h"
#include "ocs_QQuotaViewBase.h"
#include "ocs_QQuotaModelBase.h"

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
      std::ostream &out_;
   private:
      char *qquota_get_next_filter(stringT filter, const char *cp);
      void qquota_print_out_rule(std::ostream &os, const lListElem *rqs, lListElem *rule, const char *limit_name,
                                        ocs::CEntry::Type type, uint64_t usage_value, uint64_t limit_value, qquota_filter_t qfilter,
                                        lList *printed_rules, QQuotaViewBase &view);
      void qquota_print_out_filter(std::ostream &os, lListElem *filter, const char *name, const char *value, QQuotaViewBase &view);
   public:

      explicit QQuotaController(std::ostream &out) : out_(out) {};
      virtual ~QQuotaController();

      virtual void process_request(QQuotaParameter &parameter, QQuotaModelBase &model, QQuotaViewBase &view);

   };
}
