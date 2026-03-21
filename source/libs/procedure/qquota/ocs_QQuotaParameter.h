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

#include "cull/cull.h"

namespace ocs {
   class QQuotaParameter {
   public:
      lList *host_list = nullptr;             /* -h host_list                  */
      lList *resource_match_list = nullptr;   /* -l resource_request           */
      lList *user_list = nullptr;             /* -u user_list                  */
      lList *pe_list = nullptr;               /* -pe pe_list                   */
      lList *project_list = nullptr;          /* -P project_list               */
      lList *cqueue_list = nullptr;           /* -q wc_queue_list              */
      bool is_xml = false;                    // -xml
   private:
      void free_data();

      bool qquota_usage(FILE *fp);
      bool sge_parse_cmdline_qquota(char **argv, lList **ppcmdline, lList **alpp);
      bool sge_parse_from_file_qquota(const char *file, lList **ppcmdline, lList **alpp);
      bool sge_parse_qquota(lList **ppcmdline, lList **alpp);
   public:
      QQuotaParameter() = default;
      virtual ~QQuotaParameter() { free_data(); }

      bool parse_parameters(lList **answer_list, char **argv, char **envp);
   };
}