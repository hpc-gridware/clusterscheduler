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

#include <string>
#include <utility>
#include <array>
#include <memory>

#include "cull/cull.h"

#include "sgeobj/sge_advance_reservation.h"

#include "uti/sge_bootstrap_files.h"

#include "sge_job_qmaster.h"

namespace oge {
   class ReportingFileWriter {
      enum {
         CLASSIC_ACCOUNTING = 0,
         CLASSIC_REPORTING,
         JSON_ACCOUNTING,
         JSON_REPORTING,
         NUM_WRITERS
      };

   private:
      static std::array<ReportingFileWriter *,NUM_WRITERS> writers;
      static std::string reporting_params;

   protected:
      std::string filename;
      std::string buffer;
      pthread_mutex_t mutex;
      u_long32 config_flush_time;
      u_long32 next_flush_time;

   public:
      explicit ReportingFileWriter(std::string filename)
              : filename(std::move(filename)), mutex(PTHREAD_MUTEX_INITIALIZER),
              config_flush_time(0), next_flush_time(0) {
      };

      virtual ~ReportingFileWriter() {
         flush();
      }

      // Class methods
      static void initialize();
      static void shutdown();
      static bool flush_all();
      static u_long32 trigger_all(monitoring_t *monitor);
      static void update_config_all();

      static bool
      create_new_job_records(lList **answer_list, const lListElem *job);

      static bool
      create_job_logs(lList **answer_list, u_long32 event_time, job_log_t, const char *user, const char *host,
                      const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                      const lListElem *pe_task, const char *message);

      static bool
      create_acct_records(lList **answer_list, lListElem *job_report, lListElem *job,
                          lListElem *ja_task, bool intermediate);

      static bool
      create_host_records(lList **answer_list, const lListElem *host, u_long32 report_time);

      static bool
      create_host_consumable_records(lList **answer_list, const lListElem *host, const lListElem *job,
                                     u_long32 report_time);

      static bool
      create_queue_records(lList **answer_list, const lListElem *queue, u_long32 report_time);

      static bool
      create_queue_consumable_records(lList **answer_list, const lListElem *host, const lListElem *queue,
                                      const lListElem *job, u_long32 report_time);

      static bool
      create_new_ar_records(lList **answer_list, const lListElem *ar, u_long32 report_time);

      static bool
      create_ar_attribute_records(lList **answer_list, const lListElem *ar, u_long32 report_time);

      static bool
      create_ar_log_records(lList **answer_list, const lListElem *ar, ar_state_event_t state,
                            const char *ar_description, u_long32 report_time);

      static bool
      create_ar_acct_records(lList **answer_list, const lListElem *ar, u_long32 report_time);

      static bool
      is_intermediate_acct_required(const lListElem *job, const lListElem *ja_task, const lListElem *pe_task);


      // Object methods
      virtual bool flush();
      virtual u_long32 trigger(monitoring_t *monitor);
      virtual void update_config();
      void update_config_flush_time(u_long32 new_flush_time);

      virtual bool
      create_new_job_record(lList **answer_list, const lListElem *job) { return true; }

      virtual bool
      create_job_log(lList **answer_list, u_long32 event_time, const job_log_t, const char *user, const char *host,
                     const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                     const lListElem *pe_task, const char *message) { return true; }

      virtual bool
      create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job,
                         lListElem *ja_task, bool intermediate) = 0;

      virtual bool
      create_host_record(lList **answer_list, const lListElem *host, u_long32 report_time) { return true; }

      virtual bool
      create_host_consumable_record(lList **answer_list, const lListElem *host, const lListElem *job,
                                    u_long32 report_time) { return true; }

      virtual bool
      create_queue_record(lList **answer_list, const lListElem *queue, u_long32 report_time) { return true; }

      virtual bool
      create_queue_consumable_record(lList **answer_list, const lListElem *host, const lListElem *queue,
                                     const lListElem *job, u_long32 report_time) { return true; }

      virtual bool
      create_new_ar_record(lList **answer_list, const lListElem *ar, u_long32 report_time) { return true; }

      virtual bool
      create_ar_attribute_record(lList **answer_list, const lListElem *ar, u_long32 report_time) { return true; }

      virtual bool
      create_ar_log_record(lList **answer_list, const lListElem *ar, ar_state_event_t state,
                           const char *ar_description, u_long32 report_time) { return true; }

      virtual bool
      create_ar_acct_record(lList **answer_list, const lListElem *ar, u_long32 report_time) { return true; }
   };
}
