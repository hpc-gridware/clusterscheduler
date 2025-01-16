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

#include "uti/sge_time.h"

#include "sgeobj/sge_conf.h"

#include "ocs_BaseReportingFileWriter.h"

namespace ocs {

   u_long64 BaseReportingFileWriter::trigger(monitoring_t *monitor) {
      u_long64 now = sge_get_gmt64();
      u_long64 next_trigger = U_LONG64_MAX;

      // trigger sharelog
      if (sharelog_interval > 0) {
         if (next_sharelog <= now) {
            create_sharelog_record(monitor);
            next_sharelog = now + sharelog_interval;
         }
         next_trigger = next_sharelog;
      }

      // trigger
      u_long64 base_trigger = ReportingFileWriter::trigger(monitor);
      if (base_trigger < next_trigger) {
         next_trigger = base_trigger;
      }

      return next_trigger;
   }

   void BaseReportingFileWriter::update_config() {
      ReportingFileWriter::update_config();
      do_joblog = mconf_get_do_joblog();
      log_consumables = mconf_get_log_consumables();
      u_long64 new_sharelog_interval = sge_gmt32_to_gmt64(mconf_get_sharelog_time());
      if (new_sharelog_interval != sharelog_interval) {
         if (new_sharelog_interval == 0) {
            next_sharelog = 0;
         } else {
            if (next_sharelog != 0) {
               next_sharelog = next_sharelog - sharelog_interval + new_sharelog_interval;
            }
         }
         sharelog_interval = new_sharelog_interval;
      }
   }
}
