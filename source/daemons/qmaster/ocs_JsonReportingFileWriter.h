#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024,2026 HPC-Gridware GmbH
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

/** @file
 * @brief The reporting file in JSON form
 */

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_STU.h"

#include "ocs_BaseReportingFileWriter.h"
#include "sgeobj/sge_answer.h"

namespace ocs {

   /** @brief Writes the reporting records as one JSON object per line */
   class JsonReportingFileWriter : public BaseReportingFileWriter {
   private:

   public:
      JsonReportingFileWriter() : BaseReportingFileWriter(std::string{bootstrap_get_reporting_file()} + ".jsonl", false) {
      }

      void
      create_record(rapidjson::StringBuffer &stringBuffer);

      bool
      create_new_job_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstElement<ocs::tom::JB> job) override;

      bool
      create_job_log(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint64_t event_time, job_log_t, const char *user,
                     const char *host,
                     ocs::tom::ConstElement<ocs::tom::JR> job_report, ocs::tom::ConstElement<ocs::tom::JB> job,
                     ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                     const char *message) override;

      bool
      create_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::Element<ocs::tom::JR> job_report,
                         ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                         bool intermediate) override;

      bool
      create_online_usage_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 ocs::tom::ConstElement<ocs::tom::JR> job_report,
                                 ocs::tom::ConstElement<ocs::tom::JB> job,
                                 ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                 ocs::tom::ConstElement<ocs::tom::PET> pe_task, bool aggregate_pe_tasks) override;

      bool
      create_host_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::EH> host,
                         uint64_t report_time) override;

      bool
      create_host_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstElement<ocs::tom::EH> host,
                                    ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time) override;

      bool
      create_queue_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::QU> queue,
                          uint64_t report_time) override;

      bool
      create_queue_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                     ocs::tom::ConstElement<ocs::tom::EH> host,
                                     ocs::tom::ConstElement<ocs::tom::QU> queue,
                                     ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time) override;

      bool
      create_new_ar_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                           uint64_t report_time) override;

      bool
      create_ar_attribute_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                                 uint64_t report_time) override;

      bool
      create_ar_log_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                           ar_state_event_t state, const char *ar_description, uint64_t report_time) override;

      bool
      create_ar_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                            uint64_t report_time) override;

      void
      create_sharelog_record(monitoring_t *monitor) override;

      // non virtual functions
      void
      create_single_ar_acct_record(ocs::tom::ConstElement<ocs::tom::AR> ar, const char *cqueue_name,
                                   const char *hostname, uint32_t slots, uint64_t report_time);

      static bool
      write_load_values(rapidjson::Writer<rapidjson::StringBuffer> &writer, ocs::tom::ConstList<ocs::tom::HL> load_list,
                        ocs::tom::ConstList<ocs::tom::STU> variables);

      bool
      write_consumables(rapidjson::Writer<rapidjson::StringBuffer> &writer, ocs::tom::ConstList<ocs::tom::RUE> actual,
                        ocs::tom::ConstList<ocs::tom::CE> total, ocs::tom::ConstElement<ocs::tom::EH> host,
                        ocs::tom::ConstElement<ocs::tom::JB> job) const;

   };
}
