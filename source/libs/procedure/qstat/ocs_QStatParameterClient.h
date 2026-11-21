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
 * @brief Client side parameters of `qstat`: parsed from argv and the environment
 */

#include "ocs_QStatParameter.h"
#include "tom/ocs_Types.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "tom/ocs_List.h"
#include "sgeobj/sge_answer.h"

namespace ocs {
   /** @brief The `qstat` parameters as the client builds them, from argv and the environment
    *
    * The server side never parses a command line - it receives the marshalled
    * bundle - so everything here is client only.
    *
    * @ingroup libprocedure
    */
   class QStatParameterClient : public QStatParameter {

#pragma region Parsing

   private:
      ocs::tom::ListPtr<ocs::tom::AN> sge_parse_qstat(ocs::tom::List<ocs::tom::SPA> ppcmdline);
      bool switch_list_qstat_parse_from_file(ocs::tom::ListPtr<ocs::tom::SPA> &switch_list,
                                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *file);
      int qstat_usage(FILE *fp, char *what);
      bool switch_list_qstat_parse_from_cmdline(ocs::tom::ListPtr<ocs::tom::SPA> &ppcmdline,
                                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **argv);

   public:
      bool parse_parameters(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **argv, char **envp);

#pragma endregion

#pragma region Parsing

   public:
      /** @brief Build empty client side parameters
       * @param procedure_name the command being run
       */
      QStatParameterClient(std::string procedure_name) : QStatParameter(std::move(procedure_name)) {};
      ~QStatParameterClient() override = default;

#pragma endregion
   };
}
