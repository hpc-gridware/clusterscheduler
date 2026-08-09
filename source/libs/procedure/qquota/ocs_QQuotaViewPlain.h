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
 * @brief Plain text rendering of `qquota`
 */

#include <sstream>

#include "ocs_QQuotaParameterClient.h"
#include "ocs_QQuotaViewBase.h"

/** @brief Column layout of the plain text report: rule, limit, filters */
#define HEAD_FORMAT "%-18s %-20.20s %s\n"

namespace ocs {
   /** @brief Renders the `qquota` report as the columnar plain text a terminal expects
    *
    * The filters of a rule are collected in #filter_stream rather than written
    * as they arrive, because plain text puts them all in one column at the end
    * of the line while the hooks deliver them one at a time.
    *
    * @ingroup libprocedure
    */
   class QQuotaViewPlain : public QQuotaViewBase {
      bool print_header = true;                 ///< Whether the table still needs its header line
      std::ostringstream filter_stream{};       ///< The filters of the current rule, until the line is written
      bool last_exclude = false;                ///< Whether the previous filter value was an exclusion
      std::string last_name = std::string();    ///< The previous filter's name, to group values of one filter
      bool first_filter_type = true;            ///< Whether no filter has been written for this rule yet
      bool filter_type_changed = true;          ///< Whether the current value belongs to a different filter than the last
   public:
      explicit QQuotaViewPlain(const QQuotaParameter &parameter);
      ~QQuotaViewPlain() override;

      void report_started(std::ostream &os) override;
      void report_finished(std::ostream &os) override;
      void report_limit_rule_begin(std::ostream &os, const char* rqs_name, const char *rule_name) override;
      void report_limit_string_value(std::ostream &os, const char *name, const char *value, bool exclude) override;
      void report_limit_rule_finished(std::ostream &os) override;
      void report_resource_value(std::ostream &os, const char *resource, CEntry::Type type, uint64_t max, uint64_t used) override;
   };
}