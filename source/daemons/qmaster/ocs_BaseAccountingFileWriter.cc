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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_conf.h"

#include "ocs_BaseAccountingFileWriter.h"

namespace ocs {
   void BaseAccountingFileWriter::update_config() {
      DENTER(TOP_LAYER);

      ReportingFileWriter::update_config();
      // if the flush_time changed, need to re-calculate the next_flush_time
      int accounting_flush_time = mconf_get_accounting_flush_time();
      // a value of 0 means immediate flush after each accounting record
      if (accounting_flush_time == 0) {
         DPRINTF("Enabled immediate flush for accounting records\n");
         accounting_immediate_flush = true;
      } else {
         DPRINTF("Disabled immediate flush for accounting records\n");
         accounting_immediate_flush = false;
      }

      // if the accounting_flush_time parameter is not set, or if it is 0 (immediate flush),
      // we use the reporting_flush_time for the regular flushing
      u_long64 new_flush_time;
      if (accounting_flush_time > 0) {
         new_flush_time = sge_gmt32_to_gmt64(accounting_flush_time);
      } else {
         new_flush_time = sge_gmt32_to_gmt64(mconf_get_reporting_flush_time());
      }
      update_config_flush_time(new_flush_time);

      DRETURN_VOID;
   }
}
