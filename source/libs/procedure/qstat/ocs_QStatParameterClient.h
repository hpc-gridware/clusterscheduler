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

#include "ocs_QStatParameter.h"
#include "cull/cull.h"

namespace ocs {
   class QStatParameterClient : public QStatParameter {

#pragma region Parsing

   private:
      lList * sge_parse_qstat(lList **ppcmdline, lList **ppljid);
      bool switch_list_qstat_parse_from_file(lList **switch_list, lList **answer_list, const char *file);
      int qstat_usage(FILE *fp, char *what);
      bool switch_list_qstat_parse_from_cmdline(lList **ppcmdline, lList **answer_list, char **argv);

   public:
      bool parse_parameters(lList **answer_list, char **argv, char **envp);

#pragma endregion

#pragma region Parsing

   public:
      QStatParameterClient(std::string procedure_name) : QStatParameter(std::move(procedure_name)) {};
      ~QStatParameterClient() override = default;

#pragma endregion
   };
}
