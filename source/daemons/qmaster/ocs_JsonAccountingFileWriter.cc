/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "category.h"

#include "sgeobj/ocs_DataStore.h"

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"

#include "sge_rusage.h"

#include "ocs_JsonAccountingFileWriter.h"

namespace ocs {
   bool JsonAccountingFileWriter::create_acct_record(lList **answer_list, lListElem *job_report,
                                                     lListElem *job, lListElem *ja_task, bool intermediate) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (!intermediate) {
         const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
         const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
         const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
         DSTRING_STATIC(category_dstring, MAX_STRING_SIZE);

         // get category string
         sge_build_job_category_dstring(&category_dstring, job, master_userset_list, master_project_list, nullptr,
                                        master_rqs_list);
         const char *category_string = sge_dstring_get_string(&category_dstring);

         // get accounting data
         rapidjson::StringBuffer json_buffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(json_buffer);

         // we need to protect the usage_pattern_list via the config_mutex
         sge_mutex_lock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);
         ret = sge_write_rusage(nullptr, &writer, job_report, job, ja_task, category_string, &usage_pattern_list, 0,
                                false, false);
         sge_mutex_unlock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);

         if (ret) {
            // append data to buffer
            json_buffer.Put('\n');
            sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
            buffer += json_buffer.GetString();
            sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

            // If immediate flushing is enabled, flush the buffer now
            if (accounting_immediate_flush) {
               ret = flush();
            }
         }
      }

      DRETURN(ret);
   }
}
