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
 * @brief Base view of `qquota`, and the interface the three output formats implement
 */

#include <ostream>

#include "ocs_ProcedureView.h"

#include "ocs_QQuotaParameterClient.h"

#include "sgeobj/ocs_CEntry.h"

namespace ocs {
   /** @brief Base view for `qquota`, and the interface the three formats implement
    *
    * The controller walks the resource quota sets and reports what it finds
    * through these hooks; #QQuotaViewPlain, #QQuotaViewXML and #QQuotaViewJSON
    * turn the events into their own syntax. Adding a format means implementing
    * the hooks, not touching the walk.
    *
    * A rule is reported as `report_limit_rule_begin()`, then its filters as
    * `report_limit_string_value()` and its limits as `report_resource_value()`,
    * then `report_limit_rule_finished()`.
    *
    * @ingroup libprocedure
    */
   class QQuotaViewBase : public ProcedureView {
   public:
      /** @brief Build a view for one qquota call
       * @param parameter the call's parameters
       */
      explicit QQuotaViewBase(const QQuotaParameter &parameter) : ProcedureView(parameter) {};

      ~QQuotaViewBase() override = default;

      /** @brief Begin the report
       * @param os stream to write to
       */
      virtual void report_started(std::ostream &os) = 0;

      /** @brief End the report
       * @param os stream to write to
       */
      virtual void report_finished(std::ostream &os) = 0;

      /** @brief Begin one rule of one resource quota set
       * @param os stream to write to
       * @param rqs_name_name the resource quota set
       * @param rule_name the rule within it
       */
      virtual void report_limit_rule_begin(std::ostream &os, const char* rqs_name_name, const char *rule_name) = 0;

      /** @brief Report one filter of the current rule
       * @param os stream to write to
       * @param name the filter, e.g. `users` or `queues`
       * @param value the value it matches
       * @param exclude whether the value is excluded rather than included
       */
      virtual void report_limit_string_value(std::ostream &os, const char *name, const char *value, bool exclude) = 0;

      /** @brief End the current rule
       * @param os stream to write to
       */
      virtual void report_limit_rule_finished(std::ostream &os) = 0;

      /** @brief Report one limit of the current rule
       * @param os stream to write to
       * @param resource the limited resource
       * @param type the resource's type, which decides how the values are rendered
       * @param max the configured limit
       * @param used how much of it is in use
       */
      virtual void report_resource_value(std::ostream &os, const char* resource, CEntry::Type type, uint64_t max, uint64_t used) = 0;
   };
}
