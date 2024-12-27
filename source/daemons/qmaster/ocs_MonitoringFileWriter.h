#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <utility>

#include "ocs_ReportingFileWriter.h"

namespace ocs {
   class MonitoringFileWriter : public ReportingFileWriter {
   public:
      explicit MonitoringFileWriter()
      : ReportingFileWriter(std::string{bootstrap_get_cell_root()} + "/" + COMMON_DIR + "/" + "monitoring.jsonl", false) {
      }
      bool create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job, lListElem *ja_task,
                              bool intermediate) override;
      bool
      create_monitoring_record(const char *json_data) override;
   };
}
