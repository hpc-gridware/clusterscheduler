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

/** @file
 * @brief JSON rendering of `qquota`
 */

#include <iosfwd>

#include "ocs_QQuotaParameterClient.h"
#include "ocs_QQuotaViewBase.h"

namespace ocs {
   /** @brief Renders the `qquota` report as JSON
    *
    * JSON needs state the other formats do not: the values of one filter go
    * into one array, and a comma belongs between rules but not before the
    * first. The hooks do not announce either boundary, so the view remembers
    * what it has already opened.
    *
    * @ingroup libprocedure
    */
   class QQuotaViewJSON : public QQuotaViewBase {
      int indent = 0;                        ///< Current indentation depth
      std::string last_filter_name{};        ///< The filter whose array is open, to keep its values together
      bool first_rule = true;                ///< Whether no rule has been written yet, so no comma is needed
   public:
      explicit QQuotaViewJSON(const QQuotaParameter &parameter);
      ~QQuotaViewJSON() override;

      void report_started(std::ostream &os) override;
      void report_finished(std::ostream &os) override;
      void report_limit_rule_begin(std::ostream &os, const char* rqs_name_name, const char *rule_name) override;
      void report_limit_string_value(std::ostream &os, const char *filter_name, const char *value, bool exclude) override;
      void report_limit_rule_finished(std::ostream &os) override;
      void report_resource_value(std::ostream &os, const char* resource, CEntry::Type type, uint64_t max, uint64_t used) override;
   };
}
