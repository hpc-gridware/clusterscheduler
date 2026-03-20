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
   class QRStatParameter {
      void free_data();

      bool sge_parse_from_file_qrstat(const char *file, lList **ppcmdline, lList **alpp);
      bool sge_parse_qrstat(lList **answer_list, lList **cmdline);
   public:
      lList* user_list = nullptr;  /* -u user_list */
      lList* ar_id_list = nullptr; /* -ar ar_id */
      bool is_explain = false;     /* -explain */
      bool is_xml = false;         /* -xml */
      bool is_summary = false;     /* show summary of selected ar's or all details of one or multiple ar's */
      char user[128] = "";

      QRStatParameter() = default;
      virtual ~QRStatParameter() { free_data(); }

      bool parse_parameters(lList **answer_list, const char **argv, char **envp);
   };
}