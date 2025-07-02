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

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_MonitoringFileWriter.h"
#include "ocs_JsonUtil.h"

namespace ocs {
   bool MonitoringFileWriter::create_acct_record(lList **answer_list, lListElem *job_report,
                                                 lListElem *job, lListElem *ja_task, bool intermediate) {
      return true;
   }

   bool
   MonitoringFileWriter::create_monitoring_record(const char *json_data) {
      DENTER(TOP_LAYER);

      // append data to buffer
      sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
      this->buffer += json_data;
      this->buffer += "\n";
      sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

      DRETURN(true);
   }
}
