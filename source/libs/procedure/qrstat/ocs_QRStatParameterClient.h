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
 * @brief Client side parameters of `qrstat`: parsed from argv and the environment
 */

#include "cull/cull.h"

#include "ocs_QRStatParameter.h"

namespace ocs {
   /** @brief The `qrstat` parameters as the client builds them, from argv and the environment
    *
    * The server side never parses a command line - it receives the marshalled
    * bundle - so everything here is client only.
    *
    * @ingroup libprocedure
    */
   class QRStatParameterClient : public QRStatParameter {

      bool sge_parse_from_file_qrstat(const char *file, lList **ppcmdline, lList **alpp);
      bool sge_parse_qrstat(lList **answer_list, lList **cmdline);

   public:

      bool parse_parameters(lList **answer_list, const char **argv, char **envp);

#pragma region Constructor/Destructor
      /** @brief Build empty client side parameters
       * @param procedure_name the command being run
       */
      explicit QRStatParameterClient(std::string procedure_name) : QRStatParameter(std::move(procedure_name)) {};
      ~QRStatParameterClient() override = default;
#pragma endregion

   };
}
