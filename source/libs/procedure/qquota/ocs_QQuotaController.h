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
 * @brief Controller of `qquota`: runs the request and drives the view
 */

#include <ostream>

#include "cull/cull.h"

#include "ocs_QQuotaParameterClient.h"
#include "ocs_QQuotaViewBase.h"
#include "ocs_QQuotaModelBase.h"

namespace ocs {
   /** @brief Runs one `qquota` request: fetch the quota sets, report the rules that apply
    *
    * @ingroup libprocedure
    */
   class QQuotaController {
   public:
      /** @brief What the user narrowed the report to, as one value per dimension
       *
       * A rule is printed only where it matches all of these. A member is
       * nullptr when the user did not restrict that dimension.
       */
      struct qquota_filter_t {
         const char* user;      ///< The user, from `-u`
         const char* project;   ///< The project, from `-P`
         const char* pe;        ///< The parallel environment, from `-pe`
         const char* queue;     ///< The cluster queue, from `-q`
         const char* host;      ///< The host, from `-h`
      };
   private:
      std::ostream &out_;   ///< Where the view writes
   private:
      char *qquota_get_next_filter(stringT filter, const char *cp);
      void qquota_print_out_rule(std::ostream &os, const lListElem *rqs, lListElem *rule, const char *limit_name,
                                        ocs::CEntry::Type type, uint64_t usage_value, uint64_t limit_value, qquota_filter_t qfilter,
                                        lList *printed_rules, QQuotaViewBase &view);
      void qquota_print_out_filter(std::ostream &os, lListElem *filter, const char *name, const char *value, QQuotaViewBase &view);
   public:

      /** @brief Bind a controller to an output stream
       * @param out the stream the view will write to
       */
      explicit QQuotaController(std::ostream &out) : out_(out) {};

      virtual ~QQuotaController();

      virtual void process_request(QQuotaParameter &parameter, QQuotaModelBase &model, QQuotaViewBase &view);

   };
}
