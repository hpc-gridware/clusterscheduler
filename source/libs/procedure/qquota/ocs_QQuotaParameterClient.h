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
 * @brief Client side parameters of `qquota`: parsed from argv and the environment
 */

#include "ocs_QQuotaParameter.h"
#include "cull/cull.h"

namespace ocs {
   /** @brief The `qquota` parameters as the client builds them, from argv and the environment
    *
    * The server side never parses a command line - it receives the marshalled
    * bundle - so everything here is client only.
    *
    * @ingroup libprocedure
    */
   class QQuotaParameterClient : public QQuotaParameter {

      bool show_usage(FILE *fp);
      bool parse_cmdline_and_env(char **argv, lList **switch_list, lList **answer_list);
      bool parse_cmdline_from_file(const char *file, lList **switch_list, lList **answer_list);
      bool parse_switch_list(lList **switch_list, lList **answer_list);
   public:
      /** @brief Build empty client side parameters
       * @param procedure_name the command being run
       */
      QQuotaParameterClient(std::string procedure_name) : QQuotaParameter(std::move(procedure_name)) {};
      ~QQuotaParameterClient() override = default;


      bool parse_parameters(lList **answer_list, char **argv, char **envp);
   };
}
