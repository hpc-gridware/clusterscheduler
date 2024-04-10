#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023 HPC-Gridware GmbH
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

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "oge_BaseReportingFileWriter.h"

namespace oge {

   class JsonReportingFileWriter : public BaseReportingFileWriter {
   private:
   public:
      JsonReportingFileWriter() : BaseReportingFileWriter(std::string{bootstrap_get_reporting_file()} + ".jsonl") {
      }

      void
      create_record(rapidjson::StringBuffer &stringBuffer);

      bool
      create_new_job_record(lList **answer_list, const lListElem *job) override;

      bool
      create_job_log(lList **answer_list, u_long32 event_time, job_log_t, const char *user, const char *host,
                     const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                     const lListElem *pe_task, const char *message) override;

      bool
      create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job,
                         lListElem *ja_task, bool intermediate) override;

      bool
      create_host_record(lList **answer_list, const lListElem *host, u_long32 report_time) override;

      bool
      create_host_consumable_record(lList **answer_list, const lListElem *host, const lListElem *job,
                                    u_long32 report_time) override;

      bool
      create_queue_record(lList **answer_list, const lListElem *queue, u_long32 report_time) override;

      bool
      create_queue_consumable_record(lList **answer_list, const lListElem *host, const lListElem *queue,
                                     const lListElem *job, u_long32 report_time) override;

      bool
      create_new_ar_record(lList **answer_list, const lListElem *ar, u_long32 report_time) override;

      bool
      create_ar_attribute_record(lList **answer_list, const lListElem *ar, u_long32 report_time) override;

      bool
      create_ar_log_record(lList **answer_list, const lListElem *ar, ar_state_event_t state,
                           const char *ar_description, u_long32 report_time) override;

      bool
      create_ar_acct_record(lList **answer_list, const lListElem *ar, u_long32 report_time) override;

      void
      create_sharelog_record(monitoring_t *monitor) override;

      // non virtual functions
      void
      create_single_ar_acct_record(const lListElem *ar, const char *cqueue_name,
                                   const char *hostname, u_long32 slots, u_long32 report_time);
      static bool
      write_load_values(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                        const lList *load_list, const lList *variables);
      bool
      write_consumables(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                        const lList *actual, const lList *total,
                        const lListElem *host, const lListElem *job) const;

   };
}
